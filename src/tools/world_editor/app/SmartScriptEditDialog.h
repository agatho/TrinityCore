/*
 * SmartScriptEditDialog - tabbed modal editor for a single smart_scripts
 * row.  The dialog has five tabs (Identity / Event / Action / Target /
 * Comment) and round-trips a render::SmartScript struct via
 * setRow(SmartScript const&) and rowSnapshot() -> SmartScript.
 *
 * Numeric event_type / action_type / target_type are surfaced as raw
 * QSpinBox values; the operator references TC core's SmartScript.h
 * enum tables.  source_type is a constrained combo (Creature=0,
 * GameObject=1, ActionList=9) because those are the only three values
 * the parser accepts.
 *
 * Nullable string columns (action_param_string, target_param_string)
 * are surfaced with a "(null)" checkbox the operator can toggle; an
 * empty + null-checked field roundtrips to SQL NULL.
 */

#pragma once

#include "../render/NavMeshView.h"

#include <QDialog>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLineEdit;
class QPlainTextEdit;
class QSpinBox;
class QTabWidget;

namespace world_editor::app
{

class SmartScriptEditDialog final : public QDialog
{
    Q_OBJECT

public:
    explicit SmartScriptEditDialog(QWidget* parent = nullptr);

    void setRow(render::SmartScript const& row);
    [[nodiscard]] render::SmartScript rowSnapshot() const;

    // Lock the composite-PK fields (entryorguid / source_type / id /
    // link / difficulties).  Used when editing an existing row -- the
    // PK is part of identity and not editable in place; the operator
    // must Delete + Add to re-key.
    void setKeyEditable(bool editable);

private:
    void buildUi();
    void buildIdentityTab();
    void buildEventTab();
    void buildActionTab();
    void buildTargetTab();
    void buildCommentTab();

    QTabWidget* m_tabs = nullptr;

    // Identity tab.  entryorguid is bigint signed (can reference creature.guid
    // as a negative value); QSpinBox max is INT_MAX so we use a QLineEdit
    // with a 64-bit validator instead.
    QLineEdit*  m_entryorguidEdit = nullptr;
    QComboBox*  m_sourceTypeCombo = nullptr;
    QSpinBox*   m_idSpin          = nullptr;
    QSpinBox*   m_linkSpin        = nullptr;
    QLineEdit*  m_difficultiesEdit = nullptr;

    // Event tab
    QComboBox*  m_eventTypeCombo      = nullptr;
    QSpinBox*   m_eventPhaseMaskSpin  = nullptr;
    QSpinBox*   m_eventChanceSpin     = nullptr;
    QSpinBox*   m_eventFlagsSpin      = nullptr;
    QSpinBox*   m_eventP1Spin         = nullptr;
    QSpinBox*   m_eventP2Spin         = nullptr;
    QSpinBox*   m_eventP3Spin         = nullptr;
    QSpinBox*   m_eventP4Spin         = nullptr;
    QSpinBox*   m_eventP5Spin         = nullptr;
    QLineEdit*  m_eventParamStringEdit = nullptr;

    // Action tab
    QComboBox*  m_actionTypeCombo      = nullptr;
    QSpinBox*   m_actionP1Spin         = nullptr;
    QSpinBox*   m_actionP2Spin         = nullptr;
    QSpinBox*   m_actionP3Spin         = nullptr;
    QSpinBox*   m_actionP4Spin         = nullptr;
    QSpinBox*   m_actionP5Spin         = nullptr;
    QSpinBox*   m_actionP6Spin         = nullptr;
    QSpinBox*   m_actionP7Spin         = nullptr;
    QLineEdit*  m_actionParamStringEdit = nullptr;
    QCheckBox*  m_actionParamStringNullChk = nullptr;

    // Target tab
    QComboBox*      m_targetTypeCombo      = nullptr;
    QSpinBox*       m_targetP1Spin         = nullptr;
    QSpinBox*       m_targetP2Spin         = nullptr;
    QSpinBox*       m_targetP3Spin         = nullptr;
    QSpinBox*       m_targetP4Spin         = nullptr;
    QLineEdit*      m_targetParamStringEdit = nullptr;
    QCheckBox*      m_targetParamStringNullChk = nullptr;
    QDoubleSpinBox* m_targetXSpin          = nullptr;
    QDoubleSpinBox* m_targetYSpin          = nullptr;
    QDoubleSpinBox* m_targetZSpin          = nullptr;
    QDoubleSpinBox* m_targetOSpin          = nullptr;

    // Comment tab
    QPlainTextEdit* m_commentEdit = nullptr;

    bool m_keyEditable = true;
};

} // namespace world_editor::app
