#include "CommandForm.h"
#include <QTimer>
#include "MachineControl.h"
#include "ui_CommandForm.h"
#include <qdebug.h>

CommandForm::CommandForm(MachineControl *machine, QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::CommandForm)
    , m_machine(machine)
{
    ui->setupUi(this);
}

CommandForm::~CommandForm()
{
    delete ui;
}

void CommandForm::on_btnStart_pressed()
{
    startCommandTimer(MachineControl::BitStartTest);
}

void CommandForm::on_btnStart_released()
{
    stopCommandTimer(MachineControl::BitStartTest);
}

void CommandForm::on_btnStop_pressed()
{
    startCommandTimer(MachineControl::BitStop);
}

void CommandForm::on_btnStop_released()
{
    stopCommandTimer(MachineControl::BitStop);
}

void CommandForm::on_btnLift_pressed()
{
    startCommandTimer(MachineControl::BitLiftTraverse);
}

void CommandForm::on_btnLift_released()
{
    stopCommandTimer(MachineControl::BitLiftTraverse);
}

void CommandForm::on_btnLower_pressed()
{
    startCommandTimer(MachineControl::BitLowerTraverse);
}

void CommandForm::on_btnLower_released()
{
    stopCommandTimer(MachineControl::BitLowerTraverse);
}

void CommandForm::on_btnStartMm_pressed()
{
    startCommandTimer(MachineControl::BitStartMMMode);
}

void CommandForm::on_btnStartMm_released()
{
    stopCommandTimer(MachineControl::BitStartMMMode);
}

void CommandForm::on_btnStopMm_pressed()
{
    startCommandTimer(MachineControl::BitStopMMMode);
}

void CommandForm::on_btnStopMm_released()
{
    stopCommandTimer(MachineControl::BitStopMMMode);
}

void CommandForm::on_btnUpMm_pressed()
{
    startCommandTimer(MachineControl::BitUpMMMode);
}

void CommandForm::on_btnUpMm_released()
{
    stopCommandTimer(MachineControl::BitUpMMMode);
}

void CommandForm::on_btnDownMm_pressed()
{
    startCommandTimer(MachineControl::BitDownMMMode);
}

void CommandForm::on_btnDownMm_released()
{
    stopCommandTimer(MachineControl::BitDownMMMode);
}

void CommandForm::startCommandTimer(int command)
{
    stopCommandTimer(command);

    // 0. Отправляем команду сразу
    if (m_machine->isConnected()) {
        m_machine->sendCommand((MachineControl::ControlBit) command, true);
    }

    // 1. Создаем таймер
    m_commandTimer = new QTimer(this);

    // 2. Настраиваем действие
    connect(m_commandTimer, &QTimer::timeout, this, [this, command]() {
        if (m_machine->isConnected()) {
            m_machine->sendCommand((MachineControl::ControlBit) command, true);
        }
    });

    // 3. Запускаем таймер (интервал в мс, например 100мс)
    m_commandTimer->start(500);
}

void CommandForm::stopCommandTimer(int command)
{
    if (!m_commandTimer) {
        return;
    }
    if (m_machine->isConnected()) {
        m_machine->sendCommand((MachineControl::ControlBit) command, false);
    }
    m_commandTimer->stop();
    m_commandTimer->deleteLater();
    m_commandTimer.clear(); // Обнуляем указатель QPointer
}
