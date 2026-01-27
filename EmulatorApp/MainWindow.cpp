#include "mainwindow.h"
#include <QDataStream>
#include <QDateTime>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QMessageBox>
#include <QRandomGenerator>
#include <QVBoxLayout>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setupUi();

    modbusDevice = new LoggingModbusServer(this);
    connect(modbusDevice, &LoggingModbusServer::logMessage, this, &MainWindow::onLogMessage);
    connect(modbusDevice, &QModbusServer::stateChanged, this, &MainWindow::onStateChanged);
    connect(modbusDevice, &QModbusServer::dataWritten, this, &MainWindow::handleDataWritten);

    simTimer = new QTimer(this);
    connect(simTimer, &QTimer::timeout, this, &MainWindow::updateSimulation);
}

MainWindow::~MainWindow()
{
    if (modbusDevice)
        modbusDevice->disconnectDevice();
}

void MainWindow::setupUi()
{
    setWindowTitle("EmulatorApp");

    QWidget *central = new QWidget(this);
    setCentralWidget(central);
    QVBoxLayout *mainLay = new QVBoxLayout(central);

    // --- ГРУППА НАСТРОЕК ---
    QGroupBox *grpSet = new QGroupBox("Настройки соединения");
    QHBoxLayout *laySet = new QHBoxLayout(grpSet);

    leIp = new QLineEdit("0.0.0.0"); // Слушаем все интерфейсы по умолчанию
    leIp->setToolTip("0.0.0.0 для доступа извне, 127.0.0.1 только локально");

    sbPort = new QSpinBox();
    sbPort->setRange(1, 65535);
    sbPort->setValue(5020);

    // Новое поле для Slave ID
    sbSlaveId = new QSpinBox();
    sbSlaveId->setRange(1, 247); // Стандартный диапазон Modbus
    sbSlaveId->setValue(1);

    btnStart = new QPushButton("Старт");
    btnStop = new QPushButton("Стоп");
    btnStop->setEnabled(false);

    laySet->addWidget(new QLabel("IP:"));
    laySet->addWidget(leIp);
    laySet->addWidget(new QLabel("Port:"));
    laySet->addWidget(sbPort);
    laySet->addWidget(new QLabel("Slave ID:")); // Подпись
    laySet->addWidget(sbSlaveId);               // Виджет
    laySet->addWidget(btnStart);
    laySet->addWidget(btnStop);

    // --- ТАБЛИЦЫ ---
    QGroupBox *grpRegs = new QGroupBox("Карта регистров (Монитор)");
    QHBoxLayout *layRegs = new QHBoxLayout(grpRegs);

    tableHolding = new QTableWidget(20, 2);
    tableHolding->setHorizontalHeaderLabels({"Addr", "Holding (RW)"});
    tableHolding->setColumnWidth(0, 40);
    tableHolding->setColumnWidth(1, 140);

    tableInput = new QTableWidget(10, 2);
    tableInput->setHorizontalHeaderLabels({"Addr", "Input (RO)"});
    tableInput->setColumnWidth(0, 40);
    tableInput->setColumnWidth(1, 140);

    layRegs->addWidget(tableHolding);
    layRegs->addWidget(tableInput);

    // --- ЛОГ ---
    txtLog = new QTextEdit();
    txtLog->setReadOnly(true);

    mainLay->addWidget(grpSet);
    mainLay->addWidget(grpRegs);
    mainLay->addWidget(new QLabel("Журнал операций:"));
    mainLay->addWidget(txtLog);

    connect(btnStart, &QPushButton::clicked, this, &MainWindow::onBtnStartClicked);
    connect(btnStop, &QPushButton::clicked, this, &MainWindow::onBtnStopClicked);

    resize(850, 600);
}

