#include "Iso6892Form.h"
#include <QLCDNumber>
#include <QMessageBox>
#include "Iso6892Analyzer.h"
#include "MachineControl.h"
#include "TensilePlotWidget.h"
#include "ui_Iso6892Form.h"
#include <cmath>

Iso6892Form::Iso6892Form(MachineControl *machine, QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::Iso6892Form)
    , m_machine(machine)
    , m_isTestRunning(false)
    , m_lastRawForceKg(0)
    , m_lastRawExtMm(0)
    , m_lastTestTimeS(0)
    , m_forceOffsetKg(0)
    , m_extOffsetMm(0)
    , m_currentS0(1.0)
{
    ui->setupUi(this);

    // Инициализация логики
    m_plot = new TensilePlotWidget(this);
    m_analyzer = new Iso6892Analyzer(this);
    m_guiTimer = new QTimer(this);

    replaceWidgetInGroupBox(ui->gbPlot, m_plot);

    // Подключения MachineControl
    connect(m_machine, &MachineControl::connected, this, &Iso6892Form::onMachineConnected);
    connect(m_machine, &MachineControl::disconnected, this, &Iso6892Form::onMachineDisconnected);
    connect(m_machine,
            &MachineControl::currentLoadChanged,
            this,
            &Iso6892Form::onCurrentLoadChanged);
    connect(m_machine, &MachineControl::lengthChanged, this, &Iso6892Form::onLengthChanged);
    connect(m_machine, &MachineControl::testTimeChanged, this, &Iso6892Form::onTestTimeChanhed);

    // Таймер GUI
    connect(m_guiTimer, &QTimer::timeout, this, &Iso6892Form::onGuiTimerTick);

    // Начальное состояние кнопок
    ui->btnStart->setEnabled(false);
    ui->btnStop->setEnabled(false);
    ui->btnReturn->setEnabled(false);
    ui->btnZero->setEnabled(false);
}

Iso6892Form::~Iso6892Form()
{
    if (m_machine->isConnected()) {
        m_machine->stopPolling();
        m_machine->disconnectDevice();
    }
    delete ui;
}

// --- СЛОТЫ МАШИНЫ ---

void Iso6892Form::onMachineConnected()
{
    ui->btnStart->setEnabled(true);
    ui->btnReturn->setEnabled(true);
    ui->btnZero->setEnabled(true);
    m_machine->startPolling(50);
    m_guiTimer->start(50);
}

void Iso6892Form::onMachineDisconnected()
{
    ui->btnStart->setEnabled(false);
    ui->btnStop->setEnabled(false);
    ui->btnReturn->setEnabled(false);
    ui->btnZero->setEnabled(false);
    m_guiTimer->stop();
}

void Iso6892Form::onCurrentLoadChanged(float kgVal)
{
    m_lastRawForceKg = kgVal;
}
void Iso6892Form::onLengthChanged(float mmVal)
{
    m_lastRawExtMm = mmVal;
}

void Iso6892Form::onTestTimeChanhed(float sVal)
{
    m_lastTestTimeS = sVal;
}

// --- УПРАВЛЕНИЕ ---

void Iso6892Form::on_btnZero_clicked()
{
    m_forceOffsetKg = m_lastRawForceKg;
    m_extOffsetMm = m_lastRawExtMm;

    // Сброс виджета графика
    m_plot->resetPlot();

    ui->lcdLoad->display(0.0);
    ui->lcdExtension->display(0.0);
    ui->lcdStress->display(0.0);
}

void Iso6892Form::on_btnStart_clicked()
{
    if (!m_machine->isConnected())
        return;

    // 1. Геометрия
    double L0 = ui->sbL0->value();
    m_currentS0 = 1.0;
    if (ui->rbRound->isChecked()) {
        double d = ui->sbDiameter->value();
        m_currentS0 = 3.14159265 * (d / 2.0) * (d / 2.0);
    } else {
        m_currentS0 = ui->sbThickness->value() * ui->sbWidth->value();
    }

    // 2. Настройка Анализатора
    m_analyzer->setSpecimenParams(m_currentS0, L0);
    m_analyzer->reset();

    // 3. Настройка Графика (передаем параметры для Live-рисования)
    m_plot->setSpecimenParams(m_currentS0, L0);
    m_plot->resetPlot();

    // 4. Очистка полей результатов
    ui->leRm->clear();
    ui->leRp02->clear();
    ui->leE->clear();
    ui->leReH->clear();
    ui->leReL->clear();
    ui->leAt->clear();
    ui->leAg->clear();

    // 5. Старт машины
    m_machine->setTestSpeed(static_cast<int>(ui->sbSpeed->value()));

    QTimer::singleShot(500, [this]() { sendCommand(MachineControl::BitStartTest); });

    m_isTestRunning = true;
    ui->gbGeometry->setEnabled(false);
    ui->btnStart->setEnabled(false);
    ui->btnReturn->setEnabled(false);
    ui->btnStop->setEnabled(true);
}

void Iso6892Form::on_btnStop_clicked()
{
    if (!m_machine->isConnected())
        return;

    sendCommand(MachineControl::BitStop);

    if (m_isTestRunning) {
        m_isTestRunning = false;

        // --- АНАЛИЗ ---
        Iso6892Results res = m_analyzer->calculateResults();
        displayResults(res);

        // --- ГРАФИК ---
        // Передаем результаты и чистую кривую в виджет для отрисовки
        m_plot->plotFinalAnalysis(res, m_analyzer->getCorrectedCurve());

        ui->gbGeometry->setEnabled(true);
        ui->btnStart->setEnabled(true);
        ui->btnReturn->setEnabled(true);
        ui->btnStop->setEnabled(false);
    }
}

