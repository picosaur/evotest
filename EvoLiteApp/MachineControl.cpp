#include "MachineControl.h"
#include <QDebug>
#include <QModbusReply>
#include <QModbusRtuSerialMaster>
#include <QModbusTcpClient>
#include <QSerialPort>
#include <QTimer>

// Адреса Holding (RW)
namespace RegRW {
const int Control = 0;
const int SpeedMmMin = 2;
const int MoveByX = 3;
const int MoveToX = 5;
const int EncoderPls = 7;
const int MotorRevs = 8;
const int ScrewMm = 9;
const int SensorRange = 10;
const int Sensitivity = 12;
const int MaxError = 13;
const int TestSpeed = 15;
const int ManualSpeed = 16;
} // namespace RegRW

// Адреса Input (RO)
namespace RegRO {
const int CurrentPos = 0;
const int CurrentLoad = 2;
const int TestTime = 4;
const int LengthStart = 6;
const int MaxLoad = 8;
const int TotalCount = 10;
} // namespace RegRO

MachineControl::MachineControl(QObject *parent)
    : QObject(parent)
    , m_modbusDevice(nullptr)
    , m_serverAddress(1)
    , m_currentControlWord(0)
{
    m_pollTimer = new QTimer(this);
    connect(m_pollTimer, &QTimer::timeout, this, &MachineControl::doPoll);
}

MachineControl::~MachineControl()
{
    disconnectDevice();
    if (m_modbusDevice)
        m_modbusDevice->deleteLater();
}

// === ПОДКЛЮЧЕНИЕ RTU (SERIAL) ===
bool MachineControl::connectRTU(const QString &portName, int baudRate, int serverAddress)
{
    disconnectDevice(); // Сначала отключаем и удаляем старое устройство

    // Создаем RTU мастер
    m_modbusDevice = new QModbusRtuSerialMaster(this);
    initDeviceSignals(); // Подключаем сигналы ошибок и статуса

    m_modbusDevice->setConnectionParameter(QModbusDevice::SerialPortNameParameter, portName);
    m_modbusDevice->setConnectionParameter(QModbusDevice::SerialParityParameter,
                                           QSerialPort::NoParity);
    m_modbusDevice->setConnectionParameter(QModbusDevice::SerialBaudRateParameter, baudRate);
    m_modbusDevice->setConnectionParameter(QModbusDevice::SerialDataBitsParameter,
                                           QSerialPort::Data8);
    m_modbusDevice->setConnectionParameter(QModbusDevice::SerialStopBitsParameter,
                                           QSerialPort::OneStop);

    m_modbusDevice->setTimeout(1000);
    m_modbusDevice->setNumberOfRetries(3);

    m_serverAddress = serverAddress;

    return m_modbusDevice->connectDevice();
}

// === ПОДКЛЮЧЕНИЕ TCP (ETHERNET) ===
bool MachineControl::connectTCP(const QString &ip, int port, int serverAddress)
{
    disconnectDevice(); // Сначала отключаем и удаляем старое устройство

    // Создаем TCP клиент
    m_modbusDevice = new QModbusTcpClient(this);
    initDeviceSignals(); // Подключаем сигналы ошибок и статуса

    m_modbusDevice->setConnectionParameter(QModbusDevice::NetworkAddressParameter, ip);
    m_modbusDevice->setConnectionParameter(QModbusDevice::NetworkPortParameter, port);

    m_modbusDevice->setTimeout(1000);
    m_modbusDevice->setNumberOfRetries(3);

    // В Modbus TCP serverAddress (Unit ID) тоже используется, если за TCP стоит шлюз,
    // или для идентификации устройства. Если устройство "чистый" TCP, часто это 1 или 255.
    m_serverAddress = serverAddress;

    return m_modbusDevice->connectDevice();
}

void MachineControl::initDeviceSignals()
{
    if (!m_modbusDevice)
        return;

    connect(m_modbusDevice, &QModbusClient::errorOccurred, [this](QModbusDevice::Error) {
        emit errorOccurred(m_modbusDevice->errorString());
    });

    connect(m_modbusDevice, &QModbusClient::stateChanged, this, &MachineControl::onStateChanged);
}

void MachineControl::disconnectDevice()
{
    stopPolling();
    if (m_modbusDevice) {
        if (m_modbusDevice->state() == QModbusDevice::ConnectedState)
            m_modbusDevice->disconnectDevice();

        // Важно удалить старый объект, чтобы создать новый (RTU или TCP) при следующем подключении
        delete m_modbusDevice;
        m_modbusDevice = nullptr;
    }
}

bool MachineControl::isConnected() const
{
    return m_modbusDevice && m_modbusDevice->state() == QModbusDevice::ConnectedState;
}

void MachineControl::onStateChanged(int state)
{
    if (state == QModbusDevice::ConnectedState)
        emit connected();
    else if (state == QModbusDevice::UnconnectedState)
        emit disconnected();
}

// --- ДАЛЕЕ КОД БЕЗ ИЗМЕНЕНИЙ (ЛОГИКА ОДИНАКОВА ДЛЯ TCP И RTU) ---

