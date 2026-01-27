#include "GuiFormulaConfig.h"
#include <QApplication>
#include <QComboBox>
#include <QDebug>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLineEdit>
#include <QMessageBox>
#include <QMetaEnum>
#include <QPushButton>
#include <QRegExpValidator>
#include <QStyle>
#include <QToolButton>
#include <QVBoxLayout>

namespace EvoGui {
using namespace EvoModbus;

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

// =========================================================
// CUSTOM WIDGET FOR INLINE FORMULA EDITING
// =========================================================
class FormulaInlineWidget : public QWidget
{
    Q_OBJECT
public:
    explicit FormulaInlineWidget(const QString &formula, QWidget *parent = nullptr)
        : QWidget(parent)
    {
        auto *layout = new QHBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(2);

        m_lineEdit = new QLineEdit(formula, this);
        m_lineEdit->setReadOnly(true); // Только через диалог редактируем код
        layout->addWidget(m_lineEdit);

        auto *btn = new QToolButton(this);
        btn->setIcon(QApplication::style()->standardIcon(QStyle::SP_FileDialogDetailedView));
        btn->setAutoRaise(true);
        connect(btn, &QToolButton::clicked, this, &FormulaInlineWidget::editClicked);
        layout->addWidget(btn);
    }

    QString text() const { return m_lineEdit->text(); }
    void setText(const QString &t) { m_lineEdit->setText(t); }

signals:
    void editClicked();

private:
    QLineEdit *m_lineEdit;
};
#include "GuiFormulaConfig.moc" // Для moc генератора если всё в одном файле

// =========================================================
// SCRIPT EDIT DIALOG
// =========================================================
ScriptEditDialog::ScriptEditDialog(const QString &currentScript, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("Edit Logic Script");
    resize(600, 400);
    auto *l = new QVBoxLayout(this);

    l->addWidget(new QLabel("JavaScript function body (e.g. return IO.val('id') * 2;):"));

    m_editor = new QTextEdit(this);
    m_editor->setFontFamily("Courier");
    m_editor->setPlainText(currentScript);
    l->addWidget(m_editor);

    auto *box = new QHBoxLayout;
    auto *ok = new QPushButton("OK");
    connect(ok, &QPushButton::clicked, this, &QDialog::accept);
    auto *cancel = new QPushButton("Cancel");
    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
    box->addWidget(ok);
    box->addWidget(cancel);
    l->addLayout(box);
}

QString ScriptEditDialog::getScript() const
{
    return m_editor->toPlainText();
}

// =========================================================
// FORMULA ADD DIALOG
// =========================================================
FormulaAddDialog::FormulaAddDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("Add Computed Channel");
    auto *l = new QFormLayout(this);

    edtId = new QLineEdit;
    edtId->setValidator(new QRegExpValidator(ID_REGEX, this));

    edtFormula = new QLineEdit;
    edtFormula->setReadOnly(true);
    btnScript = new QPushButton("Edit Script...");
    connect(btnScript, &QPushButton::clicked, this, &FormulaAddDialog::onEditScript);

    auto *hLay = new QHBoxLayout;
    hLay->addWidget(edtFormula);
    hLay->addWidget(btnScript);

    cbCategory = new QComboBox;
    fillCategoryCombo(cbCategory);
    cbUnit = new QComboBox;

    connect(cbCategory,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this,
            &FormulaAddDialog::onCategoryChanged);
    onCategoryChanged(0);

    l->addRow("ID:", edtId);
    l->addRow("Formula:", hLay);
    l->addRow("Category:", cbCategory);
    l->addRow("Unit:", cbUnit);

    auto *box = new QHBoxLayout;
    auto *ok = new QPushButton("OK");
    connect(ok, &QPushButton::clicked, this, &FormulaAddDialog::validateAndAccept);
    auto *cancel = new QPushButton("Cancel");
    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
    box->addWidget(ok);
    box->addWidget(cancel);
    l->addRow(box);
}

