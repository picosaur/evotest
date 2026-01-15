#include "PlanSensorForm.h"
#include "ui_PlanSensorForm.h"

PlanSensorForm::PlanSensorForm(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::PlanSensorForm)
{
    ui->setupUi(this);
}

PlanSensorForm::~PlanSensorForm()
{
    delete ui;
}
