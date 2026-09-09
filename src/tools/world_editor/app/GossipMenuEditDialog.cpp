#include "GossipMenuEditDialog.h"

#include "../db/MySqlClient.h"

#include <QApplication>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QSplitter>
#include <QTabWidget>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>
#include <QWidget>

#include <cstdio>
#include <limits>

namespace world_editor::app
{

namespace
{

// Render the left-pane list label.  Combining MenuID and TextID is required
// because gossip_menu has a composite PK (MenuID, TextID): a single MenuID
// can legitimately appear with multiple TextIDs (server picks by condition).
QString menuLabel(uint32_t menuId, uint32_t textId)
{
    return QStringLiteral("%1/%2").arg(menuId).arg(textId);
}

} // namespace

GossipMenuEditDialog::GossipMenuEditDialog(db::MySqlClient* dbClient,
                                           QString const& worldDbName,
                                           QWidget* parent)
    : QDialog(parent), m_db(dbClient), m_worldDb(worldDbName)
{
    setWindowTitle(tr("Gossip menu editor"));
    setModal(true);
    resize(1180, 720);

    auto* outer = new QVBoxLayout(this);

    auto* splitter = new QSplitter(Qt::Horizontal, this);
    outer->addWidget(splitter, 1);

    // ----- Left pane: search + list + menu-level toolbar --------------
    auto* leftWrap = new QWidget(this);
    auto* leftLayout = new QVBoxLayout(leftWrap);
    leftLayout->setContentsMargins(0, 0, 0, 0);

    auto* searchRow = new QHBoxLayout;
    searchRow->addWidget(new QLabel(tr("Search:"), this));
    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setPlaceholderText(tr("substring on \"<MenuID>/<TextID>\""));
    searchRow->addWidget(m_searchEdit, 1);
    leftLayout->addLayout(searchRow);

    m_menuList = new QListWidget(this);
    m_menuList->setSortingEnabled(true);
    m_menuList->setSelectionMode(QAbstractItemView::SingleSelection);
    leftLayout->addWidget(m_menuList, 1);

    auto* menuToolbar = new QHBoxLayout;
    m_newMenuBtn    = new QPushButton(tr("New menu"), this);
    m_deleteMenuBtn = new QPushButton(tr("Delete menu"), this);
    m_refreshBtn    = new QPushButton(tr("Refresh"), this);
    m_deleteMenuBtn->setEnabled(false);
    menuToolbar->addWidget(m_newMenuBtn);
    menuToolbar->addWidget(m_deleteMenuBtn);
    menuToolbar->addWidget(m_refreshBtn);
    menuToolbar->addStretch(1);
    leftLayout->addLayout(menuToolbar);

    splitter->addWidget(leftWrap);

    // ----- Right pane: 3-tab QTabWidget -------------------------------
    m_tabs = new QTabWidget(this);

    // -- Tab 1: Header --
    auto* headerTab = new QWidget(this);
    auto* headerLayout = new QVBoxLayout(headerTab);
    m_headerKeyLabel = new QLabel(tr("(no menu selected)"), this);
    headerLayout->addWidget(m_headerKeyLabel);

    auto* headerForm = new QFormLayout;
    m_verifiedBuildSpin = new QSpinBox(this);
    m_verifiedBuildSpin->setRange(0, std::numeric_limits<int>::max());
    headerForm->addRow(tr("VerifiedBuild:"), m_verifiedBuildSpin);
    headerLayout->addLayout(headerForm);

    auto* headerSaveRow = new QHBoxLayout;
    m_saveHeaderBtn = new QPushButton(tr("Save header"), this);
    m_saveHeaderBtn->setEnabled(false);
    headerSaveRow->addWidget(m_saveHeaderBtn);
    headerSaveRow->addStretch(1);
    headerLayout->addLayout(headerSaveRow);
    headerLayout->addStretch(1);

    m_tabs->addTab(headerTab, tr("Header"));

    // -- Tab 2: Options --
    auto* optionsTab = new QWidget(this);
    auto* optionsLayout = new QVBoxLayout(optionsTab);

    m_optionTable = new QTableWidget(this);
    m_optionTable->setColumnCount(7);
    m_optionTable->setHorizontalHeaderLabels({
        tr("OptionID"), tr("OptionNpc"), tr("OptionText"),
        tr("ActionMenuID"), tr("ActionPoiID"), tr("BoxMoney"), tr("SpellID") });
    m_optionTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    m_optionTable->horizontalHeader()->setStretchLastSection(true);
    m_optionTable->verticalHeader()->setVisible(false);
    m_optionTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_optionTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_optionTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_optionTable->setSortingEnabled(false);  // OptionID order is the natural display order.
    optionsLayout->addWidget(m_optionTable, 1);

    auto* optionToolbar = new QHBoxLayout;
    m_addOptionBtn    = new QPushButton(tr("Add option"), this);
    m_editOptionBtn   = new QPushButton(tr("Edit option"), this);
    m_removeOptionBtn = new QPushButton(tr("Remove option"), this);
    m_addOptionBtn   ->setEnabled(false);
    m_editOptionBtn  ->setEnabled(false);
    m_removeOptionBtn->setEnabled(false);
    optionToolbar->addWidget(m_addOptionBtn);
    optionToolbar->addWidget(m_editOptionBtn);
    optionToolbar->addWidget(m_removeOptionBtn);
    optionToolbar->addStretch(1);
    optionsLayout->addLayout(optionToolbar);

    m_tabs->addTab(optionsTab, tr("Options"));

    // -- Tab 3: NPC text --
    auto* npcTextTab = new QWidget(this);
    auto* npcTextLayout = new QVBoxLayout(npcTextTab);

    m_npcTextStatus = new QLabel(tr("(no menu selected)"), this);
    npcTextLayout->addWidget(m_npcTextStatus);

    m_npcTextTable = new QTableWidget(this);
    m_npcTextTable->setColumnCount(3);
    m_npcTextTable->setHorizontalHeaderLabels({
        tr("slot"), tr("BroadcastTextID"), tr("Probability") });
    m_npcTextTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    m_npcTextTable->horizontalHeader()->setStretchLastSection(true);
    m_npcTextTable->verticalHeader()->setVisible(false);
    m_npcTextTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_npcTextTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_npcTextTable->setSortingEnabled(false);
    m_npcTextTable->setRowCount(8);
    for (int i = 0; i < 8; ++i)
    {
        auto* slot = new QTableWidgetItem;
        slot->setData(Qt::DisplayRole, i);
        m_npcTextTable->setItem(i, 0, slot);
    }
    npcTextLayout->addWidget(m_npcTextTable, 1);

    auto* npcTextToolbar = new QHBoxLayout;
    m_editNpcTextBtn = new QPushButton(tr("Edit NPC text..."), this);
    m_editNpcTextBtn->setEnabled(false);
    npcTextToolbar->addWidget(m_editNpcTextBtn);
    npcTextToolbar->addStretch(1);
    npcTextLayout->addLayout(npcTextToolbar);

    m_tabs->addTab(npcTextTab, tr("NPC text"));

    splitter->addWidget(m_tabs);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 2);

