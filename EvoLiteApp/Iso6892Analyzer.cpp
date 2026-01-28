#include "Iso6892Analyzer.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

Iso6892Analyzer::Iso6892Analyzer(QObject *parent)
    : QObject(parent)
    , m_S0(1.0)
    , m_L0(50.0)
{}

void Iso6892Analyzer::setSpecimenParams(double areaS0, double gaugeLengthL0)
{
    m_S0 = (areaS0 > 0.0001) ? areaS0 : 1.0;
    m_L0 = (gaugeLengthL0 > 0.1) ? gaugeLengthL0 : 50.0;
}

void Iso6892Analyzer::reset()
{
    m_rawForce.clear();
    m_rawExtension.clear();
    m_stress.clear();
    m_strain.clear();
    m_results = Iso6892Results();
}

void Iso6892Analyzer::addDataPoint(double forceN, double extensionMm)
{
    m_rawForce.append(forceN);
    m_rawExtension.append(extensionMm);
}

void Iso6892Analyzer::normalizeData()
{
    m_stress.clear();
    m_strain.clear();
    int count = m_rawForce.size();

    m_stress.reserve(count);
    m_strain.reserve(count);

    for (int i = 0; i < count; ++i) {
        // Stress [MPa] = Force [N] / Area [mm2]
        m_stress.append(m_rawForce[i] / m_S0);

        // Strain [%] = (DeltaL [mm] / L0 [mm]) * 100
        m_strain.append((m_rawExtension[i] / m_L0) * 100.0);
    }
}

Iso6892Results Iso6892Analyzer::calculateResults()
{
    m_results = Iso6892Results();

    // 1. Нормализация данных
    normalizeData();

    if (m_stress.size() < 20) {
        qWarning() << "IsoAnalyzer: Not enough data points to analyze.";
        return m_results;
    }

    // 2. Расчет Модуля Упругости (E) и Коррекция нуля (Toe Compensation)
    double E = 0;
    double slopeOffset = 0;

    if (!fitElasticModulus(E, slopeOffset)) {
        qWarning() << "IsoAnalyzer: Failed to determine Elastic Modulus.";
        return m_results; // Без E анализ невозможен
    }
    m_results.E_Modulus = E;

    // 3. Применение Коррекции нуля
    // Сдвигаем весь график влево, чтобы упругий участок выходил из (0,0)
    for (int i = 0; i < m_strain.size(); ++i) {
        m_strain[i] -= slopeOffset;
        if (m_strain[i] < 0)
            m_strain[i] = 0;
    }

    // 4. Поиск Rm (Максимальное напряжение)
    double maxS = -1.0;
    int idxRm = 0;
    for (int i = 0; i < m_stress.size(); ++i) {
        if (m_stress[i] > maxS) {
            maxS = m_stress[i];
            idxRm = i;
        }
    }
    m_results.Rm = maxS;

    // 5. Поиск Ag (Удлинение при максимальной силе)
    if (idxRm < m_strain.size()) {
        m_results.Ag = m_strain[idxRm];
    }

    // 6. Поиск At (Полное удлинение при разрыве - последняя точка)
    if (!m_strain.isEmpty()) {
        m_results.At = m_strain.last();
    }

    // 7. Определение типа текучести (ReH vs Rp0.2)
    double valReH = 0, valReL = 0;
    bool foundYield = findYieldPointPhenomenon(E, idxRm, valReH, valReL);

    if (foundYield) {
        // Найден "Зуб" текучести (Discontinuous yielding)
        m_results.hasYieldPoint = true;
        m_results.ReH = valReH;
        m_results.ReL = valReL;
        m_results.Rp02 = 0; // Rp0.2 не применяется, если есть ReH (обычно)
    } else {
        // Зуба нет, плавная кривая (Continuous yielding)
        m_results.hasYieldPoint = false;
        m_results.Rp02 = findProofStrength(E, 0.2); // Ищем Rp0.2
    }

    m_results.isValid = true;
    return m_results;
}

