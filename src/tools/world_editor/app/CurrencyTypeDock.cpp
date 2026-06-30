#include "CurrencyTypeDock.h"

#include "../db/MySqlClient.h"

#include <QLabel>
#include <QVBoxLayout>

#include <set>
#include <string>

namespace world_editor::app
{

namespace
{

// MySQL error codes we treat as "fall through to the next probe".
constexpr uint32_t kErrNoSuchTable  = 1146;
constexpr uint32_t kErrNoSuchColumn = 1054;

// Quality color palette - mirrors WoW's ITEM_QUALITY_* enum (shared with
// ItemInfoDock; kept local so this dock is self-contained).
char const* qualityHexColor(uint32_t q)
{
    switch (q)
    {
        case 0: return "#9d9d9d"; // Poor (grey)
        case 1: return "#ffffff"; // Common (white)
        case 2: return "#1eff00"; // Uncommon (green)
        case 3: return "#0070dd"; // Rare (blue)
        case 4: return "#a335ee"; // Epic (purple)
        case 5: return "#ff8000"; // Legendary (orange)
        case 6: return "#e6cc80"; // Artifact (light gold)
        case 7: return "#00ccff"; // Heirloom (light blue)
        default: return "#9d9d9d";
    }
}

char const* qualityName(uint32_t q)
{
    switch (q)
    {
        case 0: return "Poor";
        case 1: return "Common";
        case 2: return "Uncommon";
        case 3: return "Rare";
        case 4: return "Epic";
        case 5: return "Legendary";
        case 6: return "Artifact";
        case 7: return "Heirloom";
        default: return "Unknown";
    }
}

// Probe INFORMATION_SCHEMA for the column set of `table`.  Empty result
// means "table absent here".
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

// First candidate name that exists in `cols`, or empty string.
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

// Project `<picked> AS alias`; "NULL AS alias" when no candidate exists
// so the result-set ordinal stays stable across schemas.
std::string projectAs(std::set<std::string, std::less<>> const& cols,
                      std::initializer_list<char const*> candidates,
                      char const* alias)
{
    std::string picked = pickColumn(cols, candidates);
    if (picked.empty())
        return std::string("NULL AS ") + alias;
    return picked + " AS " + alias;
}

// Fetch a single cell by alias; NULL or absent column renders as empty.
QString cellByAlias(db::QueryResult const& res, char const* alias)
{
    auto idx = res.columnIndex(alias);
    if (!idx) return {};
    if (res.isNull(0, *idx)) return {};
    return QString::fromStdString(res.cell(0, *idx));
}

} // namespace

CurrencyTypeDock::CurrencyTypeDock(db::MySqlClient* dbClient, QWidget* parent)
    : QWidget(parent)
    , m_db(dbClient)
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(6, 6, 6, 6);
    root->setSpacing(4);

    m_header = new QLabel(this);
    m_header->setWordWrap(true);
    root->addWidget(m_header);

    // Quality-tinted bold name (mirrors ItemInfoDock pattern).
    m_nameLabel = new QLabel(this);
    m_nameLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_nameLabel->setStyleSheet(QStringLiteral("QLabel { font-weight: bold; font-size: 12pt; }"));
    root->addWidget(m_nameLabel);

    QString const mono = QStringLiteral("QLabel { font-family: monospace; }");

    m_identity = new QLabel(this);
    m_identity->setWordWrap(true);
    m_identity->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_identity->setStyleSheet(mono);
    root->addWidget(m_identity);

    m_caps = new QLabel(this);
    m_caps->setWordWrap(true);
    m_caps->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_caps->setStyleSheet(mono);
    root->addWidget(m_caps);

    m_taxonomy = new QLabel(this);
    m_taxonomy->setWordWrap(true);
    m_taxonomy->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_taxonomy->setStyleSheet(mono);
    root->addWidget(m_taxonomy);

    m_description = new QLabel(this);
    m_description->setWordWrap(true);
    m_description->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_description->setStyleSheet(mono);
    root->addWidget(m_description);

    m_flags = new QLabel(this);
    m_flags->setWordWrap(true);
    m_flags->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_flags->setStyleSheet(mono);
    root->addWidget(m_flags);

    root->addStretch(1);

    clear();
}

void CurrencyTypeDock::clear()
{
    m_header->setText(tr("Click a vendor row with type=2 (currency) to see its CurrencyType."));
    m_nameLabel->clear();
    m_nameLabel->setStyleSheet(QStringLiteral("QLabel { font-weight: bold; font-size: 12pt; }"));
    m_identity->clear();
    m_caps->clear();
    m_taxonomy->clear();
    m_description->clear();
    m_flags->clear();
}

void CurrencyTypeDock::setCurrency(uint32_t currencyId)
{
    clear();

    if (currencyId == 0)
        return;

    if (!m_db || !m_db->isConnected())
    {
        m_header->setText(tr("DB not connected.  Currency id = %1.").arg(currencyId));
        return;
    }

    // Probe the candidate tables in priority order.  The first that
    // yields a row wins; missing tables accumulate as notes so the
    // operator sees which mirrors are absent in this schema.
    char const* const kTables[] = {
        "currency_types_dbc", // common TC mirror name
        "currency_type",      // fork variant
        "currencytypes"       // fork variant (flat name)
    };

    QString notes;
    for (char const* table : kTables)
    {
        if (tryPopulateFromTable(currencyId, table, notes))
            return;
    }

    m_header->setText(tr("no currency info table found (CurrencyType.db2 hotfix not mirrored); "
                         "currency id = %1").arg(currencyId));
    if (!notes.isEmpty())
        m_identity->setText(notes);
}

