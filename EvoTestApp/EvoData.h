#pragma once
#include <QModbusDataUnit>
#include <QString>
#include <QVector>

/*

    Адрес 7, 8, 9 (INT16): Настройки энкодера и ШВП. Это "передаточные числа". Если их сбить, машина будет показывать неправильное расстояние и скорость.
    Адрес 12 (INT16): Чувствительность датчика. Это калибровка тензодатчика.
    Адрес 13 (Float): Макс погрешность (PID tuning).
*/

namespace EvoData {

enum class ChannelType {
    Modbus,
    Script,
};

//
// ============================================================================
class ChannelInfo
{
public:
    QString id{};   // ID канала (0 - Нагрузка, 1 - Перемещение и т.д.)
    QString name{}; // Имя ("Нагрузка", "Время", "Деформация")

    ChannelType channelType{};
};

//
// ============================================================================
class MachineConfig
{
public:
    QVector<ChannelInfo> availableChannels;

    // Метод для инициализации дефолтных значений (или загрузки из файла)
    static MachineConfig loadDefault()
    {
        MachineConfig cfg;
        // Симулируем наличие двух датчиков
        //cfg.availableSensors.append({"s_10kn", "Датчик силы 10 кН"});
        //cfg.availableSensors.append({"s_50kn", "Датчик силы 50 кН"});

        // Каналы отображения
        //cfg.availableChannels.append(ChannelInfo);
        //cfg.availableChannels.append({1, "Пик нагрузки", "кН"});
        //cfg.availableChannels.append({2, "Смещение", "мм"});
        return cfg;
    }
};

} // namespace EvoData
