/*
 * ConditionEditDialog - tabbed modal editor for a single conditions row.
 *
 * The dialog has two tabs (Key / Body) and round-trips a
 * render::Condition struct via setCondition / condition().
 *
 * SourceTypeOrReferenceId and ConditionTypeOrReference are QComboBox
 * pickers populated from ConditionEnumTables.h.  Values outside the
 * known tables (e.g. negative reference ids, future TC enum additions)
 * get appended as synthetic "(unknown N)" rows so the operator can
 * round-trip exotic data without losing it.
 */

#pragma once

#include "../render/NavMeshView.h"

#include <QDialog>

class QCheckBox;
class QComboBox;
class QLineEdit;
class QPlainTextEdit;
class QSpinBox;
class QTabWidget;

namespace world_editor::app
{

class ConditionEditDialog final : public QDialog
{
    Q_OBJECT

public:
    explicit ConditionEditDialog(QWidget* parent = nullptr);

    void setCondition(render::Condition const& row);
    [[nodiscard]] render::Condition condition() const;

    // Lock the composite-PK fields.  Used when editing an existing row
    // -- the operator must Delete + Add to re-key.
    void setKeyEditable(bool editable);

private:
    void buildUi();
    void buildKeyTab();
    void buildBodyTab();

    QTabWidget* m_tabs = nullptr;

    // Key tab.  All 11 PK columns.
    QComboBox*  m_sourceTypeCombo  = nullptr;   // SourceTypeOrReferenceId (signed)
    QSpinBox*   m_sourceGroupSpin  = nullptr;   // SourceGroup (unsigned)
    QSpinBox*   m_sourceEntrySpin  = nullptr;   // SourceEntry (signed)
    QSpinBox*   m_sourceIdSpin     = nullptr;   // SourceId (signed)
    QSpinBox*   m_elseGroupSpin    = nullptr;   // ElseGroup (unsigned)
    QComboBox*  m_condTypeCombo    = nullptr;   // ConditionTypeOrReference (signed)
    QSpinBox*   m_condTargetSpin   = nullptr;   // ConditionTarget (uint8 0..255)
    QSpinBox*   m_condValue1Spin   = nullptr;   // ConditionValue1
    QSpinBox*   m_condValue2Spin   = nullptr;   // ConditionValue2
    QSpinBox*   m_condValue3Spin   = nullptr;   // ConditionValue3
    QLineEdit*  m_condStringValue1Edit = nullptr; // ConditionStringValue1

    // Body tab.
    QCheckBox*  m_negativeCheck    = nullptr;   // NegativeCondition (0/1)
    QSpinBox*   m_errorTypeSpin    = nullptr;
    QSpinBox*   m_errorTextIdSpin  = nullptr;
    QLineEdit*  m_scriptNameEdit   = nullptr;
    QPlainTextEdit* m_commentEdit  = nullptr;

    bool m_keyEditable = true;
};

} // namespace world_editor::app
