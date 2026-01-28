#ifndef EMUL_ISO6892_H
#define EMUL_ISO6892_H

#include <QString>

class EmulIso6892
{
public:
    EmulIso6892();

    void reset(float startPosition);
    void update(float dt, float speedMmMin, bool isPulling);

    float getPosition() const { return currentPos; }
    float getLoad() const { return currentLoad; }
    QString getCurrentPhaseName() const;

private:
    float currentPos;
    float startPos;
    float currentLoad;

    // Параметры материала
    float k_elastic;        // Жесткость
    float yield_load;       // Предел текучести
    float max_load;         // Предел прочности
    float break_elongation; // Удлинение при разрыве

    enum Phase {
        IDLE,
        ELASTIC,  // Упругость
        YIELDING, // Текучесть (полка)
        PLASTIC,  // Упрочнение
        NECKING,  // Шейка
        FRACTURE  // Разрыв
    } phase;
};

#endif // EMUL_ISO6892_H
