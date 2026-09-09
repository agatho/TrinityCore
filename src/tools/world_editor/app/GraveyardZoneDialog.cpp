#include "GraveyardZoneDialog.h"

#include "ConfirmSqlDialog.h"

#include <QApplication>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSortFilterProxyModel>
#include <QSpinBox>
#include <QStandardItemModel>
#include <QTableView>
#include <QTimer>
#include <QVBoxLayout>

#include <cstdio>

namespace world_editor::app
{

namespace
{
constexpr int COL_ID         = 0;
constexpr int COL_GHOST_ZONE = 1;
constexpr int COL_COMMENT    = 2;

QString escString(db::MySqlClient* c, QString const& v)
{
    if (!c) return v;
    return QString::fromStdString(c->escapeString(v.toStdString()));
}
} // namespace

GraveyardZoneDialog::GraveyardZoneDialog(db::MySqlClient* dbClient, QWidget* parent)
    : QDialog(parent), m_dbClient(dbClient)
{
    setWindowTitle(tr("graveyard_zone editor"));
    setModal(true);
    resize(720, 600);

    m_filterEdit = new QLineEdit(this);
    m_filterEdit->setPlaceholderText(tr("Filter by ID, GhostZone, or comment..."));
    m_view = new QTableView(this);
    m_view->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_view->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_view->setSelectionMode(QAbstractItemView::SingleSelection);
    m_view->setSortingEnabled(true);
    m_statusLbl = new QLabel(tr("loading..."), this);

    m_model = new QStandardItemModel(this);
    m_model->setHorizontalHeaderLabels(
        { tr("ID (world_safe_locs)"), tr("GhostZone"), tr("Comment") });

    m_proxy = new QSortFilterProxyModel(this);
    m_proxy->setFilterCaseSensitivity(Qt::CaseInsensitive);
    m_proxy->setFilterKeyColumn(-1);
    m_proxy->setSourceModel(m_model);
    m_view->setModel(m_proxy);
    m_view->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_view->horizontalHeader()->setStretchLastSection(true);

    auto* addBtn    = new QPushButton(tr("Add row..."),    this);
    auto* removeBtn = new QPushButton(tr("Remove selected"), this);
    auto* closeBtn  = new QPushButton(tr("Close"),         this);

    auto* topBar = new QHBoxLayout;
    topBar->addWidget(m_filterEdit, 1);
    topBar->addWidget(addBtn);
    topBar->addWidget(removeBtn);

    auto* outer = new QVBoxLayout(this);
    outer->addLayout(topBar);
    outer->addWidget(m_view, 1);
    outer->addWidget(m_statusLbl);

    auto* footer = new QHBoxLayout;
    footer->addStretch(1);
    footer->addWidget(closeBtn);
    outer->addLayout(footer);

    connect(m_filterEdit, &QLineEdit::textChanged, this, &GraveyardZoneDialog::onFilterTextChanged);
    connect(addBtn,       &QPushButton::clicked,  this, &GraveyardZoneDialog::onAddClicked);
    connect(removeBtn,    &QPushButton::clicked,  this, &GraveyardZoneDialog::onRemoveClicked);
    connect(closeBtn,     &QPushButton::clicked,  this, &QDialog::accept);

    QTimer::singleShot(0, this, [this]{ reload(); });
}

void GraveyardZoneDialog::reload()
{
    if (!m_dbClient || !m_dbClient->isConnected())
    {
        m_statusLbl->setText(tr("not connected to DB"));
        return;
    }
    char const* sql =
        "SELECT ID, GhostZone, COALESCE(Comment, '') "
        "FROM graveyard_zone ORDER BY ID, GhostZone";
    db::QueryResult res;
    auto const err = m_dbClient->query(sql, res);
    if (!err.ok())
    {
        m_statusLbl->setText(tr("query failed: %1").arg(QString::fromStdString(err.message)));
        return;
    }
    QApplication::setOverrideCursor(Qt::WaitCursor);
    m_model->setRowCount(int(res.rowCount()));
    for (size_t r = 0; r < res.rowCount(); ++r)
    {
        uint32_t const id        = uint32_t(res.asUInt64(r, 0).value_or(0));
        uint32_t const ghostZone = uint32_t(res.asUInt64(r, 1).value_or(0));
        QString  const comment   = QString::fromStdString(res.cell(r, 2));
        m_model->setItem(int(r), COL_ID,         new QStandardItem(QString::number(id)));
        m_model->setItem(int(r), COL_GHOST_ZONE, new QStandardItem(QString::number(ghostZone)));
        m_model->setItem(int(r), COL_COMMENT,    new QStandardItem(comment));
    }
    QApplication::restoreOverrideCursor();
    m_statusLbl->setText(tr("loaded %1 rows").arg(res.rowCount()));
}

void GraveyardZoneDialog::onFilterTextChanged(QString const& text)
{
    m_proxy->setFilterFixedString(text);
}

void GraveyardZoneDialog::onAddClicked()
{
    if (!m_dbClient || !m_dbClient->isConnected())
        return;
    // Small composite-form dialog: ID + GhostZone + Comment.
    QDialog form(this);
    form.setWindowTitle(tr("Add graveyard_zone row"));
    form.setModal(true);
    auto* idSpin = new QSpinBox(&form);
    idSpin->setRange(1, 0x7fffffff);
    idSpin->setToolTip(tr("world_safe_locs.ID (which graveyard players respawn at)"));
    auto* zoneSpin = new QSpinBox(&form);
    zoneSpin->setRange(1, 0x7fffffff);
    zoneSpin->setToolTip(tr("Zone ID whose ghosts respawn at the above graveyard"));
    auto* commentEdit = new QLineEdit(&form);
    commentEdit->setMaxLength(255);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &form);
    auto* fl = new QFormLayout(&form);
    fl->addRow(tr("ID (world_safe_locs.ID)"), idSpin);
    fl->addRow(tr("GhostZone"),                zoneSpin);
    fl->addRow(tr("Comment"),                  commentEdit);
    fl->addRow(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &form, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &form, &QDialog::reject);
    if (form.exec() != QDialog::Accepted)
        return;

