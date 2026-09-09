/*
 * FindJumpDialog - global navigator for spawns and templates.
 *
 * Three search modes:
 *   - By template: type entry# or name fragment, results match against
 *     creature_template and gameobject_template (both queried in one
 *     pass). "Jump" picks the first spawn on the currently-loaded map
 *     for that entry; if none exists, the FIRST spawn on ANY map.
 *   - By spawn guid: numeric, hits creature.guid then gameobject.guid.
 *     "Jump" goes directly to that spawn's (mapId, x, y).
 *   - By coords: type "mapId x y" and jump there (handy for following
 *     SQL hits or external tools).
 *
 * On Jump the dialog emits jumpRequested(mapId, x, y, optional<guid>).
 * MainWindow handles the actual viewer pan + spawn selection.
 *
 * Triggered via Edit -> Find... (Ctrl+F).
 */

#pragma once

#include "../db/MySqlClient.h"

#include <QDialog>
#include <QString>

#include <cstdint>
#include <optional>

class QLineEdit;
class QTabWidget;
class QTableView;
class QLabel;
class QPushButton;
class QStandardItemModel;
class QSortFilterProxyModel;
class QDoubleSpinBox;
class QSpinBox;

namespace world_editor::app
{

class FindJumpDialog final : public QDialog
{
    Q_OBJECT

public:
    explicit FindJumpDialog(db::MySqlClient* dbClient,
                            uint32_t          currentMapId,
                            float             currentWorldX = 0.0f,
                            float             currentWorldY = 0.0f,
                            QWidget*          parent = nullptr);

signals:
    // mapId + (x, y) are always set; guid is set only when a specific
    // spawn was selected (so MainWindow can highlight it).
    void jumpRequested(uint32_t mapId, float worldX, float worldY,
                       std::optional<int64_t> guid);

private slots:
    void onSearchTemplate();
    void onSearchGuid();
    void onJumpToCoords();
    void onJumpTemplate();
    void onJumpGuid();
    void onBookmarkAddCurrent();
    void onBookmarkRename();
    void onBookmarkRemove();
    void onBookmarkJump();
    void onBookmarkManage();

private:
    db::MySqlClient* m_dbClient    = nullptr;
    uint32_t         m_currentMapId = 0;

    QTabWidget*           m_tabs = nullptr;

    // Template tab
    QLineEdit*            m_tplFilter = nullptr;
    QTableView*           m_tplView   = nullptr;
    QStandardItemModel*   m_tplModel  = nullptr;
    QSortFilterProxyModel* m_tplProxy = nullptr;
    QPushButton*          m_tplJumpBtn = nullptr;
    QLabel*               m_tplStatusLbl = nullptr;

    // Guid tab
    QLineEdit*            m_guidEdit = nullptr;
    QTableView*           m_guidView = nullptr;
    QStandardItemModel*   m_guidModel = nullptr;
    QPushButton*          m_guidJumpBtn = nullptr;
    QLabel*               m_guidStatusLbl = nullptr;

    // Coords tab
    QSpinBox*             m_coordsMap = nullptr;
    QDoubleSpinBox*       m_coordsX = nullptr;
    QDoubleSpinBox*       m_coordsY = nullptr;

    // Bookmarks tab - persisted via QSettings as a flat list of
    // (name, folder, tags, mapId, x, y, z) tuples.  Operator builds the
    // list over time by Add-current from whatever they're currently
    // looking at.  Grouping/filtering lives in BookmarksManagerDialog,
    // reachable via the "Manage..." button on this tab and from the
    // View -> Bookmarks menu in MainWindow.
    QTableView*           m_bookmarkView   = nullptr;
    QStandardItemModel*   m_bookmarkModel  = nullptr;
    QPushButton*          m_bookmarkAddBtn = nullptr;
    QPushButton*          m_bookmarkRenameBtn = nullptr;
    QPushButton*          m_bookmarkRemoveBtn = nullptr;
    QPushButton*          m_bookmarkJumpBtn   = nullptr;
    QPushButton*          m_bookmarkManageBtn = nullptr;

    void loadBookmarks();
    void saveBookmarks() const;
};

} // namespace world_editor::app
