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

// =========================================================
// MANAGER IMPLEMENTATION
// =========================================================

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
bool Manager::writeBit(int serverAddress, int address, bool value)
{
    if (m_modbus->state() != QModbusDevice::ConnectedState)
        return false;
    QModbusDataUnit unit(QModbusDataUnit::Coils, address, 1);
    unit.setValue(0, value ? 1 : 0);
    if (auto *reply = m_modbus->sendWriteRequest(unit, serverAddress)) {
        if (!reply->isFinished())
            connect(reply, &QModbusReply::finished, this, &Manager::onWriteReplyFinished);
        else
            reply->deleteLater();
        return true;
    }
    return false;
}
bool Manager::writeMultipleRegisters(int serverAddress,
                                     int startAddress,
                                     const QVector<quint16> &values)
{
    if (m_modbus->state() != QModbusDevice::ConnectedState || values.isEmpty())
        return false;
    QModbusDataUnit unit(QModbusDataUnit::HoldingRegisters, startAddress, values.size());
    unit.setValues(values);
    if (auto *reply = m_modbus->sendWriteRequest(unit, serverAddress)) {
        if (!reply->isFinished())
            connect(reply, &QModbusReply::finished, this, &Manager::onWriteReplyFinished);
        else
            reply->deleteLater();
        return true;
    }
    return false;
}
bool Manager::writeValue(int sa, int addr, const QVariant &val, ValueType type, ByteOrder order)
{
    if (type == ValueType::Bool)
        return writeBit(sa, addr, val.toBool());
    QVector<quint16> regs;
    if (type == ValueType::UInt16 || type == ValueType::Int16)
        regs.append(static_cast<quint16>(val.toUInt()));
    else if (type == ValueType::UInt32 || type == ValueType::Int32)
        regs = decomposeUInt32(val.toUInt(), (int) order);
    else if (type == ValueType::Float)
        regs = decomposeFloat(val.toFloat(), (int) order);
    else
        return false;
    return writeMultipleRegisters(sa, addr, regs);
}
void Manager::onWriteReplyFinished()
{
    auto reply = qobject_cast<QModbusReply *>(sender());
    if (!reply)
        return;
    if (reply->error() != QModbusDevice::NoError)
        emit errorOccurred("Write Error: " + reply->errorString());
    else
        emit writeFinished();
    reply->deleteLater();
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
                if (s.isBitType())
                    d.append(unit.value(off));
                else
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

// Helpers
int Manager::getRegisterCount(int t)
{
    return (t >= (int) ValueType::UInt32) ? 2 : 1;
}
QVariant Manager::parseValue(const Source &src, const QVector<quint16> &d)
{
    if (d.isEmpty())
        return {};
    ValueType t = (ValueType) src.valueType;
    if (t == ValueType::Bool)
        return (bool) d[0];
    if (t == ValueType::UInt16)
        return d[0];
    if (t == ValueType::Int16)
        return (qint16) d[0];
    if (d.size() < 2)
        return {};
    if (t == ValueType::UInt32)
        return composeUInt32(d[0], d[1], src.byteOrder);
    if (t == ValueType::Int32)
        return (qint32) composeUInt32(d[0], d[1], src.byteOrder);
    if (t == ValueType::Float)
        return composeFloat(d[0], d[1], src.byteOrder);
    return {};
}
quint32 Manager::composeUInt32(quint16 w1, quint16 w2, int order)
{
    quint8 a = (w1 >> 8) & 0xFF, b = w1 & 0xFF, c = (w2 >> 8) & 0xFF, d = w2 & 0xFF;
    switch ((ByteOrder) order) {
    case ByteOrder::ABCD:
        return (w1 << 16) | w2;
    case ByteOrder::CDAB:
        return (w2 << 16) | w1;
    case ByteOrder::DCBA:
        return (d << 24) | (c << 16) | (b << 8) | a;
    case ByteOrder::BADC:
        return (b << 24) | (a << 16) | (d << 8) | c;
    }
    return 0;
}
float Manager::composeFloat(quint16 w1, quint16 w2, int order)
{
    quint32 i = composeUInt32(w1, w2, order);
    float f;
    std::memcpy(&f, &i, sizeof(float));
    return f;
}
QVector<quint16> Manager::decomposeUInt32(quint32 val, int order)
{
    QVector<quint16> res;
    quint8 a = (val >> 24) & 0xFF;
    quint8 b = (val >> 16) & 0xFF;
    quint8 c = (val >> 8) & 0xFF;
    quint8 d = val & 0xFF;
    quint16 w1 = 0, w2 = 0;
    switch ((ByteOrder) order) {
    case ByteOrder::ABCD:
        w1 = (a << 8) | b;
        w2 = (c << 8) | d;
        break;
    case ByteOrder::CDAB:
        w1 = (c << 8) | d;
        w2 = (a << 8) | b;
        break;
    case ByteOrder::DCBA:
        w1 = (d << 8) | c;
        w2 = (b << 8) | a;
        break;
    case ByteOrder::BADC:
        w1 = (b << 8) | a;
        w2 = (d << 8) | c;
        break;
    }
    res.append(w1);
    res.append(w2);
    return res;
}
QVector<quint16> Manager::decomposeFloat(float val, int order)
{
    quint32 i;
    std::memcpy(&i, &val, sizeof(float));
    return decomposeUInt32(i, order);
}

// =========================================================
// CONTROLLER IMPLEMENTATION
// =========================================================

Controller::Controller(QObject *parent)
    : QObject(parent)
{
    m_manager = new Manager(this);
    m_unitGateway = new EvoUnit::JsGateway(this);

    // JS Setup
    m_jsEngine.globalObject().setProperty("IO", m_jsEngine.newQObject(this));
    m_jsEngine.globalObject().setProperty("EvoUnit", m_jsEngine.newQObject(m_unitGateway));

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
    QString js = "";

    for (const auto &ch : qAsConst(m_computedChannels)) {
        // Мы ожидаем, что ch.formula - это тело функции, которое возвращает значение.
        // Оборачиваем в IIFE (Immediately Invoked Function Expression) для изоляции скоупа
        // и передаем результат в IO.set

        QString funcBody = ch.formula;
        QString channelId = QString::fromStdString(ch.id);
        int unitId = (int) ch.unit;

        // Генерация кода:
        // IO.set('id', (function(){ ... user code ... })(), unit);

        QString line = QString("IO.set('%1', (function(){ %2 })(), %3);")
                           .arg(channelId)
                           .arg(funcBody)
                           .arg(unitId);

        js += line + "\n";
    }

    // Устанавливаем итоговый скрипт, который будет выполняться каждый цикл
    setScript(js);
}

void Controller::setScript(const QString &code)
{
    // Оборачиваем весь скрипт в try-catch, чтобы ошибка в одной формуле не валила всё приложение
    QString full = "(function() { try { " + code + " } catch(e){ print('Script Error: ' + e); } })";

    // Компилируем скрипт в функцию
    m_jsProcessFunction = m_jsEngine.evaluate(full);

    if (m_jsProcessFunction.isError()) {
        emit error("JS Compile Error: " + m_jsProcessFunction.toString());
        qWarning() << "JS Error:" << m_jsProcessFunction.toString();
    }
}

// [FIX] Проверка уникальности
bool Controller::isIdUnique(const QString &id) const
{
    std::string sId = id.toStdString();
    for (const auto &s : m_manager->getSources())
        if (s.id == sId)
            return false;
    for (const auto &ch : m_computedChannels)
        if (ch.id == sId)
            return false;
    // Проверка текущих каналов тоже (опционально, но лучше да)
    if (m_channels.contains(id))
        return false;
    return true;
}

void Controller::addSource(const QVariantMap &c)
{
    Source s{};
    if (c.contains("id"))
        s.id = c["id"].toString().toStdString();
    else
        return;
    s.serverAddress = c.value("server", 1).toInt();
    s.valueAddress = c.value("address", 0).toInt();
    s.valueType = c.value("type", 1).toInt();
    s.regType = (QModbusDataUnit::RegisterType) c.value("reg", 4).toInt();
    s.byteOrder = c.value("byteOrder", 0).toInt();
    if (c.contains("unit"))
        s.defaultUnit = static_cast<EvoUnit::MeasUnit>(c["unit"].toInt());
    m_manager->addSource(s);
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

// =========================================================
// BINDER IMPLEMENTATION
// =========================================================

Binder::Binder(Controller *c, QObject *p)
    : QObject(p)
    , m_controller(c)
{
    connect(m_controller, &Controller::channelsUpdated, this, &Binder::syncUI);
}
void Binder::bindLabel(QLabel *w, const std::string &id)
{
    if (w) {
        m_bindings.append({w, id, 0, 1.0, nullptr});
        syncUI();
    }
}
void Binder::bindLCD(QLCDNumber *w, const std::string &id, double s)
{
    if (w) {
        m_bindings.append({w, id, 1, s, nullptr});
        syncUI();
    }
}
void Binder::bindProgressBar(QProgressBar *w, const std::string &id)
{
    if (w) {
        m_bindings.append({w, id, 2, 1.0, nullptr});
        syncUI();
    }
}
void Binder::bindCustom(const std::string &id, Callback cb)
{
    m_bindings.append({nullptr, id, 3, 1.0, cb});
    syncUI();
}

void Binder::syncUI()
{
    auto ch = m_controller->getChannels();
    for (const auto &b : qAsConst(m_bindings)) {
        QString key = QString::fromStdString(b.id);
        if (!ch.contains(key))
            continue;
        ChannelData d = ch[key];
        double v = d.value.toDouble();
        if (b.type == 3 && b.customCb) {
            b.customCb(d);
            continue;
        }
        if (b.widget.isNull())
            continue;
        if (b.type == 0)
            static_cast<QLabel *>(b.widget.data())->setText(EvoUnit::format(v * b.scale, d.unit, 2));
        else if (b.type == 1)
            static_cast<QLCDNumber *>(b.widget.data())->display(v * b.scale);
        else if (b.type == 2)
            static_cast<QProgressBar *>(b.widget.data())->setValue((int) v);
    }
}

} // namespace EvoModbus
