#include "AreaInfoDock.h"

#include "../db/MySqlClient.h"

#include <QLabel>
#include <QVBoxLayout>

#include <cstdint>
#include <set>
#include <string>

namespace world_editor::app
{

namespace
{

// MySQL error codes we treat as "fall through to the next probe".
constexpr uint32_t kErrNoSuchTable  = 1146;
constexpr uint32_t kErrNoSuchColumn = 1054;

// AreaTable mirror-table candidates, in priority order.  Different
// fork snapshots ship different names; first one that exists wins.
constexpr char const* kAreaTables[] = {
    "area_dbc",
    "area_table",
    "areatable_dbc",
    "area_table_dbc"
};

// Map.db2 mirror-table candidates.  Probed lazily for continent
// decoration; not required for the dock to render an area.
constexpr char const* kMapTables[] = {
    "map_dbc",
    "map_table",
    "map_table_dbc",
    "map"
};

// Probe INFORMATION_SCHEMA.COLUMNS for the set of columns present in
// `table` in the currently-selected schema.  Empty on missing table.
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

// First candidate column name present in `cols`, or empty string.
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

// Wrap a picked column as `<picked> AS alias` for positional SELECT.
// Returns "NULL AS alias" when no candidate exists so the result-set
// ordinals stay stable across schema variants.
std::string projectAs(std::set<std::string, std::less<>> const& cols,
                      std::initializer_list<char const*> candidates,
                      char const* alias)
{
    std::string const picked = pickColumn(cols, candidates);
    if (picked.empty())
        return std::string("NULL AS ") + alias;
    return picked + " AS " + alias;
}

// Render "Label: value" if value is non-empty; otherwise empty so the
// caller can drop the line entirely.
QString kv(QString const& label, QString const& value)
{
    if (value.isEmpty()) return {};
    return label + QStringLiteral(": ") + value.toHtmlEscaped();
}

// Hex render of an unsigned 32-bit value (or 64-bit flags blob).
QString hex64(uint64_t v) { return QStringLiteral("0x%1").arg(v, 0, 16); }

// Decode AreaTable.FactionGroupMask into a human-readable bit list.
// Mirrors FactionGroupMask flags from DB2Structure.h: 1=Horde,
// 2=Alliance, 4=Sanctuary, 6=Contested-territory (A|H), and the higher
// bits used by arenas / PvP zones.  We render every recognized bit so
// the operator doesn't have to keep the table in their head.
QString decodeFactionGroupMask(uint64_t mask)
{
    if (mask == 0) return QStringLiteral("(none)");
    QStringList parts;
    if (mask & 0x1) parts << QStringLiteral("Horde");
    if (mask & 0x2) parts << QStringLiteral("Alliance");
    if (mask & 0x4) parts << QStringLiteral("Sanctuary");
    if ((mask & 0x3) == 0x3) parts << QStringLiteral("(contested)");
    if (mask & 0x8) parts << QStringLiteral("PvP");
    if (mask & 0x10) parts << QStringLiteral("Arena");
    uint64_t known = 0x1f;
    if (mask & ~known)
        parts << QStringLiteral("unk=0x%1").arg(mask & ~known, 0, 16);
    return parts.join(QStringLiteral(", "));
}

} // namespace

AreaInfoDock::AreaInfoDock(db::MySqlClient* dbClient, QWidget* parent)
    : QWidget(parent)
    , m_db(dbClient)
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(6, 6, 6, 6);

    m_header = new QLabel(tr("Click a spawn to see its zone / area info."), this);
    m_header->setWordWrap(true);
    root->addWidget(m_header);

    auto makeSection = [this, root]() -> QLabel* {
        auto* l = new QLabel(this);
        l->setWordWrap(true);
        l->setTextInteractionFlags(Qt::TextSelectableByMouse);
        l->setTextFormat(Qt::RichText);
        root->addWidget(l);
        return l;
    };

    // Vertical-stack: each section auto-hides itself when it has no
    // content (sound block on outdoor areas with no ambient, etc.).
    m_identity    = makeSection();
    m_hierarchy   = makeSection();
    m_exploration = makeSection();
    m_sound       = makeSection();
    m_faction     = makeSection();
    m_misc        = makeSection();
    root->addStretch(1);
}

void AreaInfoDock::clear()
{
    m_header->setText(tr("Click a spawn to see its zone / area info."));
    m_identity->clear();
    m_hierarchy->clear();
    m_exploration->clear();
    m_sound->clear();
    m_faction->clear();
    m_misc->clear();
}

