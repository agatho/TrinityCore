#include "FactionTemplateDock.h"

#include "../db/MySqlClient.h"

#include <QLabel>
#include <QStringList>
#include <QVBoxLayout>

#include <array>
#include <cstdint>
#include <optional>
#include <set>
#include <string>

namespace world_editor::app
{

namespace
{

// MySQL error codes treated as "this fork doesn't carry this table" —
// drives the probe-fallthrough chain.
constexpr uint32_t kErrNoSuchTable  = 1146;
constexpr uint32_t kErrNoSuchColumn = 1054;

// INFORMATION_SCHEMA discovery — same pattern as SpellInfoDock.
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

// Pick the first candidate column that actually exists, else empty.
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

// Project "<picked> AS alias" or "NULL AS alias" so the result-set
// ordinal stays stable regardless of which columns the fork carries.
std::string projectAs(std::set<std::string, std::less<>> const& cols,
                      std::initializer_list<char const*> candidates,
                      char const* alias)
{
    std::string picked = pickColumn(cols, candidates);
    if (picked.empty())
        return std::string("NULL AS ") + alias;
    return picked + " AS " + alias;
}

// FactionTemplate.Flags bit names taken from TC's DBCEnums.h /
// FactionTemplateFlags.  Only the well-documented bits are listed —
// unknown bits are reported as raw "0x..." in the dump.  The names are
// terse so the operator can scan the line quickly.
struct FlagBit { uint32_t mask; char const* name; };
constexpr std::array<FlagBit, 14> kFactionTemplateFlagBits = { {
    { 0x00000001, "PVP" },
    { 0x00000002, "ACCEPT_GREETINGS" },
    { 0x00000004, "HIDE_REPUTATION" },
    { 0x00000008, "AT_PEACE" },
    { 0x00000010, "HATES_ALL_EXCEPT_FRIENDS" },
    { 0x00000020, "PVP_FLAGGED" },
    { 0x00000040, "CONTESTED_GUARD" },
    { 0x00000080, "GUARD" },
    { 0x00000100, "PVP_ATTACKABLE" },
    { 0x00000200, "ATTACK_PVP_ACTIVE_PLAYERS" },
    { 0x00000400, "FLEE_FROM_CALL_FOR_HELP" },
    { 0x00000800, "ASSIST_PLAYERS" },
    { 0x00001000, "FIGHT_CREATURES_BUT_NOT_PLAYERS" },
    { 0x00002000, "AGGRO_HOSTILE_FACTION_BASED" },
} };

QString decodeFlagsHex(uint64_t flags)
{
    QStringList parts;
    uint64_t residual = flags;
    for (auto const& fb : kFactionTemplateFlagBits)
    {
        if (flags & fb.mask)
        {
            parts << QString::fromUtf8(fb.name);
            residual &= ~uint64_t(fb.mask);
        }
    }
    QString hex = QStringLiteral("0x%1").arg(flags, 0, 16);
    if (parts.isEmpty())
        return hex;
    QString joined = parts.join(QStringLiteral(" | "));
    if (residual != 0)
        joined += QStringLiteral(" | 0x%1").arg(residual, 0, 16);
    return QStringLiteral("%1  [%2]").arg(hex).arg(joined);
}

} // namespace

FactionTemplateDock::FactionTemplateDock(db::MySqlClient* dbClient, QWidget* parent)
    : QWidget(parent)
    , m_db(dbClient)
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(6, 6, 6, 6);

    m_header = new QLabel(tr("Select a creature spawn to see its faction template."), this);
    m_header->setWordWrap(true);
    root->addWidget(m_header);

    m_identity = new QLabel(this);
    m_identity->setWordWrap(true);
    m_identity->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_identity->setStyleSheet(QStringLiteral("QLabel { font-family: monospace; }"));
    root->addWidget(m_identity);

    m_flags = new QLabel(this);
    m_flags->setWordWrap(true);
    m_flags->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_flags->setStyleSheet(QStringLiteral("QLabel { font-family: monospace; }"));
    root->addWidget(m_flags);

