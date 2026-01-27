#include "GuiSourceConfig.h"
#include <QApplication>
#include <QCheckBox>
#include <QDebug>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QMessageBox>
#include <QMetaEnum>
#include <QPushButton>
#include <QRegExpValidator>
#include <QStyle>
#include <QVBoxLayout>

namespace EvoGui {
using namespace EvoModbus;

// Валидатор ID: начинается с буквы/_, дальше буквы/цифры/_
static const QRegExp ID_REGEX("[a-zA-Z_][a-zA-Z0-9_]*");

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

// Хелперы отображения
static QString getRegTypeName(int type)
{
    auto t = (QModbusDataUnit::RegisterType) type;
    if (t == QModbusDataUnit::HoldingRegisters)
        return "Holding";
    if (t == QModbusDataUnit::InputRegisters)
        return "Input";
    if (t == QModbusDataUnit::Coils)
        return "Coil";
    if (t == QModbusDataUnit::DiscreteInputs)
        return "Discrete";
    return "?";
}
static QString getValTypeName(int type)
{
    auto t = (ValueType) type;
    switch (t) {
    case ValueType::Bool:
        return "Bool";
    case ValueType::UInt16:
        return "UInt16";
    case ValueType::Int16:
        return "Int16";
    case ValueType::UInt32:
        return "UInt32";
    case ValueType::Int32:
        return "Int32";
    case ValueType::Float:
        return "Float";
    }
    return QString::number(type);
}
static QString getByteOrderName(int order)
{
    auto o = (ByteOrder) order;
    switch (o) {
    case ByteOrder::ABCD:
        return "ABCD";
    case ByteOrder::CDAB:
        return "CDAB";
    case ByteOrder::DCBA:
        return "DCBA";
    case ByteOrder::BADC:
        return "BADC";
    }
    return QString::number(order);
}

// =========================================================
// DIALOG
// =========================================================

SourceAddDialog::SourceAddDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("Channel Settings");
    resize(400, 350);
    auto *l = new QFormLayout(this);

    edtId = new QLineEdit;
    edtId->setValidator(new QRegExpValidator(ID_REGEX, this));

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

    // Установка дефолтной категории и обновление списка юнитов
    int defIdx = cbCategory->findData((int) EvoUnit::UnitCategory::Dimensionless);
    if (defIdx < 0)
        defIdx = cbCategory->findData((int) EvoUnit::UnitCategory::Length);

    if (defIdx >= 0) {
        cbCategory->setCurrentIndex(defIdx);
        onCategoryChanged(defIdx);
    } else {
        onCategoryChanged(0);
    }

    l->addRow("ID (Unique):", edtId);
    l->addRow("Server ID:", sbServer);
    l->addRow("Address:", sbAddr);
    l->addRow("Register Type:", cbRegType);
    l->addRow("Data Type:", cbValType);
    l->addRow("Byte Order:", cbByteOrder);
    l->addRow("Category:", cbCategory);
    l->addRow("Unit:", cbUnit);

    auto *box = new QHBoxLayout;
    auto *ok = new QPushButton("OK");
    connect(ok, &QPushButton::clicked, this, &SourceAddDialog::validateAndAccept);

    auto *cancel = new QPushButton("Cancel");
    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);

    box->addWidget(ok);
    box->addWidget(cancel);
    l->addRow(box);
}

void SourceAddDialog::validateAndAccept()
{
    if (edtId->text().isEmpty()) {
        QMessageBox::warning(this, "Error", "ID cannot be empty.");
        edtId->setFocus();
        return;
    }
    accept();
}