void FormulaAddDialog::validateAndAccept()
{
    if (edtId->text().isEmpty()) {
        QMessageBox::warning(this, "Error", "ID required");
        return;
    }
    accept();
}

void FormulaAddDialog::onEditScript()
{
    ScriptEditDialog d(edtFormula->text(), this);
    if (d.exec() == Accepted)
        edtFormula->setText(d.getScript());
}

void FormulaAddDialog::onCategoryChanged(int)
{
    auto cat = (EvoUnit::UnitCategory) cbCategory->currentData().toInt();
    fillUnitsByCategory(cbUnit, cat);
}

ComputedChannel FormulaAddDialog::getChannel() const
{
    ComputedChannel ch;
    ch.id = edtId->text().toStdString();
    ch.formula = edtFormula->text();
    ch.unit = (EvoUnit::MeasUnit) cbUnit->currentData().toInt();
    return ch;
}

void FormulaAddDialog::setChannel(const ComputedChannel &ch)
{
    edtId->setText(QString::fromStdString(ch.id));
    edtFormula->setText(ch.formula);

    EvoUnit::UnitCategory cat = EvoUnit::category(ch.unit);
    int idx = cbCategory->findData((int) cat);
    if (idx >= 0) {
        cbCategory->setCurrentIndex(idx);
        onCategoryChanged(idx);
        int uIdx = cbUnit->findData((int) ch.unit);
        if (uIdx >= 0)
            cbUnit->setCurrentIndex(uIdx);
    }
}

void FormulaAddDialog::setIdEditable(bool editable)
{
    edtId->setReadOnly(!editable);
}

// =========================================================
// TABLE MODEL
// =========================================================
FormulaTableModel::FormulaTableModel(SourceEditMode mode, Controller *c, QObject *p)
    : QAbstractTableModel(p)
    , m_mode(mode)
    , m_controller(c)
{}

bool FormulaTableModel::isIdUnique(const QString &id, int ignoreRow) const
{
    std::string target = id.toStdString();
    for (int i = 0; i < m_data.size(); ++i) {
        if (i == ignoreRow)
            continue;
        if (m_data[i].channel.id == target)
            return false;
    }
    if (m_controller && !m_controller->isIdUnique(id))
        return false;
    return true;
}

bool FormulaTableModel::isRowEnabled(int row) const
{
    if (m_mode == SourceEditMode::Dialog)
        return true;
    if (row >= 0 && row < m_data.size())
        return m_data[row].enabled;
    return false;
}

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
    if (!idx.isValid())
        return {};
    const auto &ch = m_data[idx.row()].channel;

    if (idx.column() == Col_Action) {
        if (m_mode == SourceEditMode::Inline && role == Qt::CheckStateRole)
            return m_data[idx.row()].enabled ? Qt::Checked : Qt::Unchecked;
        return {};
    }

    if (role == Qt::DisplayRole || role == Qt::EditRole) {
        switch (idx.column()) {
        case Col_ID:
            return QString::fromStdString(ch.id);
        case Col_Formula:
            return ch.formula; // [FIX] Теперь возвращаем формулу всегда
        case Col_Category: {
            auto cat = EvoUnit::category(ch.unit);
            if (role == Qt::EditRole)
                return (int) cat;
            return EvoUnit::categoryName(cat);
        }
        case Col_Unit:
            if (role == Qt::EditRole)
                return (int) ch.unit;
            return EvoUnit::name(ch.unit);
        }
    }
    return {};
}

