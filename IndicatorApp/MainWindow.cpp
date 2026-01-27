#include "MainWindow.h"
#include <QGroupBox>
#include <QHBoxLayout>
#include <QStatusBar>
#include <QVBoxLayout>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    // 1. Создаем контроллер
    m_controller = new EvoModbus::Controller(this);

    // 2. Подписка на ошибки
    connect(m_controller, &EvoModbus::Controller::error, this, &MainWindow::onError);

    // 3. Строим интерфейс
    setupUi();

    resize(900, 600);
    setWindowTitle("EvoModbus Professional SCADA");
}

MainWindow::~MainWindow() {}

void MainWindow::setupUi()
{
    auto *central = new QWidget(this);
    setCentralWidget(central);
    auto *mainLayout = new QVBoxLayout(central);

    // --- Toolbar Подключения ---
    auto *connBox = new QGroupBox("Connection Settings", this);
    auto *connLayout = new QHBoxLayout(connBox);

    m_edtIp = new QLineEdit("127.0.0.1", this);
    m_sbPort = new QSpinBox(this);
    m_sbPort->setRange(1, 65535);
    m_sbPort->setValue(502);

    m_btnConnect = new QPushButton("Connect", this);
    m_btnDisconnect = new QPushButton("Disconnect", this);
    m_lblStatus = new QLabel("Disconnected", this);
    m_lblStatus->setStyleSheet("color: red; font-weight: bold;");

    connLayout->addWidget(new QLabel("IP:"));
    connLayout->addWidget(m_edtIp);
    connLayout->addWidget(new QLabel("Port:"));
    connLayout->addWidget(m_sbPort);
    connLayout->addWidget(m_btnConnect);
    connLayout->addWidget(m_btnDisconnect);
    connLayout->addSpacing(15);
    connLayout->addWidget(m_lblStatus);
    connLayout->addStretch();

    mainLayout->addWidget(connBox);

    // --- Вкладки ---
    m_tabs = new QTabWidget(this);
    mainLayout->addWidget(m_tabs);

    // 1. Hardware Config
    m_sourceWidget = new EvoGui::SourceConfigWidget(m_controller,
                                                    EvoGui::SourceEditMode::Dialog,
                                                    this);
    m_tabs->addTab(m_sourceWidget, "1. Channels (Hardware)");

    // 2. Logic Config
    m_formulaWidget = new EvoGui::FormulaConfigWidget(m_controller,
                                                      EvoGui::SourceEditMode::Dialog,
                                                      this);
    m_tabs->addTab(m_formulaWidget, "2. Formulas (Logic)");

    // 3. Dashboard
    m_dashboardWidget = new EvoGui::DashboardWidget(m_controller, this);
    m_tabs->addTab(m_dashboardWidget, "3. Live Dashboard");

    // --- Connect Signals ---
    connect(m_btnConnect, &QPushButton::clicked, this, &MainWindow::onConnect);
    connect(m_btnDisconnect, &QPushButton::clicked, this, &MainWindow::onDisconnect);
}

void MainWindow::onConnect()
{
    m_controller->connectToServer(m_edtIp->text(), m_sbPort->value());
    m_controller->start(500); // 500 ms poll
    m_lblStatus->setText("Connected");
    m_lblStatus->setStyleSheet("color: green; font-weight: bold;");
}

void MainWindow::onDisconnect()
{
    m_controller->stop();
    m_lblStatus->setText("Disconnected");
    m_lblStatus->setStyleSheet("color: red; font-weight: bold;");
}

void MainWindow::onError(QString msg)
{
    statusBar()->showMessage("Error: " + msg, 5000);
}
