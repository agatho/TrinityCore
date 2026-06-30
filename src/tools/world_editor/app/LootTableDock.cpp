#include "LootTableDock.h"

#include "../db/MySqlClient.h"

#include <QHeaderView>
#include <QLabel>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

#include <set>
#include <vector>

namespace world_editor::app
{

namespace
{

// One projected loot row, either direct from the *_loot_template table
// or inlined from reference_loot_template during recursive expansion.
struct LootRow
{
    uint32_t item          = 0;
    QString  name;
    double   chance        = 0.0;
    int64_t  minCount      = 0;
    int64_t  maxCount      = 0;
    uint32_t lootMode      = 0;
    int32_t  groupId       = 0;
    int32_t  questRequired = 0;
    uint32_t reference     = 0;   // 0 for non-ref rows; original ref id for inlined rows
    bool     fromReference = false;
};

// Pull rows from a single loot table.  `lootTable` is the SQL table name
// (creature_loot_template / gameobject_loot_template / reference_loot_template).
// Returns true on success (rows may be empty).
bool fetchLootRows(db::MySqlClient& db,
                   char const* lootTable,
                   uint32_t entry,
                   std::vector<LootRow>& outRows,
                   QString& outError)
{
    QString const sql = QStringLiteral(
        "SELECT lt.Item, COALESCE(it.name, '') AS name, lt.Chance, "
        "       lt.MinCount, lt.MaxCount, lt.LootMode, lt.GroupId, "
        "       lt.QuestRequired, lt.Reference "
        "FROM %1 lt "
        "LEFT JOIN item_template it ON it.entry = lt.Item "
        "WHERE lt.Entry = %2 "
        "ORDER BY lt.GroupId, lt.Item "
        "LIMIT 2000").arg(QString::fromLatin1(lootTable)).arg(entry);

    db::QueryResult res;
    auto const err = db.query(sql.toStdString(), res);
    if (!err.ok())
    {
        outError = QString::fromStdString(err.message);
        return false;
    }
    outRows.reserve(outRows.size() + res.rowCount());
    for (size_t r = 0; r < res.rowCount(); ++r)
    {
        LootRow row;
        row.item          = static_cast<uint32_t>(res.asUInt64(r, 0).value_or(0));
        row.name          = QString::fromStdString(res.cell(r, 1));
        // Chance lives in a DECIMAL column; cell() yields its text form so
        // we read it through QString to keep fractional precision.
        row.chance        = QString::fromStdString(res.cell(r, 2)).toDouble();
        row.minCount      = res.asInt64 (r, 3).value_or(0);
        row.maxCount      = res.asInt64 (r, 4).value_or(0);
        row.lootMode      = static_cast<uint32_t>(res.asUInt64(r, 5).value_or(0));
        row.groupId       = static_cast<int32_t> (res.asInt64 (r, 6).value_or(0));
        row.questRequired = static_cast<int32_t> (res.asInt64 (r, 7).value_or(0));
        row.reference     = static_cast<uint32_t>(res.asUInt64(r, 8).value_or(0));
        outRows.push_back(std::move(row));
    }
    return true;
}

// Recursively expand `Reference` rows by inlining the matching
// reference_loot_template rows.  `depth` starts at 0 for the top-level
// table; the cap prevents pathological cycles (TC data should not chain
// references this deep but we defend against it anyway).
void expandReferences(db::MySqlClient& db,
                      std::vector<LootRow>& rows,
                      int depth,
                      std::set<uint32_t>& seenRefs)
{
    if (depth >= 3) return;
    std::vector<LootRow> expanded;
    expanded.reserve(rows.size());
    for (LootRow const& row : rows)
    {
        if (row.reference == 0 || seenRefs.count(row.reference) != 0)
        {
            expanded.push_back(row);
            continue;
        }
        // Drop the reference placeholder row itself; inline its contents
        // (TC's LootStore::ProcessLootGroup semantics).
        std::vector<LootRow> refRows;
        QString err;
        seenRefs.insert(row.reference);
        if (!fetchLootRows(db, "reference_loot_template", row.reference, refRows, err))
        {
            // Keep the placeholder visible so the operator sees the ref
            // even if expansion failed (e.g. schema variant).
            expanded.push_back(row);
            continue;
        }
        // One more level of nested references.
        expandReferences(db, refRows, depth + 1, seenRefs);
        for (LootRow& inner : refRows)
        {
            inner.fromReference = true;
            // Preserve the originating reference id for the UI column;
            // nested refs keep their own once expanded.
            if (inner.reference == 0)
                inner.reference = row.reference;
            expanded.push_back(std::move(inner));
        }
    }
    rows = std::move(expanded);
}

} // namespace

LootTableDock::LootTableDock(db::MySqlClient* dbClient, QWidget* parent)
    : QWidget(parent)
    , m_db(dbClient)
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(6, 6, 6, 6);

    m_header = new QLabel(tr("Click a creature or gameobject to see its loot table."), this);
    m_header->setWordWrap(true);
    root->addWidget(m_header);

    m_table = new QTableWidget(this);
    m_table->setColumnCount(8);
    m_table->setHorizontalHeaderLabels({
        tr("item"), tr("name"), tr("chance %"), tr("count"),
        tr("group"), tr("mode"), tr("quest"), tr("ref") });
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_table->verticalHeader()->setVisible(false);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setAlternatingRowColors(true);
    root->addWidget(m_table, 1);

    // Double-click any row -> ask the ItemInfoDock (via MainWindow) to
    // load the item.  Column 0 holds the integer item id we set in
    // loadFromLootTable; reference-expansion rows still carry the
    // resolved item id in column 0, so this works for inlined rows too.
    connect(m_table, &QTableWidget::cellDoubleClicked,
            this, [this](int row, int /*col*/) {
        if (row < 0) return;
        auto* idItem = m_table->item(row, 0);
        if (!idItem) return;
        bool okParse = false;
        uint32_t const id = idItem->text().toUInt(&okParse);
        if (!okParse || id == 0) return;
        emit itemSelected(id);
    });
}