void AreaInfoDock::setArea(uint32_t areaId)
{
    m_identity->clear();
    m_hierarchy->clear();
    m_exploration->clear();
    m_sound->clear();
    m_faction->clear();
    m_misc->clear();

    if (areaId == 0)
    {
        m_header->setText(tr("Click a spawn to see its zone / area info."));
        return;
    }
    if (!m_db || !m_db->isConnected())
    {
        m_header->setText(tr("DB not connected.  Area id = %1.").arg(areaId));
        return;
    }

    // Probe mirror-table candidates in priority order; first hit wins.
    QString notes;
    for (char const* table : kAreaTables)
    {
        if (tryPopulateFromTable(areaId, table, notes))
            return;
    }

    m_header->setText(tr("no area info table found (AreaTable.db2 mirror absent); area id = %1")
        .arg(areaId));
    if (!notes.isEmpty())
        m_identity->setText(notes);
}

bool AreaInfoDock::tryPopulateFromTable(uint32_t areaId,
                                        char const* table,
                                        QString& outNoteMissing)
{
    auto const cols = discoverColumns(*m_db, table);
    if (cols.empty())
    {
        outNoteMissing.append(tr("(%1: table missing in this schema)<br>").arg(table));
        return false;
    }

    std::string const idCol = pickColumn(cols, { "ID", "Id", "id" });
    if (idCol.empty())
    {
        outNoteMissing.append(tr("(%1: no id column found)<br>").arg(table));
        return false;
    }

    // Project every interesting AreaTable.db2 attribute with a stable
    // alias.  Missing source columns project as "NULL AS alias" so the
    // accessor by alias keeps working across schema variants.
    std::string sql = "SELECT ";
    sql += idCol + " AS area_id, ";
    sql += projectAs(cols, { "AreaName_lang", "AreaName", "name", "Name" }, "name") + ", ";
    sql += projectAs(cols, { "ContinentID", "MapID", "continent_id", "map_id" }, "continent_id") + ", ";
    sql += projectAs(cols, { "ParentAreaID", "ParentAreaId", "parent_area_id" }, "parent_id") + ", ";
    sql += projectAs(cols, { "AreaBit", "ExplorationBit", "area_bit" }, "area_bit") + ", ";
    sql += projectAs(cols, { "ExplorationLevel", "exploration_level" }, "exploration_level") + ", ";
    sql += projectAs(cols, { "Flags", "Flags1", "flags" }, "flags") + ", ";
    sql += projectAs(cols, { "FactionGroupMask", "faction_group_mask" }, "faction_mask") + ", ";
    sql += projectAs(cols, { "SoundProviderPref", "sound_provider_pref" }, "sound_pref") + ", ";
    sql += projectAs(cols, { "SoundProviderPrefUnderwater", "sound_provider_pref_underwater" }, "sound_pref_uw") + ", ";
    sql += projectAs(cols, { "AmbienceID", "Ambience", "ambience_id" }, "ambience") + ", ";
    sql += projectAs(cols, { "ZoneMusic", "zone_music" }, "zone_music") + ", ";
    sql += projectAs(cols, { "IntroSound", "intro_sound" }, "intro_sound") + ", ";
    sql += projectAs(cols, { "UWIntroSound", "uw_intro_sound" }, "uw_intro_sound") + ", ";
    sql += projectAs(cols, { "UWZoneMusic", "uw_zone_music" }, "uw_zone_music") + ", ";
    sql += projectAs(cols, { "UWAmbience", "uw_ambience" }, "uw_ambience") + ", ";
    sql += projectAs(cols, { "MountFlags", "mount_flags" }, "mount_flags") + ", ";
    sql += projectAs(cols, { "MinElevation", "min_elevation" }, "min_elevation") + ", ";
    sql += projectAs(cols, { "AmbientMultiplier", "ambient_multiplier" }, "ambient_mult") + " ";
    sql += "FROM " + std::string(table) + " WHERE " + idCol + " = " + std::to_string(areaId) + " LIMIT 1";

    db::QueryResult res;
    auto const err = m_db->query(sql, res);
    if (!err.ok())
    {
        if (err.code == kErrNoSuchTable || err.code == kErrNoSuchColumn)
        {
            outNoteMissing.append(tr("(%1: table/column missing in this schema)<br>").arg(table));
            return false;
        }
        outNoteMissing.append(tr("(%1: query failed: %2)<br>").arg(table)
            .arg(QString::fromStdString(err.message)));
        return false;
    }
    if (res.rowCount() == 0)
        return false;

    // Cell-by-alias accessor: empty for NULL or absent aliases so the
    // formatter can drop empty lines.
    auto cellStr = [&res](char const* alias) -> QString {
        auto idx = res.columnIndex(alias);
        if (!idx) return {};
        if (res.isNull(0, *idx)) return {};
        return QString::fromStdString(res.cell(0, *idx));
    };
    auto cellU64 = [&res](char const* alias) -> uint64_t {
        auto idx = res.columnIndex(alias);
        if (!idx) return 0;
        if (res.isNull(0, *idx)) return 0;
        return res.asUInt64(0, *idx).value_or(0);
    };

    QString const name        = cellStr("name");
    uint32_t const continentId = uint32_t(cellU64("continent_id"));
    uint32_t const parentId   = uint32_t(cellU64("parent_id"));

    m_header->setText(tr("Area %1 — source: %2%3")
        .arg(areaId)
        .arg(table)
        .arg(name.isEmpty() ? QString() : QStringLiteral(" — <b>") + name.toHtmlEscaped() + QStringLiteral("</b>")));

    // ---- Identity section ----
    {
        QStringList lines;
        lines << kv(tr("ID"), QString::number(areaId));
        if (!name.isEmpty())
            lines << kv(tr("Name"), name);
        if (parentId != 0)
        {
            QString parentName = lookupAreaName(table, parentId);
            QString parentLabel = QString::number(parentId);
            if (!parentName.isEmpty())
                parentLabel += QStringLiteral(" (") + parentName.toHtmlEscaped() + QStringLiteral(")");
            lines << kv(tr("Parent area"), parentLabel);
        }
        if (continentId != 0)
        {
            QString contName = lookupContinentName(continentId);
            QString contLabel = QString::number(continentId);
            if (!contName.isEmpty())
                contLabel += QStringLiteral(" (") + contName.toHtmlEscaped() + QStringLiteral(")");
            lines << kv(tr("Continent"), contLabel);
        }
        m_identity->setText(QStringLiteral("<b>Identity</b><br>") + lines.join(QStringLiteral("<br>")));
    }

    // ---- Hierarchy breadcrumb ----
    if (parentId != 0)
    {
        QString const crumb = buildBreadcrumb(table, areaId);
        if (!crumb.isEmpty())
            m_hierarchy->setText(QStringLiteral("<b>Hierarchy</b><br>") + crumb.toHtmlEscaped());
    }

    // ---- Exploration ----
    {
        QStringList lines;
        lines << kv(tr("ExplorationLevel"), cellStr("exploration_level"));
        lines << kv(tr("AreaBit"), cellStr("area_bit"));
        QStringList present;
        for (QString const& l : lines) if (!l.isEmpty()) present << l;
        if (!present.isEmpty())
            m_exploration->setText(QStringLiteral("<b>Exploration</b><br>") + present.join(QStringLiteral("<br>")));
    }

    // ---- Sound ----
    {
        QStringList lines;
        lines << kv(tr("ZoneMusic"), cellStr("zone_music"));
        lines << kv(tr("AmbienceID"), cellStr("ambience"));
        lines << kv(tr("IntroSound"), cellStr("intro_sound"));
        lines << kv(tr("SoundProviderPref"), cellStr("sound_pref"));
        lines << kv(tr("UWZoneMusic"), cellStr("uw_zone_music"));
        lines << kv(tr("UWAmbience"), cellStr("uw_ambience"));
        lines << kv(tr("UWIntroSound"), cellStr("uw_intro_sound"));
        lines << kv(tr("SoundProviderPrefUnderwater"), cellStr("sound_pref_uw"));
        QStringList present;
        for (QString const& l : lines) if (!l.isEmpty() && !l.endsWith(QStringLiteral(": 0"))) present << l;
        if (!present.isEmpty())
            m_sound->setText(QStringLiteral("<b>Sound</b><br>") + present.join(QStringLiteral("<br>")));
    }

    // ---- Faction ----
    {
        uint64_t const mask = cellU64("faction_mask");
        QStringList lines;
        lines << kv(tr("FactionGroupMask"), hex64(mask) + QStringLiteral(" — ") + decodeFactionGroupMask(mask));
        m_faction->setText(QStringLiteral("<b>Faction</b><br>") + lines.join(QStringLiteral("<br>")));
    }

    // ---- Misc ----
    {
        QStringList lines;
        uint64_t const flags = cellU64("flags");
        if (flags != 0)
            lines << kv(tr("Flags"), hex64(flags));
        uint64_t const mountFlags = cellU64("mount_flags");
        if (mountFlags != 0)
            lines << kv(tr("MountFlags"), hex64(mountFlags));
        QString const minElev = cellStr("min_elevation");
        if (!minElev.isEmpty() && minElev != QStringLiteral("0") && minElev != QStringLiteral("0.0"))
            lines << kv(tr("MinElevation"), minElev);
        QString const ambMult = cellStr("ambient_mult");
        if (!ambMult.isEmpty() && ambMult != QStringLiteral("0") && ambMult != QStringLiteral("0.0"))
            lines << kv(tr("AmbientMultiplier"), ambMult);
        if (!lines.isEmpty())
            m_misc->setText(QStringLiteral("<b>Misc</b><br>") + lines.join(QStringLiteral("<br>")));
    }

    return true;
}

