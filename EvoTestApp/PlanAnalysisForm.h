#pragma once

#include <QWidget>

namespace Ui {
class PlanAnalysisForm;
}

class PlanAnalysisForm : public QWidget
{
    Q_OBJECT

public:
    explicit PlanAnalysisForm(QWidget *parent = nullptr);
    ~PlanAnalysisForm();

private:
    Ui::PlanAnalysisForm *ui;
};