void MainWindow::onBtnStartClicked()
{
    if (!modbusDevice)
        return;

    QModbusDataUnitMap regMap;
    regMap.insert(QModbusDataUnit::HoldingRegisters, {QModbusDataUnit::HoldingRegisters, 0, 50});
    regMap.insert(QModbusDataUnit::InputRegisters, {QModbusDataUnit::InputRegisters, 0, 50});

    modbusDevice->setMap(regMap);

    // --- ПРИМЕНЯЕМ SLAVE ID ИЗ GUI ---
    modbusDevice->setServerAddress(sbSlaveId->value());

    modbusDevice->setConnectionParameter(QModbusDevice::NetworkAddressParameter, leIp->text());
    modbusDevice->setConnectionParameter(QModbusDevice::NetworkPortParameter, sbPort->value());

    if (modbusDevice->connectDevice()) {
        btnStart->setEnabled(false);
        btnStop->setEnabled(true);
        // Блокируем настройки пока сервер запущен
        leIp->setEnabled(false);
        sbPort->setEnabled(false);
        sbSlaveId->setEnabled(false);

        simTimer->start(100);

        // Инициализация дефолтных значений
        setHoldingInt(H_TEST_SPEED, 50);
        setHoldingInt(H_MANUAL_SPEED, 200);

        onLogMessage(QString("СЕРВЕР ЗАПУЩЕН. IP: %1 : %2 | Slave ID: %3")
                         .arg(leIp->text())
                         .arg(sbPort->value())
                         .arg(sbSlaveId->value()));
    } else {
        onLogMessage("Ошибка запуска: " + modbusDevice->errorString());
    }
}

void MainWindow::onBtnStopClicked()
{
    if (modbusDevice)
        modbusDevice->disconnectDevice();
    simTimer->stop();

    btnStart->setEnabled(true);
    btnStop->setEnabled(false);
    // Разблокируем настройки
    leIp->setEnabled(true);
    sbPort->setEnabled(true);
    sbSlaveId->setEnabled(true);

    onLogMessage("Сервер остановлен.");
}

void MainWindow::onLogMessage(const QString &msg)
{
    QString ts = QDateTime::currentDateTime().toString("HH:mm:ss.zzz");
    txtLog->append(QString("[%1] %2").arg(ts, msg));
}

void MainWindow::onStateChanged(QModbusDevice::State state)
{
    // Оставим пустым или добавим детальный статус,
    // основное логирование идет через onBtnStart/Stop
}

void MainWindow::handleDataWritten(QModbusDataUnit::RegisterType type, int address, int size)
{
    if (type == QModbusDataUnit::HoldingRegisters) {
        // Доп. логирование если нужно
    }
}

// === ФИЗИКА ===
void MainWindow::updateSimulation()
{
    if (modbusDevice->state() != QModbusDevice::ConnectedState)
        return;

    quint16 cmd = getHoldingInt(H_CMD);

    bool bStartTest = getBit(cmd, 0);
    bool bStop = getBit(cmd, 1);
    bool bUp = getBit(cmd, 2);
    bool bDown = getBit(cmd, 3);
    if (getBit(cmd, 5))
        bStop = true;

    float dt = 0.1f;

    if (bStop) {
        isTestRunning = false;
    }

    if (bStartTest && !isTestRunning && !bStop) {
        isTestRunning = true;
        testTime = 0;
        maxLoad = 0;
        startPos = currentPos;
        onLogMessage("--> СТАРТ ИСПЫТАНИЯ");
    }

    if (isTestRunning) {
        float speed = (float) getHoldingInt(H_TEST_SPEED);
        if (speed <= 0.1f)
            speed = 10.0f;

        currentPos += (speed / 60.0f) * dt;
        testTime += dt;

        float elongation = currentPos - startPos;
        if (elongation < 0)
            elongation = 0;

        float k = 10.0f;
        float noise = (QRandomGenerator::global()->generateDouble() - 0.5) * 0.05;

        currentLoad = (elongation * k) + noise;
        if (currentLoad > maxLoad)
            maxLoad = currentLoad;

        setInputFloat(I_TIME, testTime);
        setInputFloat(I_ELONGATION, elongation);
        setInputFloat(I_MAX_LOAD, maxLoad);

    } else {
        // Ручной
        float mSpeed = (float) getHoldingInt(H_MANUAL_SPEED);
        if (mSpeed <= 0.1f)
            mSpeed = 50.0f;

        if (bUp && !bStop) {
            currentPos += (mSpeed / 60.0f) * dt;
        } else if (bDown && !bStop) {
            currentPos -= (mSpeed / 60.0f) * dt;
            if (currentPos < 0)
                currentPos = 0;
        }

        currentLoad = (QRandomGenerator::global()->generateDouble() - 0.5) * 0.02;
    }

    setInputFloat(I_POS, currentPos);
    setInputFloat(I_LOAD, currentLoad);

    updateTables();
}

