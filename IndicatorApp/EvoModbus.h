#pragma once
#include <QJSEngine>
#include <QLCDNumber>
#include <QLabel>
#include <QMap>
#include <QModbusDataUnit>
#include <QModbusReply>
#include <QModbusTcpClient>
#include <QObject>
#include <QPointer>
#include <QProgressBar>
#include <QString>
#include <QTimer>
#include <QVariant>
#include <QVector>
#include <functional>
#include <string>

// Подключаем вашу библиотеку единиц измерения
#include "EvoUnit.h"

namespace EvoModbus {

// =========================================================
// 1. ОПРЕДЕЛЕНИЯ ТИПОВ
// =========================================================

enum class ValueType { Bool = 0, UInt16, Int16, UInt32, Int32, Float };

enum class ByteOrder {
    ABCD = 0, // Big Endian (Standard)
    DCBA,     // Little Endian
    CDAB,     // Middle Endian (Swap Words)
    BADC      // Swap Bytes
};

// Данные одного канала: Значение + Единица измерения
struct ChannelData
{
    QVariant value{};
    EvoUnit::MeasUnit unit{EvoUnit::MeasUnit::Unknown};
};

// Конфигурация источника данных (Hardware Register)
struct Source
{
    std::string id{};
    int serverAddress{1};
    int valueAddress{0};
    int valueType{1}; // ValueType::UInt16
    QModbusDataUnit::RegisterType regType{QModbusDataUnit::HoldingRegisters};
    int byteOrder{0}; // ByteOrder::ABCD

    // Единица измерения сырых данных по умолчанию
    EvoUnit::MeasUnit defaultUnit{EvoUnit::MeasUnit::Unknown};

    bool isBitType() const
    {
        return (regType == QModbusDataUnit::Coils || regType == QModbusDataUnit::DiscreteInputs);
    }
};

// =========================================================
// 2. MANAGER (Hardware Driver)
// Отвечает только за общение с сетью и парсинг байт.
// =========================================================
class Manager : public QObject
{
    Q_OBJECT
public:
    explicit Manager(QObject *parent = nullptr);
    ~Manager();

    // Настройка
    void addSource(const Source &source);
    Source getSourceConfig(const QString &id) const;

    // Управление соединением
    void connectTo(const QString &ip, int port);
    void disconnectFrom();
    void startPolling(int intervalMs);
    void stopPolling();

    // Доступ к сырым данным
    QVariantMap getRawData() const { return m_rawData; }

signals:
    void rawDataUpdated();
    void connectionStateChanged(int state);
    void errorOccurred(QString msg);

private slots:
    void onPollTimer();
    void onReadReady();
    void onStateChanged(QModbusDevice::State state);

private:
    // Блок оптимизированного запроса
    struct RequestBlock
    {
        int serverAddress{1};
        QModbusDataUnit::RegisterType regType{QModbusDataUnit::HoldingRegisters};
        int startAddress{0};
        int count{0};
    };

    QModbusTcpClient *m_modbus{nullptr};
    QTimer *m_pollTimer{nullptr};

    QVector<Source> m_sources{};
    QVector<RequestBlock> m_blocks{};
    QVariantMap m_rawData{};

    bool m_recalcNeeded{false};

    void recalculateBlocks();
    QVariant parseValue(const Source &src, const QVector<quint16> &data);

    static int getRegisterCount(int type);
    static quint32 composeUInt32(quint16 w1, quint16 w2, int order);
    static float composeFloat(quint16 w1, quint16 w2, int order);
};

// =========================================================
// 3. CONTROLLER (Logic Layer)
// Объединяет Hardware и Scripting. Хранит итоговые каналы.
// =========================================================
class Controller : public QObject
{
    Q_OBJECT
public:
    explicit Controller(QObject *parent = nullptr);

    // --- Config API ---

    // Добавить источник через C++ структуру
    void addModbusSource(const Source &source);

    // Добавить источник через JS-объект {id, address, ...}
    Q_INVOKABLE void addSource(const QVariantMap &config);

    // Установить JS скрипт обработки
    void setScript(const QString &scriptCode);

    // --- Control API ---
    Q_INVOKABLE void connectToServer(const QString &ip, int port);
    Q_INVOKABLE void start(int intervalMs = 1000);
    Q_INVOKABLE void stop();

    // Доступ к движку (для регистрации Enums из main.cpp)
    QJSEngine *engine() const { return const_cast<QJSEngine *>(&m_jsEngine); }

    // Получить карту всех каналов (Value + Unit)
    QMap<QString, ChannelData> getChannels() const { return m_channels; }

    // --- JS In-Script API (Object "IO") ---

    // Получить значение канала
    Q_INVOKABLE QVariant val(const QString &id);

    // Записать канал (создать новый или обновить существующий)
    // unit - int, который приводится к EvoUnit::MeasUnit
    Q_INVOKABLE void set(const QString &id, const QVariant &value, int unit = 0);

signals:
    void channelsUpdated();
    void error(QString msg);

private slots:
    void onRawDataReceived();

private:
    Manager *m_manager{nullptr};
    QJSEngine m_jsEngine;
    QJSValue m_jsProcessFunction;

    // Шлюз для функций конвертации в JS
    EvoUnit::JsGateway *m_unitGateway{nullptr};

    // Хранилище каналов
    QMap<QString, ChannelData> m_channels{};
};

// =========================================================
// 4. BINDER (Presentation Layer)
// Связывает данные с Qt Widgets
// =========================================================
class Binder : public QObject
{
    Q_OBJECT
public:
    explicit Binder(Controller *controller, QObject *parent = nullptr);

    // --- Bindings ---

    // Label: Ипользует EvoUnit::format для автоматического добавления ед. изм.
    void bindLabel(QLabel *label, const std::string &channelId);

    // LCD Number
    void bindLCD(QLCDNumber *lcd, const std::string &channelId, double scale = 1.0);

    // Progress Bar (авто convert to int)
    void bindProgressBar(QProgressBar *bar, const std::string &channelId);

    // Custom Lambda
    using Callback = std::function<void(const ChannelData &)>;
    void bindCustom(const std::string &channelId, Callback callback);

private slots:
    void syncUI();

private:
    Controller *m_controller{nullptr};

    struct BindItem
    {
        QPointer<QWidget> widget;
        std::string id{};
        int type{0}; // 0=Label, 1=LCD, 2=Progress, 3=Custom
        double scale{1.0};
        Callback customCb{nullptr};
    };

    QVector<BindItem> m_bindings{};
};

} // namespace EvoModbus
