#include "MainWindow.h"
#include <QDataStream>
#include <QDateTime>
#include <QDebug>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QRandomGenerator>
#include <QVBoxLayout>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setupUi();

    // Создаем наш кастомный сервер с логированием
    modbusDevice = new LoggingModbusServer(this);
    connect(modbusDevice, &LoggingModbusServer::logMessage, this, &MainWindow::onLogMessage);

    // !!! ВАЖНО: Подключаем сигнал записи для мгновенной реакции на кнопки !!!
    connect(modbusDevice, &QModbusServer::dataWritten, this, &MainWindow::handleDataWritten);

    // Таймер физики процесса (100 мс = 10 Гц)
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
    QWidget *central = new QWidget(this);
    setCentralWidget(central);
    QVBoxLayout *mainLay = new QVBoxLayout(central);

    // --- Группа Настроек ---
    QGroupBox *grpSet = new QGroupBox("Настройки сервера (ПЛК)");
    QHBoxLayout *laySet = new QHBoxLayout(grpSet);

    leIp = new QLineEdit("0.0.0.0");
    leIp->setToolTip("0.0.0.0 - слушать все интерфейсы");

    sbPort = new QSpinBox();
    sbPort->setRange(1, 65535);
    sbPort->setValue(5020);

    sbSlaveId = new QSpinBox();
    sbSlaveId->setRange(1, 247);
    sbSlaveId->setValue(1); // Обычно Slave ID = 1

    cmbSimType = new QComboBox();
    cmbSimType->addItem("Линейная (Пружина)");
    cmbSimType->addItem("Стандарт ISO 6892-1 (Металл)");

    btnStart = new QPushButton("Запустить сервер");
    btnStop = new QPushButton("Остановить");
    btnStop->setEnabled(false);

    laySet->addWidget(new QLabel("IP:"));
    laySet->addWidget(leIp);
    laySet->addWidget(new QLabel("Port:"));
    laySet->addWidget(sbPort);
    laySet->addWidget(new QLabel("ID:"));
    laySet->addWidget(sbSlaveId);
    laySet->addWidget(new QLabel("Режим:"));
    laySet->addWidget(cmbSimType);
    laySet->addWidget(btnStart);
    laySet->addWidget(btnStop);

    // --- Группа Таблиц ---
    QGroupBox *grpRegs = new QGroupBox("Карта регистров");
    QHBoxLayout *layRegs = new QHBoxLayout(grpRegs);

    tableHolding = new QTableWidget(20, 2);
    tableHolding->setHorizontalHeaderLabels({"Addr", "Holding (RW)"});
    tableHolding->setColumnWidth(0, 40);
    tableHolding->setColumnWidth(1, 130);

    tableInput = new QTableWidget(10, 2);
    tableInput->setHorizontalHeaderLabels({"Addr", "Input (RO)"});
    tableInput->setColumnWidth(0, 40);
    tableInput->setColumnWidth(1, 130);

    layRegs->addWidget(tableHolding);
    layRegs->addWidget(tableInput);

    // --- Лог ---
    txtLog = new QTextEdit();
    txtLog->setReadOnly(true);

    mainLay->addWidget(grpSet);
    mainLay->addWidget(grpRegs);
    mainLay->addWidget(new QLabel("Журнал событий:"));
    mainLay->addWidget(txtLog);

    connect(btnStart, &QPushButton::clicked, this, &MainWindow::onBtnStartClicked);
    connect(btnStop, &QPushButton::clicked, this, &MainWindow::onBtnStopClicked);

    resize(950, 650);
}

