#include "GameObjectInfoDock.h"

#include "../db/MySqlClient.h"

#include <QHeaderView>
#include <QLabel>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

#include <array>
#include <string>
#include <vector>

namespace world_editor::app
{

namespace
{

// MySQL error codes we treat as "fall through" (matches ItemInfoDock).
constexpr uint32_t kErrNoSuchTable  = 1146;
constexpr uint32_t kErrNoSuchColumn = 1054;

// gameobject_template carries Data0..Data35 (36 slots).  TC's enum names
// for the type values are hardcoded so the dock doesn't depend on
// pulling in any TC headers.  Unknown ids render as "type#<n>".
char const* goTypeName(uint32_t t)
{
    switch (t)
    {
        case  0: return "DOOR";
        case  1: return "BUTTON";
        case  2: return "QUESTGIVER";
        case  3: return "CHEST";
        case  4: return "BINDER";
        case  5: return "GENERIC";
        case  6: return "TRAP";
        case  7: return "CHAIR";
        case  8: return "SPELL_FOCUS";
        case  9: return "TEXT";
        case 10: return "GOOBER";
        case 11: return "TRANSPORT";
        case 12: return "AREADAMAGE";
        case 13: return "CAMERA";
        case 14: return "MAP_OBJECT";
        case 15: return "MO_TRANSPORT";
        case 16: return "DUEL_ARBITER";
        case 17: return "FISHINGNODE";
        case 18: return "RITUAL";
        case 19: return "MAILBOX";
        case 20: return "DO_NOT_USE";
        case 21: return "GUARDPOST";
        case 22: return "SPELLCASTER";
        case 23: return "MEETINGSTONE";
        case 24: return "FLAGSTAND";
        case 25: return "FISHINGHOLE";
        case 26: return "FLAGDROP";
        case 27: return "MINI_GAME";
        case 28: return "DO_NOT_USE_2";
        case 29: return "CONTROL_ZONE";
        case 30: return "AURA_GENERATOR";
        case 31: return "DUNGEON_DIFFICULTY";
        case 32: return "BARBER_CHAIR";
        case 33: return "DESTRUCTIBLE_BUILDING";
        case 34: return "GUILD_BANK";
        case 35: return "TRAPDOOR";
        case 38: return "NEW_FLAG";
        case 39: return "NEW_FLAG_DROP";
        case 40: return "GARRISON_BUILDING";
        case 41: return "GARRISON_PLOT";
        case 42: return "CLIENT_CREATURE";
        case 43: return "CLIENT_ITEM";
        case 44: return "CAPTURE_POINT";
        case 45: return "PHASEABLE_MO";
        case 46: return "GARRISON_MONUMENT";
        case 50: return "GARRISON_SHIPMENT";
        case 52: return "ITEM_FORGE";
        case 53: return "UI_LINK";
        case 54: return "KEYSTONE_RECEPTACLE";
        case 55: return "GATHERING_NODE";
        case 56: return "CHALLENGE_MODE_REWARD";
        default: return nullptr;
    }
}

// Returns a label for Data<i> based on `type`.  nullptr means "no
// type-specific name known" - the caller renders "Data<i>" verbatim.
// Coverage drawn from TC's GameObjectData.h struct layout (door /
// questgiver / chest / trap / chair / spellFocus / text / goober /
// transport / moTransport / spellcaster / meetingstone / destructibleBuilding /
// gatheringNode are the ones operators look at most often).
char const* goDataLabel(uint32_t type, int dataIdx)
{
    switch (type)
    {
        case 0: // DOOR
            switch (dataIdx) {
                case 0: return "startOpen";
                case 1: return "lockId";
                case 2: return "autoCloseTime";
                case 3: return "noDamageImmune";
                case 4: return "openTextID";
                case 5: return "closeTextID";
                default: return nullptr;
            }
        case 1: // BUTTON
            switch (dataIdx) {
                case 0: return "startOpen";
                case 1: return "lockId";
                case 2: return "autoCloseTime";
                case 3: return "linkedTrap";
                case 4: return "noDamageImmune";
                case 5: return "large";
                case 6: return "openTextID";
                case 7: return "closeTextID";
                default: return nullptr;
            }
        case 2: // QUESTGIVER
            switch (dataIdx) {
                case 0: return "lockId";
                case 1: return "questList";
                case 2: return "pageMaterial";
                case 3: return "gossipID";
                case 4: return "customAnim";
                case 5: return "noDamageImmune";
                case 6: return "openTextID";
                case 7: return "allowMounted";
                case 8: return "large";
                default: return nullptr;
            }
        case 3: // CHEST
            switch (dataIdx) {
                case 0: return "lockId";
                case 1: return "lootId";
                case 2: return "restockTime";
                case 3: return "consumable";
                case 4: return "minRestock";
                case 5: return "maxRestock";
                case 6: return "eventId";
                case 7: return "linkedTrapId";
                case 8: return "questId";
                case 9: return "level";
                case 10: return "losOK";
                case 11: return "leaveLoot";
                case 12: return "notInCombat";
                case 13: return "logLoot";
                case 14: return "openTextID";
                case 15: return "groupLootRules";
                case 16: return "floatingTooltip";
                default: return nullptr;
            }
        case 4: // BINDER
            return dataIdx == 0 ? "lockId" : nullptr;
        case 5: // GENERIC
            switch (dataIdx) {
                case 0: return "floatingTooltip";
                case 1: return "highlight";
                case 2: return "serverOnly";
                case 3: return "large";
                case 4: return "floatOnWater";
                case 5: return "questID";
                default: return nullptr;
            }
        case 6: // TRAP
            switch (dataIdx) {
                case 0: return "lockId";
                case 1: return "level";
                case 2: return "diameter";
                case 3: return "spellId";
                case 4: return "charges";
                case 5: return "cooldown";
                case 6: return "autoCloseTime";
                case 7: return "startDelay";
                case 8: return "serverOnly";
                case 9: return "stealthed";
                case 10: return "large";
                case 11: return "invisible";
                case 12: return "openTextID";
                case 13: return "closeTextID";
                case 14: return "ignoreTotems";
                default: return nullptr;
            }
        case 7: // CHAIR
            switch (dataIdx) {
                case 0: return "slots";
                case 1: return "height";
                case 2: return "onlyCreatorUse";
                case 3: return "triggeredEvent";
                default: return nullptr;
            }
        case 8: // SPELL_FOCUS
            switch (dataIdx) {
                case 0: return "spellFocusType";
                case 1: return "radius";
                case 2: return "linkedTrapId";
                case 3: return "serverOnly";
                case 4: return "questID";
                case 5: return "large";
                case 6: return "floatingTooltip";
                default: return nullptr;
            }
        case 9: // TEXT
            switch (dataIdx) {
                case 0: return "pageID";
                case 1: return "language";
                case 2: return "pageMaterial";
                case 3: return "allowMounted";
                default: return nullptr;
            }
        case 10: // GOOBER
            switch (dataIdx) {
                case 0: return "lockId";
                case 1: return "questId";
                case 2: return "eventId";
                case 3: return "autoCloseTime";
                case 4: return "customAnim";
                case 5: return "consumable";
                case 6: return "cooldown";
                case 7: return "pageId";
                case 8: return "language";
                case 9: return "pageMaterial";
                case 10: return "spellId";
                case 11: return "noDamageImmune";
                case 12: return "linkedTrapId";
                case 13: return "large";
                case 14: return "openTextID";
                case 15: return "closeTextID";
                case 16: return "losOK";
                case 17: return "allowMounted";
                case 18: return "floatingTooltip";
                case 19: return "gossipID";
                case 20: return "WorldStateSetsState";
                default: return nullptr;
            }
        case 11: // TRANSPORT
            switch (dataIdx) {
                case 0: return "pause";
                case 1: return "startOpen";
                case 2: return "autoCloseTime";
                case 3: return "pause1EventID";
                case 4: return "pause2EventID";
                case 5: return "mapID";
                default: return nullptr;
            }
        case 13: // CAMERA
            switch (dataIdx) {
                case 0: return "lockId";
                case 1: return "cinematicId";
                case 2: return "eventID";
                case 3: return "openTextID";
                default: return nullptr;
            }
        case 15: // MO_TRANSPORT
            switch (dataIdx) {
                case 0: return "taxiPathID";
                case 1: return "moveSpeed";
                case 2: return "accelRate";
                case 3: return "startEventID";
                case 4: return "stopEventID";
                case 5: return "transportPhysics";
                case 6: return "mapID";
                case 7: return "worldState1";
                case 8: return "canBeStopped";
                default: return nullptr;
            }
        case 18: // RITUAL (SUMMONING_RITUAL)
            switch (dataIdx) {
                case 0: return "reqParticipants";
                case 1: return "spellId";
                case 2: return "animSpell";
                case 3: return "ritualPersistent";
                case 4: return "casterTargetSpell";
                case 5: return "casterTargetSpellTargets";
                case 6: return "castersGrouped";
                case 7: return "ritualNoTargetCheck";
                default: return nullptr;
            }
        case 22: // SPELLCASTER
            switch (dataIdx) {
                case 0: return "spellId";
                case 1: return "charges";
                case 2: return "partyOnly";
                case 3: return "allowMounted";
                case 4: return "large";
                default: return nullptr;
            }
        case 23: // MEETINGSTONE
            switch (dataIdx) {
                case 0: return "minLevel";
                case 1: return "maxLevel";
                case 2: return "areaID";
                default: return nullptr;
            }
        case 24: // FLAGSTAND
            switch (dataIdx) {
                case 0: return "lockId";
                case 1: return "pickupSpell";
                case 2: return "radius";
                case 3: return "returnAura";
                case 4: return "returnSpell";
                case 5: return "noDamageImmune";
                case 6: return "openTextID";
                default: return nullptr;
            }
        case 25: // FISHINGHOLE
            switch (dataIdx) {
                case 0: return "radius";
                case 1: return "lootId";
                case 2: return "minRestock";
                case 3: return "maxRestock";
                case 4: return "lockId";
                default: return nullptr;
            }
        case 26: // FLAGDROP
            switch (dataIdx) {
                case 0: return "lockId";
                case 1: return "eventID";
                case 2: return "pickupSpell";
                case 3: return "noDamageImmune";
                default: return nullptr;
            }
        case 29: // CONTROL_ZONE
            switch (dataIdx) {
                case 0: return "radius";
                case 1: return "spell";
                case 2: return "worldState1";
                case 3: return "worldstate2";
                case 4: return "captureEventHorde";
                case 5: return "captureEventAlliance";
                default: return nullptr;
            }
        case 30: // AURA_GENERATOR
            switch (dataIdx) {
                case 0: return "startOpen";
                case 1: return "radius";
                case 2: return "auraID1";
                case 3: return "conditionID1";
                case 4: return "auraID2";
                case 5: return "conditionID2";
                case 6: return "serverOnly";
                default: return nullptr;
            }
        case 32: // BARBER_CHAIR
            switch (dataIdx) {
                case 0: return "chairheight";
                case 1: return "heightOffset";
                case 2: return "SitAnimKit";
                default: return nullptr;
            }
        case 33: // DESTRUCTIBLE_BUILDING
            switch (dataIdx) {
                case 0: return "intactNumHits";
                case 1: return "creditProxyCreature";
                case 2: return "intactEvent";
                case 3: return "damagedDisplayId";
                case 4: return "damagedNumHits";
                case 5: return "empty3";
                case 6: return "empty4";
                case 7: return "empty5";
                case 8: return "damagedEvent";
                case 9: return "destroyedDisplayId";
                case 10: return "empty7";
                case 11: return "empty8";
                case 12: return "empty9";
                case 13: return "destroyedEvent";
                case 14: return "empty10";
                case 15: return "debuildingTimeSecs";
                case 16: return "empty11";
                case 17: return "destructibleData";
                case 18: return "rebuildingEvent";
                case 19: return "empty12";
                case 20: return "empty13";
                case 21: return "damageEvent";
                case 22: return "empty14";
                default: return nullptr;
            }
        case 34: // GUILD_BANK
            return nullptr;
        case 35: // TRAPDOOR
            switch (dataIdx) {
                case 0: return "whenToPause";
                case 1: return "startOpen";
                case 2: return "autoClose";
                default: return nullptr;
            }
        case 55: // GATHERING_NODE
            switch (dataIdx) {
                case 0: return "openLockID";
                case 1: return "lootID";
                case 2: return "leveledLootID";
                case 3: return "xpDifficulty";
                case 4: return "objectSize";
                case 5: return "spell";
                case 6: return "triggeredEvent";
                case 7: return "linkedTrap";
                case 8: return "questID";
                case 9: return "rarity";
                case 10: return "maxNumberofLoots";
                case 11: return "logloot";
                case 12: return "linkedTrigger";
                case 13: return "spawnVignette";
                case 14: return "MaxCharges";
                default: return nullptr;
            }
        default:
            return nullptr;
    }
}

// gameobject_template.flags - decoded common bits.  Drawn from TC's
// GO_FLAG_* enum.  Each non-zero bit contributes a comma-separated
// token in the output.
QString decodeGoFlags(uint64_t flags)
{
    static struct { uint64_t bit; char const* name; } const kBits[] = {
        { 0x0001, "IN_USE" },
        { 0x0002, "LOCKED" },
        { 0x0004, "INTERACT_COND" },
        { 0x0008, "TRANSPORT" },
        { 0x0010, "NOT_SELECTABLE" },
        { 0x0020, "NODESPAWN" },
        { 0x0040, "AI_OBSTACLE" },
        { 0x0080, "FREEZE_ANIMATION" },
        { 0x0100, "DAMAGED" },
        { 0x0200, "DESTROYED" },
        { 0x0400, "INTERACT_DISTANCE_USES_TEMPLATE_MODEL" },
        { 0x0800, "MAP_OBJECT" },
        { 0x1000, "IN_MULTI_USE" },
        { 0x8000, "UNTARGETABLE" },
    };
    QStringList tokens;
    for (auto const& b : kBits)
        if (flags & b.bit)
            tokens << QString::fromLatin1(b.name);
    return tokens.isEmpty() ? QStringLiteral("(none)") : tokens.join(QStringLiteral(", "));
}

} // namespace

GameObjectInfoDock::GameObjectInfoDock(db::MySqlClient* dbClient, QWidget* parent)
    : QWidget(parent)
    , m_db(dbClient)
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(6, 6, 6, 6);
    root->setSpacing(4);

