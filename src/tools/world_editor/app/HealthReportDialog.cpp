#include "HealthReportDialog.h"

#include "../db/MySqlClient.h"

#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <set>
#include <string>

namespace world_editor::app
{

namespace
{
constexpr int kRoleMapId  = Qt::UserRole + 0;
constexpr int kRoleWorldX = Qt::UserRole + 1;
constexpr int kRoleWorldY = Qt::UserRole + 2;

// Probe INFORMATION_SCHEMA for the column set of a table so the quest-chain
// scans can tolerate fork-schema variations (LogTitle vs Title, NextQuestInChain
// vs NextQuestId, PrevQuestID absent on some forks, Method vs Flags etc.).
std::set<std::string, std::less<>> discoverColumns(world_editor::db::MySqlClient& db, char const* table)
{
    std::set<std::string, std::less<>> cols;
    std::string const sql =
        "SELECT COLUMN_NAME FROM INFORMATION_SCHEMA.COLUMNS "
        "WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = '" + std::string(table) + "'";
    world_editor::db::QueryResult res;
    auto const err = db.query(sql, res);
    if (!err.ok()) return cols;
    for (size_t r = 0; r < res.rowCount(); ++r)
        cols.insert(res.cell(r, 0));
    return cols;
}

// Project `col AS alias` when present, "NULL AS alias" otherwise; keeps
// the result-set column layout stable regardless of which fork-schema the
// operator is connected to.
std::string projectAs(std::set<std::string, std::less<>> const& cols,
                      char const* col, char const* alias)
{
    if (cols.find(col) != cols.end())
        return std::string(col) + " AS " + alias;
    return std::string("NULL AS ") + alias;
}

// Return the first column name in `candidates` that exists in `cols`, or
// empty string if none match. Used to pick the right NextQuestInChain /
// PrevQuestID / LogTitle column for the WHERE clause itself (projectAs is
// only safe for the SELECT list).
std::string pickColumn(std::set<std::string, std::less<>> const& cols,
                       std::initializer_list<char const*> candidates)
{
    for (char const* c : candidates)
        if (cols.find(c) != cols.end())
            return std::string(c);
    return {};
}
}

HealthReportDialog::HealthReportDialog(db::MySqlClient* dbClient,
                                       std::optional<uint32_t> currentMapId,
                                       QWidget* parent)
    : QDialog(parent)
    , m_db(dbClient)
    , m_currentMapId(currentMapId)
{
    setWindowTitle(tr("Health report"));
    resize(900, 640);

    auto* root = new QVBoxLayout(this);

    auto* hint = new QLabel(
        m_currentMapId.has_value()
            ? tr("Read-only validation sweep, scoped to map %1.\n"
                 "Double-click a finding with a location to jump there.\n"
                 "Use the per-category Fix button to delete every offender in a single transaction.")
                .arg(*m_currentMapId)
            : tr("Read-only validation sweep across the whole DB (no map loaded).\n"
                 "Use the per-category Fix button to delete every offender in a single transaction."),
        this);
    hint->setWordWrap(true);
    root->addWidget(hint);

    m_tree = new QTreeWidget(this);
    m_tree->setColumnCount(2);
    m_tree->setHeaderLabels({ tr("Category / Finding"), tr("Location / detail") });
    m_tree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_tree->header()->setSectionResizeMode(1, QHeaderView::Interactive);
    m_tree->header()->resizeSection(1, 240);
    connect(m_tree, &QTreeWidget::itemActivated,
            this, &HealthReportDialog::onItemActivated);
    root->addWidget(m_tree);

    auto* buttons = new QDialogButtonBox(this);
    m_refresh = buttons->addButton(tr("Refresh"), QDialogButtonBox::ActionRole);
    m_close   = buttons->addButton(QDialogButtonBox::Close);
    connect(m_refresh, &QPushButton::clicked, this, &HealthReportDialog::onRefresh);
    connect(m_close,   &QPushButton::clicked, this, &QDialog::accept);
    root->addWidget(buttons);

    runScan();
}

QTreeWidgetItem* HealthReportDialog::addCategory(Category& cat)
{
    auto* it = new QTreeWidgetItem(m_tree);
    it->setText(0, cat.title);
    QFont f = it->font(0);
    f.setBold(true);
    it->setFont(0, f);
    it->setExpanded(true);
    cat.item = it;
    return it;
}

void HealthReportDialog::addFinding(QTreeWidgetItem* category,
                                    QString const& summary,
                                    std::optional<uint32_t> mapId,
                                    std::optional<float> worldX,
                                    std::optional<float> worldY)
{
    auto* leaf = new QTreeWidgetItem(category);
    leaf->setText(0, summary);
    if (mapId.has_value() && worldX.has_value() && worldY.has_value())
    {
        leaf->setText(1, QStringLiteral("map=%1  (%2, %3)")
            .arg(*mapId)
            .arg(*worldX, 0, 'f', 1)
            .arg(*worldY, 0, 'f', 1));
        leaf->setData(0, kRoleMapId,  *mapId);
        leaf->setData(0, kRoleWorldX, *worldX);
        leaf->setData(0, kRoleWorldY, *worldY);
    }
}

void HealthReportDialog::attachFixButton(Category& cat)
{
    if (cat.deleteSql.isEmpty() || cat.findingCount <= 0 || !cat.item)
        return;
    auto* btn = new QPushButton(tr("Fix %1 row(s)").arg(cat.findingCount), m_tree);
    btn->setToolTip(tr("Delete every offender in this category inside a single transaction.\n"
                       "You will be asked to confirm before anything writes to the DB."));
    btn->setStyleSheet(QStringLiteral("QPushButton { padding: 2px 8px; }"));
    QString const deleteSql = cat.deleteSql;
    int     const cnt       = cat.findingCount;
    QString const title     = cat.title;
    connect(btn, &QPushButton::clicked, this, [this, deleteSql, cnt, title]() {
        Category snapshot;
        snapshot.deleteSql    = deleteSql;
        snapshot.findingCount = cnt;
        snapshot.title        = title;
        runFix(snapshot);
    });
    m_tree->setItemWidget(cat.item, 1, btn);
}

void HealthReportDialog::runFix(Category const& cat)
{
    auto const confirm = QMessageBox::question(this, tr("Fix %1").arg(cat.title),
        tr("This will run a DELETE against %1 row(s) inside a transaction:\n\n"
           "%2\n\nProceed?").arg(cat.findingCount).arg(cat.deleteSql),
        QMessageBox::Ok | QMessageBox::Cancel);
    if (confirm != QMessageBox::Ok) return;

    if (!m_db || !m_db->isConnected())
    {
        QMessageBox::warning(this, tr("Not connected"),
            tr("Lost DB connection between scan and fix - reconnect and re-run."));
        return;
    }

    uint64_t affected = 0;
    auto const beginErr = m_db->exec("START TRANSACTION");
    if (!beginErr.ok())
    {
        QMessageBox::critical(this, tr("Fix failed"),
            tr("Could not start transaction: %1")
              .arg(QString::fromStdString(beginErr.message)));
        return;
    }
    auto const delErr = m_db->exec(cat.deleteSql.toStdString(), &affected);
    if (!delErr.ok())
    {
        (void)m_db->exec("ROLLBACK");
        QMessageBox::critical(this, tr("Fix failed"),
            tr("DELETE failed and was rolled back:\n%1")
              .arg(QString::fromStdString(delErr.message)));
        return;
    }
    auto const commitErr = m_db->exec("COMMIT");
    if (!commitErr.ok())
    {
        (void)m_db->exec("ROLLBACK");
        QMessageBox::critical(this, tr("Fix failed"),
            tr("COMMIT failed and was rolled back:\n%1")
              .arg(QString::fromStdString(commitErr.message)));
        return;
    }

    QMessageBox::information(this, tr("Fixed"),
        tr("Deleted %1 row(s) for category '%2'.\nThe scan will now refresh.")
            .arg(affected).arg(cat.title));
    onRefresh();
}

void HealthReportDialog::onRefresh()
{
    m_tree->clear();
    runScan();
}

void HealthReportDialog::onItemActivated(QTreeWidgetItem* item, int /*column*/)
{
    if (!item) return;
    QVariant const mv = item->data(0, kRoleMapId);
    QVariant const xv = item->data(0, kRoleWorldX);
    QVariant const yv = item->data(0, kRoleWorldY);
    if (!mv.isValid() || !xv.isValid() || !yv.isValid())
        return;
    emit jumpRequested(mv.toUInt(), xv.toFloat(), yv.toFloat());
}

void HealthReportDialog::runScan()
{
    if (!m_db || !m_db->isConnected())
    {
        Category bad;
        bad.title = tr("Database not connected");
        addCategory(bad);
        addFinding(bad.item, tr("Connect to the world DB first."));
        return;
    }

    QString const mapFilterC = m_currentMapId.has_value()
        ? QStringLiteral(" AND c.map = %1").arg(*m_currentMapId)
        : QString();
    QString const mapFilterG = m_currentMapId.has_value()
        ? QStringLiteral(" AND g.map = %1").arg(*m_currentMapId)
        : QString();
    QString const mapFilterAtr = m_currentMapId.has_value()
        ? QStringLiteral(" AND at.MapId = %1").arg(*m_currentMapId)
        : QString();
    // Self-join variant: both sides must be on the loaded map. Filter against a.MapId
    // since the join already forces b.MapId = a.MapId.
    QString const mapFilterAtrPair = m_currentMapId.has_value()
        ? QStringLiteral(" AND a.MapId = %1").arg(*m_currentMapId)
        : QString();
    QString const mapFilterCreaturePair = m_currentMapId.has_value()
        ? QStringLiteral(" AND c1.map = %1").arg(*m_currentMapId)
        : QString();
    QString const mapFilterGy = m_currentMapId.has_value()
        ? QStringLiteral(" AND wsl.MapId = %1").arg(*m_currentMapId)
        : QString();

    // ---- 1. Paths with < 2 nodes -----------------------------------
    {
        Category cat;
        cat.title = tr("Paths with < 2 nodes");
        cat.deleteSql = QStringLiteral(
            "DELETE wp FROM waypoint_path wp "
            "LEFT JOIN (SELECT PathId, COUNT(*) AS n FROM waypoint_path_node GROUP BY PathId) wn "
            "  ON wn.PathId = wp.PathId "
            "WHERE COALESCE(wn.n, 0) < 2");
        addCategory(cat);

        db::QueryResult res;
        QString const sql = QStringLiteral(
            "SELECT wp.PathId, COALESCE(nodeCount, 0) "
            "FROM waypoint_path wp "
            "LEFT JOIN (SELECT PathId, COUNT(*) AS nodeCount FROM waypoint_path_node GROUP BY PathId) wn "
            "  ON wn.PathId = wp.PathId "
            "WHERE COALESCE(wn.nodeCount, 0) < 2 "
            "LIMIT 500");
        auto const err = m_db->query(sql.toStdString(), res);
        if (!err.ok())
        {
            addFinding(cat.item, tr("scan failed: %1")
                .arg(QString::fromStdString(err.message)));
        }
        else if (res.rowCount() == 0)
        {
            addFinding(cat.item, tr("OK -- no orphaned paths"));
        }
        else
        {
            cat.findingCount = int(res.rowCount());
            for (size_t r = 0; r < res.rowCount(); ++r)
            {
                addFinding(cat.item, tr("PathId %1 has only %2 node(s)")
                    .arg(res.asUInt64(r, 0).value_or(0))
                    .arg(res.asUInt64(r, 1).value_or(0)));
            }
            if (res.rowCount() >= 500)
                addFinding(cat.item, tr("(... truncated at 500 rows.)"));
        }
        attachFixButton(cat);
    }

    // ---- 2. Orphaned smart_scripts (creature side) -----------------
    {
        Category cat;
        cat.title = tr("Smart scripts orphaned by missing creature/GO");
        cat.deleteSql = QStringLiteral(
            "DELETE ss FROM smart_scripts ss "
            "WHERE (ss.entryorguid < 0 "
            "       AND NOT EXISTS (SELECT 1 FROM creature c WHERE c.guid = -ss.entryorguid)) "
            "   OR (ss.entryorguid > 0 AND ss.source_type = 0 "
            "       AND NOT EXISTS (SELECT 1 FROM creature_template ct WHERE ct.entry = ss.entryorguid)) "
            "   OR (ss.entryorguid > 0 AND ss.source_type = 1 "
            "       AND NOT EXISTS (SELECT 1 FROM gameobject_template gt WHERE gt.entry = ss.entryorguid))");
        addCategory(cat);

        QString sql = QStringLiteral(
            "SELECT entryorguid, source_type, id, link "
            "FROM smart_scripts ss "
            "WHERE (ss.entryorguid < 0 "
            "       AND NOT EXISTS (SELECT 1 FROM creature c WHERE c.guid = -ss.entryorguid)) "
            "   OR (ss.entryorguid > 0 AND ss.source_type = 0 "
            "       AND NOT EXISTS (SELECT 1 FROM creature_template ct WHERE ct.entry = ss.entryorguid)) "
            "   OR (ss.entryorguid > 0 AND ss.source_type = 1 "
            "       AND NOT EXISTS (SELECT 1 FROM gameobject_template gt WHERE gt.entry = ss.entryorguid)) "
            "LIMIT 500");
        db::QueryResult res;
        auto const err = m_db->query(sql.toStdString(), res);
        if (!err.ok())
        {
            addFinding(cat.item, tr("scan failed: %1")
                .arg(QString::fromStdString(err.message)));
        }
        else if (res.rowCount() == 0)
        {
            addFinding(cat.item, tr("OK -- no orphaned smart_scripts rows"));
        }
        else
        {
            cat.findingCount = int(res.rowCount());
            for (size_t r = 0; r < res.rowCount(); ++r)
            {
                addFinding(cat.item, tr("entryorguid=%1 source_type=%2 id=%3 link=%4")
                    .arg(res.asInt64 (r, 0).value_or(0))
                    .arg(res.asUInt64(r, 1).value_or(0))
                    .arg(res.asUInt64(r, 2).value_or(0))
                    .arg(res.asUInt64(r, 3).value_or(0)));
            }
            if (res.rowCount() >= 500)
                addFinding(cat.item, tr("(... truncated at 500 rows.)"));
        }
        attachFixButton(cat);
    }

    // ---- 3. Areatriggers without create-properties row -------------
    {
        Category cat;
        cat.title = tr("Areatriggers without matching create_properties");
        // Map-scoped delete when a map is loaded so a Fix click can't
        // accidentally nuke offenders on other maps.
        cat.deleteSql = QStringLiteral(
            "DELETE at FROM areatrigger at "
            "LEFT JOIN areatrigger_create_properties acp "
            "  ON acp.Id = at.AreaTriggerCreatePropertiesId "
            " AND acp.IsCustom = at.IsCustom "
            "WHERE acp.Id IS NULL%1").arg(mapFilterAtr);
        addCategory(cat);

        QString const sql = QStringLiteral(
            "SELECT at.SpawnId, at.AreaTriggerCreatePropertiesId, at.IsCustom, "
            "       at.MapId, at.PosX, at.PosY "
            "FROM areatrigger at "
            "LEFT JOIN areatrigger_create_properties acp "
            "  ON acp.Id = at.AreaTriggerCreatePropertiesId "
            " AND acp.IsCustom = at.IsCustom "
            "WHERE acp.Id IS NULL%1 "
            "LIMIT 500").arg(mapFilterAtr);
        db::QueryResult res;
        auto const err = m_db->query(sql.toStdString(), res);
        if (!err.ok())
        {
            addFinding(cat.item, tr("scan failed: %1")
                .arg(QString::fromStdString(err.message)));
        }
        else if (res.rowCount() == 0)
        {
            addFinding(cat.item, tr("OK -- every areatrigger has a create_properties parent"));
        }
        else
        {
            cat.findingCount = int(res.rowCount());
            for (size_t r = 0; r < res.rowCount(); ++r)
            {
                uint64_t const sid = res.asUInt64(r, 0).value_or(0);
                uint64_t const cpid = res.asUInt64(r, 1).value_or(0);
                uint64_t const isc = res.asUInt64(r, 2).value_or(0);
                uint32_t const mid = uint32_t(res.asUInt64(r, 3).value_or(0));
                float    const x   = float(res.asDouble(r, 4).value_or(0.0));
                float    const y   = float(res.asDouble(r, 5).value_or(0.0));
                addFinding(cat.item,
                    tr("SpawnId %1 references missing createProps Id=%2 IsCustom=%3")
                        .arg(sid).arg(cpid).arg(isc),
                    mid, x, y);
            }
            if (res.rowCount() >= 500)
                addFinding(cat.item, tr("(... truncated at 500 rows.)"));
        }
        attachFixButton(cat);
    }

    // ---- 4. Graveyards outside graveyard_zone ----------------------
    {
        Category cat;
        cat.title = tr("Graveyards with no graveyard_zone link");
        cat.deleteSql = QStringLiteral(
            "DELETE wsl FROM world_safe_locs wsl "
            "LEFT JOIN graveyard_zone gz ON gz.ID = wsl.ID "
            "WHERE gz.ID IS NULL%1").arg(mapFilterGy);
        addCategory(cat);

        QString const sql = QStringLiteral(
            "SELECT wsl.ID, wsl.MapId, wsl.LocX, wsl.LocY "
            "FROM world_safe_locs wsl "
            "LEFT JOIN graveyard_zone gz ON gz.ID = wsl.ID "
            "WHERE gz.ID IS NULL%1 "
            "LIMIT 500").arg(mapFilterGy);
        db::QueryResult res;
        auto const err = m_db->query(sql.toStdString(), res);
        if (!err.ok())
        {
            addFinding(cat.item, tr("scan failed: %1")
                .arg(QString::fromStdString(err.message)));
        }
        else if (res.rowCount() == 0)
        {
            addFinding(cat.item, tr("OK -- every graveyard has at least one zone link"));
        }
        else
        {
            cat.findingCount = int(res.rowCount());
            for (size_t r = 0; r < res.rowCount(); ++r)
            {
                uint32_t const gid = uint32_t(res.asUInt64(r, 0).value_or(0));
                uint32_t const mid = uint32_t(res.asUInt64(r, 1).value_or(0));
                float    const x   = float(res.asDouble(r, 2).value_or(0.0));
                float    const y   = float(res.asDouble(r, 3).value_or(0.0));
                addFinding(cat.item,
                    tr("Graveyard ID %1 has no graveyard_zone rows").arg(gid),
                    mid, x, y);
            }
            if (res.rowCount() >= 500)
                addFinding(cat.item, tr("(... truncated at 500 rows.)"));
        }
        attachFixButton(cat);
    }

    // ---- 5. GO spawns with no template ----------------------------
    {
        Category cat;
        cat.title = tr("Gameobject spawns with no gameobject_template");
        cat.deleteSql = QStringLiteral(
            "DELETE g FROM gameobject g "
            "LEFT JOIN gameobject_template gt ON gt.entry = g.id "
            "WHERE gt.entry IS NULL%1").arg(mapFilterG);
        addCategory(cat);

        QString const sql = QStringLiteral(
            "SELECT g.guid, g.id, g.map, g.position_x, g.position_y "
            "FROM gameobject g "
            "LEFT JOIN gameobject_template gt ON gt.entry = g.id "
            "WHERE gt.entry IS NULL%1 "
            "LIMIT 500").arg(mapFilterG);
        db::QueryResult res;
        auto const err = m_db->query(sql.toStdString(), res);
        if (!err.ok())
        {
            addFinding(cat.item, tr("scan failed: %1")
                .arg(QString::fromStdString(err.message)));
        }
        else if (res.rowCount() == 0)
        {
            addFinding(cat.item, tr("OK -- every gameobject spawn has a template"));
        }
        else
        {
            cat.findingCount = int(res.rowCount());
            for (size_t r = 0; r < res.rowCount(); ++r)
            {
                int64_t  const guid = res.asInt64 (r, 0).value_or(0);
                uint32_t const eid  = uint32_t(res.asUInt64(r, 1).value_or(0));
                uint32_t const mid  = uint32_t(res.asUInt64(r, 2).value_or(0));
                float    const x    = float(res.asDouble(r, 3).value_or(0.0));
                float    const y    = float(res.asDouble(r, 4).value_or(0.0));
                addFinding(cat.item,
                    tr("GO guid=%1 references missing entry=%2").arg(guid).arg(eid),
                    mid, x, y);
            }
            if (res.rowCount() >= 500)
                addFinding(cat.item, tr("(... truncated at 500 rows.)"));
        }
        attachFixButton(cat);
    }

    // ---- 6. Creature spawns with no template ----------------------
    {
        Category cat;
        cat.title = tr("Creature spawns with no creature_template");
        cat.deleteSql = QStringLiteral(
            "DELETE c FROM creature c "
            "LEFT JOIN creature_template ct ON ct.entry = c.id "
            "WHERE ct.entry IS NULL%1").arg(mapFilterC);
        addCategory(cat);

        QString const sql = QStringLiteral(
            "SELECT c.guid, c.id, c.map, c.position_x, c.position_y "
            "FROM creature c "
            "LEFT JOIN creature_template ct ON ct.entry = c.id "
            "WHERE ct.entry IS NULL%1 "
            "LIMIT 500").arg(mapFilterC);
        db::QueryResult res;
        auto const err = m_db->query(sql.toStdString(), res);
        if (!err.ok())
        {
            addFinding(cat.item, tr("scan failed: %1")
                .arg(QString::fromStdString(err.message)));
        }
        else if (res.rowCount() == 0)
        {
            addFinding(cat.item, tr("OK -- every creature spawn has a template"));
        }
        else
        {
            cat.findingCount = int(res.rowCount());
            for (size_t r = 0; r < res.rowCount(); ++r)
            {
                int64_t  const guid = res.asInt64 (r, 0).value_or(0);
                uint32_t const eid  = uint32_t(res.asUInt64(r, 1).value_or(0));
                uint32_t const mid  = uint32_t(res.asUInt64(r, 2).value_or(0));
                float    const x    = float(res.asDouble(r, 3).value_or(0.0));
                float    const y    = float(res.asDouble(r, 4).value_or(0.0));
                addFinding(cat.item,
                    tr("Creature guid=%1 references missing entry=%2").arg(guid).arg(eid),
                    mid, x, y);
            }
            if (res.rowCount() >= 500)
                addFinding(cat.item, tr("(... truncated at 500 rows.)"));
        }
        attachFixButton(cat);
    }

    // ---- 7. SAI CALL_TIMED_ACTIONLIST targets that don't exist ----
    // action_type=80 references action-list id in action_param1; the list
    // must exist as smart_scripts rows with source_type=9 AND entryorguid=<param1>.
    {
        Category cat;
        cat.title = tr("Smart-script CALL_TIMED_ACTIONLIST targets that don't exist");
        cat.deleteSql = QStringLiteral(
            "DELETE ss FROM smart_scripts ss "
            "WHERE ss.action_type = 80 "
            "  AND NOT EXISTS ("
            "    SELECT 1 FROM (SELECT entryorguid FROM smart_scripts WHERE source_type = 9) sub "
            "    WHERE sub.entryorguid = ss.action_param1)");
        addCategory(cat);

        QString const sql = QStringLiteral(
            "SELECT entryorguid, source_type, id, link, action_param1 "
            "FROM smart_scripts ss "
            "WHERE ss.action_type = 80 "
            "  AND NOT EXISTS ("
            "    SELECT 1 FROM (SELECT entryorguid FROM smart_scripts WHERE source_type = 9) sub "
            "    WHERE sub.entryorguid = ss.action_param1) "
            "LIMIT 500");
        db::QueryResult res;
        auto const err = m_db->query(sql.toStdString(), res);
        if (!err.ok())
        {
            addFinding(cat.item, tr("scan failed: %1")
                .arg(QString::fromStdString(err.message)));
        }
        else if (res.rowCount() == 0)
        {
            addFinding(cat.item, tr("OK -- every CALL_TIMED_ACTIONLIST target exists"));
        }
        else
        {
            cat.findingCount = int(res.rowCount());
            for (size_t r = 0; r < res.rowCount(); ++r)
            {
                addFinding(cat.item, tr("entryorguid=%1 source_type=%2 id=%3 link=%4 -> missing action_list %5")
                    .arg(res.asInt64 (r, 0).value_or(0))
                    .arg(res.asUInt64(r, 1).value_or(0))
                    .arg(res.asUInt64(r, 2).value_or(0))
                    .arg(res.asUInt64(r, 3).value_or(0))
                    .arg(res.asUInt64(r, 4).value_or(0)));
            }
            if (res.rowCount() >= 500)
                addFinding(cat.item, tr("(... truncated at 500 rows.)"));
        }
        attachFixButton(cat);
    }

    // ---- 8. SAI linked rules pointing at non-existent ids ---------
    // Use the less-destructive UPDATE link=0 form rather than DELETE so
    // we don't drop the rule body itself - only break its dangling link.
    {
        Category cat;
        cat.title = tr("Smart-script linked rules pointing at non-existent ids");
        cat.deleteSql = QStringLiteral(
            "UPDATE smart_scripts a SET a.link = 0 "
            "WHERE a.link <> 0 "
            "  AND NOT EXISTS ("
            "    SELECT 1 FROM (SELECT entryorguid, source_type, id FROM smart_scripts) b "
            "    WHERE b.entryorguid = a.entryorguid "
            "      AND b.source_type = a.source_type "
            "      AND b.id = a.link)");
        addCategory(cat);

        QString const sql = QStringLiteral(
            "SELECT a.entryorguid, a.source_type, a.id, a.link "
            "FROM smart_scripts a "
            "WHERE a.link <> 0 "
            "  AND NOT EXISTS ("
            "    SELECT 1 FROM smart_scripts b "
            "    WHERE b.entryorguid = a.entryorguid "
            "      AND b.source_type = a.source_type "
            "      AND b.id = a.link) "
            "LIMIT 500");
        db::QueryResult res;
        auto const err = m_db->query(sql.toStdString(), res);
        if (!err.ok())
        {
            addFinding(cat.item, tr("scan failed: %1")
                .arg(QString::fromStdString(err.message)));
        }
        else if (res.rowCount() == 0)
        {
            addFinding(cat.item, tr("OK -- every smart_scripts link resolves"));
        }
        else
        {
            cat.findingCount = int(res.rowCount());
            for (size_t r = 0; r < res.rowCount(); ++r)
            {
                addFinding(cat.item, tr("entryorguid=%1 source_type=%2 id=%3 -> dangling link=%4")
                    .arg(res.asInt64 (r, 0).value_or(0))
                    .arg(res.asUInt64(r, 1).value_or(0))
                    .arg(res.asUInt64(r, 2).value_or(0))
                    .arg(res.asUInt64(r, 3).value_or(0)));
            }
            if (res.rowCount() >= 500)
                addFinding(cat.item, tr("(... truncated at 500 rows.)"));
        }
        attachFixButton(cat);
    }

    // ---- 9. SAI action lists with no caller -----------------------
    // source_type=9 (action_list) rows whose entryorguid no action_type=80
    // references anywhere. DELETE-friendly: dead-code in the SAI table.
    {
        Category cat;
        cat.title = tr("Smart-script action lists with no caller");
        cat.deleteSql = QStringLiteral(
            "DELETE ss FROM smart_scripts ss "
            "WHERE ss.source_type = 9 "
            "  AND NOT EXISTS ("
            "    SELECT 1 FROM (SELECT action_param1 FROM smart_scripts WHERE action_type = 80) c "
            "    WHERE c.action_param1 = ss.entryorguid)");
        addCategory(cat);

        QString const sql = QStringLiteral(
            "SELECT DISTINCT entryorguid FROM smart_scripts "
            "WHERE source_type = 9 "
            "  AND NOT EXISTS ("
            "    SELECT 1 FROM smart_scripts c "
            "    WHERE c.action_type = 80 AND c.action_param1 = entryorguid) "
            "LIMIT 500");
        db::QueryResult res;
        auto const err = m_db->query(sql.toStdString(), res);
        if (!err.ok())
        {
            addFinding(cat.item, tr("scan failed: %1")
                .arg(QString::fromStdString(err.message)));
        }
        else if (res.rowCount() == 0)
        {
            addFinding(cat.item, tr("OK -- every action_list has a caller"));
        }
        else
        {
            cat.findingCount = int(res.rowCount());
            for (size_t r = 0; r < res.rowCount(); ++r)
            {
                addFinding(cat.item, tr("orphan action_list entryorguid=%1 (no action_type=80 caller)")
                    .arg(res.asInt64(r, 0).value_or(0)));
            }
            if (res.rowCount() >= 500)
                addFinding(cat.item, tr("(... truncated at 500 rows.)"));
        }
        attachFixButton(cat);
    }

    // ---- 10. Waypoint paths with no creature assigned -------------
    // Single-statement fix limitation: MySqlClient::exec runs one stmt
    // per call (mysql_real_query, no CLIENT_MULTI_STATEMENTS), so we
    // delete only the orphan waypoint_path_node child rows. Parent
    // waypoint_path entries are left so the operator can re-investigate
    // (rename a comment, reassign a creature, etc.) before nuking them.
    {
        Category cat;
        cat.title = tr("Waypoint paths with no creature assigned");
        cat.deleteSql = QStringLiteral(
            "DELETE wpn FROM waypoint_path_node wpn "
            "WHERE NOT EXISTS ("
            "  SELECT 1 FROM creature c WHERE c.currentwaypoint = wpn.PathId)");
        addCategory(cat);

        QString const sql = QStringLiteral(
            "SELECT wp.PathId, COALESCE(wp.Comment, '') AS comment "
            "FROM waypoint_path wp "
            "WHERE NOT EXISTS ("
            "  SELECT 1 FROM creature c WHERE c.currentwaypoint = wp.PathId) "
            "LIMIT 500");
        db::QueryResult res;
        auto const err = m_db->query(sql.toStdString(), res);
        if (!err.ok())
        {
            addFinding(cat.item, tr("scan failed: %1")
                .arg(QString::fromStdString(err.message)));
        }
        else if (res.rowCount() == 0)
        {
            addFinding(cat.item, tr("OK -- every waypoint_path is referenced by a creature"));
        }
        else
        {
            cat.findingCount = int(res.rowCount());
            for (size_t r = 0; r < res.rowCount(); ++r)
            {
                addFinding(cat.item, tr("PathId %1 -- '%2'")
                    .arg(res.asUInt64(r, 0).value_or(0))
                    .arg(QString::fromStdString(res.isNull(r, 1) ? std::string() : res.cell(r, 1))));
            }
            if (res.rowCount() >= 500)
                addFinding(cat.item, tr("(... truncated at 500 rows.)"));
        }
        attachFixButton(cat);
    }

    // ---- 11. Pool templates with zero members ---------------------
    {
        Category cat;
        cat.title = tr("Pool templates with zero members");
        cat.deleteSql = QStringLiteral(
            "DELETE pt FROM pool_template pt "
            "WHERE NOT EXISTS (SELECT 1 FROM pool_creature WHERE pool_entry = pt.entry) "
            "  AND NOT EXISTS (SELECT 1 FROM pool_gameobject WHERE pool_entry = pt.entry) "
            "  AND NOT EXISTS (SELECT 1 FROM pool_pool WHERE child_pool_id = pt.entry)");
        addCategory(cat);

        QString const sql = QStringLiteral(
            "SELECT pt.entry, COALESCE(pt.description, '') AS d FROM pool_template pt "
            "WHERE NOT EXISTS (SELECT 1 FROM pool_creature WHERE pool_entry = pt.entry) "
            "  AND NOT EXISTS (SELECT 1 FROM pool_gameobject WHERE pool_entry = pt.entry) "
            "  AND NOT EXISTS (SELECT 1 FROM pool_pool WHERE child_pool_id = pt.entry) "
            "LIMIT 500");
        db::QueryResult res;
        auto const err = m_db->query(sql.toStdString(), res);
        if (!err.ok())
        {
            addFinding(cat.item, tr("scan failed: %1")
                .arg(QString::fromStdString(err.message)));
        }
        else if (res.rowCount() == 0)
        {
            addFinding(cat.item, tr("OK -- every pool_template has at least one member"));
        }
        else
        {
            cat.findingCount = int(res.rowCount());
            for (size_t r = 0; r < res.rowCount(); ++r)
            {
                addFinding(cat.item, tr("pool_template entry=%1 -- '%2'")
                    .arg(res.asUInt64(r, 0).value_or(0))
                    .arg(QString::fromStdString(res.isNull(r, 1) ? std::string() : res.cell(r, 1))));
            }
            if (res.rowCount() >= 500)
                addFinding(cat.item, tr("(... truncated at 500 rows.)"));
        }
        attachFixButton(cat);
    }

    // ---- 12. Game event templates that are never active -----------
    // High blast radius: don't auto-fix. Leave deleteSql empty so no
    // Fix button is rendered; operator decides what to do per row.
    {
        Category cat;
        cat.title = tr("Game event templates that are never active");
        addCategory(cat);

        QString const sql = QStringLiteral(
            "SELECT ge.eventEntry, COALESCE(ge.description, '') FROM game_event ge "
            "WHERE ge.occurence = 0 "
            "  AND NOT EXISTS (SELECT 1 FROM game_event_creature WHERE eventEntry = ge.eventEntry) "
            "  AND NOT EXISTS (SELECT 1 FROM game_event_gameobject WHERE eventEntry = ge.eventEntry) "
            "LIMIT 500");
        db::QueryResult res;
        auto const err = m_db->query(sql.toStdString(), res);
        if (!err.ok())
        {
            addFinding(cat.item, tr("scan failed: %1")
                .arg(QString::fromStdString(err.message)));
        }
        else if (res.rowCount() == 0)
        {
            addFinding(cat.item, tr("OK -- every game_event is either schedulable or has spawn members"));
        }
        else
        {
            cat.findingCount = int(res.rowCount());
            for (size_t r = 0; r < res.rowCount(); ++r)
            {
                addFinding(cat.item, tr("eventEntry=%1 -- '%2'  (no auto-fix)")
                    .arg(res.asUInt64(r, 0).value_or(0))
                    .arg(QString::fromStdString(res.isNull(r, 1) ? std::string() : res.cell(r, 1))));
            }
            if (res.rowCount() >= 500)
                addFinding(cat.item, tr("(... truncated at 500 rows.)"));
        }
        // Intentionally no attachFixButton(cat): operator must decide.
    }

    // ---- 13. SAI rules with chance=0 ------------------------------
    // event_chance = 0 means the rule will never fire. Almost always a
    // copy-paste bug from a probabilistic event. Warning only - operator
    // must decide whether to delete or set chance=100.
    {
        Category cat;
        cat.title = tr("SAI rules with chance=0");
        addCategory(cat);

        QString const sql = QStringLiteral(
            "SELECT entryorguid, source_type, id, link, event_type "
            "FROM smart_scripts "
            "WHERE event_chance = 0 LIMIT 500");
        db::QueryResult res;
        auto const err = m_db->query(sql.toStdString(), res);
        if (!err.ok())
        {
            addFinding(cat.item, tr("scan failed: %1")
                .arg(QString::fromStdString(err.message)));
        }
        else if (res.rowCount() == 0)
        {
            addFinding(cat.item, tr("OK -- no SAI rules with event_chance=0"));
        }
        else
        {
            cat.findingCount = int(res.rowCount());
            for (size_t r = 0; r < res.rowCount(); ++r)
            {
                addFinding(cat.item, tr("entryorguid=%1 id=%2 link=%3: event_chance=0 (event_type=%4 never fires)")
                    .arg(res.asInt64 (r, 0).value_or(0))
                    .arg(res.asUInt64(r, 2).value_or(0))
                    .arg(res.asUInt64(r, 3).value_or(0))
                    .arg(res.asUInt64(r, 4).value_or(0)));
            }
            if (res.rowCount() >= 500)
                addFinding(cat.item, tr("(... truncated at 500 rows.)"));
        }
        // Warning only: no Fix button.
    }

    // ---- 14. SAI cast actions with spell_id=0 ---------------------
    // action_type=11 (CAST) requires a valid spell in action_param1.
    {
        Category cat;
        cat.title = tr("SAI cast actions with spell_id=0");
        addCategory(cat);

        QString const sql = QStringLiteral(
            "SELECT entryorguid, source_type, id, link "
            "FROM smart_scripts "
            "WHERE action_type = 11 AND action_param1 = 0 LIMIT 500");
        db::QueryResult res;
        auto const err = m_db->query(sql.toStdString(), res);
        if (!err.ok())
        {
            addFinding(cat.item, tr("scan failed: %1")
                .arg(QString::fromStdString(err.message)));
        }
        else if (res.rowCount() == 0)
        {
            addFinding(cat.item, tr("OK -- no CAST actions with spell_id=0"));
        }
        else
        {
            cat.findingCount = int(res.rowCount());
            for (size_t r = 0; r < res.rowCount(); ++r)
            {
                addFinding(cat.item, tr("entryorguid=%1 id=%2 link=%3: action_type=11 CAST with spell_id=0")
                    .arg(res.asInt64 (r, 0).value_or(0))
                    .arg(res.asUInt64(r, 2).value_or(0))
                    .arg(res.asUInt64(r, 3).value_or(0)));
            }
            if (res.rowCount() >= 500)
                addFinding(cat.item, tr("(... truncated at 500 rows.)"));
        }
        // Warning only: no Fix button.
    }

    // ---- 15. SAI talk actions with text_id=0 ----------------------
    // action_type=1 (TALK) needs a creature_text group id in action_param1.
    {
        Category cat;
        cat.title = tr("SAI talk actions with text_id=0");
        addCategory(cat);

        QString const sql = QStringLiteral(
            "SELECT entryorguid, source_type, id, link "
            "FROM smart_scripts "
            "WHERE action_type = 1 AND action_param1 = 0 LIMIT 500");
        db::QueryResult res;
        auto const err = m_db->query(sql.toStdString(), res);
        if (!err.ok())
        {
            addFinding(cat.item, tr("scan failed: %1")
                .arg(QString::fromStdString(err.message)));
        }
        else if (res.rowCount() == 0)
        {
            addFinding(cat.item, tr("OK -- no TALK actions with text_id=0"));
        }
        else
        {
            cat.findingCount = int(res.rowCount());
            for (size_t r = 0; r < res.rowCount(); ++r)
            {
                addFinding(cat.item, tr("entryorguid=%1 id=%2 link=%3: action_type=1 TALK with text_id=0")
                    .arg(res.asInt64 (r, 0).value_or(0))
                    .arg(res.asUInt64(r, 2).value_or(0))
                    .arg(res.asUInt64(r, 3).value_or(0)));
            }
            if (res.rowCount() >= 500)
                addFinding(cat.item, tr("(... truncated at 500 rows.)"));
        }
        // Warning only: no Fix button.
    }

    // ---- 16. SAI event_param ranges that look swapped -------------
    // Timed-repeat event types use param1=min and param2=max; param2<param1
    // is almost certainly swapped.
    {
        Category cat;
        cat.title = tr("SAI event_param ranges that look swapped");
        addCategory(cat);

        QString const sql = QStringLiteral(
            "SELECT entryorguid, source_type, id, link, event_type, event_param1, event_param2 "
            "FROM smart_scripts "
            "WHERE event_type IN (1,2,4,9,10,17,18) "
            "  AND event_param1 > 0 AND event_param2 > 0 AND event_param2 < event_param1 "
            "LIMIT 500");
        db::QueryResult res;
        auto const err = m_db->query(sql.toStdString(), res);
        if (!err.ok())
        {
            addFinding(cat.item, tr("scan failed: %1")
                .arg(QString::fromStdString(err.message)));
        }
        else if (res.rowCount() == 0)
        {
            addFinding(cat.item, tr("OK -- no swapped event_param min/max ranges"));
        }
        else
        {
            cat.findingCount = int(res.rowCount());
            for (size_t r = 0; r < res.rowCount(); ++r)
            {
                addFinding(cat.item, tr("entryorguid=%1 id=%2 link=%3: event_type=%4 param1=%5 > param2=%6 (min/max swapped?)")
                    .arg(res.asInt64 (r, 0).value_or(0))
                    .arg(res.asUInt64(r, 2).value_or(0))
                    .arg(res.asUInt64(r, 3).value_or(0))
                    .arg(res.asUInt64(r, 4).value_or(0))
                    .arg(res.asUInt64(r, 5).value_or(0))
                    .arg(res.asUInt64(r, 6).value_or(0)));
            }
            if (res.rowCount() >= 500)
                addFinding(cat.item, tr("(... truncated at 500 rows.)"));
        }
        // Warning only: no Fix button.
    }

    // ---- 17. SAI rules pointing at unknown action_type ------------
    // TC's SMART_ACTION enum currently tops out around 220; anything
    // higher is almost certainly a wrong action_type value.
    {
        Category cat;
        cat.title = tr("SAI events pointing at unknown action_type");
        addCategory(cat);

        QString const sql = QStringLiteral(
            "SELECT entryorguid, source_type, id, link, action_type "
            "FROM smart_scripts "
            "WHERE action_type > 220 LIMIT 500");
        db::QueryResult res;
        auto const err = m_db->query(sql.toStdString(), res);
        if (!err.ok())
        {
            addFinding(cat.item, tr("scan failed: %1")
                .arg(QString::fromStdString(err.message)));
        }
        else if (res.rowCount() == 0)
        {
            addFinding(cat.item, tr("OK -- no out-of-range action_type values"));
        }
        else
        {
            cat.findingCount = int(res.rowCount());
            for (size_t r = 0; r < res.rowCount(); ++r)
            {
                addFinding(cat.item, tr("entryorguid=%1 id=%2 link=%3: action_type=%4 exceeds known max (~220)")
                    .arg(res.asInt64 (r, 0).value_or(0))
                    .arg(res.asUInt64(r, 2).value_or(0))
                    .arg(res.asUInt64(r, 3).value_or(0))
                    .arg(res.asUInt64(r, 4).value_or(0)));
            }
            if (res.rowCount() >= 500)
                addFinding(cat.item, tr("(... truncated at 500 rows.)"));
        }
        // Warning only: no Fix button.
    }

    // ---- 18. Zones with no innkeeper ------------------------------
    // Service-coverage advisory: zones with creature spawns on the loaded
    // map that lack any UNIT_NPC_FLAG_INNKEEPER (0x10000) NPC. No Fix button.
    if (m_currentMapId.has_value())
    {
        Category cat;
        cat.title = tr("Zones with no innkeeper");
        addCategory(cat);

        QString const sql = QStringLiteral(
            "SELECT DISTINCT c.zoneId "
            "FROM creature c "
            "WHERE c.map = %1 "
            "  AND c.zoneId > 0 "
            "  AND NOT EXISTS ("
            "    SELECT 1 FROM creature c2 "
            "    JOIN creature_template ct ON ct.entry = c2.id "
            "    WHERE c2.zoneId = c.zoneId AND c2.map = c.map "
            "      AND (ct.npcflag & 0x10000) <> 0) "
            "ORDER BY c.zoneId LIMIT 500").arg(*m_currentMapId);
        db::QueryResult res;
        auto const err = m_db->query(sql.toStdString(), res);
        if (!err.ok())
        {
            addFinding(cat.item, tr("scan failed: %1")
                .arg(QString::fromStdString(err.message)));
        }
        else if (res.rowCount() == 0)
        {
            addFinding(cat.item, tr("OK -- every populated zone has an innkeeper"));
        }
        else
        {
            cat.findingCount = int(res.rowCount());
            for (size_t r = 0; r < res.rowCount(); ++r)
            {
                addFinding(cat.item, tr("zoneId=%1 has no innkeeper")
                    .arg(res.asUInt64(r, 0).value_or(0)));
            }
            if (res.rowCount() >= 500)
                addFinding(cat.item, tr("(... truncated at 500 rows.)"));
        }
        // Advisory only: no Fix button.
    }

    // ---- 19. Zones with no mailbox --------------------------------
    // Service-coverage advisory: zones with creature spawns on the loaded
    // map that have no gameobject_template.type=19 (MAILBOX) GO present.
    if (m_currentMapId.has_value())
    {
        Category cat;
        cat.title = tr("Zones with no mailbox");
        addCategory(cat);

        QString const sql = QStringLiteral(
            "SELECT DISTINCT c.zoneId "
            "FROM creature c "
            "WHERE c.map = %1 "
            "  AND c.zoneId > 0 "
            "  AND NOT EXISTS ("
            "    SELECT 1 FROM gameobject g "
            "    JOIN gameobject_template gt ON gt.entry = g.id "
            "    WHERE g.zoneId = c.zoneId AND g.map = c.map AND gt.type = 19) "
            "ORDER BY c.zoneId LIMIT 500").arg(*m_currentMapId);
        db::QueryResult res;
        auto const err = m_db->query(sql.toStdString(), res);
        if (!err.ok())
        {
            addFinding(cat.item, tr("scan failed: %1")
                .arg(QString::fromStdString(err.message)));
        }
        else if (res.rowCount() == 0)
        {
            addFinding(cat.item, tr("OK -- every populated zone has a mailbox"));
        }
        else
        {
            cat.findingCount = int(res.rowCount());
            for (size_t r = 0; r < res.rowCount(); ++r)
            {
                addFinding(cat.item, tr("zoneId=%1 has no mailbox")
                    .arg(res.asUInt64(r, 0).value_or(0)));
            }
            if (res.rowCount() >= 500)
                addFinding(cat.item, tr("(... truncated at 500 rows.)"));
        }
        // Advisory only: no Fix button.
    }

    // ---- 20. Zones with no graveyard link -------------------------
    // Zones with creature spawns but no graveyard_zone row binding them
    // to any world_safe_locs. Death -> respawn will fall back to a default.
    if (m_currentMapId.has_value())
    {
        Category cat;
        cat.title = tr("Zones with no graveyard link");
        addCategory(cat);

        QString const sql = QStringLiteral(
            "SELECT DISTINCT c.zoneId "
            "FROM creature c "
            "WHERE c.map = %1 AND c.zoneId > 0 "
            "  AND NOT EXISTS ("
            "    SELECT 1 FROM graveyard_zone gz WHERE gz.GhostZone = c.zoneId) "
            "ORDER BY c.zoneId LIMIT 500").arg(*m_currentMapId);
        db::QueryResult res;
        auto const err = m_db->query(sql.toStdString(), res);
        if (!err.ok())
        {
            addFinding(cat.item, tr("scan failed: %1")
                .arg(QString::fromStdString(err.message)));
        }
        else if (res.rowCount() == 0)
        {
            addFinding(cat.item, tr("OK -- every populated zone has a graveyard link"));
        }
        else
        {
            cat.findingCount = int(res.rowCount());
            for (size_t r = 0; r < res.rowCount(); ++r)
            {
                addFinding(cat.item, tr("zoneId=%1 has no graveyard_zone row")
                    .arg(res.asUInt64(r, 0).value_or(0)));
            }
            if (res.rowCount() >= 500)
                addFinding(cat.item, tr("(... truncated at 500 rows.)"));
        }
        // Advisory only: no Fix button.
    }

    // ---- 21. Zones with > 5000 creature spawns --------------------
    // Density anomaly: a single zone hosting > 5000 creature rows usually
    // points at a duplicate-spawn import or a runaway pool.
    if (m_currentMapId.has_value())
    {
        Category cat;
        cat.title = tr("Zones with > 5000 creature spawns");
        addCategory(cat);

        QString const sql = QStringLiteral(
            "SELECT zoneId, COUNT(*) AS n "
            "FROM creature "
            "WHERE map = %1 AND zoneId > 0 "
            "GROUP BY zoneId HAVING n > 5000 "
            "ORDER BY n DESC LIMIT 500").arg(*m_currentMapId);
        db::QueryResult res;
        auto const err = m_db->query(sql.toStdString(), res);
        if (!err.ok())
        {
            addFinding(cat.item, tr("scan failed: %1")
                .arg(QString::fromStdString(err.message)));
        }
        else if (res.rowCount() == 0)
        {
            addFinding(cat.item, tr("OK -- no zone exceeds 5000 creature spawns"));
        }
        else
        {
            cat.findingCount = int(res.rowCount());
            for (size_t r = 0; r < res.rowCount(); ++r)
            {
                addFinding(cat.item, tr("zoneId=%1 has %2 creature spawns")
                    .arg(res.asUInt64(r, 0).value_or(0))
                    .arg(res.asUInt64(r, 1).value_or(0)));
            }
            if (res.rowCount() >= 500)
                addFinding(cat.item, tr("(... truncated at 500 rows.)"));
        }
        // Advisory only: no Fix button.
    }

    // ---- 22. Areatriggers within 1 yard of each other -------------
    // Pairs of areatrigger spawns on the same map whose Euclidean distance is
    // under one yard - almost always a duplicate spawn import or a designer
    // accidentally re-stamping the same spawn twice. Warning only.
    {
        Category cat;
        cat.title = tr("Areatriggers within 1 yard of each other");
        addCategory(cat);

        QString const sql = QStringLiteral(
            "SELECT a.SpawnId AS a_spawn, b.SpawnId AS b_spawn, a.MapId, "
            "       SQRT(POW(a.PosX - b.PosX, 2) + POW(a.PosY - b.PosY, 2) + POW(a.PosZ - b.PosZ, 2)) AS dist, "
            "       a.PosX, a.PosY "
            "FROM areatrigger a "
            "JOIN areatrigger b ON a.MapId = b.MapId AND a.SpawnId < b.SpawnId "
            "WHERE SQRT(POW(a.PosX - b.PosX, 2) + POW(a.PosY - b.PosY, 2) + POW(a.PosZ - b.PosZ, 2)) < 1.0"
            "%1 "
            "LIMIT 500").arg(mapFilterAtrPair);
        db::QueryResult res;
        auto const err = m_db->query(sql.toStdString(), res);
        if (!err.ok())
        {
            addFinding(cat.item, tr("scan failed: %1")
                .arg(QString::fromStdString(err.message)));
        }
        else if (res.rowCount() == 0)
        {
            addFinding(cat.item, tr("OK -- no areatrigger pairs within 1 yard"));
        }
        else
        {
            cat.findingCount = int(res.rowCount());
            for (size_t r = 0; r < res.rowCount(); ++r)
            {
                uint64_t const aSpawn = res.asUInt64(r, 0).value_or(0);
                uint64_t const bSpawn = res.asUInt64(r, 1).value_or(0);
                uint32_t const mid    = uint32_t(res.asUInt64(r, 2).value_or(0));
                double   const dist   = res.asDouble(r, 3).value_or(0.0);
                float    const x      = float(res.asDouble(r, 4).value_or(0.0));
                float    const y      = float(res.asDouble(r, 5).value_or(0.0));
                addFinding(cat.item,
                    tr("SpawnId %1 and %2 at distance %3 yards")
                        .arg(aSpawn).arg(bSpawn).arg(dist, 0, 'f', 3),
                    mid, x, y);
            }
            if (res.rowCount() >= 500)
                addFinding(cat.item, tr("(... truncated at 500 rows.)"));
        }
        // Warning only: no Fix button.
    }

    // ---- 23. Duplicate spawn coordinates (creatures) --------------
    // Pairs of creature rows sharing the same entry, same map, whose XY
    // separation is under 0.5 yards. Strong signal of duplicated spawns.
    {
        Category cat;
        cat.title = tr("Duplicate spawn coordinates (creatures)");
        addCategory(cat);

        QString const sql = QStringLiteral(
            "SELECT c1.guid AS a, c2.guid AS b, c1.id AS entry, c1.map, "
            "       SQRT(POW(c1.position_x - c2.position_x, 2) + POW(c1.position_y - c2.position_y, 2)) AS dxy, "
            "       c1.position_x, c1.position_y "
            "FROM creature c1 "
            "JOIN creature c2 ON c1.map = c2.map AND c1.guid < c2.guid AND c1.id = c2.id "
            "WHERE SQRT(POW(c1.position_x - c2.position_x, 2) + POW(c1.position_y - c2.position_y, 2)) < 0.5"
            "%1 "
            "LIMIT 500").arg(mapFilterCreaturePair);
        db::QueryResult res;
        auto const err = m_db->query(sql.toStdString(), res);
        if (!err.ok())
        {
            addFinding(cat.item, tr("scan failed: %1")
                .arg(QString::fromStdString(err.message)));
        }
        else if (res.rowCount() == 0)
        {
            addFinding(cat.item, tr("OK -- no duplicate creature spawn coordinates"));
        }
        else
        {
            cat.findingCount = int(res.rowCount());
            for (size_t r = 0; r < res.rowCount(); ++r)
            {
                int64_t  const guidA = res.asInt64 (r, 0).value_or(0);
                int64_t  const guidB = res.asInt64 (r, 1).value_or(0);
                uint32_t const entry = uint32_t(res.asUInt64(r, 2).value_or(0));
                uint32_t const mid   = uint32_t(res.asUInt64(r, 3).value_or(0));
                double   const dxy   = res.asDouble(r, 4).value_or(0.0);
                float    const x     = float(res.asDouble(r, 5).value_or(0.0));
                float    const y     = float(res.asDouble(r, 6).value_or(0.0));
                addFinding(cat.item,
                    tr("creature entry %1 at guid %2 + %3 at distance %4 yards")
                        .arg(entry).arg(guidA).arg(guidB).arg(dxy, 0, 'f', 3),
                    mid, x, y);
            }
            if (res.rowCount() >= 500)
                addFinding(cat.item, tr("(... truncated at 500 rows.)"));
        }
        // Warning only: no Fix button.
    }

    // ---- Quest-chain scans: probe quest_template columns once ---------
    // Fork schemas occasionally rename LogTitle/Title or split chain fields.
    // Resolve column names up front so all three scans agree.
    auto const questCols = discoverColumns(*m_db, "quest_template");
    std::string const titleCol  = pickColumn(questCols, { "LogTitle", "Title" });
    std::string const nextCol   = pickColumn(questCols, { "NextQuestInChain", "NextQuestId", "NextQuestID" });
    std::string const prevCol   = pickColumn(questCols, { "PrevQuestID", "PrevQuestId", "PrevQuest" });
    std::string const methodCol = pickColumn(questCols, { "Method", "QuestType" });

    // ---- 24. Quests with NextQuestInChain pointing to missing quest ---
    if (!questCols.empty() && !nextCol.empty())
    {
        Category cat;
        cat.title = tr("Quests with NextQuestInChain pointing to missing quest");
        addCategory(cat);

        QString const sql = QStringLiteral(
            "SELECT q.ID, %1, q.%2 AS nextQ "
            "FROM quest_template q "
            "WHERE q.%2 > 0 "
            "  AND NOT EXISTS (SELECT 1 FROM quest_template q2 WHERE q2.ID = q.%2) "
            "LIMIT 500")
            .arg(QString::fromStdString(projectAs(questCols, titleCol.empty() ? "LogTitle" : titleCol.c_str(), "title")))
            .arg(QString::fromStdString(nextCol));
        db::QueryResult res;
        auto const err = m_db->query(sql.toStdString(), res);
        if (!err.ok())
        {
            addFinding(cat.item, tr("scan failed: %1")
                .arg(QString::fromStdString(err.message)));
        }
        else if (res.rowCount() == 0)
        {
            addFinding(cat.item, tr("OK -- every NextQuestInChain target exists"));
        }
        else
        {
            cat.findingCount = int(res.rowCount());
            for (size_t r = 0; r < res.rowCount(); ++r)
            {
                uint64_t const qid   = res.asUInt64(r, 0).value_or(0);
                QString  const title = QString::fromStdString(res.isNull(r, 1) ? std::string() : res.cell(r, 1));
                uint64_t const nxt   = res.asUInt64(r, 2).value_or(0);
                addFinding(cat.item, tr("Quest %1 ('%2') points NextQuestInChain to missing quest %3")
                    .arg(qid).arg(title).arg(nxt));
            }
            if (res.rowCount() >= 500)
                addFinding(cat.item, tr("(... truncated at 500 rows.)"));
        }
        // Warning only: no Fix button.
    }

    // ---- 25. Quests with PrevQuestID pointing to missing quest --------
    // PrevQuestID is negative for "exclusive" chain anchors on some forks,
    // so probe via ABS() to catch both signs.
    if (!questCols.empty() && !prevCol.empty())
    {
        Category cat;
        cat.title = tr("Quests with PrevQuestID pointing to missing quest");
        addCategory(cat);

        QString const sql = QStringLiteral(
            "SELECT q.ID, %1, q.%2 AS prevQ "
            "FROM quest_template q "
            "WHERE q.%2 <> 0 "
            "  AND NOT EXISTS (SELECT 1 FROM quest_template q2 WHERE q2.ID = ABS(q.%2)) "
            "LIMIT 500")
            .arg(QString::fromStdString(projectAs(questCols, titleCol.empty() ? "LogTitle" : titleCol.c_str(), "title")))
            .arg(QString::fromStdString(prevCol));
        db::QueryResult res;
        auto const err = m_db->query(sql.toStdString(), res);
        if (!err.ok())
        {
            addFinding(cat.item, tr("scan failed: %1")
                .arg(QString::fromStdString(err.message)));
        }
        else if (res.rowCount() == 0)
        {
            addFinding(cat.item, tr("OK -- every PrevQuestID target exists"));
        }
        else
        {
            cat.findingCount = int(res.rowCount());
            for (size_t r = 0; r < res.rowCount(); ++r)
            {
                uint64_t const qid   = res.asUInt64(r, 0).value_or(0);
                QString  const title = QString::fromStdString(res.isNull(r, 1) ? std::string() : res.cell(r, 1));
                int64_t  const prev  = res.asInt64 (r, 2).value_or(0);
                addFinding(cat.item, tr("Quest %1 ('%2') has PrevQuestID = %3 but that quest doesn't exist")
                    .arg(qid).arg(title).arg(prev));
            }
            if (res.rowCount() >= 500)
                addFinding(cat.item, tr("(... truncated at 500 rows.)"));
        }
        // Warning only: no Fix button.
    }

    // ---- 26. Quest starters with no quest enders ----------------------
    // Quest has a creature_queststarter row but neither creature_questender
    // nor gameobject_questender. Excludes auto-complete quests (Method=1).
    if (!questCols.empty())
    {
        Category cat;
        cat.title = tr("Quest starters with no quest enders");
        addCategory(cat);

        // Method column may be absent on some forks; if so we skip the
        // auto-complete filter rather than producing a syntax error.
        QString const methodFilter = methodCol.empty()
            ? QString()
            : QStringLiteral(" AND q.%1 <> 1").arg(QString::fromStdString(methodCol));

        QString const sql = QStringLiteral(
            "SELECT q.ID, %1 "
            "FROM quest_template q "
            "WHERE EXISTS (SELECT 1 FROM creature_queststarter cs WHERE cs.quest = q.ID) "
            "  AND NOT EXISTS (SELECT 1 FROM creature_questender ce WHERE ce.quest = q.ID) "
            "  AND NOT EXISTS (SELECT 1 FROM gameobject_questender ge WHERE ge.quest = q.ID)"
            "%2 "
            "LIMIT 500")
            .arg(QString::fromStdString(projectAs(questCols, titleCol.empty() ? "LogTitle" : titleCol.c_str(), "title")))
            .arg(methodFilter);
        db::QueryResult res;
        auto const err = m_db->query(sql.toStdString(), res);
        if (!err.ok())
        {
            addFinding(cat.item, tr("scan failed: %1")
                .arg(QString::fromStdString(err.message)));
        }
        else if (res.rowCount() == 0)
        {
            addFinding(cat.item, tr("OK -- every quest with a starter also has an ender"));
        }
        else
        {
            cat.findingCount = int(res.rowCount());
            for (size_t r = 0; r < res.rowCount(); ++r)
            {
                uint64_t const qid   = res.asUInt64(r, 0).value_or(0);
                QString  const title = QString::fromStdString(res.isNull(r, 1) ? std::string() : res.cell(r, 1));
                addFinding(cat.item, tr("Quest %1 ('%2') has a starter but no ender")
                    .arg(qid).arg(title));
            }
            if (res.rowCount() >= 500)
                addFinding(cat.item, tr("(... truncated at 500 rows.)"));
        }
        // Warning only: no Fix button.
    }
}

} // namespace world_editor::app
