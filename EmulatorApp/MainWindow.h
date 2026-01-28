#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QComboBox>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QMainWindow>
#include <QPushButton>
#include <QSpinBox>
#include <QTableWidget>
#include <QTextEdit>
#include <QTimer>

// Подключаем наши кастомные классы
#include "EmulIso6892.h"
#include "LoggingModbusServer.h"

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    // Слоты интерфейса
    void onBtnStartClicked();
    void onBtnStopClicked();

    // Слот для вывода логов от сервера
    void onLogMessage(const QString &msg);

    // Слот физики (вызывается по таймеру)
    void updateSimulation();

    // !!! ВАЖНО: Слот для мгновенной обработки записи регистров !!!
    // Позволяет ловить нажатия кнопок (Start/Stop) без задержек таймера
    void handleDataWritten(QModbusDataUnit::RegisterType type, int address, int size);

private:
    // --- Элементы GUI ---
    QLineEdit *leIp;
    QSpinBox *sbPort;
    QSpinBox *sbSlaveId;
    QComboBox *cmbSimType;
    QPushButton *btnStart;
    QPushButton *btnStop;
    QTextEdit *txtLog;
    QTableWidget *tableHolding; // Таблица Holding Registers (RW)
    QTableWidget *tableInput;   // Таблица Input Registers (RO)

    // --- Объекты логики ---
    LoggingModbusServer *modbusDevice;
    QTimer *simTimer;
    EmulIso6892 isoEmulator; // Математическая модель разрыва

    // --- Переменные процесса ---
    float currentPos = 0.0f;
    float currentLoad = 0.0f;
    float testTime = 0.0f;
    float startPos = 0.0f;
    float maxLoad = 0.0f;
    bool isTestRunning = false;

    // --- Карта регистров (Holding - Управление) ---
    const int H_CMD = 0;           // Командное слово (Биты)
    const int H_TEST_SPEED = 15;   // Скорость испытания (мм/мин)
    const int H_MANUAL_SPEED = 16; // Скорость ручного перемещения

    // --- Карта регистров (Input - Датчики) ---
    const int I_POS = 0;        // Текущее положение (Float)
    const int I_LOAD = 2;       // Текущая нагрузка (Float)
    const int I_TIME = 4;       // Время испытания (Float)
    const int I_ELONGATION = 6; // Удлинение (Float)
    const int I_MAX_LOAD = 8;   // Пиковая нагрузка (Float)

    // --- Вспомогательные функции ---
    void setupUi();      // Построение интерфейса кодом
    void updateTables(); // Обновление таблиц на экране

    // Работа с данными Modbus
    void setInputFloat(int addr, float val);
    quint16 getHoldingInt(int addr);
    void setHoldingInt(int addr, quint16 val);
    bool getBit(quint16 val, int bit);
};

#endif // MAINWINDOW_H
