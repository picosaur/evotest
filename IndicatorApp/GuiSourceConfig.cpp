#include "GuiSourceConfig.h"
#include <QCheckBox>
#include <QDebug>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
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

// =========================================================
// DIALOG
// =========================================================

SourceAddDialog::SourceAddDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("Add Channel");
    resize(400, 300);
    auto *l = new QFormLayout(this);

    edtId = new QLineEdit;
    sbServer = new QSpinBox;
    sbServer->setRange(1, 255);
    sbAddr = new QSpinBox;
    sbAddr->setRange(0, 65535);

    cbRegType = new QComboBox;
    cbRegType->addItem("Holding (4x)", QModbusDataUnit::HoldingRegisters);
    cbRegType->addItem("Input (3x)", QModbusDataUnit::InputRegisters);
    cbRegType->addItem("Coil (0x)", QModbusDataUnit::Coils);
    cbRegType->addItem("Discrete (1x)", QModbusDataUnit::DiscreteInputs);

    cbValType = new QComboBox;
    cbValType->addItem("UInt16", (int) ValueType::UInt16);
    cbValType->addItem("Int16", (int) ValueType::Int16);
    cbValType->addItem("Float", (int) ValueType::Float);
    cbValType->addItem("UInt32", (int) ValueType::UInt32);
    cbValType->addItem("Int32", (int) ValueType::Int32);
    cbValType->addItem("Bool", (int) ValueType::Bool);

    cbByteOrder = new QComboBox;
    cbByteOrder->addItem("ABCD", (int) ByteOrder::ABCD);
    cbByteOrder->addItem("CDAB", (int) ByteOrder::CDAB);
    cbByteOrder->addItem("DCBA", (int) ByteOrder::DCBA);
    cbByteOrder->addItem("BADC", (int) ByteOrder::BADC);

    cbCategory = new QComboBox;
    fillCategoryCombo(cbCategory);
    cbUnit = new QComboBox;

    connect(cbCategory,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this,
            &SourceAddDialog::onCategoryChanged);
    onCategoryChanged(0);

    l->addRow("ID:", edtId);
    l->addRow("Server:", sbServer);
    l->addRow("Address:", sbAddr);
    l->addRow("Register:", cbRegType);
    l->addRow("Type:", cbValType);
    l->addRow("Order:", cbByteOrder);
    l->addRow("Category:", cbCategory);
    l->addRow("Unit:", cbUnit);

    auto *box = new QHBoxLayout;
    auto *ok = new QPushButton("OK");
    auto *cancel = new QPushButton("Cancel");
    connect(ok, &QPushButton::clicked, this, &QDialog::accept);
    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
    box->addWidget(ok);
    box->addWidget(cancel);
    l->addRow(box);
}

void SourceAddDialog::onCategoryChanged(int)
{
    auto cat = (EvoUnit::UnitCategory) cbCategory->currentData().toInt();
    fillUnitsByCategory(cbUnit, cat);
}

Source SourceAddDialog::getSource() const
{
    Source s;
    s.id = edtId->text().toStdString();
    s.serverAddress = sbServer->value();
    s.valueAddress = sbAddr->value();
    s.regType = (QModbusDataUnit::RegisterType) cbRegType->currentData().toInt();
    s.valueType = cbValType->currentData().toInt();
    s.byteOrder = cbByteOrder->currentData().toInt();
    s.defaultUnit = (EvoUnit::MeasUnit) cbUnit->currentData().toInt();
    return s;
}

// =========================================================
// TABLE MODEL
// =========================================================

SourceTableModel::SourceTableModel(QObject *p)
    : QAbstractTableModel(p)
{}

bool SourceTableModel::isIdUnique(const QString &id, int ignoreRow) const
{
    std::string target = id.toStdString();
    for (int i = 0; i < m_data.size(); ++i) {
        if (i == ignoreRow)
            continue;
        if (m_data[i].source.id == target)
            return false;
    }
    return true;
}

int SourceTableModel::rowCount(const QModelIndex &) const
{
    return m_data.size();
}
int SourceTableModel::columnCount(const QModelIndex &) const
{
    return Col_Count;
}

