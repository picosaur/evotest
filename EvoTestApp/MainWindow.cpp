#include "MainWindow.h"
#include <QComboBox>
#include <QLabel>
#include <QLayout>
#include <QStyle>
#include <QTabBar>
#include "./ui_MainWindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    m_defaultFont = QApplication::font();

    auto scaleCombo = new QComboBox();
    scaleCombo->addItem("100% (Норма)", 100);
    scaleCombo->addItem("125%", 125);
    scaleCombo->addItem("150%", 150);
    scaleCombo->addItem("200%", 200);
    int defaultIndex = scaleCombo->findData(100);
    if (defaultIndex != -1) {
        scaleCombo->setCurrentIndex(defaultIndex);
    }

    // 4. Подключаем сигнал (используем QOverload для точности)
    connect(scaleCombo,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this,
            [this, scaleCombo](int index) {
                // Получаем значение из user data (второй параметр addItem)
                int percent = scaleCombo->itemData(index).toInt();
                // Вызываем чистый метод установки масштаба
                setScale(percent);
            });

    ui->toolBar->addWidget(new QLabel("Масштаб:"));
    ui->toolBar->addWidget(scaleCombo);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::setScale(int percent)
{
    // Берем наш эталонный шрифт
    QFont newFont = m_defaultFont;

    // Получаем исходный размер
    // Важно использовать qreal (double) для точности, но setPointSize принимает int
    int baseSize = m_defaultFont.pointSize();

    // Если вдруг pointSize вернул -1 (значит шрифт в пикселях), берем pixelSize
    if (baseSize <= 0) {
        int basePixel = m_defaultFont.pixelSize();
        int newPixel = static_cast<int>(basePixel * (percent / 100.0));
        newFont.setPixelSize(newPixel);
    } else {
        // Стандартная логика для пунктов (points)
        int newSize = static_cast<int>(baseSize * (percent / 100.0));

        // Защита от слишком мелкого шрифта (чтобы не стал 0)
        if (newSize < 1)
            newSize = 1;

        newFont.setPointSize(newSize);
    }

    // Применяем шрифт КО ВСЕМУ приложению
    qApp->setFont(newFont);

    // Принудительно обновляем стили для всех виджетов
    for (QWidget *widget : QApplication::allWidgets()) {
        widget->style()->unpolish(widget);
        widget->style()->polish(widget);
        widget->update();
    }
}
