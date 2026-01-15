#include "RealDisplayWidget.h"
#include "ui_RealDisplayWidget.h"

RealDisplayWidget::RealDisplayWidget(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::RealDisplayWidget)
{
    ui->setupUi(this);
}

RealDisplayWidget::~RealDisplayWidget()
{
    delete ui;
}
