#pragma once

#include <QWidget>

namespace Ui {
class PlanSetupWidget;
}

class PlanSetupWidget : public QWidget
{
    Q_OBJECT

public:
    explicit PlanSetupWidget(QWidget *parent = nullptr);
    ~PlanSetupWidget();

private:
    Ui::PlanSetupWidget *ui;
};
