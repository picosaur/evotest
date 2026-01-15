#include "PlanLeftWidget.h"
#include "ui_PlanLeftWidget.h"

PlanLeftWidget::PlanLeftWidget(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::PlanLeftWidget)
{
    ui->setupUi(this);
}

PlanLeftWidget::~PlanLeftWidget()
{
    delete ui;
}
