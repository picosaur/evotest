#include "MainWindow.h"
#include <QComboBox>
#include <QDataStream>
#include <QDebug>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QModbusReply>
#include <QPushButton>
#include <QSpinBox>
#include <QStatusBar>
#include <QTableWidget>
#include <QVBoxLayout>
#include <algorithm>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , modbusDevice(nullptr)
{
    setupUi();

    // Инициализация Modbus Client
    modbusDevice = new QModbusTcpClient(this);

    // Подключение сигналов статуса
    connect(modbusDevice, &QModbusClient::stateChanged, this, &MainWindow::onStateChanged);
    connect(modbusDevice, &QModbusClient::errorOccurred, [this](QModbusDevice::Error) {
        statusBar()->showMessage("Ошибка: " + modbusDevice->errorString(), 5000);
    });
}

MainWindow::~MainWindow()
{
    if (modbusDevice)
        modbusDevice->disconnectDevice();
}

void MainWindow::setupUi()
{
    QWidget *centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);
    QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);

    // --- 1. Группа Подключения ---
    QGroupBox *grpConn = new QGroupBox("Подключение");
    QHBoxLayout *connLayout = new QHBoxLayout(grpConn);

    lineIpAddress = new QLineEdit("127.0.0.1");
    spinPort = new QSpinBox();
    spinPort->setRange(1, 65535);
    spinPort->setValue(502);
    btnConnect = new QPushButton("Подключить");

    connLayout->addWidget(new QLabel("IP:"));
    connLayout->addWidget(lineIpAddress);
    connLayout->addWidget(new QLabel("Port:"));
    connLayout->addWidget(spinPort);
    connLayout->addWidget(btnConnect);

    mainLayout->addWidget(grpConn);

    // --- 2. Группа Управления (Сетка) ---
    QGroupBox *grpReq = new QGroupBox("Операции");
    QGridLayout *reqLayout = new QGridLayout(grpReq);

    // Создание виджетов
    comboRegType = new QComboBox();
    comboRegType->addItem("Coils (0)", QModbusDataUnit::Coils);
    comboRegType->addItem("Discrete Inputs (1)", QModbusDataUnit::DiscreteInputs);
    comboRegType->addItem("Input Registers (3)", QModbusDataUnit::InputRegisters);
    comboRegType->addItem("Holding Registers (4)", QModbusDataUnit::HoldingRegisters);
    comboRegType->setCurrentIndex(3); // Holding Regs

    spinStartAddr = new QSpinBox();
    spinStartAddr->setRange(0, 65535);
    spinStartAddr->setPrefix("Addr: ");

    spinCount = new QSpinBox();
    spinCount->setRange(1, 125);
    spinCount->setValue(10);
    spinCount->setPrefix("Qty: ");

    btnRead = new QPushButton("Прочитать");
    btnRead->setEnabled(false);

    // Виджеты записи
    comboWriteType = new QComboBox();
    comboWriteType->addItems({"Int16", "UInt16", "Int32", "UInt32", "Float", "Double"});

    lineWriteValue = new QLineEdit();
    lineWriteValue->setPlaceholderText("Значение для записи");

    btnWrite = new QPushButton("Записать");
    btnWrite->setEnabled(false);

    // --- РАССТАНОВКА ПО СЕТКЕ ---

    // Ряд 0: Тип Регистра | Адрес
    reqLayout->addWidget(comboRegType, 0, 0);
    reqLayout->addWidget(spinStartAddr, 0, 1);

    // Ряд 1: Количество | Кнопка Чтения
    reqLayout->addWidget(spinCount, 1, 0);
    reqLayout->addWidget(btnRead, 1, 1);

    // Ряд 2: Запись (Тип + Значение) | Кнопка Записи
    // Чтобы в левую ячейку влезло два элемента, кладем их в sub-layout
    QHBoxLayout *writeSubLayout = new QHBoxLayout();
    writeSubLayout->setContentsMargins(0, 0, 0, 0);
    writeSubLayout->addWidget(comboWriteType);
    writeSubLayout->addWidget(lineWriteValue);

    reqLayout->addLayout(writeSubLayout, 2, 0); // Вставляем sub-layout в сетку
    reqLayout->addWidget(btnWrite, 2, 1);

    // Растягиваем колонки
    reqLayout->setColumnStretch(0, 2); // Левая часть шире
    reqLayout->setColumnStretch(1, 1); // Правая (кнопки) уже

    mainLayout->addWidget(grpReq);

    // --- 3. Таблица ---
    tableResult = new QTableWidget();
    tableResult->setColumnCount(2);
    tableResult->setHorizontalHeaderLabels({"Регистр", "Raw (UInt16)"});
    tableResult->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    tableResult->setSelectionBehavior(QAbstractItemView::SelectRows);
    tableResult->setSelectionMode(QAbstractItemView::SingleSelection);

    mainLayout->addWidget(tableResult);

    // --- 4. Декодер (Внизу) ---
    QGroupBox *grpDecoder = new QGroupBox("Декодер / Настройки байт");
    QVBoxLayout *decoderLayout = new QVBoxLayout(grpDecoder);

    QHBoxLayout *decSettingsLayout = new QHBoxLayout();

    comboDataType = new QComboBox();
    comboDataType->addItems({"Int16", "UInt16", "Int32", "UInt32", "Float", "Double", "Boolean"});

    comboEndian = new QComboBox();
    comboEndian->addItem("ABCD (Big Endian - Std)", 0);
    comboEndian->addItem("CDAB (Word Swap)", 1);
    comboEndian->addItem("BADC (Byte Swap)", 2);
    comboEndian->addItem("DCBA (Little Endian)", 3);

    decSettingsLayout->addWidget(new QLabel("Тип:"));
    decSettingsLayout->addWidget(comboDataType);
    decSettingsLayout->addWidget(new QLabel("Порядок байт:"));
    decSettingsLayout->addWidget(comboEndian);
    decSettingsLayout->addStretch();

    QHBoxLayout *decResultLayout = new QHBoxLayout();
    lineDecodedResult = new QLineEdit();
    lineDecodedResult->setReadOnly(true);
    lineDecodedResult->setPlaceholderText("Результат декодирования выбранной строки...");

    decResultLayout->addWidget(new QLabel("Результат:"));
    decResultLayout->addWidget(lineDecodedResult);

    decoderLayout->addLayout(decSettingsLayout);
    decoderLayout->addLayout(decResultLayout);

    mainLayout->addWidget(grpDecoder);

    // --- Connections ---
    connect(btnConnect, &QPushButton::clicked, this, &MainWindow::onConnectClicked);
    connect(btnRead, &QPushButton::clicked, this, &MainWindow::onReadClicked);
    connect(btnWrite, &QPushButton::clicked, this, &MainWindow::onWriteClicked);

    // Обновление декодера при любом чихе
    connect(tableResult, &QTableWidget::itemSelectionChanged, this, &MainWindow::updateDecodedValue);
    connect(comboDataType,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this,
            &MainWindow::updateDecodedValue);
    connect(comboEndian,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this,
            &MainWindow::updateDecodedValue);

    setWindowTitle("Modbus Client");
    resize(550, 700);
}