    m_statusLabel = new QLabel(tr("Loading..."), this);
    outer->addWidget(m_statusLabel);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    outer->addWidget(buttons);

    // ----- Wire signals -----------------------------------------------
    connect(m_searchEdit,     &QLineEdit::textChanged,
            this, &GossipMenuEditDialog::onMenuSearchChanged);
    connect(m_menuList,       &QListWidget::itemSelectionChanged,
            this, &GossipMenuEditDialog::onMenuSelectionChanged);
    connect(m_newMenuBtn,     &QPushButton::clicked, this, &GossipMenuEditDialog::onNewMenu);
    connect(m_deleteMenuBtn,  &QPushButton::clicked, this, &GossipMenuEditDialog::onDeleteMenu);
    connect(m_refreshBtn,     &QPushButton::clicked, this, &GossipMenuEditDialog::onRefresh);
    connect(m_saveHeaderBtn,  &QPushButton::clicked, this, &GossipMenuEditDialog::onSaveHeader);
    connect(m_addOptionBtn,   &QPushButton::clicked, this, &GossipMenuEditDialog::onAddOption);
    connect(m_editOptionBtn,  &QPushButton::clicked, this, &GossipMenuEditDialog::onEditOption);
    connect(m_removeOptionBtn,&QPushButton::clicked, this, &GossipMenuEditDialog::onRemoveOption);
    connect(m_editNpcTextBtn, &QPushButton::clicked, this, &GossipMenuEditDialog::onEditNpcText);
    connect(m_optionTable,    &QTableWidget::itemSelectionChanged,
            this, [this]() {
                bool const has = m_optionTable->currentRow() >= 0;
                m_editOptionBtn  ->setEnabled(has);
                m_removeOptionBtn->setEnabled(has);
            });

    loadMenus();
}

void GossipMenuEditDialog::onMenuSearchChanged(QString const& text)
{
    // Substring filter on the rendered label - items are hidden so the
    // underlying (MenuID, TextID) UserRole data survives the filter.
    QString const needle = text.trimmed();
    for (int i = 0; i < m_menuList->count(); ++i)
    {
        auto* item = m_menuList->item(i);
        if (needle.isEmpty())
            item->setHidden(false);
        else
            item->setHidden(!item->text().contains(needle, Qt::CaseInsensitive));
    }
}

