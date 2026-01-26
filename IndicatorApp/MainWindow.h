#pragma once

#include <QLabel>
#include <QLineEdit>
#include <QMainWindow>
#include <QPushButton>
#include <QSpinBox>
#include <QTabWidget>

// Подключаем ядро и GUI модули
#include "EvoGui.h"
#include "EvoModbus.h"

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onConnect();
    void onDisconnect();
    void onError(QString msg);

private:
    void setupUi();

    // Modbus Controller
    EvoModbus::Controller *m_controller{nullptr};

    // UI Elements
    QTabWidget *m_tabs{nullptr};

    // Connection Toolbar
    QLineEdit *m_edtIp{nullptr};
    QSpinBox *m_sbPort{nullptr};
    QPushButton *m_btnConnect{nullptr};
    QPushButton *m_btnDisconnect{nullptr};
    QLabel *m_lblStatus{nullptr};

    // Widgets from EvoModbusGui
    EvoGui::SourceConfigWidget *m_sourceWidget{nullptr};
    EvoGui::FormulaConfigWidget *m_formulaWidget{nullptr};
    EvoGui::DashboardWidget *m_dashboardWidget{nullptr};
};
