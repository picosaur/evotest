#pragma once

#include <QWidget>

namespace Ui {
class Iso6892Form;
}

class Iso6892Form : public QWidget
{
    Q_OBJECT

public:
    explicit Iso6892Form(QWidget *parent = nullptr);
    ~Iso6892Form();

private:
    Ui::Iso6892Form *ui;
};