void GossipMenuEditDialog::onMenuSelectionChanged()
{
    if (m_loading) return;
    uint32_t menuId = 0, textId = 0;
    bool const has = selectedKey(menuId, textId);
    loadMenu(has ? menuId : 0, has ? textId : 0);
    m_saveHeaderBtn  ->setEnabled(has);
    m_deleteMenuBtn  ->setEnabled(has);
    m_addOptionBtn   ->setEnabled(has);
    m_editNpcTextBtn ->setEnabled(has);
}

bool GossipMenuEditDialog::selectedKey(uint32_t& menuIdOut, uint32_t& textIdOut) const
{
    auto* item = m_menuList->currentItem();
    if (!item) return false;
    // (MenuID, TextID) packed into Qt::UserRole as a 64-bit value:
    // high 32 bits = MenuID, low 32 bits = TextID.  Composite PK round-trip
    // without parsing the label.
    uint64_t const packed = item->data(Qt::UserRole).toULongLong();
    menuIdOut = uint32_t(packed >> 32);
    textIdOut = uint32_t(packed & 0xFFFFFFFFu);
    return true;
}

bool GossipMenuEditDialog::currentOptionId(uint32_t& optionIdOut) const
{
    int const row = m_optionTable->currentRow();
    if (row < 0) return false;
    auto* cell = m_optionTable->item(row, 0);
    if (!cell) return false;
    optionIdOut = uint32_t(cell->data(Qt::DisplayRole).toULongLong());
    return true;
}

void GossipMenuEditDialog::loadMenus()
{
    m_loading = true;
    uint32_t prevMenu = 0, prevText = 0;
    bool const hadSelection = selectedKey(prevMenu, prevText);
    m_menuList->clear();

    if (!m_db || !m_db->isConnected())
    {
        m_statusLabel->setText(tr("DB not connected."));
        m_loading = false;
        return;
    }

    char sql[384];
    std::snprintf(sql, sizeof(sql),
        "SELECT MenuID, TextID FROM %s.gossip_menu ORDER BY MenuID, TextID",
        m_worldDb.toStdString().c_str());
    db::QueryResult res;
    QApplication::setOverrideCursor(Qt::WaitCursor);
    auto const err = m_db->query(sql, res);
    QApplication::restoreOverrideCursor();
    if (!err.ok())
    {
        m_statusLabel->setText(tr("gossip_menu query failed: %1")
            .arg(QString::fromStdString(err.message)));
        m_loading = false;
        return;
    }

    QListWidgetItem* restoreSelect = nullptr;
    for (size_t r = 0; r < res.rowCount(); ++r)
    {
        uint32_t const mid = uint32_t(res.asUInt64(r, 0).value_or(0));
        uint32_t const tid = uint32_t(res.asUInt64(r, 1).value_or(0));
        auto* item = new QListWidgetItem(menuLabel(mid, tid), m_menuList);
        uint64_t const packed = (uint64_t(mid) << 32) | uint64_t(tid);
        item->setData(Qt::UserRole, qulonglong(packed));
        if (hadSelection && mid == prevMenu && tid == prevText)
            restoreSelect = item;
    }

    m_statusLabel->setText(tr("menus=%1").arg(res.rowCount()));
    onMenuSearchChanged(m_searchEdit->text());
    m_loading = false;

    if (restoreSelect)
        m_menuList->setCurrentItem(restoreSelect);
    else
        loadMenu(0, 0);
}

