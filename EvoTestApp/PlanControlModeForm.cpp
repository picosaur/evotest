#include "PlanControlModeForm.h"
#include "ui_PlanControlModeForm.h"

PlanControlModeForm::PlanControlModeForm(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::PlanControlModeForm)
{
    ui->setupUi(this);
}

PlanControlModeForm::~PlanControlModeForm()
{
    delete ui;
}
