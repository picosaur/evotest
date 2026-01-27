#include "GuiDashboard.h"
#include <QComboBox>
#include <QDebug>
#include <QHeaderView>
#include <QVBoxLayout>

namespace EvoGui {
using namespace EvoModbus;

// Хелпер
static void fillUnitsByCategory(QComboBox *cb, EvoUnit::UnitCategory cat)
{
    cb->clear();
    // Добавляем "Auto" (родная единица)
    cb->addItem("Auto (Native)", (int) EvoUnit::MeasUnit::Unknown);

    if (cat != EvoUnit::UnitCategory::Unknown) {
        QList<EvoUnit::MeasUnit> units = EvoUnit::unitsByType(cat);
        for (auto u : units)
            cb->addItem(EvoUnit::name(u), (int) u);
    }
}

// =========================================================
// TABLE MODEL
// =========================================================

MonitorTableModel::MonitorTableModel(Controller *c, QObject *p)
    : QAbstractTableModel(p)
    , m_controller(c)
{
    // При обновлении данных в контроллере обновляем таблицу
    connect(m_controller, &Controller::channelsUpdated, this, &MonitorTableModel::onDataUpdated);
    // Первичная синхронизация
    syncRows();
}

void MonitorTableModel::onDataUpdated()
{
    // 1. Синхронизируем структуру (если появились новые каналы или удалились старые)
    syncRows();

    // 2. Обновляем значения (все строки, колонка Value)
    if (!m_rows.isEmpty()) {
        QModelIndex topLeft = index(0, Col_Value);
        QModelIndex bottomRight = index(m_rows.size() - 1, Col_Value);
        emit dataChanged(topLeft, bottomRight, {Qt::DisplayRole});

        // Также обновляем NativeUnit, вдруг он сменился в конфиге
        QModelIndex tlNative = index(0, Col_NativeUnit);
        QModelIndex brNative = index(m_rows.size() - 1, Col_NativeUnit);
        emit dataChanged(tlNative, brNative, {Qt::DisplayRole});
    }
}

void MonitorTableModel::syncRows()
{
    auto channels = m_controller->getChannels(); // QMap<QString, ChannelData>
    QList<QString> currentIds = channels.keys();

    // Простой алгоритм синхронизации:
    // 1. Удаляем из m_rows те, которых нет в channels
    for (int i = m_rows.size() - 1; i >= 0; --i) {
        if (!channels.contains(m_rows[i].id)) {
            beginRemoveRows(QModelIndex(), i, i);
            m_rows.removeAt(i);
            endRemoveRows();
        }
    }

    // 2. Добавляем в m_rows те, которые есть в channels, но нет у нас
    // Для оптимизации поиска можно использовать QSet, но для GUI списка < 1000 элементов это не критично
    for (const QString &id : currentIds) {
        bool found = false;
        for (const auto &row : m_rows) {
            if (row.id == id) {
                found = true;
                break;
            }
        }
        if (!found) {
            beginInsertRows(QModelIndex(), m_rows.size(), m_rows.size());
            MonitorRow newRow;
            newRow.id = id;
            newRow.displayUnit = EvoUnit::MeasUnit::Unknown; // По умолчанию Auto
            m_rows.append(newRow);
            endInsertRows();
        }
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

QVariant MonitorTableModel::data(const QModelIndex &idx, int role) const
{
    if (!idx.isValid() || idx.row() >= m_rows.size())
        return {};

    const auto &row = m_rows[idx.row()];

    // Получаем актуальные данные из контроллера
    auto channels = m_controller->getChannels();
    if (!channels.contains(row.id))
        return QVariant(); // Should not happen after sync

    ChannelData chData = channels[row.id];

    if (role == Qt::DisplayRole) {
        switch (idx.column()) {
        case Col_ID:
            return row.id;

        case Col_DisplayUnit: {
            if (row.displayUnit == EvoUnit::MeasUnit::Unknown)
                return "Auto";
            return EvoUnit::name(row.displayUnit);
        }

        case Col_NativeUnit:
            return EvoUnit::name(chData.unit);

        case Col_Value: {
            double val = chData.value.toDouble();
            EvoUnit::MeasUnit targetUnit = row.displayUnit;

            // Если пользователь не выбрал юнит, берем родной
            if (targetUnit == EvoUnit::MeasUnit::Unknown) {
                targetUnit = chData.unit;
            }

            // Пытаемся конвертировать
            double convertedVal = EvoUnit::convert(val, chData.unit, targetUnit);

            // Форматируем число
            return QString::number(convertedVal, 'f', 2);
        }
        }
    }

    if (role == Qt::EditRole && idx.column() == Col_DisplayUnit) {
        return (int) row.displayUnit;
    }

    return {};
}

bool MonitorTableModel::setData(const QModelIndex &idx, const QVariant &val, int role)
{
    if (role == Qt::EditRole && idx.column() == Col_DisplayUnit) {
        m_rows[idx.row()].displayUnit = (EvoUnit::MeasUnit) val.toInt();
        emit dataChanged(idx, idx);
        // Значение зависит от юнита, обновляем и его
        emit dataChanged(index(idx.row(), Col_Value), index(idx.row(), Col_Value));
        return true;
    }
    return false;
}

QVariant MonitorTableModel::headerData(int sec, Qt::Orientation o, int r) const
{
    if (r == Qt::DisplayRole && o == Qt::Horizontal) {
        switch (sec) {
        case Col_ID:
            return "Channel ID";
        case Col_DisplayUnit:
            return "View Unit (Edit)";
        case Col_Value:
            return "Value";
        case Col_NativeUnit:
            return "Native Unit";
        }
    }
    return {};
}

Qt::ItemFlags MonitorTableModel::flags(const QModelIndex &idx) const
{
    if (!idx.isValid())
        return Qt::NoItemFlags;

    // Редактируемая только колонка DisplayUnit
    if (idx.column() == Col_DisplayUnit)
        return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable;

    return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

// =========================================================
// DELEGATE (Для выбора единицы отображения)
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

        // Узнаем ID канала
        QString id = idx.sibling(idx.row(), MonitorTableModel::Col_ID).data().toString();
        // Узнаем его родную категорию из контроллера
        auto channels = m_controller->getChannels();
        if (channels.contains(id)) {
            EvoUnit::UnitCategory cat = EvoUnit::category(channels[id].unit);
            fillUnitsByCategory(cb, cat);
        }
        return cb;
    }
    return nullptr;
}

void MonitorDelegate::setEditorData(QWidget *e, const QModelIndex &idx) const
{
    if (auto *cb = qobject_cast<QComboBox *>(e)) {
        int val = idx.model()->data(idx, Qt::EditRole).toInt();
        int i = cb->findData(val);
        if (i >= 0)
            cb->setCurrentIndex(i);
    }
}

void MonitorDelegate::setModelData(QWidget *e, QAbstractItemModel *m, const QModelIndex &idx) const
{
    if (auto *cb = qobject_cast<QComboBox *>(e)) {
        m->setData(idx, cb->currentData(), Qt::EditRole);
    }
}

// =========================================================
// WIDGET
// =========================================================

DashboardWidget::DashboardWidget(Controller *c, QWidget *p)
    : QWidget(p)
{
    auto *layout = new QVBoxLayout(this);

    // Таблица занимает всё пространство
    m_model = new MonitorTableModel(c, this);
    m_view = new QTableView(this);

    m_view->setModel(m_model);
    m_view->setItemDelegate(new MonitorDelegate(c, this));

    m_view->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_view->setAlternatingRowColors(true);
    m_view->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    // Колонка Value и ID получают больше места, юниты меньше
    m_view->horizontalHeader()->setSectionResizeMode(MonitorTableModel::Col_DisplayUnit,
                                                     QHeaderView::ResizeToContents);
    m_view->horizontalHeader()->setSectionResizeMode(MonitorTableModel::Col_NativeUnit,
                                                     QHeaderView::ResizeToContents);

    layout->addWidget(m_view);
}

} // namespace EvoGui
