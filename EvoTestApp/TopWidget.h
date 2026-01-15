#pragma once

#include <QWidget>

namespace Ui {
class TopWidget;
}

class TopWidget : public QWidget
{
    Q_OBJECT

public:
    explicit TopWidget(QWidget *parent = nullptr);
    ~TopWidget();

private:
    Ui::TopWidget *ui;
};
