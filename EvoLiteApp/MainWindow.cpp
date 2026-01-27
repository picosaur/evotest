#include "MainWindow.h"
#include "./ui_MainWindow.h"
#include "Iso6892Form.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    auto w = new Iso6892Form();
    ui->stackedWidget->addWidget(w);
    ui->stackedWidget->setCurrentWidget(w);
}

MainWindow::~MainWindow()
{
    delete ui;
}
