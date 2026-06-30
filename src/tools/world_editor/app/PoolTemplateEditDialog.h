/*
 * PoolTemplateEditDialog - modal editor for a single pool_template row.
 *
 * Surfaces the three columns of pool_template (entry, max_limit,
 * description).  The dialog is used for both Insert and Edit; the
 * caller seeds field values via setEntry / setMaxLimit / setDescription
 * and pulls them back out via the accessors after exec() == Accepted.
 *
 * `entry` is the primary key.  On Insert the caller seeds it with
 * COALESCE(MAX(entry),0)+1; on Edit the caller calls setKeyEditable(false)
 * to lock the field (re-key requires Delete + Insert).
 */

#pragma once

#include <QDialog>

#include <cstdint>

class QLineEdit;
class QSpinBox;

namespace world_editor::app
{

class PoolTemplateEditDialog final : public QDialog
{
    Q_OBJECT

public:
    explicit PoolTemplateEditDialog(QWidget* parent = nullptr);

    void setEntry(uint32_t entry);
    void setMaxLimit(uint32_t maxLimit);
    void setDescription(QString const& description);
    void setKeyEditable(bool editable);

    [[nodiscard]] uint32_t entry() const;
    [[nodiscard]] uint32_t maxLimit() const;
    [[nodiscard]] QString  description() const;

private:
    QSpinBox*  m_entrySpin    = nullptr;
    QSpinBox*  m_maxLimitSpin = nullptr;
    QLineEdit* m_descEdit     = nullptr;
};

} // namespace world_editor::app
