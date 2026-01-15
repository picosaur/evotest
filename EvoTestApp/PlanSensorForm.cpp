#include "PlanSensorForm.h"
#include "ui_PlanSensorForm.h"

MeasPlanSensorForm::MeasPlanSensorForm(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::PlanSensorForm)
{
    ui->setupUi(this);
}

MeasPlanSensorForm::~MeasPlanSensorForm()
{
    delete ui;
}
