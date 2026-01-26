#pragma once

#include <QAbstractTableModel>
#include <QStyledItemDelegate>
#include <QTableView>
#include <QWidget>
#include "EvoModbus.h"

namespace EvoGui {

class FormulaTableModel : public QAbstractTableModel
{
    Q_OBJECT
public:
    enum Columns { Col_ID = 0, Col_Formula, Col_Category, Col_Unit, Col_Count };
    explicit FormulaTableModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &) const override;
    int columnCount(const QModelIndex &) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    bool setData(const QModelIndex &index, const QVariant &value, int role) override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;

    void addChannel(const EvoModbus::ComputedChannel &ch);
    void removeChannel(int row);
    void setChannels(const QVector<EvoModbus::ComputedChannel> &ch);
    QVector<EvoModbus::ComputedChannel> getChannels() const;

private:
    QVector<EvoModbus::ComputedChannel> m_data;
};

class FormulaDelegate : public QStyledItemDelegate
{
    Q_OBJECT
public:
    explicit FormulaDelegate(QObject *parent = nullptr);
    QWidget *createEditor(QWidget *parent,
                          const QStyleOptionViewItem &option,
                          const QModelIndex &index) const override;
    void setEditorData(QWidget *editor, const QModelIndex &index) const override;
    void setModelData(QWidget *editor,
                      QAbstractItemModel *model,
                      const QModelIndex &index) const override;
};

class FormulaConfigWidget : public QWidget
{
    Q_OBJECT
public:
    explicit FormulaConfigWidget(EvoModbus::Controller *controller, QWidget *parent = nullptr);
    void load();
    void apply();

private slots:
    void onAddClicked();
    void onRemoveClicked();
    void onApplyClicked();

private:
    EvoModbus::Controller *m_controller{nullptr};
    FormulaTableModel *m_model{nullptr};
    QTableView *m_view{nullptr};
};

} // namespace EvoGui
