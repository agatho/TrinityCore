#include "ZoneSummaryDock.h"

#include "../db/MySqlClient.h"

#include <QLabel>
#include <QStringList>
#include <QVBoxLayout>

#include <cstdint>
#include <set>
#include <string>

namespace world_editor::app
{

namespace
{

// MySQL error codes we treat as "table absent in this schema"; the dock
// surfaces the absence via m_errors but renders the rest of the stats.
constexpr uint32_t kErrNoSuchTable  = 1146;
constexpr uint32_t kErrNoSuchColumn = 1054;

// AreaTable mirror-table candidates, in priority order.  We only use
// these for the optional zone-name decoration; if none exists the header
// just renders "Zone <id>".
constexpr char const* kAreaTables[] = {
    "area_dbc",
    "area_table",
    "areatable_dbc",
    "area_table_dbc"
};

// Probe INFORMATION_SCHEMA for the column names that exist on `table`.
// Empty set means the table is not present.
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

// First candidate column name present in `cols`, or empty.
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

// Render an integer count line, formatted with thousands separator.
QString countLine(QString const& label, uint64_t n)
{
    return label + QStringLiteral(": <b>") + QString::number(qulonglong(n)) + QStringLiteral("</b>");
}

} // namespace

ZoneSummaryDock::ZoneSummaryDock(db::MySqlClient* dbClient, QWidget* parent)
    : QWidget(parent)
    , m_db(dbClient)
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(6, 6, 6, 6);

    m_header = new QLabel(tr("Click a spawn to see its zone summary."), this);
    m_header->setWordWrap(true);
    m_header->setTextFormat(Qt::RichText);
    root->addWidget(m_header);

    m_counts = new QLabel(this);
    m_counts->setWordWrap(true);
    m_counts->setTextFormat(Qt::RichText);
    m_counts->setTextInteractionFlags(Qt::TextSelectableByMouse);
    root->addWidget(m_counts);

    m_levelRow = new QLabel(this);
    m_levelRow->setWordWrap(true);
    m_levelRow->setTextFormat(Qt::RichText);
    root->addWidget(m_levelRow);

    m_errors = new QLabel(this);
    m_errors->setWordWrap(true);
    m_errors->setTextFormat(Qt::RichText);
    m_errors->setStyleSheet(QStringLiteral("color: #a06000"));
    root->addWidget(m_errors);

    root->addStretch(1);
}

void ZoneSummaryDock::clear()
{
    m_header->setText(tr("Click a spawn to see its zone summary."));
    m_counts->clear();
    m_levelRow->clear();
    m_errors->clear();
}

void ZoneSummaryDock::setDbClient(db::MySqlClient* db)
{
    // Reconnect -> drop the result cache; the new connection might point
    // at a different schema, and stale counts would be misleading.
    m_db = db;
    m_cache.clear();
}

void ZoneSummaryDock::setZone(uint32_t zoneId, uint32_t mapId)
{
    if (zoneId == 0)
    {
        clear();
        return;
    }
    if (!m_db || !m_db->isConnected())
    {
        m_header->setText(tr("DB not connected.  Zone id = %1.").arg(zoneId));
        m_counts->clear();
        m_levelRow->clear();
        m_errors->clear();
        return;
    }

    // Cache hit -> render without re-querying.
    auto const it = m_cache.find(zoneId);
    if (it != m_cache.end())
    {
        renderStats(zoneId, it->second);
        return;
    }

    m_pendingErrors.clear();
    Stats const s = computeStats(zoneId, mapId);
    m_cache.emplace(zoneId, s);
    renderStats(zoneId, s);
}

