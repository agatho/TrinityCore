#include "PlayerConditionDock.h"

#include "../db/MySqlClient.h"

#include <QLabel>
#include <QVBoxLayout>

#include <array>
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
// or empty string if none.  Lets us project "ID" / "Id" / "id" without
// three separate SELECTs.
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

// Fetch a cell by alias, treating NULLs and absent columns as empty.
QString cellByAlias(db::QueryResult const& res, char const* alias)
{
    auto idx = res.columnIndex(alias);
    if (!idx) return {};
    if (res.isNull(0, *idx)) return {};
    return QString::fromStdString(res.cell(0, *idx));
}

// Format a multi-line section.  Drops the section entirely if every
// line resolved to empty so the operator doesn't see "Quest gates:"
// headers with no body.
QString formatSection(QString const& header, QStringList const& lines)
{
    QStringList present;
    for (QString const& l : lines)
        if (!l.isEmpty())
            present << l;
    if (present.isEmpty()) return {};
    return QStringLiteral("<b>%1</b><br>%2").arg(header, present.join(QStringLiteral("<br>")));
}

// Render "label: value" if value is non-empty AND not "0" / "0,0,0,..."
// (a row with no rep gates leaves all five faction columns as 0).
QString kv(QString const& label, QString const& value, bool dropZero = true)
{
    if (value.isEmpty()) return {};
    if (dropZero && (value == QStringLiteral("0") || value == QStringLiteral("0.0")))
        return {};
    return label + QStringLiteral(": ") + value.toHtmlEscaped();
}

// Collect a fixed set of N numbered columns (e.g. AchievementID0..3) into
// a single comma-separated string with zeros stripped.  Returns empty
// if every column was NULL/0.
QString collectN(db::QueryResult const& res, char const* aliasPrefix, int n)
{
    QStringList vals;
    for (int i = 0; i < n; ++i)
    {
        std::string alias = std::string(aliasPrefix) + std::to_string(i);
        QString v = cellByAlias(res, alias.c_str());
        if (!v.isEmpty() && v != QStringLiteral("0"))
            vals << v;
    }
    return vals.join(QStringLiteral(", "));
}

// Same as collectN but pairs each entry with the matching value from a
// secondary column family (ItemID0/ItemCount0, QuestID0/QuestState0...).
QString collectPairsN(db::QueryResult const& res,
                      char const* idPrefix,
                      char const* valuePrefix,
                      char const* valueLabel,
                      int n)
{
    QStringList vals;
    for (int i = 0; i < n; ++i)
    {
        std::string idAlias  = std::string(idPrefix)    + std::to_string(i);
        std::string valAlias = std::string(valuePrefix) + std::to_string(i);
        QString id  = cellByAlias(res, idAlias.c_str());
        QString val = cellByAlias(res, valAlias.c_str());
        if (id.isEmpty() || id == QStringLiteral("0"))
            continue;
        if (!val.isEmpty() && val != QStringLiteral("0"))
            vals << QStringLiteral("%1 (%2=%3)").arg(id, valueLabel, val);
        else
            vals << id;
    }
    return vals.join(QStringLiteral(", "));
}

} // namespace

PlayerConditionDock::PlayerConditionDock(db::MySqlClient* dbClient, QWidget* parent)
    : QWidget(parent)
    , m_db(dbClient)
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(6, 6, 6, 6);

    m_header = new QLabel(tr("No PlayerCondition selected.  Conditions dock rows "
                             "that reference PlayerConditionID will trigger this view."), this);
    m_header->setWordWrap(true);
    root->addWidget(m_header);

    auto makeSection = [this, root]() {
        auto* lbl = new QLabel(this);
        lbl->setWordWrap(true);
        lbl->setTextInteractionFlags(Qt::TextSelectableByMouse);
        lbl->setStyleSheet(QStringLiteral("QLabel { font-family: monospace; }"));
        lbl->setTextFormat(Qt::RichText);
        root->addWidget(lbl);
        return lbl;
    };

    m_identity      = makeSection();
    m_restrictions  = makeSection();
    m_questGates    = makeSection();
    m_repGates      = makeSection();
    m_itemSpellAura = makeSection();
    m_achievements  = makeSection();
    m_areaZone      = makeSection();
    m_wseExpr       = makeSection();
    m_flags         = makeSection();

    root->addStretch(1);
}

