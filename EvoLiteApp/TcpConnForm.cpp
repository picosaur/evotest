#include "TcpConnForm.h"
#include <QDebug>
#include <QMessageBox>
#include "MachineControl.h"
#include "ui_TcpConnForm.h"

TcpConnForm::TcpConnForm(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::TcpConnForm)
{
    ui->setupUi(this);

    // Настройки по умолчанию
    ui->leIpAddress->setText("127.0.0.1");

    ui->sbPort->setRange(1, 65535);
    ui->sbPort->setValue(5020);

    ui->sbServerId->setRange(1, 255);
    ui->sbServerId->setValue(1);

    updateStatusUi(false);
}

TcpConnForm::~TcpConnForm()
{
    delete ui;
}

void TcpConnForm::setMachineControl(MachineControl *machine)
{
    m_machine = machine;

    if (m_machine) {
        connect(m_machine, &MachineControl::connected, this, &TcpConnForm::onConnected);
        connect(m_machine, &MachineControl::disconnected, this, &TcpConnForm::onDisconnected);
        connect(m_machine, &MachineControl::errorOccurred, this, &TcpConnForm::onError);

        updateStatusUi(m_machine->isConnected());
    }
}

// Слот переименован
void TcpConnForm::on_btnConnect_clicked()
{
    if (!m_machine)
        return;

    if (m_machine->isConnected()) {
        m_machine->disconnectDevice();
    } else {
        QString ip = ui->leIpAddress->text();
        int port = ui->sbPort->value();
        int serverId = ui->sbServerId->value();

        if (ip.isEmpty()) {
            QMessageBox::warning(this, "Ошибка", "Введите IP адрес");
            return;
        }

        ui->btnConnect->setEnabled(false);
        ui->btnConnect->setText("Wait...");

        bool success = m_machine->connectTCP(ip, port, serverId);

        if (!success) {
            ui->btnConnect->setEnabled(true);
            updateStatusUi(false);
            QMessageBox::critical(this, "Ошибка", "Не удалось инициировать подключение");
        }
    }
}

void TcpConnForm::onConnected()
{
    updateStatusUi(true);
    if (m_machine) {
        m_machine->startPolling(200);
    }
}

void TcpConnForm::onDisconnected()
{
    updateStatusUi(false);
}

void TcpConnForm::onError(const QString &msg)
{
    qDebug() << "TCP Error:" << msg;
    ui->btnConnect->setEnabled(true);
}

void TcpConnForm::updateStatusUi(bool isConnected)
{
    ui->btnConnect->setEnabled(true);

    if (isConnected) {
        ui->btnConnect->setText("Disconnect");

        ui->lblStatus->setText("Connected");
        ui->lblStatus->setStyleSheet("QLabel { color : green; font-weight: bold; }");

        ui->leIpAddress->setEnabled(false);
        ui->sbServerId->setEnabled(false);
        ui->sbPort->setEnabled(false);
    } else {
        ui->btnConnect->setText("Connect");

        ui->lblStatus->setText("Disconnected");
        ui->lblStatus->setStyleSheet("QLabel { color : red; font-weight: bold; }");

        ui->leIpAddress->setEnabled(true);
        ui->sbServerId->setEnabled(true);
        ui->sbPort->setEnabled(true);
    }
}