// Метод наименьших квадратов для поиска наклона E
bool Iso6892Analyzer::fitElasticModulus(double &E, double &slopeOffset)
{
    // Находим максимум
    double maxVal = *std::max_element(m_stress.begin(), m_stress.end());
    if (maxVal < 1.0)
        return false;

    // ISO 6892 рекомендует диапазон для E
    // Берем участок от 10% до 40% от максимальной нагрузки
    double lowerLimit = maxVal * 0.10;
    double upperLimit = maxVal * 0.40;

    double sumX = 0, sumY = 0, sumXY = 0, sumXX = 0;
    int n = 0;

    for (int i = 0; i < m_stress.size(); ++i) {
        // Дополнительная проверка: только на восходящем участке (до Rm)
        if (m_stress[i] >= lowerLimit && m_stress[i] <= upperLimit) {
            double x = m_strain[i];
            double y = m_stress[i];
            sumX += x;
            sumY += y;
            sumXY += x * y;
            sumXX += x * x;
            n++;
        }
        // Если нагрузка стала падать (шейка) или ушла далеко, прерываем
        if (m_stress[i] > upperLimit * 1.5)
            break;
    }

    if (n < 5)
        return false; // Мало точек

    double denominator = (n * sumXX - sumX * sumX);
    if (std::abs(denominator) < 1e-9)
        return false;

    E = (n * sumXY - sumX * sumY) / denominator; // Наклон
    double intercept = (sumY - E * sumX) / n;    // Свободный член b

    // Смещение нуля по X (Toe correction)
    // 0 = E * x + b  =>  x = -b / E
    slopeOffset = -intercept / E;

    return true;
}

// Поиск физического предела текучести (ReH, ReL)
bool Iso6892Analyzer::findYieldPointPhenomenon(double E_modulus,
                                               int maxLoadIdx,
                                               double &ReH,
                                               double &ReL)
{
    // 1. БАЗОВЫЕ ПРОВЕРКИ БЕЗОПАСНОСТИ
    // Если массив слишком мал или индекс максимума выходит за границы
    if (m_stress.size() < 10 || maxLoadIdx >= m_stress.size() || maxLoadIdx < 5) {
        return false;
    }

    // 2. ПОИСК НАЧАЛА (startIdx)
    // Начинаем поиск не с самого начала массива, чтобы не ловить шум.
    // Ищем точку, где деформация превысила 0.05%
    int startIdx = 1; // ВАЖНО: Начинаем минимум с 1, чтобы i-1 был безопасным!

    for (int i = 0; i < m_strain.size(); ++i) {
        if (m_strain[i] > 0.05) {
            startIdx = i;
            break;
        }
    }

    // Если startIdx все еще 0 (например, первая точка уже > 0.05%), принудительно ставим 1
    if (startIdx < 1)
        startIdx = 1;

    // Если начало поиска оказалось дальше, чем (максимум - 5 точек), искать негде
    if (startIdx >= maxLoadIdx - 5) {
        return false;
    }

    // 3. ОСНОВНОЙ ЦИКЛ
    // Ищем в первой половине теста (до Rm минус отступ)
    for (int i = startIdx; i < maxLoadIdx - 5; ++i) {
        // ПРОВЕРКА ГРАНИЦ ВНУТРИ ЦИКЛА (на всякий случай)
        if (i - 1 < 0 || i + 1 >= m_stress.size())
            continue;

        double s_curr = m_stress[i];

        // А. Проверяем, является ли это локальным пиком (больше соседей)
        bool isLocalMax = (s_curr > m_stress[i - 1]) && (s_curr > m_stress[i + 1]);

        // Доп. условие: Напряжение должно быть значимым (больше 10% от макс), чтобы не ловить шум в нуле
        if (isLocalMax && s_curr > m_stress[maxLoadIdx] * 0.1) {
            // Б. Проверяем, есть ли за ним ЗНАЧИМЫЙ спад (drop)
            // Ищем локальный минимум после этого пика
            double minAfterPeak = s_curr;
            bool dropFound = false;

            // Смотрим вперед до начала нового роста или до конца зоны ReH
            // Ограничиваем поиск, чтобы не уйти в бесконечность
            int searchLimit = std::min(maxLoadIdx, i + 500);

            for (int k = i + 1; k < searchLimit; ++k) {
                if (m_stress[k] < minAfterPeak) {
                    minAfterPeak = m_stress[k];
                }

                // Если нагрузка снова выросла выше пика - значит упрочнение началось, выходим
                if (m_stress[k] > s_curr)
                    break;
            }

            // В. Критерий ISO: Спад должен быть ощутимым (не шум).
            // Допустим, 0.5% от величины напряжения ReH или жесткий порог шума.
            double dropMagnitude = s_curr - minAfterPeak;

            // Порог чувствительности: 0.5% от значения ИЛИ минимум 1 МПа (чтобы не ловить микрошум)
            double threshold = std::max(s_curr * 0.005, 1.0);

            if (dropMagnitude > threshold) {
                ReH = s_curr;
                ReL = minAfterPeak;
                return true; // Нашли!
            }
        }
    }

    return false; // Не нашли (плавная кривая)
}

