#include "PlanWidget.h"
#include "ui_PlanWidget.h"

PlanWidget::PlanWidget(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::PlanWidget)
{
    ui->setupUi(this);
}

PlanWidget::~PlanWidget()
{
    delete ui;
}