    m_header = new QLabel(tr("Click a GameObject spawn to inspect its template."), this);
    m_header->setWordWrap(true);
    root->addWidget(m_header);

    // Bold header carrying the GO name + decoded type.
    m_nameLabel = new QLabel(this);
    m_nameLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_nameLabel->setStyleSheet(QStringLiteral("QLabel { font-weight: bold; font-size: 12pt; }"));
    m_nameLabel->setWordWrap(true);
    root->addWidget(m_nameLabel);

    QString const mono = QStringLiteral("QLabel { font-family: monospace; }");

    m_identity = new QLabel(this);
    m_identity->setWordWrap(true);
    m_identity->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_identity->setStyleSheet(mono);
    root->addWidget(m_identity);

    m_typeHeader = new QLabel(this);
    m_typeHeader->setStyleSheet(mono);
    root->addWidget(m_typeHeader);

    // Type-specific Data0..Data35 pivot.  Three columns: slot label
    // (Data<n> / decoded name), value, raw idx for operator reference.
    m_typeTable = new QTableWidget(this);
    m_typeTable->setColumnCount(2);
    m_typeTable->setHorizontalHeaderLabels({ tr("field"), tr("value") });
    m_typeTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_typeTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_typeTable->verticalHeader()->setVisible(false);
    m_typeTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_typeTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_typeTable->setAlternatingRowColors(true);
    m_typeTable->setMaximumHeight(420);
    root->addWidget(m_typeTable);

