#pragma once

#include <QWidget>

namespace Ui {
class PlanCurveForm;
}

class PlanCurveForm : public QWidget
{
    Q_OBJECT

public:
    explicit PlanCurveForm(QWidget *parent = nullptr);
    ~PlanCurveForm();

private:
    Ui::PlanCurveForm *ui;
};