bool FormulaTableModel::setData(const QModelIndex &idx, const QVariant &val, int role)
{
    if (!idx.isValid())
        return false;
    auto &row = m_data[idx.row()];
    auto &ch = row.channel;

    if (m_mode == SourceEditMode::Inline && idx.column() == Col_Action
        && role == Qt::CheckStateRole) {
        row.enabled = (val.toInt() == Qt::Checked);
        emit dataChanged(idx, idx);
        emit dataChanged(index(idx.row(), 0), index(idx.row(), Col_Count - 1));
        return true;
    }

    if (role == Qt::EditRole) {
        if (idx.column() == Col_ID) {
            QString newId = val.toString();
            if (newId.isEmpty() || !isIdUnique(newId, idx.row()))
                return false;
            ch.id = newId.toStdString();
        } else if (idx.column() == Col_Formula) {
            ch.formula = val.toString();
        } else if (idx.column() == Col_Category) {
            auto cat = (EvoUnit::UnitCategory) val.toInt();
            auto units = EvoUnit::unitsByType(cat);
            ch.unit = units.isEmpty() ? EvoUnit::MeasUnit::Unknown : units.first();
            emit dataChanged(index(idx.row(), Col_Unit), index(idx.row(), Col_Unit));
        } else if (idx.column() == Col_Unit) {
            ch.unit = (EvoUnit::MeasUnit) val.toInt();
        }
        emit dataChanged(idx, idx);
        return true;
    }
    return false;
}

QVariant FormulaTableModel::headerData(int s, Qt::Orientation o, int r) const
{
    if (r == Qt::DisplayRole && o == Qt::Horizontal) {
        QString c1 = (m_mode == SourceEditMode::Inline) ? "En" : "Ed";
        if (s == 0)
            return c1;
        if (s == 1)
            return "ID";
        if (s == 2)
            return "Formula";
        if (s == 3)
            return "Category";
        if (s == 4)
            return "Unit";
        if (s == 5)
            return "Del";
    }
    return {};
}

Qt::ItemFlags FormulaTableModel::flags(const QModelIndex &i) const
{
    if (!i.isValid())
        return Qt::NoItemFlags;
    Qt::ItemFlags f = Qt::ItemIsEnabled;
    if (i.column() == Col_Remove)
        return f;

    // В Dialog режиме колонка Formula только для чтения (отображает код)
    // Но в Inline режиме она редактируется через кастомный виджет
    if (m_mode == SourceEditMode::Dialog) {
        if (i.column() == Col_Action)
            return f; // Кнопка Edit
        return f;     // Остальные ReadOnly
    }

    // Inline
    if (i.column() == Col_Action)
        return f | Qt::ItemIsUserCheckable;
    if (m_data[i.row()].enabled)
        f |= Qt::ItemIsEditable;
    return f;
}

bool FormulaTableModel::addChannel(const ComputedChannel &ch, QString &err)
{
    if (!isIdUnique(QString::fromStdString(ch.id))) {
        err = "Duplicate ID";
        return false;
    }
    beginInsertRows({}, m_data.size(), m_data.size());
    RowData rd;
    rd.channel = ch;
    rd.enabled = true;
    m_data.append(rd);
    endInsertRows();
    return true;
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
    m_data.clear();
    for (const auto &ch : c) {
        RowData rd;
        rd.channel = ch;
        rd.enabled = true;
        m_data.append(rd);
    }
    endResetModel();
}
QVector<ComputedChannel> FormulaTableModel::getChannels() const
{
    QVector<ComputedChannel> res;
    for (const auto &rd : m_data)
        if (rd.enabled)
            res.append(rd.channel);
    return res;
}
ComputedChannel FormulaTableModel::getChannelAt(int r) const
{
    return m_data[r].channel;
}
void FormulaTableModel::updateChannel(int r, const ComputedChannel &ch)
{
    if (r >= 0 && r < m_data.size()) {
        m_data[r].channel = ch;
        emit dataChanged(index(r, 0), index(r, Col_Count - 1));
    }
}

// =========================================================
// DELEGATE
// =========================================================
FormulaDelegate::FormulaDelegate(SourceEditMode mode, QObject *p)
    : QStyledItemDelegate(p)
    , m_mode(mode)
{}

