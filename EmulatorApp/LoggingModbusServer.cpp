#include "LoggingModbusServer.h"
#include <QDataStream>
#include <QModbusPdu>

LoggingModbusServer::LoggingModbusServer(QObject *parent)
    : QModbusTcpServer(parent)
{}

QModbusResponse LoggingModbusServer::processRequest(const QModbusPdu &request)
{
    QString funcName;

    // ВАЖНО: Используем quint16, так как decodeData принимает только их
    quint16 startAddr = 0;
    quint16 count = 0;
    bool hasDetails = false; // Флаг, удалось ли извлечь адрес и кол-во

    // Разбор PDU для логирования
    switch (request.functionCode()) {
    case QModbusPdu::ReadHoldingRegisters:
        funcName = "Read Holding (0x03)";
        request.decodeData(&startAddr, &count);
        hasDetails = true;
        break;

    case QModbusPdu::ReadInputRegisters:
        funcName = "Read Input (0x04)";
        request.decodeData(&startAddr, &count);
        hasDetails = true;
        break;

    case QModbusPdu::WriteSingleRegister:
        funcName = "Write Single (0x06)";
        // Для 0x06: первый параметр - адрес, второй - значение
        request.decodeData(&startAddr, &count);
        hasDetails = true;
        break;

    case QModbusPdu::WriteMultipleRegisters:
        funcName = "Write Multi (0x10)";
        // Для 0x10 decodeData с двумя параметрами извлечет Адрес и Количество
        request.decodeData(&startAddr, &count);
        hasDetails = true;
        break;

    default:
        funcName = QString("Func 0x%1").arg(request.functionCode(), 2, 16, QChar('0'));
        break;
    }

    if (hasDetails) {
        // Если это запись одного регистра, count на самом деле является значением (Value)
        if (request.functionCode() == QModbusPdu::WriteSingleRegister) {
            emit logMessage(
                QString("REQ: %1 | Addr: %2 | Value: %3").arg(funcName).arg(startAddr).arg(count));
        } else {
            emit logMessage(
                QString("REQ: %1 | Addr: %2 | Count: %3").arg(funcName).arg(startAddr).arg(count));
        }
    } else {
        emit logMessage(QString("REQ: %1").arg(funcName));
    }

    // Вызываем оригинальный обработчик, чтобы Qt сделала свою работу (ответила клиенту)
    return QModbusTcpServer::processRequest(request);
}
