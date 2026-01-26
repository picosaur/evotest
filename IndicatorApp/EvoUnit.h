#pragma once

#include <QCoreApplication>
#include <QDebug>
#include <QList>
#include <QMetaType>
#include <QObject>
#include <QString>
#include <QVariant>
#include <cmath>

namespace EvoUnit {

// Включаем поддержку мета-системы для namespace
Q_NAMESPACE

// 1. Идентификаторы единиц
enum class MeasUnit {
    Unknown = 0,

    // --- Длина ---
    Millimeter,
    Centimeter,
    Decimeter, // дм
    Meter,
    Kilometer,
    Micrometer,

    Inch,
    Foot,
    Yard,
    Mil, // мил (1/1000 дюйма)

    // --- Время ---
    Second,
    Minute,
    Hour,
    Day,
    Millisecond,
    Microsecond,

    // --- Температура ---
    Kelvin,
    Celsius,
    Fahrenheit,

    // --- Масса ---
    Gram,
    Kilogram,
    Pound,
    Tonne,

    // --- Сила ---
    Newton,
    KiloNewton,
    MegaNewton,
    GramForce,
    KilogramForce,
    TonForce,
    PoundForce,
    Kip,

    // --- Давление ---
    Pascal,
    KiloPascal,
    MegaPascal,
    GigaPascal,
    Bar,
    MilliBar,
    PSI,
    KSI,
    Kgf_per_CM2,
    Atmosphere,

    // --- Напряжение (Voltage) ---
    Microvolt,
    Millivolt,
    Volt,
    Kilovolt,

    // --- Сила тока (Current) ---
    Microampere,
    Milliampere,
    Ampere,
    Kiloampere,

    // --- Скорость (Velocity) ---
    // .. в секунду
    MM_per_Sec,
    CM_per_Sec,
    DM_per_Sec,
    Meter_per_Sec,
    Inch_per_Sec,
    Foot_per_Sec,
    Mil_per_Sec,

    // .. в минуту
    MM_per_Min,
    CM_per_Min,
    DM_per_Min,
    Meter_per_Min,
    Inch_per_Min,
    Foot_per_Min,
    Mil_per_Min,

    KM_per_Hour,

    // --- Скорость роста давления (Pressure Rate) ---
    Pascal_per_Sec,
    KiloPascal_per_Sec,
    MegaPascal_per_Sec,
    GigaPascal_per_Sec,
    Newton_per_MM2_per_Sec,
    Kgf_per_CM2_per_Sec,
    PSI_per_Sec,
    KSI_per_Sec,

    // --- Скорость роста силы (Force Rate) ---
    Newton_per_Sec,
    KiloNewton_per_Sec,
    KilogramForce_per_Sec,
    PoundForce_per_Sec
};
Q_ENUM_NS(MeasUnit)

// 2. Категории
enum class UnitCategory {
    Unknown,
    Length,
    Time,
    Temperature,
    Mass,
    Force,
    Pressure,
    Voltage,
    Current,
    Velocity,
    PressureRate,
    ForceRate
};
Q_ENUM_NS(UnitCategory)

// 3. Размерности [L, M, T, Theta, I]
struct Dimensions
{
    int8_t L, M, T, Theta, I;
    bool operator==(const Dimensions &o) const
    {
        return L == o.L && M == o.M && T == o.T && Theta == o.Theta && I == o.I;
    }
    bool operator!=(const Dimensions &o) const { return !(*this == o); }
};

// =========================================================================
// UnitDef: Value Object (Физика)
// =========================================================================
class UnitDef
{
public:
    UnitDef(double factor = 1.0, double offset = 0.0, Dimensions dim = {0, 0, 0, 0, 0});

    UnitDef operator*(const UnitDef &other) const;
    UnitDef operator/(const UnitDef &other) const;
    UnitDef operator*(double scale) const;

    double factor() const { return m_factor; }
    double offset() const { return m_offset; }
    Dimensions dimensions() const { return m_dim; }

private:
    double m_factor{1.0};
    double m_offset{0.0};
    Dimensions m_dim{};
};

// =========================================================================
// API Пространства имен (C++ Free Functions)
// =========================================================================

double convert(double val, MeasUnit from, MeasUnit to);
UnitCategory category(MeasUnit u);
QString categoryName(UnitCategory type); // [NEW] Получить имя категории
QList<MeasUnit> unitsByType(UnitCategory type);
bool isCompatible(MeasUnit u1, MeasUnit u2);

QString symbol(MeasUnit u);
QString name(MeasUnit u);
QString format(double value, MeasUnit u, int precision = 2);

struct ParsedValue
{
    double value{0.0};
    MeasUnit unit{MeasUnit::Unknown};
    bool valid{false};
};
ParsedValue parse(const QString &input);

QPair<double, MeasUnit> autoScale(double value, MeasUnit currentUnit);

// =========================================================================
// Converter (Helper Class)
// =========================================================================
class Converter
{
public:
    Converter(MeasUnit from, MeasUnit to);

    inline double process(double value) const { return value * m_slope + m_offset; }
    bool isValid() const { return m_valid; }

private:
    double m_slope{1.0};
    double m_offset{0.0};
    bool m_valid{false};
};

QDebug operator<<(QDebug dbg, MeasUnit u);

// =========================================================================
// Класс-мост для QJSEngine
// =========================================================================
class JsGateway : public QObject
{
    Q_OBJECT
public:
    explicit JsGateway(QObject *parent = nullptr);

    Q_INVOKABLE double convert(double val, EvoUnit::MeasUnit from, EvoUnit::MeasUnit to);
    Q_INVOKABLE QString format(double val, EvoUnit::MeasUnit u, int precision = 2);
    Q_INVOKABLE QString symbol(EvoUnit::MeasUnit u);
    Q_INVOKABLE QString name(EvoUnit::MeasUnit u);

    Q_INVOKABLE EvoUnit::UnitCategory category(EvoUnit::MeasUnit u);
    Q_INVOKABLE QString categoryName(EvoUnit::UnitCategory type); // [NEW]

    Q_INVOKABLE QVariantList unitsByType(EvoUnit::UnitCategory type);
    Q_INVOKABLE QVariantMap parse(const QString &input);
};

} // namespace EvoUnit

Q_DECLARE_METATYPE(EvoUnit::MeasUnit)