QWidget *FormulaDelegate::createEditor(QWidget *p,
                                       const QStyleOptionViewItem &,
                                       const QModelIndex &idx) const
{
    auto self = const_cast<FormulaDelegate *>(this);

    // Кнопка удаления
    if (idx.column() == FormulaTableModel::Col_Remove) {
        auto *btn = new QPushButton(p);
        btn->setIcon(QApplication::style()->standardIcon(QStyle::SP_TitleBarCloseButton));
        btn->setStyleSheet("border: none; background: transparent;");
        auto m = const_cast<FormulaTableModel *>(
            qobject_cast<const FormulaTableModel *>(idx.model()));
        connect(btn, &QPushButton::clicked, self, [m, idx]() { m->removeChannel(idx.row()); });
        return btn;
    }

    // Режим Dialog: кнопка Edit
    if (m_mode == SourceEditMode::Dialog) {
        if (idx.column() == FormulaTableModel::Col_Action) {
            auto *btn = new QPushButton(p);
            btn->setIcon(QApplication::style()->standardIcon(QStyle::SP_FileDialogDetailedView));
            btn->setStyleSheet("border: none; background: transparent;");
            connect(btn, &QPushButton::clicked, self, [self, idx]() {
                emit self->editRequested(idx.row());
            });
            return btn;
        }
        return nullptr; // Остальное не редактируется
    }

    // Режим Inline
    const FormulaTableModel *model = qobject_cast<const FormulaTableModel *>(idx.model());
    if (model && !model->isRowEnabled(idx.row()))
        return nullptr;

    if (idx.column() == FormulaTableModel::Col_ID) {
        auto *le = new QLineEdit(p);
        le->setValidator(new QRegExpValidator(ID_REGEX, le));
        connect(le, &QLineEdit::editingFinished, self, [self, le, idx, model]() {
            if (!le->isVisible())
                return;
            QString t = le->text();
            QString old = model->data(idx, Qt::DisplayRole).toString();
            if (t == old)
                return;
            if (t.isEmpty()) {
                QMessageBox::warning(le, "Error", "Empty ID");
                le->setText(old);
                return;
            }
            if (!model->isIdUnique(t, idx.row())) {
                QMessageBox::warning(le, "Error", "Duplicate ID");
                le->setText(old);
                return;
            }
            emit self->commitData(le);
        });
        return le;
    }

    // [FIX] Inline Formula Editor (Widget + Icon)
    if (idx.column() == FormulaTableModel::Col_Formula) {
        QString currentFormula = idx.model()->data(idx, Qt::DisplayRole).toString();
        auto *w = new FormulaInlineWidget(currentFormula, p);
        // При клике на иконку в виджете - эмитим сигнал для открытия скрипт-диалога
        connect(w, &FormulaInlineWidget::editClicked, self, [self, idx]() {
            emit self->scriptRequested(idx.row());
        });
        return w;
    }

    if (idx.column() == FormulaTableModel::Col_Category) {
        auto *cb = new QComboBox(p);
        fillCategoryCombo(cb);
        connect(cb, QOverload<int>::of(&QComboBox::currentIndexChanged), self, [self, cb]() {
            emit self->commitData(cb);
        });
        return cb;
    }
    if (idx.column() == FormulaTableModel::Col_Unit) {
        auto *cb = new QComboBox(p);
        QModelIndex catIdx = idx.sibling(idx.row(), FormulaTableModel::Col_Category);
        auto cat = (EvoUnit::UnitCategory) model->data(catIdx, Qt::EditRole).toInt();
        fillUnitsByCategory(cb, cat);
        connect(cb, QOverload<int>::of(&QComboBox::currentIndexChanged), self, [self, cb]() {
            emit self->commitData(cb);
        });
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
    } else if (auto *fw = qobject_cast<FormulaInlineWidget *>(e)) {
        fw->setText(idx.model()->data(idx, Qt::EditRole).toString());
    }
}
void FormulaDelegate::setModelData(QWidget *e, QAbstractItemModel *m, const QModelIndex &idx) const
{
    if (auto *le = qobject_cast<QLineEdit *>(e)) {
        if (!le->text().isEmpty())
            m->setData(idx, le->text(), Qt::EditRole);
    } else if (auto *cb = qobject_cast<QComboBox *>(e))
        m->setData(idx, cb->currentData(), Qt::EditRole);
    // FormulaInlineWidget не используется для сохранения (сохранение через диалог скрипта)
    // Но если бы мы хотели сохранять текст напрямую из lineedit виджета, то:
    // else if(auto *fw=qobject_cast<FormulaInlineWidget*>(e)) m->setData(idx, fw->text(), Qt::EditRole);
}

