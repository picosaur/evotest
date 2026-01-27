#include "MainWindow.h"
#include <QDebug>
#include "ui_MainWindow.h"

const int SERVER_ID = 1;

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , modbusDevice(new QModbusTcpClient(this))
{
    ui->setupUi(this);
    setWindowTitle("CommandApp");

    // ===============================================
    // НАСТРОЙКА ВАЛИДАТОРОВ (ФИЛЬТРЫ ВВОДА)
    // ===============================================

    // 1. Валидатор для INT16 (от -32768 до 32767).
    // Если параметры всегда положительные (UINT), можно поставить (0, 65535)
    QIntValidator *intVal = new QIntValidator(-32768, 32767, this);

    // Для интервалов времени (только положительные числа)
    QIntValidator *timeVal = new QIntValidator(1, 999999, this);

    // 2. Валидатор для FLOAT (стандартная запись, 4 знака после запятой)
    QDoubleValidator *floatVal = new QDoubleValidator(-999999.0, 999999.0, 4, this);
    // Важно: чтобы принимал "точку" как разделитель независимо от локали
    floatVal->setLocale(QLocale::C);

    // Применяем INT валидатор к полям INT16 протокола
    ui->speedMmMinIn->setValidator(intVal);
    ui->encoderPulsesIn->setValidator(intVal);
    ui->motorScrewRatioIn->setValidator(intVal);
    ui->screwPitchIn->setValidator(intVal);
    ui->sensorSensitivityIn->setValidator(intVal);
    ui->testSpeedIn->setValidator(intVal);
    ui->manualSpeedIn->setValidator(intVal);

    // Применяем FLOAT валидатор к полям Float протокола
    ui->moveByMmIn->setValidator(floatVal);
    ui->moveToMmIn->setValidator(floatVal);
    ui->sensorRangeKnIn->setValidator(floatVal);
    ui->controlErrorMmIn->setValidator(floatVal);

    // Применяем валидатор для настроек времени авто-чтения
    ui->autoReadCommandsIntervalIn->setValidator(timeVal);
    ui->autoReadParamsIntervalIn->setValidator(timeVal);
    ui->autoReadIndicatorsIntervalIn->setValidator(timeVal);

    // ===============================================
    // ТАЙМЕРЫ И СИГНАЛЫ (Остальной код без изменений)
    // ===============================================
    timerCommands = new QTimer(this);
    timerParams = new QTimer(this);
    timerIndicators = new QTimer(this);

    connect(timerCommands, &QTimer::timeout, this, &MainWindow::processAutoReadCommands);
    connect(timerParams, &QTimer::timeout, this, &MainWindow::processAutoReadParams);
    connect(timerIndicators, &QTimer::timeout, this, &MainWindow::processAutoReadIndicators);

    connect(modbusDevice, &QModbusClient::stateChanged, this, &MainWindow::onStateChanged);
    connect(modbusDevice, &QModbusClient::errorOccurred, [this](QModbusDevice::Error) {
        ui->statusbar->showMessage("Modbus Error: " + modbusDevice->errorString());
    });

    // чекбоксы вывода недоступны для пользователя
    ui->startTestingOut->setAttribute(Qt::WA_TransparentForMouseEvents);
    ui->startTestingOut->setFocusPolicy(Qt::NoFocus);

    ui->stopOut->setAttribute(Qt::WA_TransparentForMouseEvents);
    ui->stopOut->setFocusPolicy(Qt::NoFocus);

    ui->moveTraverseUpOut->setAttribute(Qt::WA_TransparentForMouseEvents);
    ui->moveTraverseUpOut->setFocusPolicy(Qt::NoFocus);

    ui->moveTraverseDownOut->setAttribute(Qt::WA_TransparentForMouseEvents);
    ui->moveTraverseDownOut->setFocusPolicy(Qt::NoFocus);

    ui->startMmModeOut->setAttribute(Qt::WA_TransparentForMouseEvents);
    ui->startMmModeOut->setFocusPolicy(Qt::NoFocus);

    ui->stopMmModeOut->setAttribute(Qt::WA_TransparentForMouseEvents);
    ui->stopMmModeOut->setFocusPolicy(Qt::NoFocus);

    ui->moveUpMmModeOut->setAttribute(Qt::WA_TransparentForMouseEvents);
    ui->moveUpMmModeOut->setFocusPolicy(Qt::NoFocus);

    ui->moveDownMmModeOut->setAttribute(Qt::WA_TransparentForMouseEvents);
    ui->moveDownMmModeOut->setFocusPolicy(Qt::NoFocus);

    ui->useServoDriveOut->setAttribute(Qt::WA_TransparentForMouseEvents);
    ui->useServoDriveOut->setFocusPolicy(Qt::NoFocus);

    ui->useTopGripOut->setAttribute(Qt::WA_TransparentForMouseEvents);
    ui->useTopGripOut->setFocusPolicy(Qt::NoFocus);

    ui->compressionModeOut->setAttribute(Qt::WA_TransparentForMouseEvents);
    ui->compressionModeOut->setFocusPolicy(Qt::NoFocus);
}

