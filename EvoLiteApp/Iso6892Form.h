#pragma once
#include <QTimer>
#include <QWidget>
class QGroupBox;

class MachineControl;
class Iso6892Analyzer;
class Iso6892Results;
class TensilePlotWidget;

namespace Ui {
class Iso6892Form;
}

class Iso6892Form : public QWidget
{
    Q_OBJECT

public:
    explicit Iso6892Form(MachineControl *machine, QWidget *parent = nullptr);
    ~Iso6892Form();

    // Метод для доступа к управлению машиной извне (если нужно подключиться из главного окна)
    MachineControl *getMachineControl() const { return m_machine; }

private slots:
    // --- UI Слоты (Кнопки управления тестом) ---
    void on_btnStart_clicked();
    void on_btnStop_clicked();
    void on_btnReturn_clicked();
    void on_btnZero_clicked();       // Тарирование (Обнуление)
    void on_btnCalcManual_clicked(); // Пересчет Z и A вручную

    // Переключение типа образца (блокировка полей)
    void on_rbRound_toggled(bool checked);

    // --- Слоты от MachineControl ---
    void onMachineConnected();
    void onMachineDisconnected();

    // Получение сырых данных (сигналы MachineControl)
    void onCurrentLoadChanged(float kgVal);
    void onLengthChanged(float mmVal);

    // --- Таймер GUI ---
    // Обновляет графики и цифры (независимо от частоты Modbus)
    void onGuiTimerTick();

private:
    Ui::Iso6892Form *ui;

    TensilePlotWidget *m_plot;

    // Логика анализа ISO 6892-1
    Iso6892Analyzer *m_analyzer;

    // Драйвер машины
    MachineControl *m_machine;

    // Таймер для отрисовки (20-25 FPS)
    QTimer *m_guiTimer;

    // Состояние теста
    bool m_isTestRunning;

    // Кэш последних данных с машины
    double m_lastRawForceKg;
    double m_lastRawExtMm;

    // Смещение нуля (Tare)
    double m_forceOffsetKg;
    double m_extOffsetMm;

    // Текущая площадь сечения (для Live-расчета напряжения)
    double m_currentS0;

    // Вспомогательные методы
    void displayResults(const Iso6892Results &res);
    void setupPlot();

    void replaceWidgetInGroupBox(QGroupBox *groupBox, QWidget *newWidget);
};