void GossipMenuEditDialog::loadMenu(uint32_t menuId, uint32_t textId)
{
    m_loading = true;
    m_headerKeyLabel->setText(tr("(no menu selected)"));
    m_verifiedBuildSpin->setValue(0);
    m_optionTable->setRowCount(0);
    // NPC-text rows: leave the 8-row skeleton but blank columns 1+2.
    for (int i = 0; i < 8; ++i)
    {
        auto* btCell = new QTableWidgetItem;
        btCell->setData(Qt::DisplayRole, 0);
        m_npcTextTable->setItem(i, 1, btCell);
        auto* probCell = new QTableWidgetItem;
        probCell->setData(Qt::DisplayRole, 0.0);
        m_npcTextTable->setItem(i, 2, probCell);
    }
    m_npcTextStatus->setText(tr("(no menu selected)"));

    if (menuId == 0 || !m_db || !m_db->isConnected())
    {
        m_loading = false;
        return;
    }

    m_headerKeyLabel->setText(tr("MenuID = %1, TextID = %2").arg(menuId).arg(textId));

    // Header VerifiedBuild.  COALESCE so legacy snapshots without the column
    // degrade to 0 rather than blowing up the SELECT.
    char hsql[384];
    std::snprintf(hsql, sizeof(hsql),
        "SELECT COALESCE(VerifiedBuild, 0) FROM %s.gossip_menu "
        "WHERE MenuID=%u AND TextID=%u",
        m_worldDb.toStdString().c_str(), menuId, textId);
    db::QueryResult hRes;
    auto err = m_db->query(hsql, hRes);
    if (err.ok() && hRes.rowCount() > 0)
        m_verifiedBuildSpin->setValue(int(hRes.asInt64(0, 0).value_or(0)));

    // Options for this MenuID.  gossip_menu_option is keyed by MenuID alone
    // (OptionID is the within-menu ordinal), so we don't filter by TextID
    // here even though the user picked a (MenuID, TextID) pair.
    char osql[768];
    std::snprintf(osql, sizeof(osql),
        "SELECT OptionID, OptionNpc, COALESCE(OptionText, ''), "
        "       ActionMenuID, ActionPoiID, BoxMoney, SpellID "
        "FROM %s.gossip_menu_option WHERE MenuID=%u ORDER BY OptionID",
        m_worldDb.toStdString().c_str(), menuId);
    db::QueryResult oRes;
    err = m_db->query(osql, oRes);
    if (!err.ok())
    {
        m_statusLabel->setText(tr("gossip_menu_option query failed: %1")
            .arg(QString::fromStdString(err.message)));
        m_loading = false;
        return;
    }

    m_optionTable->setRowCount(int(oRes.rowCount()));
    for (size_t r = 0; r < oRes.rowCount(); ++r)
    {
        auto setU = [&](int col, uint64_t v) {
            auto* c = new QTableWidgetItem;
            c->setData(Qt::DisplayRole, qulonglong(v));
            m_optionTable->setItem(int(r), col, c);
        };
        setU(0, oRes.asUInt64(r, 0).value_or(0));
        setU(1, oRes.asUInt64(r, 1).value_or(0));
        m_optionTable->setItem(int(r), 2,
            new QTableWidgetItem(QString::fromStdString(oRes.cell(r, 2))));
        setU(3, oRes.asUInt64(r, 3).value_or(0));
        setU(4, oRes.asUInt64(r, 4).value_or(0));
        setU(5, oRes.asUInt64(r, 5).value_or(0));
        setU(6, oRes.asUInt64(r, 6).value_or(0));
    }

    // NPC text row keyed by gossip_menu.TextID.  npc_text.ID == TextID.
    char nsql[640];
    std::snprintf(nsql, sizeof(nsql),
        "SELECT BroadcastTextID0, BroadcastTextID1, BroadcastTextID2, BroadcastTextID3, "
        "       BroadcastTextID4, BroadcastTextID5, BroadcastTextID6, BroadcastTextID7, "
        "       Probability0, Probability1, Probability2, Probability3, "
        "       Probability4, Probability5, Probability6, Probability7 "
        "FROM %s.npc_text WHERE ID=%u",
        m_worldDb.toStdString().c_str(), textId);
    db::QueryResult nRes;
    err = m_db->query(nsql, nRes);
    if (!err.ok())
    {
        m_npcTextStatus->setText(tr("npc_text query failed: %1")
            .arg(QString::fromStdString(err.message)));
    }
    else if (nRes.rowCount() == 0)
    {
        m_npcTextStatus->setText(tr("npc_text ID=%1: no row").arg(textId));
    }
    else
    {
        m_npcTextStatus->setText(tr("npc_text ID=%1").arg(textId));
        for (int i = 0; i < 8; ++i)
        {
            auto* btCell = new QTableWidgetItem;
            btCell->setData(Qt::DisplayRole, qulonglong(nRes.asUInt64(0, size_t(i)).value_or(0)));
            m_npcTextTable->setItem(i, 1, btCell);
            auto* probCell = new QTableWidgetItem;
            probCell->setData(Qt::DisplayRole, nRes.asDouble(0, size_t(8 + i)).value_or(0.0));
            m_npcTextTable->setItem(i, 2, probCell);
        }
    }

    m_statusLabel->setText(tr("MenuID=%1 TextID=%2  options=%3")
        .arg(menuId).arg(textId).arg(oRes.rowCount()));
    m_loading = false;
}

