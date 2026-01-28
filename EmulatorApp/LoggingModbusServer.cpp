#include "LoggingModbusServer.h"
#include <QDataStream>
#include <QModbusPdu>

LoggingModbusServer::LoggingModbusServer(QObject *parent)
    : QModbusTcpServer(parent)
{}

QModbusResponse LoggingModbusServer::processRequest(const QModbusPdu &request)
{
    QString funcName;
    quint16 startAddr = 0;
    quint16 count = 0;
    bool hasDetails = false;

    switch (request.functionCode()) {
    case QModbusPdu::ReadHoldingRegisters:
        funcName = "Read Holding (03)";
        request.decodeData(&startAddr, &count);
        hasDetails = true;
        return QModbusTcpServer::processRequest(request);
        break;
    case QModbusPdu::ReadInputRegisters:
        funcName = "Read Input (04)";
        request.decodeData(&startAddr, &count);
        hasDetails = true;
        return QModbusTcpServer::processRequest(request);
        break;
    case QModbusPdu::WriteSingleRegister:
        funcName = "Write Single (06)";
        request.decodeData(&startAddr, &count); // count здесь Value
        hasDetails = true;
        break;
    case QModbusPdu::WriteMultipleRegisters:
        funcName = "Write Multi (16)";
        request.decodeData(&startAddr, &count);
        hasDetails = true;
        break;
    default:
        funcName = QString("Func 0x%1").arg(request.functionCode(), 2, 16, QChar('0'));
    }

    if (hasDetails) {
        if (request.functionCode() == QModbusPdu::WriteSingleRegister)
            emit logMessage(
                QString("REQ: %1 | Addr: %2 | Value: %3").arg(funcName).arg(startAddr).arg(count));
        else
            emit logMessage(
                QString("REQ: %1 | Addr: %2 | Count: %3").arg(funcName).arg(startAddr).arg(count));
    } else {
        emit logMessage(QString("REQ: %1").arg(funcName));
    }

    return QModbusTcpServer::processRequest(request);
}
