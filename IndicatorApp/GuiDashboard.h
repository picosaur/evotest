#pragma once

#include <QAbstractTableModel>
#include <QStyledItemDelegate>
#include <QTableView>
#include <QWidget>
#include "EvoModbus.h"

namespace EvoGui {

struct MonitorRow
{
    QString id;
    EvoUnit::MeasUnit displayUnit;
};

class MonitorTableModel : public QAbstractTableModel
{
    Q_OBJECT
public:
    enum Columns { Col_ID = 0, Col_DisplayUnit, Col_Value, Col_Count };
    explicit MonitorTableModel(EvoModbus::Controller *controller, QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override;
    QVariant headerData(int section,
                        Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;

    void addRow(const QString &id);
    void removeRow(int idx);

public slots:
    void onDataUpdated();

private:
    EvoModbus::Controller *m_controller;
    QVector<MonitorRow> m_rows;
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

private slots:
    void onAddClicked();    // Объявлено
    void onRemoveClicked(); // Объявлено

private:
    EvoModbus::Controller *m_controller;
    MonitorTableModel *m_model{nullptr};
    QTableView *m_view{nullptr};
};

} // namespace EvoGui