bool SourceTableModel::isRowEnabled(int row) const
{
    if (row >= 0 && row < m_data.size())
        return m_data[row].enabled;
    return false;
}

QVariant SourceTableModel::data(const QModelIndex &idx, int role) const
{
    if (!idx.isValid() || idx.row() >= m_data.size())
        return {};
    const auto &row = m_data[idx.row()];
    const auto &s = row.source;

    // Чекбокс рисуем через CheckStateRole
    if (idx.column() == Col_Enabled && role == Qt::CheckStateRole) {
        return row.enabled ? Qt::Checked : Qt::Unchecked;
    }

    if (role == Qt::DisplayRole || role == Qt::EditRole) {
        switch (idx.column()) {
        case Col_Enabled:
            return QVariant(); // Пустой текст, только чекбокс
        case Col_ID:
            return QString::fromStdString(s.id);
        case Col_Server:
            return s.serverAddress;
        case Col_Address:
            return s.valueAddress;
        case Col_RegType:
            if (role == Qt::EditRole)
                return (int) s.regType;
            return (s.regType == QModbusDataUnit::Coils) ? "Coil" : "Holding";
        case Col_ValType:
            if (role == Qt::EditRole)
                return s.valueType;
            return QString::number(s.valueType);
        case Col_ByteOrder:
            if (role == Qt::EditRole)
                return s.byteOrder;
            return QString::number(s.byteOrder);
        case Col_Category: {
            auto cat = EvoUnit::category(s.defaultUnit);
            if (role == Qt::EditRole)
                return (int) cat;
            return EvoUnit::categoryName(cat);
        }
        case Col_Unit:
            if (role == Qt::EditRole)
                return (int) s.defaultUnit;
            return EvoUnit::name(s.defaultUnit);
        }
    }
    return {};
}

bool SourceTableModel::setData(const QModelIndex &idx, const QVariant &val, int role)
{
    if (!idx.isValid())
        return false;
    RowData &row = m_data[idx.row()];
    Source &s = row.source;

    // Обработка клика по чекбоксу
    if (idx.column() == Col_Enabled && role == Qt::CheckStateRole) {
        row.enabled = (val.toInt() == Qt::Checked);
        emit dataChanged(idx, idx);
        // Нужно обновить всю строку, чтобы перерисовались редакторы (включились/выключились)
        emit dataChanged(index(idx.row(), 0), index(idx.row(), Col_Count - 1));
        return true;
    }

    // Редактирование данных только если строка включена
    if (!row.enabled && idx.column() != Col_Enabled)
        return false;

    if (role == Qt::EditRole) {
        bool changed = false;
        switch (idx.column()) {
        case Col_ID: {
            QString newId = val.toString();
            if (newId.isEmpty() || !isIdUnique(newId, idx.row()))
                return false;
            s.id = newId.toStdString();
            changed = true;
            break;
        }
        case Col_Server:
            s.serverAddress = val.toInt();
            changed = true;
            break;
        case Col_Address:
            s.valueAddress = val.toInt();
            changed = true;
            break;
        case Col_RegType:
            s.regType = (QModbusDataUnit::RegisterType) val.toInt();
            changed = true;
            break;
        case Col_ValType:
            s.valueType = val.toInt();
            changed = true;
            break;
        case Col_ByteOrder:
            s.byteOrder = val.toInt();
            changed = true;
            break;

        case Col_Category: {
            auto cat = (EvoUnit::UnitCategory) val.toInt();
            auto units = EvoUnit::unitsByType(cat);
            s.defaultUnit = units.isEmpty() ? EvoUnit::MeasUnit::Unknown : units.first();
            emit dataChanged(index(idx.row(), Col_Unit), index(idx.row(), Col_Unit));
            changed = true;
            break;
        }
        case Col_Unit:
            s.defaultUnit = (EvoUnit::MeasUnit) val.toInt();
            changed = true;
            break;
        }
        if (changed)
            emit dataChanged(idx, idx);
        return changed;
    }
    return false;
}

