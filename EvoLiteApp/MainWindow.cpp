#include "MainWindow.h"
#include "./ui_MainWindow.h"
#include "Iso6892Form.h"
#include "MachineControl.h"
#include "TcpConnForm.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    setWindowTitle("EvoLite");

    m_machine = new MachineControl();
    m_tcpConnForm = new TcpConnForm();
    m_iso6892Form = new Iso6892Form(m_machine, this);

    while (ui->stackedWidget->count() > 0) {
        QWidget *widget = ui->stackedWidget->widget(0);
        ui->stackedWidget->removeWidget(widget);
        widget->setParent(nullptr);
        widget->deleteLater();
    }

    m_tcpConnForm->setMachineControl(m_machine);
    ui->toolBar->addWidget(m_tcpConnForm);
    ui->listWidget->addItem("ISO 6892-1. Металлические материалы. Испытания на растяжения");
    ui->stackedWidget->addWidget(m_iso6892Form);

    connect(ui->listWidget,
            &QListWidget::currentRowChanged,
            ui->stackedWidget,
            &QStackedWidget::setCurrentIndex);
}

MainWindow::~MainWindow()
{
    delete ui;
}
