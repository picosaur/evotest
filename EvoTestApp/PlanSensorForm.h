#pragma once

#include <QWidget>

namespace Ui {
class PlanSensorForm;
}

class MeasPlanSensorForm : public QWidget
{
    Q_OBJECT

public:
    explicit MeasPlanSensorForm(QWidget *parent = nullptr);
    ~MeasPlanSensorForm();

private:
    Ui::PlanSensorForm *ui;
};
