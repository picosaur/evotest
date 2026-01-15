#pragma once

#include <QWidget>

namespace Ui {
class PlanMenuWidget;
}

class PlanMenuWidget : public QWidget
{
    Q_OBJECT

public:
    explicit PlanMenuWidget(QWidget *parent = nullptr);
    ~PlanMenuWidget();

private:
    Ui::PlanMenuWidget *ui;
};
