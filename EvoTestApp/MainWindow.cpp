#include "MainWindow.h"
#include "./ui_MainWindow.h"
#include <QLayout>
#include <QTabBar>
#include <QLabel>

namespace EvoTest {

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
}

MainWindow::~MainWindow()
{
    delete ui;
}

} // namespace EvoTest
