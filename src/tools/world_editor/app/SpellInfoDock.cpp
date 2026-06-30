#include "SpellInfoDock.h"

#include "../db/MySqlClient.h"

#include <QHeaderView>
#include <QLabel>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

#include <set>
#include <string>
#include <vector>

namespace world_editor::app
{

namespace
{

// MySQL error codes we treat as "fall through to the next probe".
constexpr uint32_t kErrNoSuchTable  = 1146;
constexpr uint32_t kErrNoSuchColumn = 1054;

// Probe INFORMATION_SCHEMA.COLUMNS for the set of columns present in
// `table` in the currently-selected schema.  Returns empty on missing
// table (1146) or any other error; the caller treats empty as
// "table absent here".
std::set<std::string, std::less<>> discoverColumns(db::MySqlClient& db, char const* table)
{
    std::set<std::string, std::less<>> cols;
    // DATABASE() resolves to the schema the connection is bound to,
    // so the operator doesn't have to spell it out.
    std::string const sql =
        "SELECT COLUMN_NAME FROM INFORMATION_SCHEMA.COLUMNS "
        "WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = '" + std::string(table) + "'";
    db::QueryResult res;
    auto const err = db.query(sql, res);
    if (!err.ok()) return cols;
    for (size_t r = 0; r < res.rowCount(); ++r)
        cols.insert(res.cell(r, 0));
    return cols;
}

// Pick the first column name from `candidates` that exists in `cols`,
// or empty string if none.  Lets us project "SpellName_lang" or
// "Name" or "name" without three separate SELECTs.
std::string pickColumn(std::set<std::string, std::less<>> const& cols,
                       std::initializer_list<char const*> candidates)
{
    for (char const* c : candidates)
    {
        auto it = cols.find(c);
        if (it != cols.end())
            return *it;
    }
    return {};
}

// Wrap a picked column as `<picked> AS alias` for SELECT projection.
// Returns "NULL AS alias" when no candidate exists so the projection
// stays positional and we don't have to track sparse output columns.
std::string projectAs(std::set<std::string, std::less<>> const& cols,
                      std::initializer_list<char const*> candidates,
                      char const* alias)
{
    std::string picked = pickColumn(cols, candidates);
    if (picked.empty())
        return std::string("NULL AS ") + alias;
    return picked + " AS " + alias;
}

} // namespace

SpellInfoDock::SpellInfoDock(db::MySqlClient* dbClient, QWidget* parent)
    : QWidget(parent)
    , m_db(dbClient)
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(6, 6, 6, 6);

    m_header = new QLabel(tr("Select a spell-referencing smart-script row to see its spell info."), this);
    m_header->setWordWrap(true);
    root->addWidget(m_header);

    m_summary = new QLabel(this);
    m_summary->setWordWrap(true);
    m_summary->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_summary->setStyleSheet(QStringLiteral("QLabel { font-family: monospace; }"));
    root->addWidget(m_summary);

    m_effectsHeader = new QLabel(this);
    m_effectsHeader->setWordWrap(true);
    root->addWidget(m_effectsHeader);

    m_effects = new QTableWidget(this);
    m_effects->setColumnCount(8);
    m_effects->setHorizontalHeaderLabels({
        tr("idx"), tr("Effect"), tr("AuraName"), tr("BasePts"),
        tr("MiscVal"), tr("TriggerSpell"), tr("TargetA"), tr("TargetB") });
    m_effects->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_effects->verticalHeader()->setVisible(false);
    m_effects->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_effects->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_effects->setAlternatingRowColors(true);
    root->addWidget(m_effects, 1);
}

void SpellInfoDock::clear()
{
    m_header->setText(tr("Select a spell-referencing smart-script row to see its spell info."));
    m_summary->clear();
    m_effectsHeader->clear();
    m_effects->setRowCount(0);
}

