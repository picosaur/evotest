#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QLabel>
#include <QLineEdit>
#include <QMainWindow>
#include <QPushButton>
#include <QSpinBox>
#include <QTableWidget>
#include <QTextEdit>
#include <QTimer>
#include "loggingmodbusserver.h"

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onBtnStartClicked();
    void onBtnStopClicked();
    void onLogMessage(const QString &msg);
    void onStateChanged(QModbusDevice::State state);
    void updateSimulation();
    void handleDataWritten(QModbusDataUnit::RegisterType type, int address, int size);

private:
    // GUI элементы
    QLineEdit *leIp;
    QSpinBox *sbPort;
    QSpinBox *sbSlaveId; // <--- НОВОЕ ПОЛЕ
    QPushButton *btnStart;
    QPushButton *btnStop;
    QTextEdit *txtLog;
    QTableWidget *tableHolding;
    QTableWidget *tableInput;

    LoggingModbusServer *modbusDevice;
    QTimer *simTimer;

    // --- Переменные процесса ---
    float currentPos = 0.0f;
    float currentLoad = 0.0f;
    float testTime = 0.0f;
    float startPos = 0.0f;
    float maxLoad = 0.0f;
    bool isTestRunning = false;
    quint16 lastCmd = 0;

    // Адреса (Holding)
    const int H_CMD = 0;
    const int H_SPEED_MM = 2;
    const int H_SHIFT_X = 3;
    const int H_MOVE_X = 5;
    const int H_TEST_SPEED = 15;
    const int H_MANUAL_SPEED = 16;

    // Адреса (Input)
    const int I_POS = 0;
    const int I_LOAD = 2;
    const int I_TIME = 4;
    const int I_ELONGATION = 6;
    const int I_MAX_LOAD = 8;

    void setupUi();
    void updateTables();

    // Хелперы
    void setInputFloat(int addr, float val);
    float getHoldingFloat(int addr);
    quint16 getHoldingInt(int addr);
    void setHoldingInt(int addr, quint16 val);
    bool getBit(quint16 val, int bit);
};

#endif // MAINWINDOW_H
