#pragma once

#include <QJSEngine>
#include <QMap>
#include <QModbusDataUnit>
#include <QModbusReply>
#include <QModbusTcpClient>
#include <QObject>
#include <QString>
#include <QTimer>
#include <QVariant>
#include <QVector>
#include <string>

// Подключаем вашу библиотеку единиц измерения
#include "EvoUnit.h"

namespace EvoModbus {

// Типы данных
enum class ValueType { Bool = 0, UInt16, Int16, UInt32, Int32, Float };
enum class ByteOrder { ABCD = 0, DCBA, CDAB, BADC };

// Данные канала (значение + единица)
struct ChannelData
{
    QVariant value{};
    EvoUnit::MeasUnit unit{EvoUnit::MeasUnit::Unknown};
};

// Конфигурация первичного источника (Modbus)
struct Source
{
    std::string id{};
    int serverAddress{1};
    int valueAddress{0};
    int valueType{1}; // ValueType::UInt16
    QModbusDataUnit::RegisterType regType{QModbusDataUnit::HoldingRegisters};
    int byteOrder{0}; // ByteOrder::ABCD
    EvoUnit::MeasUnit defaultUnit{EvoUnit::MeasUnit::Unknown};
};

// Конфигурация вычислимого канала (JS Формула)
struct ComputedChannel
{
    std::string id{};
    QString formula{};
    EvoUnit::MeasUnit unit{EvoUnit::MeasUnit::Unknown};
};

// --- MANAGER (Hardware Driver) ---
class Manager : public QObject
{
    Q_OBJECT
public:
    explicit Manager(QObject *parent = nullptr);
    ~Manager();

    void addSource(const Source &source);
    void clearSources();
    QVector<Source> getSources() const;
    Source getSourceConfig(const QString &id) const;

    void connectTo(const QString &ip, int port);
    void disconnectFrom();
    void startPolling(int intervalMs);
    void stopPolling();

    QVariantMap getRawData() const { return m_rawData; }

    // Прямая запись
    bool writeValue(int serverAddress,
                    int startAddress,
                    const QVariant &value,
                    ValueType type,
                    ByteOrder order);

signals:
    void rawDataUpdated();
    void connectionStateChanged(int state);
    void errorOccurred(QString msg);
    void writeFinished();

private slots:
    void onPollTimer();
    void onReadReady();
    void onStateChanged(QModbusDevice::State state);
    void onWriteReplyFinished();

private:
    struct RequestBlock
    {
        int serverAddress;
        QModbusDataUnit::RegisterType regType;
        int startAddress;
        int count;
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
    static QVector<quint16> decomposeUInt32(quint32 val, int order);
    static QVector<quint16> decomposeFloat(float val, int order);
};

// --- CONTROLLER (Logic Layer) ---
class Controller : public QObject
{
    Q_OBJECT
public:
    explicit Controller(QObject *parent = nullptr);

    // Управление источниками (Hardware)
    void addModbusSource(const Source &source);
    void clearSources();
    QVector<Source> getSources() const;

    // Управление формулами (Logic)
    void addComputedChannel(const ComputedChannel &ch);
    void clearComputedChannels();
    QVector<ComputedChannel> getComputedChannels() const;

    // Сборка и применение скрипта
    void buildAndApplyScript();
    // [FIX] Добавлен метод, которого не хватало в header
    void setScript(const QString &scriptCode);

    // Управление подключением
    Q_INVOKABLE void connectToServer(const QString &ip, int port);
    Q_INVOKABLE void start(int intervalMs = 1000);
    Q_INVOKABLE void stop();

    // Доступ к движку и каналам
    QJSEngine *engine() const { return const_cast<QJSEngine *>(&m_jsEngine); }
    QMap<QString, ChannelData> getChannels() const { return m_channels; }

    // JS API (доступны внутри скрипта как IO.val, IO.set)
    Q_INVOKABLE QVariant val(const QString &id);
    Q_INVOKABLE void set(const QString &id, const QVariant &value, int unit = 0);
    Q_INVOKABLE void write(const QString &id, const QVariant &value);

signals:
    void channelsUpdated();
    void error(QString msg);

private slots:
    void onRawDataReceived();

private:
    Manager *m_manager{nullptr};
    QJSEngine m_jsEngine;
    QJSValue m_jsProcessFunction;
    EvoUnit::JsGateway *m_unitGateway{nullptr};
    QMap<QString, ChannelData> m_channels{};
    QVector<ComputedChannel> m_computedChannels{};
};

} // namespace EvoModbus
