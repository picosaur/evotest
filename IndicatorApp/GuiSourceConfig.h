#pragma once

#include <QAbstractTableModel>
#include <QComboBox>
#include <QDialog>
#include <QLineEdit>
#include <QSpinBox>
#include <QStyledItemDelegate>
#include <QTableView>
#include <QWidget>
#include "EvoModbus.h" // Ядро

namespace EvoGui {

// =========================================================
// DIALOG
// =========================================================
class SourceAddDialog : public QDialog
{
    Q_OBJECT
public:
    explicit SourceAddDialog(QWidget *parent = nullptr);
    EvoModbus::Source getSource() const;

private slots:
    void onCategoryChanged(int index);

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
        Col_Enabled = 0, // [NEW] Чекбокс
        Col_ID,
        Col_Server,
        Col_Address,
        Col_RegType,
        Col_ValType,
        Col_ByteOrder,
        Col_Category,
        Col_Unit,
        Col_Count
    };

    // Структура строки для GUI (Обертка над Source)
    struct RowData
    {
        bool enabled{true}; // По умолчанию включено (редакторы видны)
        EvoModbus::Source source;
    };

    explicit SourceTableModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override;
    QVariant headerData(int section,
                        Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;

    bool addSource(const EvoModbus::Source &s, QString &err);
    void removeSource(int row);
    void setSources(const QVector<EvoModbus::Source> &sources);

    // Возвращает только те сорсы, которые есть (enabled статус не влияет на возврат в контроллер,
    // или влияет? Обычно конфиг сохраняет всё, но в контроллер отправляем только активные?
    // Для простоты вернем все, контроллер сам решит).
    QVector<EvoModbus::Source> getSources() const;

    bool isRowEnabled(int row) const;

private:
    QVector<RowData> m_data;
    bool isIdUnique(const QString &id, int ignoreRow = -1) const;
};

// =========================================================
// DELEGATE
// =========================================================
class SourceDelegate : public QStyledItemDelegate
{
    Q_OBJECT
public:
    explicit SourceDelegate(QObject *parent = nullptr);

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
};

// =========================================================
// CONFIG WIDGET
// =========================================================
class SourceConfigWidget : public QWidget
{
    Q_OBJECT
public:
    explicit SourceConfigWidget(EvoModbus::Controller *controller, QWidget *parent = nullptr);
    void loadFromController();
    void applyToController();

private slots:
    void onAddClicked();
    void onRemoveClicked();
    void onApplyClicked();
    void onRowsInserted(const QModelIndex &parent, int first, int last);
    void onDataChanged(const QModelIndex &topLeft,
                       const QModelIndex &bottomRight,
                       const QVector<int> &roles);

private:
    EvoModbus::Controller *m_controller{nullptr};
    SourceTableModel *m_model{nullptr};
    QTableView *m_view{nullptr};

    void updateEditorsState(int row);
};

} // namespace EvoGui
