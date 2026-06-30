/*
 * SpawnDiffDialog - modal field-by-field comparison of two render::Spawn rows.
 *
 * Opened from "Spawn -> Diff selected pair..." when exactly two spawns are
 * selected.  Renders one row per struct field with the value for Spawn A and
 * Spawn B side by side; rows where the two values differ are tinted light
 * yellow so the operator can scan disagreements at a glance.  Read-only.
 */

#pragma once

#include "../render/NavMeshView.h"

#include <QDialog>

namespace world_editor::app
{

class SpawnDiffDialog final : public QDialog
{
    Q_OBJECT

public:
    SpawnDiffDialog(render::Spawn const& a,
                    render::Spawn const& b,
                    QWidget* parent = nullptr);
};

} // namespace world_editor::app
