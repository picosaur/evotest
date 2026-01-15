#include "TopIndicatorWidget.h"
#include <QVBoxLayout>
#include <QLabel>
#include <QVBoxLayout>
#include <QPushButton>

namespace EvoTest {

TopIndicatorWidget::TopIndicatorWidget(const QString &title, const QString &unit, int precision, QWidget *parent)
    : QWidget(parent)
    , m_precision(precision) // Инициализируем переданным значением
    , m_currentValue(0.0)
{
    // --- ГЛАВНЫЙ ЛЕЙАУТ ---
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // 1. ШАПКА
    m_headerLabel = new QLabel(title);
    m_headerLabel->setAlignment(Qt::AlignCenter);
    m_headerLabel->setFixedHeight(30);
    m_headerLabel->setStyleSheet(
        "background-color: #eaeaea;"
        "color: #000000;"
        "font-family: Arial;"
        "font-size: 14px;"
        "border: 1px solid #bfbfbf;"
        "border-bottom: none;"
    );
    mainLayout->addWidget(m_headerLabel);

    // 2. ТЕЛО
    QWidget *bodyWidget = new QWidget();
    QHBoxLayout *bodyLayout = new QHBoxLayout(bodyWidget);
    bodyLayout->setContentsMargins(0, 0, 0, 0);
    bodyLayout->setSpacing(0);

        // 2.1. ИНДИКАТОР
        m_indicator = new QLabel();
        // Текст установится в конце конструктора вызовом updateDisplay()

        m_indicator->setAlignment(Qt::AlignCenter);
        m_indicator->setStyleSheet(
            "QLabel {"
            "   background-color: #2b2b2b;"
            "   color: #ffaa00;"
            "   font-family: Arial, sans-serif;"
            "   font-size: 36px;"
            "   font-weight: bold;"
            "   border-left: 1px solid #bfbfbf;"
            "   border-bottom: 1px solid #bfbfbf;"
            "}"
        );
        bodyLayout->addWidget(m_indicator, 1);

        // 2.2. БОКОВАЯ ПАНЕЛЬ
        QWidget *rightPanel = new QWidget();
        rightPanel->setFixedWidth(60);
        rightPanel->setStyleSheet(
             "background-color: white;"
             "border-right: 1px solid #bfbfbf;"
             "border-bottom: 1px solid #bfbfbf;"
        );

        QVBoxLayout *rightLayout = new QVBoxLayout(rightPanel);
        rightLayout->setContentsMargins(0, 5, 0, 5);
        rightLayout->setSpacing(0);

            m_resetButton = new QPushButton("Сброс");
            m_resetButton->setCursor(Qt::PointingHandCursor);
            m_resetButton->setFlat(true);
            m_resetButton->setStyleSheet(
                // Обычное состояние
                "QPushButton {"
                "   border: 1px solid transparent;" /* ВАЖНО: Рамка есть, но прозрачная (чтобы текст не скакал) */
                "   background-color: transparent;" /* Фон прозрачный */
                "   font-size: 12px;"
                "   border-radius: 0px;"            /* Немного скруглим углы */
                "   padding: 2px;"
                "}"

                // Состояние при наведении курсора
                "QPushButton:hover {"
                "   border: 1px solid #888888;"     /* Рамка становится серой */
                "   background-color: #f0f0f0;"     /* Фон становится светло-серым (для подсветки) */
                "}"

                // Состояние при нажатии
                "QPushButton:pressed {"
                "   background-color: #cccccc;"     /* Темнее при клике */
                "}"
            );
            connect(m_resetButton, &QPushButton::clicked, this, &TopIndicatorWidget::resetClicked);

            rightLayout->addWidget(m_resetButton);
            rightLayout->addStretch();

            m_unitLabel = new QLabel(unit);
            m_unitLabel->setAlignment(Qt::AlignCenter);
            m_unitLabel->setStyleSheet("font-weight: bold; font-size: 16px; color: #000000; border: none;");
            rightLayout->addWidget(m_unitLabel);

        bodyLayout->addWidget(rightPanel, 0);

    mainLayout->addWidget(bodyWidget, 1);

    // Отрисовываем "0.0" или "0.000" в зависимости от переданного precision
    updateDisplay();
}

TopIndicatorWidget::~TopIndicatorWidget() {}

void TopIndicatorWidget::setValue(double value) {
    m_currentValue = value;
    updateDisplay();
}

void TopIndicatorWidget::setPrecision(int precision) {
    if (precision < 0) return;
    m_precision = precision;
    updateDisplay(); // Сразу перерисовываем с новой точностью
}

void TopIndicatorWidget::setText(const QString &text) {
    m_indicator->setText(text);
}

void TopIndicatorWidget::setTitle(const QString &title) {
    m_headerLabel->setText(title);
}

void TopIndicatorWidget::updateDisplay() {
    m_indicator->setText(QString::number(m_currentValue, 'f', m_precision));
}

} // namespace EvoTest