// Поиск Rp0.2 (Метод пересечения со смещением)
double Iso6892Analyzer::findProofStrength(double E_modulus, double offsetPercent)
{
    // offsetPercent = 0.2

    // Оптимизация: начинаем поиск с зоны, где пластика уже могла начаться
    int startIdx = m_stress.size() / 20;

    for (int i = startIdx; i < m_stress.size(); ++i) {
        double e = m_strain[i]; // Уже скорректированная деформация
        double s_real = m_stress[i];

        // Уравнение прямой: y = E * (x - offset)
        double s_target = E_modulus * (e - offsetPercent);

        // Если реальный график нырнул ПОД линию
        if (s_real < s_target) {
            // Интерполяция
            if (i > 0) {
                double y1 = m_stress[i - 1];
                double y2 = m_stress[i];
                // Простое среднее, достаточно для MVP
                return (y1 + y2) / 2.0;
            }
            return s_real;
        }
    }
    return 0.0; // Не найдено (хрупкое разрушение до 0.2%)
}

QVector<QPointF> Iso6892Analyzer::getCorrectedCurve() const
{
    QVector<QPointF> points;
    if (m_stress.size() != m_strain.size())
        return points;

    points.reserve(m_stress.size());
    for (int i = 0; i < m_stress.size(); ++i) {
        points.append(QPointF(m_strain[i], m_stress[i]));
    }
    return points;
}

double Iso6892Analyzer::calculateZ(double finalDiameterDu) const
{
    // Защита: Если S0 не задана или некорректна, вернуть 0
    if (m_S0 <= 0.0001)
        return 0.0;

    // 1. Считаем конечную площадь шейки (Su)
    // Su = pi * r^2 = pi * (d/2)^2
    double radius = finalDiameterDu / 2.0;
    double Su = M_PI * radius * radius;

    // 2. Формула Z = (S0 - Su) / S0 * 100%
    double Z = (m_S0 - Su) / m_S0 * 100.0;

    // Защита от странных значений (например, если оператор ввел диаметр больше исходного)
    if (Z < 0)
        return 0.0;

    return Z;
}

double Iso6892Analyzer::calculateManualA(double finalLengthLu) const
{
    // Защита: Если L0 не задана
    if (m_L0 <= 0.1)
        return 0.0;

    // Формула A = (Lu - L0) / L0 * 100%
    double A = (finalLengthLu - m_L0) / m_L0 * 100.0;

    if (A < 0)
        return 0.0; // Образец не может сжаться при растяжении

    return A;
}