void MainWindow::onBtnStartClicked()
{
    if (!modbusDevice)
        return;

    // Настраиваем карту памяти
    QModbusDataUnitMap regMap;
    regMap.insert(QModbusDataUnit::HoldingRegisters, {QModbusDataUnit::HoldingRegisters, 0, 50});
    regMap.insert(QModbusDataUnit::InputRegisters, {QModbusDataUnit::InputRegisters, 0, 50});

    modbusDevice->setMap(regMap);
    modbusDevice->setServerAddress(sbSlaveId->value());
    modbusDevice->setConnectionParameter(QModbusDevice::NetworkAddressParameter, leIp->text());
    modbusDevice->setConnectionParameter(QModbusDevice::NetworkPortParameter, sbPort->value());

    if (modbusDevice->connectDevice()) {
        btnStart->setEnabled(false);
        btnStop->setEnabled(true);

        // Блокируем настройки UI
        sbSlaveId->setEnabled(false);
        leIp->setEnabled(false);
        sbPort->setEnabled(false);
        cmbSimType->setEnabled(false); // Лучше заблокировать смену режима на лету

        // --- 1. СБРОС ВНУТРЕННИХ ПЕРЕМЕННЫХ ---
        currentPos = 0.0f;
        currentLoad = 0.0f;
        testTime = 0.0f;
        maxLoad = 0.0f;
        startPos = 0.0f;
        isTestRunning = false;

        // --- 2. СБРОС МАТЕМАТИЧЕСКОЙ МОДЕЛИ ---
        isoEmulator.reset(0.0f);

        // --- 3. ЗАПИСЬ НУЛЕЙ В MODBUS (Input Registers) ---
        // Чтобы клиент сразу увидел чистое состояние
        setInputFloat(I_POS, 0.0f);
        setInputFloat(I_LOAD, 0.0f);
        setInputFloat(I_TIME, 0.0f);
        setInputFloat(I_ELONGATION, 0.0f);
        setInputFloat(I_MAX_LOAD, 0.0f);

        // --- 4. УСТАНОВКА НАСТРОЕК ПО УМОЛЧАНИЮ (Holding Registers) ---
        setHoldingInt(H_TEST_SPEED, 50);    // 50 мм/мин
        setHoldingInt(H_MANUAL_SPEED, 200); // 200 мм/мин
        setHoldingInt(H_CMD, 0);            // Сброс командного слова

        // Запуск таймера физики
        simTimer->start(100);

        onLogMessage("СЕРВЕР ЗАПУЩЕН. Память очищена.");
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
    sbSlaveId->setEnabled(true);
    leIp->setEnabled(true);
    sbPort->setEnabled(true);

    onLogMessage("СЕРВЕР ОСТАНОВЛЕН.");
}

void MainWindow::onLogMessage(const QString &msg)
{
    QString ts = QDateTime::currentDateTime().toString("HH:mm:ss.zzz");
    txtLog->append(QString("[%1] %2").arg(ts, msg));
}

// --- МГНОВЕННАЯ ОБРАБОТКА КОМАНД ---
void MainWindow::handleDataWritten(QModbusDataUnit::RegisterType type, int address, int size)
{
    // Нас интересуют только Holding Register 0 (Команды)
    if (type != QModbusDataUnit::HoldingRegisters)
        return;

    // Проверяем, попал ли адрес 0 в диапазон записи
    bool cmdRegisterChanged = (address <= H_CMD) && (address + size > H_CMD);

    if (cmdRegisterChanged) {
        quint16 cmd = getHoldingInt(H_CMD);

        // Разбираем биты
        bool bStart = getBit(cmd, 0);
        bool bStop = getBit(cmd, 1);
        if (getBit(cmd, 5))
            bStop = true; // Доп. стоп

        // 1. ЛОГИКА СТОП
        if (bStop) {
            if (isTestRunning) {
                isTestRunning = false;
                onLogMessage("CMD: Получен СТОП.");
            }
        }

        // 2. ЛОГИКА СТАРТ
        // Если пришла команда СТАРТ и НЕТ команды СТОП
        if (bStart && !bStop) {
            // Запускаем, если тест не шел, или перезапускаем
            if (!isTestRunning) {
                isTestRunning = true;

                // --- ИНИЦИАЛИЗАЦИЯ ПАРАМЕТРОВ ---
                testTime = 0.0f;
                maxLoad = 0.0f;
                startPos = currentPos; // Запоминаем текущую позицию как ноль
                currentLoad = 0.0f;

                // Сброс математической модели
                if (cmbSimType->currentIndex() == 1) {
                    isoEmulator.reset(startPos);
                    onLogMessage("CMD: СТАРТ ISO 6892-1 (Металл)");
                } else {
                    onLogMessage("CMD: СТАРТ Линейный тест");
                }

                // --- ПРИНУДИТЕЛЬНОЕ ОБНУЛЕНИЕ РЕГИСТРОВ ---
                // Чтобы клиент мгновенно увидел сброс графиков
                setInputFloat(I_TIME, 0.0f);
                setInputFloat(I_ELONGATION, 0.0f);
                setInputFloat(I_MAX_LOAD, 0.0f);
                setInputFloat(I_LOAD, 0.0f);
            }
        }
    }
}

// --- ФИЗИКА СИМУЛЯЦИИ (по таймеру) ---
void MainWindow::updateSimulation()
{
    if (modbusDevice->state() != QModbusDevice::ConnectedState)
        return;

    // Читаем текущие кнопки для ручного режима (удержание)
    quint16 cmd = getHoldingInt(H_CMD);
    bool bUp = getBit(cmd, 2);
    bool bDown = getBit(cmd, 3);
    bool bStop = getBit(cmd, 1) || getBit(cmd, 5);

    if (bStop)
        isTestRunning = false;

    float dt = 0.1f; // 100ms

    if (isTestRunning) {
        // === РЕЖИМ ИСПЫТАНИЯ ===
        testTime += dt;

        float speed = (float) getHoldingInt(H_TEST_SPEED);
        if (speed < 0.1f)
            speed = 50.0f;

        if (cmbSimType->currentIndex() == 0) {
            // Линейная пружина
            currentPos += (speed / 60.0f) * dt;
            float elong = currentPos - startPos;
            if (elong < 0)
                elong = 0;
            // Простое F = k*x + шум
            currentLoad = (elong * 50.0f)
                          + ((QRandomGenerator::global()->generateDouble() - 0.5) * 0.1);
        } else {
            // ISO 6892-1
            isoEmulator.update(dt, speed, true);
            currentPos = isoEmulator.getPosition();
            currentLoad = isoEmulator.getLoad();
        }

        // Пиковый детектор
        if (currentLoad > maxLoad)
            maxLoad = currentLoad;
        float elong = currentPos - startPos;

        // Запись в Modbus
        setInputFloat(I_TIME, testTime);
        setInputFloat(I_ELONGATION, elong);
        setInputFloat(I_MAX_LOAD, maxLoad);

    } else {
        // === РУЧНОЙ РЕЖИМ ===
        float mSpeed = (float) getHoldingInt(H_MANUAL_SPEED);
        if (mSpeed < 0.1f)
            mSpeed = 50.0f;

        if (bUp && !bStop) {
            currentPos += (mSpeed / 60.0f) * dt;
            // Синхронизация ISO модели при ручном движении
            if (cmbSimType->currentIndex() == 1)
                isoEmulator.reset(currentPos);
        } else if (bDown && !bStop) {
            currentPos -= (mSpeed / 60.0f) * dt;
            if (currentPos < 0)
                currentPos = 0;
            if (cmbSimType->currentIndex() == 1)
                isoEmulator.reset(currentPos);
        }

        // Шум нуля
        currentLoad = (QRandomGenerator::global()->generateDouble() - 0.5) * 0.05;
    }

    // Обновляем главные регистры (всегда)
    setInputFloat(I_POS, currentPos);
    setInputFloat(I_LOAD, currentLoad);

    // Логирование фаз (опционально)
    if (isTestRunning && cmbSimType->currentIndex() == 1) {
        static QString lastPh;
        QString ph = isoEmulator.getCurrentPhaseName();
        if (!ph.isEmpty() && ph != lastPh) {
            // onLogMessage("Фаза: " + ph); // Можно раскомментировать
            lastPh = ph;
        }
    }

    updateTables();
}

void MainWindow::updateTables()
{
    // Обновление таблицы Holding
    for (int i = 0; i < 20; i++) {
        quint16 val;
        modbusDevice->data(QModbusDataUnit::HoldingRegisters, i, &val);
        if (!tableHolding->item(i, 0)) {
            tableHolding->setItem(i, 0, new QTableWidgetItem(QString::number(i)));
            tableHolding->setItem(i, 1, new QTableWidgetItem());
        }
        tableHolding->item(i, 1)->setText(QString::number(val));
    }

    // Обновление таблицы Input
    for (int i = 0; i < 10; i++) {
        quint16 val;
        modbusDevice->data(QModbusDataUnit::InputRegisters, i, &val);
        if (!tableInput->item(i, 0)) {
            tableInput->setItem(i, 0, new QTableWidgetItem(QString::number(i)));
            tableInput->setItem(i, 1, new QTableWidgetItem());
        }
        QString txt = QString::number(val);
        // Подсказка для float значений (парсим 2 регистра)
        if ((i == 0 || i == 2 || i == 4 || i == 6 || i == 8) && i + 1 < 10) {
            quint16 nextVal;
            modbusDevice->data(QModbusDataUnit::InputRegisters, i + 1, &nextVal);
            QByteArray b;
            b.resize(4);
            // Big Endian
            b[0] = (val >> 8) & 0xFF;
            b[1] = val & 0xFF;
            b[2] = (nextVal >> 8) & 0xFF;
            b[3] = nextVal & 0xFF;
            float f;
            QDataStream s(b);
            s.setFloatingPointPrecision(QDataStream::SinglePrecision);
            s >> f;
            txt += QString(" (f: %1)").arg(f, 0, 'f', 2);
        }
        tableInput->item(i, 1)->setText(txt);
    }
}

// --- HELPERS ---

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

bool MainWindow::getBit(quint16 val, int bit)
{
    return (val >> bit) & 1;
}

void MainWindow::setInputFloat(int addr, float val)
{
    QByteArray buf;
    QDataStream s(&buf, QIODevice::WriteOnly);
    s.setFloatingPointPrecision(QDataStream::SinglePrecision);
    s << val;
    // Big Endian для Modbus
    quint16 h = (static_cast<quint8>(buf[0]) << 8) | static_cast<quint8>(buf[1]);
    quint16 l = (static_cast<quint8>(buf[2]) << 8) | static_cast<quint8>(buf[3]);
    modbusDevice->setData(QModbusDataUnit::InputRegisters, addr, h);
    modbusDevice->setData(QModbusDataUnit::InputRegisters, addr + 1, l);
}
