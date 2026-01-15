#pragma once

#include <QWidget>

namespace Ui {
class PlanControlModeForm;
}

class PlanControlModeForm : public QWidget
{
    Q_OBJECT

public:
    explicit PlanControlModeForm(QWidget *parent = nullptr);
    ~PlanControlModeForm();

private:
    Ui::PlanControlModeForm *ui;
};
