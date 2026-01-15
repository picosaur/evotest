#pragma once

#include <QWidget>

namespace Ui {
class RealDisplayWidget;
}

class RealDisplayWidget : public QWidget
{
    Q_OBJECT

public:
    explicit RealDisplayWidget(QWidget *parent = nullptr);
    ~RealDisplayWidget();

private:
    Ui::RealDisplayWidget *ui;
};
