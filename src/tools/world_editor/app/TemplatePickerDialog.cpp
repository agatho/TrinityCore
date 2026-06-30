#include "TemplatePickerDialog.h"

#include "../db/TemplateLookup.h"

#include <QApplication>
#include <QDialogButtonBox>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QPushButton>
#include <QSortFilterProxyModel>
#include <QStandardItemModel>
#include <QTabWidget>
#include <QTimer>
#include <QVBoxLayout>

#include <cstdio>

namespace world_editor::app
{

namespace
{
constexpr int COL_ENTRY = 0;
constexpr int COL_NAME  = 1;
} // namespace

TemplatePickerDialog::TemplatePickerDialog(db::MySqlClient* dbClient,
                                           QString const& worldDbName,
                                           QWidget* parent)
    : QDialog(parent), m_dbClient(dbClient), m_worldDb(worldDbName)
{
    setWindowTitle(tr("Pick a template"));
    setModal(true);
    resize(560, 600);

    m_tabs       = new QTabWidget(this);
    m_filterEdit = new QLineEdit(this);
    m_filterEdit->setPlaceholderText(tr("Filter by entry# or name..."));
    m_view       = new QListView(this);
    m_view->setUniformItemSizes(true);
    m_view->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_statusLbl  = new QLabel(tr("loading..."), this);

    m_creatureModel = new QStandardItemModel(this);
    m_creatureModel->setHorizontalHeaderLabels({ tr("Entry"), tr("Name") });
    m_goModel       = new QStandardItemModel(this);
    m_goModel->setHorizontalHeaderLabels({ tr("Entry"), tr("Name") });

    m_proxy = new QSortFilterProxyModel(this);
    m_proxy->setFilterCaseSensitivity(Qt::CaseInsensitive);
    m_proxy->setFilterKeyColumn(-1); // any column (combined display AND pure name)
    m_view->setModel(m_proxy);
    // QListView renders column 0; the combined "id · name" display is put there
    // (see loadTemplates) so the name is always visible without depending on
    // setModelColumn (which is ignored before the proxy has a source model).

    m_tabs->addTab(new QWidget(this), tr("Creature"));
    m_tabs->addTab(new QWidget(this), tr("GameObject"));

    auto* buttons = new QDialogButtonBox(this);
    m_okButton = buttons->addButton(tr("Use selected"), QDialogButtonBox::AcceptRole);
    m_okButton->setEnabled(false);
    buttons->addButton(QDialogButtonBox::Cancel);

    connect(m_tabs,       &QTabWidget::currentChanged,  this, &TemplatePickerDialog::onTabChanged);
    connect(m_filterEdit, &QLineEdit::textChanged,      this, &TemplatePickerDialog::onFilterTextChanged);
    connect(m_view,       &QListView::doubleClicked,    this, &TemplatePickerDialog::onDoubleClicked);
    connect(m_view->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, [this](QItemSelection const&, QItemSelection const&) {
                m_okButton->setEnabled(m_view->selectionModel()->hasSelection());
            });
    connect(buttons,      &QDialogButtonBox::accepted,  this, &TemplatePickerDialog::onAccept);
    connect(buttons,      &QDialogButtonBox::rejected,  this, &QDialog::reject);

    auto* outer = new QVBoxLayout(this);
    outer->addWidget(m_tabs);
    outer->addWidget(m_filterEdit);
    outer->addWidget(m_view, 1);
    outer->addWidget(m_statusLbl);
    outer->addWidget(buttons);

    QTimer::singleShot(0, this, &TemplatePickerDialog::loadTemplates);
}

void TemplatePickerDialog::loadTemplates()
{
    if (m_loaded) return;
    m_loaded = true;

    // Use the shared TemplateLookup service instead of an inline SELECT, so the
    // template query lives in one place (db/TemplateLookup) reused by every
    // picker + the spawn-placement FK gate.
    db::TemplateLookup lk(m_dbClient, m_worldDb.toStdString());

    auto fill = [&](db::TemplateLookup::Table table, QStandardItemModel* model) -> size_t
    {
        std::vector<db::TemplateLookup::Row> const rows = lk.loadAll(table);
        model->setRowCount(int(rows.size()));
        for (size_t r = 0; r < rows.size(); ++r)
        {
            uint32_t const entry = rows[r].entry;
            QString  const name  = QString::fromStdString(rows[r].name);

            // id first, then name (e.g. "448  ·  Hogger"). The combined display
            // goes in column 0 (the one the QListView renders); the entry id +
            // PURE name are kept in roles for selection. A nameless row shows
            // the id alone.
            QString const display = name.isEmpty()
                ? QString::number(entry)
                : QStringLiteral("%1   ·   %2").arg(entry).arg(name);
            auto* displayItem = new QStandardItem(display);
            displayItem->setData(static_cast<qulonglong>(entry), Qt::UserRole);     // entry id
            displayItem->setData(name,                           Qt::UserRole + 1); // pure name
            model->setItem(int(r), COL_ENTRY, displayItem);

            // Column 1 carries the pure name too, so the all-columns filter
            // matches names cleanly (and a future table view could show it).
            model->setItem(int(r), COL_NAME, new QStandardItem(name));
        }
        return rows.size();
    };

    QApplication::setOverrideCursor(Qt::WaitCursor);
    size_t const nCreature = fill(db::TemplateLookup::Table::Creature,   m_creatureModel);
    size_t const nGo       = fill(db::TemplateLookup::Table::GameObject, m_goModel);
    QApplication::restoreOverrideCursor();

    m_statusLbl->setText(tr("creature_template=%1  gameobject_template=%2")
        .arg(nCreature).arg(nGo));
    populateModel(render::SpawnKind::Creature);
}

void TemplatePickerDialog::onTabChanged(int idx)
{
    if (idx == 0) populateModel(render::SpawnKind::Creature);
    else          populateModel(render::SpawnKind::GameObject);
}

void TemplatePickerDialog::populateModel(render::SpawnKind kind)
{
    m_proxy->setSourceModel(kind == render::SpawnKind::Creature ? m_creatureModel : m_goModel);
    m_filterEdit->clear();
    m_view->clearSelection();
    m_okButton->setEnabled(false);
}

void TemplatePickerDialog::onFilterTextChanged(QString const& text)
{
    m_proxy->setFilterFixedString(text);
}

void TemplatePickerDialog::onDoubleClicked(QModelIndex const& /*idx*/)
{
    onAccept();
}

PickedTemplate TemplatePickerDialog::currentSelection() const
{
    PickedTemplate out;
    QModelIndex const proxyIdx = m_view->currentIndex();
    if (!proxyIdx.isValid()) return out;
    QModelIndex const srcIdx = m_proxy->mapToSource(proxyIdx);
    int const row = srcIdx.row();
    QStandardItemModel const* model = (m_tabs->currentIndex() == 0) ? m_creatureModel : m_goModel;
    out.kind  = (m_tabs->currentIndex() == 0) ? render::SpawnKind::Creature : render::SpawnKind::GameObject;
    QStandardItem const* item = model->item(row, COL_ENTRY); // column 0 carries the roles
    out.entry = static_cast<uint32_t>(item->data(Qt::UserRole).toULongLong());
    out.name  = item->data(Qt::UserRole + 1).toString();     // pure name, not the "id · name" display
    return out;
}

void TemplatePickerDialog::onAccept()
{
    m_picked = currentSelection();
    if (m_picked.entry == 0)
        return;
    accept();
}

} // namespace world_editor::app