    int const id     = idSpin->value();
    int const zone   = zoneSpin->value();
    QString const cm = commentEdit->text();
    QString const sql = QString(
        "INSERT INTO graveyard_zone (ID, GhostZone, Comment) "
        "VALUES (%1, %2, '%3');")
        .arg(id).arg(zone).arg(escString(m_dbClient, cm));
    QString const summary = tr("Add graveyard_zone row (ID=%1, GhostZone=%2)").arg(id).arg(zone);
    ConfirmSqlDialog dlg(m_dbClient, summary, sql, this);
    if (dlg.exec() == QDialog::Accepted && dlg.applied())
        reload();
}

void GraveyardZoneDialog::onRemoveClicked()
{
    if (!m_dbClient || !m_dbClient->isConnected())
        return;
    QModelIndex const proxyIdx = m_view->currentIndex();
    if (!proxyIdx.isValid())
    {
        QMessageBox::information(this, tr("No row"),
            tr("Select a row to remove."));
        return;
    }
    QModelIndex const srcIdx = m_proxy->mapToSource(proxyIdx);
    auto* idItem   = m_model->item(srcIdx.row(), COL_ID);
    auto* zoneItem = m_model->item(srcIdx.row(), COL_GHOST_ZONE);
    if (!idItem || !zoneItem) return;
    bool okId = false, okZone = false;
    int const id   = idItem->text().toInt(&okId);
    int const zone = zoneItem->text().toInt(&okZone);
    if (!okId || !okZone) return;
    QString const sql = QString(
        "DELETE FROM graveyard_zone WHERE ID=%1 AND GhostZone=%2;").arg(id).arg(zone);
    QString const summary = tr("Remove graveyard_zone row (ID=%1, GhostZone=%2)").arg(id).arg(zone);
    ConfirmSqlDialog dlg(m_dbClient, summary, sql, this);
    if (dlg.exec() == QDialog::Accepted && dlg.applied())
        reload();
}

} // namespace world_editor::app
