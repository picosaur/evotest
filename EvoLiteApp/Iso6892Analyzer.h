#ifndef ISO6892ANALYZER_H
#define ISO6892ANALYZER_H

#include <QDebug>
#include <QObject>
#include <QPointF>
#include <QVector>
#include <cmath>

// Структура для хранения полных результатов по ISO 6892-1
struct Iso6892Results
{
    // --- Основные параметры ---
    double E_Modulus; // Модуль упругости (Young's Modulus), МПа
    double Rm;        // Временное сопротивление (Tensile Strength), МПа
    double At;        // Полное удлинение при разрыве (Total Extension), %
    double Ag;        // Равномерное удлинение при макс. силе (Plastic+Elastic extension at Fmax), %

    // --- Параметры текучести (Взаимоисключающие) ---
    bool hasYieldPoint; // true = обнаружен "зуб" (ReH/ReL), false = плавная кривая (Rp0.2)

    double Rp02; // Условный предел текучести (0.2% Proof Strength), МПа
    double ReH;  // Верхний предел текучести (Upper Yield Strength), МПа
    double ReL;  // Нижний предел текучести (Lower Yield Strength), МПа

    bool isValid; // Флаг успешности расчета

    Iso6892Results()
        : E_Modulus(0)
        , Rm(0)
        , At(0)
        , Ag(0)
        , hasYieldPoint(false)
        , Rp02(0)
        , ReH(0)
        , ReL(0)
        , isValid(false)
    {}
};

class Iso6892Analyzer : public QObject
{
    Q_OBJECT
public:
    explicit Iso6892Analyzer(QObject *parent = nullptr);

    // [Setup] Установка параметров образца. Вызывать перед стартом теста.
    // areaS0: Площадь сечения (мм^2)
    // gaugeLengthL0: Начальная длина (мм)
    void setSpecimenParams(double areaS0, double gaugeLengthL0);

    // [Control] Сброс данных
    void reset();

    // [Input] Добавление данных в реальном времени (Слот)
    // forceN: Сила с тензодатчика (Ньютоны)
    // extensionMm: Перемещение с энкодера/экстензометра (мм)
    void addDataPoint(double forceN, double extensionMm);

    // [Process] Основной расчет. Вызывать после остановки машины.
    Iso6892Results calculateResults();

    // [Output] Получить данные для построения графика (Stress-Strain)
    // Данные возвращаются уже с КОРРЕКЦИЕЙ НУЛЯ (сдвинуты влево).
    // X = Strain (%), Y = Stress (MPa)
    QVector<QPointF> getCorrectedCurve() const;

    // [Manual Input] Расчет относительного сужения (Z) после разрыва
    // finalDiameterDu: Конечный диаметр шейки образца (мм), измеренный вручную
    // Возвращает Z в процентах (%).
    double calculateZ(double finalDiameterDu) const;

    // [Manual Input] Расчет относительного удлинения (A) после разрыва вручную
    // finalLengthLu: Конечная длина образца (мм), измеренная вручную (сложив половинки)
    // Возвращает A в процентах (%).
    double calculateManualA(double finalLengthLu) const;

private:
    // Параметры
    double m_S0;
    double m_L0;

    // Сырые данные
    QVector<double> m_rawForce;     // N
    QVector<double> m_rawExtension; // mm

    // Расчетные данные (Нормализованные)
    QVector<double> m_stress; // MPa
    QVector<double> m_strain; // %

    Iso6892Results m_results;

    // --- Внутренние алгоритмы ---

    // 1. Пересчет Force/Ext -> Stress/Strain
    void normalizeData();

    // 2. Расчет Модуля Упругости (Least Squares)
    // slopeOffset - выходной параметр (сдвиг по X для коррекции нуля)
    bool fitElasticModulus(double &E, double &slopeOffset);

    // 3. Поиск физического предела текучести (Зуб - ReH/ReL)
    bool findYieldPointPhenomenon(double E_modulus, int maxLoadIdx, double &ReH, double &ReL);

    // 4. Поиск условного предела текучести (Rp0.2)
    double findProofStrength(double E_modulus, double offsetPercent);
};

#endif // ISO6892ANALYZER_H
