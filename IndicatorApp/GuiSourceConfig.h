#pragma once

#include <QAbstractTableModel>
#include <QComboBox>
#include <QDialog>
#include <QLineEdit>
#include <QSpinBox>
#include <QStyledItemDelegate>
#include <QTableView>
#include <QWidget>
#include "EvoModbus.h"

namespace EvoGui {

// Режимы работы таблицы конфигурации
enum class SourceEditMode {
    Inline, // Чекбокс + редактирование прямо в таблице
    Dialog  // Кнопка "Edit" + редактирование во всплывающем окне
};

// =========================================================
// DIALOG
// =========================================================
class SourceAddDialog : public QDialog
{
    Q_OBJECT
public:
    explicit SourceAddDialog(QWidget *parent = nullptr);

    EvoModbus::Source getSource() const;
    void setSource(const EvoModbus::Source &s); // Для режима редактирования

    // Блокировка ID при редактировании
    void setIdEditable(bool editable);

private slots:
    void onCategoryChanged(int index);
    void validateAndAccept(); // Кастомный слот валидации

private:
    QLineEdit *edtId;
    QSpinBox *sbServer;
    QSpinBox *sbAddr;
    QComboBox *cbRegType;
    QComboBox *cbValType;
    QComboBox *cbByteOrder;
    QComboBox *cbCategory;
    QComboBox *cbUnit;
};

// =========================================================
// TABLE MODEL
// =========================================================
class SourceTableModel : public QAbstractTableModel
{
    Q_OBJECT
public:
    enum Columns {
        Col_Action = 0, // Checkbox (Inline) или Button (Dialog)
        Col_ID,
        Col_Server,
        Col_Address,
        Col_RegType,
        Col_ValType,
        Col_ByteOrder,
        Col_Category,
        Col_Unit,
        Col_Remove,
        Col_Count
    };

    struct RowData
    {
        bool enabled{true};
        EvoModbus::Source source;
    };

    explicit SourceTableModel(SourceEditMode mode, QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override;
    QVariant headerData(int section,
                        Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;

    bool addSource(const EvoModbus::Source &s, QString &err);
    void updateSource(int row, const EvoModbus::Source &s);
    void removeSource(int row);
    void setSources(const QVector<EvoModbus::Source> &sources);
    QVector<EvoModbus::Source> getSources() const;

    EvoModbus::Source getSourceAt(int row) const;

    bool isRowEnabled(int row) const;
    bool isIdUnique(const QString &id, int ignoreRow = -1) const;

private:
    SourceEditMode m_mode;
    QVector<RowData> m_data;
};

// =========================================================
// DELEGATE
// =========================================================
class SourceDelegate : public QStyledItemDelegate
{
    Q_OBJECT
public:
    explicit SourceDelegate(SourceEditMode mode, QObject *parent = nullptr);

    QWidget *createEditor(QWidget *parent,
                          const QStyleOptionViewItem &option,
                          const QModelIndex &index) const override;
    void setEditorData(QWidget *editor, const QModelIndex &index) const override;
    void setModelData(QWidget *editor,
                      QAbstractItemModel *model,
                      const QModelIndex &index) const override;
    void updateEditorGeometry(QWidget *editor,
                              const QStyleOptionViewItem &option,
                              const QModelIndex &index) const override;

signals:
    void editRequested(int row);

private:
    SourceEditMode m_mode;
};

// =========================================================
// CONFIG WIDGET
// =========================================================
class SourceConfigWidget : public QWidget
{
    Q_OBJECT
public:
    explicit SourceConfigWidget(EvoModbus::Controller *controller,
                                SourceEditMode mode = SourceEditMode::Inline,
                                QWidget *parent = nullptr);

    void loadFromController();
    void applyToController();

private slots:
    void onAddClicked();
    void onEditRequested(int row);
    void onApplyClicked();
    void onRowsInserted(const QModelIndex &parent, int first, int last);
    void onDataChanged(const QModelIndex &topLeft,
                       const QModelIndex &bottomRight,
                       const QVector<int> &roles);

private:
    EvoModbus::Controller *m_controller{nullptr};
    SourceEditMode m_mode;
    SourceTableModel *m_model{nullptr};
    QTableView *m_view{nullptr};

    void updateEditorsState(int row);
    void openEditorsForRange(int first, int last);
};

} // namespace EvoGui
