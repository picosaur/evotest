#include "MainWindow.h"
#include "./ui_MainWindow.h"
#include "CommandForm.h"
#include "Iso6892Form.h"
#include "MachineControl.h"
#include "TcpConnForm.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    setWindowTitle("EvoLite");

    m_machine = new MachineControl(this);
    m_tcpConnForm = new TcpConnForm(this);
    m_tcpConnForm->setMachineControl(m_machine);

    m_commandForm = new CommandForm(m_machine, this);
    m_iso6892Form = new Iso6892Form(m_machine, this);

    while (ui->stackedWidget->count() > 0) {
        QWidget *widget = ui->stackedWidget->widget(0);
        ui->stackedWidget->removeWidget(widget);
        widget->setParent(nullptr);
        widget->deleteLater();
    }

    ui->toolBar->addWidget(m_tcpConnForm);
    ui->listWidget->addItem("ISO 6892-1. Металлические материалы. Испытания на растяжения");
    ui->stackedWidget->addWidget(m_iso6892Form);
    ui->commandWidget->setLayout(new QVBoxLayout());
    ui->commandWidget->layout()->addWidget(m_commandForm);

    connect(ui->listWidget,
            &QListWidget::currentRowChanged,
            ui->stackedWidget,
            &QStackedWidget::setCurrentIndex);
}

MainWindow::~MainWindow()
{
    delete ui;
}
