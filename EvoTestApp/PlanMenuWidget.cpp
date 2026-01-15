#include "PlanMenuWidget.h"
#include "ui_PlanMenuWidget.h"

PlanMenuWidget::PlanMenuWidget(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::PlanMenuWidget)
{
    ui->setupUi(this);
}

PlanMenuWidget::~PlanMenuWidget()
{
    delete ui;
}
