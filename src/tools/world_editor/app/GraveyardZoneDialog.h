/*
 * GraveyardZoneDialog - browser + editor for the `graveyard_zone` table.
 *
 * Schema is just (ID, GhostZone, Comment) with composite PK (ID, GhostZone):
 *   - `ID` is a `world_safe_locs.ID` (which graveyard).
 *   - `GhostZone` is the zone whose ghosts respawn at that graveyard.
 *   - `Comment` is a free-text note.
 *
 * The editor opens a single-table dialog with filter + Add / Remove
 * buttons.  Each write goes through ConfirmSqlDialog so the operator
 * sees the SQL before commit (mirrors GroupsPoolsDialog pattern).
 */

#pragma once

#include "../db/MySqlClient.h"

#include <QDialog>
#include <QString>

class QLineEdit;
class QTableView;
class QLabel;
class QPushButton;
class QStandardItemModel;
class QSortFilterProxyModel;

namespace world_editor::app
{

class GraveyardZoneDialog final : public QDialog
{
    Q_OBJECT

public:
    explicit GraveyardZoneDialog(db::MySqlClient* dbClient,
                                 QWidget* parent = nullptr);

private slots:
    void onFilterTextChanged(QString const& text);
    void onAddClicked();
    void onRemoveClicked();

private:
    void reload();

    db::MySqlClient*       m_dbClient   = nullptr;
    QLineEdit*             m_filterEdit = nullptr;
    QTableView*            m_view       = nullptr;
    QLabel*                m_statusLbl  = nullptr;
    QStandardItemModel*    m_model      = nullptr;
    QSortFilterProxyModel* m_proxy      = nullptr;
};

} // namespace world_editor::app
