#include "PlanStopCondForm.h"
#include "ui_PlanStopCondForm.h"

PlanStopCondForm::PlanStopCondForm(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::PlanStopCondForm)
{
    ui->setupUi(this);
}

PlanStopCondForm::~PlanStopCondForm()
{
    delete ui;
}
