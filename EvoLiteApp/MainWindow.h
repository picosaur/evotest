#pragma once
#include <QMainWindow>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class TcpConnForm;
class MachineControl;
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
    Iso6892Form *m_iso6892Form;
};
