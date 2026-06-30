#include "ConditionsDock.h"

#include "ConditionCommitDialog.h"
#include "ConditionEditDialog.h"
#include "ConditionEnumTables.h"
#include "UndoManager.h"
#include "../db/ConditionsModel.h"
#include "../db/MySqlClient.h"

#include <QColor>
#include <QFont>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

namespace world_editor::app
{

namespace
{
QString lookupEnum(ConditionEnumEntry const* table, size_t tableSize, int value)
{
    for (size_t i = 0; i < tableSize; ++i)
        if (table[i].value == value)
            return QString::fromLatin1(table[i].name);
    return QStringLiteral("<%1>").arg(value);
}
} // namespace

ConditionsDock::ConditionsDock(QWidget* parent)
    : QWidget(parent)
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(6, 6, 6, 6);

    m_header = new QLabel(tr("Click a spawn / SAI rule / areatrigger to see "
                             "the conditions that gate it."), this);
    m_header->setWordWrap(true);
    root->addWidget(m_header);

    m_table = new QTableWidget(this);
    m_table->setColumnCount(8);
    m_table->setHorizontalHeaderLabels({
        tr("src"), tr("grp"), tr("entry"), tr("id"),
        tr("type"), tr("v1"), tr("v2"), tr("comment") });
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(7, QHeaderView::Stretch);
    m_table->verticalHeader()->setVisible(false);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setAlternatingRowColors(true);
    root->addWidget(m_table, 1);

    // ---- Footer: pending count + edit/commit buttons ----------------
    m_pendingLabel = new QLabel(tr("0 pending"), this);
    m_pendingLabel->setStyleSheet(QStringLiteral("color: #888;"));
    root->addWidget(m_pendingLabel);

    auto* btnRow = new QHBoxLayout();
    m_insertButton = new QPushButton(tr("Insert..."), this);
    m_editButton   = new QPushButton(tr("Edit..."),   this);
    m_deleteButton = new QPushButton(tr("Delete"),    this);
    m_revertButton = new QPushButton(tr("Revert all"), this);
    m_commitButton = new QPushButton(tr("Commit..."), this);
    m_insertButton->setToolTip(tr("Insert a new conditions row (pending until commit)."));
    m_editButton  ->setToolTip(tr("Edit the selected row (pending until commit)."));
    m_deleteButton->setToolTip(tr("Mark the selected row for deletion (pending until commit)."));
    m_revertButton->setToolTip(tr("Discard ALL pending edits and reload from DB."));
    m_commitButton->setToolTip(tr("Review pending changes and write them to DB."));
    btnRow->addWidget(m_insertButton);
    btnRow->addWidget(m_editButton);
    btnRow->addWidget(m_deleteButton);
    btnRow->addStretch(1);
    btnRow->addWidget(m_revertButton);
    btnRow->addWidget(m_commitButton);
    root->addLayout(btnRow);

    connect(m_insertButton, &QPushButton::clicked, this, &ConditionsDock::onInsertClicked);
    connect(m_editButton,   &QPushButton::clicked, this, &ConditionsDock::onEditClicked);
    connect(m_deleteButton, &QPushButton::clicked, this, &ConditionsDock::onDeleteClicked);
    connect(m_revertButton, &QPushButton::clicked, this, &ConditionsDock::onRevertClicked);
    connect(m_commitButton, &QPushButton::clicked, this, &ConditionsDock::onCommitClicked);
    connect(m_table,        &QTableWidget::itemSelectionChanged,
            this, &ConditionsDock::onSelectionChanged);
    connect(m_table,        &QTableWidget::cellDoubleClicked,
            this, &ConditionsDock::onRowDoubleClicked);

    updateFooter();
}

void ConditionsDock::onRowDoubleClicked(int row, int column)
{
    (void)column;
    if (!m_model) return;
    auto const& rows = m_model->current();
    if (row < 0 || row >= int(rows.size())) return;
    render::Condition const& a = rows[size_t(row)];
    // ConditionType 56 = CONDITION_PLAYER_CONDITION per TC's ConditionMgr.h.
    // ConditionValue1 holds the PlayerConditionID; route it to the
    // PlayerConditionDock via the MainWindow.
    if (a.conditionTypeOrReference == 56)
        emit playerConditionSelected(a.conditionValue1);
}

void ConditionsDock::clear()
{
    m_scopeKind = ScopeKind::None;
    m_table->setRowCount(0);
    if (m_model)
        m_model->setBaseline({});
    m_header->setText(tr("Click a spawn / SAI rule / areatrigger to see "
                         "the conditions that gate it."));
    updateFooter();
}

void ConditionsDock::setSpawnScope(uint32_t entry, uint8_t kind)
{
    m_scopeKind  = ScopeKind::Spawn;
    m_spawnEntry = entry;
    m_spawnKind  = kind;
    runQuery();
}

