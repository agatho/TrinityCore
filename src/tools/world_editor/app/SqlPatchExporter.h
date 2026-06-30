/*
 * SqlPatchExporter - serialise every pending edit across the editor's
 * models into a single review-ready .sql patch file.
 *
 * Mirrors the per-row SQL emitted by the per-model commit dialogs
 * (CommitDialog / SpawnCommitDialog / WaypointCommitDialog /
 * AreatriggerCommitDialog / GraveyardCommitDialog / SmartScriptCommitDialog /
 * ConditionCommitDialog) but does NOT execute the SQL -- it just writes
 * it to disk.  The full transaction is wrapped in
 * START TRANSACTION ... COMMIT so the operator (or CI) can pipe the
 * file straight into mysql.
 *
 * Models are passed as borrowed const pointers; any of them may be null
 * (e.g. when DB isn't connected yet) and is simply skipped.
 *
 * Threading: not thread-safe.  Intended to be called from the UI thread.
 */

#pragma once

#include <QString>

#include <cstddef>

namespace world_editor::db
{
class MySqlClient;
class AnnotationModel;
class SpawnModel;
class WaypointModel;
class AreatriggerModel;
class GraveyardModel;
class SmartScriptModel;
class ConditionsModel;
}

namespace world_editor::app
{

class SqlPatchExporter
{
public:
    struct Result
    {
        size_t  statements = 0;     // count of INSERT/UPDATE/DELETE lines emitted
        QString errMsg;             // empty on success
    };

    // The MySqlClient is borrowed only for escapeString() - the exporter
    // never runs a query.  It may be null; without it strings are emitted
    // unescaped (operator must hand-verify before applying).
    Result exportAll(QString const& filepath,
                     db::MySqlClient* dbClient,
                     db::AnnotationModel const* annot,
                     db::SpawnModel const* spawn,
                     db::WaypointModel const* waypoint,
                     db::AreatriggerModel const* atr,
                     db::GraveyardModel const* gy,
                     db::SmartScriptModel const* sai,
                     db::ConditionsModel const* cond);
};

} // namespace world_editor::app