QVariant SourceTableModel::headerData(int sec, Qt::Orientation o, int r) const
{
    if (r == Qt::DisplayRole && o == Qt::Horizontal) {
        const char *n[] = {"", "ID", "Srv", "Addr", "Reg", "Type", "Ord", "Cat", "Unit"};
        if (sec < 9)
            return n[sec];
    }
    return {};
}

Qt::ItemFlags SourceTableModel::flags(const QModelIndex &idx) const
{
    if (!idx.isValid())
        return Qt::NoItemFlags;

    Qt::ItemFlags f = Qt::ItemIsSelectable | Qt::ItemIsEnabled;

    // Колонка чекбокса всегда активна и чекабельна
    if (idx.column() == Col_Enabled) {
        return f | Qt::ItemIsUserCheckable; // Убрали Editable, добавили UserCheckable
    }

    // Остальные колонки редактируемы только если строка enabled
    if (m_data[idx.row()].enabled) {
        f |= Qt::ItemIsEditable;
    } else {
        // Можно убрать ItemIsEnabled чтобы визуально задизейблить, но тогда выделение пропадет
        // Лучше оставить, но не давать Editable
    }
    return f;
}

bool SourceTableModel::addSource(const Source &s, QString &err)
{
    if (!isIdUnique(QString::fromStdString(s.id))) {
        err = "ID exists";
        return false;
    }
    beginInsertRows({}, m_data.size(), m_data.size());
    RowData rd;
    rd.source = s;
    rd.enabled = true; // Новые всегда активны
    m_data.append(rd);
    endInsertRows();
    return true;
}

void SourceTableModel::removeSource(int r)
{
    if (r >= 0 && r < m_data.size()) {
        beginRemoveRows({}, r, r);
        m_data.removeAt(r);
        endRemoveRows();
    }
}

void SourceTableModel::setSources(const QVector<Source> &s)
{
    beginResetModel();
    m_data.clear();
    for (const auto &src : s) {
        RowData rd;
        rd.source = src;
        rd.enabled = true; // При загрузке включаем
        m_data.append(rd);
    }
    endResetModel();
}

QVector<Source> SourceTableModel::getSources() const
{
    QVector<Source> res;
    for (const auto &rd : m_data) {
        // Возвращаем все (включая выключенные), или только включенные?
        // Логичнее отправлять в контроллер только включенные, чтобы драйвер их читал.
        if (rd.enabled) {
            res.append(rd.source);
        }
    }
    return res;
}

// =========================================================
// DELEGATE
// =========================================================

SourceDelegate::SourceDelegate(QObject *p)
    : QStyledItemDelegate(p)
{}

