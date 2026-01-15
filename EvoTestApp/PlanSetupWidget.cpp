#include "PlanSetupWidget.h"
#include "ui_PlanSetupWidget.h"

PlanSetupWidget::PlanSetupWidget(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::PlanSetupWidget)
{
    ui->setupUi(this);
}

PlanSetupWidget::~PlanSetupWidget()
{
    delete ui;
}
