#include "AreatriggerScriptDock.h"

#include "../db/MySqlClient.h"

#include <QHeaderView>
#include <QLabel>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

#include <set>
#include <string>

namespace world_editor::app
{

namespace
{

constexpr uint32_t kErrNoSuchTable  = 1146;
constexpr uint32_t kErrNoSuchColumn = 1054;
constexpr int      kSummaryLimit    = 500;

// Probe INFORMATION_SCHEMA for the column set of `table`.  Empty result
// means "table absent in this schema" (or perms missing).
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

// Per-cell item factory that suppresses edits and (optionally) tags the
// row with the SpawnId so onCellDoubleClicked can pick it back up.
QTableWidgetItem* makeItem(QString const& text)
{
    auto* item = new QTableWidgetItem(text);
    item->setFlags(item->flags() & ~Qt::ItemIsEditable);
    return item;
}

} // namespace

AreatriggerScriptDock::AreatriggerScriptDock(db::MySqlClient* dbClient, QWidget* parent)
    : QWidget(parent)
    , m_db(dbClient)
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(6, 6, 6, 6);
    root->setSpacing(4);

    m_header = new QLabel(this);
    m_header->setWordWrap(true);
    m_header->setStyleSheet(QStringLiteral("QLabel { font-weight: bold; font-size: 11pt; }"));
    root->addWidget(m_header);

    m_table = new QTableWidget(this);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setSortingEnabled(true);
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->verticalHeader()->setVisible(false);
    root->addWidget(m_table, 1);

    connect(m_table, &QTableWidget::cellDoubleClicked,
            this, &AreatriggerScriptDock::onCellDoubleClicked);

    clear();
}

void AreatriggerScriptDock::clear()
{
    m_summaryMode = true;
    m_header->setText(tr("Use Tools -> Areatrigger scripts... or click an areatrigger spawn to scope."));
    m_table->clear();
    m_table->setRowCount(0);
    m_table->setColumnCount(0);
}

void AreatriggerScriptDock::setScriptName(QString const& name)
{
    if (name.isEmpty())
    {
        renderSummary();
        return;
    }
    renderScoped(name);
}

void AreatriggerScriptDock::renderScoped(QString const& scriptName)
{
    m_summaryMode = false;
    m_table->setSortingEnabled(false);
    m_table->clear();
    m_table->setRowCount(0);
    m_table->setColumnCount(5);
    m_table->setHorizontalHeaderLabels(
        { tr("ScriptName"), tr("SpawnId"), tr("MapId"), tr("x"), tr("y") });

    m_header->setText(tr("Areatrigger script: %1").arg(scriptName));

    if (!m_db || !m_db->isConnected())
    {
        m_header->setText(tr("Areatrigger script: %1 (DB not connected)").arg(scriptName));
        return;
    }

    // Make sure both tables exist before issuing the UNION ALL; a missing
    // table on either side would make MySQL error the entire statement.
    auto const atrCols = discoverColumns(*m_db, "areatrigger");
    auto const tplCols = discoverColumns(*m_db, "areatrigger_template");
    bool const haveSpawn = !atrCols.empty() && atrCols.count("ScriptName") > 0;
    bool const haveTpl   = !tplCols.empty() && tplCols.count("ScriptName") > 0;

    if (!haveSpawn && !haveTpl)
    {
        m_header->setText(tr("Areatrigger script: %1 (no ScriptName column in this schema)").arg(scriptName));
        return;
    }

    std::string const escName = m_db->escapeString(scriptName.toStdString());

    // UNION ALL of spawn rows + template rows; the template side projects
    // NULL columns so result-set ordinals are stable.  LIMIT is generous;
    // operators looking at a noisy script can still narrow externally.
    std::string sql;
    if (haveSpawn)
    {
        sql += "SELECT ScriptName, SpawnId, MapId, PosX, PosY FROM areatrigger "
               "WHERE ScriptName = '" + escName + "'";
    }
    if (haveSpawn && haveTpl)
        sql += " UNION ALL ";
    if (haveTpl)
    {
        sql += "SELECT ScriptName, NULL AS SpawnId, NULL AS MapId, NULL AS PosX, NULL AS PosY "
               "FROM areatrigger_template WHERE ScriptName = '" + escName + "'";
    }

    db::QueryResult res;
    auto const err = m_db->query(sql, res);
    if (!err.ok())
    {
        if (err.code == kErrNoSuchTable || err.code == kErrNoSuchColumn)
        {
            m_header->setText(tr("Areatrigger script: %1 (schema unexpected: %2)")
                .arg(scriptName).arg(QString::fromStdString(err.message)));
            return;
        }
        m_header->setText(tr("Areatrigger script: %1 (query failed: %2)")
            .arg(scriptName).arg(QString::fromStdString(err.message)));
        return;
    }

    m_table->setRowCount(int(res.rowCount()));
    int spawnRows = 0;
    int tplRows   = 0;
    for (size_t r = 0; r < res.rowCount(); ++r)
    {
        QString const sn = QString::fromStdString(res.cell(r, 0));

        bool const isTemplate = res.isNull(r, 1);
        if (isTemplate) ++tplRows; else ++spawnRows;

        // Column 0: ScriptName (always populated).
        m_table->setItem(int(r), 0, makeItem(sn));

        // Column 1: SpawnId.  Template rows show "(template)" so the
        // operator can tell the two apart at a glance.
        if (isTemplate)
        {
            m_table->setItem(int(r), 1, makeItem(tr("(template)")));
        }
        else
        {
            auto const spawnId = res.asInt64(r, 1);
            m_table->setItem(int(r), 1, makeItem(
                spawnId ? QString::number(*spawnId) : QStringLiteral("?")));
        }

        // Column 2: MapId.
        if (res.isNull(r, 2))
        {
            m_table->setItem(int(r), 2, makeItem(QStringLiteral("-")));
        }
        else
        {
            auto const mapId = res.asInt64(r, 2);
            m_table->setItem(int(r), 2, makeItem(
                mapId ? QString::number(*mapId) : QStringLiteral("?")));
        }

        // Columns 3+4: world x / y.
        auto cellNum = [&](size_t col) -> QString {
            if (res.isNull(r, col)) return QStringLiteral("-");
            auto const v = res.asDouble(r, col);
            return v ? QString::number(*v, 'f', 1) : QStringLiteral("?");
        };
        m_table->setItem(int(r), 3, makeItem(cellNum(3)));
        m_table->setItem(int(r), 4, makeItem(cellNum(4)));
    }

    m_table->setSortingEnabled(true);
    m_header->setText(tr("Areatrigger script: %1 (%2 spawn%3, %4 template ref%5)")
        .arg(scriptName)
        .arg(spawnRows).arg(spawnRows == 1 ? QString() : QStringLiteral("s"))
        .arg(tplRows).arg(tplRows == 1 ? QString() : QStringLiteral("s")));
}

