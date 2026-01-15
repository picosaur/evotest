#pragma once
#include <QWidget>

class QLabel;
class QPushButton;

namespace EvoTest {

class TopIndicatorWidget : public QWidget
{
    Q_OBJECT

public:
    explicit TopIndicatorWidget(const QString &title, const QString &unit,  int precision = 1, QWidget *parent = nullptr);
    ~TopIndicatorWidget();

    // Основной метод установки значения. Теперь он берет точность из настроек класса.
    void setValue(double value);

    // Новая настройка: установить количество знаков после запятой
    void setPrecision(int precision);

    // Установить произвольный текст (например "Error")
    void setText(const QString &text);

    void setTitle(const QString &title);

signals:
    void resetClicked();

private:
    // Вспомогательный метод для обновления текста
    void updateDisplay();

    QLabel *m_headerLabel;
    QLabel *m_indicator;
    QLabel *m_unitLabel;
    QPushButton *m_resetButton;

    int m_precision;       // Храним настройку точности
    double m_currentValue; // Храним текущее число
};

} // namespace EvoTest
