#include "GroupsPoolsDialog.h"

#include "ConfirmSqlDialog.h"
#include "PoolTemplateEditDialog.h"
#include "SpawnGroupTemplateEditDialog.h"

#include <QApplication>
#include <QDialogButtonBox>
#include <QInputDialog>
#include <QMenu>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QMessageBox>
#include <QPushButton>
#include <QSortFilterProxyModel>
#include <QStandardItemModel>
#include <QTabWidget>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTimer>
#include <QVBoxLayout>

#include <cstdio>
#include <functional>

namespace world_editor::app
{

GroupsPoolsDialog::GroupsPoolsDialog(db::MySqlClient* dbClient,
                                     QString const& worldDbName,
                                     QWidget* parent)
    : QDialog(parent), m_dbClient(dbClient), m_worldDb(worldDbName)
{
    setWindowTitle(tr("Groups & Pools"));
    setModal(false);
    resize(900, 640);

    m_tabs = new QTabWidget(this);
    m_statusLabel = new QLabel(tr("loading..."), this);

    // ---- Spawn Groups tab ----
    {
        auto* page = new QWidget(this);
        auto* row  = new QHBoxLayout(page);

        auto* leftBox = new QVBoxLayout;
        m_groupFilter = new QLineEdit(this);
        m_groupFilter->setPlaceholderText(tr("filter by id or name..."));
        m_groupList   = new QListView(this);
        m_groupList->setUniformItemSizes(true);
        m_groupList->setEditTriggers(QAbstractItemView::NoEditTriggers);
        m_groupModel  = new QStandardItemModel(this);
        m_groupModel->setHorizontalHeaderLabels({ tr("groupId"), tr("name"), tr("members") });
        m_groupProxy  = new QSortFilterProxyModel(this);
        m_groupProxy->setFilterCaseSensitivity(Qt::CaseInsensitive);
        m_groupProxy->setFilterKeyColumn(-1);
        m_groupProxy->setSourceModel(m_groupModel);
        m_groupList->setModel(m_groupProxy);
        leftBox->addWidget(new QLabel(tr("spawn_group_template")));
        leftBox->addWidget(m_groupFilter);
        // Insert/Edit/Delete toolbar for spawn_group_template rows.
        m_groupInsertButton = new QPushButton(tr("Insert group"), this);
        m_groupEditButton   = new QPushButton(tr("Edit group"), this);
        m_groupDeleteButton = new QPushButton(tr("Delete group"), this);
        m_groupEditButton->setEnabled(false);
        m_groupDeleteButton->setEnabled(false);
        auto* groupToolbar = new QHBoxLayout;
        groupToolbar->addWidget(m_groupInsertButton);
        groupToolbar->addWidget(m_groupEditButton);
        groupToolbar->addWidget(m_groupDeleteButton);
        groupToolbar->addStretch(1);
        leftBox->addLayout(groupToolbar);
        leftBox->addWidget(m_groupList, 1);
        m_newGroupButton = new QPushButton(tr("+ New group..."), this);
        leftBox->addWidget(m_newGroupButton);
        row->addLayout(leftBox, 1);

        auto* rightBox = new QVBoxLayout;
        rightBox->addWidget(new QLabel(tr("members (spawnType, spawnId)")));
        // Member-level toolbar: add/remove + 'add from selection' hint.
        m_memberAddButton     = new QPushButton(tr("Add member (by guid)..."), this);
        m_memberRemoveButton  = new QPushButton(tr("Remove member"), this);
        m_memberFromSelButton = new QPushButton(tr("Add from selection"), this);
        m_memberAddButton->setEnabled(false);
        m_memberRemoveButton->setEnabled(false);
        // GroupsPoolsDialog has no live spawn-selection context, so this
        // button only surfaces a tooltip explaining the workflow.
        m_memberFromSelButton->setEnabled(false);
        m_memberFromSelButton->setToolTip(tr(
            "Select spawns in the main window first, then re-enter this dialog "
            "with the selected list to add them as group members."));
        auto* memberToolbar = new QHBoxLayout;
        memberToolbar->addWidget(m_memberAddButton);
        memberToolbar->addWidget(m_memberRemoveButton);
        memberToolbar->addWidget(m_memberFromSelButton);
        memberToolbar->addStretch(1);
        rightBox->addLayout(memberToolbar);
        m_groupMembers = new QTableWidget(this);
        m_groupMembers->setColumnCount(2);
        m_groupMembers->setHorizontalHeaderLabels({ tr("spawnType"), tr("spawnId") });
        m_groupMembers->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
        m_groupMembers->verticalHeader()->setVisible(false);
        m_groupMembers->setEditTriggers(QAbstractItemView::NoEditTriggers);
        m_groupMembers->setSelectionBehavior(QAbstractItemView::SelectRows);
        m_groupMembers->setSelectionMode(QAbstractItemView::SingleSelection);
        rightBox->addWidget(m_groupMembers, 1);
        // Header edit: groupName + groupFlags + Save.
        m_groupNameEdit  = new QLineEdit(this); m_groupNameEdit->setEnabled(false);
        m_groupFlagsEdit = new QLineEdit(this); m_groupFlagsEdit->setEnabled(false);
        m_groupSaveButton = new QPushButton(tr("Save header"), this);
        m_groupSaveButton->setEnabled(false);
        auto* nameRow = new QHBoxLayout;
        nameRow->addWidget(new QLabel(tr("groupName:")));
        nameRow->addWidget(m_groupNameEdit, 1);
        nameRow->addWidget(new QLabel(tr("flags:")));
        nameRow->addWidget(m_groupFlagsEdit);
        nameRow->addWidget(m_groupSaveButton);
        rightBox->addLayout(nameRow);
        m_highlightButton = new QPushButton(tr("Highlight creature members on map"), this);
        m_highlightButton->setEnabled(false);
        rightBox->addWidget(m_highlightButton);
        m_groupMembers->setContextMenuPolicy(Qt::CustomContextMenu);
        row->addLayout(rightBox, 1);
        m_tabs->addTab(page, tr("Spawn Groups"));
    }

    // ---- Pools tab ----
    {
        auto* page = new QWidget(this);
        auto* row  = new QHBoxLayout(page);
        auto* leftBox = new QVBoxLayout;
        m_poolFilter = new QLineEdit(this);
        m_poolFilter->setPlaceholderText(tr("filter by id or description..."));
        m_poolList   = new QListView(this);
        m_poolList->setUniformItemSizes(true);
        m_poolList->setEditTriggers(QAbstractItemView::NoEditTriggers);
        m_poolModel  = new QStandardItemModel(this);
        m_poolModel->setHorizontalHeaderLabels(
            { tr("entry"), tr("max_limit"), tr("description"), tr("members") });
        m_poolProxy  = new QSortFilterProxyModel(this);
        m_poolProxy->setFilterCaseSensitivity(Qt::CaseInsensitive);
        m_poolProxy->setFilterKeyColumn(-1);
        m_poolProxy->setSourceModel(m_poolModel);
        m_poolList->setModel(m_poolProxy);
        leftBox->addWidget(new QLabel(tr("pool_template")));
        leftBox->addWidget(m_poolFilter);
        // Insert/Edit/Delete toolbar for pool_template rows.
        m_poolInsertButton = new QPushButton(tr("Insert pool"), this);
        m_poolEditButton   = new QPushButton(tr("Edit pool"), this);
        m_poolDeleteButton = new QPushButton(tr("Delete pool"), this);
        m_poolEditButton->setEnabled(false);
        m_poolDeleteButton->setEnabled(false);
        auto* poolToolbar = new QHBoxLayout;
        poolToolbar->addWidget(m_poolInsertButton);
        poolToolbar->addWidget(m_poolEditButton);
        poolToolbar->addWidget(m_poolDeleteButton);
        poolToolbar->addStretch(1);
        leftBox->addLayout(poolToolbar);
        leftBox->addWidget(m_poolList, 1);
        m_newPoolButton = new QPushButton(tr("+ New pool..."), this);
        leftBox->addWidget(m_newPoolButton);
        row->addLayout(leftBox, 1);

        auto* rightBox = new QVBoxLayout;
        rightBox->addWidget(new QLabel(tr("members (pool_members)")));
        // Member-level toolbar: Add creature / Add gameobject / Remove / Edit chance.
        m_poolMemberAddCreatureButton   = new QPushButton(tr("Add creature (guid + chance)..."), this);
        m_poolMemberAddGameobjectButton = new QPushButton(tr("Add gameobject (guid + chance)..."), this);
        m_poolMemberRemoveButton        = new QPushButton(tr("Remove member"), this);
        m_poolMemberEditChanceButton    = new QPushButton(tr("Edit chance"), this);
        m_poolMemberAddCreatureButton->setEnabled(false);
        m_poolMemberAddGameobjectButton->setEnabled(false);
        m_poolMemberRemoveButton->setEnabled(false);
        m_poolMemberEditChanceButton->setEnabled(false);
        auto* poolMemberToolbar = new QHBoxLayout;
        poolMemberToolbar->addWidget(m_poolMemberAddCreatureButton);
        poolMemberToolbar->addWidget(m_poolMemberAddGameobjectButton);
        poolMemberToolbar->addWidget(m_poolMemberRemoveButton);
        poolMemberToolbar->addWidget(m_poolMemberEditChanceButton);
        poolMemberToolbar->addStretch(1);
        rightBox->addLayout(poolMemberToolbar);
        m_poolMembers = new QTableWidget(this);
        m_poolMembers->setColumnCount(4);
        m_poolMembers->setHorizontalHeaderLabels(
            { tr("kind"), tr("guid"), tr("chance%"), tr("description") });
        m_poolMembers->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
        m_poolMembers->verticalHeader()->setVisible(false);
        m_poolMembers->setSelectionBehavior(QAbstractItemView::SelectRows);
        m_poolMembers->setSelectionMode(QAbstractItemView::SingleSelection);
        // chance column is editable; kind/guid/description stay read-only.
        m_poolMembers->setEditTriggers(QAbstractItemView::DoubleClicked
                                     | QAbstractItemView::EditKeyPressed);
        rightBox->addWidget(m_poolMembers, 1);
        // Header edit: description + Save (entry + max_limit immutable).
        m_poolDescEdit = new QLineEdit(this); m_poolDescEdit->setEnabled(false);
        m_poolSaveButton = new QPushButton(tr("Save description"), this);
        m_poolSaveButton->setEnabled(false);
        auto* descRow = new QHBoxLayout;
        descRow->addWidget(new QLabel(tr("description:")));
        descRow->addWidget(m_poolDescEdit, 1);
        descRow->addWidget(m_poolSaveButton);
        rightBox->addLayout(descRow);
        m_poolMembers->setContextMenuPolicy(Qt::CustomContextMenu);
        row->addLayout(rightBox, 1);
        m_tabs->addTab(page, tr("Pools"));
    }

    auto* outer = new QVBoxLayout(this);
    outer->addWidget(m_tabs, 1);
    outer->addWidget(m_statusLabel);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    outer->addWidget(buttons);

    connect(m_tabs,        &QTabWidget::currentChanged, this, &GroupsPoolsDialog::onTabChanged);
    connect(m_groupFilter, &QLineEdit::textChanged,     this, &GroupsPoolsDialog::onGroupFilter);
    connect(m_poolFilter,  &QLineEdit::textChanged,     this, &GroupsPoolsDialog::onPoolFilter);
    connect(m_groupList->selectionModel(), &QItemSelectionModel::currentRowChanged,
            this, &GroupsPoolsDialog::onGroupRowChanged);
    connect(m_poolList->selectionModel(), &QItemSelectionModel::currentRowChanged,
            this, &GroupsPoolsDialog::onPoolRowChanged);
    connect(m_highlightButton, &QPushButton::clicked, this, &GroupsPoolsDialog::onHighlightClicked);
    connect(m_poolMembers, &QTableWidget::cellChanged,
            this, &GroupsPoolsDialog::onPoolMemberCellChanged);
    connect(m_groupMembers, &QWidget::customContextMenuRequested,
            this, &GroupsPoolsDialog::onGroupMembersContextMenu);
    connect(m_poolMembers, &QWidget::customContextMenuRequested,
            this, &GroupsPoolsDialog::onPoolMembersContextMenu);
    connect(m_newGroupButton, &QPushButton::clicked, this, &GroupsPoolsDialog::onCreateGroup);
    connect(m_newPoolButton,  &QPushButton::clicked, this, &GroupsPoolsDialog::onCreatePool);
    connect(m_groupSaveButton, &QPushButton::clicked, this, &GroupsPoolsDialog::onSaveGroupHeader);
    connect(m_poolSaveButton,  &QPushButton::clicked, this, &GroupsPoolsDialog::onSavePoolHeader);
    connect(m_poolInsertButton, &QPushButton::clicked, this, &GroupsPoolsDialog::onInsertPoolTemplate);
    connect(m_poolEditButton,   &QPushButton::clicked, this, &GroupsPoolsDialog::onEditPoolTemplate);
    connect(m_poolDeleteButton, &QPushButton::clicked, this, &GroupsPoolsDialog::onDeletePoolTemplate);
    connect(m_groupInsertButton, &QPushButton::clicked, this, &GroupsPoolsDialog::onInsertSpawnGroupTemplate);
    connect(m_groupEditButton,   &QPushButton::clicked, this, &GroupsPoolsDialog::onEditSpawnGroupTemplate);
    connect(m_groupDeleteButton, &QPushButton::clicked, this, &GroupsPoolsDialog::onDeleteSpawnGroupTemplate);
    connect(m_memberAddButton,    &QPushButton::clicked, this, &GroupsPoolsDialog::onAddSpawnGroupMember);
    connect(m_memberRemoveButton, &QPushButton::clicked, this, &GroupsPoolsDialog::onRemoveSpawnGroupMember);
    connect(m_poolMemberAddCreatureButton,   &QPushButton::clicked, this, &GroupsPoolsDialog::onAddPoolCreature);
    connect(m_poolMemberAddGameobjectButton, &QPushButton::clicked, this, &GroupsPoolsDialog::onAddPoolGameobject);
    connect(m_poolMemberRemoveButton,        &QPushButton::clicked, this, &GroupsPoolsDialog::onRemovePoolMember);
    connect(m_poolMemberEditChanceButton,    &QPushButton::clicked, this, &GroupsPoolsDialog::onEditPoolMemberChance);

    QTimer::singleShot(0, this, [this] { loadGroups(); loadPools(); });
}

void GroupsPoolsDialog::onTabChanged(int /*idx*/) {}
void GroupsPoolsDialog::onGroupFilter(QString const& text) { m_groupProxy->setFilterFixedString(text); }
void GroupsPoolsDialog::onPoolFilter (QString const& text) { m_poolProxy ->setFilterFixedString(text); }

void GroupsPoolsDialog::loadGroups()
{
    if (!m_dbClient || !m_dbClient->isConnected()) return;
    QApplication::setOverrideCursor(Qt::WaitCursor);

    char sql[512];
    std::snprintf(sql, sizeof(sql),
        "SELECT sgt.groupId, sgt.groupName, "
        "       COALESCE((SELECT COUNT(*) FROM %s.spawn_group sg WHERE sg.groupId = sgt.groupId), 0) AS members "
        "FROM %s.spawn_group_template sgt ORDER BY sgt.groupId",
        m_worldDb.toStdString().c_str(), m_worldDb.toStdString().c_str());
    db::QueryResult res;
    auto err = m_dbClient->query(sql, res);
    QApplication::restoreOverrideCursor();
    if (!err.ok())
    {
        m_statusLabel->setText(tr("group query failed: %1")
            .arg(QString::fromStdString(err.message)));
        return;
    }
    m_groupModel->setRowCount(int(res.rowCount()));
    for (size_t r = 0; r < res.rowCount(); ++r)
    {
        uint64_t const gid    = res.asUInt64(r, 0).value_or(0);
        QString  const name   = QString::fromStdString(res.cell(r, 1));
        uint64_t const nMem   = res.asUInt64(r, 2).value_or(0);
        auto* idItem = new QStandardItem(QString::number(gid));
        idItem->setData(qulonglong(gid), Qt::UserRole);
        m_groupModel->setItem(int(r), 0, idItem);
        m_groupModel->setItem(int(r), 1, new QStandardItem(name));
        m_groupModel->setItem(int(r), 2, new QStandardItem(QString::number(nMem)));
    }
    m_statusLabel->setText(tr("groups=%1  pools=%2")
        .arg(res.rowCount()).arg(m_poolModel->rowCount()));
}

void GroupsPoolsDialog::loadPools()
{
    if (!m_dbClient || !m_dbClient->isConnected()) return;
    char sql[512];
    std::snprintf(sql, sizeof(sql),
        "SELECT pt.entry, pt.max_limit, COALESCE(pt.description, ''), "
        "       COALESCE((SELECT COUNT(*) FROM %s.pool_members pm WHERE pm.poolSpawnId = pt.entry), 0) "
        "FROM %s.pool_template pt ORDER BY pt.entry",
        m_worldDb.toStdString().c_str(), m_worldDb.toStdString().c_str());
    db::QueryResult res;
    auto err = m_dbClient->query(sql, res);
    if (!err.ok())
    {
        m_statusLabel->setText(tr("pool query failed: %1")
            .arg(QString::fromStdString(err.message)));
        return;
    }
    m_poolModel->setRowCount(int(res.rowCount()));
    for (size_t r = 0; r < res.rowCount(); ++r)
    {
        uint64_t const entry = res.asUInt64(r, 0).value_or(0);
        uint64_t const mx    = res.asUInt64(r, 1).value_or(0);
        QString  const desc  = QString::fromStdString(res.cell(r, 2));
        uint64_t const nMem  = res.asUInt64(r, 3).value_or(0);
        auto* eItem = new QStandardItem(QString::number(entry));
        eItem->setData(qulonglong(entry), Qt::UserRole);
        m_poolModel->setItem(int(r), 0, eItem);
        m_poolModel->setItem(int(r), 1, new QStandardItem(QString::number(mx)));
        m_poolModel->setItem(int(r), 2, new QStandardItem(desc));
        m_poolModel->setItem(int(r), 3, new QStandardItem(QString::number(nMem)));
    }
    m_statusLabel->setText(tr("groups=%1  pools=%2")
        .arg(m_groupModel->rowCount()).arg(res.rowCount()));
}

void GroupsPoolsDialog::onGroupRowChanged(QModelIndex const& cur, QModelIndex const&)
{
    if (!cur.isValid()) return;
    QModelIndex const src = m_groupProxy->mapToSource(cur);
    uint64_t const gid = m_groupModel->item(src.row(), 0)->data(Qt::UserRole).toULongLong();
    m_selectedGroupId = uint32_t(gid);
    loadGroupMembers(m_selectedGroupId);
    m_highlightButton->setEnabled(true);

    // Populate header editors.
    m_groupNameEdit->setText(m_groupModel->item(src.row(), 1)->text());
    // Re-fetch flags (the listing doesn't store them).
    char fsql[200];
    std::snprintf(fsql, sizeof(fsql),
        "SELECT groupFlags FROM %s.spawn_group_template WHERE groupId = %u",
        m_worldDb.toStdString().c_str(), m_selectedGroupId);
    db::QueryResult fres;
    if (m_dbClient->query(fsql, fres).ok() && fres.rowCount() > 0)
        m_groupFlagsEdit->setText(QString::number(fres.asUInt64(0, 0).value_or(0)));
    m_groupNameEdit->setEnabled(true);
    m_groupFlagsEdit->setEnabled(true);
    m_groupSaveButton->setEnabled(true);
    if (m_groupEditButton)   m_groupEditButton->setEnabled(true);
    if (m_groupDeleteButton) m_groupDeleteButton->setEnabled(true);
    if (m_memberAddButton)    m_memberAddButton->setEnabled(true);
    if (m_memberRemoveButton) m_memberRemoveButton->setEnabled(m_groupMembers->rowCount() > 0);
}

void GroupsPoolsDialog::loadGroupMembers(uint32_t groupId)
{
    if (!m_dbClient || !m_dbClient->isConnected()) return;
    char sql[256];
    std::snprintf(sql, sizeof(sql),
        "SELECT spawnType, spawnId FROM %s.spawn_group "
        "WHERE groupId = %u ORDER BY spawnType, spawnId",
        m_worldDb.toStdString().c_str(), groupId);
    db::QueryResult res;
    if (!m_dbClient->query(sql, res).ok())
        return;
    m_groupMembers->setRowCount(int(res.rowCount()));
    m_lastMemberGuids.clear();
    for (size_t r = 0; r < res.rowCount(); ++r)
    {
        uint64_t const stype  = res.asUInt64(r, 0).value_or(0);
        int64_t  const sid    = res.asInt64 (r, 1).value_or(0);
        m_groupMembers->setItem(int(r), 0, new QTableWidgetItem(QString::number(stype)));
        m_groupMembers->setItem(int(r), 1, new QTableWidgetItem(QString::number(sid)));
        if (stype == 1)  // 1 = creature on this server
            m_lastMemberGuids.append(qlonglong(sid));
    }
    if (m_memberRemoveButton)
        m_memberRemoveButton->setEnabled(m_groupMembers->rowCount() > 0);
}

void GroupsPoolsDialog::onPoolRowChanged(QModelIndex const& cur, QModelIndex const&)
{
    if (!cur.isValid()) return;
    QModelIndex const src = m_poolProxy->mapToSource(cur);
    uint64_t const entry = m_poolModel->item(src.row(), 0)->data(Qt::UserRole).toULongLong();
    m_selectedPoolEntry = uint32_t(entry);
    loadPoolMembers(m_selectedPoolEntry);
    m_poolDescEdit->setText(m_poolModel->item(src.row(), 2)->text());
    m_poolDescEdit->setEnabled(true);
    m_poolSaveButton->setEnabled(true);
    if (m_poolEditButton)   m_poolEditButton->setEnabled(true);
    if (m_poolDeleteButton) m_poolDeleteButton->setEnabled(true);
    if (m_poolMemberAddCreatureButton)   m_poolMemberAddCreatureButton->setEnabled(true);
    if (m_poolMemberAddGameobjectButton) m_poolMemberAddGameobjectButton->setEnabled(true);
    // Remove + Edit chance are enabled by loadPoolMembers based on row count.
}

void GroupsPoolsDialog::onPoolMemberCellChanged(int row, int col)
{
    if (m_suppressCellChange) return;
    if (col != 2) return;   // only chance is editable
    if (!m_dbClient || !m_dbClient->isConnected() || m_selectedPoolEntry == 0)
        return;
    QString const newValue = m_poolMembers->item(row, col)->text();
    bool ok = false;
    double const chance = newValue.toDouble(&ok);
    if (!ok)
    {
        QMessageBox::warning(this, tr("Bad value"),
            tr("'%1' is not a valid number.").arg(newValue));
        return;
    }
    int const type    = int(m_poolMembers->item(row, 0)->data(Qt::UserRole).toULongLong());
    int64_t const sid = m_poolMembers->item(row, 1)->text().toLongLong();
    QString const sql = QString(
        "UPDATE %1.pool_members SET chance = %2 "
        "WHERE poolSpawnId = %3 AND type = %4 AND spawnId = %5;")
        .arg(m_worldDb).arg(chance, 0, 'f', 4)
        .arg(m_selectedPoolEntry).arg(type).arg(sid);

    ConfirmSqlDialog confirm(m_dbClient,
        tr("Update pool_members.chance for pool %1 row %2:")
            .arg(m_selectedPoolEntry).arg(row + 1),
        sql, this);
    if (confirm.exec() != QDialog::Accepted)
    {
        // Operator cancelled - reload the row to restore the old value.
        loadPoolMembers(m_selectedPoolEntry);
        return;
    }
    m_statusLabel->setText(tr("pool_members.chance updated (affected=%1)")
        .arg(qulonglong(confirm.affectedRows())));
}

void GroupsPoolsDialog::loadPoolMembers(uint32_t poolEntry)
{
    if (!m_dbClient || !m_dbClient->isConnected()) return;
    // UNION of pool_creature + pool_gameobject views over pool_members.  The
    // runtime schema is the unified pool_members table; we render type=0 as
    // "creature" and type=1 as "gameobject" for operator clarity.
    char sql[256];
    std::snprintf(sql, sizeof(sql),
        "SELECT type, spawnId, chance, COALESCE(description, '') "
        "FROM %s.pool_members WHERE poolSpawnId = %u "
        "ORDER BY type, spawnId",
        m_worldDb.toStdString().c_str(), poolEntry);
    db::QueryResult res;
    if (!m_dbClient->query(sql, res).ok())
        return;
    m_suppressCellChange = true;
    m_poolMembers->setRowCount(int(res.rowCount()));
    for (size_t r = 0; r < res.rowCount(); ++r)
    {
        uint64_t const type = res.asUInt64(r, 0).value_or(0);
        QString const kindLabel = (type == 0) ? tr("creature")
                                : (type == 1) ? tr("gameobject")
                                              : QString::number(type);
        auto* kindItem = new QTableWidgetItem(kindLabel);
        kindItem->setData(Qt::UserRole, qulonglong(type));
        auto* sidItem  = new QTableWidgetItem(QString::number(res.asInt64(r, 1).value_or(0)));
        auto* chanceItem = new QTableWidgetItem(QString::number(res.asDouble(r, 2).value_or(0.0), 'f', 2));
        auto* descItem = new QTableWidgetItem(QString::fromStdString(res.cell(r, 3)));
        kindItem->setFlags(kindItem->flags() & ~Qt::ItemIsEditable);
        sidItem ->setFlags(sidItem ->flags() & ~Qt::ItemIsEditable);
        descItem->setFlags(descItem->flags() & ~Qt::ItemIsEditable);
        // chanceItem stays editable.
        m_poolMembers->setItem(int(r), 0, kindItem);
        m_poolMembers->setItem(int(r), 1, sidItem);
        m_poolMembers->setItem(int(r), 2, chanceItem);
        m_poolMembers->setItem(int(r), 3, descItem);
    }
    m_suppressCellChange = false;
    bool const haveRows = m_poolMembers->rowCount() > 0;
    if (m_poolMemberRemoveButton)     m_poolMemberRemoveButton->setEnabled(haveRows);
    if (m_poolMemberEditChanceButton) m_poolMemberEditChanceButton->setEnabled(haveRows);
}

void GroupsPoolsDialog::onHighlightClicked()
{
    if (!m_lastMemberGuids.isEmpty())
        emit highlightSpawnGuids(m_lastMemberGuids);
}

void GroupsPoolsDialog::onGroupMembersContextMenu(QPoint const& pos)
{
    if (m_selectedGroupId == 0) return;
    QTableWidgetItem* const it = m_groupMembers->itemAt(pos);
    if (!it) return;
    int const row = it->row();
    int const stype = m_groupMembers->item(row, 0)->text().toInt();
    int64_t const sid = m_groupMembers->item(row, 1)->text().toLongLong();

    QMenu menu(this);
    QAction* removeAction = menu.addAction(tr("Remove member from group"));
    QAction* picked = menu.exec(m_groupMembers->viewport()->mapToGlobal(pos));
    if (picked != removeAction) return;

    QString const sql = QString(
        "DELETE FROM %1.spawn_group "
        "WHERE groupId = %2 AND spawnType = %3 AND spawnId = %4;")
        .arg(m_worldDb).arg(m_selectedGroupId).arg(stype).arg(sid);
    ConfirmSqlDialog confirm(m_dbClient,
        tr("Remove (spawnType=%1, spawnId=%2) from group %3:").arg(stype).arg(sid).arg(m_selectedGroupId),
        sql, this);
    if (confirm.exec() == QDialog::Accepted)
    {
        loadGroupMembers(m_selectedGroupId);
        m_statusLabel->setText(tr("removed (affected=%1)").arg(qulonglong(confirm.affectedRows())));
    }
}

void GroupsPoolsDialog::onPoolMembersContextMenu(QPoint const& pos)
{
    if (m_selectedPoolEntry == 0) return;
    QTableWidgetItem* const it = m_poolMembers->itemAt(pos);
    if (!it) return;
    int const row = it->row();
    int const type = int(m_poolMembers->item(row, 0)->data(Qt::UserRole).toULongLong());
    int64_t const sid = m_poolMembers->item(row, 1)->text().toLongLong();

    QMenu menu(this);
    QAction* removeAction = menu.addAction(tr("Remove from pool"));
    QAction* picked = menu.exec(m_poolMembers->viewport()->mapToGlobal(pos));
    if (picked != removeAction) return;

    QString const sql = QString(
        "DELETE FROM %1.pool_members "
        "WHERE poolSpawnId = %2 AND type = %3 AND spawnId = %4;")
        .arg(m_worldDb).arg(m_selectedPoolEntry).arg(type).arg(sid);
    ConfirmSqlDialog confirm(m_dbClient,
        tr("Remove (type=%1, spawnId=%2) from pool %3:").arg(type).arg(sid).arg(m_selectedPoolEntry),
        sql, this);
    if (confirm.exec() == QDialog::Accepted)
    {
        loadPoolMembers(m_selectedPoolEntry);
        m_statusLabel->setText(tr("removed (affected=%1)").arg(qulonglong(confirm.affectedRows())));
    }
}

void GroupsPoolsDialog::onCreateGroup()
{
    if (!m_dbClient || !m_dbClient->isConnected()) return;
    // Reserve next groupId.
    char maxSql[256];
    std::snprintf(maxSql, sizeof(maxSql),
        "SELECT COALESCE(MAX(groupId), 0)+1 FROM %s.spawn_group_template",
        m_worldDb.toStdString().c_str());
    db::QueryResult mr;
    (void)m_dbClient->query(maxSql, mr);
    uint64_t const newId = mr.rowCount() > 0 ? mr.asUInt64(0, 0).value_or(0) : 0;
    if (newId == 0) { QMessageBox::warning(this, tr("Failed"), tr("Could not reserve groupId.")); return; }

    bool ok = false;
    QString const name = QInputDialog::getText(this, tr("New group"),
        tr("groupName (groupId will be %1):").arg(qulonglong(newId)),
        QLineEdit::Normal, QString{}, &ok);
    if (!ok || name.isEmpty()) return;

    QString const escaped = QString::fromStdString(m_dbClient->escapeString(name.toStdString()));
    QString const sql = QString(
        "INSERT INTO %1.spawn_group_template (groupId, groupName, groupFlags) "
        "VALUES (%2, '%3', 0);")
        .arg(m_worldDb).arg(newId).arg(escaped);
    ConfirmSqlDialog confirm(m_dbClient,
        tr("Create new spawn group %1 named \"%2\":").arg(qulonglong(newId)).arg(name),
        sql, this);
    if (confirm.exec() == QDialog::Accepted)
    {
        loadGroups();
        m_statusLabel->setText(tr("group %1 created").arg(qulonglong(newId)));
    }
}

void GroupsPoolsDialog::onCreatePool()
{
    if (!m_dbClient || !m_dbClient->isConnected()) return;
    char maxSql[256];
    std::snprintf(maxSql, sizeof(maxSql),
        "SELECT COALESCE(MAX(entry), 0)+1 FROM %s.pool_template",
        m_worldDb.toStdString().c_str());
    db::QueryResult mr;
    (void)m_dbClient->query(maxSql, mr);
    uint64_t const newId = mr.rowCount() > 0 ? mr.asUInt64(0, 0).value_or(0) : 0;
    if (newId == 0) { QMessageBox::warning(this, tr("Failed"), tr("Could not reserve pool entry.")); return; }

    bool ok = false;
    int const maxLimit = QInputDialog::getInt(this, tr("New pool"),
        tr("max_limit (entry will be %1):").arg(qulonglong(newId)),
        1, 0, 1000000, 1, &ok);
    if (!ok) return;
    QString const desc = QInputDialog::getText(this, tr("New pool"),
        tr("description (optional):"), QLineEdit::Normal, QString{}, &ok);
    if (!ok) return;

    QString const escaped = QString::fromStdString(m_dbClient->escapeString(desc.toStdString()));
    QString const sql = QString(
        "INSERT INTO %1.pool_template (entry, max_limit, description) "
        "VALUES (%2, %3, '%4');")
        .arg(m_worldDb).arg(newId).arg(maxLimit).arg(escaped);
    ConfirmSqlDialog confirm(m_dbClient,
        tr("Create new pool %1 (max_limit=%2):").arg(qulonglong(newId)).arg(maxLimit),
        sql, this);
    if (confirm.exec() == QDialog::Accepted)
    {
        loadPools();
        m_statusLabel->setText(tr("pool %1 created").arg(qulonglong(newId)));
    }
}

void GroupsPoolsDialog::onSaveGroupHeader()
{
    if (m_selectedGroupId == 0) return;
    QString const name  = m_groupNameEdit->text();
    bool ok = false;
    uint32_t const flags = m_groupFlagsEdit->text().toUInt(&ok);
    if (!ok)
    {
        QMessageBox::warning(this, tr("Bad flags"),
            tr("'%1' is not a valid integer.").arg(m_groupFlagsEdit->text()));
        return;
    }
    QString const escaped = QString::fromStdString(m_dbClient->escapeString(name.toStdString()));
    QString const sql = QString(
        "UPDATE %1.spawn_group_template SET groupName = '%2', groupFlags = %3 "
        "WHERE groupId = %4;")
        .arg(m_worldDb).arg(escaped).arg(flags).arg(m_selectedGroupId);
    ConfirmSqlDialog confirm(m_dbClient,
        tr("Save group %1 header:").arg(m_selectedGroupId), sql, this);
    if (confirm.exec() == QDialog::Accepted)
    {
        loadGroups();
        m_statusLabel->setText(tr("group %1 header saved").arg(m_selectedGroupId));
    }
}

void GroupsPoolsDialog::onSavePoolHeader()
{
    if (m_selectedPoolEntry == 0) return;
    QString const desc = m_poolDescEdit->text();
    QString const escaped = QString::fromStdString(m_dbClient->escapeString(desc.toStdString()));
    QString const sql = QString(
        "UPDATE %1.pool_template SET description = '%2' WHERE entry = %3;")
        .arg(m_worldDb).arg(escaped).arg(m_selectedPoolEntry);
    ConfirmSqlDialog confirm(m_dbClient,
        tr("Save pool %1 description:").arg(m_selectedPoolEntry), sql, this);
    if (confirm.exec() == QDialog::Accepted)
    {
        loadPools();
        m_statusLabel->setText(tr("pool %1 description saved").arg(m_selectedPoolEntry));
    }
}

// Insert a new pool_template row.  Reserves entry via COALESCE(MAX)+1,
// opens the modal editor, then wraps the INSERT in a START TRANSACTION /
// COMMIT pair with ROLLBACK on any error.
void GroupsPoolsDialog::onInsertPoolTemplate()
{
    if (!m_dbClient || !m_dbClient->isConnected()) return;

    char maxSql[256];
    std::snprintf(maxSql, sizeof(maxSql),
        "SELECT COALESCE(MAX(entry), 0)+1 FROM %s.pool_template",
        m_worldDb.toStdString().c_str());
    db::QueryResult mr;
    auto err = m_dbClient->query(maxSql, mr);
    if (!err.ok())
    {
        QMessageBox::warning(this, tr("Insert failed"),
            tr("entry reservation failed: %1").arg(QString::fromStdString(err.message)));
        return;
    }
    uint32_t const nextEntry = mr.rowCount() > 0 ? uint32_t(mr.asUInt64(0, 0).value_or(0)) : 0;
    if (nextEntry == 0)
    {
        QMessageBox::warning(this, tr("Insert failed"), tr("Could not reserve pool entry."));
        return;
    }

    PoolTemplateEditDialog dlg(this);
    dlg.setEntry(nextEntry);
    dlg.setMaxLimit(0);
    dlg.setDescription(QString{});
    dlg.setKeyEditable(true);
    if (dlg.exec() != QDialog::Accepted) return;

    QString const desc = dlg.description();
    QString const escaped = QString::fromStdString(m_dbClient->escapeString(desc.toStdString()));
    QString const sql = QString(
        "INSERT INTO %1.pool_template (entry, max_limit, description) "
        "VALUES (%2, %3, '%4');")
        .arg(m_worldDb).arg(dlg.entry()).arg(dlg.maxLimit()).arg(escaped);

    err = m_dbClient->exec("START TRANSACTION");
    if (!err.ok())
    {
        QMessageBox::warning(this, tr("Insert failed"),
            tr("BEGIN failed: %1").arg(QString::fromStdString(err.message)));
        return;
    }
    uint64_t affected = 0;
    err = m_dbClient->exec(sql.toStdString(), &affected);
    if (!err.ok())
    {
        (void)m_dbClient->exec("ROLLBACK");
        QMessageBox::warning(this, tr("Insert failed"),
            tr("INSERT failed: %1").arg(QString::fromStdString(err.message)));
        return;
    }
    err = m_dbClient->exec("COMMIT");
    if (!err.ok())
    {
        (void)m_dbClient->exec("ROLLBACK");
        QMessageBox::warning(this, tr("Insert failed"),
            tr("COMMIT failed: %1").arg(QString::fromStdString(err.message)));
        return;
    }

    loadPools();
    m_statusLabel->setText(tr("pool %1 inserted (affected=%2)")
        .arg(dlg.entry()).arg(qulonglong(affected)));
}

// Edit the selected pool_template row.  Opens the modal seeded with
// current values; entry is locked (re-key requires Delete+Insert).
void GroupsPoolsDialog::onEditPoolTemplate()
{
    if (!m_dbClient || !m_dbClient->isConnected() || m_selectedPoolEntry == 0) return;

    char selSql[256];
    std::snprintf(selSql, sizeof(selSql),
        "SELECT entry, max_limit, COALESCE(description, '') "
        "FROM %s.pool_template WHERE entry = %u",
        m_worldDb.toStdString().c_str(), m_selectedPoolEntry);
    db::QueryResult res;
    auto err = m_dbClient->query(selSql, res);
    if (!err.ok() || res.rowCount() == 0)
    {
        QMessageBox::warning(this, tr("Edit failed"),
            tr("Could not load pool %1.").arg(m_selectedPoolEntry));
        return;
    }
    uint32_t const curEntry = uint32_t(res.asUInt64(0, 0).value_or(0));
    uint32_t const curMax   = uint32_t(res.asUInt64(0, 1).value_or(0));
    QString  const curDesc  = QString::fromStdString(res.cell(0, 2));

    PoolTemplateEditDialog dlg(this);
    dlg.setEntry(curEntry);
    dlg.setMaxLimit(curMax);
    dlg.setDescription(curDesc);
    dlg.setKeyEditable(false); // composite-PK identity is locked on Edit.
    if (dlg.exec() != QDialog::Accepted) return;

    QString const escaped = QString::fromStdString(m_dbClient->escapeString(dlg.description().toStdString()));
    QString const sql = QString(
        "UPDATE %1.pool_template SET max_limit = %2, description = '%3' "
        "WHERE entry = %4;")
        .arg(m_worldDb).arg(dlg.maxLimit()).arg(escaped).arg(curEntry);

    err = m_dbClient->exec("START TRANSACTION");
    if (!err.ok())
    {
        QMessageBox::warning(this, tr("Edit failed"),
            tr("BEGIN failed: %1").arg(QString::fromStdString(err.message)));
        return;
    }
    uint64_t affected = 0;
    err = m_dbClient->exec(sql.toStdString(), &affected);
    if (!err.ok())
    {
        (void)m_dbClient->exec("ROLLBACK");
        QMessageBox::warning(this, tr("Edit failed"),
            tr("UPDATE failed: %1").arg(QString::fromStdString(err.message)));
        return;
    }
    err = m_dbClient->exec("COMMIT");
    if (!err.ok())
    {
        (void)m_dbClient->exec("ROLLBACK");
        QMessageBox::warning(this, tr("Edit failed"),
            tr("COMMIT failed: %1").arg(QString::fromStdString(err.message)));
        return;
    }

    loadPools();
    m_statusLabel->setText(tr("pool %1 updated (affected=%2)")
        .arg(curEntry).arg(qulonglong(affected)));
}

// Delete the selected pool_template row.  Pre-counts dependent rows in
// pool_creature / pool_gameobject / pool_pool and surfaces them in the
// confirmation prompt; the operator decides whether to proceed.
void GroupsPoolsDialog::onDeletePoolTemplate()
{
    if (!m_dbClient || !m_dbClient->isConnected() || m_selectedPoolEntry == 0) return;

    auto countDeps = [&](char const* table, char const* col) -> uint64_t {
        char sql[256];
        std::snprintf(sql, sizeof(sql),
            "SELECT COUNT(*) FROM %s.%s WHERE %s = %u",
            m_worldDb.toStdString().c_str(), table, col, m_selectedPoolEntry);
        db::QueryResult r;
        if (!m_dbClient->query(sql, r).ok() || r.rowCount() == 0) return 0;
        return r.asUInt64(0, 0).value_or(0);
    };

    uint64_t const depCreature = countDeps("pool_creature",   "pool_entry");
    uint64_t const depGo       = countDeps("pool_gameobject", "pool_entry");
    uint64_t const depPool     = countDeps("pool_pool",       "mother_pool");
    uint64_t const depTotal    = depCreature + depGo + depPool;

    QString prompt = tr("Delete pool_template entry %1?").arg(m_selectedPoolEntry);
    if (depTotal > 0)
    {
        prompt += tr("\n\nWARNING: dependent rows exist:\n"
                     "  pool_creature:   %1\n"
                     "  pool_gameobject: %2\n"
                     "  pool_pool:       %3\n\n"
                     "Deleting the pool will leave these rows orphaned. Proceed?")
                     .arg(qulonglong(depCreature))
                     .arg(qulonglong(depGo))
                     .arg(qulonglong(depPool));
    }
    auto const reply = QMessageBox::question(this, tr("Delete pool"), prompt,
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (reply != QMessageBox::Yes) return;

    QString const sql = QString("DELETE FROM %1.pool_template WHERE entry = %2;")
        .arg(m_worldDb).arg(m_selectedPoolEntry);

    auto err = m_dbClient->exec("START TRANSACTION");
    if (!err.ok())
    {
        QMessageBox::warning(this, tr("Delete failed"),
            tr("BEGIN failed: %1").arg(QString::fromStdString(err.message)));
        return;
    }
    uint64_t affected = 0;
    err = m_dbClient->exec(sql.toStdString(), &affected);
    if (!err.ok())
    {
        (void)m_dbClient->exec("ROLLBACK");
        QMessageBox::warning(this, tr("Delete failed"),
            tr("DELETE failed: %1").arg(QString::fromStdString(err.message)));
        return;
    }
    err = m_dbClient->exec("COMMIT");
    if (!err.ok())
    {
        (void)m_dbClient->exec("ROLLBACK");
        QMessageBox::warning(this, tr("Delete failed"),
            tr("COMMIT failed: %1").arg(QString::fromStdString(err.message)));
        return;
    }

    uint32_t const deletedEntry = m_selectedPoolEntry;
    m_selectedPoolEntry = 0;
    m_poolDescEdit->clear();
    m_poolDescEdit->setEnabled(false);
    m_poolSaveButton->setEnabled(false);
    m_poolEditButton->setEnabled(false);
    m_poolDeleteButton->setEnabled(false);
    if (m_poolMemberAddCreatureButton)   m_poolMemberAddCreatureButton->setEnabled(false);
    if (m_poolMemberAddGameobjectButton) m_poolMemberAddGameobjectButton->setEnabled(false);
    if (m_poolMemberRemoveButton)        m_poolMemberRemoveButton->setEnabled(false);
    if (m_poolMemberEditChanceButton)    m_poolMemberEditChanceButton->setEnabled(false);
    m_poolMembers->setRowCount(0);
    loadPools();
    m_statusLabel->setText(tr("pool %1 deleted (affected=%2)")
        .arg(deletedEntry).arg(qulonglong(affected)));
}

// Insert a new spawn_group_template row.  Reserves groupId via
// COALESCE(MAX)+1, opens the modal editor, then wraps the INSERT in a
// START TRANSACTION / COMMIT pair with ROLLBACK on any error.
void GroupsPoolsDialog::onInsertSpawnGroupTemplate()
{
    if (!m_dbClient || !m_dbClient->isConnected()) return;

    char maxSql[256];
    std::snprintf(maxSql, sizeof(maxSql),
        "SELECT COALESCE(MAX(groupId), 0)+1 FROM %s.spawn_group_template",
        m_worldDb.toStdString().c_str());
    db::QueryResult mr;
    auto err = m_dbClient->query(maxSql, mr);
    if (!err.ok())
    {
        QMessageBox::warning(this, tr("Insert failed"),
            tr("groupId reservation failed: %1").arg(QString::fromStdString(err.message)));
        return;
    }
    uint32_t const nextId = mr.rowCount() > 0 ? uint32_t(mr.asUInt64(0, 0).value_or(0)) : 0;
    if (nextId == 0)
    {
        QMessageBox::warning(this, tr("Insert failed"), tr("Could not reserve groupId."));
        return;
    }

    SpawnGroupTemplateEditDialog dlg(this);
    dlg.setGroupId(nextId);
    dlg.setGroupName(QString{});
    dlg.setGroupFlags(0);
    dlg.setKeyEditable(true);
    if (dlg.exec() != QDialog::Accepted) return;

    QString const name = dlg.groupName();
    QString const escaped = QString::fromStdString(m_dbClient->escapeString(name.toStdString()));
    QString const sql = QString(
        "INSERT INTO %1.spawn_group_template (groupId, groupName, groupFlags) "
        "VALUES (%2, '%3', %4);")
        .arg(m_worldDb).arg(dlg.groupId()).arg(escaped).arg(dlg.groupFlags());

    err = m_dbClient->exec("START TRANSACTION");
    if (!err.ok())
    {
        QMessageBox::warning(this, tr("Insert failed"),
            tr("BEGIN failed: %1").arg(QString::fromStdString(err.message)));
        return;
    }
    uint64_t affected = 0;
    err = m_dbClient->exec(sql.toStdString(), &affected);
    if (!err.ok())
    {
        (void)m_dbClient->exec("ROLLBACK");
        QMessageBox::warning(this, tr("Insert failed"),
            tr("INSERT failed: %1").arg(QString::fromStdString(err.message)));
        return;
    }
    err = m_dbClient->exec("COMMIT");
    if (!err.ok())
    {
        (void)m_dbClient->exec("ROLLBACK");
        QMessageBox::warning(this, tr("Insert failed"),
            tr("COMMIT failed: %1").arg(QString::fromStdString(err.message)));
        return;
    }

    loadGroups();
    m_statusLabel->setText(tr("group %1 inserted (affected=%2)")
        .arg(dlg.groupId()).arg(qulonglong(affected)));
}

// Edit the selected spawn_group_template row.  Opens the modal seeded
// with current values; groupId is locked (re-key requires Delete+Insert
// because spawn_group rows reference it).
void GroupsPoolsDialog::onEditSpawnGroupTemplate()
{
    if (!m_dbClient || !m_dbClient->isConnected() || m_selectedGroupId == 0) return;

    char selSql[256];
    std::snprintf(selSql, sizeof(selSql),
        "SELECT groupId, COALESCE(groupName, ''), groupFlags "
        "FROM %s.spawn_group_template WHERE groupId = %u",
        m_worldDb.toStdString().c_str(), m_selectedGroupId);
    db::QueryResult res;
    auto err = m_dbClient->query(selSql, res);
    if (!err.ok() || res.rowCount() == 0)
    {
        QMessageBox::warning(this, tr("Edit failed"),
            tr("Could not load group %1.").arg(m_selectedGroupId));
        return;
    }
    uint32_t const curId    = uint32_t(res.asUInt64(0, 0).value_or(0));
    QString  const curName  = QString::fromStdString(res.cell(0, 1));
    uint32_t const curFlags = uint32_t(res.asUInt64(0, 2).value_or(0));

    SpawnGroupTemplateEditDialog dlg(this);
    dlg.setGroupId(curId);
    dlg.setGroupName(curName);
    dlg.setGroupFlags(curFlags);
    dlg.setKeyEditable(false); // PK locked on Edit.
    if (dlg.exec() != QDialog::Accepted) return;

    QString const escaped = QString::fromStdString(m_dbClient->escapeString(dlg.groupName().toStdString()));
    QString const sql = QString(
        "UPDATE %1.spawn_group_template SET groupName = '%2', groupFlags = %3 "
        "WHERE groupId = %4;")
        .arg(m_worldDb).arg(escaped).arg(dlg.groupFlags()).arg(curId);

    err = m_dbClient->exec("START TRANSACTION");
    if (!err.ok())
    {
        QMessageBox::warning(this, tr("Edit failed"),
            tr("BEGIN failed: %1").arg(QString::fromStdString(err.message)));
        return;
    }
    uint64_t affected = 0;
    err = m_dbClient->exec(sql.toStdString(), &affected);
    if (!err.ok())
    {
        (void)m_dbClient->exec("ROLLBACK");
        QMessageBox::warning(this, tr("Edit failed"),
            tr("UPDATE failed: %1").arg(QString::fromStdString(err.message)));
        return;
    }
    err = m_dbClient->exec("COMMIT");
    if (!err.ok())
    {
        (void)m_dbClient->exec("ROLLBACK");
        QMessageBox::warning(this, tr("Edit failed"),
            tr("COMMIT failed: %1").arg(QString::fromStdString(err.message)));
        return;
    }

    loadGroups();
    m_statusLabel->setText(tr("group %1 updated (affected=%2)")
        .arg(curId).arg(qulonglong(affected)));
}

// Delete the selected spawn_group_template row.  Pre-counts dependent
// spawn_group rows and surfaces the count in the confirmation prompt; the
// operator decides whether to proceed.
void GroupsPoolsDialog::onDeleteSpawnGroupTemplate()
{
    if (!m_dbClient || !m_dbClient->isConnected() || m_selectedGroupId == 0) return;

    char depSql[256];
    std::snprintf(depSql, sizeof(depSql),
        "SELECT COUNT(*) FROM %s.spawn_group WHERE groupId = %u",
        m_worldDb.toStdString().c_str(), m_selectedGroupId);
    db::QueryResult depRes;
    uint64_t depCount = 0;
    if (m_dbClient->query(depSql, depRes).ok() && depRes.rowCount() > 0)
        depCount = depRes.asUInt64(0, 0).value_or(0);

    QString prompt = tr("Delete spawn_group_template groupId %1?").arg(m_selectedGroupId);
    if (depCount > 0)
    {
        prompt += tr("\n\nWARNING: dependent rows exist:\n"
                     "  spawn_group: %1\n\n"
                     "Deleting the group will leave these member rows orphaned. Proceed?")
                     .arg(qulonglong(depCount));
    }
    auto const reply = QMessageBox::question(this, tr("Delete group"), prompt,
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (reply != QMessageBox::Yes) return;

    QString const sql = QString("DELETE FROM %1.spawn_group_template WHERE groupId = %2;")
        .arg(m_worldDb).arg(m_selectedGroupId);

    auto err = m_dbClient->exec("START TRANSACTION");
    if (!err.ok())
    {
        QMessageBox::warning(this, tr("Delete failed"),
            tr("BEGIN failed: %1").arg(QString::fromStdString(err.message)));
        return;
    }
    uint64_t affected = 0;
    err = m_dbClient->exec(sql.toStdString(), &affected);
    if (!err.ok())
    {
        (void)m_dbClient->exec("ROLLBACK");
        QMessageBox::warning(this, tr("Delete failed"),
            tr("DELETE failed: %1").arg(QString::fromStdString(err.message)));
        return;
    }
    err = m_dbClient->exec("COMMIT");
    if (!err.ok())
    {
        (void)m_dbClient->exec("ROLLBACK");
        QMessageBox::warning(this, tr("Delete failed"),
            tr("COMMIT failed: %1").arg(QString::fromStdString(err.message)));
        return;
    }

    uint32_t const deletedId = m_selectedGroupId;
    m_selectedGroupId = 0;
    m_groupNameEdit->clear();
    m_groupFlagsEdit->clear();
    m_groupNameEdit->setEnabled(false);
    m_groupFlagsEdit->setEnabled(false);
    m_groupSaveButton->setEnabled(false);
    m_groupEditButton->setEnabled(false);
    m_groupDeleteButton->setEnabled(false);
    m_highlightButton->setEnabled(false);
    if (m_memberAddButton)    m_memberAddButton->setEnabled(false);
    if (m_memberRemoveButton) m_memberRemoveButton->setEnabled(false);
    m_groupMembers->setRowCount(0);
    loadGroups();
    m_statusLabel->setText(tr("group %1 deleted (affected=%2)")
        .arg(deletedId).arg(qulonglong(affected)));
}

// Add a single (spawnType, spawnId) row to spawn_group for the currently
// selected group.  Operator picks spawnType (0=creature, 1=GO) and types
// the spawnId.  Pre-checks: group selected, row not already a member.
// All DML wrapped in START TRANSACTION / COMMIT with ROLLBACK on error.
void GroupsPoolsDialog::onAddSpawnGroupMember()
{
    if (!m_dbClient || !m_dbClient->isConnected() || m_selectedGroupId == 0) return;

    QStringList types;
    types << tr("0 - creature") << tr("1 - gameobject");
    bool ok = false;
    QString const typeChoice = QInputDialog::getItem(this, tr("Add group member"),
        tr("spawnType:"), types, 0, false, &ok);
    if (!ok) return;
    int const spawnType = typeChoice.startsWith(QLatin1Char('1')) ? 1 : 0;

    qlonglong const spawnId = QInputDialog::getInt(this, tr("Add group member"),
        tr("spawnId (creature.guid for type=0, gameobject.guid for type=1):"),
        0, 0, 2147483647, 1, &ok);
    if (!ok) return;

    // Reject duplicates - spawn_group PK is (groupId, spawnType, spawnId).
    char dupSql[256];
    std::snprintf(dupSql, sizeof(dupSql),
        "SELECT COUNT(*) FROM %s.spawn_group "
        "WHERE groupId = %u AND spawnType = %d AND spawnId = %lld",
        m_worldDb.toStdString().c_str(), m_selectedGroupId, spawnType,
        (long long)spawnId);
    db::QueryResult dupRes;
    auto err = m_dbClient->query(dupSql, dupRes);
    if (!err.ok())
    {
        QMessageBox::warning(this, tr("Add failed"),
            tr("dup-check failed: %1").arg(QString::fromStdString(err.message)));
        return;
    }
    uint64_t const dupCount = dupRes.rowCount() > 0 ? dupRes.asUInt64(0, 0).value_or(0) : 0;
    if (dupCount > 0)
    {
        QMessageBox::warning(this, tr("Add failed"),
            tr("(groupId=%1, spawnType=%2, spawnId=%3) is already a member.")
                .arg(m_selectedGroupId).arg(spawnType).arg(spawnId));
        return;
    }

    QString const sql = QString(
        "INSERT INTO %1.spawn_group (groupId, spawnType, spawnId) "
        "VALUES (%2, %3, %4);")
        .arg(m_worldDb).arg(m_selectedGroupId).arg(spawnType).arg(spawnId);

    err = m_dbClient->exec("START TRANSACTION");
    if (!err.ok())
    {
        QMessageBox::warning(this, tr("Add failed"),
            tr("BEGIN failed: %1").arg(QString::fromStdString(err.message)));
        return;
    }
    uint64_t affected = 0;
    err = m_dbClient->exec(sql.toStdString(), &affected);
    if (!err.ok())
    {
        (void)m_dbClient->exec("ROLLBACK");
        QMessageBox::warning(this, tr("Add failed"),
            tr("INSERT failed: %1").arg(QString::fromStdString(err.message)));
        return;
    }
    err = m_dbClient->exec("COMMIT");
    if (!err.ok())
    {
        (void)m_dbClient->exec("ROLLBACK");
        QMessageBox::warning(this, tr("Add failed"),
            tr("COMMIT failed: %1").arg(QString::fromStdString(err.message)));
        return;
    }

    loadGroupMembers(m_selectedGroupId);
    loadGroups(); // refresh members count column
    m_statusLabel->setText(tr("group %1 +member (type=%2 id=%3) affected=%4")
        .arg(m_selectedGroupId).arg(spawnType).arg(spawnId).arg(qulonglong(affected)));
}

// Remove the currently selected (spawnType, spawnId) row from spawn_group.
// Wrapped in START TRANSACTION / COMMIT with ROLLBACK on error.
void GroupsPoolsDialog::onRemoveSpawnGroupMember()
{
    if (!m_dbClient || !m_dbClient->isConnected() || m_selectedGroupId == 0) return;
    int const row = m_groupMembers->currentRow();
    if (row < 0 || row >= m_groupMembers->rowCount())
    {
        QMessageBox::information(this, tr("No selection"),
            tr("Select a member row first."));
        return;
    }
    int const spawnType  = m_groupMembers->item(row, 0)->text().toInt();
    qlonglong const sid  = m_groupMembers->item(row, 1)->text().toLongLong();

    auto const reply = QMessageBox::question(this, tr("Remove member"),
        tr("Remove (spawnType=%1, spawnId=%2) from group %3?")
            .arg(spawnType).arg(sid).arg(m_selectedGroupId),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (reply != QMessageBox::Yes) return;

    QString const sql = QString(
        "DELETE FROM %1.spawn_group "
        "WHERE groupId = %2 AND spawnType = %3 AND spawnId = %4;")
        .arg(m_worldDb).arg(m_selectedGroupId).arg(spawnType).arg(sid);

    auto err = m_dbClient->exec("START TRANSACTION");
    if (!err.ok())
    {
        QMessageBox::warning(this, tr("Remove failed"),
            tr("BEGIN failed: %1").arg(QString::fromStdString(err.message)));
        return;
    }
    uint64_t affected = 0;
    err = m_dbClient->exec(sql.toStdString(), &affected);
    if (!err.ok())
    {
        (void)m_dbClient->exec("ROLLBACK");
        QMessageBox::warning(this, tr("Remove failed"),
            tr("DELETE failed: %1").arg(QString::fromStdString(err.message)));
        return;
    }
    err = m_dbClient->exec("COMMIT");
    if (!err.ok())
    {
        (void)m_dbClient->exec("ROLLBACK");
        QMessageBox::warning(this, tr("Remove failed"),
            tr("COMMIT failed: %1").arg(QString::fromStdString(err.message)));
        return;
    }

    loadGroupMembers(m_selectedGroupId);
    loadGroups(); // refresh members count column
    m_statusLabel->setText(tr("group %1 -member (type=%2 id=%3) affected=%4")
        .arg(m_selectedGroupId).arg(spawnType).arg(sid).arg(qulonglong(affected)));
}

// Shared INSERT helper for pool_members.  type=0 = creature (pool_creature view),
// type=1 = gameobject (pool_gameobject view).  Prompts for guid, chance, and
// description, rejects duplicates on the (poolSpawnId, type, spawnId) tuple,
// and wraps the INSERT in START TRANSACTION / COMMIT with ROLLBACK on error.
static void addPoolMemberWithType(GroupsPoolsDialog* dlg,
                                  db::MySqlClient* dbClient,
                                  QString const& worldDb,
                                  uint32_t selectedPoolEntry,
                                  int type,
                                  QString const& kindLabel,
                                  std::function<void()> const& refreshUi,
                                  QLabel* statusLabel)
{
    if (!dbClient || !dbClient->isConnected() || selectedPoolEntry == 0) return;

    bool ok = false;
    qlonglong const guid = QInputDialog::getInt(dlg,
        QObject::tr("Add %1 to pool").arg(kindLabel),
        QObject::tr("%1.guid:").arg(kindLabel),
        0, 0, 2147483647, 1, &ok);
    if (!ok) return;
    double const chance = QInputDialog::getDouble(dlg,
        QObject::tr("Add %1 to pool").arg(kindLabel),
        QObject::tr("chance (0..100):"),
        0.0, 0.0, 100.0, 4, &ok);
    if (!ok) return;
    QString const desc = QInputDialog::getText(dlg,
        QObject::tr("Add %1 to pool").arg(kindLabel),
        QObject::tr("description (optional):"),
        QLineEdit::Normal, QString{}, &ok);
    if (!ok) return;

    // Reject duplicates - pool_members PK is (type, spawnId).
    char dupSql[320];
    std::snprintf(dupSql, sizeof(dupSql),
        "SELECT COUNT(*) FROM %s.pool_members WHERE type = %d AND spawnId = %lld",
        worldDb.toStdString().c_str(), type, (long long)guid);
    db::QueryResult dupRes;
    auto err = dbClient->query(dupSql, dupRes);
    if (!err.ok())
    {
        QMessageBox::warning(dlg, QObject::tr("Add failed"),
            QObject::tr("dup-check failed: %1").arg(QString::fromStdString(err.message)));
        return;
    }
    uint64_t const dupCount = dupRes.rowCount() > 0 ? dupRes.asUInt64(0, 0).value_or(0) : 0;
    if (dupCount > 0)
    {
        QMessageBox::warning(dlg, QObject::tr("Add failed"),
            QObject::tr("(type=%1, spawnId=%2) already exists in pool_members.")
                .arg(type).arg(guid));
        return;
    }

    QString const escaped = QString::fromStdString(dbClient->escapeString(desc.toStdString()));
    QString const sql = QString(
        "INSERT INTO %1.pool_members (type, spawnId, poolSpawnId, chance, description) "
        "VALUES (%2, %3, %4, %5, '%6');")
        .arg(worldDb).arg(type).arg(guid).arg(selectedPoolEntry)
        .arg(chance, 0, 'f', 4).arg(escaped);

    err = dbClient->exec("START TRANSACTION");
    if (!err.ok())
    {
        QMessageBox::warning(dlg, QObject::tr("Add failed"),
            QObject::tr("BEGIN failed: %1").arg(QString::fromStdString(err.message)));
        return;
    }
    uint64_t affected = 0;
    err = dbClient->exec(sql.toStdString(), &affected);
    if (!err.ok())
    {
        (void)dbClient->exec("ROLLBACK");
        QMessageBox::warning(dlg, QObject::tr("Add failed"),
            QObject::tr("INSERT failed: %1").arg(QString::fromStdString(err.message)));
        return;
    }
    err = dbClient->exec("COMMIT");
    if (!err.ok())
    {
        (void)dbClient->exec("ROLLBACK");
        QMessageBox::warning(dlg, QObject::tr("Add failed"),
            QObject::tr("COMMIT failed: %1").arg(QString::fromStdString(err.message)));
        return;
    }

    refreshUi();
    if (statusLabel)
        statusLabel->setText(QObject::tr("pool %1 +%2 (guid=%3 chance=%4) affected=%5")
            .arg(selectedPoolEntry).arg(kindLabel).arg(guid)
            .arg(chance, 0, 'f', 2).arg(qulonglong(affected)));
}

void GroupsPoolsDialog::onAddPoolCreature()
{
    addPoolMemberWithType(this, m_dbClient, m_worldDb, m_selectedPoolEntry,
        0, tr("creature"),
        [this] { loadPoolMembers(m_selectedPoolEntry); loadPools(); },
        m_statusLabel);
}

void GroupsPoolsDialog::onAddPoolGameobject()
{
    addPoolMemberWithType(this, m_dbClient, m_worldDb, m_selectedPoolEntry,
        1, tr("gameobject"),
        [this] { loadPoolMembers(m_selectedPoolEntry); loadPools(); },
        m_statusLabel);
}

// Drop the selected pool_members row.  Reads the 'kind' column UserRole to
// decide type=0 vs type=1, then DELETEs by (poolSpawnId, type, spawnId).
// Wrapped in START TRANSACTION / COMMIT with ROLLBACK on error.
void GroupsPoolsDialog::onRemovePoolMember()
{
    if (!m_dbClient || !m_dbClient->isConnected() || m_selectedPoolEntry == 0) return;
    int const row = m_poolMembers->currentRow();
    if (row < 0 || row >= m_poolMembers->rowCount())
    {
        QMessageBox::information(this, tr("No selection"),
            tr("Select a pool member row first."));
        return;
    }
    int const type      = int(m_poolMembers->item(row, 0)->data(Qt::UserRole).toULongLong());
    qlonglong const sid = m_poolMembers->item(row, 1)->text().toLongLong();
    QString const kindLabel = m_poolMembers->item(row, 0)->text();

    auto const reply = QMessageBox::question(this, tr("Remove pool member"),
        tr("Remove (%1, spawnId=%2) from pool %3?")
            .arg(kindLabel).arg(sid).arg(m_selectedPoolEntry),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (reply != QMessageBox::Yes) return;

    QString const sql = QString(
        "DELETE FROM %1.pool_members "
        "WHERE poolSpawnId = %2 AND type = %3 AND spawnId = %4;")
        .arg(m_worldDb).arg(m_selectedPoolEntry).arg(type).arg(sid);

    auto err = m_dbClient->exec("START TRANSACTION");
    if (!err.ok())
    {
        QMessageBox::warning(this, tr("Remove failed"),
            tr("BEGIN failed: %1").arg(QString::fromStdString(err.message)));
        return;
    }
    uint64_t affected = 0;
    err = m_dbClient->exec(sql.toStdString(), &affected);
    if (!err.ok())
    {
        (void)m_dbClient->exec("ROLLBACK");
        QMessageBox::warning(this, tr("Remove failed"),
            tr("DELETE failed: %1").arg(QString::fromStdString(err.message)));
        return;
    }
    err = m_dbClient->exec("COMMIT");
    if (!err.ok())
    {
        (void)m_dbClient->exec("ROLLBACK");
        QMessageBox::warning(this, tr("Remove failed"),
            tr("COMMIT failed: %1").arg(QString::fromStdString(err.message)));
        return;
    }

    loadPoolMembers(m_selectedPoolEntry);
    loadPools();
    m_statusLabel->setText(tr("pool %1 -%2 (spawnId=%3) affected=%4")
        .arg(m_selectedPoolEntry).arg(kindLabel).arg(sid).arg(qulonglong(affected)));
}

// Edit chance for the selected pool_members row via QInputDialog::getDouble.
// Wrapped in START TRANSACTION / COMMIT with ROLLBACK on error.
void GroupsPoolsDialog::onEditPoolMemberChance()
{
    if (!m_dbClient || !m_dbClient->isConnected() || m_selectedPoolEntry == 0) return;
    int const row = m_poolMembers->currentRow();
    if (row < 0 || row >= m_poolMembers->rowCount())
    {
        QMessageBox::information(this, tr("No selection"),
            tr("Select a pool member row first."));
        return;
    }
    int const type      = int(m_poolMembers->item(row, 0)->data(Qt::UserRole).toULongLong());
    qlonglong const sid = m_poolMembers->item(row, 1)->text().toLongLong();
    double const cur    = m_poolMembers->item(row, 2)->text().toDouble();

    bool ok = false;
    double const newChance = QInputDialog::getDouble(this, tr("Edit chance"),
        tr("New chance for (%1, spawnId=%2):")
            .arg(m_poolMembers->item(row, 0)->text()).arg(sid),
        cur, 0.0, 100.0, 4, &ok);
    if (!ok) return;

    QString const sql = QString(
        "UPDATE %1.pool_members SET chance = %2 "
        "WHERE poolSpawnId = %3 AND type = %4 AND spawnId = %5;")
        .arg(m_worldDb).arg(newChance, 0, 'f', 4)
        .arg(m_selectedPoolEntry).arg(type).arg(sid);

    auto err = m_dbClient->exec("START TRANSACTION");
    if (!err.ok())
    {
        QMessageBox::warning(this, tr("Edit failed"),
            tr("BEGIN failed: %1").arg(QString::fromStdString(err.message)));
        return;
    }
    uint64_t affected = 0;
    err = m_dbClient->exec(sql.toStdString(), &affected);
    if (!err.ok())
    {
        (void)m_dbClient->exec("ROLLBACK");
        QMessageBox::warning(this, tr("Edit failed"),
            tr("UPDATE failed: %1").arg(QString::fromStdString(err.message)));
        return;
    }
    err = m_dbClient->exec("COMMIT");
    if (!err.ok())
    {
        (void)m_dbClient->exec("ROLLBACK");
        QMessageBox::warning(this, tr("Edit failed"),
            tr("COMMIT failed: %1").arg(QString::fromStdString(err.message)));
        return;
    }

    loadPoolMembers(m_selectedPoolEntry);
    m_statusLabel->setText(tr("pool %1 chance updated (affected=%2)")
        .arg(m_selectedPoolEntry).arg(qulonglong(affected)));
}

} // namespace world_editor::app
