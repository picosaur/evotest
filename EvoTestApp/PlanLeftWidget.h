#pragma once

#include <QWidget>

namespace Ui {
class PlanLeftWidget;
}

class PlanLeftWidget : public QWidget
{
    Q_OBJECT

public:
    explicit PlanLeftWidget(QWidget *parent = nullptr);
    ~PlanLeftWidget();

private:
    Ui::PlanLeftWidget *ui;
};