void AreatriggerScriptDock::renderSummary()
{
    m_summaryMode = true;
    m_table->setSortingEnabled(false);
    m_table->clear();
    m_table->setRowCount(0);
    m_table->setColumnCount(2);
    m_table->setHorizontalHeaderLabels({ tr("ScriptName"), tr("RowCount") });

    m_header->setText(tr("All scripts (loading...)"));

    if (!m_db || !m_db->isConnected())
    {
        m_header->setText(tr("All scripts (DB not connected)"));
        return;
    }

    auto const cols = discoverColumns(*m_db, "areatrigger");
    if (cols.empty() || cols.count("ScriptName") == 0)
    {
        m_header->setText(tr("All scripts (areatrigger.ScriptName not in this schema)"));
        return;
    }

    // LIMIT keeps the table cheap on schemas with thousands of distinct
    // script names; a 500-row ceiling covers every fork we've audited.
    std::string const sql =
        "SELECT ScriptName, COUNT(*) FROM areatrigger "
        "WHERE ScriptName <> '' "
        "GROUP BY ScriptName ORDER BY ScriptName LIMIT " + std::to_string(kSummaryLimit);

    db::QueryResult res;
    auto const err = m_db->query(sql, res);
    if (!err.ok())
    {
        m_header->setText(tr("All scripts (query failed: %1)")
            .arg(QString::fromStdString(err.message)));
        return;
    }

    m_table->setRowCount(int(res.rowCount()));
    for (size_t r = 0; r < res.rowCount(); ++r)
    {
        QString const sn   = QString::fromStdString(res.cell(r, 0));
        auto    const cnt  = res.asUInt64(r, 1);
        m_table->setItem(int(r), 0, makeItem(sn));
        m_table->setItem(int(r), 1, makeItem(cnt ? QString::number(*cnt) : QStringLiteral("?")));
    }

    m_table->setSortingEnabled(true);
    m_header->setText(tr("All scripts (%1 total%2)")
        .arg(res.rowCount())
        .arg(res.rowCount() >= size_t(kSummaryLimit)
                ? QStringLiteral(", LIMIT reached")
                : QString()));
}

void AreatriggerScriptDock::onCellDoubleClicked(int row, int /*col*/)
{
    if (row < 0 || row >= m_table->rowCount())
        return;

    if (m_summaryMode)
    {
        // Summary mode: narrow scope to the picked ScriptName.
        QTableWidgetItem* item = m_table->item(row, 0);
        if (!item) return;
        setScriptName(item->text());
        return;
    }

    // Scope mode: emit jump iff the row carries a real SpawnId (template
    // rows in the union have "(template)" in the SpawnId column).
    QTableWidgetItem* idItem = m_table->item(row, 1);
    if (!idItem) return;
    bool okParse = false;
    qlonglong const spawnId = idItem->text().toLongLong(&okParse);
    if (!okParse) return;

    QTableWidgetItem* mapItem = m_table->item(row, 2);
    QTableWidgetItem* xItem   = m_table->item(row, 3);
    QTableWidgetItem* yItem   = m_table->item(row, 4);
    if (!mapItem || !xItem || !yItem) return;

    bool okMap = false, okX = false, okY = false;
    uint32_t const mapId = uint32_t(mapItem->text().toUInt(&okMap));
    float const    x     = xItem->text().toFloat(&okX);
    float const    y     = yItem->text().toFloat(&okY);
    if (!okMap || !okX || !okY) return;

    emit jumpRequested(mapId, x, y, std::optional<int64_t>{ int64_t(spawnId) });
}

} // namespace world_editor::app
