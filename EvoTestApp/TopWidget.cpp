#include "TopWidget.h"
#include "ui_TopWidget.h"

TopWidget::TopWidget(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::TopWidget)
{
    ui->setupUi(this);
}

TopWidget::~TopWidget()
{
    delete ui;
}