MainWindow::~MainWindow()
{
    if (modbusDevice)
        modbusDevice->disconnectDevice();
    delete ui;
}

// ==========================================
// ПОДКЛЮЧЕНИЕ
// ==========================================
void MainWindow::on_connectBtn_clicked()
{
    if (!modbusDevice)
        return;

    if (modbusDevice->state() == QModbusDevice::ConnectedState) {
        modbusDevice->disconnectDevice();
    } else {
        modbusDevice->setConnectionParameter(QModbusDevice::NetworkAddressParameter,
                                             ui->hostIn->text());
        modbusDevice->setConnectionParameter(QModbusDevice::NetworkPortParameter,
                                             ui->portIn->value());
        modbusDevice->setTimeout(1000);
        modbusDevice->setNumberOfRetries(3);

        if (!modbusDevice->connectDevice()) {
            ui->statusbar->showMessage("Ошибка подключения: " + modbusDevice->errorString());
        }
    }
}

void MainWindow::onStateChanged(QModbusDevice::State state)
{
    if (state == QModbusDevice::ConnectedState) {
        ui->statusbar->showMessage("Подключено");
        ui->connectBtn->setText("Отключить");
    } else if (state == QModbusDevice::UnconnectedState) {
        ui->statusbar->showMessage("Отключено");
        ui->connectBtn->setText("Подключить");
    }
}

// ==========================================
// ОБРАБОТКА ДАННЫХ
// ==========================================
void MainWindow::onReadReady()
{
    auto reply = qobject_cast<QModbusReply *>(sender());
    if (!reply)
        return;

    if (reply->error() == QModbusDevice::NoError) {
        const QModbusDataUnit unit = reply->result();
        QString id = reply->property("ID").toString();
        QString type = reply->property("Type").toString();

        if (type == "Holding") {
            // КОМАНДЫ
            if (id == "CMD" && unit.valueCount() >= 1) {
                setCommandUIFromWord(unit.value(0));
            }
            // АВТО ОПРОС ПАРАМЕТРОВ (Registers 2-16)
            else if (id == "AllParams") {
                QVector<quint16> v = unit.values();
                if (v.size() > 0)
                    ui->speedMmMinOut->setText(QString::number((short) v[0]));
                if (v.size() > 2)
                    ui->moveByMmOut->setText(QString::number(registersToFloat(v.mid(1, 2)), 'f', 2));
                if (v.size() > 4)
                    ui->moveToMmOut->setText(QString::number(registersToFloat(v.mid(3, 2)), 'f', 2));
                if (v.size() > 5)
                    ui->encoderPulsesOut->setText(QString::number((short) v[5]));
                if (v.size() > 6)
                    ui->motorScrewRatioOut->setText(QString::number((short) v[6]));
                if (v.size() > 7)
                    ui->screwPitchOut->setText(QString::number((short) v[7]));
                if (v.size() > 9)
                    ui->sensorRangeKnOut->setText(
                        QString::number(registersToFloat(v.mid(8, 2)), 'f', 2));
                if (v.size() > 10)
                    ui->sensorSensitivityOut->setText(QString::number((short) v[10]));
                if (v.size() > 12)
                    ui->controlErrorMmOut->setText(
                        QString::number(registersToFloat(v.mid(11, 2)), 'f', 4));
                if (v.size() > 13)
                    ui->testSpeedOut->setText(QString::number((short) v[13]));
                if (v.size() > 14)
                    ui->manualSpeedOut->setText(QString::number((short) v[14]));
            }
            // ОДИНОЧНЫЕ ОТВЕТЫ
            else if (id == "speedMmMin")
                ui->speedMmMinOut->setText(QString::number((short) unit.value(0)));
            else if (id == "moveByMm")
                ui->moveByMmOut->setText(QString::number(registersToFloat(unit.values()), 'f', 2));
            else if (id == "moveToMm")
                ui->moveToMmOut->setText(QString::number(registersToFloat(unit.values()), 'f', 2));
            else if (id == "encoderPulses")
                ui->encoderPulsesOut->setText(QString::number((short) unit.value(0)));
            else if (id == "motorScrewRatio")
                ui->motorScrewRatioOut->setText(QString::number((short) unit.value(0)));
            else if (id == "screwPitch")
                ui->screwPitchOut->setText(QString::number((short) unit.value(0)));
            else if (id == "sensorRangeKn")
                ui->sensorRangeKnOut->setText(
                    QString::number(registersToFloat(unit.values()), 'f', 2));
            else if (id == "sensorSensitivity")
                ui->sensorSensitivityOut->setText(QString::number((short) unit.value(0)));
            else if (id == "controlErrorMm")
                ui->controlErrorMmOut->setText(
                    QString::number(registersToFloat(unit.values()), 'f', 4));
            else if (id == "testSpeed")
                ui->testSpeedOut->setText(QString::number((short) unit.value(0)));
            else if (id == "manualSpeed")
                ui->manualSpeedOut->setText(QString::number((short) unit.value(0)));

        } else if (type == "Input") {
            // АВТО ОПРОС ИНДИКАТОРОВ (Registers 0-9)
            if (id == "AllIndicators") {
                QVector<quint16> v = unit.values();
                if (v.size() >= 10) {
                    ui->currentPositionOut->setText(
                        QString::number(registersToFloat(v.mid(0, 2)), 'f', 2));
                    ui->currentLoadOut->setText(
                        QString::number(registersToFloat(v.mid(2, 2)), 'f', 2));
                    ui->testTimeOut->setText(QString::number(registersToFloat(v.mid(4, 2)), 'f', 1));
                    ui->testLengthOut->setText(
                        QString::number(registersToFloat(v.mid(6, 2)), 'f', 2));
                    ui->maxLoadOut->setText(QString::number(registersToFloat(v.mid(8, 2)), 'f', 2));
                }
            }
            // ОДИНОЧНЫЕ
            else if (id == "currentPosition")
                ui->currentPositionOut->setText(
                    QString::number(registersToFloat(unit.values()), 'f', 2));
            else if (id == "currentLoad")
                ui->currentLoadOut->setText(
                    QString::number(registersToFloat(unit.values()), 'f', 2));
            else if (id == "testTime")
                ui->testTimeOut->setText(QString::number(registersToFloat(unit.values()), 'f', 1));
            else if (id == "testLength")
                ui->testLengthOut->setText(QString::number(registersToFloat(unit.values()), 'f', 2));
            else if (id == "maxLoad")
                ui->maxLoadOut->setText(QString::number(registersToFloat(unit.values()), 'f', 2));
        }
    }
    reply->deleteLater();
}

