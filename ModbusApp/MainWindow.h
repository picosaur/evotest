#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QModbusDataUnit>
#include <QModbusTcpClient>

class QLineEdit;
class QSpinBox;
class QPushButton;
class QTableWidget;
class QComboBox;
class QLabel;
class QGroupBox;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    // Слоты действий
    void onConnectClicked();
    void onReadClicked();
    void onWriteClicked();

    // Слоты Modbus событий
    void onStateChanged(QModbusDevice::State state);
    void onReadReady();
    void onWriteFinished();

    // Слот Декодера (пересчет отображения)
    void updateDecodedValue();

private:
    void setupUi();

    QModbusTcpClient *modbusDevice;

    // --- GUI Элементы ---

    // 1. Подключение
    QLineEdit *lineIpAddress;
    QSpinBox *spinPort;
    QPushButton *btnConnect;

    // 2. Чтение / Запись
    QComboBox *comboRegType;
    QSpinBox *spinStartAddr;

    QSpinBox *spinCount;
    QPushButton *btnRead;

    // Элементы записи (Тип + Текст + Кнопка)
    QComboBox *comboWriteType;
    QLineEdit *lineWriteValue;
    QPushButton *btnWrite;

    // 3. Таблица
    QTableWidget *tableResult;

    // 4. Декодер (Внизу)
    QComboBox *comboDataType; // Int16, Float...
    QComboBox *comboEndian;   // ABCD, CDAB...
    QLineEdit *lineDecodedResult;
};

#endif // MAINWINDOW_H
