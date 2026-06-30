/*
 * SpawnCloneDialog - modal dialog driving the spawn "Clone..." right-click action.
 *
 * Operator right-clicks a spawn icon in the 2D viewer; MainWindow opens this
 * dialog to gather (count, pattern, radius, snap-to-ground, preserve-orientation)
 * before producing N copies of the source spawn with offsets drawn from the
 * chosen pattern.  The dialog emits one cloneRequested signal on Apply; the
 * actual row construction + GUID reservation + undo frame all live in
 * MainWindow so the dialog stays Qt-only and easy to unit-test from afar.
 *
 * Patterns:
 *   0  Random scatter - uniform (theta, r) within [0, 2pi) x [0, radius]
 *   1  Ring            - even angular spacing at exactly `radius` from source
 *   2  Grid            - ceil(sqrt(N)) x ceil(sqrt(N)) square, `radius` spacing,
 *                        centered on source (excess slots dropped).
 */

#pragma once

#include <QDialog>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QSpinBox;

namespace world_editor::app
{

class SpawnCloneDialog final : public QDialog
{
    Q_OBJECT

public:
    explicit SpawnCloneDialog(QWidget* parent = nullptr);

signals:
    // Emitted exactly once when the operator hits Apply with a valid count.
    // patternIdx matches the QComboBox order (0 scatter, 1 ring, 2 grid).
    void cloneRequested(int count, int patternIdx, float radius, bool snap, bool preserveOri);

private slots:
    void onApply();

private:
    QSpinBox*       m_count          = nullptr;
    QComboBox*      m_pattern        = nullptr;
    QDoubleSpinBox* m_radius         = nullptr;
    QCheckBox*      m_snap           = nullptr;
    QCheckBox*      m_preserveOri    = nullptr;
};

} // namespace world_editor::app