void SourceAddDialog::onCategoryChanged(int)
{
    auto cat = (EvoUnit::UnitCategory) cbCategory->currentData().toInt();
    fillUnitsByCategory(cbUnit, cat);

    // Ставим дефолтный юнит для категории
    if (cat == EvoUnit::UnitCategory::Dimensionless) {
        int idx = cbUnit->findData((int) EvoUnit::MeasUnit::Dimensionless);
        if (idx >= 0)
            cbUnit->setCurrentIndex(idx);
    } else if (cat == EvoUnit::UnitCategory::Length) {
        int idx = cbUnit->findData((int) EvoUnit::MeasUnit::Millimeter);
        if (idx >= 0)
            cbUnit->setCurrentIndex(idx);
    }
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

void SourceAddDialog::setSource(const Source &s)
{
    edtId->setText(QString::fromStdString(s.id));
    sbServer->setValue(s.serverAddress);
    sbAddr->setValue(s.valueAddress);

    int idx = cbRegType->findData((int) s.regType);
    if (idx >= 0)
        cbRegType->setCurrentIndex(idx);

    idx = cbValType->findData(s.valueType);
    if (idx >= 0)
        cbValType->setCurrentIndex(idx);

    idx = cbByteOrder->findData(s.byteOrder);
    if (idx >= 0)
        cbByteOrder->setCurrentIndex(idx);

    EvoUnit::UnitCategory cat = EvoUnit::category(s.defaultUnit);
    idx = cbCategory->findData((int) cat);
    if (idx >= 0) {
        cbCategory->setCurrentIndex(idx);
        onCategoryChanged(idx);
        idx = cbUnit->findData((int) s.defaultUnit);
        if (idx >= 0)
            cbUnit->setCurrentIndex(idx);
    }
}

void SourceAddDialog::setIdEditable(bool editable)
{
    edtId->setReadOnly(!editable);
}

// =========================================================
// TABLE MODEL
// =========================================================

SourceTableModel::SourceTableModel(SourceEditMode mode, QObject *p)
    : QAbstractTableModel(p)
    , m_mode(mode)
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

bool SourceTableModel::isRowEnabled(int row) const
{
    if (row >= 0 && row < m_data.size()) {
        if (m_mode == SourceEditMode::Dialog)
            return true;
        return m_data[row].enabled;
    }
    return false;
}

EvoModbus::Source SourceTableModel::getSourceAt(int row) const
{
    if (row >= 0 && row < m_data.size())
        return m_data[row].source;
    return {};
}

int SourceTableModel::rowCount(const QModelIndex &) const
{
    return m_data.size();
}
int SourceTableModel::columnCount(const QModelIndex &) const
{
    return Col_Count;
}

QVariant SourceTableModel::data(const QModelIndex &idx, int role) const
{
    if (!idx.isValid() || idx.row() >= m_data.size())
        return {};
    const auto &row = m_data[idx.row()];
    const auto &s = row.source;

    if (idx.column() == Col_Action) {
        if (m_mode == SourceEditMode::Inline) {
            if (role == Qt::CheckStateRole)
                return row.enabled ? Qt::Checked : Qt::Unchecked;
        } else {
            return QVariant();
        }
    }

    if (role == Qt::DisplayRole) {
        switch (idx.column()) {
        case Col_ID:
            return QString::fromStdString(s.id);
        case Col_Server:
            return s.serverAddress;
        case Col_Address:
            return s.valueAddress;
        case Col_RegType:
            return getRegTypeName((int) s.regType);
        case Col_ValType:
            return getValTypeName(s.valueType);
        case Col_ByteOrder:
            return getByteOrderName(s.byteOrder);
        case Col_Category:
            return EvoUnit::categoryName(EvoUnit::category(s.defaultUnit));
        case Col_Unit:
            return EvoUnit::name(s.defaultUnit);
        case Col_Remove:
            return "";
        }
    }

    if (role == Qt::EditRole) {
        switch (idx.column()) {
        case Col_ID:
            return QString::fromStdString(s.id);
        case Col_Server:
            return s.serverAddress;
        case Col_Address:
            return s.valueAddress;
        case Col_RegType:
            return (int) s.regType;
        case Col_ValType:
            return s.valueType;
        case Col_ByteOrder:
            return s.byteOrder;
        case Col_Category:
            return (int) EvoUnit::category(s.defaultUnit);
        case Col_Unit:
            return (int) s.defaultUnit;
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

    // Inline Checkbox
    if (m_mode == SourceEditMode::Inline && idx.column() == Col_Action
        && role == Qt::CheckStateRole) {
        row.enabled = (val.toInt() == Qt::Checked);
        emit dataChanged(idx, idx);
        emit dataChanged(index(idx.row(), 0), index(idx.row(), Col_Count - 1));
        return true;
    }

    if (m_mode == SourceEditMode::Dialog && idx.column() != Col_Action && idx.column() != Col_Remove)
        return false;
    if (m_mode == SourceEditMode::Inline && !row.enabled && idx.column() != Col_Action
        && idx.column() != Col_Remove)
        return false;

    if (role == Qt::EditRole) {
        bool changed = false;
        switch (idx.column()) {
        case Col_ID: {
            QString newId = val.toString();
            // [FIX] Не даем сохранить пустой ID или дубликат в INLINE режиме
            if (newId.isEmpty())
                return false;
            if (!isIdUnique(newId, idx.row()))
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
        QString firstCol = (m_mode == SourceEditMode::Inline) ? "En" : "";
        const QString names[]
            = {firstCol, "ID", "Srv", "Addr", "Reg", "Type", "Ord", "Cat", "Unit", ""};
        if (sec < 10)
            return names[sec];
    }
    return {};
}

Qt::ItemFlags SourceTableModel::flags(const QModelIndex &idx) const
{
    if (!idx.isValid())
        return Qt::NoItemFlags;

    Qt::ItemFlags f = Qt::ItemIsEnabled;

    if (idx.column() == Col_Remove)
        return f;

    if (m_mode == SourceEditMode::Dialog) {
        if (idx.column() == Col_Action)
            return f;
        return f;
    }

    if (idx.column() == Col_Action)
        return f | Qt::ItemIsUserCheckable;
    if (m_data[idx.row()].enabled) {
        f |= Qt::ItemIsEditable;
    }
    return f;
}

bool SourceTableModel::addSource(const Source &s, QString &err)
{
    if (s.id.empty()) {
        err = "Empty ID";
        return false;
    }
    if (!isIdUnique(QString::fromStdString(s.id))) {
        err = "ID exists";
        return false;
    }
    beginInsertRows({}, m_data.size(), m_data.size());
    RowData rd;
    rd.source = s;
    rd.enabled = true;
    m_data.append(rd);
    endInsertRows();
    return true;
}

void SourceTableModel::updateSource(int row, const Source &s)
{
    if (row >= 0 && row < m_data.size()) {
        m_data[row].source = s;
        emit dataChanged(index(row, 0), index(row, Col_Count - 1));
    }
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
        rd.enabled = true;
        m_data.append(rd);
    }
    endResetModel();
}

QVector<Source> SourceTableModel::getSources() const
{
    QVector<Source> res;
    for (const auto &rd : m_data)
        if (rd.enabled)
            res.append(rd.source);
    return res;
}

// =========================================================
// DELEGATE
// =========================================================

SourceDelegate::SourceDelegate(SourceEditMode mode, QObject *p)
    : QStyledItemDelegate(p)
    , m_mode(mode)
{}

QWidget *SourceDelegate::createEditor(QWidget *p,
                                      const QStyleOptionViewItem &,
                                      const QModelIndex &idx) const
{
    auto self = const_cast<SourceDelegate *>(this);

    // Колонка удаления (всегда)
    if (idx.column() == SourceTableModel::Col_Remove) {
        auto *btn = new QPushButton(p);
        btn->setIcon(QApplication::style()->standardIcon(QStyle::SP_TitleBarCloseButton));
        btn->setStyleSheet("border: none; background: transparent;");
        auto m = const_cast<SourceTableModel *>(qobject_cast<const SourceTableModel *>(idx.model()));
        connect(btn, &QPushButton::clicked, self, [m, idx]() { m->removeSource(idx.row()); });
        return btn;
    }

    // Режим Dialog: Иконка Edit
    if (m_mode == SourceEditMode::Dialog) {
        if (idx.column() == SourceTableModel::Col_Action) {
            auto *btn = new QPushButton(p);
            btn->setIcon(QApplication::style()->standardIcon(QStyle::SP_FileDialogDetailedView));
            btn->setStyleSheet("border: none; background: transparent;");
            connect(btn, &QPushButton::clicked, self, [self, idx]() {
                emit self->editRequested(idx.row());
            });
            return btn;
        }
        return nullptr;
    }

    // Режим Inline
    const SourceTableModel *model = qobject_cast<const SourceTableModel *>(idx.model());
    if (model && !model->isRowEnabled(idx.row()))
        return nullptr;

    if (idx.column() == SourceTableModel::Col_ID) {
        auto *le = new QLineEdit(p);
        le->setValidator(new QRegExpValidator(ID_REGEX, le));

        connect(le, &QLineEdit::editingFinished, self, [self, le, idx, model]() {
            // Fix: Проверка на видимость, чтобы избежать краша
            if (!le->isVisible())
                return;

            QString txt = le->text();
            QString old = model->data(idx, Qt::DisplayRole).toString();

            if (txt == old)
                return;

            // [FIX] Блокируем сигналы, чтобы избежать рекурсии message box
            bool error = false;
            QString msg;

            if (txt.isEmpty()) {
                error = true;
                msg = "ID cannot be empty";
            } else if (!model->isIdUnique(txt, idx.row())) {
                error = true;
                msg = "ID must be unique";
            }

            if (error) {
                le->blockSignals(true);
                QMessageBox::warning(le, "Error", msg);
                le->setText(old);
                le->setFocus();
                le->blockSignals(false);
            } else {
                emit self->commitData(le);
            }
        });
        return le;
    }

    // ... (Остальные редакторы)
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
        auto cat = (EvoUnit::UnitCategory) model->data(catIdx, Qt::EditRole).toInt();
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
    if (auto *le = qobject_cast<QLineEdit *>(e)) {
        // Доп проверка не нужна, так как валидация в createEditor
        if (!le->text().isEmpty())
            m->setData(idx, le->text(), Qt::EditRole);
    } else if (auto *sb = qobject_cast<QSpinBox *>(e))
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

SourceConfigWidget::SourceConfigWidget(Controller *c, SourceEditMode mode, QWidget *p)
    : QWidget(p)
    , m_controller(c)
    , m_mode(mode)
{
    auto *l = new QVBoxLayout(this);
    auto *tb = new QHBoxLayout();
    auto *btnAdd = new QPushButton("Add...");
    auto *btnApply = new QPushButton("Apply");
    btnApply->setStyleSheet("color: green; font-weight: bold;");

    tb->addWidget(btnAdd);
    tb->addStretch();
    tb->addWidget(btnApply);
    l->addLayout(tb);

    m_model = new SourceTableModel(m_mode, this);
    m_view = new QTableView(this);
    m_view->setModel(m_model);

    auto *delegate = new SourceDelegate(m_mode, this);
    m_view->setItemDelegate(delegate);
    connect(delegate, &SourceDelegate::editRequested, this, &SourceConfigWidget::onEditRequested);

    m_view->setSelectionMode(QAbstractItemView::NoSelection);
    m_view->setFocusPolicy(Qt::NoFocus);
    m_view->setShowGrid(true);

    m_view->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_view->horizontalHeader()->setSectionResizeMode(SourceTableModel::Col_Action,
                                                     QHeaderView::ResizeToContents);
    m_view->horizontalHeader()->setSectionResizeMode(SourceTableModel::Col_Remove,
                                                     QHeaderView::ResizeToContents);

    l->addWidget(m_view);

    connect(m_model, &QAbstractTableModel::rowsInserted, this, &SourceConfigWidget::onRowsInserted);
    connect(m_model, &QAbstractTableModel::dataChanged, this, &SourceConfigWidget::onDataChanged);

    connect(btnAdd, &QPushButton::clicked, this, &SourceConfigWidget::onAddClicked);
    connect(btnApply, &QPushButton::clicked, this, &SourceConfigWidget::onApplyClicked);

    if (m_controller)
        loadFromController();
}

void SourceConfigWidget::onRowsInserted(const QModelIndex &, int first, int last)
{
    openEditorsForRange(first, last);
}

void SourceConfigWidget::onDataChanged(const QModelIndex &topLeft,
                                       const QModelIndex &bottomRight,
                                       const QVector<int> &)
{
    if (m_mode == SourceEditMode::Inline) {
        // Переоткрываем редактор юнитов при смене категории
        if (topLeft.column() <= SourceTableModel::Col_Category
            && bottomRight.column() >= SourceTableModel::Col_Category) {
            for (int r = topLeft.row(); r <= bottomRight.row(); ++r) {
                QModelIndex idxUnit = m_model->index(r, SourceTableModel::Col_Unit);
                m_view->closePersistentEditor(idxUnit);
                m_view->openPersistentEditor(idxUnit);
            }
        }

        if (topLeft.column() <= SourceTableModel::Col_Action
            && bottomRight.column() >= SourceTableModel::Col_Action) {
            for (int r = topLeft.row(); r <= bottomRight.row(); ++r)
                updateEditorsState(r);
        }
    }
}

void SourceConfigWidget::updateEditorsState(int row)
{
    bool enabled = m_model->isRowEnabled(row);

    if (m_mode == SourceEditMode::Dialog) {
        m_view->openPersistentEditor(m_model->index(row, SourceTableModel::Col_Action));
        m_view->openPersistentEditor(m_model->index(row, SourceTableModel::Col_Remove));
        return;
    }

    for (int col = 1; col < SourceTableModel::Col_Count; ++col) {
        QModelIndex idx = m_model->index(row, col);
        if (col == SourceTableModel::Col_Remove) {
            m_view->openPersistentEditor(idx);
        } else {
            if (enabled)
                m_view->openPersistentEditor(idx);
            else
                m_view->closePersistentEditor(idx);
        }
    }
}

void SourceConfigWidget::openEditorsForRange(int first, int last)
{
    for (int r = first; r <= last; ++r)
        updateEditorsState(r);
}

void SourceConfigWidget::onAddClicked()
{
    if (m_mode == SourceEditMode::Dialog) {
        SourceAddDialog d(this);
        int cnt = m_model->rowCount(QModelIndex()) + 1;
        Source def;
        do {
            def.id = "channel_" + std::to_string(cnt++);
        } while (!m_model->isIdUnique(QString::fromStdString(def.id)));
        d.setSource(def);

        while (true) {
            if (d.exec() == QDialog::Accepted) {
                QString err;
                Source s = d.getSource();
                if (m_model->addSource(s, err)) {
                    break;
                } else {
                    QMessageBox::warning(this, "Error", err);
                }
            } else {
                break;
            }
        }
    } else {
        // INLINE MODE
        Source s;
        int cnt = m_model->rowCount(QModelIndex()) + 1;
        do {
            s.id = "channel_" + std::to_string(cnt++);
        } while (!m_model->isIdUnique(QString::fromStdString(s.id)));

        s.defaultUnit = EvoUnit::MeasUnit::Dimensionless;

        QString err;
        m_model->addSource(s, err);
    }
}

void SourceConfigWidget::onEditRequested(int row)
{
    if (m_mode != SourceEditMode::Dialog)
        return;

    SourceAddDialog d(this);
    d.setWindowTitle("Edit Channel");
    d.setSource(m_model->getSourceAt(row));
    d.setIdEditable(true);

    // [FIX] Цикл валидации ID при редактировании
    while (true) {
        if (d.exec() == QDialog::Accepted) {
            Source newSource = d.getSource();
            QString newId = QString::fromStdString(newSource.id);
            // Проверка уникальности, исключая себя (row)
            if (m_model->isIdUnique(newId, row)) {
                m_model->updateSource(row, newSource);
                break;
            } else {
                QMessageBox::warning(this, "Error", "ID already exists. Must be unique.");
            }
        } else {
            break;
        }
    }
}

void SourceConfigWidget::loadFromController()
{
    if (m_controller) {
        m_model->setSources(m_controller->getSources());
        openEditorsForRange(0, m_model->rowCount(QModelIndex()) - 1);
    }
}

void SourceConfigWidget::applyToController()
{
    if (!m_controller)
        return;

    m_controller->stop();
    m_controller->clearSources();
    for (const auto &s : m_model->getSources())
        m_controller->addModbusSource(s);
    m_controller->start();
}

void SourceConfigWidget::onApplyClicked()
{
    if (m_view->focusWidget())
        m_view->focusWidget()->clearFocus();
    applyToController();
    QMessageBox::information(this, "Success", "Configuration Applied");
}

} // namespace EvoGui
