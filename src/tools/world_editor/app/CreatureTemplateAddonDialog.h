/*
 * CreatureTemplateAddonDialog - modal editor for one `creature_template_addon` row.
 *
 * Schema (modern TC):
 *
 *   creature_template_addon(
 *       entry                  INT      UNSIGNED PRIMARY KEY,  -- creature_template.entry
 *       PathId                 INT      UNSIGNED,              -- waypoint_path.PathID
 *       mount                  INT      UNSIGNED,              -- mount creature_template.entry
 *       StandState             TINYINT  UNSIGNED,
 *       AnimTier               TINYINT  UNSIGNED,
 *       VisFlags               TINYINT  UNSIGNED,
 *       SheathState            TINYINT  UNSIGNED,
 *       PvpFlags               TINYINT  UNSIGNED,
 *       emote                  INT      UNSIGNED,
 *       AiAnimKit              SMALLINT UNSIGNED,
 *       MovementAnimKit        SMALLINT UNSIGNED,
 *       MeleeAnimKit           SMALLINT UNSIGNED,
 *       VisibilityDistanceType TINYINT  UNSIGNED,
 *       auras                  TEXT)                            -- space-separated SpellID list
 *
 * The row carries the per-creature_template defaults that apply to every spawn
 * unless overridden by a per-spawn `creature_addon`.  Editing here changes the
 * defaults for ALL spawns of that template that lack their own addon row.
 *
 * Schema-tolerant: we probe INFORMATION_SCHEMA.COLUMNS for the canonical column
 * names (case-insensitive) so legacy forks (PvPFlags vs PvpFlags, aiAnimKit vs
 * AiAnimKit, ...) still load.  The first match wins; the dialog binds whichever
 * name MySQL actually exposes.
 */

#pragma once

#include <QDialog>
#include <QString>

#include <cstdint>

class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;

namespace world_editor::db { class MySqlClient; }

namespace world_editor::app
{

class CreatureTemplateAddonDialog final : public QDialog
{
    Q_OBJECT

public:
    CreatureTemplateAddonDialog(db::MySqlClient* dbClient,
                                QString const& worldDbName,
                                QWidget* parent = nullptr);

private slots:
    void onLoad();
    void onSave();
    void onDelete();
    void onRefresh();

private:
    // Probe INFORMATION_SCHEMA once: settle on the actual column names so we
    // emit SQL whose identifiers match the live schema (case + spelling).
    void detectSchema();

    // Resolve creature_template.name1 / .name for the loaded entry; "(unknown)"
    // fallback when no template row exists.
    void refreshCreatureName(uint32_t entry);

    // Load the existing creature_template_addon row into the form, or zero
    // every spinbox + clear the auras field if no row exists yet.
    void loadAddonRow();

    // Wrap `sqls` in START TRANSACTION / COMMIT, ROLLBACK + QMessageBox on the
    // first error.  Returns true on COMMIT.
    bool runInTransaction(QString const& sql, QString const& description);

    // Clears every form field.  Called when no row exists yet so Save acts as
    // an INSERT.
    void clearForm();

    db::MySqlClient* m_db = nullptr;
    QString          m_worldDb;
    uint32_t         m_loadedEntry = 0;
    bool             m_rowExists   = false;
    bool             m_schemaDetected = false;

    // Resolved column names (modern TC defaults).  detectSchema() rebinds these
    // case-insensitively against INFORMATION_SCHEMA.
    QString m_colPathId          = QStringLiteral("PathId");
    QString m_colMount           = QStringLiteral("mount");
    QString m_colStandState      = QStringLiteral("StandState");
    QString m_colAnimTier        = QStringLiteral("AnimTier");
    QString m_colVisFlags        = QStringLiteral("VisFlags");
    QString m_colSheathState     = QStringLiteral("SheathState");
    QString m_colPvpFlags        = QStringLiteral("PvpFlags");
    QString m_colEmote           = QStringLiteral("emote");
    QString m_colAiAnimKit       = QStringLiteral("AiAnimKit");
    QString m_colMovementAnimKit = QStringLiteral("MovementAnimKit");
    QString m_colMeleeAnimKit    = QStringLiteral("MeleeAnimKit");
    QString m_colVisDistType     = QStringLiteral("VisibilityDistanceType");
    QString m_colAuras           = QStringLiteral("auras");

    QSpinBox*  m_entrySpin       = nullptr;
    QPushButton* m_loadBtn       = nullptr;
    QLabel*    m_creatureLbl     = nullptr;

    QSpinBox*  m_pathIdSpin          = nullptr;
    QSpinBox*  m_mountSpin           = nullptr;
    QSpinBox*  m_standStateSpin      = nullptr;
    QSpinBox*  m_animTierSpin        = nullptr;
    QSpinBox*  m_visFlagsSpin        = nullptr;
    QSpinBox*  m_sheathStateSpin     = nullptr;
    QSpinBox*  m_pvpFlagsSpin        = nullptr;
    QSpinBox*  m_emoteSpin           = nullptr;
    QSpinBox*  m_aiAnimKitSpin       = nullptr;
    QSpinBox*  m_movementAnimKitSpin = nullptr;
    QSpinBox*  m_meleeAnimKitSpin    = nullptr;
    QSpinBox*  m_visDistTypeSpin     = nullptr;
    QLineEdit* m_aurasEdit           = nullptr;

    QPushButton* m_saveBtn      = nullptr;
    QPushButton* m_deleteBtn    = nullptr;
    QPushButton* m_refreshBtn   = nullptr;

    QLabel*      m_statusLabel  = nullptr;
};

} // namespace world_editor::app