void SpellInfoDock::setSpell(uint32_t spellId)
{
    m_summary->clear();
    m_effectsHeader->clear();
    m_effects->setRowCount(0);

    if (spellId == 0)
    {
        m_header->setText(tr("Select a spell-referencing smart-script row to see its spell info."));
        return;
    }
    if (!m_db || !m_db->isConnected())
    {
        m_header->setText(tr("DB not connected.  Spell id = %1.").arg(spellId));
        return;
    }

    // Probe the candidate tables in priority order.  The first table
    // that yields a row wins; later probes are skipped.  Schema notes
    // accumulate so the operator sees which tables are missing.
    char const* const kSummaryTables[] = {
        "spell_dbc",        // TC mirror table (legacy).
        "spell_template",   // some older forks.
        "serverside_spell"  // TC custom serverside spells.
    };

    QString notes;
    bool populated = false;
    for (char const* table : kSummaryTables)
    {
        if (tryPopulateFromTable(spellId, table, notes))
        {
            populated = true;
            // The matching summary table dictates the effects-table
            // probe name: spell_dbc -> spell_effect_dbc, spell_template
            // -> spell_template_effect, serverside_spell ->
            // serverside_spell_effect.  Try the most likely sibling
            // first, fall through to the others on 1146.
            char const* const kEffectCandidates[] = {
                "spell_effect_dbc",
                "spell_template_effect",
                "serverside_spell_effect",
                "spell_effect" };
            for (char const* eff : kEffectCandidates)
            {
                tryPopulateEffects(spellId, eff);
                if (m_effects->rowCount() > 0)
                    break;
            }
            break;
        }
    }

    if (!populated)
    {
        m_header->setText(tr("no spell info table found (hotfix DBs not connected); spell id = %1")
            .arg(spellId));
        if (!notes.isEmpty())
            m_summary->setText(notes);
    }
}

bool SpellInfoDock::tryPopulateFromTable(uint32_t spellId,
                                         char const* table,
                                         QString& outNoteMissing)
{
    auto const cols = discoverColumns(*m_db, table);
    if (cols.empty())
    {
        outNoteMissing.append(tr("(%1: table missing in this schema)\n").arg(table));
        return false;
    }

    // Find the spell-id column.  Different forks name it ID / Id /
    // entry / spellID; we accept all common spellings.
    std::string const idCol = pickColumn(cols, { "ID", "Id", "id", "entry", "spellID", "SpellID" });
    if (idCol.empty())
    {
        outNoteMissing.append(tr("(%1: no id column found)\n").arg(table));
        return false;
    }

    // Project every interesting attribute with a stable alias.  When
    // the source column is absent we emit "NULL AS alias" so the
    // result-set ordinal stays fixed.
    std::string sql = "SELECT ";
    sql += idCol + " AS spell_id, ";
    sql += projectAs(cols, { "SpellName_lang", "Name", "name", "spellName" }, "name") + ", ";
    sql += projectAs(cols, { "SchoolMask", "School", "schoolMask", "school" }, "school") + ", ";
    sql += projectAs(cols, { "CastingTimeIndex", "CastTime", "castTime", "casting_time" }, "cast_time") + ", ";
    sql += projectAs(cols, { "RecoveryTime", "recoveryTime", "Cooldown", "cooldown" }, "cooldown") + ", ";
    sql += projectAs(cols, { "RangeIndex", "Range", "range", "rangeIndex" }, "range_") + ", ";
    sql += projectAs(cols, { "StartRecoveryTime", "start_recovery_time" }, "start_recovery") + ", ";
    sql += projectAs(cols, { "Description_lang", "Description", "description" }, "description") + " ";
    sql += "FROM " + std::string(table) + " WHERE " + idCol + " = " + std::to_string(spellId) + " LIMIT 1";

    db::QueryResult res;
    auto const err = m_db->query(sql, res);
    if (!err.ok())
    {
        if (err.code == kErrNoSuchTable || err.code == kErrNoSuchColumn)
        {
            outNoteMissing.append(tr("(%1: table/column missing in this schema)\n").arg(table));
            return false;
        }
        outNoteMissing.append(tr("(%1: query failed: %2)\n").arg(table)
            .arg(QString::fromStdString(err.message)));
        return false;
    }
    if (res.rowCount() == 0)
        return false;

    // Render the summary block.  Each line skips itself if the source
    // column wasn't present (alias projected as NULL).
    auto cellStr = [&res](char const* alias) -> QString {
        auto idx = res.columnIndex(alias);
        if (!idx) return {};
        if (res.isNull(0, *idx)) return {};
        return QString::fromStdString(res.cell(0, *idx));
    };

    QStringList lines;
    lines << tr("ID:          %1").arg(QString::fromStdString(res.cell(0, 0)));
    if (auto v = cellStr("name");           !v.isEmpty()) lines << tr("Name:        %1").arg(v);
    if (auto v = cellStr("school");         !v.isEmpty()) lines << tr("School:      %1").arg(v);
    if (auto v = cellStr("cast_time");      !v.isEmpty()) lines << tr("Cast time:   %1").arg(v);
    if (auto v = cellStr("cooldown");       !v.isEmpty()) lines << tr("Cooldown:    %1 ms").arg(v);
    if (auto v = cellStr("range_");         !v.isEmpty()) lines << tr("Range:       %1").arg(v);
    if (auto v = cellStr("start_recovery"); !v.isEmpty()) lines << tr("Recovery:    %1 ms").arg(v);
    if (auto v = cellStr("description");    !v.isEmpty()) lines << tr("Description: %1").arg(v);

    m_header->setText(tr("Spell %1 — source: %2").arg(spellId).arg(table));
    QString body = lines.join(QChar('\n'));
    if (!outNoteMissing.isEmpty())
        body.append(QStringLiteral("\n\n")).append(outNoteMissing);
    m_summary->setText(body);
    return true;
}

