#include "Iso6892Form.h"
#include "ui_Iso6892Form.h"

Iso6892Form::Iso6892Form(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::Iso6892Form)
{
    ui->setupUi(this);
}

Iso6892Form::~Iso6892Form()
{
    delete ui;
}
