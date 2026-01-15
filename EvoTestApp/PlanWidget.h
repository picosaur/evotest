#pragma once

#include <QWidget>

namespace Ui {
class PlanWidget;
}

class PlanWidget : public QWidget
{
    Q_OBJECT

public:
    explicit PlanWidget(QWidget *parent = nullptr);
    ~PlanWidget();

private:
    Ui::PlanWidget *ui;
};
