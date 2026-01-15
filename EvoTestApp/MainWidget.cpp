#include "MainWidget.h"
#include <QTabBar>
#include "ui_MainWidget.h"

MainWidget::MainWidget(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::MainWidget)
{
    ui->setupUi(this);
    ui->mainTabWidget->tabBar()->setExpanding(true);
    ui->mainTabWidget->setStyleSheet(
        "#mainTabWidget > QTabBar::tab { height: 50px; width: 100px; }");
}

MainWidget::~MainWidget()
{
    delete ui;
}
