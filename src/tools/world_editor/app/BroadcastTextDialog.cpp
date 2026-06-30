#include "BroadcastTextDialog.h"

#include "../db/MySqlClient.h"

#include <QApplication>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

#include <cstdio>
#include <limits>

namespace world_editor::app
{

namespace
{

// Column indices for the main table view.  Order kept in sync with the
// SELECT in loadRows() so the Add/Edit modal can read cell values back
// without hitting the DB again.
enum Col : int
{
    COL_ID               = 0,
    COL_LANGUAGE         = 1,
    COL_MALETEXT         = 2,
    COL_FEMALETEXT       = 3,
    COL_EMOTE1           = 4,
    COL_EMOTE2           = 5,
    COL_EMOTE3           = 6,
    COL_SOUND1           = 7,
    COL_SOUND2           = 8,
    COL_EMOTES_ID        = 9,
    COL_FLAGS            = 10,
    COL_COUNT            = 11,
};

// Best-effort tables/columns to scan when "Show references" is invoked or
// before deletion.  Each entry is probed for existence at runtime via
// INFORMATION_SCHEMA.COLUMNS so missing tables (legacy schemas) are
// silently skipped.
struct RefTarget { char const* table; char const* column; };

constexpr RefTarget kRefTargets[] = {
    { "creature_text",            "BroadcastTextId"          },
    { "gossip_menu_option",       "OptionBroadcastTextID"    },
    { "gossip_menu_option",       "BoxBroadcastTextID"       },
    { "quest_offer_reward",       "RewardText"               },
    { "quest_request_items",      "CompletionText"           },
    { "quest_template",           "LogDescription"           },
    { "npc_text",                 "BroadcastTextID0"         },
    { "npc_text",                 "BroadcastTextID1"         },
    { "npc_text",                 "BroadcastTextID2"         },
    { "npc_text",                 "BroadcastTextID3"         },
    { "npc_text",                 "BroadcastTextID4"         },
    { "npc_text",                 "BroadcastTextID5"         },
    { "npc_text",                 "BroadcastTextID6"         },
    { "npc_text",                 "BroadcastTextID7"         },
    { "page_text",                "NextPageID"               },
    { "points_of_interest",       "Name"                     },
};

// Returns a heap-allocated QTableWidgetItem with both the displayed string
// and the raw sort value attached, so QTableWidget sorting works while the
// rendered text is still QString.  We don't use setSortingEnabled (the
// canonical view is "ORDER BY ID"), but the UserRole keeps the data round-
// trippable when the modal reads cells back.
QTableWidgetItem* makeIdItem(uint64_t v)
{
    auto* c = new QTableWidgetItem;
    c->setData(Qt::DisplayRole, qulonglong(v));
    return c;
}

QTableWidgetItem* makeIntItem(int64_t v)
{
    auto* c = new QTableWidgetItem;
    c->setData(Qt::DisplayRole, qlonglong(v));
    return c;
}

QTableWidgetItem* makeTextItem(QString const& s)
{
    auto* c = new QTableWidgetItem(s);
    return c;
}

} // namespace

BroadcastTextDialog::BroadcastTextDialog(db::MySqlClient* dbClient,
                                         QString const& worldDbName,
                                         QWidget* parent)
    : QDialog(parent), m_db(dbClient), m_worldDb(worldDbName)
{
    setWindowTitle(tr("Broadcast text editor"));
    setModal(true);
    resize(1280, 620);

    auto* outer = new QVBoxLayout(this);

    // -- Top filter row: search box (ID exact / substring on text) + Refresh.
    auto* filterRow = new QHBoxLayout;
    filterRow->addWidget(new QLabel(tr("Filter (ID exact, or substring of MaleText/FemaleText):"), this));
    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setPlaceholderText(tr("e.g. 12345 or 'hello'"));
    filterRow->addWidget(m_searchEdit, 1);
    m_refreshBtn = new QPushButton(tr("Refresh"), this);
    filterRow->addWidget(m_refreshBtn);
    outer->addLayout(filterRow);

    // -- Main table -----------------------------------------------------
    m_table = new QTableWidget(this);
    m_table->setColumnCount(COL_COUNT);
    m_table->setHorizontalHeaderLabels({
        tr("ID"), tr("LanguageID"),
        tr("MaleText"), tr("FemaleText"),
        tr("EmoteID1"), tr("EmoteID2"), tr("EmoteID3"),
        tr("SoundEntriesID1"), tr("SoundEntriesID2"),
        tr("EmotesID"), tr("Flags") });
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    m_table->horizontalHeader()->setStretchLastSection(false);
    m_table->verticalHeader()->setVisible(false);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setSortingEnabled(false);  // ORDER BY ID is the canonical view.
    m_table->setColumnWidth(COL_ID,         80);
    m_table->setColumnWidth(COL_LANGUAGE,   80);
    m_table->setColumnWidth(COL_MALETEXT,   360);
    m_table->setColumnWidth(COL_FEMALETEXT, 360);
    outer->addWidget(m_table, 1);

    // -- Action buttons -------------------------------------------------
    auto* btnRow = new QHBoxLayout;
    m_addBtn    = new QPushButton(tr("Add new..."), this);
    m_editBtn   = new QPushButton(tr("Edit row..."), this);
    m_removeBtn = new QPushButton(tr("Remove row"), this);
    m_refsBtn   = new QPushButton(tr("Show references"), this);
    m_editBtn  ->setEnabled(false);
    m_removeBtn->setEnabled(false);
    m_refsBtn  ->setEnabled(false);
    btnRow->addWidget(m_addBtn);
    btnRow->addWidget(m_editBtn);
    btnRow->addWidget(m_removeBtn);
    btnRow->addWidget(m_refsBtn);
    btnRow->addStretch(1);
    outer->addLayout(btnRow);

    m_statusLabel = new QLabel(tr("Press Refresh to load the first 5000 broadcast_text rows."), this);
    outer->addWidget(m_statusLabel);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    outer->addWidget(buttons);

    // Wire signals.
    connect(m_refreshBtn, &QPushButton::clicked, this, &BroadcastTextDialog::onRefresh);
    connect(m_addBtn,     &QPushButton::clicked, this, &BroadcastTextDialog::onAdd);
    connect(m_editBtn,    &QPushButton::clicked, this, &BroadcastTextDialog::onEdit);
    connect(m_removeBtn,  &QPushButton::clicked, this, &BroadcastTextDialog::onRemove);
    connect(m_refsBtn,    &QPushButton::clicked, this, &BroadcastTextDialog::onShowReferences);
    connect(m_searchEdit, &QLineEdit::returnPressed, this, &BroadcastTextDialog::onRefresh);
    connect(m_table,      &QTableWidget::itemSelectionChanged,
            this, &BroadcastTextDialog::onSelectionChanged);

    // Eagerly probe and load on open so the operator sees a populated grid.
    detectSchemaColumns();
    loadRows();
}

void BroadcastTextDialog::detectSchemaColumns()
{
    if (m_schemaDetected) return;
    m_schemaDetected = true;
    m_cols.clear();
    if (!m_db || !m_db->isConnected()) return;

    char sql[512];
    std::snprintf(sql, sizeof(sql),
        "SELECT COLUMN_NAME FROM INFORMATION_SCHEMA.COLUMNS "
        "WHERE TABLE_SCHEMA='%s' AND TABLE_NAME='broadcast_text'",
        m_worldDb.toStdString().c_str());
    db::QueryResult res;
    auto const err = m_db->query(sql, res);
    if (!err.ok()) return;
    for (size_t r = 0; r < res.rowCount(); ++r)
        m_cols.insert(QString::fromStdString(res.cell(r, 0)).toLower());
}

bool BroadcastTextDialog::hasCol(QString const& colName) const
{
    return m_cols.contains(colName.toLower());
}

void BroadcastTextDialog::onRefresh()
{
    detectSchemaColumns();
    loadRows();
}

void BroadcastTextDialog::onSelectionChanged()
{
    if (m_loading) return;
    bool const has = m_table->currentRow() >= 0;
    m_editBtn  ->setEnabled(has);
    m_removeBtn->setEnabled(has);
    m_refsBtn  ->setEnabled(has);
}

bool BroadcastTextDialog::currentRowId(uint32_t& idOut, int& rowOut) const
{
    int const row = m_table->currentRow();
    if (row < 0) return false;
    auto* idCell = m_table->item(row, COL_ID);
    if (!idCell) return false;
    idOut  = static_cast<uint32_t>(idCell->data(Qt::DisplayRole).toULongLong());
    rowOut = row;
    return true;
}

void BroadcastTextDialog::loadRows()
{
    m_loading = true;
    m_table->setRowCount(0);

    if (!m_db || !m_db->isConnected())
    {
        m_statusLabel->setText(tr("DB not connected."));
        m_loading = false;
        return;
    }
    if (m_cols.isEmpty())
    {
        m_statusLabel->setText(tr("broadcast_text not found in schema '%1'.").arg(m_worldDb));
        m_loading = false;
        return;
    }

    // Build the WHERE clause from the search box.  Pure numeric input matches
    // ID exactly; everything else becomes a LIKE substring scan across
    // MaleText/FemaleText.  Empty input means "no filter".
    QString const filter = m_searchEdit->text().trimmed();
    QString whereClause;
    if (!filter.isEmpty())
    {
        bool numeric = false;
        ulong const num = filter.toULong(&numeric);
        if (numeric)
        {
            whereClause = QStringLiteral("WHERE ID=%1").arg(num);
        }
        else
        {
            QString const esc = QString::fromStdString(m_db->escapeString(filter.toStdString()));
            QStringList ors;
            if (hasCol(QStringLiteral("MaleText")))
                ors << QStringLiteral("MaleText LIKE '%%%1%%'").arg(esc);
            if (hasCol(QStringLiteral("FemaleText")))
                ors << QStringLiteral("FemaleText LIKE '%%%1%%'").arg(esc);
            if (!ors.isEmpty())
                whereClause = QStringLiteral("WHERE ") + ors.join(QStringLiteral(" OR "));
        }
    }

    // Column projection - substitute zero for any field the live schema
    // happens to lack so the column count stays stable.
    auto col = [&](char const* name) -> QString {
        return hasCol(QString::fromLatin1(name)) ? QString::fromLatin1(name) : QStringLiteral("0");
    };
    auto colText = [&](char const* name) -> QString {
        return hasCol(QString::fromLatin1(name)) ? QString::fromLatin1(name) : QStringLiteral("''");
    };

    QString const sql = QStringLiteral(
        "SELECT ID, %1, %2, %3, %4, %5, %6, %7, %8, %9, %10 "
        "FROM %11.broadcast_text "
        "%12 "
        "ORDER BY ID LIMIT 5000")
        .arg(col("LanguageID"))
        .arg(colText("MaleText"))
        .arg(colText("FemaleText"))
        .arg(col("EmoteID1"))
        .arg(col("EmoteID2"))
        .arg(col("EmoteID3"))
        .arg(col("SoundEntriesID1"))
        .arg(col("SoundEntriesID2"))
        .arg(col("EmotesID"))
        .arg(col("Flags"))
        .arg(m_worldDb)
        .arg(whereClause);

    db::QueryResult res;
    QApplication::setOverrideCursor(Qt::WaitCursor);
    auto const err = m_db->query(sql.toStdString(), res);
    QApplication::restoreOverrideCursor();
    if (!err.ok())
    {
        m_statusLabel->setText(tr("broadcast_text query failed: %1")
            .arg(QString::fromStdString(err.message)));
        m_loading = false;
        return;
    }

    m_table->setRowCount(static_cast<int>(res.rowCount()));
    for (size_t r = 0; r < res.rowCount(); ++r)
    {
        m_table->setItem(int(r), COL_ID,         makeIdItem (res.asUInt64(r, 0).value_or(0)));
        m_table->setItem(int(r), COL_LANGUAGE,   makeIntItem(res.asInt64 (r, 1).value_or(0)));
        m_table->setItem(int(r), COL_MALETEXT,   makeTextItem(QString::fromStdString(res.cell(r, 2))));
        m_table->setItem(int(r), COL_FEMALETEXT, makeTextItem(QString::fromStdString(res.cell(r, 3))));
        m_table->setItem(int(r), COL_EMOTE1,     makeIdItem (res.asUInt64(r, 4).value_or(0)));
        m_table->setItem(int(r), COL_EMOTE2,     makeIdItem (res.asUInt64(r, 5).value_or(0)));
        m_table->setItem(int(r), COL_EMOTE3,     makeIdItem (res.asUInt64(r, 6).value_or(0)));
        m_table->setItem(int(r), COL_SOUND1,     makeIdItem (res.asUInt64(r, 7).value_or(0)));
        m_table->setItem(int(r), COL_SOUND2,     makeIdItem (res.asUInt64(r, 8).value_or(0)));
        m_table->setItem(int(r), COL_EMOTES_ID,  makeIdItem (res.asUInt64(r, 9).value_or(0)));
        m_table->setItem(int(r), COL_FLAGS,      makeIdItem (res.asUInt64(r, 10).value_or(0)));
    }

    m_statusLabel->setText(tr("broadcast_text rows=%1%2")
        .arg(res.rowCount())
        .arg(res.rowCount() == 5000 ? tr(" (capped; narrow your filter to see more)") : QString()));
    m_loading = false;
    onSelectionChanged();
}

bool BroadcastTextDialog::runInTransaction(QStringList const& sqls, QString const& description)
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
    uint64_t totalAffected = 0;
    for (QString const& sql : sqls)
    {
        uint64_t affected = 0;
        err = m_db->exec(sql.toStdString(), &affected);
        if (!err.ok())
        {
            (void)m_db->exec("ROLLBACK");
            QMessageBox::critical(this, tr("DML failed"),
                tr("%1\n\n%2").arg(description).arg(QString::fromStdString(err.message)));
            return false;
        }
        totalAffected += affected;
    }
    err = m_db->exec("COMMIT");
    if (!err.ok())
    {
        (void)m_db->exec("ROLLBACK");
        QMessageBox::critical(this, tr("Commit failed"),
            tr("%1\n\n%2").arg(description).arg(QString::fromStdString(err.message)));
        return false;
    }
    m_statusLabel->setText(tr("%1 (affected=%2)").arg(description).arg(qulonglong(totalAffected)));
    return true;
}