// --- ПОДКЛЮЧЕНИЕ ---
void MainWindow::onConnectClicked()
{
    if (!modbusDevice)
        return;

    if (modbusDevice->state() != QModbusDevice::ConnectedState) {
        modbusDevice->setConnectionParameter(QModbusDevice::NetworkPortParameter, spinPort->value());
        modbusDevice->setConnectionParameter(QModbusDevice::NetworkAddressParameter,
                                             lineIpAddress->text());
        modbusDevice->setTimeout(1000);
        modbusDevice->setNumberOfRetries(3);

        if (!modbusDevice->connectDevice()) {
            statusBar()->showMessage("Ошибка подключения: " + modbusDevice->errorString(), 3000);
        }
    } else {
        modbusDevice->disconnectDevice();
    }
}

void MainWindow::onStateChanged(QModbusDevice::State state)
{
    if (state == QModbusDevice::ConnectedState) {
        btnConnect->setText("Отключить");
        btnRead->setEnabled(true);
        btnWrite->setEnabled(true);
        statusBar()->showMessage("Подключено");
    } else if (state == QModbusDevice::UnconnectedState) {
        btnConnect->setText("Подключить");
        btnRead->setEnabled(false);
        btnWrite->setEnabled(false);
        statusBar()->showMessage("Отключено");
    }
}

