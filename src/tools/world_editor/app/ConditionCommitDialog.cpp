#include "ConditionCommitDialog.h"

#include <QApplication>
#include <QCheckBox>
#include <QDateTime>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFontDatabase>
#include <QLabel>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QStringList>
#include <QTextStream>
#include <QVBoxLayout>

#include <algorithm>
#include <set>
#include <tuple>

namespace world_editor::app
{

namespace
{

QString esc(db::MySqlClient* c, QString const& v)
{
    if (!c) return v;
    return QString::fromStdString(c->escapeString(v.toStdString()));
}

// All 11 PK columns participate in the WHERE clause.  Drift here =
// "UPDATE clobbers the wrong row" -- the smoketest covers it.
QString formatWhere(db::MySqlClient* c, render::Condition const& a)
{
    return QString(
        "SourceTypeOrReferenceId=%1 AND SourceGroup=%2 AND SourceEntry=%3 AND SourceId=%4 "
        "AND ElseGroup=%5 AND ConditionTypeOrReference=%6 AND ConditionTarget=%7 "
        "AND ConditionValue1=%8 AND ConditionValue2=%9 AND ConditionValue3=%10 "
        "AND ConditionStringValue1='%11'")
        .arg(int(a.sourceTypeOrReferenceId))
        .arg(a.sourceGroup)
        .arg(int(a.sourceEntry))
        .arg(int(a.sourceId))
        .arg(a.elseGroup)
        .arg(int(a.conditionTypeOrReference))
        .arg(int(a.conditionTarget))
        .arg(a.conditionValue1)
        .arg(a.conditionValue2)
        .arg(a.conditionValue3)
        .arg(esc(c, a.conditionStringValue1));
}

QString formatUpdate(db::MySqlClient* c, render::Condition const& a)
{
    // PK columns are NOT in the SET list -- re-key = Delete + Add.
    return QString(
        "UPDATE conditions SET "
        "NegativeCondition=%1, ErrorType=%2, ErrorTextId=%3, "
        "ScriptName='%4', Comment='%5' "
        "WHERE %6;")
        .arg(int(a.negativeCondition))
        .arg(a.errorType)
        .arg(a.errorTextId)
        .arg(esc(c, a.scriptName))
        .arg(esc(c, a.comment))
        .arg(formatWhere(c, a));
}

QString formatInsert(db::MySqlClient* c, render::Condition const& a)
{
    return QString(
        "INSERT INTO conditions "
        "(SourceTypeOrReferenceId, SourceGroup, SourceEntry, SourceId, "
        " ElseGroup, ConditionTypeOrReference, ConditionTarget, "
        " ConditionValue1, ConditionValue2, ConditionValue3, ConditionStringValue1, "
        " NegativeCondition, ErrorType, ErrorTextId, ScriptName, Comment) "
        "VALUES (%1, %2, %3, %4, %5, %6, %7, %8, %9, %10, '%11', "
        "        %12, %13, %14, '%15', '%16');")
        .arg(int(a.sourceTypeOrReferenceId))
        .arg(a.sourceGroup)
        .arg(int(a.sourceEntry))
        .arg(int(a.sourceId))
        .arg(a.elseGroup)
        .arg(int(a.conditionTypeOrReference))
        .arg(int(a.conditionTarget))
        .arg(a.conditionValue1)
        .arg(a.conditionValue2)
        .arg(a.conditionValue3)
        .arg(esc(c, a.conditionStringValue1))
        .arg(int(a.negativeCondition))
        .arg(a.errorType)
        .arg(a.errorTextId)
        .arg(esc(c, a.scriptName))
        .arg(esc(c, a.comment));
}

QString formatDelete(db::MySqlClient* c, render::Condition const& a)
{
    return QString("DELETE FROM conditions WHERE %1;").arg(formatWhere(c, a));
}

// Backup: re-applyable INSERT...ON DUPLICATE KEY UPDATE that restores
// the *before* image for every Update/Delete row in the changeset.
QString formatBackup(db::MySqlClient* c, render::Condition const& a)
{
    return QString(
        "INSERT INTO conditions "
        "(SourceTypeOrReferenceId, SourceGroup, SourceEntry, SourceId, "
        " ElseGroup, ConditionTypeOrReference, ConditionTarget, "
        " ConditionValue1, ConditionValue2, ConditionValue3, ConditionStringValue1, "
        " NegativeCondition, ErrorType, ErrorTextId, ScriptName, Comment) "
        "VALUES (%1, %2, %3, %4, %5, %6, %7, %8, %9, %10, '%11', "
        "        %12, %13, %14, '%15', '%16') "
        "ON DUPLICATE KEY UPDATE "
        "NegativeCondition=VALUES(NegativeCondition), "
        "ErrorType=VALUES(ErrorType), ErrorTextId=VALUES(ErrorTextId), "
        "ScriptName=VALUES(ScriptName), Comment=VALUES(Comment);")
        .arg(int(a.sourceTypeOrReferenceId))
        .arg(a.sourceGroup)
        .arg(int(a.sourceEntry))
        .arg(int(a.sourceId))
        .arg(a.elseGroup)
        .arg(int(a.conditionTypeOrReference))
        .arg(int(a.conditionTarget))
        .arg(a.conditionValue1)
        .arg(a.conditionValue2)
        .arg(a.conditionValue3)
        .arg(esc(c, a.conditionStringValue1))
        .arg(int(a.negativeCondition))
        .arg(a.errorType)
        .arg(a.errorTextId)
        .arg(esc(c, a.scriptName))
        .arg(esc(c, a.comment));
}

} // namespace

ConditionCommitDialog::ConditionCommitDialog(db::MySqlClient* dbClient,
                                             db::ConditionsModel const& model,
                                             QWidget* parent)
    : QDialog(parent), m_dbClient(dbClient), m_model(model)
{
    setWindowTitle(tr("Commit conditions changes"));
    setModal(true);
    resize(840, 620);

    m_sqlPreview = new QPlainTextEdit(this);
    m_sqlPreview->setReadOnly(true);
    m_sqlPreview->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    m_sqlPreview->setPlainText(buildSqlPreview());

    m_backupCheckbox = new QCheckBox(
        tr("Write before-images to editor_backups/ before applying"), this);
    m_backupCheckbox->setChecked(true);

    m_statusLabel = new QLabel(QString{}, this);
    m_statusLabel->setWordWrap(true);

    auto* buttons = new QDialogButtonBox(this);
    m_commitButton = buttons->addButton(tr("Commit"), QDialogButtonBox::AcceptRole);
    m_commitButton->setDefault(true);
    buttons->addButton(QDialogButtonBox::Cancel);

    connect(m_commitButton, &QPushButton::clicked, this, &ConditionCommitDialog::onCommitClicked);
    connect(buttons,        &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto* outer = new QVBoxLayout(this);
    outer->addWidget(new QLabel(tr(
        "The following SQL will run inside a single transaction. Review before committing.")));
    outer->addWidget(m_sqlPreview, 1);
    outer->addWidget(m_backupCheckbox);
    outer->addWidget(m_statusLabel);
    outer->addWidget(buttons);
}

QString ConditionCommitDialog::buildSqlPreview() const
{
    QString out;
    QTextStream ts(&out);
    ts << "BEGIN;\n\n";

    size_t ins = 0, upd = 0, del = 0;
    for (db::ConditionChangeRecord const& c : m_model.changes())
    {
        if      (c.kind == db::ConditionChangeKind::Insert) ++ins;
        else if (c.kind == db::ConditionChangeKind::Update) ++upd;
        else if (c.kind == db::ConditionChangeKind::Delete) ++del;
    }

    if (del > 0)
    {
        ts << "-- " << del << " DELETE(s)\n";
        for (db::ConditionChangeRecord const& c : m_model.changes())
            if (c.kind == db::ConditionChangeKind::Delete)
                ts << formatDelete(m_dbClient, c.before) << "\n";
        ts << "\n";
    }
    if (upd > 0)
    {
        ts << "-- " << upd << " UPDATE(s)\n";
        for (db::ConditionChangeRecord const& c : m_model.changes())
            if (c.kind == db::ConditionChangeKind::Update)
                ts << formatUpdate(m_dbClient, c.after) << "\n";
        ts << "\n";
    }
    if (ins > 0)
    {
        ts << "-- " << ins << " INSERT(s)\n";
        for (db::ConditionChangeRecord const& c : m_model.changes())
            if (c.kind == db::ConditionChangeKind::Insert)
                ts << formatInsert(m_dbClient, c.after) << "\n";
        ts << "\n";
    }

    ts << "COMMIT;\n";
    return out;
}

QString ConditionCommitDialog::buildBackupSql() const
{
    QString out;
    QTextStream ts(&out);
    ts << "-- world_editor conditions backup\n";
    ts << "-- generated: " << QDateTime::currentDateTimeUtc().toString(Qt::ISODate) << "Z\n\n";
    ts << "BEGIN;\n\n";
    for (db::ConditionChangeRecord const& c : m_model.changes())
    {
        if (c.kind == db::ConditionChangeKind::Update
         || c.kind == db::ConditionChangeKind::Delete)
        {
            ts << formatBackup(m_dbClient, c.before) << "\n";
        }
    }
    ts << "\nCOMMIT;\n";
    return out;
}

bool ConditionCommitDialog::writeBackupFile(QString const& sql, QString& outPath, QString& outError) const
{
    QString const dirPath = QStringLiteral("editor_backups");
    QDir().mkpath(dirPath);
    QString const stamp = QDateTime::currentDateTimeUtc().toString(QStringLiteral("yyyyMMdd_HHmmss"));
    QString const fileName = QStringLiteral("%1/%2_conditions.sql").arg(dirPath).arg(stamp);
    QFile f(fileName);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
    {
        outError = f.errorString();
        return false;
    }
    QByteArray const bytes = sql.toUtf8();
    if (f.write(bytes) != bytes.size())
    {
        outError = f.errorString();
        return false;
    }
    f.close();
    outPath = fileName;
    return true;
}

bool ConditionCommitDialog::validateChanges(QString& outError) const
{
    if (!m_dbClient || !m_dbClient->isConnected())
    {
        outError = tr("not connected to DB");
        return false;
    }

    QStringList errors;
    for (db::ConditionChangeRecord const& c : m_model.changes())
    {
        if (c.kind != db::ConditionChangeKind::Insert
         && c.kind != db::ConditionChangeKind::Update)
            continue;
        if (c.after.conditionTarget > 255)
        {
            // The model stores uint8_t so this is unreachable in
            // practice, but the explicit check documents the schema
            // constraint at the validation boundary.
            errors << QString("ConditionTarget=%1 out of range (0..255)")
                .arg(int(c.after.conditionTarget));
        }
        if (c.after.negativeCondition > 1)
        {
            errors << QString("NegativeCondition=%1 out of range (0..1)")
                .arg(int(c.after.negativeCondition));
        }
    }
    if (!errors.isEmpty())
    {
        outError = errors.join(QStringLiteral("; "));
        return false;
    }
    return true;
}

bool ConditionCommitDialog::applyTransaction(QString& outError)
{
    if (!m_dbClient || !m_dbClient->isConnected())
    {
        outError = tr("not connected to DB");
        return false;
    }
    if (!validateChanges(outError))
        return false;

    auto fail = [&](db::QueryError const& e, char const* phase) -> bool
    {
        outError = QString("[%1] %2 (during %3)")
                   .arg(e.code).arg(QString::fromStdString(e.message))
                   .arg(QString::fromLatin1(phase));
        return false;
    };

    db::QueryError err = m_dbClient->exec("START TRANSACTION");
    if (!err.ok()) return fail(err, "START");

    // Delete first so a re-key (Delete old + Insert new with same logical
    // PK) doesn't blow up on duplicate-key collision.
    for (db::ConditionChangeRecord const& c : m_model.changes())
    {
        if (c.kind == db::ConditionChangeKind::Delete)
        {
            err = m_dbClient->exec(formatDelete(m_dbClient, c.before).toStdString());
            if (!err.ok()) { (void)m_dbClient->exec("ROLLBACK"); return fail(err, "DELETE"); }
        }
    }
    for (db::ConditionChangeRecord const& c : m_model.changes())
    {
        if (c.kind == db::ConditionChangeKind::Update)
        {
            err = m_dbClient->exec(formatUpdate(m_dbClient, c.after).toStdString());
            if (!err.ok()) { (void)m_dbClient->exec("ROLLBACK"); return fail(err, "UPDATE"); }
        }
    }
    for (db::ConditionChangeRecord const& c : m_model.changes())
    {
        if (c.kind == db::ConditionChangeKind::Insert)
        {
            err = m_dbClient->exec(formatInsert(m_dbClient, c.after).toStdString());
            if (!err.ok()) { (void)m_dbClient->exec("ROLLBACK"); return fail(err, "INSERT"); }
        }
    }

    err = m_dbClient->exec("COMMIT");
    if (!err.ok()) { (void)m_dbClient->exec("ROLLBACK"); return fail(err, "COMMIT"); }

    return refetchRows(outError);
}

bool ConditionCommitDialog::refetchRows(QString& outError)
{
    m_committedRows.clear();

    // Collect every PK tuple still alive after commit (Insert.after +
    // Update.after).  Deletes are gone -- nothing to refetch for them.
    // We don't refetch by "scope" the way SmartScript does because
    // conditions has no clean grouping key; per-row PK lookup is
    // correct and bounded by the changeset size.
    using Key = std::tuple<int32_t, uint32_t, int32_t, int32_t, uint32_t,
                           int32_t, uint8_t, uint32_t, uint32_t, uint32_t,
                           QString>;
    std::vector<Key> keys;
    auto keyOf = [](render::Condition const& a) {
        return Key(a.sourceTypeOrReferenceId, a.sourceGroup, a.sourceEntry,
                   a.sourceId, a.elseGroup, a.conditionTypeOrReference,
                   a.conditionTarget, a.conditionValue1, a.conditionValue2,
                   a.conditionValue3, a.conditionStringValue1);
    };
    for (db::ConditionChangeRecord const& c : m_model.changes())
    {
        if (c.kind == db::ConditionChangeKind::Insert
         || c.kind == db::ConditionChangeKind::Update)
        {
            keys.push_back(keyOf(c.after));
        }
    }
    if (keys.empty())
        return true;

    QStringList preds;
    preds.reserve(int(keys.size()));
    for (auto const& [src, grp, ent, sid, els, ct, tgt, v1, v2, v3, sv1] : keys)
    {
        preds << QString(
            "(SourceTypeOrReferenceId=%1 AND SourceGroup=%2 AND SourceEntry=%3 "
            "AND SourceId=%4 AND ElseGroup=%5 AND ConditionTypeOrReference=%6 "
            "AND ConditionTarget=%7 AND ConditionValue1=%8 AND ConditionValue2=%9 "
            "AND ConditionValue3=%10 AND ConditionStringValue1='%11')")
            .arg(int(src)).arg(grp).arg(int(ent)).arg(int(sid)).arg(els)
            .arg(int(ct)).arg(int(tgt)).arg(v1).arg(v2).arg(v3)
            .arg(esc(m_dbClient, sv1));
    }
    std::string const sql =
        "SELECT SourceTypeOrReferenceId, SourceGroup, SourceEntry, SourceId, "
        "       ElseGroup, ConditionTypeOrReference, ConditionTarget, "
        "       ConditionValue1, ConditionValue2, ConditionValue3, "
        "       COALESCE(ConditionStringValue1, ''), "
        "       NegativeCondition, ErrorType, ErrorTextId, "
        "       COALESCE(ScriptName, ''), COALESCE(Comment, '') "
        "FROM conditions WHERE "
        + preds.join(QStringLiteral(" OR ")).toStdString();

    db::QueryResult res;
    db::QueryError const e = m_dbClient->query(sql, res);
    if (!e.ok())
    {
        outError = QString::fromStdString(e.message);
        return false;
    }
    m_committedRows.reserve(res.rowCount());
    for (size_t r = 0; r < res.rowCount(); ++r)
    {
        render::Condition a;
        a.sourceTypeOrReferenceId  = int32_t (res.asInt64 (r, 0).value_or(0));
        a.sourceGroup              = uint32_t(res.asUInt64(r, 1).value_or(0));
        a.sourceEntry              = int32_t (res.asInt64 (r, 2).value_or(0));
        a.sourceId                 = int32_t (res.asInt64 (r, 3).value_or(0));
        a.elseGroup                = uint32_t(res.asUInt64(r, 4).value_or(0));
        a.conditionTypeOrReference = int32_t (res.asInt64 (r, 5).value_or(0));
        a.conditionTarget          = uint8_t (res.asUInt64(r, 6).value_or(0));
        a.conditionValue1          = uint32_t(res.asUInt64(r, 7).value_or(0));
        a.conditionValue2          = uint32_t(res.asUInt64(r, 8).value_or(0));
        a.conditionValue3          = uint32_t(res.asUInt64(r, 9).value_or(0));
        a.conditionStringValue1    = QString::fromStdString(res.cell(r, 10));
        a.negativeCondition        = uint8_t (res.asUInt64(r, 11).value_or(0));
        a.errorType                = uint32_t(res.asUInt64(r, 12).value_or(0));
        a.errorTextId              = uint32_t(res.asUInt64(r, 13).value_or(0));
        a.scriptName               = QString::fromStdString(res.cell(r, 14));
        a.comment                  = QString::fromStdString(res.cell(r, 15));
        m_committedRows.push_back(std::move(a));
    }
    return true;
}

void ConditionCommitDialog::onCommitClicked()
{
    m_commitButton->setEnabled(false);
    m_statusLabel->setText(tr("preparing backup..."));
    QApplication::processEvents();

    if (m_backupCheckbox->isChecked())
    {
        bool const hasBackupable = std::any_of(
            m_model.changes().begin(), m_model.changes().end(),
            [](db::ConditionChangeRecord const& c) {
                return c.kind == db::ConditionChangeKind::Update
                    || c.kind == db::ConditionChangeKind::Delete;
            });
        if (hasBackupable)
        {
            QString backupPath, backupErr;
            if (!writeBackupFile(buildBackupSql(), backupPath, backupErr))
            {
                QMessageBox::critical(this, tr("Backup write failed"),
                    tr("Aborting commit. Could not write backup file:\n%1").arg(backupErr));
                m_statusLabel->setText(tr("aborted: backup failed"));
                m_commitButton->setEnabled(true);
                return;
            }
            m_statusLabel->setText(tr("backup -> %1").arg(backupPath));
            QApplication::processEvents();
        }
    }

    QString applyErr;
    if (!applyTransaction(applyErr))
    {
        QMessageBox::critical(this, tr("Commit failed"),
            tr("Transaction rolled back.\n%1").arg(applyErr));
        m_statusLabel->setText(tr("failed: %1").arg(applyErr));
        m_commitButton->setEnabled(true);
        return;
    }
    m_statusLabel->setText(tr("committed."));
    accept();
}

} // namespace world_editor::app
