#pragma once

#include <QJSEngine>
#include <QLCDNumber>
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
#include <QWidget>
#include <qlabel.h>
#include <string>

// Подключаем EvoUnit
#include "EvoUnit.h"

namespace EvoModbus {

// =========================================================
// 1. DATA TYPES
// =========================================================

enum class ValueType { Bool = 0, UInt16, Int16, UInt32, Int32, Float };

enum class ByteOrder { ABCD = 0, DCBA, CDAB, BADC };

struct ChannelData
{
    QVariant value{};
    EvoUnit::MeasUnit unit{EvoUnit::MeasUnit::Unknown};
};

struct Source
{
    std::string id{};
    int serverAddress{1};
    int valueAddress{0};
    int valueType{1}; // ValueType::UInt16
    QModbusDataUnit::RegisterType regType{QModbusDataUnit::HoldingRegisters};
    int byteOrder{0}; // ByteOrder::ABCD
    EvoUnit::MeasUnit defaultUnit{EvoUnit::MeasUnit::Unknown};

    bool isBitType() const
    {
        return (regType == QModbusDataUnit::Coils || regType == QModbusDataUnit::DiscreteInputs);
    }
};

struct ComputedChannel
{
    std::string id{};
    QString formula{};
    EvoUnit::MeasUnit unit{EvoUnit::MeasUnit::Unknown};
};

// =========================================================
// 2. MANAGER (Hardware Driver)
// =========================================================
class Manager : public QObject
{
    Q_OBJECT
public:
    explicit Manager(QObject *parent = nullptr);
    ~Manager();

    // Config
    void addSource(const Source &source);
    void clearSources();
    QVector<Source> getSources() const;
    Source getSourceConfig(const QString &id) const;

    // Connection
    void connectTo(const QString &ip, int port);
    void disconnectFrom();
    void startPolling(int intervalMs);
    void stopPolling();

    QVariantMap getRawData() const { return m_rawData; }

    // Writing
    bool writeValue(int serverAddress,
                    int startAddress,
                    const QVariant &value,
                    ValueType type,
                    ByteOrder order);
    bool writeBit(int serverAddress, int address, bool value);
    bool writeMultipleRegisters(int serverAddress, int startAddress, const QVector<quint16> &values);

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
    static QVector<quint16> decomposeUInt32(quint32 val, int order);
    static QVector<quint16> decomposeFloat(float val, int order);
};

// =========================================================
// 3. CONTROLLER (Logic Layer)
// =========================================================
class Controller : public QObject
{
    Q_OBJECT

public:
    explicit Controller(QObject *parent = nullptr);

    // Config
    void addModbusSource(const Source &source);
    Q_INVOKABLE void addSource(const QVariantMap &config);
    void clearSources();
    QVector<Source> getSources() const;

    void addComputedChannel(const ComputedChannel &ch);
    void clearComputedChannels();
    QVector<ComputedChannel> getComputedChannels() const;
    void buildAndApplyScript();
    void setScript(const QString &scriptCode);

    bool isIdUnique(const QString &id) const;

    // Control
    Q_INVOKABLE void connectToServer(const QString &ip, int port);
    Q_INVOKABLE void start(int intervalMs = 1000);
    Q_INVOKABLE void stop();

    QJSEngine *engine() const { return const_cast<QJSEngine *>(&m_jsEngine); }
    QMap<QString, ChannelData> getChannels() const { return m_channels; }

    // JS API
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

// =========================================================
// 4. BINDER (Presentation Helper)
// =========================================================
class Binder : public QObject
{
    Q_OBJECT
public:
    explicit Binder(Controller *controller, QObject *parent = nullptr);

    void bindLabel(QLabel *label, const std::string &channelId);
    void bindLCD(QLCDNumber *lcd, const std::string &channelId, double scale = 1.0);
    void bindProgressBar(QProgressBar *bar, const std::string &channelId);

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
        int type{0};
        double scale{1.0};
        Callback customCb{nullptr};
    };
    QVector<BindItem> m_bindings{};
};

} // namespace EvoModbus
