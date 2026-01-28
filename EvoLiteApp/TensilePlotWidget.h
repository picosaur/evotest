#ifndef TENSILEPLOTWIDGET_H
#define TENSILEPLOTWIDGET_H

#include "Iso6892Analyzer.h" // Подключаем, чтобы знать структуру Iso6892Results
#include "qcustomplot.h"

class TensilePlotWidget : public QCustomPlot
{
    Q_OBJECT
public:
    explicit TensilePlotWidget(QWidget *parent = nullptr);

    // Настройка внешнего вида (оси, сетка)
    void setupUi();

    // Установка параметров для Live-пересчета (Force -> MPa, mm -> %)
    void setSpecimenParams(double areaS0, double gaugeLengthL0);

    // Очистка графика перед новым тестом
    void resetPlot();

    // --- Метод для REAL-TIME отрисовки ---
    // Вызывайте его в цикле опроса датчиков
    void addLivePoint(double forceN, double extensionMm);

    // --- Метод для ПОСТ-АНАЛИЗА ---
    // Вызывайте его после окончания теста, когда Analyzer посчитал результаты
    void plotFinalAnalysis(const Iso6892Results &results, const QVector<QPointF> &correctedCurve);

private:
    // Параметры для конвертации на лету
    double m_S0;
    double m_L0;

    // Указатели на графики (Layers)
    QCPGraph *m_mainCurve;      // Основная синяя линия
    QCPGraph *m_elasticLine;    // Зеленая линия модуля упругости
    QCPGraph *m_rp02Marker;     // Точка Rp0.2
    QCPItemText *m_resultLabel; // Текстовая метка на графике
};

#endif // TENSILEPLOTWIDGET_H