void Iso6892Form::on_btnReturn_clicked()
{
    if (!m_machine->isConnected()) {
        return;
    }

    sendCommand(MachineControl::BitDownMMMode);
}

// --- GUI UPDATE LOOP ---
void Iso6892Form::onGuiTimerTick()
{
    double netForceKg = m_lastRawForceKg - m_forceOffsetKg;
    double netExtMm = m_lastRawExtMm - m_extOffsetMm;

    // Фильтр нуля
    if (std::abs(netForceKg) < 0.05)
        netForceKg = 0;

    double forceN = netForceKg * 9.80665;

    // Обновляем дисплеи
    double stress = (m_currentS0 > 0) ? (forceN / m_currentS0) : 0;
    //ui->lcdLoad->display(forceN / 1000.0); // кН
    //ui->lcdExtension->display(netExtMm);
    //ui->lcdStress->display(stress);
    //ui->lcdTime->display(m_lastTestTimeS);

    setLcdUniversal(ui->lcdLoad, forceN / 1000.0);
    setLcdUniversal(ui->lcdExtension, netExtMm);
    setLcdUniversal(ui->lcdStress, stress);
    setLcdUniversal(ui->lcdTime, m_lastTestTimeS);

    if (m_isTestRunning) {
        // Данные для математики
        m_analyzer->addDataPoint(forceN, netExtMm);

        // Данные для Live-графика (виджет сам переведет их в % и МПа)
        m_plot->addLivePoint(forceN, netExtMm);
    }
}

void Iso6892Form::displayResults(const Iso6892Results &res)
{
    if (!res.isValid) {
        QMessageBox::warning(this, "Результат", "Недостаточно данных для анализа.");
        return;
    }
    ui->leRm->setText(QString::number(res.Rm, 'f', 1));
    ui->leE->setText(QString::number(res.E_Modulus / 1000.0, 'f', 1) + " ГПа");
    ui->leAt->setText(QString::number(res.At, 'f', 1));
    ui->leAg->setText(QString::number(res.Ag, 'f', 1));

    if (res.hasYieldPoint) {
        ui->leReH->setText(QString::number(res.ReH, 'f', 1));
        ui->leReL->setText(QString::number(res.ReL, 'f', 1));
        ui->leRp02->setText("-");
    } else {
        ui->leReH->setText("-");
        ui->leReL->setText("-");
        ui->leRp02->setText(QString::number(res.Rp02, 'f', 1));
    }
}

void Iso6892Form::sendCommand(int cmd)
{
    m_machine->sendCommand((MachineControl::ControlBit) cmd, true);
    QTimer::singleShot(500, [this, cmd]() {
        if (m_machine->isConnected())
            m_machine->sendCommand((MachineControl::ControlBit) cmd, false);
    });
}

void Iso6892Form::replaceWidgetInGroupBox(QGroupBox *groupBox, QWidget *newWidget)
{
    // Убедимся, что layout существует
    QLayout *layout = groupBox->layout();
    if (!layout) {
        layout = new QVBoxLayout(groupBox);
    }

    // Очистка
    QLayoutItem *item;
    while ((item = layout->takeAt(0)) != nullptr) {
        delete item->widget();
        delete item;
    }

    // Добавление
    layout->addWidget(newWidget);
}

void Iso6892Form::setLcdUniversal(QLCDNumber *lcd, double val, int width, int prec)
{
    // 1. Устанавливаем количество сегментов в самом LCD
    lcd->setDigitCount(width);

    // 2. Форматируем модуль числа (без знака) с фиксированной точкой
    QString text = QString::number(qAbs(val), 'f', prec);

    // 3. Вычисляем, сколько места нужно заполнить нулями
    // Если число отрицательное, резервируем 1 знак под минус
    int paddingWidth = (val < 0) ? (width - 1) : width;

    // 4. Дополняем нулями слева
    text = text.rightJustified(paddingWidth, '0');

    // 5. Если было отрицательное число, добавляем минус в начало
    if (val < 0) {
        text.prepend("-");
    }

    // 6. Обрезаем, если вдруг число больше чем влезает (защита от переполнения)
    // Например, если width=5, а число 12345.6, берем последние 5 символов или оставляем как есть
    if (text.length() > width) {
        // Можно решить: обрезать или оставить (LCD сам может показать ошибку)
        // text = text.right(width);
    }

    lcd->display(text);
}

void Iso6892Form::on_rbRound_toggled(bool checked)
{
    ui->sbDiameter->setEnabled(checked);
    ui->sbThickness->setEnabled(!checked);
    ui->sbWidth->setEnabled(!checked);
}

void Iso6892Form::on_btnCalcManual_clicked()
{
    double du = ui->sbDu->value();
    double lu = ui->sbLu->value();

    double Z = m_analyzer->calculateZ(du);
    double A = m_analyzer->calculateManualA(lu);

    ui->label_ResZ->setText(QString("Z: %1 %").arg(Z, 0, 'f', 1));
    ui->label_ResA->setText(QString("A: %1 %").arg(A, 0, 'f', 1));
}
