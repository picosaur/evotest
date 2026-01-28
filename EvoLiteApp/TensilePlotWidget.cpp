#include "TensilePlotWidget.h"

TensilePlotWidget::TensilePlotWidget(QWidget *parent)
    : QCustomPlot(parent)
    , m_S0(1.0)
    , m_L0(50.0)
{
    setupUi();
}

void TensilePlotWidget::setupUi()
{
    // 1. Создаем основной график
    m_mainCurve = this->addGraph();
    m_mainCurve->setPen(QPen(Qt::blue, 2));
    m_mainCurve->setName("Stress-Strain Curve");

    // 2. Создаем линию для Модуля упругости (Пунктирная)
    m_elasticLine = this->addGraph();
    m_elasticLine->setPen(QPen(Qt::darkGreen, 2, Qt::DashLine));
    m_elasticLine->setName("Modulus E");

    // 3. Создаем график для одной точки (Rp0.2)
    m_rp02Marker = this->addGraph();
    m_rp02Marker->setPen(QPen(Qt::red));
    m_rp02Marker->setLineStyle(QCPGraph::lsNone); // Нет линий, только точки
    m_rp02Marker->setScatterStyle(
        QCPScatterStyle(QCPScatterStyle::ssCircle, Qt::yellow, Qt::red, 8));
    m_rp02Marker->setName("Rp0.2");

    // 4. Настройка осей
    this->xAxis->setLabel("Strain [%]");
    this->yAxis->setLabel("Stress [MPa]");

    // Включаем сетку
    this->xAxis->grid()->setVisible(true);
    this->yAxis->grid()->setVisible(true);

    // Включаем возможность зума и перетаскивания (удобно для анализа)
    this->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom);
}

void TensilePlotWidget::setSpecimenParams(double areaS0, double gaugeLengthL0)
{
    m_S0 = (areaS0 > 0.0001) ? areaS0 : 1.0;
    m_L0 = (gaugeLengthL0 > 0.1) ? gaugeLengthL0 : 50.0;
}

void TensilePlotWidget::resetPlot()
{
    // Очищаем данные
    m_mainCurve->data()->clear();
    m_elasticLine->data()->clear();
    m_rp02Marker->data()->clear();

    // Сбрасываем масштаб
    this->xAxis->setRange(0, 10);  // Начальный диапазон 0-10%
    this->yAxis->setRange(0, 100); // Начальный диапазон 0-100 МПа
    this->replot();
}

void TensilePlotWidget::addLivePoint(double forceN, double extensionMm)
{
    // 1. Простая конвертация "на лету" (без коррекции нуля)
    // Это нужно, чтобы оператор видел динамику
    double stress = forceN / m_S0;
    double strain = (extensionMm / m_L0) * 100.0;

    // 2. Добавляем точку
    m_mainCurve->addData(strain, stress);

    // 3. Автомасштабирование (Rescale)
    // Чтобы график всегда влезал в окно
    m_mainCurve->rescaleAxes(true); // true = расширять границы, но не сужать

    // 4. Перерисовка
    // Внимание: при частоте 50Гц replot может грузить процессор.
    // Если тормозит, используйте replot(QCustomPlot::rpQueuedReplot);
    this->replot();
}

void TensilePlotWidget::plotFinalAnalysis(const Iso6892Results &results,
                                          const QVector<QPointF> &correctedCurve)
{
    if (!results.isValid)
        return;

    // 1. Заменяем "сырой" график на "скорректированный" (красивый, из нуля)
    m_mainCurve->data()->clear();
    for (const QPointF &pt : correctedCurve) {
        m_mainCurve->addData(pt.x(), pt.y());
    }

    // 2. Рисуем линию Модуля Упругости (Визуализация E)
    // Строим прямую y = E*x от 0 до, скажем, 0.5% деформации
    m_elasticLine->data()->clear();
    double xMaxPreview = 0.5; // Рисуем небольшой хвостик
    if (results.Rp02 > 0) {
        // Или рисуем до уровня Rp0.2 по высоте
        xMaxPreview = results.Rp02 / results.E_Modulus;
    }

    m_elasticLine->addData(0, 0);
    m_elasticLine->addData(xMaxPreview, results.E_Modulus * xMaxPreview);

    // 3. Ставим точку Rp0.2
    m_rp02Marker->data()->clear();
    if (results.Rp02 > 0) {
        // Находим X координату для точки Rp0.2 на графике
        // Так как это точка на кривой, нам нужно найти X, соответствующий Y=Rp02
        // Но проще взять формулу линии смещения: Y = E*(X - 0.2) => X = Y/E + 0.2
        double x_rp = (results.Rp02 / results.E_Modulus) + 0.2;
        m_rp02Marker->addData(x_rp, results.Rp02);
    }

    // 4. Финальное масштабирование, чтобы показать всё красиво
    this->rescaleAxes();

    // Чуть-чуть добавим отступов сверху и справа
    this->yAxis->scaleRange(1.1, this->yAxis->range().center());
    this->xAxis->scaleRange(1.1, this->xAxis->range().center());

    this->replot();
}
