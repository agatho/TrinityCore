/*
 * MapPickerDialog - searchable, grouped map selector for the world editor.
 *
 * Replaces the bare "type a map id" prompt. Maps are grouped by instance type
 * (Continents first, then Dungeons / Raids / Battlegrounds / Arenas /
 * Scenarios) and, within each, by expansion. A live filter box narrows by id
 * or name. Only maps that actually have mmaps on disk are listed, and each
 * leaf shows a road-count badge from the shared handcrafted_road table so the
 * operator sees at a glance which maps are already authored.
 */

#pragma once

#include "../io/MapDb2Lookup.h"

#include <QDialog>

#include <cstdint>
#include <map>
#include <set>
#include <vector>

class QLineEdit;
class QTreeWidget;
class QTreeWidgetItem;
class QDialogButtonBox;

namespace world_editor::app
{

class MapPickerDialog final : public QDialog
{
    Q_OBJECT

public:
    MapPickerDialog(std::vector<io::MapMetadata> maps,
                    std::set<uint32_t> availableMapIds,
                    std::map<uint32_t, int> roadCounts,
                    std::vector<uint32_t> recentMapIds,
                    QWidget* parent = nullptr);

    // Selected map id, or -1 if the dialog was cancelled / nothing chosen.
    [[nodiscard]] int selectedMapId() const noexcept { return m_selected; }

private slots:
    void onFilterChanged(QString const& text);
    void onItemActivated(QTreeWidgetItem* item, int column);
    void onSelectionChanged();
    void onAccept();

private:
    void build();
    void addLeaf(QTreeWidgetItem* parent, io::MapMetadata const& m);

    std::vector<io::MapMetadata> m_maps;
    std::set<uint32_t>           m_available;
    std::map<uint32_t, int>      m_roadCounts;
    std::vector<uint32_t>        m_recent;

    QLineEdit*        m_filter  = nullptr;
    QTreeWidget*      m_tree    = nullptr;
    QDialogButtonBox* m_buttons = nullptr;
    int               m_selected = -1;
};

} // namespace world_editor::app