// ==========================================
// АВТОМАТИЗАЦИЯ (Блочное чтение)
// ==========================================
void MainWindow::on_autoReadCommandsCheckBox_toggled(bool checked)
{
    if (checked)
        timerCommands->start(ui->autoReadCommandsIntervalIn->text().toInt());
    else
        timerCommands->stop();
}
void MainWindow::processAutoReadCommands()
{
    readHoldingRegister(0, 1, "CMD");
}

void MainWindow::on_autoReadParamsCheckBox_toggled(bool checked)
{
    if (checked)
        timerParams->start(ui->autoReadParamsIntervalIn->text().toInt());
    else
        timerParams->stop();
}
void MainWindow::processAutoReadParams()
{
    // Читаем с адреса 2 по 16 (всего 15 регистров)
    if (modbusDevice->state() == QModbusDevice::ConnectedState) {
        if (auto *reply = modbusDevice->sendReadRequest(
                QModbusDataUnit(QModbusDataUnit::HoldingRegisters, 2, 15), ui->slaveIdIn->value())) {
            if (!reply->isFinished()) {
                connect(reply, &QModbusReply::finished, this, &MainWindow::onReadReady);
                reply->setProperty("ID", "AllParams");
                reply->setProperty("Type", "Holding");
            } else
                delete reply;
        }
    }
}

void MainWindow::on_autoReadIndicatorsCheckBox_toggled(bool checked)
{
    if (checked)
        timerIndicators->start(ui->autoReadIndicatorsIntervalIn->text().toInt());
    else
        timerIndicators->stop();
}
void MainWindow::processAutoReadIndicators()
{
    // Читаем с адреса 0 по 9 (всего 10 регистров = 5 float)
    if (modbusDevice->state() == QModbusDevice::ConnectedState) {
        if (auto *reply
            = modbusDevice->sendReadRequest(QModbusDataUnit(QModbusDataUnit::InputRegisters, 0, 10),
                                            ui->slaveIdIn->value())) {
            if (!reply->isFinished()) {
                connect(reply, &QModbusReply::finished, this, &MainWindow::onReadReady);
                reply->setProperty("ID", "AllIndicators");
                reply->setProperty("Type", "Input");
            } else
                delete reply;
        }
    }
}