void MachineControl::sendCommand(ControlBit bit, bool state)
{
    if (state)
        m_currentControlWord |= (1 << bit);
    else
        m_currentControlWord &= ~(1 << bit);
    writeRegister(RegRW::Control, m_currentControlWord);
}

void MachineControl::setFullControlWord(quint16 word)
{
    m_currentControlWord = word;
    writeRegister(RegRW::Control, m_currentControlWord);
}

void MachineControl::setSpeedMmMin(quint16 value)
{
    writeRegister(RegRW::SpeedMmMin, value);
}
void MachineControl::setMoveByX(float value)
{
    writeFloat(RegRW::MoveByX, value);
}
void MachineControl::setMoveToX(float value)
{
    writeFloat(RegRW::MoveToX, value);
}
void MachineControl::setEncoderPulses(quint16 value)
{
    writeRegister(RegRW::EncoderPls, value);
}
void MachineControl::setMotorRevs(quint16 value)
{
    writeRegister(RegRW::MotorRevs, value);
}
void MachineControl::setScrewMmRev(quint16 value)
{
    writeRegister(RegRW::ScrewMm, value);
}
void MachineControl::setSensorRange(float value)
{
    writeFloat(RegRW::SensorRange, value);
}
void MachineControl::setSensitivity(quint16 value)
{
    writeRegister(RegRW::Sensitivity, value);
}
void MachineControl::setMaxError(float value)
{
    writeFloat(RegRW::MaxError, value);
}
void MachineControl::setTestSpeed(quint16 value)
{
    writeRegister(RegRW::TestSpeed, value);
}
void MachineControl::setManualSpeed(quint16 value)
{
    writeRegister(RegRW::ManualSpeed, value);
}

void MachineControl::writeRegister(int address, quint16 value)
{
    if (!isConnected())
        return;
    QModbusDataUnit writeUnit(QModbusDataUnit::HoldingRegisters, address, 1);
    writeUnit.setValue(0, value);

    if (auto *reply = m_modbusDevice->sendWriteRequest(writeUnit, m_serverAddress)) {
        if (!reply->isFinished()) {
            connect(reply, &QModbusReply::finished, reply, &QObject::deleteLater);
        } else {
            reply->deleteLater();
        }
    } else {
        emit errorOccurred(tr("Write error: ") + m_modbusDevice->errorString());
    }
}

void MachineControl::writeFloat(int address, float value)
{
    if (!isConnected())
        return;
    QVector<quint16> values;
    const quint8 *p = reinterpret_cast<const quint8 *>(&value);
    quint16 r1 = (quint16(p[3]) << 8) | p[2];
    quint16 r2 = (quint16(p[1]) << 8) | p[0];
    values.append(r1);
    values.append(r2);

    QModbusDataUnit writeUnit(QModbusDataUnit::HoldingRegisters, address, values);
    if (auto *reply = m_modbusDevice->sendWriteRequest(writeUnit, m_serverAddress)) {
        connect(reply, &QModbusReply::finished, reply, &QObject::deleteLater);
    } else {
        emit errorOccurred(tr("Write float error: ") + m_modbusDevice->errorString());
    }
}

void MachineControl::startPolling(int intervalMs)
{
    if (!m_pollTimer->isActive())
        m_pollTimer->start(intervalMs);
}

void MachineControl::stopPolling()
{
    m_pollTimer->stop();
}

void MachineControl::doPoll()
{
    if (!isConnected())
        return;
    // Опрос Input Registers (0-9)
    QModbusDataUnit readUnit(QModbusDataUnit::InputRegisters, RegRO::CurrentPos, RegRO::TotalCount);

    if (auto *reply = m_modbusDevice->sendReadRequest(readUnit, m_serverAddress)) {
        if (!reply->isFinished())
            connect(reply, &QModbusReply::finished, this, &MachineControl::onReadReady);
        else
            delete reply;
    }
}

void MachineControl::onReadReady()
{
    auto reply = qobject_cast<QModbusReply *>(sender());
    if (!reply)
        return;

    if (reply->error() == QModbusDevice::NoError) {
        const QModbusDataUnit unit = reply->result();
        if (unit.valueCount() >= 10) {
            emit currentPositionChanged(decodeFloat(unit.value(0), unit.value(1)));
            emit currentLoadChanged(decodeFloat(unit.value(2), unit.value(3)));
            emit testTimeChanged(decodeFloat(unit.value(4), unit.value(5)));
            emit lengthChanged(decodeFloat(unit.value(6), unit.value(7)));
            emit maxLoadChanged(decodeFloat(unit.value(8), unit.value(9)));
        }
    } else {
        emit errorOccurred(tr("Read error: ") + reply->errorString());
    }
    reply->deleteLater();
}

float MachineControl::decodeFloat(quint16 r1, quint16 r2)
{
    quint32 temp = (quint32(r1) << 16) | r2;
    float result;
    memcpy(&result, &temp, sizeof(float));
    return result;
}