    m_groups = new QLabel(this);
    m_groups->setWordWrap(true);
    m_groups->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_groups->setStyleSheet(QStringLiteral("QLabel { font-family: monospace; }"));
    root->addWidget(m_groups);

    m_relations = new QLabel(this);
    m_relations->setWordWrap(true);
    m_relations->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_relations->setStyleSheet(QStringLiteral("QLabel { font-family: monospace; }"));
    root->addWidget(m_relations);

    m_notes = new QLabel(this);
    m_notes->setWordWrap(true);
    m_notes->setStyleSheet(QStringLiteral("QLabel { color: gray; }"));
    root->addWidget(m_notes);

    root->addStretch(1);
}

void FactionTemplateDock::clear()
{
    m_header->setText(tr("Select a creature spawn to see its faction template."));
    m_identity->clear();
    m_flags->clear();
    m_groups->clear();
    m_relations->clear();
    m_notes->clear();
}

void FactionTemplateDock::setCreatureEntry(uint32_t entry)
{
    m_identity->clear();
    m_flags->clear();
    m_groups->clear();
    m_relations->clear();
    m_notes->clear();

    if (entry == 0)
    {
        m_header->setText(tr("Select a creature spawn to see its faction template."));
        return;
    }
    if (!m_db || !m_db->isConnected())
    {
        m_header->setText(tr("DB not connected.  Creature entry = %1.").arg(entry));
        return;
    }

    // Step 1: creature_template.faction is the faction-template id; the
    // `creature` row table does NOT carry a faction column in this
    // schema (see SpawnSearchDialog notes).
    uint32_t factionTemplateId = 0;
    {
        std::string const sql =
            "SELECT faction FROM creature_template WHERE entry = " + std::to_string(entry) + " LIMIT 1";
        db::QueryResult res;
        auto const err = m_db->query(sql, res);
        if (!err.ok())
        {
            m_header->setText(tr("creature_template lookup failed: %1")
                .arg(QString::fromStdString(err.message)));
            return;
        }
        if (res.rowCount() == 0)
        {
            m_header->setText(tr("creature_template has no entry %1.").arg(entry));
            return;
        }
        factionTemplateId = static_cast<uint32_t>(res.asUInt64(0, 0).value_or(0));
    }

    if (factionTemplateId == 0)
    {
        m_header->setText(tr("Creature %1 has faction = 0 (no template).").arg(entry));
        return;
    }

    // Step 2: probe the candidate template-mirror tables in priority order.
    char const* const kTemplateTables[] = {
        "faction_template_dbc",        // legacy TC mirror.
        "faction_template",            // alternative naming.
        "serverside_faction_template", // TC custom server-side templates.
    };

    QString notes;
    bool populated = false;
    for (char const* table : kTemplateTables)
    {
        if (tryPopulateFromTable(factionTemplateId, table, notes))
        {
            populated = true;
            m_header->setText(tr("Creature %1 — faction template %2 — source: %3")
                .arg(entry).arg(factionTemplateId).arg(table));
            break;
        }
    }

    if (!populated)
    {
        m_header->setText(tr("Creature %1 — faction template id %2 — "
                             "(faction_template table not in this schema — hotfix DB needed)")
            .arg(entry).arg(factionTemplateId));
        if (!notes.isEmpty())
            m_notes->setText(notes);
    }
    else if (!notes.isEmpty())
    {
        m_notes->setText(notes);
    }
}