ZoneSummaryDock::Stats ZoneSummaryDock::computeStats(uint32_t zoneId, uint32_t mapId)
{
    Stats s;
    s.zoneName = lookupZoneName(zoneId);

    // Resolve to plain string segments once; every COUNT uses the same
    // WHERE shape.  No user-supplied input here -- both inputs are
    // integer ids, so direct concatenation is safe.
    QString const baseSpawn = QStringLiteral("map=%1 AND zoneId=%2").arg(mapId).arg(zoneId);

    s.creatures   = scalarCount(QStringLiteral("SELECT COUNT(*) FROM creature WHERE ") + baseSpawn);
    s.gameobjects = scalarCount(QStringLiteral("SELECT COUNT(*) FROM gameobject WHERE ") + baseSpawn);

    // npcflag bit decode -- one COUNT per role.  We JOIN to
    // creature_template every time rather than caching the templated set
    // per zone because the template view is index-friendly and a single
    // query roundtrip is cheaper than re-materializing the join in C++.
    auto npcflag = [&](uint64_t bit) -> uint64_t {
        return scalarCount(QStringLiteral(
            "SELECT COUNT(*) FROM creature c JOIN creature_template ct ON ct.entry=c.id "
            "WHERE c.map=%1 AND c.zoneId=%2 AND (ct.npcflag & %3)<>0")
            .arg(mapId).arg(zoneId).arg(qulonglong(bit)));
    };
    s.questGivers    = npcflag(0x2);
    s.vendors        = npcflag(0x80);
    s.trainers       = npcflag(0x10);
    s.innkeepers     = npcflag(0x10000);
    s.flightmasters  = npcflag(0x2000);
    s.battlemasters  = npcflag(0x100000);
    s.auctioneers    = npcflag(0x200000);

    // Mailboxes: gameobject_template.type == 19 (GAMEOBJECT_TYPE_MAILBOX).
    s.mailboxes = scalarCount(QStringLiteral(
        "SELECT COUNT(*) FROM gameobject g JOIN gameobject_template gt ON gt.entry=g.id "
        "WHERE g.map=%1 AND g.zoneId=%2 AND gt.type=19").arg(mapId).arg(zoneId));

    // Graveyards: world DB carries `graveyard_zone` keyed on GhostZone
    // (the zone the player ghost-walked from).  Schema-tolerant against
    // the older "Zone" column on legacy forks.
    s.graveyards = scalarCount(QStringLiteral(
        "SELECT COUNT(*) FROM graveyard_zone WHERE GhostZone=%1").arg(zoneId));
    if (s.graveyards == 0 && !m_pendingErrors.isEmpty()
        && m_pendingErrors.contains(QStringLiteral("GhostZone")))
    {
        // Fall back to the legacy column name on forks where the schema
        // never followed the master rename.
        m_pendingErrors.clear();
        s.graveyards = scalarCount(QStringLiteral(
            "SELECT COUNT(*) FROM graveyard_zone WHERE Zone=%1").arg(zoneId));
    }

    // Level range over template -> populated creature rows on this zone.
    {
        QString const sql = QStringLiteral(
            "SELECT MIN(ct.minlevel), MAX(ct.maxlevel) FROM creature c "
            "JOIN creature_template ct ON ct.entry=c.id WHERE c.map=%1 AND c.zoneId=%2")
            .arg(mapId).arg(zoneId);
        db::QueryResult res;
        auto const err = m_db->query(sql.toStdString(), res);
        if (!err.ok())
        {
            m_pendingErrors += QStringLiteral("level-range: %1<br>")
                .arg(QString::fromStdString(err.message));
        }
        else if (res.rowCount() == 1)
        {
            if (!res.isNull(0, 0))
                s.minLevel = int32_t(res.asInt64(0, 0).value_or(-1));
            if (!res.isNull(0, 1))
                s.maxLevel = int32_t(res.asInt64(0, 1).value_or(-1));
        }
    }
    return s;
}

