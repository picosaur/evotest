#include "EvoModbus.h"
#include <QDebug>
#include <algorithm>
#include <cstring> // std::memcpy

namespace EvoModbus {

// Константы оптимизации
static const int MAX_GAP{10};        // Макс разрыв регистров
static const int MAX_PDU{120};       // Макс регистров в пакете
static const int MAX_PDU_BITS{1900}; // Макс бит в пакете

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

Source Manager::getSourceConfig(const QString &id) const
{
    std::string target = id.toStdString();
    for (const auto &s : m_sources) {
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
    m_modbus->setNumberOfRetries(2);
    m_modbus->connectDevice();
}

void Manager::disconnectFrom()
{
    m_modbus->disconnectDevice();
}

void Manager::startPolling(int intervalMs)
{
    if (m_recalcNeeded) {
        recalculateBlocks();
    }
    m_pollTimer->start(intervalMs);
}

void Manager::stopPolling()
{
    m_pollTimer->stop();
}

void Manager::onStateChanged(QModbusDevice::State state)
{
    emit connectionStateChanged(static_cast<int>(state));
    if (state == QModbusDevice::ConnectedState) {
        qDebug() << "[EvoModbus] Connected to PLC";
    }
}

// ---------------------------------------------------------
// ALGORITHM: Grouping
// ---------------------------------------------------------
void Manager::recalculateBlocks()
{
    m_blocks.clear();
    if (m_sources.isEmpty())
        return;

    QVector<const Source *> sorted{};
    sorted.reserve(m_sources.size());
    for (const auto &s : m_sources)
        sorted.append(&s);

    // Сортировка: Server -> RegType -> Address
    std::sort(sorted.begin(), sorted.end(), [](const Source *a, const Source *b) {
        if (a->serverAddress != b->serverAddress)
            return a->serverAddress < b->serverAddress;
        if (a->regType != b->regType)
            return a->regType < b->regType;
        return a->valueAddress < b->valueAddress;
    });

    RequestBlock currentBlock{};
    const Source *first = sorted.first();

    currentBlock.serverAddress = first->serverAddress;
    currentBlock.regType = first->regType;
    currentBlock.startAddress = first->valueAddress;
    currentBlock.count = getRegisterCount(first->valueType);

    for (int i = 1; i < sorted.size(); ++i) {
        const Source *next = sorted[i];

        bool sameServer = (next->serverAddress == currentBlock.serverAddress);
        bool sameType = (next->regType == currentBlock.regType);

        int currentEnd = currentBlock.startAddress + currentBlock.count;
        int gap = next->valueAddress - currentEnd;

        int nextLen = getRegisterCount(next->valueType);
        int newCount = (next->valueAddress + nextLen) - currentBlock.startAddress;

        // Лимит зависит от типа (биты или слова)
        int maxSize = (currentBlock.regType == QModbusDataUnit::Coils
                       || currentBlock.regType == QModbusDataUnit::DiscreteInputs)
                          ? MAX_PDU_BITS
                          : MAX_PDU;

        if (!sameServer || !sameType || gap > MAX_GAP || gap < 0 || newCount > maxSize) {
            m_blocks.append(currentBlock);

            currentBlock.serverAddress = next->serverAddress;
            currentBlock.regType = next->regType;
            currentBlock.startAddress = next->valueAddress;
            currentBlock.count = nextLen;
        } else {
            currentBlock.count = newCount;
        }
    }
    m_blocks.append(currentBlock);
    m_recalcNeeded = false;

    qDebug() << "[EvoModbus] Optimized into" << m_blocks.size() << "requests";
}

// ---------------------------------------------------------
// POLLING & PARSING
// ---------------------------------------------------------
void Manager::onPollTimer()
{
    if (m_modbus->state() != QModbusDevice::ConnectedState)
        return;

    for (const auto &block : m_blocks) {
        QModbusDataUnit unit(block.regType, block.startAddress, block.count);

        if (auto *reply = m_modbus->sendReadRequest(unit, block.serverAddress)) {
            if (!reply->isFinished()) {
                connect(reply, &QModbusReply::finished, this, &Manager::onReadReady);
            } else {
                delete reply;
            }
        }
    }
}

void Manager::onReadReady()
{
    auto reply = qobject_cast<QModbusReply *>(sender());
    if (!reply)
        return;

    if (reply->error() == QModbusDevice::NoError) {
        const QModbusDataUnit unit = reply->result();
        bool changed{false};

        for (const auto &src : m_sources) {
            if (src.serverAddress != reply->serverAddress() || src.regType != unit.registerType())
                continue;

            int len = getRegisterCount(src.valueType);
            int srcEnd = src.valueAddress + len;
            int unitEnd = unit.startAddress() + unit.valueCount();

            if (src.valueAddress >= unit.startAddress() && srcEnd <= unitEnd) {
                int offset = src.valueAddress - unit.startAddress();
                QVector<quint16> raw{};

                if (src.isBitType()) {
                    raw.append(unit.value(offset));
                } else {
                    for (int k = 0; k < len; ++k)
                        raw.append(unit.value(offset + k));
                }

                QVariant val = parseValue(src, raw);
                QString key = QString::fromStdString(src.id);

                if (m_rawData.value(key) != val) {
                    m_rawData[key] = val;
                    changed = true;
                }
            }
        }
        if (changed)
            emit rawDataUpdated();

    } else {
        // Лог ошибок можно раскомментировать
        // qWarning() << "[EvoModbus] Read Error:" << reply->errorString();
    }
    reply->deleteLater();
}

int Manager::getRegisterCount(int t)
{
    if (t >= static_cast<int>(ValueType::UInt32))
        return 2;
    return 1;
}

QVariant Manager::parseValue(const Source &src, const QVector<quint16> &d)
{
    if (d.isEmpty())
        return QVariant();

    ValueType t = static_cast<ValueType>(src.valueType);

    if (t == ValueType::Bool)
        return (bool) d[0];
    if (t == ValueType::UInt16)
        return d[0];
    if (t == ValueType::Int16)
        return (qint16) d[0];

    if (d.size() < 2)
        return QVariant();

    if (t == ValueType::UInt32)
        return composeUInt32(d[0], d[1], src.byteOrder);
    if (t == ValueType::Int32)
        return (qint32) composeUInt32(d[0], d[1], src.byteOrder);
    if (t == ValueType::Float)
        return composeFloat(d[0], d[1], src.byteOrder);

    return QVariant();
}

quint32 Manager::composeUInt32(quint16 w1, quint16 w2, int order)
{
    quint8 a = (w1 >> 8) & 0xFF;
    quint8 b = w1 & 0xFF;
    quint8 c = (w2 >> 8) & 0xFF;
    quint8 d = w2 & 0xFF;

    switch (static_cast<ByteOrder>(order)) {
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
    float f{0.0f};
    std::memcpy(&f, &i, sizeof(float));
    return f;
}

// =========================================================
// CONTROLLER IMPLEMENTATION
// =========================================================

Controller::Controller(QObject *parent)
    : QObject(parent)
{
    m_manager = new Manager(this);
    m_unitGateway = new EvoUnit::JsGateway(this);

    // Подготовка JS среды
    m_jsEngine.globalObject().setProperty("IO", m_jsEngine.newQObject(this));
    m_jsEngine.globalObject().setProperty("EvoUnit", m_jsEngine.newQObject(m_unitGateway));

    connect(m_manager, &Manager::rawDataUpdated, this, &Controller::onRawDataReceived);
}

void Controller::addModbusSource(const Source &source)
{
    m_manager->addSource(source);
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
    s.regType = static_cast<QModbusDataUnit::RegisterType>(c.value("reg", 4).toInt());
    s.byteOrder = c.value("byteOrder", 0).toInt();

    // Парсим Unit из конфига
    if (c.contains("unit")) {
        s.defaultUnit = static_cast<EvoUnit::MeasUnit>(c["unit"].toInt());
    }

    m_manager->addSource(s);
}

void Controller::setScript(const QString &scriptCode)
{
    QString fullScript = "(function() { " + scriptCode + " })";
    m_jsProcessFunction = m_jsEngine.evaluate(fullScript);

    if (m_jsProcessFunction.isError()) {
        qWarning() << "[EvoModbus] Script Error:" << m_jsProcessFunction.toString();
    }
}

void Controller::connectToServer(const QString &ip, int port)
{
    m_manager->connectTo(ip, port);
}

void Controller::start(int intervalMs)
{
    m_manager->startPolling(intervalMs);
}

void Controller::stop()
{
    m_manager->stopPolling();
}

QVariant Controller::val(const QString &id)
{
    // Возвращаем просто значение для арифметики в JS
    if (m_channels.contains(id)) {
        return m_channels[id].value;
    }
    return 0.0;
}

void Controller::set(const QString &id, const QVariant &value, int unit)
{
    EvoUnit::MeasUnit u = static_cast<EvoUnit::MeasUnit>(unit);

    // Если юнит не передан (Unknown), пытаемся сохранить существующий
    if (u == EvoUnit::MeasUnit::Unknown && m_channels.contains(id)) {
        u = m_channels[id].unit;
    }

    m_channels[id] = {value, u};
}

void Controller::onRawDataReceived()
{
    QVariantMap raw = m_manager->getRawData();

    // 1. Обновляем каналы сырыми данными (с учетом их дефолтных единиц)
    for (auto it = raw.begin(); it != raw.end(); ++it) {
        QString id = it.key();
        Source cfg = m_manager->getSourceConfig(id);
        m_channels[id] = {it.value(), cfg.defaultUnit};
    }

    // 2. Выполняем JS логику (она может перезаписать каналы или создать новые)
    if (m_jsProcessFunction.isCallable()) {
        QJSValue res = m_jsProcessFunction.call();
        if (res.isError()) {
            qWarning() << "[EvoModbus] JS Runtime Error:" << res.toString();
        }
    }

    // 3. Уведомляем UI
    emit channelsUpdated();
}

// =========================================================
// BINDER IMPLEMENTATION
// =========================================================

Binder::Binder(Controller *controller, QObject *parent)
    : QObject(parent)
    , m_controller(controller)
{
    if (m_controller) {
        connect(m_controller, &Controller::channelsUpdated, this, &Binder::syncUI);
    }
}

void Binder::bindLabel(QLabel *label, const std::string &channelId)
{
    if (!label)
        return;
    m_bindings.append({label, channelId, 0, 1.0, nullptr});
    syncUI();
}

void Binder::bindLCD(QLCDNumber *lcd, const std::string &channelId, double scale)
{
    if (!lcd)
        return;
    m_bindings.append({lcd, channelId, 1, scale, nullptr});
    syncUI();
}

void Binder::bindProgressBar(QProgressBar *bar, const std::string &channelId)
{
    if (!bar)
        return;
    m_bindings.append({bar, channelId, 2, 1.0, nullptr});
    syncUI();
}

void Binder::bindCustom(const std::string &channelId, Callback callback)
{
    m_bindings.append({nullptr, channelId, 3, 1.0, callback});
    syncUI();
}

void Binder::syncUI()
{
    if (!m_controller)
        return;
    auto channels = m_controller->getChannels();

    for (const auto &b : m_bindings) {
        QString key = QString::fromStdString(b.id);
        if (!channels.contains(key))
            continue;

        ChannelData data = channels[key];
        double val = data.value.toDouble();

        // Custom Handler
        if (b.type == 3 && b.customCb) {
            b.customCb(data);
            continue;
        }

        if (b.widget.isNull())
            continue;

        if (b.type == 0) { // QLabel
            // АВТОМАТИЧЕСКОЕ ФОРМАТИРОВАНИЕ ЧЕРЕЗ EvoUnit
            // Если unit == Unknown, просто число
            // Если unit == Volt, будет "120.5 V"
            QString txt = EvoUnit::format(val * b.scale, data.unit, 2);
            static_cast<QLabel *>(b.widget.data())->setText(txt);
        } else if (b.type == 1) { // QLCDNumber
            static_cast<QLCDNumber *>(b.widget.data())->display(val * b.scale);
        } else if (b.type == 2) { // QProgressBar
            static_cast<QProgressBar *>(b.widget.data())->setValue(static_cast<int>(val));
        }
    }
}

} // namespace EvoModbus