void ConditionsDock::setSmartScriptScope(int64_t entryOrGuid,
                                         uint16_t id,
                                         uint8_t sourceType)
{
    m_scopeKind        = ScopeKind::SmartScript;
    m_saiEntryOrGuid   = entryOrGuid;
    m_saiId            = id;
    m_saiSourceType    = sourceType;
    runQuery();
}

void ConditionsDock::setAreatriggerScope(uint32_t createPropsId)
{
    m_scopeKind        = ScopeKind::Areatrigger;
    m_areatriggerProps = createPropsId;
    runQuery();
}

void ConditionsDock::refreshFromDb()
{
    runQuery();
}

render::Condition ConditionsDock::scopeDefaults() const
{
    render::Condition row;
    switch (m_scopeKind)
    {
    case ScopeKind::Spawn:
        // Best-effort default: most NPC-affecting source types put the
        // creature entry in SourceGroup (SPELL_CLICK_EVENT=18,
        // NPC_VENDOR=23) so we seed both fields.  Operator can edit
        // SourceTypeOrReferenceId before commit.
        row.sourceTypeOrReferenceId = (m_spawnKind == 0) ? 18 : 30;
        row.sourceGroup             = m_spawnEntry;
        row.sourceEntry             = int32_t(m_spawnEntry);
        break;
    case ScopeKind::SmartScript:
        row.sourceTypeOrReferenceId = 22;   // SMART_EVENT
        row.sourceGroup             = m_saiId;
        row.sourceEntry             = int32_t(m_saiEntryOrGuid);
        row.sourceId                = int32_t(m_saiSourceType);
        break;
    case ScopeKind::Areatrigger:
        row.sourceTypeOrReferenceId = 28;   // AREATRIGGER
        row.sourceEntry             = int32_t(m_areatriggerProps);
        break;
    case ScopeKind::None:
        break;
    }
    return row;
}

void ConditionsDock::runQuery()
{
    m_table->setRowCount(0);

    QString scopeLabel;
    QString where;
    switch (m_scopeKind)
    {
    case ScopeKind::Spawn:
        // ConditionSourceType values come from TC's enum in
        // src/server/game/Conditions/ConditionMgr.h.  See header doc.
        where = QStringLiteral(
            "(SourceTypeOrReferenceId = 16 AND SourceEntry = %1) "
            "OR (SourceTypeOrReferenceId = 18 AND SourceGroup = %1) "
            "OR (SourceTypeOrReferenceId = 21 AND SourceGroup = %1) "
            "OR (SourceTypeOrReferenceId = 22 AND SourceEntry = %1 AND SourceEntry > 0) "
            "OR (SourceTypeOrReferenceId = 23 AND SourceGroup = %1) "
            "OR (SourceTypeOrReferenceId = 32 AND SourceEntry = %1)").arg(m_spawnEntry);
        scopeLabel = (m_spawnKind == 0)
            ? tr("Conditions affecting creature entry %1").arg(m_spawnEntry)
            : tr("Conditions affecting gameobject entry %1").arg(m_spawnEntry);
        break;
    case ScopeKind::SmartScript:
        where = QStringLiteral(
            "SourceTypeOrReferenceId = 22 "
            "AND SourceGroup = %1 "
            "AND SourceEntry = %2 "
            "AND SourceId = %3")
            .arg(m_saiId).arg(qlonglong(m_saiEntryOrGuid)).arg(int(m_saiSourceType));
        scopeLabel = tr("Conditions on SAI rule entryorguid=%1 id=%2 source_type=%3")
            .arg(qlonglong(m_saiEntryOrGuid)).arg(m_saiId).arg(m_saiSourceType);
        break;
    case ScopeKind::Areatrigger:
        where = QStringLiteral(
            "(SourceTypeOrReferenceId = 28 OR SourceTypeOrReferenceId = 30) "
            "AND SourceEntry = %1").arg(m_areatriggerProps);
        scopeLabel = tr("Conditions on areatrigger create_properties Id=%1")
            .arg(m_areatriggerProps);
        break;
    case ScopeKind::None:
        return;
    }

    m_header->setText(scopeLabel);
    if (!m_db || !m_db->isConnected())
    {
        m_header->setText(scopeLabel + tr(" - DB not connected"));
        if (m_model)
            m_model->setBaseline({});
        rebuildTable();
        updateFooter();
        return;
    }
    QString const sql = QStringLiteral(
        "SELECT SourceTypeOrReferenceId, SourceGroup, SourceEntry, SourceId, "
        "       ElseGroup, ConditionTypeOrReference, ConditionTarget, "
        "       ConditionValue1, ConditionValue2, ConditionValue3, "
        "       COALESCE(ConditionStringValue1, ''), "
        "       NegativeCondition, ErrorType, ErrorTextId, "
        "       COALESCE(ScriptName, ''), COALESCE(Comment, '') "
        "FROM conditions "
        "WHERE %1 "
        "ORDER BY SourceTypeOrReferenceId, SourceGroup, SourceEntry, SourceId, ElseGroup "
        "LIMIT 500").arg(where);

    db::QueryResult res;
    auto const err = m_db->query(sql.toStdString(), res);
    if (!err.ok())
    {
        m_header->setText(scopeLabel + tr(" - query failed: %1")
            .arg(QString::fromStdString(err.message)));
        if (m_model)
            m_model->setBaseline({});
        rebuildTable();
        updateFooter();
        return;
    }

    std::vector<render::Condition> rows;
    rows.reserve(res.rowCount());
    for (size_t r = 0; r < res.rowCount(); ++r)
    {
        render::Condition a;
        a.sourceTypeOrReferenceId  = int32_t (res.asInt64 (r, 0).value_or(0));
        a.sourceGroup              = uint32_t(res.asUInt64(r, 1).value_or(0));
        a.sourceEntry              = int32_t (res.asInt64 (r, 2).value_or(0));
        a.sourceId                 = int32_t (res.asInt64 (r, 3).value_or(0));
        a.elseGroup                = uint32_t(res.asUInt64(r, 4).value_or(0));
        a.conditionTypeOrReference = int32_t (res.asInt64 (r, 5).value_or(0));
        a.conditionTarget          = uint8_t (res.asUInt64(r, 6).value_or(0));
        a.conditionValue1          = uint32_t(res.asUInt64(r, 7).value_or(0));
        a.conditionValue2          = uint32_t(res.asUInt64(r, 8).value_or(0));
        a.conditionValue3          = uint32_t(res.asUInt64(r, 9).value_or(0));
        a.conditionStringValue1    = QString::fromStdString(res.cell(r, 10));
        a.negativeCondition        = uint8_t (res.asUInt64(r, 11).value_or(0));
        a.errorType                = uint32_t(res.asUInt64(r, 12).value_or(0));
        a.errorTextId              = uint32_t(res.asUInt64(r, 13).value_or(0));
        a.scriptName               = QString::fromStdString(res.cell(r, 14));
        a.comment                  = QString::fromStdString(res.cell(r, 15));
        rows.push_back(std::move(a));
    }
    if (m_model)
        m_model->setBaseline(std::move(rows));

    if (m_model && m_model->current().empty())
        m_header->setText(scopeLabel + tr(" - no conditions"));
    else if (m_model)
        m_header->setText(scopeLabel + tr(" - %1 condition row(s)").arg(m_model->current().size()));

    rebuildTable();
    updateFooter();
}

