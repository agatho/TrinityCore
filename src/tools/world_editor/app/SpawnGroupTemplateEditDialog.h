/*
 * SpawnGroupTemplateEditDialog - modal editor for a single spawn_group_template row.
 *
 * Surfaces the three columns of spawn_group_template (groupId, groupName,
 * groupFlags).  Used by GroupsPoolsDialog's Insert/Edit toolbar buttons;
 * the caller seeds field values via setGroupId / setGroupName / setGroupFlags
 * and reads them back via the accessors after exec() == Accepted.
 *
 * groupId is the primary key.  On Insert the caller seeds it with
 * COALESCE(MAX(groupId),0)+1; on Edit the caller calls setKeyEditable(false)
 * to lock the field (re-key requires Delete + Insert because spawn_group
 * rows reference groupId).
 *
 * The flags field is shown as a hex integer (0xFF range fits the
 * current SPAWNGROUP_FLAG_* set) with a tooltip enumerating the flag bits.
 */

#pragma once

#include <QDialog>

#include <cstdint>

class QLineEdit;
class QSpinBox;

namespace world_editor::app
{

class SpawnGroupTemplateEditDialog final : public QDialog
{
    Q_OBJECT

public:
    explicit SpawnGroupTemplateEditDialog(QWidget* parent = nullptr);

    void setGroupId(uint32_t groupId);
    void setGroupName(QString const& groupName);
    void setGroupFlags(uint32_t groupFlags);
    void setKeyEditable(bool editable);

    [[nodiscard]] uint32_t groupId()    const;
    [[nodiscard]] QString  groupName()  const;
    [[nodiscard]] uint32_t groupFlags() const;

private:
    QSpinBox*  m_groupIdSpin    = nullptr;
    QLineEdit* m_groupNameEdit  = nullptr;
    QSpinBox*  m_groupFlagsSpin = nullptr;
};

} // namespace world_editor::app
