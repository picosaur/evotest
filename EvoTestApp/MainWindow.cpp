#include "MainWindow.h"
#include "TopIndicatorWidget.h"
#include "./ui_MainWindow.h"
#include <QLayout>
#include <QTabBar>
#include <QLabel>


MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    auto centralWidget = new QWidget();
    auto centralLayout = new QVBoxLayout();
    auto topWidget = new QWidget();
    auto topLayout = new QHBoxLayout();
    auto tabBar = new QTabBar();

    topLayout->addWidget(new TopIndicatorWidget(tr("Нагрузка [10000Н]"), "Н", 1));
    topLayout->addWidget(new TopIndicatorWidget(tr("Деформация [Длинноходовой экстензометр]"), "мм", 4));
    topLayout->addWidget(new TopIndicatorWidget("Время испытания", "с",3));
    topLayout->addWidget(new TopIndicatorWidget("Смещение", "мм", 2));

    topWidget->setLayout(topLayout);
    topWidget->setFixedHeight(100);

    tabBar->addTab(tr("Меню"));
    tabBar->addTab(tr("План испытанний"));
    tabBar->addTab(tr("Испытание"));

    centralLayout->addWidget(topWidget);
    centralLayout->addWidget(tabBar);
    centralWidget->setLayout(centralLayout);
    centralLayout->addStretch(1);

    setCentralWidget(centralWidget);
}

MainWindow::~MainWindow()
{
    delete ui;
}



