#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QModbusDataUnit>
#include <QModbusTcpClient>
#include <QTimer>
#include <QVector>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    // Подключение
    void on_connectBtn_clicked();
    void onStateChanged(QModbusDevice::State state);

    // Слот приема данных
    void onReadReady();

    // Команды
    void on_writeAllCommandsBtn_clicked();
    void on_readAllCommandsBtn_clicked();

    // Параметры (Кнопки)
    void on_speedMmMinWriteBtn_clicked();
    void on_speedMmMinReadBtn_clicked();
    void on_moveByMmWriteBtn_clicked();
    void on_moveByMmReadBtn_clicked();
    void on_moveToMmWriteBtn_clicked();
    void on_moveToMmReadBtn_clicked();
    void on_encoderPulsesWriteBtn_clicked();
    void on_encoderPulsesReadBtn_clicked();
    void on_motorScrewRatioWriteBtn_clicked();
    void on_motorScrewRatioReadBtn_clicked();
    void on_screwPitchWriteBtn_clicked();
    void on_screwPitchReadBtn_clicked();
    void on_sensorRangeKnWriteBtn_clicked();
    void on_sensorRangeKnReadBtn_clicked();
    void on_sensorSensitivityWriteBtn_clicked();
    void on_sensorSensitivityReadBtn_clicked();
    void on_controlErrorMmWriteBtn_clicked();
    void on_controlErrorMmReadBtn_clicked();
    void on_testSpeedWriteBtn_clicked();
    void on_testSpeedReadBtn_clicked();
    void on_manualSpeedWriteBtn_clicked();
    void on_manualSpeedReadBtn_clicked();

    // Индикаторы (Кнопки)
    void on_currentPositionReadBtn_clicked();
    void on_currentLoadReadBtn_clicked();
    void on_testTimeReadBtn_clicked();
    void on_testLengthReadBtn_clicked();
    void on_maxLoadReadBtn_clicked();

    // Авто-чтение
    void on_autoReadCommandsCheckBox_toggled(bool checked);
    void on_autoReadParamsCheckBox_toggled(bool checked);
    void on_autoReadIndicatorsCheckBox_toggled(bool checked);

    // Обработчики таймеров
    void processAutoReadCommands();
    void processAutoReadParams();
    void processAutoReadIndicators();

private:
    Ui::MainWindow *ui;
    QModbusTcpClient *modbusDevice;

    QTimer *timerCommands;
    QTimer *timerParams;
    QTimer *timerIndicators;

    // Вспомогательные функции
    quint16 getCommandWordFromUI();
    void setCommandUIFromWord(quint16 word);

    QVector<quint16> floatToRegisters(float value);
    float registersToFloat(const QVector<quint16> &values);

    void writeSingleRegister(int address, quint16 value);
    void writeFloatRegister(int address, float value);
    void readHoldingRegister(int address, int count, const QString &id);
    void readInputRegister(int address, int count, const QString &id);
};

#endif // MAINWINDOW_H
