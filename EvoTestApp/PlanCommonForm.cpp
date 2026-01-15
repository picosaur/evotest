#include "PlanCommonForm.h"
#include "ui_PlanCommonForm.h"

PlanCommonForm::PlanCommonForm(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::PlanCommonForm)
{
    ui->setupUi(this);
}

PlanCommonForm::~PlanCommonForm()
{
    delete ui;
}
