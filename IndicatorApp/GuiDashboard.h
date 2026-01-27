#pragma once

#include <QAbstractTableModel>
#include <QStyledItemDelegate>
#include <QTableView>
#include <QWidget>
#include "EvoModbus.h"

namespace EvoGui {

// Структура строки монитора
struct MonitorRow
{
    QString id;
    // Единица, в которой пользователь ХОЧЕТ видеть значение.
    // Если Unknown, отображается в "родной" единице канала.
    EvoUnit::MeasUnit displayUnit{EvoUnit::MeasUnit::Unknown};
};

class MonitorTableModel : public QAbstractTableModel
{
    Q_OBJECT
public:
    enum Columns {
        Col_ID = 0,
        Col_DisplayUnit,
        Col_Value,
        Col_NativeUnit, // Доп. колонка для инфо
        Col_Count
    };

    explicit MonitorTableModel(EvoModbus::Controller *controller, QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override;
    QVariant headerData(int section,
                        Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;

public slots:
    void onDataUpdated();

private:
    EvoModbus::Controller *m_controller;
    QVector<MonitorRow> m_rows;

    // Синхронизация списка строк с контроллером
    void syncRows();
};

class MonitorDelegate : public QStyledItemDelegate
{
    Q_OBJECT
public:
    explicit MonitorDelegate(EvoModbus::Controller *c, QObject *parent = nullptr);

    QWidget *createEditor(QWidget *parent,
                          const QStyleOptionViewItem &option,
                          const QModelIndex &index) const override;
    void setEditorData(QWidget *editor, const QModelIndex &index) const override;
    void setModelData(QWidget *editor,
                      QAbstractItemModel *model,
                      const QModelIndex &index) const override;

private:
    EvoModbus::Controller *m_controller;
};

class DashboardWidget : public QWidget
{
    Q_OBJECT
public:
    explicit DashboardWidget(EvoModbus::Controller *c, QWidget *parent = nullptr);

private:
    MonitorTableModel *m_model{nullptr};
    QTableView *m_view{nullptr};
};

} // namespace EvoGui