    m_flags = new QLabel(this);
    m_flags->setWordWrap(true);
    m_flags->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_flags->setStyleSheet(mono);
    root->addWidget(m_flags);

    m_gold = new QLabel(this);
    m_gold->setWordWrap(true);
    m_gold->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_gold->setStyleSheet(mono);
    root->addWidget(m_gold);

    root->addStretch(1);

    clear();
}

void GameObjectInfoDock::clear()
{
    m_header->setText(tr("Click a GameObject spawn to inspect its template."));
    m_nameLabel->clear();
    m_identity->clear();
    m_typeHeader->clear();
    m_typeTable->setRowCount(0);
    m_flags->clear();
    m_gold->clear();
}

void GameObjectInfoDock::setGameObjectEntry(uint32_t entry)
{
    clear();

    if (entry == 0)
        return;

    if (!m_db || !m_db->isConnected())
    {
        m_header->setText(tr("DB not connected.  GO entry = %1.").arg(entry));
        return;
    }

    // Probe gameobject_template existence with a cheap row probe so a
    // missing table surfaces as our fallback message rather than an
    // SQL error per cluster query.
    {
        std::string const probe = "SELECT entry FROM gameobject_template WHERE entry = "
                                + std::to_string(entry) + " LIMIT 1";
        db::QueryResult res;
        auto const err = m_db->query(probe, res);
        if (!err.ok())
        {
            if (err.code == kErrNoSuchTable)
            {
                m_header->setText(tr("GO %1 — (gameobject_template not present)").arg(entry));
                return;
            }
            m_header->setText(tr("GO %1 — query failed: %2")
                .arg(entry)
                .arg(QString::fromStdString(err.message)));
            return;
        }
        if (res.rowCount() == 0)
        {
            m_header->setText(tr("GO %1 — no gameobject_template row found.").arg(entry));
            return;
        }
    }

    m_header->setText(tr("GameObject %1").arg(entry));

    populateIdentity     (entry);
    populateTypeSpecific (entry);
    populateFlagsFaction (entry);
    populateGold         (entry);
}

