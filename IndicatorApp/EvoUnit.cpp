#include "EvoUnit.h"
#include <QLocale>
#include <QMap>
#include <QRegularExpression>
#include <limits>
#include <mutex>

namespace EvoUnit {

class TranslationContext
{
    Q_DECLARE_TR_FUNCTIONS(TranslationContext)
};

struct UnitEntry
{
    UnitDef def{};
    UnitCategory category{UnitCategory::Unknown};
    const char *nameKey{nullptr};
    const char *symKey{nullptr};
};

static QMap<MeasUnit, UnitEntry> registry;

// =========================================================================
// Реализация UnitDef
// =========================================================================
UnitDef::UnitDef(double factor, double offset, Dimensions dim)
    : m_factor{factor}
    , m_offset{offset}
    , m_dim{dim}
{}

UnitDef UnitDef::operator*(const UnitDef &other) const
{
    return UnitDef(m_factor * other.m_factor,
                   0.0,
                   {static_cast<int8_t>(m_dim.L + other.m_dim.L),
                    static_cast<int8_t>(m_dim.M + other.m_dim.M),
                    static_cast<int8_t>(m_dim.T + other.m_dim.T),
                    static_cast<int8_t>(m_dim.Theta + other.m_dim.Theta),
                    static_cast<int8_t>(m_dim.I + other.m_dim.I)});
}

UnitDef UnitDef::operator/(const UnitDef &other) const
{
    return UnitDef(m_factor / other.m_factor,
                   0.0,
                   {static_cast<int8_t>(m_dim.L - other.m_dim.L),
                    static_cast<int8_t>(m_dim.M - other.m_dim.M),
                    static_cast<int8_t>(m_dim.T - other.m_dim.T),
                    static_cast<int8_t>(m_dim.Theta - other.m_dim.Theta),
                    static_cast<int8_t>(m_dim.I - other.m_dim.I)});
}

UnitDef UnitDef::operator*(double scale) const
{
    return UnitDef(m_factor * scale, m_offset, m_dim);
}

// =========================================================================
// ИНИЦИАЛИЗАЦИЯ РЕЕСТРА
// =========================================================================
static void initRegistry()
{
    if (!registry.isEmpty())
        return;

    // --- БАЗОВЫЕ ЕДИНИЦЫ (СИ) ---
    UnitDef m{1.0, 0.0, {1, 0, 0, 0, 0}};  // Метр
    UnitDef kg{1.0, 0.0, {0, 1, 0, 0, 0}}; // Килограмм
    UnitDef s{1.0, 0.0, {0, 0, 1, 0, 0}};  // Секунда
    UnitDef K{1.0, 0.0, {0, 0, 0, 1, 0}};  // Кельвин
    UnitDef A{1.0, 0.0, {0, 0, 0, 0, 1}};  // Ампер

    // --- БЕЗРАЗМЕРНАЯ ---
    UnitDef dimless{1.0, 0.0, {0, 0, 0, 0, 0}};

    // --- 1. ДЛИНА ---
    UnitDef mm = m * 0.001;
    UnitDef cm = m * 0.01;
    UnitDef dm = m * 0.1;
    UnitDef km = m * 1000.0;
    UnitDef um = m * 1.0e-6;

    UnitDef inch = m * 0.0254;
    UnitDef ft = inch * 12.0;
    UnitDef yd = ft * 3.0;
    UnitDef mil = inch * 0.001; // мил

    // Площади (для расчета давления)
    UnitDef mm2 = mm * mm;
    UnitDef cm2 = cm * cm;
    UnitDef inch2 = inch * inch;

    // --- 2. ВРЕМЯ ---
    UnitDef ms = s * 0.001;
    UnitDef us = s * 1.0e-6;
    UnitDef min = s * 60.0;
    UnitDef hr = min * 60.0;
    UnitDef day = hr * 24.0;

    // --- 3. МАССА ---
    UnitDef g_mass = kg * 0.001;
    UnitDef lb = kg * 0.45359237;
    UnitDef tonne = kg * 1000.0;

    // --- 4. ТЕМПЕРАТУРА ---
    UnitDef degC{1.0, 273.15, {0, 0, 0, 1, 0}};
    UnitDef degF{5.0 / 9.0, 459.67, {0, 0, 0, 1, 0}};

    // --- 5. СИЛА ТОКА ---
    UnitDef mA = A * 0.001;
    UnitDef uA = A * 1.0e-6;
    UnitDef kA = A * 1000.0;

    // --- 6. СИЛА (F = ma) ---
    UnitDef N = kg * m / (s * s);
    UnitDef kN = N * 1000.0;
    UnitDef MN = N * 1.0e6;

    const double gravity{9.80665};
    UnitDef kgf = N * gravity;
    UnitDef gf = kgf * 0.001;
    UnitDef tf = kgf * 1000.0;

    UnitDef lbf = lb * gravity * m / (s * s);
    UnitDef kip = lbf * 1000.0;

    // --- 7. ДАВЛЕНИЕ (P = F / S) ---
    UnitDef Pa = N / (m * m);
    UnitDef kPa = Pa * 1000.0;
    UnitDef MPa = Pa * 1.0e6;
    UnitDef GPa = Pa * 1.0e9;

    UnitDef bar = Pa * 1.0e5;
    UnitDef mbar = bar * 0.001;

    UnitDef psi = lbf / inch2;
    UnitDef ksi = psi * 1000.0;
    UnitDef kgf_cm2 = kgf / cm2;
    UnitDef atm = Pa * 101325.0;

    // --- 8. НАПРЯЖЕНИЕ ---
    UnitDef V = (N * m / s) / A;
    UnitDef mV = V * 0.001;
    UnitDef uV = V * 1.0e-6;
    UnitDef kV = V * 1000.0;

    // --- Лямбда для добавления ---
    auto add = [](MeasUnit u, UnitDef d, UnitCategory cat, const char *sym, const char *name) {
        registry.insert(u, {d, cat, name, sym});
    };

    // =========================================================================
    // ЗАПОЛНЕНИЕ РЕЕСТРА
    // =========================================================================

    // БЕЗРАЗМЕРНАЯ
    add(MeasUnit::Dimensionless,
        dimless,
        UnitCategory::Dimensionless,
        QT_TR_NOOP("-"),
        QT_TR_NOOP("Безразмерная"));

    // ДЛИНА
    add(MeasUnit::Millimeter, mm, UnitCategory::Length, QT_TR_NOOP("мм"), QT_TR_NOOP("Миллиметр"));
    add(MeasUnit::Centimeter, cm, UnitCategory::Length, QT_TR_NOOP("см"), QT_TR_NOOP("Сантиметр"));
    add(MeasUnit::Decimeter, dm, UnitCategory::Length, QT_TR_NOOP("дм"), QT_TR_NOOP("Дециметр"));
    add(MeasUnit::Meter, m, UnitCategory::Length, QT_TR_NOOP("м"), QT_TR_NOOP("Метр"));
    add(MeasUnit::Kilometer, km, UnitCategory::Length, QT_TR_NOOP("км"), QT_TR_NOOP("Километр"));
    add(MeasUnit::Micrometer, um, UnitCategory::Length, QT_TR_NOOP("мкм"), QT_TR_NOOP("Микрометр"));
    add(MeasUnit::Inch, inch, UnitCategory::Length, QT_TR_NOOP("дюйм"), QT_TR_NOOP("Дюйм"));
    add(MeasUnit::Foot, ft, UnitCategory::Length, QT_TR_NOOP("фт"), QT_TR_NOOP("Фут"));
    add(MeasUnit::Yard, yd, UnitCategory::Length, QT_TR_NOOP("ярд"), QT_TR_NOOP("Ярд"));
    add(MeasUnit::Mil, mil, UnitCategory::Length, QT_TR_NOOP("мил"), QT_TR_NOOP("Мил"));

    // ВРЕМЯ
    add(MeasUnit::Second, s, UnitCategory::Time, QT_TR_NOOP("с"), QT_TR_NOOP("Секунда"));
    add(MeasUnit::Minute, min, UnitCategory::Time, QT_TR_NOOP("мин"), QT_TR_NOOP("Минута"));
    add(MeasUnit::Hour, hr, UnitCategory::Time, QT_TR_NOOP("ч"), QT_TR_NOOP("Час"));
    add(MeasUnit::Day, day, UnitCategory::Time, QT_TR_NOOP("дн"), QT_TR_NOOP("День"));
    add(MeasUnit::Millisecond, ms, UnitCategory::Time, QT_TR_NOOP("мс"), QT_TR_NOOP("Миллисекунда"));
    add(MeasUnit::Microsecond, us, UnitCategory::Time, QT_TR_NOOP("мкс"), QT_TR_NOOP("Микросекунда"));

    // ТЕМПЕРАТУРА
    add(MeasUnit::Kelvin, K, UnitCategory::Temperature, QT_TR_NOOP("K"), QT_TR_NOOP("Кельвин"));
    add(MeasUnit::Celsius,
        degC,
        UnitCategory::Temperature,
        QT_TR_NOOP("°C"),
        QT_TR_NOOP("Градус Цельсия"));
    add(MeasUnit::Fahrenheit,
        degF,
        UnitCategory::Temperature,
        QT_TR_NOOP("°F"),
        QT_TR_NOOP("Градус Фаренгейта"));

    // МАССА
    add(MeasUnit::Gram, g_mass, UnitCategory::Mass, QT_TR_NOOP("г"), QT_TR_NOOP("Грамм"));
    add(MeasUnit::Kilogram, kg, UnitCategory::Mass, QT_TR_NOOP("кг"), QT_TR_NOOP("Килограмм"));
    add(MeasUnit::Pound, lb, UnitCategory::Mass, QT_TR_NOOP("lb"), QT_TR_NOOP("Фунт"));
    add(MeasUnit::Tonne, tonne, UnitCategory::Mass, QT_TR_NOOP("т"), QT_TR_NOOP("Тонна"));

    // СИЛА
    add(MeasUnit::Newton, N, UnitCategory::Force, QT_TR_NOOP("Н"), QT_TR_NOOP("Ньютон"));
    add(MeasUnit::KiloNewton, kN, UnitCategory::Force, QT_TR_NOOP("кН"), QT_TR_NOOP("Килоньютон"));
    add(MeasUnit::MegaNewton, MN, UnitCategory::Force, QT_TR_NOOP("МН"), QT_TR_NOOP("Меганьютон"));
    add(MeasUnit::GramForce, gf, UnitCategory::Force, QT_TR_NOOP("гс"), QT_TR_NOOP("Грамм-сила"));
    add(MeasUnit::KilogramForce,
        kgf,
        UnitCategory::Force,
        QT_TR_NOOP("кгс"),
        QT_TR_NOOP("Килограмм-сила"));
    add(MeasUnit::TonForce, tf, UnitCategory::Force, QT_TR_NOOP("тс"), QT_TR_NOOP("Тонна-сила"));
    add(MeasUnit::PoundForce,
        lbf,
        UnitCategory::Force,
        QT_TR_NOOP("фунт-сила"),
        QT_TR_NOOP("Фунт-сила"));
    add(MeasUnit::Kip, kip, UnitCategory::Force, QT_TR_NOOP("kip"), QT_TR_NOOP("Килофунт-сила"));

    // ДАВЛЕНИЕ
    add(MeasUnit::Pascal, Pa, UnitCategory::Pressure, QT_TR_NOOP("Па"), QT_TR_NOOP("Паскаль"));
    add(MeasUnit::KiloPascal,
        kPa,
        UnitCategory::Pressure,
        QT_TR_NOOP("кПа"),
        QT_TR_NOOP("Килопаскаль"));
    add(MeasUnit::MegaPascal,
        MPa,
        UnitCategory::Pressure,
        QT_TR_NOOP("МПа"),
        QT_TR_NOOP("Мегапаскаль"));
    add(MeasUnit::GigaPascal,
        GPa,
        UnitCategory::Pressure,
        QT_TR_NOOP("ГПа"),
        QT_TR_NOOP("Гигапаскаль"));
    add(MeasUnit::Bar, bar, UnitCategory::Pressure, QT_TR_NOOP("бар"), QT_TR_NOOP("Бар"));
    add(MeasUnit::MilliBar, mbar, UnitCategory::Pressure, QT_TR_NOOP("мбар"), QT_TR_NOOP("Миллибар"));
    add(MeasUnit::PSI, psi, UnitCategory::Pressure, QT_TR_NOOP("psi"), QT_TR_NOOP("PSI"));
    add(MeasUnit::KSI, ksi, UnitCategory::Pressure, QT_TR_NOOP("ksi"), QT_TR_NOOP("KSI"));
    add(MeasUnit::Kgf_per_CM2,
        kgf_cm2,
        UnitCategory::Pressure,
        QT_TR_NOOP("кгс/см²"),
        QT_TR_NOOP("Тех. атмосфера"));
    add(MeasUnit::Atmosphere,
        atm,
        UnitCategory::Pressure,
        QT_TR_NOOP("атм"),
        QT_TR_NOOP("Атмосфера"));

    // НАПРЯЖЕНИЕ (VOLTAGE)
    add(MeasUnit::Microvolt, uV, UnitCategory::Voltage, QT_TR_NOOP("мкВ"), QT_TR_NOOP("Микровольт"));
    add(MeasUnit::Millivolt, mV, UnitCategory::Voltage, QT_TR_NOOP("мВ"), QT_TR_NOOP("Милливольт"));
    add(MeasUnit::Volt, V, UnitCategory::Voltage, QT_TR_NOOP("В"), QT_TR_NOOP("Вольт"));
    add(MeasUnit::Kilovolt, kV, UnitCategory::Voltage, QT_TR_NOOP("кВ"), QT_TR_NOOP("Киловольт"));

    // СИЛА ТОКА (CURRENT)
    add(MeasUnit::Microampere,
        uA,
        UnitCategory::Current,
        QT_TR_NOOP("мкА"),
        QT_TR_NOOP("Микроампер"));
    add(MeasUnit::Milliampere, mA, UnitCategory::Current, QT_TR_NOOP("мА"), QT_TR_NOOP("Миллиампер"));
    add(MeasUnit::Ampere, A, UnitCategory::Current, QT_TR_NOOP("А"), QT_TR_NOOP("Ампер"));
    add(MeasUnit::Kiloampere, kA, UnitCategory::Current, QT_TR_NOOP("кА"), QT_TR_NOOP("Килоампер"));

    // --- СКОРОСТЬ (VELOCITY) ---
    // В Секунду
    add(MeasUnit::MM_per_Sec,
        mm / s,
        UnitCategory::Velocity,
        QT_TR_NOOP("мм/с"),
        QT_TR_NOOP("мм в секунду"));
    add(MeasUnit::CM_per_Sec,
        cm / s,
        UnitCategory::Velocity,
        QT_TR_NOOP("см/с"),
        QT_TR_NOOP("см в секунду"));
    add(MeasUnit::DM_per_Sec,
        dm / s,
        UnitCategory::Velocity,
        QT_TR_NOOP("дм/с"),
        QT_TR_NOOP("дм в секунду"));
    add(MeasUnit::Meter_per_Sec,
        m / s,
        UnitCategory::Velocity,
        QT_TR_NOOP("м/с"),
        QT_TR_NOOP("м в секунду"));
    add(MeasUnit::Inch_per_Sec,
        inch / s,
        UnitCategory::Velocity,
        QT_TR_NOOP("дюйм/с"),
        QT_TR_NOOP("дюйм в секунду"));
    add(MeasUnit::Foot_per_Sec,
        ft / s,
        UnitCategory::Velocity,
        QT_TR_NOOP("фут/с"),
        QT_TR_NOOP("фут в секунду"));
    add(MeasUnit::Mil_per_Sec,
        mil / s,
        UnitCategory::Velocity,
        QT_TR_NOOP("мил/с"),
        QT_TR_NOOP("мил в секунду"));

    // В Минуту
    add(MeasUnit::MM_per_Min,
        mm / min,
        UnitCategory::Velocity,
        QT_TR_NOOP("мм/мин"),
        QT_TR_NOOP("мм в минуту"));
    add(MeasUnit::CM_per_Min,
        cm / min,
        UnitCategory::Velocity,
        QT_TR_NOOP("см/мин"),
        QT_TR_NOOP("см в минуту"));
    add(MeasUnit::DM_per_Min,
        dm / min,
        UnitCategory::Velocity,
        QT_TR_NOOP("дм/мин"),
        QT_TR_NOOP("дм в минуту"));
    add(MeasUnit::Meter_per_Min,
        m / min,
        UnitCategory::Velocity,
        QT_TR_NOOP("м/мин"),
        QT_TR_NOOP("м в минуту"));
    add(MeasUnit::Inch_per_Min,
        inch / min,
        UnitCategory::Velocity,
        QT_TR_NOOP("дюйм/мин"),
        QT_TR_NOOP("дюйм в минуту"));
    add(MeasUnit::Foot_per_Min,
        ft / min,
        UnitCategory::Velocity,
        QT_TR_NOOP("фут/мин"),
        QT_TR_NOOP("фут в минуту"));
    add(MeasUnit::Mil_per_Min,
        mil / min,
        UnitCategory::Velocity,
        QT_TR_NOOP("мил/мин"),
        QT_TR_NOOP("мил в минуту"));

    add(MeasUnit::KM_per_Hour,
        km / hr,
        UnitCategory::Velocity,
        QT_TR_NOOP("км/ч"),
        QT_TR_NOOP("км в час"));

    // --- СКОРОСТЬ РОСТА ДАВЛЕНИЯ (PRESSURE RATE) ---
    // Па/с, кПа/с, МПа/с, ГПа/с
    add(MeasUnit::Pascal_per_Sec,
        Pa / s,
        UnitCategory::PressureRate,
        QT_TR_NOOP("Па/с"),
        QT_TR_NOOP("Паскаль в секунду"));
    add(MeasUnit::KiloPascal_per_Sec,
        kPa / s,
        UnitCategory::PressureRate,
        QT_TR_NOOP("кПа/с"),
        QT_TR_NOOP("Килопаскаль в секунду"));
    add(MeasUnit::MegaPascal_per_Sec,
        MPa / s,
        UnitCategory::PressureRate,
        QT_TR_NOOP("МПа/с"),
        QT_TR_NOOP("Мегапаскаль в секунду"));
    add(MeasUnit::GigaPascal_per_Sec,
        GPa / s,
        UnitCategory::PressureRate,
        QT_TR_NOOP("ГПа/с"),
        QT_TR_NOOP("Гигапаскаль в секунду"));

    // Н/мм2/с, кгс/см2/с
    add(MeasUnit::Newton_per_MM2_per_Sec,
        N / mm2 / s,
        UnitCategory::PressureRate,
        QT_TR_NOOP("Н/мм²/с"),
        QT_TR_NOOP("Н/мм² в секунду"));
    add(MeasUnit::Kgf_per_CM2_per_Sec,
        kgf / cm2 / s,
        UnitCategory::PressureRate,
        QT_TR_NOOP("кгс/см²/с"),
        QT_TR_NOOP("кгс/см² в секунду"));

    // psi/c, ksi/c
    add(MeasUnit::PSI_per_Sec,
        psi / s,
        UnitCategory::PressureRate,
        QT_TR_NOOP("psi/с"),
        QT_TR_NOOP("psi в секунду"));
    add(MeasUnit::KSI_per_Sec,
        ksi / s,
        UnitCategory::PressureRate,
        QT_TR_NOOP("ksi/с"),
        QT_TR_NOOP("ksi в секунду"));

    // --- СКОРОСТЬ РОСТА СИЛЫ (FORCE RATE) ---
    // кН/с, Н/с, кгс/с, фунт-сила/с
    add(MeasUnit::Newton_per_Sec,
        N / s,
        UnitCategory::ForceRate,
        QT_TR_NOOP("Н/с"),
        QT_TR_NOOP("Ньютон в секунду"));
    add(MeasUnit::KiloNewton_per_Sec,
        kN / s,
        UnitCategory::ForceRate,
        QT_TR_NOOP("кН/с"),
        QT_TR_NOOP("Килоньютон в секунду"));
    add(MeasUnit::KilogramForce_per_Sec,
        kgf / s,
        UnitCategory::ForceRate,
        QT_TR_NOOP("кгс/с"),
        QT_TR_NOOP("кгс в секунду"));
    add(MeasUnit::PoundForce_per_Sec,
        lbf / s,
        UnitCategory::ForceRate,
        QT_TR_NOOP("фунт-сила/с"),
        QT_TR_NOOP("Фунт-сила в секунду"));
}

// Внутренний хелпер доступа к реестру
const UnitEntry &getEntry(MeasUnit u)
{
    static std::once_flag flag;
    std::call_once(flag, initRegistry);
    if (registry.contains(u))
        return registry[u];
    static UnitEntry unknown{UnitDef(), UnitCategory::Unknown, "?", "?"};
    return unknown;
}

// =========================================================================
// Реализация функций API
// =========================================================================

bool isCompatible(MeasUnit u1, MeasUnit u2)
{
    return getEntry(u1).def.dimensions() == getEntry(u2).def.dimensions();
}

double convert(double val, MeasUnit from, MeasUnit to)
{
    if (from == to)
        return val;
    Converter conv(from, to);
    if (!conv.isValid())
        return std::numeric_limits<double>::quiet_NaN();
    return conv.process(val);
}

UnitCategory category(MeasUnit u)
{
    return getEntry(u).category;
}

QString categoryName(UnitCategory type)
{
    const char *key = nullptr;
    switch (type) {
    case UnitCategory::Dimensionless:
        key = QT_TR_NOOP("Безразмерная");
        break;
    case UnitCategory::Length:
        key = QT_TR_NOOP("Длина");
        break;
    case UnitCategory::Time:
        key = QT_TR_NOOP("Время");
        break;
    case UnitCategory::Temperature:
        key = QT_TR_NOOP("Температура");
        break;
    case UnitCategory::Mass:
        key = QT_TR_NOOP("Масса");
        break;
    case UnitCategory::Force:
        key = QT_TR_NOOP("Сила");
        break;
    case UnitCategory::Pressure:
        key = QT_TR_NOOP("Давление");
        break;
    case UnitCategory::Voltage:
        key = QT_TR_NOOP("Напряжение");
        break;
    case UnitCategory::Current:
        key = QT_TR_NOOP("Сила тока");
        break;
    case UnitCategory::Velocity:
        key = QT_TR_NOOP("Скорость");
        break;
    case UnitCategory::PressureRate:
        key = QT_TR_NOOP("Скорость нагружения (Давление)");
        break;
    case UnitCategory::ForceRate:
        key = QT_TR_NOOP("Скорость нагружения (Сила)");
        break;
    default:
        key = QT_TR_NOOP("Неизвестно");
        break;
    }
    return QCoreApplication::translate("EvoUnit::TranslationContext", key);
}

QList<MeasUnit> unitsByType(UnitCategory type)
{
    static std::once_flag flag;
    std::call_once(flag, initRegistry);
    QList<MeasUnit> result;
    auto i = registry.constBegin();
    while (i != registry.constEnd()) {
        if (i.value().category == type)
            result.append(i.key());
        ++i;
    }
    return result;
}

QString symbol(MeasUnit u)
{
    return QCoreApplication::translate("EvoUnit::TranslationContext", getEntry(u).symKey);
}

QString name(MeasUnit u)
{
    return QCoreApplication::translate("EvoUnit::TranslationContext", getEntry(u).nameKey);
}

QString format(double value, MeasUnit u, int precision)
{
    return QLocale::system().toString(value, 'f', precision) + " " + symbol(u);
}

ParsedValue parse(const QString &input)
{
    static QRegularExpression re("^([-+]?[0-9]*\\.?[0-9]+)\\s*(.*)$");
    QRegularExpressionMatch match = re.match(input.trimmed().replace(',', '.'));

    if (!match.hasMatch())
        return {0.0, MeasUnit::Unknown, false};

    double val = match.captured(1).toDouble();
    QString sym = match.captured(2).trimmed();

    static std::once_flag flag;
    std::call_once(flag, initRegistry);

    for (auto it = registry.constBegin(); it != registry.constEnd(); ++it) {
        if (symbol(it.key()).compare(sym, Qt::CaseInsensitive) == 0) {
            return {val, it.key(), true};
        }
    }
    return {val, MeasUnit::Unknown, false};
}

QPair<double, MeasUnit> autoScale(double value, MeasUnit currentUnit)
{
    UnitCategory cat = category(currentUnit);
    if (cat == UnitCategory::Unknown || cat == UnitCategory::Temperature)
        return {value, currentUnit};

    const QList<MeasUnit> candidates = unitsByType(cat);

    MeasUnit bestUnit{currentUnit};
    double bestVal{value};
    double bestScore{std::numeric_limits<double>::max()};

    for (const auto &u : candidates) {
        double v = convert(value, currentUnit, u);
        double absV = std::abs(v);
        if (absV >= 1.0 && absV < 1000.0)
            return {v, u};

        if (absV >= 1.0 && absV < bestScore) {
            bestScore = absV;
            bestVal = v;
            bestUnit = u;
        }
    }
    return {bestVal, bestUnit};
}

// =========================================================================
// Реализация Converter
// =========================================================================
Converter::Converter(MeasUnit from, MeasUnit to)
    : m_valid{false}
{
    if (!isCompatible(from, to)) {
        m_slope = 1.0;
        m_offset = 0.0;
        return;
    }
    const auto &dFrom = getEntry(from).def;
    const auto &dTo = getEntry(to).def;

    double ratio = dFrom.factor() / dTo.factor();
    m_slope = ratio;
    // Корректная логика offset:
    m_offset = (dFrom.offset() * ratio) - dTo.offset();
    m_valid = true;
}

QDebug operator<<(QDebug dbg, MeasUnit u)
{
    QDebugStateSaver saver(dbg);
    dbg.nospace() << "MeasUnit(" << EvoUnit::symbol(u) << ")";
    return dbg;
}

// =========================================================================
// Реализация JsGateway
// =========================================================================
JsGateway::JsGateway(QObject *parent)
    : QObject(parent)
{}

double JsGateway::convert(double val, EvoUnit::MeasUnit from, EvoUnit::MeasUnit to)
{
    return EvoUnit::convert(val, from, to);
}

QString JsGateway::format(double val, EvoUnit::MeasUnit u, int precision)
{
    return EvoUnit::format(val, u, precision);
}

QString JsGateway::symbol(EvoUnit::MeasUnit u)
{
    return EvoUnit::symbol(u);
}

QString JsGateway::name(EvoUnit::MeasUnit u)
{
    return EvoUnit::name(u);
}

EvoUnit::UnitCategory JsGateway::category(EvoUnit::MeasUnit u)
{
    return EvoUnit::category(u);
}

QString JsGateway::categoryName(EvoUnit::UnitCategory type)
{
    return EvoUnit::categoryName(type);
}

QVariantList JsGateway::unitsByType(EvoUnit::UnitCategory type)
{
    QVariantList list;
    const auto units = EvoUnit::unitsByType(type);
    for (const auto &u : units) {
        list.append(QVariant::fromValue(u));
    }
    return list;
}

QVariantMap JsGateway::parse(const QString &input)
{
    auto p = EvoUnit::parse(input);
    QVariantMap map;
    map["value"] = p.value;
    map["unit"] = QVariant::fromValue(p.unit);
    map["valid"] = p.valid;
    return map;
}

} // namespace EvoUnit
