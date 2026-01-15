#pragma once

#include <QWidget>

namespace Ui {
class PlanDisplayForm;
}

class PlanDisplayForm : public QWidget
{
    Q_OBJECT

public:
    explicit PlanDisplayForm(QWidget *parent = nullptr);
    ~PlanDisplayForm();

private:
    Ui::PlanDisplayForm *ui;
};