bool CurrencyTypeDock::tryPopulateFromTable(uint32_t currencyId,
                                            char const* table,
                                            QString& outNote)
{
    auto const cols = discoverColumns(*m_db, table);
    if (cols.empty())
    {
        outNote.append(tr("(%1: table missing in this schema)\n").arg(table));
        return false;
    }

    // Pick the id column (ID / Id / id).
    std::string const idCol = pickColumn(cols, { "ID", "Id", "id" });
    if (idCol.empty())
    {
        outNote.append(tr("(%1: no id column found)\n").arg(table));
        return false;
    }

    // Project every attribute we render with a stable alias.  Absent
    // columns become "NULL AS alias" so positional access stays sane.
    std::string sql = "SELECT ";
    sql += idCol + " AS currency_id, ";
    sql += projectAs(cols, { "Name_lang", "Name", "name" },                       "name")          + ", ";
    sql += projectAs(cols, { "Description_lang", "Description", "description" }, "description")   + ", ";
    sql += projectAs(cols, { "MaxQty", "maxQty", "max_qty" },                     "max_qty")       + ", ";
    sql += projectAs(cols, { "MaxEarnablePerWeek", "maxEarnablePerWeek",
                             "max_earnable_per_week", "WeeklyCap" },              "max_week")     + ", ";
    sql += projectAs(cols, { "Quality", "quality" },                              "quality")       + ", ";
    sql += projectAs(cols, { "InventoryIcon", "inventoryIcon", "IconFileDataID",
                             "icon_file_data_id" },                               "icon_fdid")    + ", ";
    sql += projectAs(cols, { "Flags", "flags" },                                  "flags")         + ", ";
    sql += projectAs(cols, { "CategoryID", "categoryID", "category_id",
                             "Category" },                                        "category_id")  + ", ";
    sql += projectAs(cols, { "FactionID", "factionID", "faction_id", "Faction" }, "faction_id")    + " ";
    sql += "FROM " + std::string(table) + " WHERE " + idCol + " = "
         + std::to_string(currencyId) + " LIMIT 1";

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

    // Identity + quality-tinted name header.
    QString  const name        = cellByAlias(res, "name");
    QString  const qualityStr  = cellByAlias(res, "quality");
    uint32_t const quality     = qualityStr.isEmpty() ? 1u : qualityStr.toUInt();

    m_header->setText(tr("Currency %1 — source: %2").arg(currencyId).arg(table));

    m_nameLabel->setText(name.isEmpty() ? tr("(unnamed)") : name);
    m_nameLabel->setStyleSheet(QStringLiteral(
        "QLabel { font-weight: bold; font-size: 12pt; color: %1; }")
        .arg(QString::fromLatin1(qualityHexColor(quality))));

    m_identity->setText(
        tr("ID:           %1\n"
           "Quality:      %2 (%3)")
            .arg(currencyId)
            .arg(quality).arg(QString::fromLatin1(qualityName(quality))));

    // Caps - 0 means unlimited for both fields (matches client UI).
    auto formatCap = [](QString const& raw) -> QString {
        if (raw.isEmpty()) return QStringLiteral("(n/a)");
        bool okParse = false;
        uint64_t const v = raw.toULongLong(&okParse);
        if (!okParse) return raw;
        if (v == 0) return QStringLiteral("unlimited");
        return QString::number(v);
    };
    m_caps->setText(
        tr("MaxQty:       %1\n"
           "MaxPerWeek:   %2")
            .arg(formatCap(cellByAlias(res, "max_qty")))
            .arg(formatCap(cellByAlias(res, "max_week"))));

    // Category + faction.  Render "-" when absent so the operator can
    // distinguish "schema doesn't carry it" from "value is zero".
    auto orDash = [](QString const& raw) -> QString {
        return raw.isEmpty() ? QStringLiteral("-") : raw;
    };
    m_taxonomy->setText(
        tr("CategoryID:   %1\n"
           "FactionID:    %2\n"
           "Icon (FDID):  %3")
            .arg(orDash(cellByAlias(res, "category_id")))
            .arg(orDash(cellByAlias(res, "faction_id")))
            .arg(orDash(cellByAlias(res, "icon_fdid"))));

    // Description (multi-line; renders as a block).
    QString const desc = cellByAlias(res, "description");
    if (!desc.isEmpty())
        m_description->setText(tr("Description:\n%1").arg(desc));

    // Flags - hex so the operator can spot known bits at a glance.
    QString const flagsRaw = cellByAlias(res, "flags");
    if (!flagsRaw.isEmpty())
    {
        bool okParse = false;
        uint64_t const f = flagsRaw.toULongLong(&okParse);
        if (okParse)
            m_flags->setText(tr("Flags:        0x%1").arg(QString::number(f, 16)));
        else
            m_flags->setText(tr("Flags:        %1").arg(flagsRaw));
    }

    // Surface schema-miss notes accumulated from earlier probes.
    if (!outNote.isEmpty())
        m_identity->setText(m_identity->text() + QStringLiteral("\n\n") + outNote);

    return true;
}

} // namespace world_editor::app