bool GameObjectInfoDock::populateIdentity(uint32_t entry)
{
    std::string const sql =
        "SELECT name, type, displayId "
        "FROM gameobject_template WHERE entry = " + std::to_string(entry) + " LIMIT 1";
    db::QueryResult res;
    auto const err = m_db->query(sql, res);
    if (!err.ok())
    {
        if (err.code == kErrNoSuchColumn)
            return false;
        m_identity->setText(tr("(identity query failed: %1)")
            .arg(QString::fromStdString(err.message)));
        return false;
    }
    if (res.rowCount() == 0)
        return false;

    QString  const name      = QString::fromStdString(res.cell(0, 0));
    uint32_t const type      = static_cast<uint32_t>(res.asUInt64(0, 1).value_or(0));
    uint32_t const displayId = static_cast<uint32_t>(res.asUInt64(0, 2).value_or(0));

    char const* tname = goTypeName(type);
    QString const typeDisplay = tname ? QString::fromLatin1(tname)
                                      : QStringLiteral("type#%1").arg(type);

    m_nameLabel->setText(tr("%1  —  %2").arg(name, typeDisplay));

    m_identity->setText(
        tr("Entry:        %1\n"
           "Type:         %2 (%3)\n"
           "DisplayId:    %4")
            .arg(entry)
            .arg(type).arg(typeDisplay)
            .arg(displayId));
    return true;
}

