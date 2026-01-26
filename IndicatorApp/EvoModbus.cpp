#include "EvoModbus.h"
#include <QDebug>
#include <QMetaEnum>
#include <QtGlobal>
#include <algorithm>
#include <cstring>

namespace EvoModbus {

static const int MAX_GAP{10};
static const int MAX_PDU{120};
static const int MAX_PDU_BITS{1900};

// --- MANAGER ---

Manager::Manager(QObject *parent)
    : QObject(parent)
{
    m_modbus = new QModbusTcpClient(this);
    m_pollTimer = new QTimer(this);
    connect(m_modbus, &QModbusClient::stateChanged, this, &Manager::onStateChanged);
    connect(m_modbus, &QModbusClient::errorOccurred, this, [this](QModbusDevice::Error) {
        emit errorOccurred(m_modbus->errorString());
    });
    connect(m_pollTimer, &QTimer::timeout, this, &Manager::onPollTimer);
}
Manager::~Manager()
{
    if (m_modbus)
        m_modbus->disconnectDevice();
}

void Manager::addSource(const Source &source)
{
    m_sources.append(source);
    m_recalcNeeded = true;
}
void Manager::clearSources()
{
    m_sources.clear();
    m_blocks.clear();
    m_recalcNeeded = true;
}
QVector<Source> Manager::getSources() const
{
    return m_sources;
}

Source Manager::getSourceConfig(const QString &id) const
{
    std::string target = id.toStdString();
    for (const auto &s : qAsConst(m_sources)) {
        if (s.id == target)
            return s;
    }
    return Source{};
}

void Manager::connectTo(const QString &ip, int port)
{
    if (m_modbus->state() != QModbusDevice::UnconnectedState)
        m_modbus->disconnectDevice();
    m_modbus->setConnectionParameter(QModbusDevice::NetworkPortParameter, port);
    m_modbus->setConnectionParameter(QModbusDevice::NetworkAddressParameter, ip);
    m_modbus->setTimeout(1000);
    m_modbus->connectDevice();
}

void Manager::disconnectFrom()
{
    m_modbus->disconnectDevice();
}
void Manager::startPolling(int intervalMs)
{
    if (m_recalcNeeded)
        recalculateBlocks();
    m_pollTimer->start(intervalMs);
}
void Manager::stopPolling()
{
    m_pollTimer->stop();
}

void Manager::onStateChanged(QModbusDevice::State state)
{
    emit connectionStateChanged(static_cast<int>(state));
    if (state == QModbusDevice::ConnectedState)
        qDebug() << "[EvoModbus] Connected";
}

// Write Logic
bool Manager::writeValue(int sa, int addr, const QVariant &val, ValueType type, ByteOrder order)
{
    // Упрощенная реализация записи (пример)
    QModbusDataUnit unit(QModbusDataUnit::HoldingRegisters, addr, 0);
    QVector<quint16> regs;

    if (type == ValueType::Bool) {
        unit.setRegisterType(QModbusDataUnit::Coils);
        unit.setValueCount(1);
        unit.setValue(0, val.toBool() ? 1 : 0);
    } else {
        if (type == ValueType::Float)
            regs = decomposeFloat(val.toFloat(), (int) order);
        else if (type == ValueType::UInt32 || type == ValueType::Int32)
            regs = decomposeUInt32(val.toUInt(), (int) order);
        else
            regs.append((quint16) val.toUInt());

        unit.setRegisterType(QModbusDataUnit::HoldingRegisters);
        unit.setValueCount(regs.size());
        unit.setValues(regs);
    }

    if (auto *reply = m_modbus->sendWriteRequest(unit, sa)) {
        connect(reply, &QModbusReply::finished, this, &Manager::onWriteReplyFinished);
        return true;
    }
    return false;
}
void Manager::onWriteReplyFinished()
{
    auto r = qobject_cast<QModbusReply *>(sender());
    if (r) {
        if (r->error() == QModbusDevice::NoError)
            emit writeFinished();
        else
            emit errorOccurred("Write: " + r->errorString());
        r->deleteLater();
    }
}

// Read Logic
void Manager::recalculateBlocks()
{
    m_blocks.clear();
    if (m_sources.isEmpty())
        return;
    QVector<const Source *> sorted;
    for (const auto &s : qAsConst(m_sources))
        sorted.append(&s);
    std::sort(sorted.begin(), sorted.end(), [](const Source *a, const Source *b) {
        if (a->serverAddress != b->serverAddress)
            return a->serverAddress < b->serverAddress;
        if (a->regType != b->regType)
            return a->regType < b->regType;
        return a->valueAddress < b->valueAddress;
    });

    RequestBlock cur{sorted[0]->serverAddress,
                     sorted[0]->regType,
                     sorted[0]->valueAddress,
                     getRegisterCount(sorted[0]->valueType)};
    for (int i = 1; i < sorted.size(); ++i) {
        const Source *n = sorted[i];
        int end = cur.startAddress + cur.count;
        int gap = n->valueAddress - end;
        int nLen = getRegisterCount(n->valueType);
        int newCount = (n->valueAddress + nLen) - cur.startAddress;
        int max = (cur.regType == QModbusDataUnit::Coils
                   || cur.regType == QModbusDataUnit::DiscreteInputs)
                      ? MAX_PDU_BITS
                      : MAX_PDU;
        if (n->serverAddress != cur.serverAddress || n->regType != cur.regType || gap > MAX_GAP
            || gap < 0 || newCount > max) {
            m_blocks.append(cur);
            cur = {n->serverAddress, n->regType, n->valueAddress, nLen};
        } else {
            cur.count = newCount;
        }
    }
    m_blocks.append(cur);
    m_recalcNeeded = false;
}

void Manager::onPollTimer()
{
    if (m_modbus->state() != QModbusDevice::ConnectedState)
        return;
    for (const auto &b : qAsConst(m_blocks)) {
        QModbusDataUnit unit(b.regType, b.startAddress, b.count);
        if (auto *r = m_modbus->sendReadRequest(unit, b.serverAddress)) {
            if (!r->isFinished())
                connect(r, &QModbusReply::finished, this, &Manager::onReadReady);
            else
                delete r;
        }
    }
}

void Manager::onReadReady()
{
    auto r = qobject_cast<QModbusReply *>(sender());
    if (!r)
        return;
    if (r->error() == QModbusDevice::NoError) {
        auto unit = r->result();
        bool chg = false;
        for (const auto &s : qAsConst(m_sources)) {
            if (s.serverAddress != r->serverAddress() || s.regType != unit.registerType())
                continue;
            int len = getRegisterCount(s.valueType);
            if (s.valueAddress >= unit.startAddress()
                && (s.valueAddress + len) <= (unit.startAddress() + unit.valueCount())) {
                int off = s.valueAddress - unit.startAddress();
                QVector<quint16> d;
                for (int k = 0; k < len; ++k)
                    d.append(unit.value(off + k));
                QVariant v = parseValue(s, d);
                QString k = QString::fromStdString(s.id);
                if (m_rawData.value(k) != v) {
                    m_rawData[k] = v;
                    chg = true;
                }
            }
        }
        if (chg)
            emit rawDataUpdated();
    }
    r->deleteLater();
}

int Manager::getRegisterCount(int t)
{
    return (t >= (int) ValueType::UInt32) ? 2 : 1;
}
QVariant Manager::parseValue(const Source &src, const QVector<quint16> &d)
{
    if (d.isEmpty())
        return {};
    ValueType t = (ValueType) src.valueType;
    if (t == ValueType::UInt16)
        return d[0];
    if (t == ValueType::Float && d.size() == 2) {
        quint32 i = composeUInt32(d[0], d[1], src.byteOrder);
        float f;
        std::memcpy(&f, &i, 4);
        return f;
    }
    return d[0];
}

// Helpers (Decompose/Compose) - simplified for brevity
quint32 Manager::composeUInt32(quint16 w1, quint16 w2, int order)
{
    if (order == 0)
        return (w1 << 16) | w2;
    return (w2 << 16) | w1; // Simplified ABCD vs CDAB
}
float Manager::composeFloat(quint16 w1, quint16 w2, int order)
{
    quint32 i = composeUInt32(w1, w2, order);
    float f;
    std::memcpy(&f, &i, 4);
    return f;
}
QVector<quint16> Manager::decomposeUInt32(quint32 val, int order)
{
    QVector<quint16> res;
    res.append(val >> 16);
    res.append(val & 0xFFFF);
    return res; // Simplified
}
QVector<quint16> Manager::decomposeFloat(float val, int order)
{
    quint32 i;
    std::memcpy(&i, &val, 4);
    return decomposeUInt32(i, order);
}

// --- CONTROLLER ---

Controller::Controller(QObject *parent)
    : QObject(parent)
{
    m_manager = new Manager(this);
    m_unitGateway = new EvoUnit::JsGateway(this);

    // JS Setup
    m_jsEngine.globalObject().setProperty("IO", m_jsEngine.newQObject(this));
    m_jsEngine.globalObject().setProperty("EvoUnit", m_jsEngine.newQObject(m_unitGateway));

    // Register EvoUnit Enums to JS globally
    QJSValue unitsObj = m_jsEngine.newObject();
    const QMetaObject &mo = EvoUnit::staticMetaObject;
    int index = mo.indexOfEnumerator("MeasUnit");
    QMetaEnum me = mo.enumerator(index);
    for (int i = 0; i < me.keyCount(); ++i) {
        unitsObj.setProperty(me.key(i), me.value(i));
    }
    m_jsEngine.globalObject().setProperty("Units", unitsObj);

    connect(m_manager, &Manager::rawDataUpdated, this, &Controller::onRawDataReceived);
}

void Controller::addModbusSource(const Source &s)
{
    m_manager->addSource(s);
}
void Controller::clearSources()
{
    m_manager->clearSources();
}
QVector<Source> Controller::getSources() const
{
    return m_manager->getSources();
}

void Controller::addComputedChannel(const ComputedChannel &ch)
{
    m_computedChannels.append(ch);
}
void Controller::clearComputedChannels()
{
    m_computedChannels.clear();
}
QVector<ComputedChannel> Controller::getComputedChannels() const
{
    return m_computedChannels;
}

void Controller::buildAndApplyScript()
{
    QString js;
    for (const auto &ch : qAsConst(m_computedChannels)) {
        js += QString("IO.set('%1', %2, %3);\n")
                  .arg(QString::fromStdString(ch.id))
                  .arg(ch.formula)
                  .arg((int) ch.unit);
    }
    setScript(js);
}

void Controller::setScript(const QString &code)
{
    QString full = "(function() { try { " + code + " } catch(e){ print('Script Error: ' + e); } })";
    m_jsProcessFunction = m_jsEngine.evaluate(full);
    if (m_jsProcessFunction.isError())
        emit error("JS Compile: " + m_jsProcessFunction.toString());
}

void Controller::connectToServer(const QString &ip, int port)
{
    m_manager->connectTo(ip, port);
}
void Controller::start(int ms)
{
    m_manager->startPolling(ms);
}
void Controller::stop()
{
    m_manager->stopPolling();
}

QVariant Controller::val(const QString &id)
{
    if (m_channels.contains(id))
        return m_channels[id].value;
    return 0.0;
}
void Controller::set(const QString &id, const QVariant &val, int unit)
{
    EvoUnit::MeasUnit u = (EvoUnit::MeasUnit) unit;
    if (u == EvoUnit::MeasUnit::Unknown && m_channels.contains(id))
        u = m_channels[id].unit;
    m_channels[id] = {val, u};
}
void Controller::write(const QString &id, const QVariant &val)
{
    auto cfg = m_manager->getSourceConfig(id);
    if (!cfg.id.empty())
        m_manager->writeValue(cfg.serverAddress,
                              cfg.valueAddress,
                              val,
                              (ValueType) cfg.valueType,
                              (ByteOrder) cfg.byteOrder);
}

void Controller::onRawDataReceived()
{
    QVariantMap raw = m_manager->getRawData();
    for (auto i = raw.begin(); i != raw.end(); ++i)
        m_channels[i.key()] = {i.value(), m_manager->getSourceConfig(i.key()).defaultUnit};

    if (m_jsProcessFunction.isCallable())
        m_jsProcessFunction.call();
    emit channelsUpdated();
}

} // namespace EvoModbus
