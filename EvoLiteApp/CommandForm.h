#pragma once
#include <QPointer>
#include <QWidget>

namespace Ui {
class CommandForm;
}

class QTimer;
class MachineControl;

class CommandForm : public QWidget
{
    Q_OBJECT

public:
    explicit CommandForm(MachineControl *machine, QWidget *parent = nullptr);
    ~CommandForm();

private slots:
    void on_btnStart_pressed();
    void on_btnStart_released();
    void on_btnStop_pressed();
    void on_btnStop_released();
    void on_btnLift_pressed();
    void on_btnLift_released();
    void on_btnLower_pressed();
    void on_btnLower_released();
    void on_btnStartMm_pressed();
    void on_btnStartMm_released();
    void on_btnStopMm_pressed();
    void on_btnStopMm_released();

    void on_btnUpMm_pressed();
    void on_btnUpMm_released();

    void on_btnDownMm_pressed();

    void on_btnDownMm_released();

private:
    Ui::CommandForm *ui;
    MachineControl *m_machine{};
    QPointer<QTimer> m_commandTimer{};

    void startCommandTimer(int command);
    void stopCommandTimer(int command);
};