// =========================================================
// WIDGET
// =========================================================
FormulaConfigWidget::FormulaConfigWidget(Controller *c, SourceEditMode mode, QWidget *p)
    : QWidget(p)
    , m_controller(c)
    , m_mode(mode)
{
    auto *l = new QVBoxLayout(this);
    auto *tb = new QHBoxLayout();
    auto *btnAdd = new QPushButton("Add Formula");
    auto *btnApply = new QPushButton("Apply Scripts");
    btnApply->setStyleSheet("color: blue; font-weight: bold;");

    tb->addWidget(btnAdd);
    tb->addStretch();
    tb->addWidget(btnApply);
    l->addLayout(tb);

    m_model = new FormulaTableModel(m_mode, c, this);
    m_view = new QTableView(this);
    m_view->setModel(m_model);

    auto *delegate = new FormulaDelegate(m_mode, this);
    m_view->setItemDelegate(delegate);

    connect(delegate,
            &FormulaDelegate::scriptRequested,
            this,
            &FormulaConfigWidget::onScriptRequested);
    connect(delegate, &FormulaDelegate::editRequested, this, &FormulaConfigWidget::onEditRequested);

    m_view->setSelectionMode(QAbstractItemView::NoSelection);
    m_view->setFocusPolicy(Qt::NoFocus);
    m_view->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_view->horizontalHeader()->setSectionResizeMode(FormulaTableModel::Col_Action,
                                                     QHeaderView::ResizeToContents);
    m_view->horizontalHeader()->setSectionResizeMode(FormulaTableModel::Col_Remove,
                                                     QHeaderView::ResizeToContents);
    l->addWidget(m_view);

    connect(m_model, &QAbstractTableModel::rowsInserted, this, &FormulaConfigWidget::onRowsInserted);
    connect(m_model, &QAbstractTableModel::dataChanged, this, &FormulaConfigWidget::onDataChanged);

    connect(btnAdd, &QPushButton::clicked, this, &FormulaConfigWidget::onAddClicked);
    connect(btnApply, &QPushButton::clicked, this, &FormulaConfigWidget::onApplyClicked);

    if (m_controller)
        load();
}

void FormulaConfigWidget::onRowsInserted(const QModelIndex &, int first, int last)
{
    openEditorsForRange(first, last);
}

void FormulaConfigWidget::onDataChanged(const QModelIndex &tl, const QModelIndex &br)
{
    if (m_mode == SourceEditMode::Inline) {
        if (tl.column() <= FormulaTableModel::Col_Category
            && br.column() >= FormulaTableModel::Col_Category) {
            for (int r = tl.row(); r <= br.row(); ++r) {
                QModelIndex idx = m_model->index(r, FormulaTableModel::Col_Unit);
                m_view->closePersistentEditor(idx);
                m_view->openPersistentEditor(idx);
            }
        }
        if (tl.column() <= FormulaTableModel::Col_Action
            && br.column() >= FormulaTableModel::Col_Action) {
            for (int r = tl.row(); r <= br.row(); ++r)
                updateEditorsState(r);
        }
    }
}

