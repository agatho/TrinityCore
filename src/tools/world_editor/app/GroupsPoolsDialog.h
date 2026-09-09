/*
 * GroupsPoolsDialog - read-only browser for spawn_group + pool tables.
 *
 * Two tabs.  Spawn Groups: spawn_group_template rows on the left;
 * picking one shows the spawn_group members in a table on the right
 * (spawnType + spawnId pairs).  Pools: pool_template rows on the left;
 * picking one shows pool_members (type + spawnId + chance).
 *
 * "Highlight on map" button selects the picked group's members in the
 * viewer so the operator can see them spatially - useful for
 * 'where does this spawn_group sit on the map?'.
 *
 * Phase 6a is read-only.  Editing (assignment, weights, rename) ships
 * in 6b once this baseline is validated.
 */

#pragma once

#include "../db/MySqlClient.h"

#include <QDialog>
#include <QVector>

#include <cstdint>

class QTabWidget;
class QListView;
class QTableWidget;
class QStandardItemModel;
class QLineEdit;
class QPushButton;
class QLabel;
class QSortFilterProxyModel;

namespace world_editor::app
{

class GroupsPoolsDialog final : public QDialog
{
    Q_OBJECT

public:
    GroupsPoolsDialog(db::MySqlClient* dbClient,
                      QString const& worldDbName,
                      QWidget* parent = nullptr);

signals:
    // Operator asked to highlight a group's members on the map.
    // The vector contains creature.guid values for spawnType=1 rows.
    void highlightSpawnGuids(QVector<qlonglong> const& guids);

private slots:
    void onTabChanged(int idx);
    void onGroupFilter(QString const& text);
    void onPoolFilter(QString const& text);
    void onGroupRowChanged(QModelIndex const& cur, QModelIndex const& prev);
    void onPoolRowChanged (QModelIndex const& cur, QModelIndex const& prev);
    void onHighlightClicked();
    void onPoolMemberCellChanged(int row, int col);
    void onGroupMembersContextMenu(QPoint const& pos);
    void onPoolMembersContextMenu(QPoint const& pos);
    void onCreateGroup();
    void onCreatePool();
    void onSaveGroupHeader();
    void onSavePoolHeader();
    void onInsertPoolTemplate();
    void onEditPoolTemplate();
    void onDeletePoolTemplate();
    void onInsertSpawnGroupTemplate();
    void onEditSpawnGroupTemplate();
    void onDeleteSpawnGroupTemplate();
    // spawn_group member ops: explicit add (prompt for spawnType+spawnId)
    // and remove (drop currently-selected member row).  Both wrap their
    // DML in START TRANSACTION / COMMIT, rolling back on any error.
    void onAddSpawnGroupMember();
    void onRemoveSpawnGroupMember();
    // pool_members ops: type-aware Add creature / Add gameobject inserts a
    // new row (type+spawnId+chance+description); Remove drops the selected
    // row using the kind column to pick type=0 vs type=1; Edit chance
    // opens a getDouble dialog for the selected row.  All DML wrapped in
    // START TRANSACTION / COMMIT with ROLLBACK on error.
    void onAddPoolCreature();
    void onAddPoolGameobject();
    void onRemovePoolMember();
    void onEditPoolMemberChance();

private:
    void loadGroups();
    void loadPools();
    void loadGroupMembers(uint32_t groupId);
    void loadPoolMembers(uint32_t poolEntry);

    db::MySqlClient* m_dbClient = nullptr;
    QString          m_worldDb;

    QTabWidget*           m_tabs       = nullptr;
    QLineEdit*            m_groupFilter = nullptr;
    QListView*            m_groupList   = nullptr;
    QStandardItemModel*   m_groupModel  = nullptr;
    QTableWidget*         m_groupMembers = nullptr;
    QPushButton*          m_highlightButton = nullptr;
    QLineEdit*            m_groupNameEdit = nullptr;
    QLineEdit*            m_groupFlagsEdit = nullptr;
    QPushButton*          m_groupSaveButton = nullptr;
    QPushButton*          m_newGroupButton = nullptr;
    QPushButton*          m_groupInsertButton = nullptr;
    QPushButton*          m_groupEditButton   = nullptr;
    QPushButton*          m_groupDeleteButton = nullptr;
    // Member-level toolbar (right pane, above the spawn_group members table).
    QPushButton*          m_memberAddButton       = nullptr;
    QPushButton*          m_memberRemoveButton    = nullptr;
    QPushButton*          m_memberFromSelButton   = nullptr;
    QLineEdit*            m_poolFilter  = nullptr;
    QListView*            m_poolList    = nullptr;
    QStandardItemModel*   m_poolModel   = nullptr;
    QTableWidget*         m_poolMembers = nullptr;
    QLineEdit*            m_poolDescEdit = nullptr;
    QPushButton*          m_poolSaveButton = nullptr;
    QPushButton*          m_newPoolButton = nullptr;
    QPushButton*          m_poolInsertButton = nullptr;
    QPushButton*          m_poolEditButton   = nullptr;
    QPushButton*          m_poolDeleteButton = nullptr;
    // pool_members toolbar (right pane, above the pool members table).
    QPushButton*          m_poolMemberAddCreatureButton   = nullptr;
    QPushButton*          m_poolMemberAddGameobjectButton = nullptr;
    QPushButton*          m_poolMemberRemoveButton        = nullptr;
    QPushButton*          m_poolMemberEditChanceButton    = nullptr;
    QLabel*               m_statusLabel = nullptr;
    ::QSortFilterProxyModel*  m_groupProxy = nullptr;
    ::QSortFilterProxyModel*  m_poolProxy  = nullptr;

    uint32_t              m_selectedGroupId = 0;
    QVector<qlonglong>    m_lastMemberGuids;
    uint32_t              m_selectedPoolEntry = 0;
    bool                  m_suppressCellChange = false;
};

} // namespace world_editor::app