// --- ЧТЕНИЕ ---
void MainWindow::onReadClicked()
{
    if (!modbusDevice)
        return;
    tableResult->setRowCount(0);
    lineDecodedResult->clear();

    auto type = static_cast<QModbusDataUnit::RegisterType>(comboRegType->currentData().toInt());
    int startAddress = spinStartAddr->value();
    int count = spinCount->value();

    QModbusDataUnit readUnit(type, startAddress, count);

    if (auto *reply = modbusDevice->sendReadRequest(readUnit, 1)) { // SlaveID 1
        if (!reply->isFinished())
            connect(reply, &QModbusReply::finished, this, &MainWindow::onReadReady);
        else
            delete reply;
    } else {
        statusBar()->showMessage("Ошибка Read: " + modbusDevice->errorString(), 3000);
    }
}

void MainWindow::onReadReady()
{
    auto reply = qobject_cast<QModbusReply *>(sender());
    if (!reply)
        return;
    reply->deleteLater();

    if (reply->error() == QModbusDevice::NoError) {
        const QModbusDataUnit unit = reply->result();
        tableResult->setRowCount(unit.valueCount());

        for (uint i = 0; i < unit.valueCount(); i++) {
            int addr = unit.startAddress() + i;
            quint16 value = unit.value(i);
            tableResult->setItem(i, 0, new QTableWidgetItem(QString::number(addr)));
            tableResult->setItem(i, 1, new QTableWidgetItem(QString::number(value)));
        }
        statusBar()->showMessage("Чтение OK");
    } else {
        statusBar()->showMessage("Ошибка Read: " + reply->errorString(), 3000);
    }
}

// --- ЗАПИСЬ (Сложная логика) ---
void MainWindow::onWriteClicked()
{
    if (!modbusDevice)
        return;

    auto regType = static_cast<QModbusDataUnit::RegisterType>(comboRegType->currentData().toInt());

    // Проверка на Coils (простая запись Bool)
    if (regType == QModbusDataUnit::Coils) {
        bool val = (lineWriteValue->text().toInt() > 0)
                   || (lineWriteValue->text().toLower() == "true");
        QModbusDataUnit writeUnit(QModbusDataUnit::Coils, spinStartAddr->value(), 1);
        writeUnit.setValue(0, val);
        if (auto *reply = modbusDevice->sendWriteRequest(writeUnit, 1)) {
            connect(reply, &QModbusReply::finished, this, &MainWindow::onWriteFinished);
        }
        return;
    }

    if (regType != QModbusDataUnit::HoldingRegisters) {
        QMessageBox::warning(this, "Ошибка", "Писать можно только в Holding Registers или Coils");
        return;
    }

    // Обработка Holding Registers (Int/Float + Endianness)
    QString typeName = comboWriteType->currentText();
    QString textVal = lineWriteValue->text();
    textVal.replace(",", ".");

    QByteArray bytes;
    QDataStream stream(&bytes, QIODevice::WriteOnly);
    // QDataStream пишет BigEndian по умолчанию

    if (typeName == "Int16")
        stream << (qint16) textVal.toShort();
    else if (typeName == "UInt16")
        stream << (quint16) textVal.toUShort();
    else if (typeName == "Int32")
        stream << (qint32) textVal.toLong();
    else if (typeName == "UInt32")
        stream << (quint32) textVal.toULong();
    else if (typeName == "Float") {
        stream.setFloatingPointPrecision(QDataStream::SinglePrecision);
        stream << (float) textVal.toFloat();
    } else if (typeName == "Double") {
        stream.setFloatingPointPrecision(QDataStream::DoublePrecision);
        stream << (double) textVal.toDouble();
    }

    // Применяем Endianness к байтам перед отправкой
    int endianMode = comboEndian->currentData().toInt(); // 0=ABCD, 1=CDAB, 2=BADC, 3=DCBA

    // Если нам нужно CDAB (Swap Words)
    if (bytes.size() >= 4 && (endianMode == 1 || endianMode == 3)) {
        // Меняем слова (2 байта) местами
        for (int i = 0; i < bytes.size(); i += 4) {
            if (i + 3 < bytes.size()) {
                char b0 = bytes[i];
                char b1 = bytes[i + 1];
                bytes[i] = bytes[i + 2];
                bytes[i + 1] = bytes[i + 3];
                bytes[i + 2] = b0;
                bytes[i + 3] = b1;
            }
        }
    }
    // Если нам нужен Byte Swap (BADC или DCBA)
    if (endianMode == 2 || endianMode == 3) {
        for (int i = 0; i < bytes.size(); i += 2) {
            std::swap(bytes[i], bytes[i + 1]);
        }
    }
    // Прим: Для Double (DCBA) полный реверс сложнее, здесь упрощенная логика Swap Words + Swap Bytes дает нужный эффект

    // Конвертация QByteArray -> QVector<quint16>
    QVector<quint16> values;
    for (int i = 0; i < bytes.size(); i += 2) {
        quint16 val = (quint16) ((uchar) bytes[i] << 8 | (uchar) bytes[i + 1]);
        values.append(val);
    }

    QModbusDataUnit writeUnit(QModbusDataUnit::HoldingRegisters,
                              spinStartAddr->value(),
                              values.count());
    writeUnit.setValues(values);

    if (auto *reply = modbusDevice->sendWriteRequest(writeUnit, 1)) {
        connect(reply, &QModbusReply::finished, this, &MainWindow::onWriteFinished);
    } else {
        statusBar()->showMessage("Ошибка Write: " + modbusDevice->errorString(), 3000);
    }
}