void LootTableDock::clear()
{
    m_table->setRowCount(0);
    m_header->setText(tr("Click a creature or gameobject to see its loot table."));
}

void LootTableDock::setCreatureEntry(uint32_t entry)
{
    m_table->setRowCount(0);
    if (entry == 0)
    {
        m_header->setText(tr("Click a creature or gameobject to see its loot table."));
        return;
    }
    if (!m_db || !m_db->isConnected())
    {
        m_header->setText(tr("DB not connected."));
        return;
    }
    // Resolve creature_template_difficulty.LootID for the (entry, default
    // difficulty 0) row.  We take MAX(LootID) so a creature with multiple
    // difficulty rows still surfaces the populated loot id when the row
    // for difficulty 0 carries 0; the active table is usually identical
    // across difficulties.
    QString const sql = QStringLiteral(
        "SELECT MAX(LootID) FROM creature_template_difficulty "
        "WHERE CreatureID = %1").arg(entry);
    db::QueryResult res;
    auto const err = m_db->query(sql.toStdString(), res);
    if (!err.ok())
    {
        m_header->setText(tr("Query failed: %1")
            .arg(QString::fromStdString(err.message)));
        return;
    }
    uint32_t lootId = 0;
    if (res.rowCount() > 0)
        lootId = static_cast<uint32_t>(res.asUInt64(0, 0).value_or(0));
    if (lootId == 0)
    {
        m_header->setText(tr("Creature entry %1 has no LootID (creature_template_difficulty).")
            .arg(entry));
        return;
    }
    loadFromLootTable("creature_loot_template", "creature", entry, lootId);
}

void LootTableDock::setGameObjectEntry(uint32_t entry)
{
    m_table->setRowCount(0);
    if (entry == 0)
    {
        m_header->setText(tr("Click a creature or gameobject to see its loot table."));
        return;
    }
    if (!m_db || !m_db->isConnected())
    {
        m_header->setText(tr("DB not connected."));
        return;
    }
    // gameobject_template.Data1 is the loot Entry into gameobject_loot_template
    // for GO types where Data1 carries a lootId (chests/fishing/etc).
    // For other GO types Data1 means something else; the LEFT JOIN to
    // gameobject_loot_template naturally returns 0 rows and we display
    // the "no rows" message.
    QString const sql = QStringLiteral(
        "SELECT Data1 FROM gameobject_template WHERE entry = %1").arg(entry);
    db::QueryResult res;
    auto const err = m_db->query(sql.toStdString(), res);
    if (!err.ok())
    {
        m_header->setText(tr("Query failed: %1")
            .arg(QString::fromStdString(err.message)));
        return;
    }
    uint32_t lootId = 0;
    if (res.rowCount() > 0)
        lootId = static_cast<uint32_t>(res.asUInt64(0, 0).value_or(0));
    if (lootId == 0)
    {
        m_header->setText(tr("GO entry %1 has no Data1 loot reference.").arg(entry));
        return;
    }
    loadFromLootTable("gameobject_loot_template", "gameobject", entry, lootId);
}

void LootTableDock::loadFromLootTable(char const* lootTable,
                                      char const* refKindLabel,
                                      uint32_t entry,
                                      uint32_t lootId)
{
    std::vector<LootRow> rows;
    QString errMsg;
    if (!fetchLootRows(*m_db, lootTable, lootId, rows, errMsg))
    {
        m_header->setText(tr("Query failed: %1").arg(errMsg));
        return;
    }
    // Inline reference_loot_template rows (one level deep, recursion cap 3).
    std::set<uint32_t> seenRefs;
    expandReferences(*m_db, rows, 0, seenRefs);

    if (rows.empty())
    {
        m_header->setText(tr("Loot for %1 entry %2 (LootID %3) — 0 row(s)")
            .arg(QString::fromLatin1(refKindLabel)).arg(entry).arg(lootId));
        return;
    }
    m_header->setText(tr("Loot for %1 entry %2 (LootID %3) — %4 row(s)")
        .arg(QString::fromLatin1(refKindLabel)).arg(entry).arg(lootId).arg(rows.size()));

    m_table->setRowCount(int(rows.size()));
    for (size_t r = 0; r < rows.size(); ++r)
    {
        LootRow const& row = rows[r];
        auto setCell = [this, r](int col, QString const& text) {
            auto* cell = new QTableWidgetItem(text);
            cell->setFlags(cell->flags() & ~Qt::ItemIsEditable);
            m_table->setItem(int(r), col, cell);
        };
        setCell(0, QString::number(row.item));
        setCell(1, row.name);
        setCell(2, QString::number(row.chance, 'f', 2));
        setCell(3, row.minCount == row.maxCount
            ? QString::number(row.minCount)
            : QStringLiteral("%1..%2").arg(row.minCount).arg(row.maxCount));
        setCell(4, QString::number(row.groupId));
        // LootMode is a bitmask (LOOT_MODE_DEFAULT=1, etc.); hex keeps
        // bit-by-bit inspection cheap.
        setCell(5, QStringLiteral("0x%1").arg(row.lootMode, 0, 16));
        setCell(6, row.questRequired != 0 ? tr("yes") : tr("-"));
        setCell(7, row.fromReference
            ? QStringLiteral("(ref %1)").arg(row.reference)
            : (row.reference != 0
                ? QStringLiteral("#%1").arg(row.reference)
                : QStringLiteral("-")));
    }
}

} // namespace world_editor::app