QString AreaInfoDock::lookupAreaName(char const* table, uint32_t areaId)
{
    if (!m_db || !m_db->isConnected() || areaId == 0) return {};
    auto const cols = discoverColumns(*m_db, table);
    if (cols.empty()) return {};
    std::string const idCol = pickColumn(cols, { "ID", "Id", "id" });
    std::string const nameCol = pickColumn(cols, { "AreaName_lang", "AreaName", "name", "Name" });
    if (idCol.empty() || nameCol.empty()) return {};
    std::string const sql = "SELECT " + nameCol + " FROM " + std::string(table)
        + " WHERE " + idCol + " = " + std::to_string(areaId) + " LIMIT 1";
    db::QueryResult res;
    auto const err = m_db->query(sql, res);
    if (!err.ok() || res.rowCount() == 0 || res.isNull(0, 0)) return {};
    return QString::fromStdString(res.cell(0, 0));
}

QString AreaInfoDock::buildBreadcrumb(char const* table, uint32_t areaId)
{
    if (!m_db || !m_db->isConnected()) return {};
    auto const cols = discoverColumns(*m_db, table);
    if (cols.empty()) return {};
    std::string const idCol = pickColumn(cols, { "ID", "Id", "id" });
    std::string const parentCol = pickColumn(cols, { "ParentAreaID", "ParentAreaId", "parent_area_id" });
    std::string const nameCol = pickColumn(cols, { "AreaName_lang", "AreaName", "name", "Name" });
    if (idCol.empty() || parentCol.empty()) return {};

    // Walk parent chain bottom-up; cap recursion at 6 to defang any
    // cycle in dodgy fork data.  We prepend each level so the final
    // order is "Root > ... > Current".
    QStringList chain;
    uint32_t cur = areaId;
    for (int depth = 0; depth < 6 && cur != 0; ++depth)
    {
        std::string sql = "SELECT " + idCol + " AS aid, "
            + parentCol + " AS pid";
        if (!nameCol.empty()) sql += ", " + nameCol + " AS aname";
        sql += " FROM " + std::string(table) + " WHERE " + idCol + " = " + std::to_string(cur) + " LIMIT 1";
        db::QueryResult res;
        auto const err = m_db->query(sql, res);
        if (!err.ok() || res.rowCount() == 0) break;
        QString label = QString::number(cur);
        auto nameIdx = res.columnIndex("aname");
        if (nameIdx && !res.isNull(0, *nameIdx))
            label = QString::fromStdString(res.cell(0, *nameIdx));
        chain.prepend(label);
        auto pidIdx = res.columnIndex("pid");
        if (!pidIdx || res.isNull(0, *pidIdx)) break;
        uint32_t const parent = uint32_t(res.asUInt64(0, *pidIdx).value_or(0));
        if (parent == 0 || parent == cur) break;
        cur = parent;
    }
    if (chain.size() < 2) return {};
    return chain.join(QStringLiteral(" > "));
}

QString AreaInfoDock::lookupContinentName(uint32_t mapId)
{
    if (!m_db || !m_db->isConnected() || mapId == 0) return {};
    for (char const* table : kMapTables)
    {
        auto const cols = discoverColumns(*m_db, table);
        if (cols.empty()) continue;
        std::string const idCol = pickColumn(cols, { "ID", "Id", "id" });
        std::string const nameCol = pickColumn(cols, { "MapName_lang", "MapName", "name", "Name" });
        if (idCol.empty() || nameCol.empty()) continue;
        std::string const sql = "SELECT " + nameCol + " FROM " + std::string(table)
            + " WHERE " + idCol + " = " + std::to_string(mapId) + " LIMIT 1";
        db::QueryResult res;
        auto const err = m_db->query(sql, res);
        if (!err.ok() || res.rowCount() == 0 || res.isNull(0, 0)) continue;
        return QString::fromStdString(res.cell(0, 0));
    }
    return {};
}

} // namespace world_editor::app