void ConditionsDock::rebuildTable()
{
    m_table->setRowCount(0);
    if (!m_model) return;

    auto const& rows = m_model->current();
    auto const& changes = m_model->changes();
    m_table->setRowCount(int(rows.size()));
    for (size_t r = 0; r < rows.size(); ++r)
    {
        render::Condition const& a = rows[r];
        auto setCell = [this, r](int col, QString const& text, QColor const& fg) {
            auto* item = new QTableWidgetItem(text);
            item->setFlags(item->flags() & ~Qt::ItemIsEditable);
            if (fg.isValid())
                item->setForeground(fg);
            m_table->setItem(int(r), col, item);
        };
        // Tint Insert rows green and Update rows orange so the operator
        // can see pending edits at a glance.
        QColor fg;
        if (r < changes.size())
        {
            switch (changes[r].kind)
            {
            case db::ConditionChangeKind::Insert: fg = QColor(60, 160, 60); break;
            case db::ConditionChangeKind::Update: fg = QColor(200, 140, 0); break;
            case db::ConditionChangeKind::Delete: fg = QColor(180, 60, 60); break;
            default: break;
            }
        }

        setCell(0, QStringLiteral("%1 %2").arg(int(a.sourceTypeOrReferenceId))
            .arg(lookupEnum(kConditionSourceTypes,
                            std::size(kConditionSourceTypes),
                            int(a.sourceTypeOrReferenceId))), fg);
        setCell(1, QString::number(a.sourceGroup), fg);
        setCell(2, QString::number(int(a.sourceEntry)), fg);
        setCell(3, QString::number(int(a.sourceId)), fg);
        QString const condName = lookupEnum(kConditionTypes,
            std::size(kConditionTypes), int(a.conditionTypeOrReference));
        setCell(4, a.negativeCondition
            ? QStringLiteral("!%1 %2").arg(int(a.conditionTypeOrReference)).arg(condName)
            : QStringLiteral("%1 %2").arg(int(a.conditionTypeOrReference)).arg(condName), fg);
        setCell(5, QString::number(a.conditionValue1), fg);
        setCell(6, QString::number(a.conditionValue2), fg);
        setCell(7, a.comment, fg);
        // For CONDITION_PLAYER_CONDITION (56), ConditionValue1 carries
        // the PlayerConditionID.  Visually flag the v1 cell so the
        // operator sees it's a navigable link (double-click drops into
        // the PlayerCondition dock).
        if (a.conditionTypeOrReference == 56)
        {
            if (auto* item = m_table->item(int(r), 5))
            {
                QFont f = item->font();
                f.setBold(true);
                f.setUnderline(true);
                item->setFont(f);
                item->setForeground(QColor(60, 110, 200));
                item->setToolTip(tr("Double-click any cell in this row to open "
                                    "PlayerCondition %1 in the PlayerCondition dock.")
                                    .arg(a.conditionValue1));
            }
        }
    }
}

