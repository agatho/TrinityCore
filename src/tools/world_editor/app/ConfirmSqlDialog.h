/*
 * ConfirmSqlDialog - lightweight confirm-and-apply dialog for one-off
 * SQL statements.  Used by Phase 6b (add-to-group, edit-pool-chance)
 * where building a full pending-change model would be overkill.
 *
 * Shows the SQL to be executed, the table affected, and Apply/Cancel.
 * On Apply, runs the statement inside a transaction; on success the
 * caller can re-query the affected table.  Mirrors §10.2 "explicit
 * commit + diff preview" for single-row operations.
 */

#pragma once

#include "../db/MySqlClient.h"

#include <QDialog>
#include <QString>

class QPlainTextEdit;
class QPushButton;
class QLabel;

namespace world_editor::app
{

class ConfirmSqlDialog final : public QDialog
{
    Q_OBJECT

public:
    ConfirmSqlDialog(db::MySqlClient* dbClient,
                     QString const& summary,
                     QString const& sql,
                     QWidget* parent = nullptr);

    [[nodiscard]] bool applied() const noexcept { return m_applied; }
    [[nodiscard]] uint64_t affectedRows() const noexcept { return m_affected; }

private slots:
    void onApply();

private:
    db::MySqlClient* m_dbClient = nullptr;
    QString          m_sql;
    bool             m_applied  = false;
    uint64_t         m_affected = 0;

    QPlainTextEdit*  m_sqlView    = nullptr;
    QLabel*          m_statusLabel = nullptr;
    QPushButton*     m_applyBtn   = nullptr;
};

} // namespace world_editor::app