bool GameObjectInfoDock::populateTypeSpecific(uint32_t entry)
{
    // Pull type + Data0..Data35 in one query.  Some old TC builds carry
    // fewer Data slots; on a 1054 we fall back to a 0..23 probe.
    auto pullData = [this, entry](int upper, uint32_t& outType,
                                  std::vector<int64_t>& outVals) -> bool
    {
        std::string sql = "SELECT type";
        for (int i = 0; i <= upper; ++i)
            sql += ", Data" + std::to_string(i);
        sql += " FROM gameobject_template WHERE entry = " + std::to_string(entry) + " LIMIT 1";

        db::QueryResult res;
        auto const err = m_db->query(sql, res);
        if (!err.ok() || res.rowCount() == 0)
            return false;
        outType = static_cast<uint32_t>(res.asUInt64(0, 0).value_or(0));
        outVals.clear();
        outVals.reserve(size_t(upper + 1));
        for (int i = 0; i <= upper; ++i)
            outVals.push_back(res.asInt64(0, size_t(i + 1)).value_or(0));
        return true;
    };

    uint32_t type = 0;
    std::vector<int64_t> vals;
    if (!pullData(35, type, vals))
    {
        // Retry with the older 0..23 layout before giving up.
        if (!pullData(23, type, vals))
        {
            m_typeHeader->clear();
            m_typeTable->setRowCount(0);
            return false;
        }
    }

    char const* tname = goTypeName(type);
    m_typeHeader->setText(tr("Data fields (%1):")
        .arg(tname ? QString::fromLatin1(tname) : QStringLiteral("type#%1").arg(type)));

    // Build display rows: every slot that has a label is shown even
    // when zero (so operators see the schema); unlabeled slots only
    // render when non-zero (to keep noise down for narrow types).
    struct Row { QString label; int64_t value; bool labeled; };
    std::vector<Row> rows;
    rows.reserve(vals.size());
    for (size_t i = 0; i < vals.size(); ++i)
    {
        char const* label = goDataLabel(type, int(i));
        bool const labeled = (label != nullptr);
        if (!labeled && vals[i] == 0)
            continue;
        QString const display = labeled
            ? tr("Data%1  %2").arg(i, 2, 10, QChar('0')).arg(QString::fromLatin1(label))
            : tr("Data%1").arg(i, 2, 10, QChar('0'));
        rows.push_back({ display, vals[i], labeled });
    }

    m_typeTable->setRowCount(int(rows.size()));
    for (size_t r = 0; r < rows.size(); ++r)
    {
        auto* nameCell = new QTableWidgetItem(rows[r].label);
        nameCell->setFlags(nameCell->flags() & ~Qt::ItemIsEditable);
        m_typeTable->setItem(int(r), 0, nameCell);
        auto* valCell = new QTableWidgetItem(QString::number(rows[r].value));
        valCell->setFlags(valCell->flags() & ~Qt::ItemIsEditable);
        m_typeTable->setItem(int(r), 1, valCell);
    }
    return true;
}

