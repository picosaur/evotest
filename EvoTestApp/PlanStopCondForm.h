#pragma once

#include <QWidget>

namespace Ui {
class PlanStopCondForm;
}

class PlanStopCondForm : public QWidget
{
    Q_OBJECT

public:
    explicit PlanStopCondForm(QWidget *parent = nullptr);
    ~PlanStopCondForm();

private:
    Ui::PlanStopCondForm *ui;
};
