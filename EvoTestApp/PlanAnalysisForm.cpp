#include "PlanAnalysisForm.h"
#include "ui_PlanAnalysisForm.h"

PlanAnalysisForm::PlanAnalysisForm(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::PlanAnalysisForm)
{
    ui->setupUi(this);
}

PlanAnalysisForm::~PlanAnalysisForm()
{
    delete ui;
}
