#include "DisablesEditDialog.h"

#include "../db/MySqlClient.h"

#include <QApplication>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

#include <array>
#include <cstdio>
#include <limits>

namespace world_editor::app
{

namespace
{

// Friendly label for TC's Disables::DisableType.  Mirrors the enum in
// src/server/game/Conditions/DisableMgr.cpp.  Any value outside [0..9]
// renders as "Unknown (N)" so the operator can still see the raw int.
struct SourceTypeLabel { int value; char const* label; };
constexpr std::array<SourceTypeLabel, 10> kSourceTypes = { {
    { 0, "Spell" },
    { 1, "Quest" },
    { 2, "Map" },
    { 3, "Battleground" },
    { 4, "Achievement criteria" },
    { 5, "Outdoor PvP" },
    { 6, "VMAP (LOS/heights)" },
    { 7, "Game event" },
    { 8, "Loot template" },
    { 9, "MMAP" },
} };

QString prettySourceType(uint32_t t)
{
    for (auto const& s : kSourceTypes)
        if (uint32_t(s.value) == t) return QString::fromUtf8(s.label);
    return QStringLiteral("Unknown (%1)").arg(t);
}

} // namespace

DisablesEditDialog::DisablesEditDialog(db::MySqlClient* dbClient,
                                       QString const& worldDbName,
                                       QWidget* parent)
    : QDialog(parent), m_db(dbClient), m_worldDb(worldDbName)
{
    setWindowTitle(tr("Disables editor (kill-switch table)"));
    setModal(true);
    resize(1000, 620);

    auto* outer = new QVBoxLayout(this);

    // --- Top: sourceType filter --------------------------------------
    auto* filterRow = new QHBoxLayout;
    filterRow->addWidget(new QLabel(tr("Source type:"), this));
    m_sourceTypeFilter = new QComboBox(this);
    // Special "(all)" sentinel uses Qt::UserRole = -1 so loadRows() knows
    // not to apply a WHERE clause.
    m_sourceTypeFilter->addItem(tr("(all)"), int(-1));
    for (auto const& s : kSourceTypes)
        m_sourceTypeFilter->addItem(QStringLiteral("%1 - %2").arg(s.value).arg(QString::fromUtf8(s.label)), s.value);
    filterRow->addWidget(m_sourceTypeFilter, 1);

    m_applyBtn = new QPushButton(tr("Apply"), this);
    filterRow->addWidget(m_applyBtn);
    m_refreshBtn = new QPushButton(tr("Refresh"), this);
    filterRow->addWidget(m_refreshBtn);
    filterRow->addStretch(1);
    outer->addLayout(filterRow);

    // --- Bottom: rows + toolbar --------------------------------------
    m_table = new QTableWidget(this);
    m_table->setColumnCount(6);
    m_table->setHorizontalHeaderLabels({
        tr("sourceType"), tr("entry"), tr("flags"),
        tr("params_0"), tr("params_1"), tr("comment") });
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->verticalHeader()->setVisible(false);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setSortingEnabled(true);
    outer->addWidget(m_table, 1);

    auto* toolbar = new QHBoxLayout;
    m_addBtn    = new QPushButton(tr("Add disable..."), this);
    m_editBtn   = new QPushButton(tr("Edit row"), this);
    m_removeBtn = new QPushButton(tr("Remove disable"), this);
    m_editBtn  ->setEnabled(false);
    m_removeBtn->setEnabled(false);
    toolbar->addWidget(m_addBtn);
    toolbar->addWidget(m_editBtn);
    toolbar->addWidget(m_removeBtn);
    toolbar->addStretch(1);
    outer->addLayout(toolbar);

    m_statusLabel = new QLabel(tr("Press Apply or Refresh to load."), this);
    outer->addWidget(m_statusLabel);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    outer->addWidget(buttons);

    // Wire signals.
    connect(m_applyBtn,   &QPushButton::clicked, this, &DisablesEditDialog::onApplyFilter);
    connect(m_refreshBtn, &QPushButton::clicked, this, &DisablesEditDialog::onRefresh);
    connect(m_addBtn,     &QPushButton::clicked, this, &DisablesEditDialog::onAddDisable);
    connect(m_editBtn,    &QPushButton::clicked, this, &DisablesEditDialog::onEditRow);
    connect(m_removeBtn,  &QPushButton::clicked, this, &DisablesEditDialog::onRemoveDisable);
    connect(m_table,      &QTableWidget::itemSelectionChanged,
            this, &DisablesEditDialog::onSelectionChanged);

    // Initial load: show all rows.
    loadRows();
}

QString DisablesEditDialog::resolveEntryColumn() const
{
    if (!m_entryCol.isEmpty()) return m_entryCol;
    if (!m_db || !m_db->isConnected()) return QString();
    // Probe INFORMATION_SCHEMA for the actual column name used by this
    // schema: modern TC = `entry`, some legacy snapshots = `entryID`.
    char sql[512];
    std::snprintf(sql, sizeof(sql),
        "SELECT COLUMN_NAME FROM INFORMATION_SCHEMA.COLUMNS "
        "WHERE TABLE_SCHEMA='%s' AND TABLE_NAME='disables' "
        "AND COLUMN_NAME IN ('entry','entryID')",
        m_worldDb.toStdString().c_str());
    db::QueryResult res;
    auto const err = m_db->query(sql, res);
    if (!err.ok() || res.rowCount() == 0)
    {
        // Default to `entry`; SELECT will surface an error if the column
        // is actually missing.
        m_entryCol = QStringLiteral("entry");
        return m_entryCol;
    }
    // Prefer `entry` when both exist (shouldn't happen).
    for (size_t r = 0; r < res.rowCount(); ++r)
    {
        if (res.cell(r, 0) == "entry") { m_entryCol = QStringLiteral("entry"); return m_entryCol; }
    }
    m_entryCol = QString::fromStdString(res.cell(0, 0));
    return m_entryCol;
}

void DisablesEditDialog::onApplyFilter()
{
    loadRows();
}

void DisablesEditDialog::onRefresh()
{
    loadRows();
}

void DisablesEditDialog::onSelectionChanged()
{
    bool const hasSelection = m_table->currentRow() >= 0;
    bool const connected    = m_db && m_db->isConnected();
    m_editBtn  ->setEnabled(hasSelection && connected);
    m_removeBtn->setEnabled(hasSelection && connected);
}

void DisablesEditDialog::loadRows()
{
    m_table->setRowCount(0);
    if (!m_db || !m_db->isConnected())
    {
        m_statusLabel->setText(tr("DB not connected."));
        return;
    }

    QString const entryCol = resolveEntryColumn();
    if (entryCol.isEmpty())
    {
        m_statusLabel->setText(tr("Could not resolve disables.entry column."));
        return;
    }

    int const filter = m_sourceTypeFilter->currentData().toInt();
    QString whereClause;
    if (filter >= 0)
        whereClause = QStringLiteral(" WHERE sourceType=%1").arg(filter);

    QApplication::setOverrideCursor(Qt::WaitCursor);
    // COALESCE the optional params/comment columns so legacy schemas (no
    // params_0/params_1) don't blow up the SELECT.  We still probe for
    // params_0 presence via INFORMATION_SCHEMA so a missing column
    // degrades gracefully to empty strings.
    char sql[1024];
    std::snprintf(sql, sizeof(sql),
        "SELECT sourceType, %s AS entry_id, flags, "
        "       COALESCE(params_0, '') AS params_0, "
        "       COALESCE(params_1, '') AS params_1, "
        "       COALESCE(comment,  '') AS comment "
        "FROM %s.disables%s "
        "ORDER BY sourceType, %s",
        entryCol.toStdString().c_str(),
        m_worldDb.toStdString().c_str(),
        whereClause.toStdString().c_str(),
        entryCol.toStdString().c_str());
    db::QueryResult res;
    auto err = m_db->query(sql, res);
    if (!err.ok())
    {
        // Retry without params_0/params_1 in case the legacy schema lacks
        // them (e.g. very old 3.3.5 snapshot with only flags + comment).
        std::snprintf(sql, sizeof(sql),
            "SELECT sourceType, %s AS entry_id, flags, "
            "       '' AS params_0, '' AS params_1, "
            "       COALESCE(comment, '') AS comment "
            "FROM %s.disables%s "
            "ORDER BY sourceType, %s",
            entryCol.toStdString().c_str(),
            m_worldDb.toStdString().c_str(),
            whereClause.toStdString().c_str(),
            entryCol.toStdString().c_str());
        err = m_db->query(sql, res);
    }
    QApplication::restoreOverrideCursor();
    if (!err.ok())
    {
        m_statusLabel->setText(tr("Query failed: %1").arg(QString::fromStdString(err.message)));
        return;
    }

    m_table->setSortingEnabled(false);
    m_table->setRowCount(int(res.rowCount()));
    for (size_t r = 0; r < res.rowCount(); ++r)
    {
        uint64_t const stype  = res.asUInt64(r, 0).value_or(0);
        uint64_t const entry  = res.asUInt64(r, 1).value_or(0);
        int64_t  const flags  = res.asInt64 (r, 2).value_or(0);
        QString  const p0     = QString::fromStdString(res.cell(r, 3));
        QString  const p1     = QString::fromStdString(res.cell(r, 4));
        QString  const cmt    = QString::fromStdString(res.cell(r, 5));

        // Column 0 (sourceType) shows the pretty label but stashes the raw
        // int on UserRole so currentRowKey() can recover the PK without
        // round-tripping through the label.
        auto* stypeCell = new QTableWidgetItem(prettySourceType(uint32_t(stype)));
        stypeCell->setData(Qt::UserRole, qulonglong(stype));
        m_table->setItem(int(r), 0, stypeCell);

        auto* entryCell = new QTableWidgetItem;
        entryCell->setData(Qt::DisplayRole, qulonglong(entry));
        m_table->setItem(int(r), 1, entryCell);

        auto* flagsCell = new QTableWidgetItem;
        flagsCell->setData(Qt::DisplayRole, qlonglong(flags));
        m_table->setItem(int(r), 2, flagsCell);

        m_table->setItem(int(r), 3, new QTableWidgetItem(p0));
        m_table->setItem(int(r), 4, new QTableWidgetItem(p1));
        m_table->setItem(int(r), 5, new QTableWidgetItem(cmt));
    }
    m_table->setSortingEnabled(true);

    if (filter >= 0)
        m_statusLabel->setText(tr("rows=%1 (sourceType=%2)").arg(res.rowCount()).arg(filter));
    else
        m_statusLabel->setText(tr("rows=%1 (all sourceTypes)").arg(res.rowCount()));
}

bool DisablesEditDialog::currentRowKey(uint32_t& sourceTypeOut, uint32_t& entryOut) const
{
    int const row = m_table->currentRow();
    if (row < 0) return false;
    auto* stypeCell = m_table->item(row, 0);
    auto* entryCell = m_table->item(row, 1);
    if (!stypeCell || !entryCell) return false;
    sourceTypeOut = uint32_t(stypeCell->data(Qt::UserRole).toULongLong());
    entryOut      = uint32_t(entryCell->data(Qt::DisplayRole).toULongLong());
    return true;
}

bool DisablesEditDialog::runInTransaction(QString const& sql, QString const& description, uint64_t* affectedOut)
{
    if (!m_db || !m_db->isConnected())
    {
        QMessageBox::warning(this, tr("Not connected"), tr("World DB not open."));
        return false;
    }
    auto err = m_db->exec("START TRANSACTION");
    if (!err.ok())
    {
        QMessageBox::critical(this, tr("Transaction failed"),
            tr("BEGIN failed: %1").arg(QString::fromStdString(err.message)));
        return false;
    }
    uint64_t affected = 0;
    err = m_db->exec(sql.toStdString(), &affected);
    if (!err.ok())
    {
        (void)m_db->exec("ROLLBACK");
        QMessageBox::critical(this, tr("DML failed"),
            tr("%1\n\n%2").arg(description).arg(QString::fromStdString(err.message)));
        return false;
    }
    err = m_db->exec("COMMIT");
    if (!err.ok())
    {
        (void)m_db->exec("ROLLBACK");
        QMessageBox::critical(this, tr("Commit failed"),
            tr("%1\n\n%2").arg(description).arg(QString::fromStdString(err.message)));
        return false;
    }
    if (affectedOut) *affectedOut = affected;
    m_statusLabel->setText(tr("%1 (affected=%2)").arg(description).arg(qulonglong(affected)));
    return true;
}

void DisablesEditDialog::onAddDisable()
{
    openRowModal(/*editKey=*/nullptr);
}

void DisablesEditDialog::onEditRow()
{
    RowKey key{};
    if (!currentRowKey(key.sourceType, key.entry))
        return;
    openRowModal(&key);
}

void DisablesEditDialog::onRemoveDisable()
{
    uint32_t stype = 0, entry = 0;
    if (!currentRowKey(stype, entry))
        return;
    if (QMessageBox::question(this, tr("Remove disable"),
            tr("Delete disables row sourceType=%1 (%2) entry=%3?")
                .arg(stype).arg(prettySourceType(stype)).arg(entry),
            QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
        return;

    QString const entryCol = resolveEntryColumn();
    if (entryCol.isEmpty())
    {
        QMessageBox::warning(this, tr("Remove disable"),
            tr("Could not resolve disables.entry column."));
        return;
    }

    char del[384];
    std::snprintf(del, sizeof(del),
        "DELETE FROM %s.disables WHERE sourceType=%u AND %s=%u",
        m_worldDb.toStdString().c_str(), stype,
        entryCol.toStdString().c_str(), entry);
    if (runInTransaction(QString::fromUtf8(del),
            tr("DELETE disables (sourceType=%1, entry=%2)").arg(stype).arg(entry)))
        loadRows();
}

void DisablesEditDialog::openRowModal(RowKey const* editKey)
{
    // Local modal for INSERT / UPDATE.  Kept inline (no separate file)
    // because the field set is small (6 fields) - matches the NPC vendor
    // editor's pattern.
    QDialog dlg(this);
    dlg.setWindowTitle(editKey ? tr("Edit disables row") : tr("Add disables row"));
    dlg.setModal(true);

    auto* form = new QFormLayout(&dlg);

    auto* stypeCombo = new QComboBox(&dlg);
    for (auto const& s : kSourceTypes)
        stypeCombo->addItem(QStringLiteral("%1 - %2").arg(s.value).arg(QString::fromUtf8(s.label)), s.value);

    auto* entrySpin = new QSpinBox(&dlg);
    entrySpin->setRange(0, std::numeric_limits<int>::max());

    auto* flagsSpin = new QSpinBox(&dlg);
    flagsSpin->setRange(std::numeric_limits<int16_t>::min(), std::numeric_limits<int16_t>::max());

    auto* p0Edit = new QLineEdit(&dlg);
    p0Edit->setMaxLength(255);
    auto* p1Edit = new QLineEdit(&dlg);
    p1Edit->setMaxLength(255);
    auto* commentEdit = new QLineEdit(&dlg);
    commentEdit->setMaxLength(255);

    form->addRow(tr("sourceType:"), stypeCombo);
    form->addRow(tr("entry:"),      entrySpin);
    form->addRow(tr("flags:"),      flagsSpin);
    form->addRow(tr("params_0:"),   p0Edit);
    form->addRow(tr("params_1:"),   p1Edit);
    form->addRow(tr("comment:"),    commentEdit);

    if (editKey)
    {
        // Pre-populate from the selected table row.  Key fields stay
        // editable but the WHERE clause below uses the ORIGINAL editKey
        // so a key edit becomes a PK-changing UPDATE.
        int const row = m_table->currentRow();
        if (row >= 0)
        {
            uint32_t const stype = uint32_t(m_table->item(row, 0)->data(Qt::UserRole).toULongLong());
            uint32_t const entry = uint32_t(m_table->item(row, 1)->data(Qt::DisplayRole).toULongLong());
            int      const flags = int     (m_table->item(row, 2)->data(Qt::DisplayRole).toLongLong());
            int idx = stypeCombo->findData(int(stype));
            if (idx < 0) // Unknown sourceType - append it so the operator can re-save without losing the value.
            {
                stypeCombo->addItem(QStringLiteral("Unknown (%1)").arg(stype), int(stype));
                idx = stypeCombo->count() - 1;
            }
            stypeCombo->setCurrentIndex(idx);
            entrySpin->setValue(int(entry));
            flagsSpin->setValue(flags);
            p0Edit     ->setText(m_table->item(row, 3) ? m_table->item(row, 3)->text() : QString());
            p1Edit     ->setText(m_table->item(row, 4) ? m_table->item(row, 4)->text() : QString());
            commentEdit->setText(m_table->item(row, 5) ? m_table->item(row, 5)->text() : QString());
        }
    }
    else
    {
        // Default Add: seed combo from the current filter when it's a
        // concrete type, so the operator doesn't re-pick it.
        int const filter = m_sourceTypeFilter->currentData().toInt();
        if (filter >= 0)
        {
            int const idx = stypeCombo->findData(filter);
            if (idx >= 0) stypeCombo->setCurrentIndex(idx);
        }
    }

    auto* btns = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    connect(btns, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(btns, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    form->addRow(btns);

    if (dlg.exec() != QDialog::Accepted)
        return;

    uint32_t const stype = uint32_t(stypeCombo->currentData().toInt());
    uint32_t const entry = uint32_t(entrySpin->value());
    int      const flags = flagsSpin->value();

    if (entry == 0)
    {
        QMessageBox::warning(this, tr("Invalid entry"), tr("entry must be non-zero."));
        return;
    }

    QString const entryCol = resolveEntryColumn();
    if (entryCol.isEmpty())
    {
        QMessageBox::warning(this, tr("Disables row"),
            tr("Could not resolve disables.entry column."));
        return;
    }

    QString const escP0  = QString::fromStdString(m_db->escapeString(p0Edit     ->text().toStdString()));
    QString const escP1  = QString::fromStdString(m_db->escapeString(p1Edit     ->text().toStdString()));
    QString const escCmt = QString::fromStdString(m_db->escapeString(commentEdit->text().toStdString()));

    if (editKey)
    {
        QString const upd = QStringLiteral(
            "UPDATE %1.disables SET sourceType=%2, %3=%4, flags=%5, "
            "params_0='%6', params_1='%7', comment='%8' "
            "WHERE sourceType=%9 AND %3=%10")
            .arg(m_worldDb)
            .arg(stype)
            .arg(entryCol)
            .arg(entry)
            .arg(flags)
            .arg(escP0, escP1, escCmt)
            .arg(editKey->sourceType)
            .arg(editKey->entry);
        if (runInTransaction(upd,
                tr("UPDATE disables (sourceType=%1, entry=%2)").arg(editKey->sourceType).arg(editKey->entry)))
            loadRows();
    }
    else
    {
        QString const ins = QStringLiteral(
            "INSERT INTO %1.disables (sourceType, %2, flags, params_0, params_1, comment) "
            "VALUES (%3, %4, %5, '%6', '%7', '%8')")
            .arg(m_worldDb)
            .arg(entryCol)
            .arg(stype)
            .arg(entry)
            .arg(flags)
            .arg(escP0, escP1, escCmt);
        if (runInTransaction(ins,
                tr("INSERT disables (sourceType=%1, entry=%2)").arg(stype).arg(entry)))
            loadRows();
    }
}

} // namespace world_editor::app
