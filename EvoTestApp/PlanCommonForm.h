#pragma once

#include <QWidget>

namespace Ui {
class PlanCommonForm;
}

class PlanCommonForm : public QWidget
{
    Q_OBJECT

public:
    explicit PlanCommonForm(QWidget *parent = nullptr);
    ~PlanCommonForm();

private:
    Ui::PlanCommonForm *ui;
};
