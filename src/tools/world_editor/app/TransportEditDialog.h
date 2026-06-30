/*
 * TransportEditDialog - modal form for one `transports` row.
 *
 * Schema (live MySQL 9.4):
 *   guid           bigint unsigned   PK
 *   entry          int unsigned      UNIQUE
 *   name           mediumtext        nullable
 *   phaseUseFlags  tinyint unsigned
 *   phaseid        int
 *   phasegroup     int
 *   ScriptName     varchar(64)
 *
 * `guid` and `entry` are key fields; `setKeyEditable(false)` locks them
 * when the dialog opens for editing an existing row (the operator must
 * Remove + Add to re-key).  All other fields edit in place.
 *
 * The dialog round-trips a TransportRow struct; the caller generates
 * the actual INSERT/UPDATE SQL via ConfirmSqlDialog so the operator
 * sees the statement before commit.
 */

#pragma once

#include <QDialog>
#include <QString>

#include <cstdint>

class QLineEdit;
class QSpinBox;
class QPlainTextEdit;

namespace world_editor::app
{

struct TransportRow
{
    int64_t  guid          = 0;
    uint32_t entry         = 0;
    QString  name;
    uint8_t  phaseUseFlags = 0;
    int32_t  phaseId       = 0;
    int32_t  phaseGroup    = 0;
    QString  scriptName;
};

class TransportEditDialog final : public QDialog
{
    Q_OBJECT

public:
    explicit TransportEditDialog(QWidget* parent = nullptr);

    void setRow(TransportRow const& r);
    [[nodiscard]] TransportRow rowSnapshot() const;

    // Lock guid + entry when editing an existing row.
    void setKeyEditable(bool editable);

private:
    QLineEdit*      m_guidEdit          = nullptr;  // bigint, LineEdit for 64-bit
    QSpinBox*       m_entrySpin         = nullptr;
    QPlainTextEdit* m_nameEdit          = nullptr;
    QSpinBox*       m_phaseUseFlagsSpin = nullptr;
    QSpinBox*       m_phaseIdSpin       = nullptr;
    QSpinBox*       m_phaseGroupSpin    = nullptr;
    QLineEdit*      m_scriptNameEdit    = nullptr;
};

} // namespace world_editor::app
