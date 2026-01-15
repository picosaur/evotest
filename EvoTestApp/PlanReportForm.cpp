#include "PlanReportForm.h"
#include "ui_PlanReportForm.h"

PlanReportForm::PlanReportForm(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::PlanReportForm)
{
    ui->setupUi(this);
}

PlanReportForm::~PlanReportForm()
{
    delete ui;
}