void MainWindow::updateTables()
{
    // Обновляем UI таблицы (Holding)
    for (int i = 0; i < 20; i++) {
        quint16 val;
        modbusDevice->data(QModbusDataUnit::HoldingRegisters, i, &val);

        if (tableHolding->item(i, 0) == nullptr) {
            tableHolding->setItem(i, 0, new QTableWidgetItem(QString::number(i)));
            tableHolding->setItem(i, 1, new QTableWidgetItem());
        }

        QString strVal = QString::number(val);
        if (i == H_SHIFT_X || i == H_MOVE_X || i == H_TEST_SPEED || i == H_MANUAL_SPEED) {
            // Можно добавить расшифровку для удобства
        }
        tableHolding->item(i, 1)->setText(strVal);
    }

    // Обновляем UI таблицы (Input)
    for (int i = 0; i < 10; i++) {
        quint16 val;
        modbusDevice->data(QModbusDataUnit::InputRegisters, i, &val);

        if (tableInput->item(i, 0) == nullptr) {
            tableInput->setItem(i, 0, new QTableWidgetItem(QString::number(i)));
            tableInput->setItem(i, 1, new QTableWidgetItem());
        }

        QString strVal = QString::number(val);
        // Визуальная подсказка для float значений
        if (i == I_POS && i + 1 < 10)
            strVal += QString(" (Pos: %1)").arg(currentPos, 0, 'f', 1);
        if (i == I_LOAD && i + 1 < 10)
            strVal += QString(" (Load: %1)").arg(currentLoad, 0, 'f', 1);

        tableInput->item(i, 1)->setText(strVal);
    }
}

// Helpers
bool MainWindow::getBit(quint16 val, int bit)
{
    return (val >> bit) & 1;
}
quint16 MainWindow::getHoldingInt(int addr)
{
    quint16 v;
    modbusDevice->data(QModbusDataUnit::HoldingRegisters, addr, &v);
    return v;
}
void MainWindow::setHoldingInt(int addr, quint16 val)
{
    modbusDevice->setData(QModbusDataUnit::HoldingRegisters, addr, val);
}
void MainWindow::setInputFloat(int addr, float val)
{
    QByteArray buf;
    QDataStream s(&buf, QIODevice::WriteOnly);
    s.setFloatingPointPrecision(QDataStream::SinglePrecision);
    s << val;
    quint16 h = (static_cast<quint8>(buf[0]) << 8) | static_cast<quint8>(buf[1]);
    quint16 l = (static_cast<quint8>(buf[2]) << 8) | static_cast<quint8>(buf[3]);
    modbusDevice->setData(QModbusDataUnit::InputRegisters, addr, h);
    modbusDevice->setData(QModbusDataUnit::InputRegisters, addr + 1, l);
}
float MainWindow::getHoldingFloat(int addr)
{
    quint16 h, l;
    modbusDevice->data(QModbusDataUnit::HoldingRegisters, addr, &h);
    modbusDevice->data(QModbusDataUnit::HoldingRegisters, addr + 1, &l);
    QByteArray buf;
    buf.resize(4);
    buf[0] = (h >> 8) & 0xFF;
    buf[1] = h & 0xFF;
    buf[2] = (l >> 8) & 0xFF;
    buf[3] = l & 0xFF;
    float val;
    QDataStream s(buf);
    s.setFloatingPointPrecision(QDataStream::SinglePrecision);
    s >> val;
    return val;
}
