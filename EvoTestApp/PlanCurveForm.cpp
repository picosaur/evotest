#include "PlanCurveForm.h"
#include "ui_PlanCurveForm.h"

PlanCurveForm::PlanCurveForm(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::PlanCurveForm)
{
    ui->setupUi(this);
}

PlanCurveForm::~PlanCurveForm()
{
    delete ui;
}
