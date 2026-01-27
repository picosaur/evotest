#pragma once

#include <QLabel>
#include <QLineEdit>
#include <QMainWindow>
#include <QPushButton>
#include <QSpinBox>
#include <QTabWidget>
#include "EvoGui.h"
#include "EvoModbus.h"

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onConnectClicked();
    void onDisconnectClicked();

    // [NEW] Слоты меню
    void onActionSave();
    void onActionLoad();

    // [NEW] Слоты состояния
    void onConnectionState(bool connected);
    void onError(QString msg);

private:
    void setupUi();
    void updateUiState(bool connected);

    EvoModbus::Controller *m_controller{nullptr};

    QTabWidget *m_tabs{nullptr};
    QLineEdit *m_edtIp{nullptr};
    QSpinBox *m_sbPort{nullptr};
    QPushButton *m_btnConnect{nullptr};
    QPushButton *m_btnDisconnect{nullptr};
    QLabel *m_lblStatus{nullptr};

    EvoGui::SourceConfigWidget *m_sourceWidget{nullptr};
    EvoGui::FormulaConfigWidget *m_formulaWidget{nullptr};
    EvoGui::DashboardWidget *m_dashboardWidget{nullptr};
};