bool FactionTemplateDock::tryPopulateFromTable(uint32_t factionTemplateId,
                                               char const* table,
                                               QString& outNote)
{
    auto const cols = discoverColumns(*m_db, table);
    if (cols.empty())
    {
        outNote.append(tr("(%1: table missing in this schema)\n").arg(table));
        return false;
    }

    std::string const idCol = pickColumn(cols, { "ID", "Id", "id", "entry" });
    if (idCol.empty())
    {
        outNote.append(tr("(%1: no id column found)\n").arg(table));
        return false;
    }

    // The 4-slot enemy/friend arrays are usually stored as Enemies1..4
    // / Friend1..4 (TC mirror), but some forks use Enemy[1-4] / Friend[1-4]
    // or _0..3.  Project each slot independently with stable aliases.
    std::string sql = "SELECT ";
    sql += idCol + " AS ft_id, ";
    sql += projectAs(cols, { "Faction", "faction", "ParentFaction" }, "parent_faction") + ", ";
    sql += projectAs(cols, { "Flags", "FactionFlags", "flags" }, "flags") + ", ";
    sql += projectAs(cols, { "FactionGroup", "FactionGroupMask", "factionGroup" }, "faction_group") + ", ";
    sql += projectAs(cols, { "FriendGroup", "FriendlyMask", "friendGroup", "friendlyMask" }, "friend_group") + ", ";
    sql += projectAs(cols, { "EnemyGroup", "EnemyMask", "enemyGroup", "enemyMask" }, "enemy_group") + ", ";
    sql += projectAs(cols, { "Enemies1", "Enemy1", "Enemies_0", "enemy_1" }, "enemy_1") + ", ";
    sql += projectAs(cols, { "Enemies2", "Enemy2", "Enemies_1", "enemy_2" }, "enemy_2") + ", ";
    sql += projectAs(cols, { "Enemies3", "Enemy3", "Enemies_2", "enemy_3" }, "enemy_3") + ", ";
    sql += projectAs(cols, { "Enemies4", "Enemy4", "Enemies_3", "enemy_4" }, "enemy_4") + ", ";
    sql += projectAs(cols, { "Friend1", "Friends1", "Friend_0", "friend_1" }, "friend_1") + ", ";
    sql += projectAs(cols, { "Friend2", "Friends2", "Friend_1", "friend_2" }, "friend_2") + ", ";
    sql += projectAs(cols, { "Friend3", "Friends3", "Friend_2", "friend_3" }, "friend_3") + ", ";
    sql += projectAs(cols, { "Friend4", "Friends4", "Friend_3", "friend_4" }, "friend_4") + " ";
    sql += "FROM " + std::string(table) + " WHERE " + idCol + " = "
        + std::to_string(factionTemplateId) + " LIMIT 1";

    db::QueryResult res;
    auto const err = m_db->query(sql, res);
    if (!err.ok())
    {
        if (err.code == kErrNoSuchTable || err.code == kErrNoSuchColumn)
        {
            outNote.append(tr("(%1: table/column missing in this schema)\n").arg(table));
            return false;
        }
        outNote.append(tr("(%1: query failed: %2)\n").arg(table)
            .arg(QString::fromStdString(err.message)));
        return false;
    }
    if (res.rowCount() == 0)
        return false;

    // Helper: fetch aliased column as uint64; returns nullopt if absent
    // or NULL so we can render "-" for fork-missing fields.
    auto asU64 = [&res](char const* alias) -> std::optional<uint64_t> {
        auto idx = res.columnIndex(alias);
        if (!idx) return std::nullopt;
        if (res.isNull(0, *idx)) return std::nullopt;
        return res.asUInt64(0, *idx);
    };
    auto fmtHex = [](std::optional<uint64_t> v) -> QString {
        if (!v) return QStringLiteral("-");
        return QStringLiteral("0x%1 (%2)").arg(*v, 0, 16).arg(*v);
    };

    // Identity block: template id + parent faction (+ name if resolvable).
    auto parentFaction = asU64("parent_faction");
    QString identity = tr("Template ID:    %1\n").arg(factionTemplateId);
    if (parentFaction)
    {
        QString name = resolveFactionName(static_cast<uint32_t>(*parentFaction));
        if (!name.isEmpty())
            identity += tr("Parent faction: %1  (%2)").arg(*parentFaction).arg(name);
        else
            identity += tr("Parent faction: %1").arg(*parentFaction);
    }
    else
    {
        identity += tr("Parent faction: -");
    }
    m_identity->setText(identity);

    // Flags block: hex + decoded bits.
    auto flags = asU64("flags");
    if (flags)
        m_flags->setText(tr("Flags:          %1").arg(decodeFlagsHex(*flags)));
    else
        m_flags->setText(tr("Flags:          -"));

    // Group-mask block: faction-group / friend-group / enemy-group.
    QStringList gl;
    gl << tr("FactionGroup:   %1").arg(fmtHex(asU64("faction_group")));
    gl << tr("FriendGroup:    %1").arg(fmtHex(asU64("friend_group")));
    gl << tr("EnemyGroup:     %1").arg(fmtHex(asU64("enemy_group")));
    m_groups->setText(gl.join(QChar('\n')));

    // 4-slot friend/enemy lists.  Annotate with parent-faction name
    // when the template id resolves on the same mirror table.
    auto renderSlots = [&](char const* prefix, std::initializer_list<char const*> aliases) -> QString {
        QStringList lines;
        lines << QString::fromUtf8(prefix);
        int slot = 1;
        for (char const* a : aliases)
        {
            auto v = asU64(a);
            if (!v || *v == 0)
            {
                lines << tr("  [%1] -").arg(slot);
            }
            else
            {
                QString lbl = resolveFactionTemplateLabel(static_cast<uint32_t>(*v), table);
                if (lbl.isEmpty())
                    lines << tr("  [%1] %2").arg(slot).arg(*v);
                else
                    lines << tr("  [%1] %2  (%3)").arg(slot).arg(*v).arg(lbl);
            }
            ++slot;
        }
        return lines.join(QChar('\n'));
    };

    QString rel;
    rel += renderSlots("Enemies (4):",
        { "enemy_1", "enemy_2", "enemy_3", "enemy_4" });
    rel += QStringLiteral("\n");
    rel += renderSlots("Friends (4):",
        { "friend_1", "friend_2", "friend_3", "friend_4" });
    m_relations->setText(rel);

    return true;
}

