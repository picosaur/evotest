#pragma once

#include <QWidget>

namespace Ui {
class PlanSensorForm;
}

class PlanSensorForm : public QWidget
{
    Q_OBJECT

public:
    explicit PlanSensorForm(QWidget *parent = nullptr);
    ~PlanSensorForm();

private:
    Ui::PlanSensorForm *ui;
};