bool GameObjectInfoDock::populateFlagsFaction(uint32_t entry)
{
    std::string const sql =
        "SELECT flags, faction, size "
        "FROM gameobject_template WHERE entry = " + std::to_string(entry) + " LIMIT 1";
    db::QueryResult res;
    auto const err = m_db->query(sql, res);
    if (!err.ok() || res.rowCount() == 0)
        return false;

    uint64_t const flags   = res.asUInt64(0, 0).value_or(0);
    uint64_t const faction = res.asUInt64(0, 1).value_or(0);
    double   const size    = res.asDouble(0, 2).value_or(0.0);

    m_flags->setText(
        tr("Flags:        0x%1 (%2)\n"
           "Faction:      %3\n"
           "Size:         %4")
            .arg(QString::number(flags, 16))
            .arg(decodeGoFlags(flags))
            .arg(faction)
            .arg(QString::number(size, 'f', 2)));
    return true;
}

bool GameObjectInfoDock::populateGold(uint32_t entry)
{
    std::string const sql =
        "SELECT MinMoneyLoot, MaxMoneyLoot "
        "FROM gameobject_template WHERE entry = " + std::to_string(entry) + " LIMIT 1";
    db::QueryResult res;
    auto const err = m_db->query(sql, res);
    if (!err.ok() || res.rowCount() == 0)
        return false;

    uint64_t const mn = res.asUInt64(0, 0).value_or(0);
    uint64_t const mx = res.asUInt64(0, 1).value_or(0);

    // Skip the section entirely when both are zero - most chest /
    // non-loot GOs sit at 0/0 and the row is noise.
    if (mn == 0 && mx == 0)
        return false;

    m_gold->setText(
        tr("MinGoldLoot:  %1\n"
           "MaxGoldLoot:  %2")
            .arg(mn).arg(mx));
    return true;
}

} // namespace world_editor::app
