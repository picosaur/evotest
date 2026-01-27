#ifndef LOGGINGMODBUSSERVER_H
#define LOGGINGMODBUSSERVER_H

#include <QModbusTcpServer>
#include <QObject>

class LoggingModbusServer : public QModbusTcpServer
{
    Q_OBJECT
public:
    explicit LoggingModbusServer(QObject *parent = nullptr);

protected:
    // Переопределяем этот метод, чтобы перехватывать запросы
    QModbusResponse processRequest(const QModbusPdu &request) override;

signals:
    void logMessage(const QString &msg);
};

#endif // LOGGINGMODBUSSERVER_H
