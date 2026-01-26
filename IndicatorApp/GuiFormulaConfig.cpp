#include "GuiFormulaConfig.h"
#include <QComboBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLineEdit>
#include <QMessageBox>
#include <QMetaEnum>
#include <QPushButton>
#include <QVBoxLayout>

namespace EvoGui {
using namespace EvoModbus;

static void fillCategoryCombo(QComboBox *cb)
{
    cb->clear();
    const QMetaObject &mo = EvoUnit::staticMetaObject;
    int index = mo.indexOfEnumerator("UnitCategory");
    QMetaEnum me = mo.enumerator(index);
    for (int i = 0; i < me.keyCount(); ++i) {
        auto cat = (EvoUnit::UnitCategory) me.value(i);
        if (cat != EvoUnit::UnitCategory::Unknown)
            cb->addItem(EvoUnit::categoryName(cat), me.value(i));
    }
}
static void fillUnitsByCategory(QComboBox *cb, EvoUnit::UnitCategory cat)
{
    cb->clear();
    QList<EvoUnit::MeasUnit> units = EvoUnit::unitsByType(cat);
    for (auto u : units)
        cb->addItem(EvoUnit::name(u), (int) u);
}

// --- FormulaTableModel ---
FormulaTableModel::FormulaTableModel(QObject *p)
    : QAbstractTableModel(p)
{}
int FormulaTableModel::rowCount(const QModelIndex &) const
{
    return m_data.size();
}
int FormulaTableModel::columnCount(const QModelIndex &) const
{
    return Col_Count;
}

QVariant FormulaTableModel::data(const QModelIndex &idx, int role) const
{
    if (!idx.isValid() || idx.row() >= m_data.size())
        return {};
    const auto &ch = m_data[idx.row()];
    if (role == Qt::DisplayRole || role == Qt::EditRole) {
        if (idx.column() == Col_ID)
            return QString::fromStdString(ch.id);
        if (idx.column() == Col_Formula)
            return ch.formula;
        if (idx.column() == Col_Category) {
            auto cat = EvoUnit::category(ch.unit);
            if (role == Qt::EditRole)
                return (int) cat;
            return EvoUnit::categoryName(cat);
        }
        if (idx.column() == Col_Unit) {
            if (role == Qt::EditRole)
                return (int) ch.unit;
            return EvoUnit::name(ch.unit);
        }
    }
    return {};
}

bool FormulaTableModel::setData(const QModelIndex &idx, const QVariant &val, int role)
{
    if (idx.isValid() && role == Qt::EditRole) {
        auto &ch = m_data[idx.row()];
        if (idx.column() == Col_ID)
            ch.id = val.toString().toStdString();
        else if (idx.column() == Col_Formula)
            ch.formula = val.toString();
        else if (idx.column() == Col_Category) {
            auto cat = (EvoUnit::UnitCategory) val.toInt();
            auto units = EvoUnit::unitsByType(cat);
            ch.unit = units.isEmpty() ? EvoUnit::MeasUnit::Unknown : units.first();
            emit dataChanged(index(idx.row(), Col_Unit), index(idx.row(), Col_Unit));
        } else if (idx.column() == Col_Unit)
            ch.unit = (EvoUnit::MeasUnit) val.toInt();
        emit dataChanged(idx, idx);
        return true;
    }
    return false;
}

QVariant FormulaTableModel::headerData(int s, Qt::Orientation o, int r) const
{
    if (r == Qt::DisplayRole && o == Qt::Horizontal) {
        if (s == 0)
            return "ID";
        if (s == 1)
            return "Formula";
        if (s == 2)
            return "Category";
        if (s == 3)
            return "Unit";
    }
    return {};
}

Qt::ItemFlags FormulaTableModel::flags(const QModelIndex &i) const
{
    if (!i.isValid())
        return Qt::NoItemFlags;
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable;
}

void FormulaTableModel::addChannel(const ComputedChannel &ch)
{
    beginInsertRows({}, m_data.size(), m_data.size());
    m_data.append(ch);
    endInsertRows();
}
void FormulaTableModel::removeChannel(int r)
{
    if (r >= 0 && r < m_data.size()) {
        beginRemoveRows({}, r, r);
        m_data.removeAt(r);
        endRemoveRows();
    }
}
void FormulaTableModel::setChannels(const QVector<ComputedChannel> &c)
{
    beginResetModel();
    m_data = c;
    endResetModel();
}
QVector<ComputedChannel> FormulaTableModel::getChannels() const
{
    return m_data;
}

// --- FormulaDelegate ---
FormulaDelegate::FormulaDelegate(QObject *p)
    : QStyledItemDelegate(p)
{}
QWidget *FormulaDelegate::createEditor(QWidget *p,
                                       const QStyleOptionViewItem &,
                                       const QModelIndex &idx) const
{
    if (idx.column() == FormulaTableModel::Col_ID || idx.column() == FormulaTableModel::Col_Formula)
        return new QLineEdit(p);
    if (idx.column() == FormulaTableModel::Col_Category) {
        auto *cb = new QComboBox(p);
        fillCategoryCombo(cb);
        return cb;
    }
    if (idx.column() == FormulaTableModel::Col_Unit) {
        auto *cb = new QComboBox(p);
        QModelIndex catIdx = idx.sibling(idx.row(), FormulaTableModel::Col_Category);
        auto cat = (EvoUnit::UnitCategory) idx.model()->data(catIdx, Qt::EditRole).toInt();
        fillUnitsByCategory(cb, cat);
        return cb;
    }
    return nullptr;
}
void FormulaDelegate::setEditorData(QWidget *e, const QModelIndex &idx) const
{
    if (auto *le = qobject_cast<QLineEdit *>(e))
        le->setText(idx.model()->data(idx, Qt::EditRole).toString());
    else if (auto *cb = qobject_cast<QComboBox *>(e)) {
        int i = cb->findData(idx.model()->data(idx, Qt::EditRole).toInt());
        if (i >= 0)
            cb->setCurrentIndex(i);
    }
}
void FormulaDelegate::setModelData(QWidget *e, QAbstractItemModel *m, const QModelIndex &idx) const
{
    if (auto *le = qobject_cast<QLineEdit *>(e))
        m->setData(idx, le->text(), Qt::EditRole);
    else if (auto *cb = qobject_cast<QComboBox *>(e))
        m->setData(idx, cb->currentData(), Qt::EditRole);
}

// --- FormulaConfigWidget ---
FormulaConfigWidget::FormulaConfigWidget(Controller *c, QWidget *p)
    : QWidget(p)
    , m_controller(c)
{
    auto *l = new QVBoxLayout(this);
    auto *tb = new QHBoxLayout();
    auto *btnAdd = new QPushButton("Add Formula");
    auto *btnDel = new QPushButton("Remove");
    auto *btnApply = new QPushButton("Apply Scripts");

    tb->addWidget(btnAdd);
    tb->addWidget(btnDel);
    tb->addStretch();
    tb->addWidget(btnApply);
    l->addLayout(tb);

    m_model = new FormulaTableModel(this);
    m_view = new QTableView(this);
    m_view->setModel(m_model);
    m_view->setItemDelegate(new FormulaDelegate(this));
    m_view->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    l->addWidget(m_view);

    connect(btnAdd, &QPushButton::clicked, this, &FormulaConfigWidget::onAddClicked);
    connect(btnDel, &QPushButton::clicked, this, &FormulaConfigWidget::onRemoveClicked);
    connect(btnApply, &QPushButton::clicked, this, &FormulaConfigWidget::onApplyClicked);

    if (m_controller)
        load();
}

void FormulaConfigWidget::onAddClicked()
{
    ComputedChannel ch;
    ch.id = "new_calc";
    ch.formula = "val('src_id') * 1.0";
    m_model->addChannel(ch);
}
void FormulaConfigWidget::onRemoveClicked()
{
    m_model->removeChannel(m_view->currentIndex().row());
}
void FormulaConfigWidget::load()
{
    m_model->setChannels(m_controller->getComputedChannels());
}
void FormulaConfigWidget::onApplyClicked()
{
    m_controller->clearComputedChannels();
    for (const auto &c : m_model->getChannels())
        m_controller->addComputedChannel(c);
    m_controller->buildAndApplyScript();
    QMessageBox::information(this, "Info", "Formulas Applied");
}

} // namespace EvoGui