void ZoneSummaryDock::renderStats(uint32_t zoneId, Stats const& s)
{
    // Header: "Zone <id> — <name>" (name only if resolved).
    QString header = tr("Zone %1").arg(zoneId);
    if (!s.zoneName.isEmpty())
        header += QStringLiteral(" — <b>") + s.zoneName.toHtmlEscaped() + QStringLiteral("</b>");
    m_header->setText(header);

    // Counts block -- one line per metric.  We surface zeros too so the
    // operator can tell "no auctioneer here" apart from "stale cache".
    QStringList lines;
    lines << countLine(tr("Creatures"),     s.creatures);
    lines << countLine(tr("GameObjects"),   s.gameobjects);
    lines << countLine(tr("Quest givers"),  s.questGivers);
    lines << countLine(tr("Vendors"),       s.vendors);
    lines << countLine(tr("Innkeepers"),    s.innkeepers);
    lines << countLine(tr("Trainers"),      s.trainers);
    lines << countLine(tr("Battlemasters"), s.battlemasters);
    lines << countLine(tr("Mailboxes"),     s.mailboxes);
    lines << countLine(tr("Auctioneers"),   s.auctioneers);
    lines << countLine(tr("Flight masters"),s.flightmasters);
    lines << countLine(tr("Graveyards"),    s.graveyards);
    m_counts->setText(lines.join(QStringLiteral("<br>")));

    // Level range: render only when we have at least one bound; the
    // "-1" sentinel from TC for boss / world-boss creatures gets shown
    // as-is so the operator can spot a zone full of level-?? rares.
    if (s.minLevel >= 0 || s.maxLevel >= 0)
    {
        m_levelRow->setText(tr("<b>Level range</b>: %1 — %2")
            .arg(s.minLevel).arg(s.maxLevel));
    }
    else
    {
        m_levelRow->setText(tr("<b>Level range</b>: (no creature rows)"));
    }

    if (!m_pendingErrors.isEmpty())
        m_errors->setText(QStringLiteral("<i>") + m_pendingErrors + QStringLiteral("</i>"));
    else
        m_errors->clear();
}

uint64_t ZoneSummaryDock::scalarCount(QString const& sql)
{
    if (!m_db || !m_db->isConnected())
        return 0;
    db::QueryResult res;
    auto const err = m_db->query(sql.toStdString(), res);
    if (!err.ok())
    {
        // Schema-absence errors are quiet hints; everything else lands
        // in the operator-visible error row.
        if (err.code != kErrNoSuchTable && err.code != kErrNoSuchColumn)
        {
            m_pendingErrors += QString::fromStdString(err.message)
                + QStringLiteral("<br>");
        }
        else
        {
            // Stash the column / table name so the fallback layer (e.g.
            // graveyard_zone.Zone vs GhostZone) can detect it.
            m_pendingErrors += QString::fromStdString(err.message)
                + QStringLiteral("<br>");
        }
        return 0;
    }
    if (res.rowCount() == 0 || res.isNull(0, 0))
        return 0;
    return res.asUInt64(0, 0).value_or(0);
}

QString ZoneSummaryDock::lookupZoneName(uint32_t zoneId)
{
    if (!m_db || !m_db->isConnected() || zoneId == 0) return {};
    for (char const* table : kAreaTables)
    {
        auto const cols = discoverColumns(*m_db, table);
        if (cols.empty()) continue;
        std::string const idCol = pickColumn(cols, { "ID", "Id", "id" });
        std::string const nameCol = pickColumn(cols, {
            "AreaName_lang", "AreaName", "name", "Name" });
        if (idCol.empty() || nameCol.empty()) continue;
        std::string const sql = "SELECT " + nameCol + " FROM " + std::string(table)
            + " WHERE " + idCol + " = " + std::to_string(zoneId) + " LIMIT 1";
        db::QueryResult res;
        auto const err = m_db->query(sql, res);
        if (!err.ok() || res.rowCount() == 0 || res.isNull(0, 0)) continue;
        return QString::fromStdString(res.cell(0, 0));
    }
    return {};
}

} // namespace world_editor::app