void FormulaConfigWidget::updateEditorsState(int row)
{
    bool enabled = m_model->isRowEnabled(row);
    if (m_mode == SourceEditMode::Dialog) {
        m_view->openPersistentEditor(m_model->index(row, FormulaTableModel::Col_Action));
        m_view->openPersistentEditor(m_model->index(row, FormulaTableModel::Col_Remove));
        return;
    }
    for (int col = 1; col < FormulaTableModel::Col_Count; ++col) {
        QModelIndex idx = m_model->index(row, col);
        if (col == FormulaTableModel::Col_Remove) {
            m_view->openPersistentEditor(idx);
        } else {
            if (enabled)
                m_view->openPersistentEditor(idx);
            else
                m_view->closePersistentEditor(idx);
        }
    }
}

void FormulaConfigWidget::openEditorsForRange(int first, int last)
{
    for (int r = first; r <= last; ++r)
        updateEditorsState(r);
}

void FormulaConfigWidget::onAddClicked()
{
    if (m_mode == SourceEditMode::Dialog) {
        FormulaAddDialog d(this);
        int cnt = m_model->rowCount(QModelIndex()) + 1;
        ComputedChannel def;
        do {
            def.id = "calc_" + std::to_string(cnt++);
        } while (!m_model->isIdUnique(QString::fromStdString(def.id)));
        def.formula = "return IO.val('source_id') * 1.0;";
        def.unit = EvoUnit::MeasUnit::Dimensionless;
        d.setChannel(def);

        while (true) {
            if (d.exec() == QDialog::Accepted) {
                QString err;
                ComputedChannel ch = d.getChannel();
                if (m_model->addChannel(ch, err))
                    break;
                else
                    QMessageBox::warning(this, "Error", err);
            } else
                break;
        }
    } else {
        ComputedChannel ch;
        int cnt = m_model->rowCount(QModelIndex()) + 1;
        do {
            ch.id = "calc_" + std::to_string(cnt++);
        } while (!m_model->isIdUnique(QString::fromStdString(ch.id)));
        ch.formula = "return 0;";
        ch.unit = EvoUnit::MeasUnit::Dimensionless;

        QString err;
        if (!m_model->addChannel(ch, err))
            QMessageBox::warning(this, "Error", err);
    }
}

void FormulaConfigWidget::onScriptRequested(int row)
{
    ComputedChannel ch = m_model->getChannelAt(row);
    ScriptEditDialog d(ch.formula, this);
    if (d.exec() == QDialog::Accepted) {
        ch.formula = d.getScript();
        m_model->updateChannel(row, ch);

        // [FIX] Обновляем отображение в виджете если он открыт
        if (m_mode == SourceEditMode::Inline) {
            // Перезагрузка persistent editor для формулы
            QModelIndex idx = m_model->index(row, FormulaTableModel::Col_Formula);
            m_view->closePersistentEditor(idx);
            m_view->openPersistentEditor(idx);
        }
    }
}

void FormulaConfigWidget::onEditRequested(int row)
{
    if (m_mode != SourceEditMode::Dialog)
        return;
    FormulaAddDialog d(this);
    d.setChannel(m_model->getChannelAt(row));
    d.setIdEditable(true);
    while (true) {
        if (d.exec() == QDialog::Accepted) {
            ComputedChannel ch = d.getChannel();
            // Check unique excluding self
            if (m_model->isIdUnique(QString::fromStdString(ch.id), row)) {
                m_model->updateChannel(row, ch);
                break;
            } else
                QMessageBox::warning(this, "Error", "Duplicate ID");
        } else
            break;
    }
}

void FormulaConfigWidget::onRemoveClicked() {}

void FormulaConfigWidget::load()
{
    if (m_controller) {
        m_model->setChannels(m_controller->getComputedChannels());
        openEditorsForRange(0, m_model->rowCount(QModelIndex()) - 1);
    }
}

void FormulaConfigWidget::apply()
{
    m_controller->clearComputedChannels();
    for (const auto &c : m_model->getChannels())
        m_controller->addComputedChannel(c);
    m_controller->buildAndApplyScript();
}

void FormulaConfigWidget::onApplyClicked()
{
    apply();
    QMessageBox::information(this, "Info", "Formulas Compiled & Applied");
}

} // namespace EvoGui