QList<BroadcastTextDialog::RefHit> BroadcastTextDialog::scanReferences(uint32_t broadcastId)
{
    QList<RefHit> hits;
    if (!m_db || !m_db->isConnected()) return hits;

    for (auto const& tgt : kRefTargets)
    {
        // Probe INFORMATION_SCHEMA.COLUMNS once per (table, column) to avoid
        // a noisy MySQL error when the table or column doesn't exist on the
        // operator's schema (e.g. some npc_text shapes carry only 4 of 8).
        char probeSql[512];
        std::snprintf(probeSql, sizeof(probeSql),
            "SELECT 1 FROM INFORMATION_SCHEMA.COLUMNS "
            "WHERE TABLE_SCHEMA='%s' AND TABLE_NAME='%s' AND COLUMN_NAME='%s'",
            m_worldDb.toStdString().c_str(), tgt.table, tgt.column);
        db::QueryResult probeRes;
        auto pErr = m_db->query(probeSql, probeRes);
        if (!pErr.ok() || probeRes.rowCount() == 0)
            continue;

        char cntSql[512];
        std::snprintf(cntSql, sizeof(cntSql),
            "SELECT COUNT(*) FROM %s.%s WHERE %s=%u",
            m_worldDb.toStdString().c_str(), tgt.table, tgt.column, broadcastId);
        db::QueryResult cRes;
        auto cErr = m_db->query(cntSql, cRes);
        if (!cErr.ok() || cRes.rowCount() == 0) continue;
        uint64_t const cnt = cRes.asUInt64(0, 0).value_or(0);
        if (cnt > 0)
            hits.push_back({ QString::fromLatin1(tgt.table), QString::fromLatin1(tgt.column), cnt });
    }
    return hits;
}