void PlayerConditionDock::clear()
{
    m_header->setText(tr("No PlayerCondition selected.  Conditions dock rows "
                         "that reference PlayerConditionID will trigger this view."));
    for (QLabel* l : { m_identity, m_restrictions, m_questGates, m_repGates,
                       m_itemSpellAura, m_achievements, m_areaZone,
                       m_wseExpr, m_flags })
        l->clear();
}

void PlayerConditionDock::setPlayerConditionId(uint32_t pcId)
{
    for (QLabel* l : { m_identity, m_restrictions, m_questGates, m_repGates,
                       m_itemSpellAura, m_achievements, m_areaZone,
                       m_wseExpr, m_flags })
        l->clear();

    if (pcId == 0)
    {
        m_header->setText(tr("No PlayerCondition selected.  Conditions dock rows "
                             "that reference PlayerConditionID will trigger this view."));
        return;
    }
    if (!m_db || !m_db->isConnected())
    {
        m_header->setText(tr("DB not connected.  PlayerCondition id = %1.").arg(pcId));
        return;
    }

    // Probe the candidate tables in priority order.  The first table
    // that yields a row wins; later probes are skipped.  Schema notes
    // accumulate so the operator sees which tables are missing.
    char const* const kTables[] = {
        "player_condition",      // most common mirror name.
        "player_condition_dbc",  // some hotfix dumps.
        "playercondition"        // some older forks.
    };

    QString notes;
    bool populated = false;
    for (char const* table : kTables)
    {
        if (tryPopulateFromTable(pcId, table, notes))
        {
            populated = true;
            break;
        }
    }

    if (!populated)
    {
        m_header->setText(tr("no PlayerCondition mirror table found "
                             "(hotfix DBs not connected); PlayerCondition id = %1").arg(pcId));
        if (!notes.isEmpty())
            m_identity->setText(QStringLiteral("<i>%1</i>").arg(notes.toHtmlEscaped()
                .replace(QStringLiteral("\n"), QStringLiteral("<br>"))));
    }
}