QWidget *SourceDelegate::createEditor(QWidget *p,
                                      const QStyleOptionViewItem &,
                                      const QModelIndex &idx) const
{
    // 1. Проверяем, включена ли строка. Если нет, редактор не создаем.
    // Мы можем получить доступ к модели через индекс
    const SourceTableModel *model = qobject_cast<const SourceTableModel *>(idx.model());
    if (model && !model->isRowEnabled(idx.row())) {
        return nullptr;
    }

    auto self = const_cast<SourceDelegate *>(this);

    if (idx.column() == SourceTableModel::Col_ID) {
        auto *le = new QLineEdit(p);
        connect(le, &QLineEdit::editingFinished, self, [self, le]() { emit self->commitData(le); });
        return le;
    }

    if (idx.column() == SourceTableModel::Col_Server
        || idx.column() == SourceTableModel::Col_Address) {
        auto *sb = new QSpinBox(p);
        sb->setRange(0, 65535);
        connect(sb, QOverload<int>::of(&QSpinBox::valueChanged), self, [self, sb]() {
            emit self->commitData(sb);
        });
        return sb;
    }

    if (idx.column() == SourceTableModel::Col_RegType) {
        auto *cb = new QComboBox(p);
        cb->addItem("Holding", QModbusDataUnit::HoldingRegisters);
        cb->addItem("Input", QModbusDataUnit::InputRegisters);
        cb->addItem("Coil", QModbusDataUnit::Coils);
        cb->addItem("Discrete", QModbusDataUnit::DiscreteInputs);
        connect(cb, QOverload<int>::of(&QComboBox::currentIndexChanged), self, [self, cb]() {
            emit self->commitData(cb);
        });
        return cb;
    }

    if (idx.column() == SourceTableModel::Col_ValType) {
        auto *cb = new QComboBox(p);
        cb->addItem("UInt16", (int) ValueType::UInt16);
        cb->addItem("Int16", (int) ValueType::Int16);
        cb->addItem("Float", (int) ValueType::Float);
        cb->addItem("UInt32", (int) ValueType::UInt32);
        cb->addItem("Int32", (int) ValueType::Int32);
        cb->addItem("Bool", (int) ValueType::Bool);
        connect(cb, QOverload<int>::of(&QComboBox::currentIndexChanged), self, [self, cb]() {
            emit self->commitData(cb);
        });
        return cb;
    }

    if (idx.column() == SourceTableModel::Col_ByteOrder) {
        auto *cb = new QComboBox(p);
        cb->addItem("ABCD", (int) ByteOrder::ABCD);
        cb->addItem("CDAB", (int) ByteOrder::CDAB);
        cb->addItem("DCBA", (int) ByteOrder::DCBA);
        cb->addItem("BADC", (int) ByteOrder::BADC);
        connect(cb, QOverload<int>::of(&QComboBox::currentIndexChanged), self, [self, cb]() {
            emit self->commitData(cb);
        });
        return cb;
    }

    if (idx.column() == SourceTableModel::Col_Category) {
        auto *cb = new QComboBox(p);
        fillCategoryCombo(cb);
        connect(cb, QOverload<int>::of(&QComboBox::currentIndexChanged), self, [self, cb]() {
            emit self->commitData(cb);
        });
        return cb;
    }

    if (idx.column() == SourceTableModel::Col_Unit) {
        auto *cb = new QComboBox(p);
        QModelIndex catIdx = idx.sibling(idx.row(), SourceTableModel::Col_Category);
        auto cat = (EvoUnit::UnitCategory) idx.model()->data(catIdx, Qt::EditRole).toInt();
        fillUnitsByCategory(cb, cat);
        connect(cb, QOverload<int>::of(&QComboBox::currentIndexChanged), self, [self, cb]() {
            emit self->commitData(cb);
        });
        return cb;
    }

    return nullptr;
}

void SourceDelegate::setEditorData(QWidget *e, const QModelIndex &idx) const
{
    if (auto *le = qobject_cast<QLineEdit *>(e))
        le->setText(idx.model()->data(idx, Qt::EditRole).toString());
    else if (auto *sb = qobject_cast<QSpinBox *>(e))
        sb->setValue(idx.model()->data(idx, Qt::EditRole).toInt());
    else if (auto *cb = qobject_cast<QComboBox *>(e)) {
        bool b = cb->blockSignals(true);
        int i = cb->findData(idx.model()->data(idx, Qt::EditRole).toInt());
        if (i >= 0)
            cb->setCurrentIndex(i);
        cb->blockSignals(b);
    }
}

void SourceDelegate::setModelData(QWidget *e, QAbstractItemModel *m, const QModelIndex &idx) const
{
    if (auto *le = qobject_cast<QLineEdit *>(e))
        m->setData(idx, le->text(), Qt::EditRole);
    else if (auto *sb = qobject_cast<QSpinBox *>(e))
        m->setData(idx, sb->value(), Qt::EditRole);
    else if (auto *cb = qobject_cast<QComboBox *>(e))
        m->setData(idx, cb->currentData(), Qt::EditRole);
}

void SourceDelegate::updateEditorGeometry(QWidget *e,
                                          const QStyleOptionViewItem &o,
                                          const QModelIndex &) const
{
    e->setGeometry(o.rect);
}

// =========================================================
// WIDGET
// =========================================================

