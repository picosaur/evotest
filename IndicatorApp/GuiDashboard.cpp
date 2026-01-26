#include "GuiDashboard.h"
#include <QComboBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

namespace EvoGui {
using namespace EvoModbus;

// Хелпер для заполнения комбобокса (локальный)
static void fillUnitsByCategory(QComboBox *cb, EvoUnit::UnitCategory cat)
{
    cb->clear();
    QList<EvoUnit::MeasUnit> units = EvoUnit::unitsByType(cat);
    for (auto u : units)
        cb->addItem(EvoUnit::name(u), (int) u);
}

// =========================================================
// MONITOR TABLE MODEL
// =========================================================

MonitorTableModel::MonitorTableModel(Controller *c, QObject *p)
    : QAbstractTableModel(p)
    , m_controller(c)
{
    connect(m_controller, &Controller::channelsUpdated, this, &MonitorTableModel::onDataUpdated);
}

void MonitorTableModel::onDataUpdated()
{
    // Обновляем только колонку со значениями (индекс 2)
    if (rowCount() > 0) {
        emit dataChanged(index(0, Col_Value), index(rowCount() - 1, Col_Value));
    }
}

int MonitorTableModel::rowCount(const QModelIndex &) const
{
    return m_rows.size();
}
int MonitorTableModel::columnCount(const QModelIndex &) const
{
    return Col_Count;
}

void MonitorTableModel::addRow(const QString &id)
{
    auto ch = m_controller->getChannels().value(id);
    beginInsertRows({}, m_rows.size(), m_rows.size());
    m_rows.append({id, ch.unit});
    endInsertRows();
}

void MonitorTableModel::removeRow(int r)
{
    if (r >= 0 && r < m_rows.size()) {
        beginRemoveRows({}, r, r);
        m_rows.removeAt(r);
        endRemoveRows();
    }
}

QVariant MonitorTableModel::data(const QModelIndex &idx, int role) const
{
    if (!idx.isValid() || idx.row() >= m_rows.size())
        return {};
    const auto &row = m_rows[idx.row()];

    if (role == Qt::DisplayRole) {
        if (idx.column() == Col_ID)
            return row.id;
        if (idx.column() == Col_DisplayUnit)
            return EvoUnit::name(row.displayUnit);
        if (idx.column() == Col_Value) {
            auto ch = m_controller->getChannels().value(row.id);
            double val = ch.value.toDouble();
            // Конвертация "на лету" для отображения
            double conv = EvoUnit::convert(val, ch.unit, row.displayUnit);
            return QString::number(conv, 'f', 2);
        }
    }
    if (role == Qt::EditRole && idx.column() == Col_DisplayUnit)
        return (int) row.displayUnit;
    return {};
}

bool MonitorTableModel::setData(const QModelIndex &idx, const QVariant &val, int role)
{
    if (role == Qt::EditRole && idx.column() == Col_DisplayUnit) {
        m_rows[idx.row()].displayUnit = (EvoUnit::MeasUnit) val.toInt();
        emit dataChanged(idx, idx);
        // Принудительно обновляем значение, т.к. оно зависит от Unit
        emit dataChanged(index(idx.row(), Col_Value), index(idx.row(), Col_Value));
        return true;
    }
    return false;
}

QVariant MonitorTableModel::headerData(int sec, Qt::Orientation o, int r) const
{
    if (r == Qt::DisplayRole && o == Qt::Horizontal) {
        if (sec == Col_ID)
            return "ID";
        if (sec == Col_DisplayUnit)
            return "Display Unit";
        if (sec == Col_Value)
            return "Value";
    }
    return {};
}

Qt::ItemFlags MonitorTableModel::flags(const QModelIndex &idx) const
{
    if (!idx.isValid())
        return Qt::NoItemFlags;
    if (idx.column() == Col_DisplayUnit)
        return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable;
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

// =========================================================
// MONITOR DELEGATE
// =========================================================

MonitorDelegate::MonitorDelegate(Controller *c, QObject *p)
    : QStyledItemDelegate(p)
    , m_controller(c)
{}

QWidget *MonitorDelegate::createEditor(QWidget *p,
                                       const QStyleOptionViewItem &,
                                       const QModelIndex &idx) const
{
    if (idx.column() == MonitorTableModel::Col_DisplayUnit) {
        QComboBox *cb = new QComboBox(p);
        // Получаем ID канала из этой строки
        QString id = idx.sibling(idx.row(), 0).data().toString();
        auto ch = m_controller->getChannels().value(id);

        EvoUnit::UnitCategory cat = EvoUnit::category(ch.unit);

        if (cat != EvoUnit::UnitCategory::Unknown)
            fillUnitsByCategory(cb, cat);
        else
            cb->addItem("Unknown", (int) EvoUnit::MeasUnit::Unknown);
        return cb;
    }
    return nullptr;
}

void MonitorDelegate::setEditorData(QWidget *e, const QModelIndex &idx) const
{
    if (auto *cb = qobject_cast<QComboBox *>(e)) {
        int i = cb->findData(idx.model()->data(idx, Qt::EditRole).toInt());
        if (i >= 0)
            cb->setCurrentIndex(i);
    }
}
void MonitorDelegate::setModelData(QWidget *e, QAbstractItemModel *m, const QModelIndex &idx) const
{
    if (auto *cb = qobject_cast<QComboBox *>(e))
        m->setData(idx, cb->currentData(), Qt::EditRole);
}

// =========================================================
// DASHBOARD WIDGET
// =========================================================

DashboardWidget::DashboardWidget(Controller *c, QWidget *p)
    : QWidget(p)
    , m_controller(c)
{
    auto *l = new QVBoxLayout(this);
    auto *tb = new QHBoxLayout();
    auto *btnAdd = new QPushButton("Add Watch");
    auto *btnDel = new QPushButton("Remove");
    tb->addWidget(btnAdd);
    tb->addWidget(btnDel);
    tb->addStretch();
    l->addLayout(tb);

    m_model = new MonitorTableModel(c, this);
    auto *tv = new QTableView(this);
    tv->setModel(m_model);
    tv->setItemDelegate(new MonitorDelegate(c, this));
    tv->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    l->addWidget(tv);

    m_view = tv; // Сохраняем указатель для удаления

    // СВЯЗЫВАЕМ СИГНАЛЫ СО СЛОТАМИ КЛАССА
    connect(btnAdd, &QPushButton::clicked, this, &DashboardWidget::onAddClicked);
    connect(btnDel, &QPushButton::clicked, this, &DashboardWidget::onRemoveClicked);
}

void DashboardWidget::onAddClicked()
{
    bool ok;
    QString id
        = QInputDialog::getText(this, "Watch", "Enter Channel ID:", QLineEdit::Normal, "", &ok);
    if (ok && !id.isEmpty()) {
        if (m_controller->getChannels().contains(id)) {
            m_model->addRow(id);
        } else {
            QMessageBox::warning(this, "Error", "Channel ID not found in controller.");
        }
    }
}

void DashboardWidget::onRemoveClicked()
{
    QModelIndex idx = m_view->currentIndex();
    if (idx.isValid()) {
        m_model->removeRow(idx.row());
    }
}

} // namespace EvoGui