void SpellInfoDock::tryPopulateEffects(uint32_t spellId, char const* effectTable)
{
    auto const cols = discoverColumns(*m_db, effectTable);
    if (cols.empty())
    {
        // Silent fallthrough; the summary table tells us which family
        // we're in, and missing sibling effect tables are expected.
        return;
    }

    // Locate the spell-id reference column.  spell_dbc-style mirrors
    // use SpellID; older forks use spellId / entry.
    std::string const spellIdCol = pickColumn(cols, { "SpellID", "spellID", "spellId", "spell_id", "entry" });
    if (spellIdCol.empty())
        return;

    std::string sql = "SELECT ";
    sql += projectAs(cols, { "EffectIndex", "effectIndex", "EffectIdx", "idx" }, "idx") + ", ";
    sql += projectAs(cols, { "Effect", "effect", "EffectId" }, "effect") + ", ";
    sql += projectAs(cols, { "EffectAura", "ApplyAuraName", "applyAuraName", "AuraName", "aura_name" }, "aura") + ", ";
    sql += projectAs(cols, { "EffectBasePoints", "BasePoints", "basePoints", "base_points" }, "base_points") + ", ";
    sql += projectAs(cols, { "EffectMiscValue", "EffectMiscValue0", "MiscValue", "miscValue" }, "misc_value") + ", ";
    sql += projectAs(cols, { "EffectTriggerSpell", "TriggerSpell", "triggerSpell" }, "trigger_spell") + ", ";
    sql += projectAs(cols, { "ImplicitTarget0", "ImplicitTargetA", "EffectImplicitTargetA", "implicit_target_a" }, "target_a") + ", ";
    sql += projectAs(cols, { "ImplicitTarget1", "ImplicitTargetB", "EffectImplicitTargetB", "implicit_target_b" }, "target_b") + " ";
    sql += "FROM " + std::string(effectTable) + " WHERE " + spellIdCol + " = " + std::to_string(spellId) + " ";
    sql += "ORDER BY idx LIMIT 64";

    db::QueryResult res;
    auto const err = m_db->query(sql, res);
    if (!err.ok())
    {
        if (err.code == kErrNoSuchTable || err.code == kErrNoSuchColumn)
            return;
        m_effectsHeader->setText(tr("(%1: query failed: %2)").arg(effectTable)
            .arg(QString::fromStdString(err.message)));
        return;
    }
    if (res.rowCount() == 0)
        return;

    m_effectsHeader->setText(tr("Effects (%1 row(s) from %2):")
        .arg(res.rowCount()).arg(effectTable));
    m_effects->setRowCount(int(res.rowCount()));
    auto putCell = [this](int r, int c, std::string const& v) {
        auto* item = new QTableWidgetItem(QString::fromStdString(v));
        item->setFlags(item->flags() & ~Qt::ItemIsEditable);
        m_effects->setItem(r, c, item);
    };
    for (size_t r = 0; r < res.rowCount(); ++r)
    {
        for (int c = 0; c < 8; ++c)
        {
            // Null cells render as empty so the operator visually sees
            // "this fork's schema doesn't have that column".
            if (res.isNull(r, size_t(c)))
                putCell(int(r), c, {});
            else
                putCell(int(r), c, res.cell(r, size_t(c)));
        }
    }
}

} // namespace world_editor::app
