/*
 * GraveyardPropertiesDock - editable property panel for a selected
 * `world_safe_locs` (graveyard) row.
 *
 * All 7 schema columns editable: MapID, LocX, LocY, LocZ, Facing,
 * TransportSpawnId (0 -> NULL), Comment.  ID is read-only (PK).
 */

#pragma once

#include "../render/NavMeshView.h"

#include <QWidget>

class QLabel;
class QLineEdit;
class QSpinBox;
class QDoubleSpinBox;
class QPushButton;

namespace world_editor::app
{

class GraveyardPropertiesDock final : public QWidget
{
    Q_OBJECT

public:
    explicit GraveyardPropertiesDock(QWidget* parent = nullptr);

    void setGraveyard(int index, render::Graveyard const& g);
    void clear();
    [[nodiscard]] int currentIndex() const noexcept { return m_index; }

    void setPendingCount(size_t count);

signals:
    void graveyardEdited(render::Graveyard const& proposed);
    void deleteGraveyardRequested();
    void commitRequested();
    void revertRequested();

private slots:
    void onFormChanged();

private:
    [[nodiscard]] render::Graveyard snapshotFromForm() const;
    void applyToForm(render::Graveyard const& g);

    int               m_index = -1;
    render::Graveyard m_baseline{};
    bool              m_suppress = false;

    QLabel*         m_headerLabel       = nullptr;
    QSpinBox*       m_mapIdSpin         = nullptr;
    QDoubleSpinBox* m_locXSpin          = nullptr;
    QDoubleSpinBox* m_locYSpin          = nullptr;
    QDoubleSpinBox* m_locZSpin          = nullptr;
    QDoubleSpinBox* m_facingSpin        = nullptr;
    QSpinBox*       m_transportSpin     = nullptr;
    QLineEdit*      m_commentEdit       = nullptr;
    QLabel*         m_pendingLabel      = nullptr;
    QPushButton*    m_deleteButton      = nullptr;
    QPushButton*    m_revertButton      = nullptr;
    QPushButton*    m_commitButton      = nullptr;
};

} // namespace world_editor::app