void MainWindow::onWriteFinished()
{
    auto reply = qobject_cast<QModbusReply *>(sender());
    if (!reply)
        return;
    reply->deleteLater();

    if (reply->error() == QModbusDevice::NoError) {
        statusBar()->showMessage("Запись успешна!");
        // Можно авто-обновить: onReadClicked();
    } else {
        statusBar()->showMessage("Ошибка записи: " + reply->errorString(), 3000);
    }
}

// --- ДЕКОДЕР (View Logic) ---
void MainWindow::updateDecodedValue()
{
    auto selectedItems = tableResult->selectedItems();
    if (selectedItems.isEmpty()) {
        lineDecodedResult->clear();
        return;
    }

    int row = tableResult->row(selectedItems.first());
    QString typeName = comboDataType->currentText();

    int regsNeeded = 1;
    if (typeName.contains("32") || typeName == "Float")
        regsNeeded = 2;
    if (typeName == "Double")
        regsNeeded = 4;

    if (row + regsNeeded > tableResult->rowCount()) {
        lineDecodedResult->setText("Мало данных (конец таблицы)");
        return;
    }

    // Читаем Raw UInt16
    QVector<quint16> rawRegs;
    for (int i = 0; i < regsNeeded; ++i) {
        rawRegs.append(tableResult->item(row + i, 1)->text().toUShort());
    }

    // Конвертируем в байты (BigEndian по умолчанию для Modbus)
    QByteArray bytes;
    for (quint16 r : rawRegs) {
        bytes.append((char) (r >> 8));
        bytes.append((char) (r & 0xFF));
    }

    // Применяем Endianness (Разворачиваем для QDataStream)
    int endianMode = comboEndian->currentData().toInt(); // 0=ABCD, 1=CDAB...

    // Word Swap (CDAB / DCBA)
    if (regsNeeded >= 2 && (endianMode == 1 || endianMode == 3)) {
        for (int i = 0; i < bytes.size(); i += 4) {
            if (i + 3 < bytes.size()) {
                char b0 = bytes[i];
                char b1 = bytes[i + 1];
                bytes[i] = bytes[i + 2];
                bytes[i + 1] = bytes[i + 3];
                bytes[i + 2] = b0;
                bytes[i + 3] = b1;
            }
        }
    }
    // Byte Swap (BADC / DCBA)
    if (endianMode == 2 || endianMode == 3) {
        for (int i = 0; i < bytes.size(); i += 2) {
            std::swap(bytes[i], bytes[i + 1]);
        }
    }

    // Читаем типизированное значение
    QDataStream stream(bytes);
    QString res;

    if (typeName == "Int16") {
        qint16 v;
        stream >> v;
        res = QString::number(v);
    } else if (typeName == "UInt16") {
        quint16 v;
        stream >> v;
        res = QString::number(v);
    } else if (typeName == "Int32") {
        qint32 v;
        stream >> v;
        res = QString::number(v);
    } else if (typeName == "UInt32") {
        quint32 v;
        stream >> v;
        res = QString::number(v);
    } else if (typeName == "Float") {
        float v;
        stream.setFloatingPointPrecision(QDataStream::SinglePrecision);
        stream >> v;
        res = QString::number(v, 'f', 4);
    } else if (typeName == "Double") {
        double v;
        stream.setFloatingPointPrecision(QDataStream::DoublePrecision);
        stream >> v;
        res = QString::number(v, 'f', 8);
    } else if (typeName == "Boolean") {
        bool b = false;
        for (auto r : rawRegs)
            if (r > 0)
                b = true;
        res = b ? "TRUE" : "FALSE";
    }

    lineDecodedResult->setText(res);
}
