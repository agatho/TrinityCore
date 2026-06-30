#include "GossipMenuDialog.h"

#include "../db/MySqlClient.h"

#include <QDialogButtonBox>
#include <QFont>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>
#include <QVariant>

#include <set>
#include <string>

namespace world_editor::app
{

namespace
{

// Custom role on tree items: the menu id this row represents (if any).
// Used both by double-click navigation and (defensively) to detect node
// type when a single tree carries menu rows AND option rows.
constexpr int kMenuIdRole = Qt::UserRole + 1;

// Probe INFORMATION_SCHEMA for the column set of `table`.  Empty result
// means "table absent in this schema".  Mirrors the helper in NpcTextDock.
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

// Project `col AS alias` when present, "NULL AS alias" otherwise so the
// SELECT keeps stable aliases independent of fork-schema deltas.
std::string projectAs(std::set<std::string, std::less<>> const& cols,
                      char const* col, char const* alias)
{
    if (cols.find(col) != cols.end())
        return std::string(col) + " AS " + alias;
    return std::string("NULL AS ") + alias;
}

QString cellByAlias(db::QueryResult const& res, size_t row, char const* alias)
{
    auto idx = res.columnIndex(alias);
    if (!idx) return {};
    if (res.isNull(row, *idx)) return {};
    return QString::fromStdString(res.cell(row, *idx));
}

uint32_t parseUInt(QString const& s, uint32_t fallback)
{
    if (s.isEmpty()) return fallback;
    bool ok = false;
    uint32_t const v = s.toUInt(&ok);
    return ok ? v : fallback;
}

int32_t parseInt(QString const& s, int32_t fallback)
{
    if (s.isEmpty()) return fallback;
    bool ok = false;
    int32_t const v = s.toInt(&ok);
    return ok ? v : fallback;
}

// Italic-detail child for non-empty hint text (BoxText, BroadcastTextID).
// We attach as a non-MenuID child so it isn't reachable via double-click.
void appendDetail(QTreeWidgetItem* parent, QString const& text)
{
    auto* child = new QTreeWidgetItem(parent);
    child->setText(0, text);
    QFont f = child->font(0);
    f.setItalic(true);
    child->setFont(0, f);
    child->setForeground(0, QBrush(QColor(110, 110, 110)));
}

} // namespace

GossipMenuDialog::GossipMenuDialog(db::MySqlClient* db, uint32_t rootMenuId, QWidget* parent)
    : QDialog(parent)
    , m_db(db)
    , m_rootMenuId(rootMenuId)
{
    setWindowTitle(tr("Gossip menu walker - root %1").arg(rootMenuId));
    resize(720, 560);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(8, 8, 8, 8);
    root->setSpacing(6);

    m_header = new QLabel(this);
    m_header->setWordWrap(true);
    m_header->setStyleSheet(QStringLiteral("QLabel { font-weight: bold; }"));
    m_header->setText(tr("Walking gossip menu %1 ...").arg(rootMenuId));
    root->addWidget(m_header);

    m_tree = new QTreeWidget(this);
    m_tree->setHeaderLabels({ tr("Gossip tree") });
    m_tree->header()->setStretchLastSection(true);
    m_tree->setSelectionMode(QAbstractItemView::SingleSelection);
    m_tree->setUniformRowHeights(false);
    m_tree->setExpandsOnDoubleClick(false);
    connect(m_tree, &QTreeWidget::itemDoubleClicked,
            this, &GossipMenuDialog::onItemDoubleClicked);
    root->addWidget(m_tree, 1);

    m_statusLbl = new QLabel(this);
    m_statusLbl->setStyleSheet(QStringLiteral("QLabel { color: gray; }"));
    root->addWidget(m_statusLbl);

    auto* btnRow = new QHBoxLayout();
    btnRow->addStretch(1);
    m_refreshBtn = new QPushButton(tr("&Refresh"), this);
    connect(m_refreshBtn, &QPushButton::clicked, this, &GossipMenuDialog::onRefresh);
    btnRow->addWidget(m_refreshBtn);
    m_closeBtn = new QPushButton(tr("&Close"), this);
    m_closeBtn->setDefault(true);
    connect(m_closeBtn, &QPushButton::clicked, this, &QDialog::accept);
    btnRow->addWidget(m_closeBtn);
    root->addLayout(btnRow);

    walk();
}

void GossipMenuDialog::onRefresh()
{
    // Re-probe schema in case the operator pointed at a different DB
    // mid-session, then re-walk from the original root id.
    m_schemaProbed = false;
    walk();
}

void GossipMenuDialog::onItemDoubleClicked(QTreeWidgetItem* item, int /*column*/)
{
    if (!item) return;
    QVariant const v = item->data(0, kMenuIdRole);
    if (!v.isValid()) return;
    bool ok = false;
    uint32_t const menuId = v.toUInt(&ok);
    if (!ok || menuId == 0) return;
    emit menuSelected(menuId);
}

void GossipMenuDialog::probeSchema()
{
    if (m_schemaProbed) return;
    m_schemaProbed = true;

    if (!m_db || !m_db->isConnected())
        return;

    m_haveGossipMenu       = !discoverColumns(*m_db, "gossip_menu").empty();
    m_haveGossipMenuOption = !discoverColumns(*m_db, "gossip_menu_option").empty();
    m_haveGossipMenuAddon  = !discoverColumns(*m_db, "gossip_menu_addon").empty();
}

void GossipMenuDialog::walk()
{
    m_tree->clear();

    if (!m_db || !m_db->isConnected())
    {
        m_statusLbl->setText(tr("DB not connected."));
        return;
    }

    probeSchema();

    if (!m_haveGossipMenu || !m_haveGossipMenuOption)
    {
        m_statusLbl->setText(tr(
            "gossip_menu / gossip_menu_option tables not present in this schema."));
        return;
    }

    // Root tree row: synthetic anchor labelled with the requested menu id.
    auto* rootItem = new QTreeWidgetItem(m_tree);
    rootItem->setText(0, tr("Root: Menu %1").arg(m_rootMenuId));
    rootItem->setData(0, kMenuIdRole, m_rootMenuId);
    QFont f = rootItem->font(0);
    f.setBold(true);
    rootItem->setFont(0, f);

    std::unordered_set<uint32_t> visited;
    walkMenu(m_rootMenuId, rootItem, /*depth=*/0, visited);
    rootItem->setExpanded(true);

    m_statusLbl->setText(tr("Visited %1 unique menu id(s); max depth = %2.")
        .arg(static_cast<int>(visited.size()))
        .arg(kMaxDepth));
}

void GossipMenuDialog::walkMenu(uint32_t menuId, QTreeWidgetItem* parent, int depth,
                                std::unordered_set<uint32_t>& visited)
{
    if (depth >= kMaxDepth)
    {
        appendDetail(parent, tr("(depth cap %1 reached - stopping)").arg(kMaxDepth));
        return;
    }
    if (menuId == 0)
        return;

    // Cycle guard: if we've already expanded this menu in the current walk,
    // emit a back-reference stub rather than re-recursing.
    if (visited.count(menuId))
    {
        appendDetail(parent, tr("(already expanded Menu %1 - skipping cycle)").arg(menuId));
        return;
    }
    visited.insert(menuId);

    // Pull gossip_menu rows for this MenuID.  Schema-stable: MenuID + TextID
    // are required; everything else hangs off gossip_menu_option.  Multiple
    // gossip_menu rows can share a MenuID (one per condition variant).
    {
        std::string const sql =
            "SELECT MenuID, TextID FROM gossip_menu "
            "WHERE MenuID = " + std::to_string(menuId) + " ORDER BY TextID";
        db::QueryResult res;
        auto err = m_db->query(sql, res);
        if (!err.ok())
        {
            appendDetail(parent, tr("gossip_menu query failed for %1: %2")
                .arg(menuId).arg(QString::fromStdString(err.message)));
        }
        else if (res.rowCount() == 0)
        {
            appendDetail(parent, tr("(no gossip_menu row for MenuID %1)").arg(menuId));
        }
        else
        {
            for (size_t r = 0; r < res.rowCount(); ++r)
            {
                uint32_t const id     = parseUInt(QString::fromStdString(res.cell(r, 0)), 0);
                uint32_t const textId = parseUInt(QString::fromStdString(res.cell(r, 1)), 0);

                auto* menuItem = new QTreeWidgetItem(parent);
                menuItem->setText(0, tr("Menu %1: TextID %2").arg(id).arg(textId));
                menuItem->setData(0, kMenuIdRole, id);
                menuItem->setExpanded(true);

                // Pull options under this MenuID.  Project every relevant
                // column with stable aliases so missing columns fall back to
                // NULL instead of breaking the SELECT.
                auto const optCols = discoverColumns(*m_db, "gossip_menu_option");
                std::string optSql = "SELECT ";
                optSql += projectAs(optCols, "OptionID",                  "OptionID");
                optSql += ", " + projectAs(optCols, "OptionIcon",         "OptionIcon");
                optSql += ", " + projectAs(optCols, "OptionText",         "OptionText");
                optSql += ", " + projectAs(optCols, "OptionBroadcastTextID", "OptionBroadcastTextID");
                optSql += ", " + projectAs(optCols, "OptionType",         "OptionType");
                optSql += ", " + projectAs(optCols, "NpcOptionNpcFlag",   "NpcOptionNpcFlag");
                optSql += ", " + projectAs(optCols, "ActionMenuID",       "ActionMenuID");
                optSql += ", " + projectAs(optCols, "ActionPoiID",        "ActionPoiID");
                optSql += ", " + projectAs(optCols, "BoxCoded",           "BoxCoded");
                optSql += ", " + projectAs(optCols, "BoxMoney",           "BoxMoney");
                optSql += ", " + projectAs(optCols, "BoxText",            "BoxText");
                optSql += ", " + projectAs(optCols, "BoxBroadcastTextID", "BoxBroadcastTextID");
                optSql += " FROM gossip_menu_option WHERE MenuID = "
                       + std::to_string(menuId) + " ORDER BY OptionID";

                db::QueryResult optRes;
                err = m_db->query(optSql, optRes);
                if (!err.ok())
                {
                    appendDetail(menuItem, tr("gossip_menu_option query failed: %1")
                        .arg(QString::fromStdString(err.message)));
                    continue;
                }
                if (optRes.rowCount() == 0)
                {
                    appendDetail(menuItem, tr("(no options)"));
                    continue;
                }

                for (size_t or_ = 0; or_ < optRes.rowCount(); ++or_)
                {
                    uint32_t const optId    = parseUInt(cellByAlias(optRes, or_, "OptionID"), 0);
                    int32_t  const icon     = parseInt (cellByAlias(optRes, or_, "OptionIcon"), 0);
                    QString  const text     = cellByAlias(optRes, or_, "OptionText");
                    uint32_t const broadId  = parseUInt(cellByAlias(optRes, or_, "OptionBroadcastTextID"), 0);
                    int32_t  const type     = parseInt (cellByAlias(optRes, or_, "OptionType"), 0);
                    uint32_t const npcFlag  = parseUInt(cellByAlias(optRes, or_, "NpcOptionNpcFlag"), 0);
                    uint32_t const actMenu  = parseUInt(cellByAlias(optRes, or_, "ActionMenuID"), 0);
                    uint32_t const actPoi   = parseUInt(cellByAlias(optRes, or_, "ActionPoiID"), 0);
                    uint32_t const boxCoded = parseUInt(cellByAlias(optRes, or_, "BoxCoded"), 0);
                    uint64_t const boxMoney = parseUInt(cellByAlias(optRes, or_, "BoxMoney"), 0);
                    QString  const boxText  = cellByAlias(optRes, or_, "BoxText");
                    uint32_t const boxBcast = parseUInt(cellByAlias(optRes, or_, "BoxBroadcastTextID"), 0);

                    QString const label = text.isEmpty()
                        ? tr("Option %1: (no text) (icon=%2, type=%3)").arg(optId).arg(icon).arg(type)
                        : tr("Option %1: %2 (icon=%3, type=%4)").arg(optId).arg(text).arg(icon).arg(type);
                    auto* optItem = new QTreeWidgetItem(menuItem);
                    optItem->setText(0, label);
                    optItem->setExpanded(true);

                    if (npcFlag != 0)
                        appendDetail(optItem, tr("NpcOptionNpcFlag: 0x%1")
                            .arg(QString::number(npcFlag, 16).toUpper()));
                    if (actPoi != 0)
                        appendDetail(optItem, tr("ActionPoiID: %1").arg(actPoi));
                    if (broadId != 0)
                        appendDetail(optItem, tr("OptionBroadcastTextID: %1").arg(broadId));
                    if (boxCoded != 0)
                        appendDetail(optItem, tr("BoxCoded: %1").arg(boxCoded));
                    if (boxMoney != 0)
                        appendDetail(optItem, tr("BoxMoney: %1").arg(static_cast<qulonglong>(boxMoney)));
                    if (!boxText.isEmpty())
                        appendDetail(optItem, tr("BoxText: %1").arg(boxText));
                    if (boxBcast != 0)
                        appendDetail(optItem, tr("BoxBroadcastTextID: %1").arg(boxBcast));

                    // Recurse into ActionMenuID jumps.  We always recurse,
                    // even for self-references; walkMenu's `visited` set
                    // shortcuts cycles with a stub.
                    if (actMenu != 0)
                    {
                        auto* jumpItem = new QTreeWidgetItem(optItem);
                        jumpItem->setText(0, tr("-> ActionMenuID %1").arg(actMenu));
                        jumpItem->setData(0, kMenuIdRole, actMenu);
                        jumpItem->setExpanded(true);
                        walkMenu(actMenu, jumpItem, depth + 1, visited);
                    }
                }
            }
        }
    }

    // Optional addon hint (modern TC stores friend/script overrides here).
    if (m_haveGossipMenuAddon)
    {
        std::string const sql =
            "SELECT MenuID, FriendshipFactionID FROM gossip_menu_addon WHERE MenuID = "
            + std::to_string(menuId);
        db::QueryResult res;
        if (m_db->query(sql, res).ok() && res.rowCount() > 0)
        {
            uint32_t const fac = parseUInt(QString::fromStdString(res.cell(0, 1)), 0);
            if (fac != 0)
                appendDetail(parent, tr("[addon] FriendshipFactionID: %1").arg(fac));
        }
    }
}

} // namespace world_editor::app
