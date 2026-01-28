#pragma once
#include <QWidget>

class MachineControl;

namespace Ui {
class TcpConnForm;
}

class TcpConnForm : public QWidget
{
    Q_OBJECT

public:
    explicit TcpConnForm(QWidget *parent = nullptr);
    ~TcpConnForm();

    void setMachineControl(MachineControl *machine);

private slots:
    // Имя слота изменилось соответственно имени кнопки в UI
    void on_btnConnect_clicked();

    void onConnected();
    void onDisconnected();
    void onError(const QString &msg);

private:
    Ui::TcpConnForm *ui;
    MachineControl *m_machine = nullptr;

    void updateStatusUi(bool isConnected);
};