QString FactionTemplateDock::resolveFactionName(uint32_t factionId)
{
    // Probe a `faction_dbc` / `faction` mirror for the parent faction's
    // localized name.  Silent fallthrough on any error — the dock works
    // fine without the name annotation.
    if (!m_db || !m_db->isConnected() || factionId == 0)
        return {};

    char const* const kFactionTables[] = { "faction_dbc", "faction" };
    for (char const* table : kFactionTables)
    {
        auto const cols = discoverColumns(*m_db, table);
        if (cols.empty()) continue;

        std::string const idCol = pickColumn(cols, { "ID", "Id", "id", "entry" });
        if (idCol.empty()) continue;

        std::string const nameCol = pickColumn(cols,
            { "Name_lang", "Name", "name", "FactionName" });
        if (nameCol.empty()) continue;

        std::string sql = "SELECT " + nameCol + " FROM " + std::string(table)
            + " WHERE " + idCol + " = " + std::to_string(factionId) + " LIMIT 1";
        db::QueryResult res;
        auto const err = m_db->query(sql, res);
        if (!err.ok() || res.rowCount() == 0) continue;
        if (res.isNull(0, 0)) continue;
        return QString::fromStdString(res.cell(0, 0));
    }
    return {};
}

QString FactionTemplateDock::resolveFactionTemplateLabel(uint32_t factionTemplateId,
                                                         char const* templateTable)
{
    // Read just the parent faction id off the same template table, then
    // hand it to resolveFactionName.  Keeps the annotation cheap (one
    // SELECT per slot) and consistent with whichever mirror is in use.
    if (!m_db || !m_db->isConnected() || factionTemplateId == 0)
        return {};

    auto const cols = discoverColumns(*m_db, templateTable);
    if (cols.empty()) return {};
    std::string const idCol = pickColumn(cols, { "ID", "Id", "id", "entry" });
    std::string const parentCol = pickColumn(cols, { "Faction", "faction", "ParentFaction" });
    if (idCol.empty() || parentCol.empty()) return {};

    std::string sql = "SELECT " + parentCol + " FROM " + std::string(templateTable)
        + " WHERE " + idCol + " = " + std::to_string(factionTemplateId) + " LIMIT 1";
    db::QueryResult res;
    auto const err = m_db->query(sql, res);
    if (!err.ok() || res.rowCount() == 0) return {};
    if (res.isNull(0, 0)) return {};
    uint32_t parentFaction = static_cast<uint32_t>(res.asUInt64(0, 0).value_or(0));
    return resolveFactionName(parentFaction);
}

} // namespace world_editor::app
