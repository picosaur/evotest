#include "PlanDisplayForm.h"
#include "ui_PlanDisplayForm.h"

PlanDisplayForm::PlanDisplayForm(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::PlanDisplayForm)
{
    ui->setupUi(this);
}

PlanDisplayForm::~PlanDisplayForm()
{
    delete ui;
}
