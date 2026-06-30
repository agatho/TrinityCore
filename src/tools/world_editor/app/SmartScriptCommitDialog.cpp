#include "SmartScriptCommitDialog.h"

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
#include <cstdio>
#include <set>

namespace world_editor::app
{

namespace
{

QString esc(db::MySqlClient* c, QString const& v)
{
    if (!c) return v;
    return QString::fromStdString(c->escapeString(v.toStdString()));
}

// Render a nullable string column either as 'escaped' or NULL.
QString nullableLit(db::MySqlClient* c, QString const& v, bool isNull)
{
    if (isNull) return QStringLiteral("NULL");
    return QStringLiteral("'%1'").arg(esc(c, v));
}

// Render assignment value (foo=...) for the same column in UPDATE.
QString nullableAssign(db::MySqlClient* c, QString const& v, bool isNull)
{
    if (isNull) return QStringLiteral("NULL");
    return QStringLiteral("'%1'").arg(esc(c, v));
}

QString formatWhere(render::SmartScript const& a)
{
    return QString("entryorguid=%1 AND source_type=%2 AND id=%3 AND link=%4")
        .arg(qlonglong(a.entryorguid))
        .arg(int(a.sourceType))
        .arg(int(a.id))
        .arg(int(a.link));
}

QString formatUpdate(db::MySqlClient* c, render::SmartScript const& a)
{
    // Composite-PK UPDATE.  The PK columns themselves are part of the
    // WHERE clause and not in the SET list -- if the operator wants to
    // re-key a row they must Delete + Add.
    return QString(
        "UPDATE smart_scripts SET "
        "Difficulties='%1', "
        "event_type=%2, event_phase_mask=%3, event_chance=%4, event_flags=%5, "
        "event_param1=%6, event_param2=%7, event_param3=%8, event_param4=%9, event_param5=%10, "
        "event_param_string='%11', "
        "action_type=%12, "
        "action_param1=%13, action_param2=%14, action_param3=%15, action_param4=%16, "
        "action_param5=%17, action_param6=%18, action_param7=%19, "
        "action_param_string=%20, "
        "target_type=%21, "
        "target_param1=%22, target_param2=%23, target_param3=%24, target_param4=%25, "
        "target_param_string=%26, "
        "target_x=%27, target_y=%28, target_z=%29, target_o=%30, "
        "comment='%31' "
        "WHERE %32;")
        .arg(esc(c, a.difficulties))
        .arg(int(a.eventType)).arg(int(a.eventPhaseMask))
        .arg(int(a.eventChance)).arg(int(a.eventFlags))
        .arg(a.eventParam1).arg(a.eventParam2).arg(a.eventParam3)
        .arg(a.eventParam4).arg(a.eventParam5)
        .arg(esc(c, a.eventParamString))
        .arg(int(a.actionType))
        .arg(a.actionParam1).arg(a.actionParam2).arg(a.actionParam3).arg(a.actionParam4)
        .arg(a.actionParam5).arg(a.actionParam6).arg(a.actionParam7)
        .arg(nullableAssign(c, a.actionParamString, a.actionParamStringIsNull))
        .arg(int(a.targetType))
        .arg(a.targetParam1).arg(a.targetParam2).arg(a.targetParam3).arg(a.targetParam4)
        .arg(nullableAssign(c, a.targetParamString, a.targetParamStringIsNull))
        .arg(a.targetX, 0, 'f', 4).arg(a.targetY, 0, 'f', 4)
        .arg(a.targetZ, 0, 'f', 4).arg(a.targetO, 0, 'f', 4)
        .arg(esc(c, a.comment))
        .arg(formatWhere(a));
}

QString formatInsert(db::MySqlClient* c, render::SmartScript const& a)
{
    return QString(
        "INSERT INTO smart_scripts "
        "(entryorguid, source_type, id, link, Difficulties, "
        " event_type, event_phase_mask, event_chance, event_flags, "
        " event_param1, event_param2, event_param3, event_param4, event_param5, "
        " event_param_string, "
        " action_type, action_param1, action_param2, action_param3, action_param4, "
        " action_param5, action_param6, action_param7, action_param_string, "
        " target_type, target_param1, target_param2, target_param3, target_param4, "
        " target_param_string, target_x, target_y, target_z, target_o, comment) "
        "VALUES (%1, %2, %3, %4, '%5', "
        "        %6, %7, %8, %9, "
        "        %10, %11, %12, %13, %14, "
        "        '%15', "
        "        %16, %17, %18, %19, %20, "
        "        %21, %22, %23, %24, "
        "        %25, %26, %27, %28, %29, "
        "        %30, %31, %32, %33, %34, '%35');")
        .arg(qlonglong(a.entryorguid))
        .arg(int(a.sourceType))
        .arg(int(a.id))
        .arg(int(a.link))
        .arg(esc(c, a.difficulties))
        .arg(int(a.eventType)).arg(int(a.eventPhaseMask))
        .arg(int(a.eventChance)).arg(int(a.eventFlags))
        .arg(a.eventParam1).arg(a.eventParam2).arg(a.eventParam3)
        .arg(a.eventParam4).arg(a.eventParam5)
        .arg(esc(c, a.eventParamString))
        .arg(int(a.actionType))
        .arg(a.actionParam1).arg(a.actionParam2).arg(a.actionParam3).arg(a.actionParam4)
        .arg(a.actionParam5).arg(a.actionParam6).arg(a.actionParam7)
        .arg(nullableLit(c, a.actionParamString, a.actionParamStringIsNull))
        .arg(int(a.targetType))
        .arg(a.targetParam1).arg(a.targetParam2).arg(a.targetParam3).arg(a.targetParam4)
        .arg(nullableLit(c, a.targetParamString, a.targetParamStringIsNull))
        .arg(a.targetX, 0, 'f', 4).arg(a.targetY, 0, 'f', 4)
        .arg(a.targetZ, 0, 'f', 4).arg(a.targetO, 0, 'f', 4)
        .arg(esc(c, a.comment));
}

QString formatDelete(render::SmartScript const& a)
{
    return QString("DELETE FROM smart_scripts WHERE %1;").arg(formatWhere(a));
}

QString formatBackup(db::MySqlClient* c, render::SmartScript const& a)
{
    // Re-applyable full-row INSERT...ON DUPLICATE KEY UPDATE.  Because
    // the PK is composite, the ON DUPLICATE clause covers it cleanly
    // (every non-key column listed in the SET).
    return QString(
        "INSERT INTO smart_scripts "
        "(entryorguid, source_type, id, link, Difficulties, "
        " event_type, event_phase_mask, event_chance, event_flags, "
        " event_param1, event_param2, event_param3, event_param4, event_param5, "
        " event_param_string, "
        " action_type, action_param1, action_param2, action_param3, action_param4, "
        " action_param5, action_param6, action_param7, action_param_string, "
        " target_type, target_param1, target_param2, target_param3, target_param4, "
        " target_param_string, target_x, target_y, target_z, target_o, comment) "
        "VALUES (%1, %2, %3, %4, '%5', "
        "        %6, %7, %8, %9, "
        "        %10, %11, %12, %13, %14, "
        "        '%15', "
        "        %16, %17, %18, %19, %20, "
        "        %21, %22, %23, %24, "
        "        %25, %26, %27, %28, %29, "
        "        %30, %31, %32, %33, %34, '%35') "
        "ON DUPLICATE KEY UPDATE "
        "Difficulties=VALUES(Difficulties), "
        "event_type=VALUES(event_type), event_phase_mask=VALUES(event_phase_mask), "
        "event_chance=VALUES(event_chance), event_flags=VALUES(event_flags), "
        "event_param1=VALUES(event_param1), event_param2=VALUES(event_param2), "
        "event_param3=VALUES(event_param3), event_param4=VALUES(event_param4), "
        "event_param5=VALUES(event_param5), "
        "event_param_string=VALUES(event_param_string), "
        "action_type=VALUES(action_type), "
        "action_param1=VALUES(action_param1), action_param2=VALUES(action_param2), "
        "action_param3=VALUES(action_param3), action_param4=VALUES(action_param4), "
        "action_param5=VALUES(action_param5), action_param6=VALUES(action_param6), "
        "action_param7=VALUES(action_param7), "
        "action_param_string=VALUES(action_param_string), "
        "target_type=VALUES(target_type), "
        "target_param1=VALUES(target_param1), target_param2=VALUES(target_param2), "
        "target_param3=VALUES(target_param3), target_param4=VALUES(target_param4), "
        "target_param_string=VALUES(target_param_string), "
        "target_x=VALUES(target_x), target_y=VALUES(target_y), "
        "target_z=VALUES(target_z), target_o=VALUES(target_o), "
        "comment=VALUES(comment);")
        .arg(qlonglong(a.entryorguid))
        .arg(int(a.sourceType))
        .arg(int(a.id))
        .arg(int(a.link))
        .arg(esc(c, a.difficulties))
        .arg(int(a.eventType)).arg(int(a.eventPhaseMask))
        .arg(int(a.eventChance)).arg(int(a.eventFlags))
        .arg(a.eventParam1).arg(a.eventParam2).arg(a.eventParam3)
        .arg(a.eventParam4).arg(a.eventParam5)
        .arg(esc(c, a.eventParamString))
        .arg(int(a.actionType))
        .arg(a.actionParam1).arg(a.actionParam2).arg(a.actionParam3).arg(a.actionParam4)
        .arg(a.actionParam5).arg(a.actionParam6).arg(a.actionParam7)
        .arg(nullableLit(c, a.actionParamString, a.actionParamStringIsNull))
        .arg(int(a.targetType))
        .arg(a.targetParam1).arg(a.targetParam2).arg(a.targetParam3).arg(a.targetParam4)
        .arg(nullableLit(c, a.targetParamString, a.targetParamStringIsNull))
        .arg(a.targetX, 0, 'f', 4).arg(a.targetY, 0, 'f', 4)
        .arg(a.targetZ, 0, 'f', 4).arg(a.targetO, 0, 'f', 4)
        .arg(esc(c, a.comment));
}

// Collect the union of (entryorguid, source_type) scopes from the
// changeset (Insert.after / Update.after / Delete.before).  These are
// the rows we refetch after commit.
std::vector<std::pair<int64_t, uint8_t>> collectScopes(db::SmartScriptModel const& model)
{
    std::set<std::pair<int64_t, uint8_t>> seen;
    for (db::SmartScriptChangeRecord const& c : model.changes())
    {
        if (c.kind == db::SmartScriptChangeKind::Delete)
            seen.emplace(c.before.entryorguid, c.before.sourceType);
        else if (c.kind == db::SmartScriptChangeKind::Insert
              || c.kind == db::SmartScriptChangeKind::Update)
            seen.emplace(c.after.entryorguid, c.after.sourceType);
    }
    return std::vector<std::pair<int64_t, uint8_t>>(seen.begin(), seen.end());
}

} // namespace

SmartScriptCommitDialog::SmartScriptCommitDialog(db::MySqlClient* dbClient,
                                                 db::SmartScriptModel const& model,
                                                 QWidget* parent)
    : QDialog(parent), m_dbClient(dbClient), m_model(model)
{
    setWindowTitle(tr("Commit smart_scripts changes"));
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

    connect(m_commitButton, &QPushButton::clicked, this, &SmartScriptCommitDialog::onCommitClicked);
    connect(buttons,        &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto* outer = new QVBoxLayout(this);
    outer->addWidget(new QLabel(tr(
        "The following SQL will run inside a single transaction. Review before committing.")));
    outer->addWidget(m_sqlPreview, 1);
    outer->addWidget(m_backupCheckbox);
    outer->addWidget(m_statusLabel);
    outer->addWidget(buttons);
}

QString SmartScriptCommitDialog::buildSqlPreview() const
{
    QString out;
    QTextStream ts(&out);
    ts << "BEGIN;\n\n";

    size_t ins = 0, upd = 0, del = 0;
    for (db::SmartScriptChangeRecord const& c : m_model.changes())
    {
        if      (c.kind == db::SmartScriptChangeKind::Insert) ++ins;
        else if (c.kind == db::SmartScriptChangeKind::Update) ++upd;
        else if (c.kind == db::SmartScriptChangeKind::Delete) ++del;
    }

    if (del > 0)
    {
        ts << "-- " << del << " DELETE(s)\n";
        for (db::SmartScriptChangeRecord const& c : m_model.changes())
            if (c.kind == db::SmartScriptChangeKind::Delete)
                ts << formatDelete(c.before) << "\n";
        ts << "\n";
    }
    if (upd > 0)
    {
        ts << "-- " << upd << " UPDATE(s)\n";
        for (db::SmartScriptChangeRecord const& c : m_model.changes())
            if (c.kind == db::SmartScriptChangeKind::Update)
                ts << formatUpdate(m_dbClient, c.after) << "\n";
        ts << "\n";
    }
    if (ins > 0)
    {
        ts << "-- " << ins << " INSERT(s)\n";
        for (db::SmartScriptChangeRecord const& c : m_model.changes())
            if (c.kind == db::SmartScriptChangeKind::Insert)
                ts << formatInsert(m_dbClient, c.after) << "\n";
        ts << "\n";
    }

    ts << "COMMIT;\n";
    return out;
}

QString SmartScriptCommitDialog::buildBackupSql() const
{
    QString out;
    QTextStream ts(&out);
    ts << "-- world_editor smart_scripts backup\n";
    ts << "-- generated: " << QDateTime::currentDateTimeUtc().toString(Qt::ISODate) << "Z\n\n";
    ts << "BEGIN;\n\n";
    for (db::SmartScriptChangeRecord const& c : m_model.changes())
    {
        if (c.kind == db::SmartScriptChangeKind::Update
         || c.kind == db::SmartScriptChangeKind::Delete)
        {
            ts << formatBackup(m_dbClient, c.before) << "\n";
        }
    }
    ts << "\nCOMMIT;\n";
    return out;
}

bool SmartScriptCommitDialog::writeBackupFile(QString const& sql, QString& outPath, QString& outError) const
{
    QString const dirPath = QStringLiteral("editor_backups");
    QDir().mkpath(dirPath);
    QString const stamp = QDateTime::currentDateTimeUtc().toString(QStringLiteral("yyyyMMdd_HHmmss"));
    QString const fileName = QStringLiteral("%1/%2_smart_scripts.sql").arg(dirPath).arg(stamp);
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

bool SmartScriptCommitDialog::validateChanges(QString& outError) const
{
    if (!m_dbClient || !m_dbClient->isConnected())
    {
        outError = tr("not connected to DB");
        return false;
    }

    // Group required references by (source_type, polarity).  For each
    // Insert/Update with source_type in {0,1}, an entryorguid > 0
    // must hit the corresponding *_template.entry and < 0 must hit
    // |entryorguid| in the corresponding live spawn table's guid.
    std::set<int64_t> creatureTemplateIds;
    std::set<int64_t> creatureGuids;
    std::set<int64_t> gameobjectTemplateIds;
    std::set<int64_t> gameobjectGuids;
    QStringList rangeErrors;

    auto recordRow = [&](render::SmartScript const& a) {
        if (a.eventChance > 100)
        {
            rangeErrors << QString("event_chance=%1 out of range (0..100) for (%2,%3,%4,%5)")
                .arg(int(a.eventChance))
                .arg(qlonglong(a.entryorguid))
                .arg(int(a.sourceType)).arg(int(a.id)).arg(int(a.link));
        }
        if (a.sourceType == 0)
        {
            if (a.entryorguid > 0) creatureTemplateIds.insert(a.entryorguid);
            else if (a.entryorguid < 0) creatureGuids.insert(-a.entryorguid);
            // entryorguid == 0 with source_type=0 is illegal (no row).
            else
            {
                rangeErrors << QString("entryorguid=0 with source_type=0 is invalid "
                                       "(id=%1, link=%2)").arg(int(a.id)).arg(int(a.link));
            }
        }
        else if (a.sourceType == 1)
        {
            if (a.entryorguid > 0) gameobjectTemplateIds.insert(a.entryorguid);
            else if (a.entryorguid < 0) gameobjectGuids.insert(-a.entryorguid);
            else
            {
                rangeErrors << QString("entryorguid=0 with source_type=1 is invalid "
                                       "(id=%1, link=%2)").arg(int(a.id)).arg(int(a.link));
            }
        }
        // source_type=9 (action list): no FK validation.
    };

    for (db::SmartScriptChangeRecord const& c : m_model.changes())
    {
        if (c.kind == db::SmartScriptChangeKind::Insert
         || c.kind == db::SmartScriptChangeKind::Update)
        {
            recordRow(c.after);
        }
    }

    if (!rangeErrors.isEmpty())
    {
        outError = rangeErrors.join(QStringLiteral("; "));
        return false;
    }

    auto inList = [](std::set<int64_t> const& s) -> QString {
        QStringList parts;
        parts.reserve(int(s.size()));
        for (int64_t v : s) parts << QString::number(qlonglong(v));
        return parts.join(QStringLiteral(","));
    };

    auto probe = [&](char const* sqlFmt, std::set<int64_t> const& need,
                     char const* what, QString& err) -> bool
    {
        if (need.empty()) return true;
        std::string const sql = QString(sqlFmt).arg(inList(need)).toStdString();
        db::QueryResult res;
        auto const e = m_dbClient->query(sql, res);
        if (!e.ok())
        {
            err = QString("%1 probe failed: %2")
                  .arg(QString::fromLatin1(what))
                  .arg(QString::fromStdString(e.message));
            return false;
        }
        std::set<int64_t> found;
        for (size_t r = 0; r < res.rowCount(); ++r)
            found.insert(res.asInt64(r, 0).value_or(0));
        std::vector<int64_t> missing;
        std::set_difference(need.begin(), need.end(),
                            found.begin(), found.end(),
                            std::back_inserter(missing));
        if (!missing.empty())
        {
            QStringList ms;
            for (int64_t v : missing) ms << QString::number(qlonglong(v));
            err = QString("missing %1: %2")
                  .arg(QString::fromLatin1(what))
                  .arg(ms.join(QStringLiteral(", ")));
            return false;
        }
        return true;
    };

    if (!probe("SELECT entry FROM creature_template WHERE entry IN (%1)",
               creatureTemplateIds, "creature_template.entry", outError))
        return false;
    if (!probe("SELECT guid FROM creature WHERE guid IN (%1)",
               creatureGuids, "creature.guid", outError))
        return false;
    if (!probe("SELECT entry FROM gameobject_template WHERE entry IN (%1)",
               gameobjectTemplateIds, "gameobject_template.entry", outError))
        return false;
    if (!probe("SELECT guid FROM gameobject WHERE guid IN (%1)",
               gameobjectGuids, "gameobject.guid", outError))
        return false;
    return true;
}

bool SmartScriptCommitDialog::applyTransaction(QString& outError)
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

    for (db::SmartScriptChangeRecord const& c : m_model.changes())
    {
        if (c.kind == db::SmartScriptChangeKind::Delete)
        {
            err = m_dbClient->exec(formatDelete(c.before).toStdString());
            if (!err.ok()) { (void)m_dbClient->exec("ROLLBACK"); return fail(err, "DELETE"); }
        }
    }
    for (db::SmartScriptChangeRecord const& c : m_model.changes())
    {
        if (c.kind == db::SmartScriptChangeKind::Update)
        {
            err = m_dbClient->exec(formatUpdate(m_dbClient, c.after).toStdString());
            if (!err.ok()) { (void)m_dbClient->exec("ROLLBACK"); return fail(err, "UPDATE"); }
        }
    }
    for (db::SmartScriptChangeRecord const& c : m_model.changes())
    {
        if (c.kind == db::SmartScriptChangeKind::Insert)
        {
            err = m_dbClient->exec(formatInsert(m_dbClient, c.after).toStdString());
            if (!err.ok()) { (void)m_dbClient->exec("ROLLBACK"); return fail(err, "INSERT"); }
        }
    }

    err = m_dbClient->exec("COMMIT");
    if (!err.ok()) { (void)m_dbClient->exec("ROLLBACK"); return fail(err, "COMMIT"); }

    return refetchRows(outError);
}

bool SmartScriptCommitDialog::refetchRows(QString& outError)
{
    m_committedRows.clear();
    m_refetchedScopes = collectScopes(m_model);
    if (m_refetchedScopes.empty())
        return true;

    // Build a single SELECT with OR'd (entryorguid=? AND source_type=?)
    // predicates so the dock can swap baseline atomically.
    QStringList preds;
    preds.reserve(int(m_refetchedScopes.size()));
    for (auto const& [eog, st] : m_refetchedScopes)
    {
        preds << QString("(entryorguid=%1 AND source_type=%2)")
                 .arg(qlonglong(eog)).arg(int(st));
    }
    std::string const sql =
        "SELECT entryorguid, source_type, id, link, "
        "       COALESCE(Difficulties, ''), "
        "       event_type, event_phase_mask, event_chance, event_flags, "
        "       event_param1, event_param2, event_param3, event_param4, event_param5, "
        "       COALESCE(event_param_string, ''), "
        "       action_type, "
        "       action_param1, action_param2, action_param3, action_param4, "
        "       action_param5, action_param6, action_param7, "
        "       action_param_string, "
        "       target_type, "
        "       target_param1, target_param2, target_param3, target_param4, "
        "       target_param_string, "
        "       target_x, target_y, target_z, target_o, "
        "       COALESCE(comment, '') "
        "FROM smart_scripts WHERE "
        + preds.join(QStringLiteral(" OR ")).toStdString()
        + " ORDER BY entryorguid, source_type, id, link";

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
        render::SmartScript a;
        a.entryorguid    = res.asInt64 (r, 0).value_or(0);
        a.sourceType     = uint8_t (res.asUInt64(r, 1).value_or(0));
        a.id             = uint16_t(res.asUInt64(r, 2).value_or(0));
        a.link           = uint16_t(res.asUInt64(r, 3).value_or(0));
        a.difficulties   = QString::fromStdString(res.cell(r, 4));
        a.eventType      = uint8_t (res.asUInt64(r, 5).value_or(0));
        a.eventPhaseMask = uint16_t(res.asUInt64(r, 6).value_or(0));
        a.eventChance    = uint8_t (res.asUInt64(r, 7).value_or(0));
        a.eventFlags     = uint16_t(res.asUInt64(r, 8).value_or(0));
        a.eventParam1    = uint32_t(res.asUInt64(r, 9).value_or(0));
        a.eventParam2    = uint32_t(res.asUInt64(r, 10).value_or(0));
        a.eventParam3    = uint32_t(res.asUInt64(r, 11).value_or(0));
        a.eventParam4    = uint32_t(res.asUInt64(r, 12).value_or(0));
        a.eventParam5    = uint32_t(res.asUInt64(r, 13).value_or(0));
        a.eventParamString = QString::fromStdString(res.cell(r, 14));
        a.actionType     = uint8_t (res.asUInt64(r, 15).value_or(0));
        a.actionParam1   = uint32_t(res.asUInt64(r, 16).value_or(0));
        a.actionParam2   = uint32_t(res.asUInt64(r, 17).value_or(0));
        a.actionParam3   = uint32_t(res.asUInt64(r, 18).value_or(0));
        a.actionParam4   = uint32_t(res.asUInt64(r, 19).value_or(0));
        a.actionParam5   = uint32_t(res.asUInt64(r, 20).value_or(0));
        a.actionParam6   = uint32_t(res.asUInt64(r, 21).value_or(0));
        a.actionParam7   = uint32_t(res.asUInt64(r, 22).value_or(0));
        a.actionParamStringIsNull = res.isNull(r, 23);
        a.actionParamString = a.actionParamStringIsNull
            ? QString{} : QString::fromStdString(res.cell(r, 23));
        a.targetType     = uint8_t (res.asUInt64(r, 24).value_or(0));
        a.targetParam1   = uint32_t(res.asUInt64(r, 25).value_or(0));
        a.targetParam2   = uint32_t(res.asUInt64(r, 26).value_or(0));
        a.targetParam3   = uint32_t(res.asUInt64(r, 27).value_or(0));
        a.targetParam4   = uint32_t(res.asUInt64(r, 28).value_or(0));
        a.targetParamStringIsNull = res.isNull(r, 29);
        a.targetParamString = a.targetParamStringIsNull
            ? QString{} : QString::fromStdString(res.cell(r, 29));
        a.targetX        = float(res.asDouble(r, 30).value_or(0.0));
        a.targetY        = float(res.asDouble(r, 31).value_or(0.0));
        a.targetZ        = float(res.asDouble(r, 32).value_or(0.0));
        a.targetO        = float(res.asDouble(r, 33).value_or(0.0));
        a.comment        = QString::fromStdString(res.cell(r, 34));
        m_committedRows.push_back(std::move(a));
    }
    return true;
}

void SmartScriptCommitDialog::onCommitClicked()
{
    m_commitButton->setEnabled(false);
    m_statusLabel->setText(tr("preparing backup..."));
    QApplication::processEvents();

    if (m_backupCheckbox->isChecked())
    {
        bool const hasBackupable = std::any_of(
            m_model.changes().begin(), m_model.changes().end(),
            [](db::SmartScriptChangeRecord const& c) {
                return c.kind == db::SmartScriptChangeKind::Update
                    || c.kind == db::SmartScriptChangeKind::Delete;
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
