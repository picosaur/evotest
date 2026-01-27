#pragma once

#include <QAbstractTableModel>
#include <QComboBox>
#include <QDialog>
#include <QLineEdit>
#include <QPushButton>
#include <QStyledItemDelegate>
#include <QTableView>
#include <QTextEdit>
#include <QWidget>
#include "EvoModbus.h"
#include "GuiSourceConfig.h" // Для SourceEditMode

namespace EvoGui {

// Диалог редактирования скрипта
class ScriptEditDialog : public QDialog
{
    Q_OBJECT
public:
    explicit ScriptEditDialog(const QString &currentScript, QWidget *parent = nullptr);
    QString getScript() const;

private:
    QTextEdit *m_editor;
};

// Диалог добавления формулы
class FormulaAddDialog : public QDialog
{
    Q_OBJECT
public:
    explicit FormulaAddDialog(QWidget *parent = nullptr);
    EvoModbus::ComputedChannel getChannel() const;
    void setChannel(const EvoModbus::ComputedChannel &ch);
    void setIdEditable(bool editable);

private slots:
    void onEditScript();
    void onCategoryChanged(int index);
    void validateAndAccept();

private:
    QLineEdit *edtId;
    QLineEdit *edtFormula;
    QComboBox *cbCategory;
    QComboBox *cbUnit;
    QPushButton *btnScript;
};

// Модель таблицы формул
class FormulaTableModel : public QAbstractTableModel
{
    Q_OBJECT
public:
    enum Columns {
        Col_Action = 0,
        Col_ID,
        Col_Formula, // Текст формулы + кнопка (в Inline)
        Col_Category,
        Col_Unit,
        Col_Remove,
        Col_Count
    };

    struct RowData
    {
        bool enabled{true};
        EvoModbus::ComputedChannel channel;
    };

    explicit FormulaTableModel(SourceEditMode mode,
                               EvoModbus::Controller *controller,
                               QObject *parent = nullptr);

    int rowCount(const QModelIndex &) const override;
    int columnCount(const QModelIndex &) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    bool setData(const QModelIndex &index, const QVariant &value, int role) override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;

    bool addChannel(const EvoModbus::ComputedChannel &ch, QString &err);
    void updateChannel(int row, const EvoModbus::ComputedChannel &ch);
    void removeChannel(int row);
    void setChannels(const QVector<EvoModbus::ComputedChannel> &ch);

    QVector<EvoModbus::ComputedChannel> getChannels() const;
    EvoModbus::ComputedChannel getChannelAt(int row) const;

    bool isRowEnabled(int row) const;
    bool isIdUnique(const QString &id, int ignoreRow = -1) const;

private:
    SourceEditMode m_mode;
    EvoModbus::Controller *m_controller;
    QVector<RowData> m_data;
};

// Делегат
class FormulaDelegate : public QStyledItemDelegate
{
    Q_OBJECT
public:
    explicit FormulaDelegate(SourceEditMode mode, QObject *parent = nullptr);
    QWidget *createEditor(QWidget *parent,
                          const QStyleOptionViewItem &option,
                          const QModelIndex &index) const override;
    void setEditorData(QWidget *editor, const QModelIndex &index) const override;
    void setModelData(QWidget *editor,
                      QAbstractItemModel *model,
                      const QModelIndex &index) const override;

signals:
    void editRequested(int row);
    void scriptRequested(int row);

private:
    SourceEditMode m_mode;
};

// Виджет конфигурации формул
class FormulaConfigWidget : public QWidget
{
    Q_OBJECT
public:
    explicit FormulaConfigWidget(EvoModbus::Controller *controller,
                                 SourceEditMode mode = SourceEditMode::Inline,
                                 QWidget *parent = nullptr);
    void load();
    void apply();

private slots:
    void onAddClicked();
    void onRemoveClicked();
    void onApplyClicked();
    void onEditRequested(int row);
    void onScriptRequested(int row);
    void onRowsInserted(const QModelIndex &, int first, int last);
    void onDataChanged(const QModelIndex &tl, const QModelIndex &br);

private:
    EvoModbus::Controller *m_controller;
    SourceEditMode m_mode;
    FormulaTableModel *m_model;
    QTableView *m_view;

    void updateEditorsState(int row);
    void openEditorsForRange(int first, int last);
};

} // namespace EvoGui
