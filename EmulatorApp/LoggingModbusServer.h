#ifndef LOGGINGMODBUSSERVER_H
#define LOGGINGMODBUSSERVER_H

#include <QModbusTcpServer>

class LoggingModbusServer : public QModbusTcpServer
{
    Q_OBJECT
public:
    explicit LoggingModbusServer(QObject *parent = nullptr);

protected:
    QModbusResponse processRequest(const QModbusPdu &request) override;

signals:
    void logMessage(const QString &msg);
};

#endif // LOGGINGMODBUSSERVER_H
