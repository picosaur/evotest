#include "MainWindow.h"
#include <QApplication>
#include <QFileDialog>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QMenu>
#include <QMenuBar>
#include <QStatusBar>
#include <QStyle>
#include <QVBoxLayout>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    m_controller = new EvoModbus::Controller(this);

    // Подключаем "правильные" сигналы
    connect(m_controller,
            &EvoModbus::Controller::connectionStateChanged,
            this,
            &MainWindow::onConnectionState);
    connect(m_controller, &EvoModbus::Controller::error, this, &MainWindow::onError);

    setupUi();
    updateUiState(false); // Исходное состояние

    resize(1000, 700);
    setWindowTitle("IndicatorApp");
}

MainWindow::~MainWindow() {}

void MainWindow::setupUi()
{
    // --- Menu Bar ---
    QMenu *fileMenu = menuBar()->addMenu("File");
    fileMenu->addAction("Load Config...", this, &MainWindow::onActionLoad, QKeySequence::Open);
    fileMenu->addAction("Save Config...", this, &MainWindow::onActionSave, QKeySequence::Save);
    fileMenu->addSeparator();
    fileMenu->addAction("Exit", qApp, &QApplication::quit);

    auto *central = new QWidget(this);
    setCentralWidget(central);
    auto *mainLayout = new QVBoxLayout(central);

    // --- Connection Panel ---
    auto *connBox = new QGroupBox("Connection", this);
    auto *connLayout = new QHBoxLayout(connBox);

    m_edtIp = new QLineEdit("127.0.0.1", this);
    m_sbPort = new QSpinBox(this);
    m_sbPort->setRange(1, 65535);
    m_sbPort->setValue(502);

    m_btnConnect = new QPushButton("Connect", this);
    m_btnConnect->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));

    m_btnDisconnect = new QPushButton("Disconnect", this);
    m_btnDisconnect->setIcon(style()->standardIcon(QStyle::SP_MediaStop));

    m_lblStatus = new QLabel("Disconnected", this);
    m_lblStatus->setMinimumWidth(100);
    m_lblStatus->setAlignment(Qt::AlignCenter);

    connLayout->addWidget(new QLabel("IP:"));
    connLayout->addWidget(m_edtIp);
    connLayout->addWidget(new QLabel("Port:"));
    connLayout->addWidget(m_sbPort);
    connLayout->addWidget(m_btnConnect);
    connLayout->addWidget(m_btnDisconnect);
    connLayout->addWidget(m_lblStatus);
    connLayout->addStretch();

    mainLayout->addWidget(connBox);

    // --- Tabs ---
    m_tabs = new QTabWidget(this);

    // 1. Hardware Sources (Inline Mode)
    m_sourceWidget = new EvoGui::SourceConfigWidget(m_controller,
                                                    EvoGui::SourceEditMode::Inline,
                                                    this);
    m_tabs->addTab(m_sourceWidget, "1. Hardware Sources");

    // 2. Logic Formulas (Dialog Mode for complexity, or Inline)
    m_formulaWidget = new EvoGui::FormulaConfigWidget(m_controller,
                                                      EvoGui::SourceEditMode::Inline,
                                                      this);
    m_tabs->addTab(m_formulaWidget, "2. Logic & Computed");

    // 3. Dashboard
    m_dashboardWidget = new EvoGui::DashboardWidget(m_controller, this);
    m_tabs->addTab(m_dashboardWidget, "3. Dashboard Monitor");

    mainLayout->addWidget(m_tabs);

    connect(m_btnConnect, &QPushButton::clicked, this, &MainWindow::onConnectClicked);
    connect(m_btnDisconnect, &QPushButton::clicked, this, &MainWindow::onDisconnectClicked);
}

void MainWindow::onConnectClicked()
{
    m_lblStatus->setText("Connecting...");
    m_lblStatus->setStyleSheet("color: orange; font-weight: bold;");

    // Реальный коннект
    m_controller->connectToServer(m_edtIp->text(), m_sbPort->value());
    m_controller->start(500); // 500 ms polling
}

void MainWindow::onDisconnectClicked()
{
    m_controller->stop();
    m_controller->disconnectFrom();
}

void MainWindow::onConnectionState(bool connected)
{
    updateUiState(connected);
}

void MainWindow::updateUiState(bool connected)
{
    if (connected) {
        m_lblStatus->setText("CONNECTED");
        m_lblStatus->setStyleSheet("color: white; background-color: green; padding: 3px; "
                                   "border-radius: 3px; font-weight: bold;");
        m_btnConnect->setEnabled(false);
        m_btnDisconnect->setEnabled(true);
        m_edtIp->setEnabled(false);
        m_sbPort->setEnabled(false);

        // Переключаемся на дашборд для удобства
        // m_tabs->setCurrentWidget(m_dashboardWidget);
    } else {
        m_lblStatus->setText("DISCONNECTED");
        m_lblStatus->setStyleSheet("color: white; background-color: red; padding: 3px; "
                                   "border-radius: 3px; font-weight: bold;");
        m_btnConnect->setEnabled(true);
        m_btnDisconnect->setEnabled(false);
        m_edtIp->setEnabled(true);
        m_sbPort->setEnabled(true);
    }
}

void MainWindow::onError(QString msg)
{
    statusBar()->showMessage("Error: " + msg, 5000);
}

void MainWindow::onActionSave()
{
    QString fileName = QFileDialog::getSaveFileName(this, "Save Config", "", "JSON Files (*.json)");
    if (!fileName.isEmpty()) {
        if (m_controller->saveConfig(fileName)) {
            statusBar()->showMessage("Config saved to " + fileName, 3000);
        }
    }
}

void MainWindow::onActionLoad()
{
    QString fileName = QFileDialog::getOpenFileName(this, "Load Config", "", "JSON Files (*.json)");
    if (!fileName.isEmpty()) {
        if (m_controller->loadConfig(fileName)) {
            statusBar()->showMessage("Config loaded from " + fileName, 3000);

            // Важно: обновить виджеты, чтобы они подтянули новые данные из контроллера
            m_sourceWidget->loadFromController();
            m_formulaWidget->load();
            // Dashboard обновится сам по таймеру/сигналу, но список строк там ручной,
            // поэтому дашборд очистится (так как каналы пересозданы).
            // Это нормальное поведение, или нужно сохранять настройки дашборда тоже.
            // В текущей реализации Дашборд не сохраняется в JSON (только Sources/Formulas).
        }
    }
}