bool PlayerConditionDock::tryPopulateFromTable(uint32_t pcId,
                                               char const* table,
                                               QString& outNoteMissing)
{
    auto const cols = discoverColumns(*m_db, table);
    if (cols.empty())
    {
        outNoteMissing.append(tr("(%1: table missing in this schema)\n").arg(table));
        return false;
    }

    // Find the player-condition-id column.  Different forks name it
    // ID / Id / id / entry; we accept all common spellings.
    std::string const idCol = pickColumn(cols, { "ID", "Id", "id", "entry" });
    if (idCol.empty())
    {
        outNoteMissing.append(tr("(%1: no id column found)\n").arg(table));
        return false;
    }

    // Build the projection: every interesting column with a stable
    // alias, "NULL AS alias" when absent so result-set ordinals stay
    // stable regardless of which schema is in use.
    std::vector<std::string> projections;
    projections.push_back(idCol + " AS pc_id");
    projections.push_back(projectAs(cols, { "Flags", "FlagsEx", "flags" }, "flags"));
    // Restrictions block.
    projections.push_back(projectAs(cols, { "RaceMask", "racemask" }, "race_mask"));
    projections.push_back(projectAs(cols, { "ClassMask", "classmask" }, "class_mask"));
    projections.push_back(projectAs(cols, { "Gender", "GenderMask", "gender" }, "gender"));
    projections.push_back(projectAs(cols, { "MinLevel", "minLevel", "minlevel" }, "min_level"));
    projections.push_back(projectAs(cols, { "MaxLevel", "maxLevel", "maxlevel" }, "max_level"));
    // Reputation block.
    projections.push_back(projectAs(cols, { "ReputationLogic", "reputationLogic" }, "rep_logic"));
    for (int i = 0; i < 5; ++i)
    {
        std::string facCol = "ReputationFaction" + std::to_string(i);
        std::string facColAlt = "ReputationFactionID" + std::to_string(i);
        std::string rankCol = "ReputationRank" + std::to_string(i);
        projections.push_back(projectAs(cols, { facCol.c_str(), facColAlt.c_str() },
            ("rep_faction" + std::to_string(i)).c_str()));
        projections.push_back(projectAs(cols, { rankCol.c_str() },
            ("rep_rank" + std::to_string(i)).c_str()));
    }
    // Quest gates (4 slots).
    for (int i = 0; i < 4; ++i)
    {
        std::string qid = "QuestID" + std::to_string(i);
        std::string qst = "QuestState" + std::to_string(i);
        projections.push_back(projectAs(cols, { qid.c_str() },
            ("quest_id" + std::to_string(i)).c_str()));
        projections.push_back(projectAs(cols, { qst.c_str() },
            ("quest_state" + std::to_string(i)).c_str()));
    }
    // Item / Spell / Aura.
    for (int i = 0; i < 4; ++i)
    {
        std::string iid = "ItemID" + std::to_string(i);
        std::string icnt = "ItemCount" + std::to_string(i);
        projections.push_back(projectAs(cols, { iid.c_str() },
            ("item_id" + std::to_string(i)).c_str()));
        projections.push_back(projectAs(cols, { icnt.c_str() },
            ("item_count" + std::to_string(i)).c_str()));
    }
    projections.push_back(projectAs(cols, { "ItemFlags", "itemFlags" }, "item_flags"));
    for (int i = 0; i < 4; ++i)
    {
        std::string sid = "SpellID" + std::to_string(i);
        projections.push_back(projectAs(cols, { sid.c_str() },
            ("spell_id" + std::to_string(i)).c_str()));
    }
    // Aura rows are usually 2 slots (Aura1/Aura2, AuraStacks1/AuraStacks2)
    // but a few forks 0-index them.  Accept both spellings.
    projections.push_back(projectAs(cols, { "Aura1", "Aura0", "AuraSpellID0" }, "aura0"));
    projections.push_back(projectAs(cols, { "Aura2", "Aura1", "AuraSpellID1" }, "aura1"));
    projections.push_back(projectAs(cols, { "AuraStacks1", "AuraStacks0", "AuraCount0" }, "aura_stacks0"));
    projections.push_back(projectAs(cols, { "AuraStacks2", "AuraStacks1", "AuraCount1" }, "aura_stacks1"));
    // Achievements (4 slots).
    for (int i = 0; i < 4; ++i)
    {
        std::string aid = "AchievementID" + std::to_string(i);
        std::string aidAlt = "Achievement" + std::to_string(i);
        projections.push_back(projectAs(cols, { aid.c_str(), aidAlt.c_str() },
            ("achievement_id" + std::to_string(i)).c_str()));
    }
    projections.push_back(projectAs(cols, { "AchievementLogic", "achievementLogic" }, "ach_logic"));
    // Area / zone.
    projections.push_back(projectAs(cols, { "AreaID", "areaId", "AreaId" }, "area_id"));
    projections.push_back(projectAs(cols, { "ZoneSkill", "zoneSkill" }, "zone_skill"));
    projections.push_back(projectAs(cols, { "RangedWeaponType", "rangedWeaponType" }, "ranged_weapon"));
    projections.push_back(projectAs(cols, { "EquippedItemSubClassMask", "equippedItemSubclass" }, "eq_subclass"));
    // World-state expression.
    projections.push_back(projectAs(cols, { "WorldStateExpressionID", "WorldStateExpression",
                                             "worldStateExpression" }, "wse_id"));

    std::string sql = "SELECT ";
    for (size_t i = 0; i < projections.size(); ++i)
    {
        if (i) sql += ", ";
        sql += projections[i];
    }
    sql += " FROM " + std::string(table) + " WHERE " + idCol + " = "
         + std::to_string(pcId) + " LIMIT 1";

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

    m_header->setText(tr("PlayerCondition %1 - source: %2").arg(pcId).arg(table));

    // ---- Identity --------------------------------------------------
    {
        QStringList lines;
        lines << kv(tr("ID"), cellByAlias(res, "pc_id"), false);
        m_identity->setText(formatSection(tr("Identity"), lines));
    }

    // ---- Level / Race / Class / Gender ------------------------------
    {
        QStringList lines;
        lines << kv(tr("MinLevel"), cellByAlias(res, "min_level"));
        lines << kv(tr("MaxLevel"), cellByAlias(res, "max_level"));
        // RaceMask / ClassMask are hex-rendered so the operator can see
        // the bit pattern (race bits map to ChrRaces.db2 IDs).
        auto rm = cellByAlias(res, "race_mask");
        if (!rm.isEmpty() && rm != QStringLiteral("0"))
        {
            bool ok = false;
            qulonglong v = rm.toULongLong(&ok);
            lines << (ok ? tr("RaceMask: %1 (0x%2)").arg(rm).arg(v, 0, 16) : tr("RaceMask: %1").arg(rm));
        }
        auto cm = cellByAlias(res, "class_mask");
        if (!cm.isEmpty() && cm != QStringLiteral("0"))
        {
            bool ok = false;
            qulonglong v = cm.toULongLong(&ok);
            lines << (ok ? tr("ClassMask: %1 (0x%2)").arg(cm).arg(v, 0, 16) : tr("ClassMask: %1").arg(cm));
        }
        lines << kv(tr("Gender"), cellByAlias(res, "gender"));
        m_restrictions->setText(formatSection(tr("Level / Race / Class / Gender"), lines));
    }

    // ---- Quest gates -----------------------------------------------
    {
        // QuestState is a bitmask (rare/complete/turned-in/...) per TC.
        // We surface the raw int and let the operator decode against
        // TC's QuestStatus / QuestObjective bits.
        QString pairs = collectPairsN(res, "quest_id", "quest_state", "state", 4);
        QStringList lines;
        if (!pairs.isEmpty()) lines << tr("Quests: %1").arg(pairs);
        m_questGates->setText(formatSection(tr("Quest gates"), lines));
    }

    // ---- Reputation gates ------------------------------------------
    {
        QStringList lines;
        QString logic = cellByAlias(res, "rep_logic");
        if (!logic.isEmpty() && logic != QStringLiteral("0"))
            lines << tr("Logic: %1").arg(logic);
        QString pairs = collectPairsN(res, "rep_faction", "rep_rank", "rank", 5);
        if (!pairs.isEmpty()) lines << tr("Factions: %1").arg(pairs);
        m_repGates->setText(formatSection(tr("Reputation gates"), lines));
    }

    // ---- Item / spell / aura ---------------------------------------
    {
        QStringList lines;
        QString items = collectPairsN(res, "item_id", "item_count", "count", 4);
        if (!items.isEmpty()) lines << tr("Items: %1").arg(items);
        QString iflags = cellByAlias(res, "item_flags");
        if (!iflags.isEmpty() && iflags != QStringLiteral("0"))
            lines << tr("ItemFlags: %1").arg(iflags);
        QString spells = collectN(res, "spell_id", 4);
        if (!spells.isEmpty()) lines << tr("Spells: %1").arg(spells);
        QString auras = collectPairsN(res, "aura", "aura_stacks", "stacks", 2);
        if (!auras.isEmpty()) lines << tr("Auras: %1").arg(auras);
        m_itemSpellAura->setText(formatSection(tr("Item / Spell / Aura"), lines));
    }

    // ---- Achievements ----------------------------------------------
    {
        QStringList lines;
        QString logic = cellByAlias(res, "ach_logic");
        if (!logic.isEmpty() && logic != QStringLiteral("0"))
            lines << tr("Logic: %1").arg(logic);
        QString ach = collectN(res, "achievement_id", 4);
        if (!ach.isEmpty()) lines << tr("Achievements: %1").arg(ach);
        m_achievements->setText(formatSection(tr("Achievement gates"), lines));
    }

    // ---- Area / zone -----------------------------------------------
    {
        QStringList lines;
        lines << kv(tr("AreaID"),       cellByAlias(res, "area_id"));
        lines << kv(tr("ZoneSkill"),    cellByAlias(res, "zone_skill"));
        lines << kv(tr("RangedWeapon"), cellByAlias(res, "ranged_weapon"));
        auto eq = cellByAlias(res, "eq_subclass");
        if (!eq.isEmpty() && eq != QStringLiteral("0"))
        {
            bool ok = false;
            qulonglong v = eq.toULongLong(&ok);
            lines << (ok ? tr("EquippedItemSubClassMask: %1 (0x%2)").arg(eq).arg(v, 0, 16)
                         : tr("EquippedItemSubClassMask: %1").arg(eq));
        }
        m_areaZone->setText(formatSection(tr("Area / zone"), lines));
    }

    // ---- WorldStateExpression --------------------------------------
    {
        QStringList lines;
        QString wse = cellByAlias(res, "wse_id");
        if (!wse.isEmpty() && wse != QStringLiteral("0"))
            lines << tr("WorldStateExpressionID: %1").arg(wse);
        m_wseExpr->setText(formatSection(tr("WorldStateExpression"), lines));
    }

    // ---- Flags -----------------------------------------------------
    {
        QStringList lines;
        QString fl = cellByAlias(res, "flags");
        if (!fl.isEmpty() && fl != QStringLiteral("0"))
        {
            bool ok = false;
            qulonglong v = fl.toULongLong(&ok);
            lines << (ok ? tr("Flags: %1 (0x%2)").arg(fl).arg(v, 0, 16)
                         : tr("Flags: %1").arg(fl));
        }
        m_flags->setText(formatSection(tr("Flags"), lines));
    }

    return true;
}

} // namespace world_editor::app