// ==========================================
// ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ
// ==========================================
quint16 MainWindow::getCommandWordFromUI()
{
    quint16 word = 0;
    if (ui->startTestingIn->isChecked())
        word |= (1 << 0);
    if (ui->stopIn->isChecked())
        word |= (1 << 1);
    if (ui->moveTraverseUpIn->isChecked())
        word |= (1 << 2);
    if (ui->moveTraverseDownIn->isChecked())
        word |= (1 << 3);
    if (ui->startMmModeIn->isChecked())
        word |= (1 << 4);
    if (ui->stopMmModeIn->isChecked())
        word |= (1 << 5);
    if (ui->moveUpMmModeIn->isChecked())
        word |= (1 << 6);
    if (ui->moveDownMmModeIn->isChecked())
        word |= (1 << 7);
    if (ui->useServoDriveIn->isChecked())
        word |= (1 << 8);
    if (ui->useTopGripIn->isChecked())
        word |= (1 << 9);
    if (ui->compressionModeIn->isChecked())
        word |= (1 << 10);
    return word;
}

void MainWindow::setCommandUIFromWord(quint16 word)
{
    ui->startTestingOut->setChecked(word & (1 << 0));
    ui->stopOut->setChecked(word & (1 << 1));
    ui->moveTraverseUpOut->setChecked(word & (1 << 2));
    ui->moveTraverseDownOut->setChecked(word & (1 << 3));
    ui->startMmModeOut->setChecked(word & (1 << 4));
    ui->stopMmModeOut->setChecked(word & (1 << 5));
    ui->moveUpMmModeOut->setChecked(word & (1 << 6));
    ui->moveDownMmModeOut->setChecked(word & (1 << 7));
    ui->useServoDriveOut->setChecked(word & (1 << 8));
    ui->useTopGripOut->setChecked(word & (1 << 9));
    ui->compressionModeOut->setChecked(word & (1 << 10));
}

QVector<quint16> MainWindow::floatToRegisters(float value)
{
    QVector<quint16> regs;
    quint32 data;
    memcpy(&data, &value, sizeof(float));
    regs.append(data >> 16);
    regs.append(data & 0xFFFF);
    return regs;
}

float MainWindow::registersToFloat(const QVector<quint16> &values)
{
    if (values.size() < 2)
        return 0.0f;
    quint32 data = (values[0] << 16) | values[1];
    float value;
    memcpy(&value, &data, sizeof(float));
    return value;
}

void MainWindow::writeSingleRegister(int address, quint16 value)
{
    if (modbusDevice->state() != QModbusDevice::ConnectedState)
        return;
    QModbusDataUnit writeUnit(QModbusDataUnit::HoldingRegisters, address, 1);
    writeUnit.setValue(0, value);
    if (auto *reply = modbusDevice->sendWriteRequest(writeUnit, ui->slaveIdIn->value())) {
        connect(reply, &QModbusReply::finished, reply, &QModbusReply::deleteLater);
    }
}

void MainWindow::writeFloatRegister(int address, float value)
{
    if (modbusDevice->state() != QModbusDevice::ConnectedState)
        return;
    QModbusDataUnit writeUnit(QModbusDataUnit::HoldingRegisters, address, floatToRegisters(value));
    if (auto *reply = modbusDevice->sendWriteRequest(writeUnit, ui->slaveIdIn->value())) {
        connect(reply, &QModbusReply::finished, reply, &QModbusReply::deleteLater);
    }
}

void MainWindow::readHoldingRegister(int address, int count, const QString &id)
{
    if (modbusDevice->state() != QModbusDevice::ConnectedState)
        return;
    if (auto *reply = modbusDevice->sendReadRequest(QModbusDataUnit(QModbusDataUnit::HoldingRegisters,
                                                                    address,
                                                                    count),
                                                    ui->slaveIdIn->value())) {
        if (!reply->isFinished()) {
            connect(reply, &QModbusReply::finished, this, &MainWindow::onReadReady);
            reply->setProperty("ID", id);
            reply->setProperty("Type", "Holding");
        } else
            delete reply;
    }
}