void ConditionsDock::updateFooter()
{
    bool const hasModel  = (m_model != nullptr);
    bool const hasScope  = (m_scopeKind != ScopeKind::None);
    bool const hasSel    = (m_table->currentRow() >= 0);
    size_t const pending = hasModel ? m_model->pendingCount() : 0;

    m_pendingLabel->setText(tr("%1 pending").arg(pending));
    m_insertButton->setEnabled(hasModel && hasScope);
    m_editButton  ->setEnabled(hasModel && hasSel);
    m_deleteButton->setEnabled(hasModel && hasSel);
    m_revertButton->setEnabled(hasModel && pending > 0);
    m_commitButton->setEnabled(hasModel && pending > 0 && m_db && m_db->isConnected());
}

void ConditionsDock::onSelectionChanged()
{
    updateFooter();
}

void ConditionsDock::onInsertClicked()
{
    if (!m_model) return;
    ConditionEditDialog dlg(this);
    dlg.setCondition(scopeDefaults());
    dlg.setKeyEditable(true);
    if (dlg.exec() != QDialog::Accepted)
        return;
    render::Condition const proposed = dlg.condition();
    int newIdx = -1;
    auto mutate = [&]() {
        newIdx = m_model->addNew(proposed);
    };
    if (m_undo)
        m_undo->recordOn(m_model, tr("Insert condition"), mutate);
    else
        mutate();
    if (newIdx < 0)
    {
        QMessageBox::warning(this, tr("Duplicate PK"),
            tr("A condition with that 11-column primary key already exists in this scope."));
        return;
    }
    rebuildTable();
    updateFooter();
    emit modelChanged();
}

void ConditionsDock::onEditClicked()
{
    if (!m_model) return;
    int const idx = m_table->currentRow();
    if (idx < 0 || idx >= int(m_model->current().size())) return;
    render::Condition const before = m_model->current()[idx];

    ConditionEditDialog dlg(this);
    dlg.setCondition(before);
    dlg.setKeyEditable(false);   // operator must Delete + Add to re-key
    if (dlg.exec() != QDialog::Accepted)
        return;
    render::Condition const proposed = dlg.condition();

    bool changed = false;
    auto mutate = [&]() {
        changed = m_model->replaceRow(idx, proposed);
        return changed;
    };
    if (m_undo)
        m_undo->recordIf(m_model, tr("Edit condition"), mutate);
    else
        mutate();
    if (!changed)
        return;
    rebuildTable();
    updateFooter();
    emit modelChanged();
}

void ConditionsDock::onDeleteClicked()
{
    if (!m_model) return;
    int const idx = m_table->currentRow();
    if (idx < 0 || idx >= int(m_model->current().size())) return;

    bool ok = false;
    auto mutate = [&]() {
        ok = m_model->removeRow(idx);
        return ok;
    };
    if (m_undo)
        m_undo->recordIf(m_model, tr("Delete condition"), mutate);
    else
        mutate();
    if (!ok) return;
    rebuildTable();
    updateFooter();
    emit modelChanged();
}

void ConditionsDock::onRevertClicked()
{
    if (!m_model) return;
    if (m_model->pendingCount() == 0) return;
    auto mutate = [&]() { m_model->revertAll(); };
    if (m_undo)
        m_undo->recordOn(m_model, tr("Revert conditions"), mutate);
    else
        mutate();
    rebuildTable();
    updateFooter();
    emit modelChanged();
}

void ConditionsDock::onCommitClicked()
{
    if (!m_model) return;
    if (m_model->pendingCount() == 0) return;
    if (!m_db || !m_db->isConnected())
    {
        QMessageBox::warning(this, tr("Not connected"),
            tr("Connect to the world DB before committing condition edits."));
        return;
    }

    ConditionCommitDialog dlg(m_db, *m_model, this);
    if (dlg.exec() != QDialog::Accepted)
        return;

    // On success, refresh from DB so the model reflects post-commit
    // truth (other unrelated rows in the scope may also have changed
    // externally between dock open and commit).
    runQuery();
    emit modelChanged();
}

} // namespace world_editor::app