bool GossipMenuEditDialog::runInTransaction(QString const& sql, QString const& description, uint64_t* affectedOut)
{
    if (!m_db || !m_db->isConnected())
    {
        QMessageBox::warning(this, tr("Not connected"), tr("World DB not open."));
        return false;
    }
    auto err = m_db->exec("START TRANSACTION");
    if (!err.ok())
    {
        QMessageBox::critical(this, tr("Transaction failed"),
            tr("BEGIN failed: %1").arg(QString::fromStdString(err.message)));
        return false;
    }
    uint64_t affected = 0;
    err = m_db->exec(sql.toStdString(), &affected);
    if (!err.ok())
    {
        (void)m_db->exec("ROLLBACK");
        QMessageBox::critical(this, tr("DML failed"),
            tr("%1\n\n%2").arg(description).arg(QString::fromStdString(err.message)));
        return false;
    }
    err = m_db->exec("COMMIT");
    if (!err.ok())
    {
        (void)m_db->exec("ROLLBACK");
        QMessageBox::critical(this, tr("Commit failed"),
            tr("%1\n\n%2").arg(description).arg(QString::fromStdString(err.message)));
        return false;
    }
    if (affectedOut) *affectedOut = affected;
    m_statusLabel->setText(tr("%1 (affected=%2)").arg(description).arg(qulonglong(affected)));
    return true;
}

void GossipMenuEditDialog::onSaveHeader()
{
    uint32_t menuId = 0, textId = 0;
    if (!selectedKey(menuId, textId)) return;

    QString const upd = QStringLiteral(
        "UPDATE %1.gossip_menu SET VerifiedBuild=%2 WHERE MenuID=%3 AND TextID=%4")
        .arg(m_worldDb)
        .arg(m_verifiedBuildSpin->value())
        .arg(menuId).arg(textId);
    if (runInTransaction(upd,
            tr("UPDATE gossip_menu (MenuID=%1, TextID=%2)").arg(menuId).arg(textId)))
        loadMenus();
}

void GossipMenuEditDialog::onNewMenu()
{
    if (!m_db || !m_db->isConnected())
    {
        QMessageBox::warning(this, tr("Not connected"), tr("World DB not open."));
        return;
    }

    // Prompt for the TextID that the new menu should point at.  We default
    // to 0 (npc_text empty row) but operators usually paste a real npc_text.ID
    // that they have already authored.
    QDialog prompt(this);
    prompt.setWindowTitle(tr("New gossip_menu"));
    prompt.setModal(true);
    auto* pf = new QFormLayout(&prompt);
    auto* textIdSpin = new QSpinBox(&prompt);
    textIdSpin->setRange(0, std::numeric_limits<int>::max());
    pf->addRow(tr("TextID (npc_text.ID):"), textIdSpin);
    auto* pBtns = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &prompt);
    connect(pBtns, &QDialogButtonBox::accepted, &prompt, &QDialog::accept);
    connect(pBtns, &QDialogButtonBox::rejected, &prompt, &QDialog::reject);
    pf->addRow(pBtns);
    if (prompt.exec() != QDialog::Accepted)
        return;
    uint32_t const newTextId = uint32_t(textIdSpin->value());

    char maxSql[256];
    std::snprintf(maxSql, sizeof(maxSql),
        "SELECT COALESCE(MAX(MenuID), 0)+1 FROM %s.gossip_menu",
        m_worldDb.toStdString().c_str());
    db::QueryResult maxRes;
    auto err = m_db->query(maxSql, maxRes);
    if (!err.ok() || maxRes.rowCount() == 0)
    {
        QMessageBox::critical(this, tr("New menu"),
            tr("Could not reserve next MenuID: %1").arg(QString::fromStdString(err.message)));
        return;
    }
    uint32_t const newMenuId = uint32_t(maxRes.asUInt64(0, 0).value_or(0));

    QString const ins = QStringLiteral(
        "INSERT INTO %1.gossip_menu (MenuID, TextID, VerifiedBuild) VALUES (%2, %3, 0)")
        .arg(m_worldDb).arg(newMenuId).arg(newTextId);
    if (runInTransaction(ins,
            tr("INSERT gossip_menu (MenuID=%1, TextID=%2)").arg(newMenuId).arg(newTextId)))
    {
        loadMenus();
        // Auto-select the newly-inserted row.
        for (int i = 0; i < m_menuList->count(); ++i)
        {
            auto* item = m_menuList->item(i);
            uint64_t const packed = item->data(Qt::UserRole).toULongLong();
            if (uint32_t(packed >> 32) == newMenuId && uint32_t(packed & 0xFFFFFFFFu) == newTextId)
            {
                m_menuList->setCurrentItem(item);
                break;
            }
        }
    }
}