void MainWindow::readInputRegister(int address, int count, const QString &id)
{
    if (modbusDevice->state() != QModbusDevice::ConnectedState)
        return;
    if (auto *reply = modbusDevice->sendReadRequest(QModbusDataUnit(QModbusDataUnit::InputRegisters,
                                                                    address,
                                                                    count),
                                                    ui->slaveIdIn->value())) {
        if (!reply->isFinished()) {
            connect(reply, &QModbusReply::finished, this, &MainWindow::onReadReady);
            reply->setProperty("ID", id);
            reply->setProperty("Type", "Input");
        } else
            delete reply;
    }
}

// ==========================================
// КНОПКИ (Одиночные операции)
// ==========================================
void MainWindow::on_writeAllCommandsBtn_clicked()
{
    writeSingleRegister(0, getCommandWordFromUI());
}
void MainWindow::on_readAllCommandsBtn_clicked()
{
    readHoldingRegister(0, 1, "CMD");
}

void MainWindow::on_speedMmMinWriteBtn_clicked()
{
    writeSingleRegister(2, ui->speedMmMinIn->text().toUShort());
}
void MainWindow::on_speedMmMinReadBtn_clicked()
{
    readHoldingRegister(2, 1, "speedMmMin");
}

void MainWindow::on_moveByMmWriteBtn_clicked()
{
    writeFloatRegister(3, ui->moveByMmIn->text().toFloat());
}
void MainWindow::on_moveByMmReadBtn_clicked()
{
    readHoldingRegister(3, 2, "moveByMm");
}

void MainWindow::on_moveToMmWriteBtn_clicked()
{
    writeFloatRegister(5, ui->moveToMmIn->text().toFloat());
}
void MainWindow::on_moveToMmReadBtn_clicked()
{
    readHoldingRegister(5, 2, "moveToMm");
}

void MainWindow::on_encoderPulsesWriteBtn_clicked()
{
    writeSingleRegister(7, ui->encoderPulsesIn->text().toUShort());
}
void MainWindow::on_encoderPulsesReadBtn_clicked()
{
    readHoldingRegister(7, 1, "encoderPulses");
}

void MainWindow::on_motorScrewRatioWriteBtn_clicked()
{
    writeSingleRegister(8, ui->motorScrewRatioIn->text().toUShort());
}
void MainWindow::on_motorScrewRatioReadBtn_clicked()
{
    readHoldingRegister(8, 1, "motorScrewRatio");
}

void MainWindow::on_screwPitchWriteBtn_clicked()
{
    writeSingleRegister(9, ui->screwPitchIn->text().toUShort());
}
void MainWindow::on_screwPitchReadBtn_clicked()
{
    readHoldingRegister(9, 1, "screwPitch");
}

void MainWindow::on_sensorRangeKnWriteBtn_clicked()
{
    writeFloatRegister(10, ui->sensorRangeKnIn->text().toFloat());
}
void MainWindow::on_sensorRangeKnReadBtn_clicked()
{
    readHoldingRegister(10, 2, "sensorRangeKn");
}

void MainWindow::on_sensorSensitivityWriteBtn_clicked()
{
    writeSingleRegister(12, ui->sensorSensitivityIn->text().toUShort());
}
void MainWindow::on_sensorSensitivityReadBtn_clicked()
{
    readHoldingRegister(12, 1, "sensorSensitivity");
}

void MainWindow::on_controlErrorMmWriteBtn_clicked()
{
    writeFloatRegister(13, ui->controlErrorMmIn->text().toFloat());
}
void MainWindow::on_controlErrorMmReadBtn_clicked()
{
    readHoldingRegister(13, 2, "controlErrorMm");
}

void MainWindow::on_testSpeedWriteBtn_clicked()
{
    writeSingleRegister(15, ui->testSpeedIn->text().toUShort());
}
void MainWindow::on_testSpeedReadBtn_clicked()
{
    readHoldingRegister(15, 1, "testSpeed");
}

void MainWindow::on_manualSpeedWriteBtn_clicked()
{
    writeSingleRegister(16, ui->manualSpeedIn->text().toUShort());
}
void MainWindow::on_manualSpeedReadBtn_clicked()
{
    readHoldingRegister(16, 1, "manualSpeed");
}

void MainWindow::on_currentPositionReadBtn_clicked()
{
    readInputRegister(0, 2, "currentPosition");
}
void MainWindow::on_currentLoadReadBtn_clicked()
{
    readInputRegister(2, 2, "currentLoad");
}
void MainWindow::on_testTimeReadBtn_clicked()
{
    readInputRegister(4, 2, "testTime");
}
void MainWindow::on_testLengthReadBtn_clicked()
{
    readInputRegister(6, 2, "testLength");
}
void MainWindow::on_maxLoadReadBtn_clicked()
{
    readInputRegister(8, 2, "maxLoad");
}
