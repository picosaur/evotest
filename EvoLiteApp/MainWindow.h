#pragma once
#include <QMainWindow>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MachineControl;
class TcpConnForm;
class CommandForm;
class Iso6892Form;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private:
    Ui::MainWindow *ui;
    MachineControl *m_machine;
    TcpConnForm *m_tcpConnForm;
    CommandForm *m_commandForm;
    Iso6892Form *m_iso6892Form;
};