void GossipMenuEditDialog::onDeleteMenu()
{
    uint32_t menuId = 0, textId = 0;
    if (!selectedKey(menuId, textId)) return;

    // Warn if any creature.gossip_menu_id still points at this menu.  We
    // don't block - some operators wipe a menu before reassigning creatures.
    char refSql[384];
    std::snprintf(refSql, sizeof(refSql),
        "SELECT COUNT(*) FROM %s.creature_template WHERE gossip_menu_id=%u",
        m_worldDb.toStdString().c_str(), menuId);
    db::QueryResult refRes;
    uint64_t refs = 0;
    if (m_db->query(refSql, refRes).ok() && refRes.rowCount() > 0)
        refs = refRes.asUInt64(0, 0).value_or(0);

    QString msg = tr("Delete gossip_menu MenuID=%1 (TextID=%2) and all its options?")
        .arg(menuId).arg(textId);
    if (refs > 0)
        msg += tr("\n\nWarning: %1 creature_template row(s) still reference gossip_menu_id=%2.")
            .arg(refs).arg(menuId);
    if (QMessageBox::question(this, tr("Delete menu"), msg,
            QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
        return;

    // Two DELETEs in one tx; runInTransaction only handles a single statement.
    if (!m_db || !m_db->isConnected())
    {
        QMessageBox::warning(this, tr("Not connected"), tr("World DB not open."));
        return;
    }
    auto err = m_db->exec("START TRANSACTION");
    if (!err.ok())
    {
        QMessageBox::critical(this, tr("Transaction failed"),
            tr("BEGIN failed: %1").arg(QString::fromStdString(err.message)));
        return;
    }
    char delOpts[256], delMenu[256];
    std::snprintf(delOpts, sizeof(delOpts),
        "DELETE FROM %s.gossip_menu_option WHERE MenuID=%u",
        m_worldDb.toStdString().c_str(), menuId);
    std::snprintf(delMenu, sizeof(delMenu),
        "DELETE FROM %s.gossip_menu WHERE MenuID=%u AND TextID=%u",
        m_worldDb.toStdString().c_str(), menuId, textId);
    err = m_db->exec(delOpts);
    if (err.ok()) err = m_db->exec(delMenu);
    if (!err.ok())
    {
        (void)m_db->exec("ROLLBACK");
        QMessageBox::critical(this, tr("Delete failed"),
            tr("DELETE failed: %1").arg(QString::fromStdString(err.message)));
        return;
    }
    err = m_db->exec("COMMIT");
    if (!err.ok())
    {
        (void)m_db->exec("ROLLBACK");
        QMessageBox::critical(this, tr("Commit failed"),
            tr("COMMIT failed: %1").arg(QString::fromStdString(err.message)));
        return;
    }
    m_statusLabel->setText(tr("Deleted MenuID=%1 (TextID=%2) + options").arg(menuId).arg(textId));
    loadMenus();
}

void GossipMenuEditDialog::onRefresh()
{
    loadMenus();
}

void GossipMenuEditDialog::onAddOption()
{
    openOptionModal(/*editOptionId=*/std::numeric_limits<uint32_t>::max());
}

void GossipMenuEditDialog::onEditOption()
{
    uint32_t opt = 0;
    if (!currentOptionId(opt)) return;
    openOptionModal(opt);
}

void GossipMenuEditDialog::onRemoveOption()
{
    uint32_t menuId = 0, textId = 0;
    uint32_t opt = 0;
    if (!selectedKey(menuId, textId) || !currentOptionId(opt))
        return;

    if (QMessageBox::question(this, tr("Remove option"),
            tr("Delete option OptionID=%1 from MenuID=%2?").arg(opt).arg(menuId),
            QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
        return;

    QString const del = QStringLiteral(
        "DELETE FROM %1.gossip_menu_option WHERE MenuID=%2 AND OptionID=%3")
        .arg(m_worldDb).arg(menuId).arg(opt);
    if (runInTransaction(del,
            tr("DELETE gossip_menu_option (MenuID=%1, OptionID=%2)").arg(menuId).arg(opt)))
        loadMenu(menuId, textId);
}

void GossipMenuEditDialog::openOptionModal(uint32_t editOptionId)
{
    uint32_t menuId = 0, textId = 0;
    if (!selectedKey(menuId, textId)) return;

    QDialog dlg(this);
    bool const isEdit = (editOptionId != std::numeric_limits<uint32_t>::max());
    dlg.setWindowTitle(isEdit ? tr("Edit gossip option") : tr("Add gossip option"));
    dlg.setModal(true);

    auto* form = new QFormLayout(&dlg);

    auto* optionIdSpin = new QSpinBox(&dlg);
    optionIdSpin->setRange(0, std::numeric_limits<int>::max());
    auto* optionNpcSpin = new QSpinBox(&dlg);
    optionNpcSpin->setRange(0, std::numeric_limits<int>::max());
    auto* actionMenuSpin = new QSpinBox(&dlg);
    actionMenuSpin->setRange(0, std::numeric_limits<int>::max());
    auto* actionPoiSpin = new QSpinBox(&dlg);
    actionPoiSpin->setRange(0, std::numeric_limits<int>::max());
    auto* spellIdSpin = new QSpinBox(&dlg);
    spellIdSpin->setRange(0, std::numeric_limits<int>::max());
    auto* boxMoneySpin = new QSpinBox(&dlg);
    boxMoneySpin->setRange(0, std::numeric_limits<int>::max());
    auto* optionTextEdit = new QLineEdit(&dlg);
    optionTextEdit->setMaxLength(255);

    form->addRow(tr("OptionID:"),     optionIdSpin);
    form->addRow(tr("OptionNpc:"),    optionNpcSpin);
    form->addRow(tr("OptionText:"),   optionTextEdit);
    form->addRow(tr("ActionMenuID:"), actionMenuSpin);
    form->addRow(tr("ActionPoiID:"),  actionPoiSpin);
    form->addRow(tr("SpellID:"),      spellIdSpin);
    form->addRow(tr("BoxMoney:"),     boxMoneySpin);

    if (isEdit)
    {
        int const row = m_optionTable->currentRow();
        if (row >= 0)
        {
            auto getU = [&](int col) -> uint64_t {
                auto* c = m_optionTable->item(row, col);
                return c ? c->data(Qt::DisplayRole).toULongLong() : 0;
            };
            optionIdSpin   ->setValue(int(getU(0)));
            optionNpcSpin  ->setValue(int(getU(1)));
            optionTextEdit ->setText(m_optionTable->item(row, 2) ? m_optionTable->item(row, 2)->text() : QString());
            actionMenuSpin ->setValue(int(getU(3)));
            actionPoiSpin  ->setValue(int(getU(4)));
            boxMoneySpin   ->setValue(int(getU(5)));
            spellIdSpin    ->setValue(int(getU(6)));
        }
        // PK field is fixed for an UPDATE - the operator wants to edit the
        // payload of the selected row, not rekey it.
        optionIdSpin->setEnabled(false);
    }
    else
    {
        // Seed OptionID with MAX(OptionID)+1 for the current MenuID so the
        // operator doesn't pick a colliding value by hand.
        char maxSql[320];
        std::snprintf(maxSql, sizeof(maxSql),
            "SELECT COALESCE(MAX(OptionID)+1, 0) FROM %s.gossip_menu_option WHERE MenuID=%u",
            m_worldDb.toStdString().c_str(), menuId);
        db::QueryResult maxRes;
        if (m_db->query(maxSql, maxRes).ok() && maxRes.rowCount() > 0)
            optionIdSpin->setValue(int(maxRes.asUInt64(0, 0).value_or(0)));
    }

    auto* btns = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    connect(btns, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(btns, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    form->addRow(btns);

    if (dlg.exec() != QDialog::Accepted)
        return;

    uint32_t const optId    = uint32_t(optionIdSpin   ->value());
    uint32_t const optNpc   = uint32_t(optionNpcSpin  ->value());
    uint32_t const actMenu  = uint32_t(actionMenuSpin ->value());
    uint32_t const actPoi   = uint32_t(actionPoiSpin  ->value());
    uint32_t const spellId  = uint32_t(spellIdSpin    ->value());
    uint32_t const boxMoney = uint32_t(boxMoneySpin   ->value());
    QString  const escText  = QString::fromStdString(
        m_db->escapeString(optionTextEdit->text().toStdString()));

    if (isEdit)
    {
        QString const upd = QStringLiteral(
            "UPDATE %1.gossip_menu_option SET OptionNpc=%2, OptionText='%3', "
            "ActionMenuID=%4, ActionPoiID=%5, BoxMoney=%6, SpellID=%7 "
            "WHERE MenuID=%8 AND OptionID=%9")
            .arg(m_worldDb)
            .arg(optNpc).arg(escText)
            .arg(actMenu).arg(actPoi).arg(boxMoney).arg(spellId)
            .arg(menuId).arg(editOptionId);
        if (runInTransaction(upd,
                tr("UPDATE gossip_menu_option (MenuID=%1, OptionID=%2)").arg(menuId).arg(editOptionId)))
            loadMenu(menuId, textId);
    }
    else
    {
        // Explicit zero defaults for columns the simplified form doesn't
        // expose - keeps the INSERT compatible with the modern column set.
        QString const ins = QStringLiteral(
            "INSERT INTO %1.gossip_menu_option "
            "(MenuID, OptionID, OptionNpc, OptionText, OptionBroadcastTextID, Language, "
            " ActionMenuID, ActionPoiID, GossipNpcOptionID, BoxCoded, BoxMoney, BoxText, "
            " BoxBroadcastTextID, SpellID, OverrideIconID, VerifiedBuild) "
            "VALUES (%2, %3, %4, '%5', 0, 0, %6, %7, 0, 0, %8, '', 0, %9, 0, 0)")
            .arg(m_worldDb)
            .arg(menuId).arg(optId).arg(optNpc).arg(escText)
            .arg(actMenu).arg(actPoi).arg(boxMoney).arg(spellId);
        if (runInTransaction(ins,
                tr("INSERT gossip_menu_option (MenuID=%1, OptionID=%2)").arg(menuId).arg(optId)))
            loadMenu(menuId, textId);
    }
}

void GossipMenuEditDialog::onEditNpcText()
{
    uint32_t menuId = 0, textId = 0;
    if (!selectedKey(menuId, textId)) return;
    if (!m_db || !m_db->isConnected())
    {
        QMessageBox::warning(this, tr("Not connected"), tr("World DB not open."));
        return;
    }

    // Pre-fetch current values so the modal opens populated.  If the row is
    // missing (gossip_menu.TextID points at a TextID with no npc_text row)
    // we still let the operator UPDATE - they'll get a 0-affected-rows result
    // and can switch to an INSERT path below.
    char sql[640];
    std::snprintf(sql, sizeof(sql),
        "SELECT BroadcastTextID0, BroadcastTextID1, BroadcastTextID2, BroadcastTextID3, "
        "       BroadcastTextID4, BroadcastTextID5, BroadcastTextID6, BroadcastTextID7, "
        "       Probability0, Probability1, Probability2, Probability3, "
        "       Probability4, Probability5, Probability6, Probability7 "
        "FROM %s.npc_text WHERE ID=%u",
        m_worldDb.toStdString().c_str(), textId);
    db::QueryResult res;
    auto err = m_db->query(sql, res);
    if (!err.ok())
    {
        QMessageBox::critical(this, tr("Edit NPC text"),
            tr("SELECT failed: %1").arg(QString::fromStdString(err.message)));
        return;
    }
    bool const rowExists = res.rowCount() > 0;

    QDialog dlg(this);
    dlg.setWindowTitle(tr("Edit npc_text ID=%1").arg(textId));
    dlg.setModal(true);
    auto* form = new QFormLayout(&dlg);

    QSpinBox*       btSpins[8];
    QDoubleSpinBox* probSpins[8];
    for (int i = 0; i < 8; ++i)
    {
        btSpins[i] = new QSpinBox(&dlg);
        btSpins[i]->setRange(0, std::numeric_limits<int>::max());
        if (rowExists)
            btSpins[i]->setValue(int(res.asUInt64(0, size_t(i)).value_or(0)));
        form->addRow(tr("BroadcastTextID%1:").arg(i), btSpins[i]);

        probSpins[i] = new QDoubleSpinBox(&dlg);
        probSpins[i]->setRange(0.0, 1.0);
        probSpins[i]->setDecimals(4);
        probSpins[i]->setSingleStep(0.05);
        if (rowExists)
            probSpins[i]->setValue(res.asDouble(0, size_t(8 + i)).value_or(0.0));
        form->addRow(tr("Probability%1:").arg(i), probSpins[i]);
    }

    auto* btns = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    connect(btns, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(btns, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    form->addRow(btns);

    if (dlg.exec() != QDialog::Accepted)
        return;

    QStringList setClauses;
    setClauses.reserve(16);
    for (int i = 0; i < 8; ++i)
        setClauses << QStringLiteral("BroadcastTextID%1=%2").arg(i).arg(btSpins[i]->value());
    for (int i = 0; i < 8; ++i)
        setClauses << QStringLiteral("Probability%1=%2")
            .arg(i).arg(QString::number(probSpins[i]->value(), 'f', 4));

    if (rowExists)
    {
        QString const upd = QStringLiteral("UPDATE %1.npc_text SET %2 WHERE ID=%3")
            .arg(m_worldDb).arg(setClauses.join(", ")).arg(textId);
        if (runInTransaction(upd, tr("UPDATE npc_text (ID=%1)").arg(textId)))
            loadMenu(menuId, textId);
    }
    else
    {
        // INSERT path - build a full column list so the row is well-formed.
        QStringList cols;
        cols << "ID";
        for (int i = 0; i < 8; ++i) cols << QStringLiteral("BroadcastTextID%1").arg(i);
        for (int i = 0; i < 8; ++i) cols << QStringLiteral("Probability%1").arg(i);
        cols << "VerifiedBuild";
        QStringList vals;
        vals << QString::number(textId);
        for (int i = 0; i < 8; ++i) vals << QString::number(btSpins[i]->value());
        for (int i = 0; i < 8; ++i) vals << QString::number(probSpins[i]->value(), 'f', 4);
        vals << "0";
        QString const ins = QStringLiteral("INSERT INTO %1.npc_text (%2) VALUES (%3)")
            .arg(m_worldDb).arg(cols.join(", ")).arg(vals.join(", "));
        if (runInTransaction(ins, tr("INSERT npc_text (ID=%1)").arg(textId)))
            loadMenu(menuId, textId);
    }
}

} // namespace world_editor::app
