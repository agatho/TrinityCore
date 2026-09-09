#include "CreatureTemplateAddonDialog.h"

#include "../db/MySqlClient.h"

#include <QApplication>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

#include <cstdio>
#include <limits>

namespace world_editor::app
{

namespace
{

// Escape backslashes + single quotes for safe inclusion in a single-quoted
// MySQL string literal.  auras is the only free-text column we accept.
QString escapeSqlString(QString const& in)
{
    QString out;
    out.reserve(in.size() + 8);
    for (QChar c : in)
    {
        if (c == QLatin1Char('\\') || c == QLatin1Char('\'')) out.append(QLatin1Char('\\'));
        out.append(c);
    }
    return out;
}

} // namespace

CreatureTemplateAddonDialog::CreatureTemplateAddonDialog(db::MySqlClient* dbClient,
                                                         QString const& worldDbName,
                                                         QWidget* parent)
    : QDialog(parent), m_db(dbClient), m_worldDb(worldDbName)
{
    setWindowTitle(tr("Creature template addon editor"));
    setModal(true);
    resize(640, 620);

    auto* outer = new QVBoxLayout(this);

    // -- Top filter row: creature entry spinner + Load + name label ----
    auto* filterRow = new QHBoxLayout;
    filterRow->addWidget(new QLabel(tr("Creature entry:"), this));
    m_entrySpin = new QSpinBox(this);
    m_entrySpin->setRange(0, std::numeric_limits<int>::max());
    m_entrySpin->setValue(0);
    filterRow->addWidget(m_entrySpin);
    m_loadBtn = new QPushButton(tr("Load"), this);
    filterRow->addWidget(m_loadBtn);
    m_creatureLbl = new QLabel(tr("(no creature loaded)"), this);
    m_creatureLbl->setTextInteractionFlags(Qt::TextSelectableByMouse);
    filterRow->addWidget(m_creatureLbl, 1);
    outer->addLayout(filterRow);

    // -- Form: every editable column ---------------------------------
    auto* formBox = new QGroupBox(tr("creature_template_addon"), this);
    auto* form = new QFormLayout(formBox);

    auto makeUSpin = [&](int maxVal) {
        auto* s = new QSpinBox(formBox);
        s->setRange(0, maxVal);
        s->setValue(0);
        return s;
    };

    // INT  UNSIGNED -> clamp to int_max to fit QSpinBox's 32-bit signed range
    // (good enough for every observed mount / emote / pathid value in retail).
    int const intMax      = std::numeric_limits<int>::max();
    int const tinyMax     = 255;
    int const smallMax    = 65535;
    // VisibilityDistanceType is only ever 0..4 in modern TC but we leave the
    // tinyint range so unknown values still load.

    m_pathIdSpin          = makeUSpin(intMax);
    m_mountSpin           = makeUSpin(intMax);
    m_standStateSpin      = makeUSpin(tinyMax);
    m_animTierSpin        = makeUSpin(tinyMax);
    m_visFlagsSpin        = makeUSpin(tinyMax);
    m_sheathStateSpin     = makeUSpin(tinyMax);
    m_pvpFlagsSpin        = makeUSpin(tinyMax);
    m_emoteSpin           = makeUSpin(intMax);
    m_aiAnimKitSpin       = makeUSpin(smallMax);
    m_movementAnimKitSpin = makeUSpin(smallMax);
    m_meleeAnimKitSpin    = makeUSpin(smallMax);
    m_visDistTypeSpin     = makeUSpin(tinyMax);
    m_aurasEdit           = new QLineEdit(formBox);
    m_aurasEdit->setPlaceholderText(tr("space-separated SpellID list, e.g. \"12345 67890\""));

    form->addRow(tr("PathId:"),                m_pathIdSpin);
    form->addRow(tr("mount (NPC entry):"),     m_mountSpin);
    form->addRow(tr("StandState:"),            m_standStateSpin);
    form->addRow(tr("AnimTier:"),              m_animTierSpin);
    form->addRow(tr("VisFlags:"),              m_visFlagsSpin);
    form->addRow(tr("SheathState:"),           m_sheathStateSpin);
    form->addRow(tr("PvpFlags:"),              m_pvpFlagsSpin);
    form->addRow(tr("emote (Emote.db2):"),     m_emoteSpin);
    form->addRow(tr("AiAnimKit:"),             m_aiAnimKitSpin);
    form->addRow(tr("MovementAnimKit:"),       m_movementAnimKitSpin);
    form->addRow(tr("MeleeAnimKit:"),          m_meleeAnimKitSpin);
    form->addRow(tr("VisibilityDistanceType:"),m_visDistTypeSpin);
    form->addRow(tr("auras:"),                 m_aurasEdit);

    outer->addWidget(formBox, 1);

    // -- Action buttons + status -------------------------------------
    auto* btnRow = new QHBoxLayout;
    m_saveBtn    = new QPushButton(tr("Save"), this);
    m_deleteBtn  = new QPushButton(tr("Delete addon"), this);
    m_refreshBtn = new QPushButton(tr("Refresh"), this);
    m_saveBtn   ->setEnabled(false);
    m_deleteBtn ->setEnabled(false);
    m_refreshBtn->setEnabled(false);
    btnRow->addWidget(m_saveBtn);
    btnRow->addWidget(m_deleteBtn);
    btnRow->addWidget(m_refreshBtn);
    btnRow->addStretch(1);
    outer->addLayout(btnRow);

    m_statusLabel = new QLabel(tr("Enter a creature entry and press Load."), this);
    outer->addWidget(m_statusLabel);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    outer->addWidget(buttons);

    connect(m_loadBtn,    &QPushButton::clicked, this, &CreatureTemplateAddonDialog::onLoad);
    connect(m_saveBtn,    &QPushButton::clicked, this, &CreatureTemplateAddonDialog::onSave);
    connect(m_deleteBtn,  &QPushButton::clicked, this, &CreatureTemplateAddonDialog::onDelete);
    connect(m_refreshBtn, &QPushButton::clicked, this, &CreatureTemplateAddonDialog::onRefresh);
}

void CreatureTemplateAddonDialog::detectSchema()
{
    if (m_schemaDetected) return;
    m_schemaDetected = true;
    if (!m_db || !m_db->isConnected()) return;

    // Pull every column for creature_template_addon.  We rebind the field-name
    // members case-insensitively against this list so legacy spellings (PvPFlags
    // vs PvpFlags, aiAnimKit vs AiAnimKit, ...) survive.
    char sql[512];
    std::snprintf(sql, sizeof(sql),
        "SELECT COLUMN_NAME FROM INFORMATION_SCHEMA.COLUMNS "
        "WHERE TABLE_SCHEMA='%s' AND TABLE_NAME='creature_template_addon'",
        m_worldDb.toStdString().c_str());
    db::QueryResult res;
    auto const err = m_db->query(sql, res);
    if (!err.ok() || res.rowCount() == 0) return;

    auto rebind = [&](QString& target, std::initializer_list<char const*> candidates) {
        for (size_t r = 0; r < res.rowCount(); ++r)
        {
            QString const c = QString::fromStdString(res.cell(r, 0));
            for (char const* cand : candidates)
            {
                if (c.compare(QString::fromUtf8(cand), Qt::CaseInsensitive) == 0)
                {
                    target = c;       // use the column's actual spelling
                    return;
                }
            }
        }
    };
    rebind(m_colPathId,          { "PathId", "path_id" });
    rebind(m_colMount,           { "mount" });
    rebind(m_colStandState,      { "StandState", "bytes1" });
    rebind(m_colAnimTier,        { "AnimTier" });
    rebind(m_colVisFlags,        { "VisFlags", "VisibilityFlags" });
    rebind(m_colSheathState,     { "SheathState" });
    rebind(m_colPvpFlags,        { "PvpFlags", "PvPFlags" });
    rebind(m_colEmote,           { "emote" });
    rebind(m_colAiAnimKit,       { "AiAnimKit", "aiAnimKit" });
    rebind(m_colMovementAnimKit, { "MovementAnimKit", "movementAnimKit" });
    rebind(m_colMeleeAnimKit,    { "MeleeAnimKit", "meleeAnimKit" });
    rebind(m_colVisDistType,     { "VisibilityDistanceType", "visibilityDistanceType" });
    rebind(m_colAuras,           { "auras" });
}

void CreatureTemplateAddonDialog::refreshCreatureName(uint32_t entry)
{
    if (!m_db || !m_db->isConnected())
    {
        m_creatureLbl->setText(tr("DB not connected."));
        return;
    }
    if (entry == 0)
    {
        m_creatureLbl->setText(tr("(no creature loaded)"));
        return;
    }

    // Probe name1 first (modern TC), fall back to name (legacy schema).
    char sql[256];
    std::snprintf(sql, sizeof(sql),
        "SELECT COALESCE(name1, '') FROM %s.creature_template WHERE entry=%u",
        m_worldDb.toStdString().c_str(), entry);
    db::QueryResult res;
    auto err = m_db->query(sql, res);
    if (!err.ok())
    {
        std::snprintf(sql, sizeof(sql),
            "SELECT COALESCE(name, '') FROM %s.creature_template WHERE entry=%u",
            m_worldDb.toStdString().c_str(), entry);
        err = m_db->query(sql, res);
    }
    if (!err.ok() || res.rowCount() == 0)
    {
        m_creatureLbl->setText(tr("%1 - (unknown / no creature_template row)").arg(entry));
        return;
    }
    QString const name = QString::fromStdString(res.cell(0, 0));
    m_creatureLbl->setText(tr("%1 - %2").arg(entry).arg(name));
}

void CreatureTemplateAddonDialog::clearForm()
{
    m_pathIdSpin         ->setValue(0);
    m_mountSpin          ->setValue(0);
    m_standStateSpin     ->setValue(0);
    m_animTierSpin       ->setValue(0);
    m_visFlagsSpin       ->setValue(0);
    m_sheathStateSpin    ->setValue(0);
    m_pvpFlagsSpin       ->setValue(0);
    m_emoteSpin          ->setValue(0);
    m_aiAnimKitSpin      ->setValue(0);
    m_movementAnimKitSpin->setValue(0);
    m_meleeAnimKitSpin   ->setValue(0);
    m_visDistTypeSpin    ->setValue(0);
    m_aurasEdit          ->clear();
}

void CreatureTemplateAddonDialog::loadAddonRow()
{
    m_rowExists = false;
    clearForm();

    if (!m_db || !m_db->isConnected())
    {
        m_statusLabel->setText(tr("DB not connected."));
        return;
    }
    if (m_loadedEntry == 0)
    {
        m_statusLabel->setText(tr("No creature loaded."));
        return;
    }

    QString const sql = QStringLiteral(
        "SELECT %1, %2, %3, %4, %5, %6, %7, %8, %9, %10, %11, %12, %13 "
        "FROM %14.creature_template_addon WHERE entry=%15")
        .arg(m_colPathId, m_colMount, m_colStandState, m_colAnimTier, m_colVisFlags,
             m_colSheathState, m_colPvpFlags, m_colEmote, m_colAiAnimKit)
        .arg(m_colMovementAnimKit, m_colMeleeAnimKit, m_colVisDistType, m_colAuras,
             m_worldDb).arg(m_loadedEntry);

    db::QueryResult res;
    QApplication::setOverrideCursor(Qt::WaitCursor);
    auto const err = m_db->query(sql.toStdString(), res);
    QApplication::restoreOverrideCursor();
    if (!err.ok())
    {
        m_statusLabel->setText(tr("creature_template_addon query failed: %1")
            .arg(QString::fromStdString(err.message)));
        return;
    }
    if (res.rowCount() == 0)
    {
        m_statusLabel->setText(tr("No creature_template_addon row for entry=%1 (Save will INSERT).")
            .arg(m_loadedEntry));
        return;
    }

    auto getU = [&](size_t col) -> uint64_t { return res.asUInt64(0, col).value_or(0); };
    m_pathIdSpin         ->setValue(int(getU(0)));
    m_mountSpin          ->setValue(int(getU(1)));
    m_standStateSpin     ->setValue(int(getU(2)));
    m_animTierSpin       ->setValue(int(getU(3)));
    m_visFlagsSpin       ->setValue(int(getU(4)));
    m_sheathStateSpin    ->setValue(int(getU(5)));
    m_pvpFlagsSpin       ->setValue(int(getU(6)));
    m_emoteSpin          ->setValue(int(getU(7)));
    m_aiAnimKitSpin      ->setValue(int(getU(8)));
    m_movementAnimKitSpin->setValue(int(getU(9)));
    m_meleeAnimKitSpin   ->setValue(int(getU(10)));
    m_visDistTypeSpin    ->setValue(int(getU(11)));
    m_aurasEdit          ->setText(QString::fromStdString(res.cell(0, 12)));

    m_rowExists = true;
    m_statusLabel->setText(tr("Loaded creature_template_addon row for entry=%1.")
        .arg(m_loadedEntry));
}

void CreatureTemplateAddonDialog::onLoad()
{
    detectSchema();
    m_loadedEntry = static_cast<uint32_t>(m_entrySpin->value());
    refreshCreatureName(m_loadedEntry);
    loadAddonRow();

    bool const hasEntry = (m_loadedEntry != 0);
    m_saveBtn   ->setEnabled(hasEntry);
    m_deleteBtn ->setEnabled(hasEntry && m_rowExists);
    m_refreshBtn->setEnabled(hasEntry);
}

void CreatureTemplateAddonDialog::onRefresh()
{
    if (m_loadedEntry == 0) return;
    loadAddonRow();
    m_deleteBtn->setEnabled(m_rowExists);
}

bool CreatureTemplateAddonDialog::runInTransaction(QString const& sql,
                                                   QString const& description)
{
    if (!m_db || !m_db->isConnected())
    {
        QMessageBox::warning(this, tr("Not connected"), tr("World DB not open."));
        return false;
    }
    auto err = m_db->exec("START TRANSACTION");
    if (!err.ok())
    {
        QMessageBox::critical(this, tr("Transaction failed"),
            tr("BEGIN failed: %1").arg(QString::fromStdString(err.message)));
        return false;
    }
    uint64_t affected = 0;
    err = m_db->exec(sql.toStdString(), &affected);
    if (!err.ok())
    {
        (void)m_db->exec("ROLLBACK");
        QMessageBox::critical(this, tr("DML failed"),
            tr("%1\n\n%2").arg(description).arg(QString::fromStdString(err.message)));
        return false;
    }
    err = m_db->exec("COMMIT");
    if (!err.ok())
    {
        (void)m_db->exec("ROLLBACK");
        QMessageBox::critical(this, tr("Commit failed"),
            tr("%1\n\n%2").arg(description).arg(QString::fromStdString(err.message)));
        return false;
    }
    m_statusLabel->setText(tr("%1 (affected=%2)").arg(description).arg(qulonglong(affected)));
    return true;
}

void CreatureTemplateAddonDialog::onSave()
{
    if (m_loadedEntry == 0)
    {
        QMessageBox::warning(this, tr("Save"), tr("Load a creature entry first."));
        return;
    }

    uint32_t const pathId      = uint32_t(m_pathIdSpin         ->value());
    uint32_t const mount       = uint32_t(m_mountSpin          ->value());
    uint32_t const standState  = uint32_t(m_standStateSpin     ->value());
    uint32_t const animTier    = uint32_t(m_animTierSpin       ->value());
    uint32_t const visFlags    = uint32_t(m_visFlagsSpin       ->value());
    uint32_t const sheath      = uint32_t(m_sheathStateSpin    ->value());
    uint32_t const pvpFlags    = uint32_t(m_pvpFlagsSpin       ->value());
    uint32_t const emote       = uint32_t(m_emoteSpin          ->value());
    uint32_t const aiKit       = uint32_t(m_aiAnimKitSpin      ->value());
    uint32_t const moveKit     = uint32_t(m_movementAnimKitSpin->value());
    uint32_t const meleeKit    = uint32_t(m_meleeAnimKitSpin   ->value());
    uint32_t const visDistType = uint32_t(m_visDistTypeSpin    ->value());
    QString  const auras       = m_aurasEdit->text().trimmed();
    QString  const aurasEscaped = escapeSqlString(auras);

    QString sql;
    QString description;
    if (m_rowExists)
    {
        sql = QStringLiteral(
            "UPDATE %1.creature_template_addon SET "
            "%2=%3, %4=%5, %6=%7, %8=%9, %10=%11, %12=%13, %14=%15, %16=%17, %18=%19, "
            "%20=%21, %22=%23, %24=%25, %26='%27' "
            "WHERE entry=%28")
            .arg(m_worldDb)
            .arg(m_colPathId).arg(pathId)
            .arg(m_colMount).arg(mount)
            .arg(m_colStandState).arg(standState)
            .arg(m_colAnimTier).arg(animTier)
            .arg(m_colVisFlags).arg(visFlags)
            .arg(m_colSheathState).arg(sheath)
            .arg(m_colPvpFlags).arg(pvpFlags)
            .arg(m_colEmote).arg(emote)
            .arg(m_colAiAnimKit).arg(aiKit)
            .arg(m_colMovementAnimKit).arg(moveKit)
            .arg(m_colMeleeAnimKit).arg(meleeKit)
            .arg(m_colVisDistType).arg(visDistType)
            .arg(m_colAuras).arg(aurasEscaped)
            .arg(m_loadedEntry);
        description = tr("UPDATE creature_template_addon entry=%1").arg(m_loadedEntry);
    }
    else
    {
        sql = QStringLiteral(
            "INSERT INTO %1.creature_template_addon "
            "(entry, %2, %3, %4, %5, %6, %7, %8, %9, %10, %11, %12, %13, %14) "
            "VALUES (%15, %16, %17, %18, %19, %20, %21, %22, %23, %24, %25, %26, %27, '%28')")
            .arg(m_worldDb)
            .arg(m_colPathId, m_colMount, m_colStandState, m_colAnimTier, m_colVisFlags,
                 m_colSheathState, m_colPvpFlags, m_colEmote, m_colAiAnimKit)
            .arg(m_colMovementAnimKit, m_colMeleeAnimKit, m_colVisDistType, m_colAuras)
            .arg(m_loadedEntry)
            .arg(pathId).arg(mount).arg(standState).arg(animTier).arg(visFlags)
            .arg(sheath).arg(pvpFlags).arg(emote).arg(aiKit)
            .arg(moveKit).arg(meleeKit).arg(visDistType)
            .arg(aurasEscaped);
        description = tr("INSERT creature_template_addon entry=%1").arg(m_loadedEntry);
    }

    if (runInTransaction(sql, description))
    {
        loadAddonRow();
        m_deleteBtn->setEnabled(m_rowExists);
    }
}

void CreatureTemplateAddonDialog::onDelete()
{
    if (m_loadedEntry == 0 || !m_rowExists) return;

    auto const choice = QMessageBox::question(this, tr("Delete addon"),
        tr("Delete creature_template_addon row for entry=%1?\n"
           "Per-template defaults will revert to the hard-coded TC fallback "
           "for every spawn that lacks its own creature_addon row.")
            .arg(m_loadedEntry),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (choice != QMessageBox::Yes) return;

    QString const sql = QStringLiteral(
        "DELETE FROM %1.creature_template_addon WHERE entry=%2")
        .arg(m_worldDb).arg(m_loadedEntry);
    if (runInTransaction(sql, tr("DELETE creature_template_addon entry=%1")
            .arg(m_loadedEntry)))
    {
        loadAddonRow();
        m_deleteBtn->setEnabled(m_rowExists);
    }
}

} // namespace world_editor::app