SourceConfigWidget::SourceConfigWidget(Controller *c, QWidget *p)
    : QWidget(p)
    , m_controller(c)
{
    auto *l = new QVBoxLayout(this);
    auto *tb = new QHBoxLayout();
    auto *btnAdd = new QPushButton("Add...");
    auto *btnDel = new QPushButton("Remove");
    auto *btnApply = new QPushButton("Apply");
    btnApply->setStyleSheet("color: green; font-weight: bold;");

    tb->addWidget(btnAdd);
    tb->addWidget(btnDel);
    tb->addStretch();
    tb->addWidget(btnApply);
    l->addLayout(tb);

    m_model = new SourceTableModel(this);
    m_view = new QTableView(this);
    m_view->setModel(m_model);
    m_view->setItemDelegate(new SourceDelegate(this));

    // Сетка и Ресайз
    m_view->setShowGrid(true);
    m_view->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_view->horizontalHeader()->setSectionResizeMode(SourceTableModel::Col_Enabled,
                                                     QHeaderView::ResizeToContents);

    l->addWidget(m_view);

    // Коннекты
    connect(m_model, &QAbstractTableModel::rowsInserted, this, &SourceConfigWidget::onRowsInserted);

    // Обработка изменения чекбокса (для закрытия/открытия редакторов)
    connect(m_model, &QAbstractTableModel::dataChanged, this, &SourceConfigWidget::onDataChanged);

    connect(btnAdd, &QPushButton::clicked, this, &SourceConfigWidget::onAddClicked);
    connect(btnDel, &QPushButton::clicked, this, &SourceConfigWidget::onRemoveClicked);
    connect(btnApply, &QPushButton::clicked, this, &SourceConfigWidget::onApplyClicked);

    if (m_controller)
        loadFromController();
}

void SourceConfigWidget::onRowsInserted(const QModelIndex &, int first, int last)
{
    for (int i = first; i <= last; ++i)
        updateEditorsState(i);
}

void SourceConfigWidget::onDataChanged(const QModelIndex &topLeft,
                                       const QModelIndex &bottomRight,
                                       const QVector<int> &)
{
    // Если изменился чекбокс (Col_Enabled = 0), обновляем редакторы в этой строке
    if (topLeft.column() <= SourceTableModel::Col_Enabled
        && bottomRight.column() >= SourceTableModel::Col_Enabled) {
        for (int r = topLeft.row(); r <= bottomRight.row(); ++r) {
            updateEditorsState(r);
        }
    }
}

void SourceConfigWidget::updateEditorsState(int row)
{
    bool enabled = m_model->isRowEnabled(row);

    // Проходим по всем колонкам кроме чекбокса
    for (int col = 1; col < SourceTableModel::Col_Count; ++col) {
        QModelIndex idx = m_model->index(row, col);
        if (enabled) {
            m_view->openPersistentEditor(idx);
        } else {
            m_view->closePersistentEditor(idx);
        }
    }
}

void SourceConfigWidget::onAddClicked()
{
    SourceAddDialog d(this);
    if (d.exec() == QDialog::Accepted) {
        QString err;
        if (!m_model->addSource(d.getSource(), err)) {
            QMessageBox::warning(this, "Error", err);
        }
    }
}

void SourceConfigWidget::onRemoveClicked()
{
    m_model->removeSource(m_view->currentIndex().row());
}

void SourceConfigWidget::loadFromController()
{
    if (m_controller) {
        m_model->setSources(m_controller->getSources());
        // После загрузки обновляем все редакторы
        for (int i = 0; i < m_model->rowCount(QModelIndex()); ++i)
            updateEditorsState(i);
    }
}

void SourceConfigWidget::applyToController()
{
    onApplyClicked();
}

void SourceConfigWidget::onApplyClicked()
{
    if (!m_controller)
        return;

    if (m_view->focusWidget())
        m_view->focusWidget()->clearFocus();

    m_controller->stop();
    m_controller->clearSources();

    // getSources возвращает только enabled строки (как мы договорились)
    QVector<Source> sources = m_model->getSources();
    for (const auto &s : sources)
        m_controller->addModbusSource(s);

    m_controller->start();
    QMessageBox::information(this, "Success", "Configuration Applied (Active channels only)");
}

} // namespace EvoGui