void BroadcastTextDialog::onShowReferences()
{
    uint32_t id = 0;
    int row = -1;
    if (!currentRowId(id, row)) return;

    QApplication::setOverrideCursor(Qt::WaitCursor);
    QList<RefHit> const hits = scanReferences(id);
    QApplication::restoreOverrideCursor();

    QString body;
    if (hits.isEmpty())
    {
        body = tr("No referencing rows found for broadcast_text.ID=%1.").arg(id);
    }
    else
    {
        uint64_t total = 0;
        for (auto const& h : hits) total += h.count;
        body = tr("broadcast_text.ID=%1 is referenced by %2 row(s) across %3 column(s):\n\n")
                   .arg(id).arg(qulonglong(total)).arg(hits.size());
        for (auto const& h : hits)
        {
            body += tr("  %1.%2 - %3 row(s)\n").arg(h.table).arg(h.column).arg(qulonglong(h.count));
        }
    }
    QMessageBox::information(this, tr("Broadcast text references"), body);
}

void BroadcastTextDialog::onAdd()
{
    openModal(false, 0);
}

void BroadcastTextDialog::onEdit()
{
    uint32_t id = 0;
    int row = -1;
    if (!currentRowId(id, row)) return;
    openModal(true, id);
}

void BroadcastTextDialog::onRemove()
{
    uint32_t id = 0;
    int row = -1;
    if (!currentRowId(id, row)) return;

    // Warn the operator if anything points at this ID before we DELETE.
    QApplication::setOverrideCursor(Qt::WaitCursor);
    QList<RefHit> const hits = scanReferences(id);
    QApplication::restoreOverrideCursor();

    QString prompt = tr("Delete broadcast_text row ID=%1?").arg(id);
    if (!hits.isEmpty())
    {
        uint64_t total = 0;
        for (auto const& h : hits) total += h.count;
        prompt += tr("\n\nWARNING: %1 row(s) across %2 referencing column(s) currently point at this ID:\n")
                      .arg(qulonglong(total)).arg(hits.size());
        for (auto const& h : hits)
            prompt += tr("  %1.%2 (%3)\n").arg(h.table).arg(h.column).arg(qulonglong(h.count));
        prompt += tr("\nThose rows will be left dangling.");
    }

    auto const choice = QMessageBox::question(this, tr("Remove broadcast_text"),
        prompt, QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (choice != QMessageBox::Yes) return;

    QString const sql = QStringLiteral(
        "DELETE FROM %1.broadcast_text WHERE ID=%2").arg(m_worldDb).arg(id);
    if (runInTransaction(QStringList{ sql },
            tr("DELETE broadcast_text (ID=%1)").arg(id)))
        loadRows();
}

void BroadcastTextDialog::openModal(bool editing, uint32_t editingId)
{
    // For Add path: pre-compute MAX(ID)+1 so the operator can accept the
    // suggested next-free ID without manual lookup.
    uint32_t suggestedId = 1;
    if (!editing && m_db && m_db->isConnected())
    {
        char sql[256];
        std::snprintf(sql, sizeof(sql),
            "SELECT COALESCE(MAX(ID),0)+1 FROM %s.broadcast_text",
            m_worldDb.toStdString().c_str());
        db::QueryResult res;
        auto err = m_db->query(sql, res);
        if (err.ok() && res.rowCount() == 1)
            suggestedId = static_cast<uint32_t>(res.asUInt64(0, 0).value_or(1));
    }

    QDialog dlg(this);
    dlg.setWindowTitle(editing ? tr("Edit broadcast_text row") : tr("Add broadcast_text row"));
    dlg.setModal(true);
    dlg.resize(720, 720);

    auto* outerL = new QVBoxLayout(&dlg);

    // -- ID + LanguageID
    auto* idForm = new QFormLayout;
    auto* idSpin = new QSpinBox(&dlg);
    idSpin->setRange(0, std::numeric_limits<int>::max());
    idSpin->setEnabled(!editing);  // PK immutable post-INSERT.
    auto* langSpin = new QSpinBox(&dlg);
    langSpin->setRange(-1, 65535);
    idForm->addRow(tr("ID:"),         idSpin);
    idForm->addRow(tr("LanguageID:"), langSpin);
    outerL->addLayout(idForm);

    // -- Male / Female text bodies
    auto* textGrid = new QHBoxLayout;
    auto* maleBox  = new QGroupBox(tr("MaleText"),   &dlg);
    auto* mlLayout = new QVBoxLayout(maleBox);
    auto* maleEdit = new QPlainTextEdit(maleBox);
    mlLayout->addWidget(maleEdit);
    auto* femBox  = new QGroupBox(tr("FemaleText"), &dlg);
    auto* fmLayout = new QVBoxLayout(femBox);
    auto* femEdit = new QPlainTextEdit(femBox);
    fmLayout->addWidget(femEdit);
    textGrid->addWidget(maleBox, 1);
    textGrid->addWidget(femBox,  1);
    outerL->addLayout(textGrid, 1);

    // -- Emote channels: three (EmoteID, EmoteDelay) pairs.
    struct EmoteInputs { QGroupBox* box; QSpinBox* id; QSpinBox* delay; };
    auto makeEmoteSlot = [&](int n) -> EmoteInputs {
        auto* box  = new QGroupBox(tr("Emote %1").arg(n), &dlg);
        auto* form = new QFormLayout(box);
        auto* idSp = new QSpinBox(box);
        auto* dlSp = new QSpinBox(box);
        idSp->setRange(0, 65535);
        dlSp->setRange(0, std::numeric_limits<int>::max());
        form->addRow(tr("EmoteID:"),    idSp);
        form->addRow(tr("EmoteDelay:"), dlSp);
        return { box, idSp, dlSp };
    };
    auto* emoteRow = new QHBoxLayout;
    EmoteInputs e1 = makeEmoteSlot(1);
    EmoteInputs e2 = makeEmoteSlot(2);
    EmoteInputs e3 = makeEmoteSlot(3);
    emoteRow->addWidget(e1.box);
    emoteRow->addWidget(e2.box);
    emoteRow->addWidget(e3.box);
    outerL->addLayout(emoteRow);

    // -- Sound, EmotesID, Flags, ConditionID
    auto* soundGroup = new QGroupBox(tr("Sound / Flags / Condition"), &dlg);
    auto* sgForm     = new QFormLayout(soundGroup);
    auto* snd1 = new QSpinBox(soundGroup);
    auto* snd2 = new QSpinBox(soundGroup);
    auto* emotesId = new QSpinBox(soundGroup);
    auto* flagsSpin = new QSpinBox(soundGroup);
    auto* condSpin  = new QSpinBox(soundGroup);
    for (QSpinBox* s : { snd1, snd2, emotesId, flagsSpin, condSpin })
        s->setRange(0, std::numeric_limits<int>::max());
    sgForm->addRow(tr("SoundEntriesID1:"), snd1);
    sgForm->addRow(tr("SoundEntriesID2:"), snd2);
    sgForm->addRow(tr("EmotesID:"),        emotesId);
    sgForm->addRow(tr("Flags:"),           flagsSpin);
    sgForm->addRow(tr("ConditionID:"),     condSpin);
    outerL->addWidget(soundGroup);

    if (editing)
    {
        idSpin->setValue(static_cast<int>(editingId));
        int const row = m_table->currentRow();
        if (row >= 0)
        {
            auto getU = [&](int col) -> uint64_t {
                auto* c = m_table->item(row, col);
                return c ? c->data(Qt::DisplayRole).toULongLong() : 0ull;
            };
            auto getI = [&](int col) -> int64_t {
                auto* c = m_table->item(row, col);
                return c ? c->data(Qt::DisplayRole).toLongLong() : 0ll;
            };
            auto getS = [&](int col) -> QString {
                auto* c = m_table->item(row, col);
                return c ? c->text() : QString();
            };
            langSpin   ->setValue(static_cast<int>(getI(COL_LANGUAGE)));
            maleEdit   ->setPlainText(getS(COL_MALETEXT));
            femEdit    ->setPlainText(getS(COL_FEMALETEXT));
            e1.id      ->setValue(static_cast<int>(getU(COL_EMOTE1)));
            e2.id      ->setValue(static_cast<int>(getU(COL_EMOTE2)));
            e3.id      ->setValue(static_cast<int>(getU(COL_EMOTE3)));
            snd1       ->setValue(static_cast<int>(getU(COL_SOUND1)));
            snd2       ->setValue(static_cast<int>(getU(COL_SOUND2)));
            emotesId   ->setValue(static_cast<int>(getU(COL_EMOTES_ID)));
            flagsSpin  ->setValue(static_cast<int>(getU(COL_FLAGS)));

            // EmoteDelay1/2/3 and ConditionID are not in the table view -
            // pull them from a small targeted SELECT so the modal round-
            // trips fields the grid intentionally omits.
            QStringList extraCols;
            if (hasCol(QStringLiteral("EmoteDelay1"))) extraCols << QStringLiteral("EmoteDelay1");
            if (hasCol(QStringLiteral("EmoteDelay2"))) extraCols << QStringLiteral("EmoteDelay2");
            if (hasCol(QStringLiteral("EmoteDelay3"))) extraCols << QStringLiteral("EmoteDelay3");
            if (hasCol(QStringLiteral("ConditionID"))) extraCols << QStringLiteral("ConditionID");
            if (!extraCols.isEmpty() && m_db && m_db->isConnected())
            {
                QString const extraSql = QStringLiteral(
                    "SELECT %1 FROM %2.broadcast_text WHERE ID=%3")
                    .arg(extraCols.join(QStringLiteral(", ")))
                    .arg(m_worldDb).arg(editingId);
                db::QueryResult extraRes;
                auto eErr = m_db->query(extraSql.toStdString(), extraRes);
                if (eErr.ok() && extraRes.rowCount() == 1)
                {
                    size_t k = 0;
                    if (hasCol(QStringLiteral("EmoteDelay1")))
                        e1.delay->setValue(int(extraRes.asUInt64(0, k++).value_or(0)));
                    if (hasCol(QStringLiteral("EmoteDelay2")))
                        e2.delay->setValue(int(extraRes.asUInt64(0, k++).value_or(0)));
                    if (hasCol(QStringLiteral("EmoteDelay3")))
                        e3.delay->setValue(int(extraRes.asUInt64(0, k++).value_or(0)));
                    if (hasCol(QStringLiteral("ConditionID")))
                        condSpin->setValue(int(extraRes.asUInt64(0, k++).value_or(0)));
                }
            }
        }
    }
    else
    {
        idSpin->setValue(static_cast<int>(suggestedId));
    }

    auto* btns = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    connect(btns, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(btns, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    outerL->addWidget(btns);

    if (dlg.exec() != QDialog::Accepted)
        return;

    uint32_t const rowId    = static_cast<uint32_t>(idSpin->value());
    int32_t  const language = static_cast<int32_t>(langSpin->value());
    QString  const maleText = QString::fromStdString(
        m_db->escapeString(maleEdit->toPlainText().toStdString()));
    QString  const femText  = QString::fromStdString(
        m_db->escapeString(femEdit->toPlainText().toStdString()));
    uint32_t const e1id  = static_cast<uint32_t>(e1.id->value());
    uint32_t const e2id  = static_cast<uint32_t>(e2.id->value());
    uint32_t const e3id  = static_cast<uint32_t>(e3.id->value());
    uint32_t const e1del = static_cast<uint32_t>(e1.delay->value());
    uint32_t const e2del = static_cast<uint32_t>(e2.delay->value());
    uint32_t const e3del = static_cast<uint32_t>(e3.delay->value());
    uint32_t const s1    = static_cast<uint32_t>(snd1->value());
    uint32_t const s2    = static_cast<uint32_t>(snd2->value());
    uint32_t const emId  = static_cast<uint32_t>(emotesId->value());
    uint32_t const flags = static_cast<uint32_t>(flagsSpin->value());
    uint32_t const cond  = static_cast<uint32_t>(condSpin->value());

    // Build the DML column list dynamically; only emit assignments for
    // columns the live schema actually exposes.  Lets the same dialog
    // serve modern + legacy schemas without an explicit branch.
    struct Assignment { QString col; QString valLiteral; };
    QList<Assignment> assigns;
    auto add = [&](char const* name, QString const& valLit) {
        if (hasCol(QString::fromLatin1(name)))
            assigns.append({ QString::fromLatin1(name), valLit });
    };
    add("LanguageID",      QString::number(language));
    add("MaleText",        QStringLiteral("'%1'").arg(maleText));
    add("FemaleText",      QStringLiteral("'%1'").arg(femText));
    add("EmoteID1",        QString::number(e1id));
    add("EmoteID2",        QString::number(e2id));
    add("EmoteID3",        QString::number(e3id));
    add("EmoteDelay1",     QString::number(e1del));
    add("EmoteDelay2",     QString::number(e2del));
    add("EmoteDelay3",     QString::number(e3del));
    add("SoundEntriesID1", QString::number(s1));
    add("SoundEntriesID2", QString::number(s2));
    add("EmotesID",        QString::number(emId));
    add("Flags",           QString::number(flags));
    add("ConditionID",     QString::number(cond));

    if (editing)
    {
        QStringList setters;
        for (auto const& a : assigns)
            setters << QStringLiteral("%1=%2").arg(a.col).arg(a.valLiteral);
        if (setters.isEmpty())
        {
            QMessageBox::warning(this, tr("Edit broadcast_text"),
                tr("No editable columns matched the live schema."));
            return;
        }
        QString const upd = QStringLiteral(
            "UPDATE %1.broadcast_text SET %2 WHERE ID=%3")
            .arg(m_worldDb).arg(setters.join(QStringLiteral(", "))).arg(editingId);
        if (runInTransaction(QStringList{ upd },
                tr("UPDATE broadcast_text (ID=%1)").arg(editingId)))
            loadRows();
    }
    else
    {
        // PK collision check.  Friendly box beats a raw MySQL dup-key error.
        char checkSql[256];
        std::snprintf(checkSql, sizeof(checkSql),
            "SELECT COUNT(*) FROM %s.broadcast_text WHERE ID=%u",
            m_worldDb.toStdString().c_str(), rowId);
        db::QueryResult cRes;
        auto cErr = m_db->query(checkSql, cRes);
        if (cErr.ok() && cRes.rowCount() > 0 && cRes.asUInt64(0, 0).value_or(0) != 0)
        {
            QMessageBox::warning(this, tr("Add broadcast_text"),
                tr("broadcast_text row already exists for ID=%1.").arg(rowId));
            return;
        }

        QStringList cols;
        QStringList vals;
        cols << QStringLiteral("ID");
        vals << QString::number(rowId);
        for (auto const& a : assigns)
        {
            cols << a.col;
            vals << a.valLiteral;
        }
        QString const ins = QStringLiteral(
            "INSERT INTO %1.broadcast_text (%2) VALUES (%3)")
            .arg(m_worldDb)
            .arg(cols.join(QStringLiteral(", ")))
            .arg(vals.join(QStringLiteral(", ")));
        if (runInTransaction(QStringList{ ins },
                tr("INSERT broadcast_text (ID=%1)").arg(rowId)))
            loadRows();
    }
}

} // namespace world_editor::app
