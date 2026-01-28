#include "EmulIso6892.h"
#include <QRandomGenerator>
#include <QtMath>

EmulIso6892::EmulIso6892()
    : currentPos(0)
    , startPos(0)
    , currentLoad(0)
    , phase(IDLE)
{
    // --- ПАРАМЕТРЫ РЕАЛЬНОЙ СТАЛИ (Образец d=10mm, L0=50mm) ---

    // Модуль E ~200 ГПа.
    // При такой жесткости упругая зона очень короткая по длине!
    k_elastic = 30000.0f; // Было 400. Теперь 30 000 кг/мм (очень жестко)

    // Предел текучести (ReH) ~ 350 МПа -> ~2800 кг
    yield_load = 2800.0f;

    // Предел прочности (Rm) ~ 500 МПа -> ~4000 кг
    max_load = 4000.0f;

    // Удлинение при разрыве ~25% (12.5 мм)
    break_elongation = 12.5f;
}

void EmulIso6892::reset(float startPosition)
{
    startPos = startPosition;
    currentPos = startPosition;
    currentLoad = 0;
    phase = ELASTIC;
}

void EmulIso6892::update(float dt, float speedMmMin, bool isPulling)
{
    if (!isPulling)
        return;

    if (phase == FRACTURE) {
        currentLoad = 0;
        return;
    }

    // Движение
    float dx = (speedMmMin / 60.0f) * dt;
    currentPos += dx;

    float elongation = currentPos - startPos;
    if (elongation < 0)
        elongation = 0;

    // Расчет фаз
    float elastic_limit_mm = yield_load / k_elastic;

    if (elongation < elastic_limit_mm) {
        phase = ELASTIC;
        currentLoad = elongation * k_elastic;
    } else if (elongation < elastic_limit_mm + 1.5f) {
        phase = YIELDING;
        // Шум на полке текучести
        float noise = (QRandomGenerator::global()->generateDouble() - 0.5) * 15.0f;
        currentLoad = yield_load + noise;
    } else if (elongation < break_elongation - 2.0f) {
        phase = PLASTIC;
        float plastic_start = elastic_limit_mm + 1.5f;
        float plastic_end = break_elongation - 2.0f;
        // Нормализация от 0 до 1 внутри зоны
        float x_norm = (elongation - plastic_start) / (plastic_end - plastic_start);
        if (x_norm > 1.0f)
            x_norm = 1.0f;

        // Кривая упрочнения (синус)
        float curve = qSin(x_norm * 1.57); // до пика
        currentLoad = yield_load + (max_load - yield_load) * curve;
    } else if (elongation < break_elongation) {
        phase = NECKING;
        float neck_start = break_elongation - 2.0f;
        float x_norm = (elongation - neck_start) / 2.0f;
        // Падение нагрузки
        currentLoad = max_load - (max_load * 0.4f * x_norm);
    } else {
        phase = FRACTURE;
        currentLoad = 0;
    }

    // Общий шум датчика
    if (phase != FRACTURE) {
        currentLoad += (QRandomGenerator::global()->generateDouble() - 0.5) * 0.5f;
    }
}

QString EmulIso6892::getCurrentPhaseName() const
{
    switch (phase) {
    case IDLE:
        return "Ожидание";
    case ELASTIC:
        return "Упругая деформация";
    case YIELDING:
        return "Площадка текучести";
    case PLASTIC:
        return "Упрочнение";
    case NECKING:
        return "Шейка";
    case FRACTURE:
        return "РАЗРЫВ";
    default:
        return "";
    }
}
