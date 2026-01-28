#ifndef MACHINECONTROL_H
#define MACHINECONTROL_H

#include <QModbusDataUnit>
#include <QObject>

class QModbusClient;
class QModbusReply;
class QTimer;

class MachineControl : public QObject
{
    Q_OBJECT

public:
    explicit MachineControl(QObject *parent = nullptr);
    ~MachineControl();

    // --- ПОДКЛЮЧЕНИЕ ---

    // Подключение через COM-порт (RS-485/USB)
    bool connectRTU(const QString &portName, int baudRate = 9600, int serverAddress = 1);

    // Подключение через Ethernet (Modbus TCP)
    bool connectTCP(const QString &ip, int port = 502, int serverAddress = 1);

    void disconnectDevice();
    bool isConnected() const;

    // --- БИТЫ УПРАВЛЕНИЯ (Адрес 0) ---
    enum ControlBit {
        BitStartTest = 0,
        BitStop = 1,
        BitLiftTraverse = 2,
        BitLowerTraverse = 3,
        BitStartMMMode = 4,
        BitStopMMMode = 5,
        BitUpMMMode = 6,
        BitDownMMMode = 7,
        BitServoAsync = 8,
        BitClamp = 9,
        BitTensileCompress = 10
    };

    void sendCommand(ControlBit bit, bool state);
    void setFullControlWord(quint16 word);

    // --- НАСТРОЙКИ (Holding Registers) ---
    void setSpeedMmMin(quint16 value);
    void setMoveByX(float value);
    void setMoveToX(float value);
    void setEncoderPulses(quint16 value);
    void setMotorRevs(quint16 value);
    void setScrewMmRev(quint16 value);
    void setSensorRange(float value);
    void setSensitivity(quint16 value);
    void setMaxError(float value);
    void setTestSpeed(quint16 value);
    void setManualSpeed(quint16 value);

    // --- ОПРОС ДАТЧИКОВ ---
    void startPolling(int intervalMs = 200);
    void stopPolling();

signals:
    void errorOccurred(QString errorMsg);
    void connected();
    void disconnected();

    // Данные датчиков
    void currentPositionChanged(float val);
    void currentLoadChanged(float val);
    void testTimeChanged(float val);
    void lengthChanged(float val);
    void maxLoadChanged(float val);

private slots:
    void onStateChanged(int state);
    void onReadReady();
    void doPoll();

private:
    QModbusClient *m_modbusDevice; // Полиморфный указатель
    int m_serverAddress;
    QTimer *m_pollTimer;
    quint16 m_currentControlWord;

    void initDeviceSignals(); // Хелпер для подключения сигналов
    void writeRegister(int address, quint16 value);
    void writeFloat(int address, float value);
    float decodeFloat(quint16 r1, quint16 r2);
};

#endif // MACHINECONTROL_H
