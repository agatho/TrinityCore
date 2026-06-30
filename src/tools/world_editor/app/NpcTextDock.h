/*
 * NpcTextDock - read-only panel for the `npc_text` row referenced by
 * gossip flow (gossip_menu.TextID, npc_gossip, scripted gossip handlers,
 * etc).  Each npc_text row carries up to 8 variants (text0_0..text7_0),
 * each with its own probability, broadcast id, male/female text, language
 * and three emote ids.  We render every non-empty / non-zero-prob variant
 * stacked vertically; the operator picks an id via the Tools menu since
 * v1 has no inline trigger from another dock.
 *
 * Schema is probed via INFORMATION_SCHEMA so a fork that drops one of
 * the per-variant columns still surfaces the rest cleanly.
 */

#pragma once

#include <QWidget>

#include <cstdint>

class QLabel;
class QVBoxLayout;
class QScrollArea;

namespace world_editor::db { class MySqlClient; }

namespace world_editor::app
{

class NpcTextDock final : public QWidget
{
    Q_OBJECT

public:
    explicit NpcTextDock(db::MySqlClient* dbClient,
                         QWidget* parent = nullptr);

    // Look up `textId` in npc_text and render its variants.  id=0 clears.
    void setNpcTextId(uint32_t textId);
    void clear();
    // Late-bind the DB client (dock is built before the connection is up).
    void setDbClient(db::MySqlClient* db) { m_db = db; }

private:
    // Re-create the variant labels from the row.  The variant widgets live
    // under m_variantHost (re-parented on every refresh) so we don't have
    // to hand-clear 8 * 6 fixed labels.
    void renderRow(uint32_t textId, struct NpcTextRow const& row);

    db::MySqlClient* m_db          = nullptr;
    QLabel*          m_header      = nullptr;
    QScrollArea*     m_scroll      = nullptr;
    QWidget*         m_variantHost = nullptr;   // owned by m_scroll; replaced on refresh.
};

} // namespace world_editor::app
