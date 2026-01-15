#pragma once

#include <QWidget>

namespace Ui {
class PlanReportForm;
}

class PlanReportForm : public QWidget
{
    Q_OBJECT

public:
    explicit PlanReportForm(QWidget *parent = nullptr);
    ~PlanReportForm();

private:
    Ui::PlanReportForm *ui;
};
