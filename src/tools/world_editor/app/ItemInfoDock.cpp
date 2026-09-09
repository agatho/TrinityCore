#include "ItemInfoDock.h"

#include "../db/MySqlClient.h"

#include <QHeaderView>
#include <QLabel>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

#include <array>
#include <string>

namespace world_editor::app
{

namespace
{

// MySQL error codes we treat as "fall through" (matches SpellInfoDock).
constexpr uint32_t kErrNoSuchTable  = 1146;
constexpr uint32_t kErrNoSuchColumn = 1054;

// Quality color palette - mirrors WoW's ITEM_QUALITY_* enum.  Values
// outside 0..7 fall back to grey so the renderer can't crash on a bad
// row.  Hex strings used directly in stylesheet QSS.
char const* qualityHexColor(uint32_t q)
{
    switch (q)
    {
        case 0: return "#9d9d9d"; // Poor (grey)
        case 1: return "#ffffff"; // Common (white)
        case 2: return "#1eff00"; // Uncommon (green)
        case 3: return "#0070dd"; // Rare (blue)
        case 4: return "#a335ee"; // Epic (purple)
        case 5: return "#ff8000"; // Legendary (orange)
        case 6: return "#e6cc80"; // Artifact (light gold)
        case 7: return "#00ccff"; // Heirloom (light blue / "gold-tier" per spec)
        default: return "#9d9d9d";
    }
}

char const* qualityName(uint32_t q)
{
    switch (q)
    {
        case 0: return "Poor";
        case 1: return "Common";
        case 2: return "Uncommon";
        case 3: return "Rare";
        case 4: return "Epic";
        case 5: return "Legendary";
        case 6: return "Artifact";
        case 7: return "Heirloom";
        default: return "Unknown";
    }
}

// TC's ItemModType enum.  Hardcoded so the dock doesn't depend on
// pulling in any TC headers.  Values not in this table render as
// "stat#<n>" so unknown indices stay visible.
char const* itemModTypeName(uint32_t t)
{
    switch (t)
    {
        case  0: return "Mana";
        case  1: return "Health";
        case  3: return "Agility";
        case  4: return "Strength";
        case  5: return "Intellect";
        case  6: return "Spirit";
        case  7: return "Stamina";
        case 12: return "Defense Rating";
        case 13: return "Dodge Rating";
        case 14: return "Parry Rating";
        case 15: return "Block Rating";
        case 16: return "Melee Hit Rating";
        case 17: return "Ranged Hit Rating";
        case 18: return "Spell Hit Rating";
        case 19: return "Melee Crit Rating";
        case 20: return "Ranged Crit Rating";
        case 21: return "Spell Crit Rating";
        case 28: return "Melee Haste Rating";
        case 29: return "Ranged Haste Rating";
        case 30: return "Spell Haste Rating";
        case 31: return "Hit Rating";
        case 32: return "Crit Rating";
        case 35: return "Resilience Rating";
        case 36: return "Haste Rating";
        case 37: return "Expertise Rating";
        case 38: return "Attack Power";
        case 39: return "Ranged Attack Power";
        case 41: return "Spell Healing";
        case 42: return "Spell Damage";
        case 43: return "Mana Regen";
        case 44: return "Armor Penetration";
        case 45: return "Spell Power";
        case 46: return "Health Regen";
        case 47: return "Spell Penetration";
        case 48: return "Block Value";
        case 49: return "Mastery Rating";
        case 50: return "Bonus Armor";
        case 51: return "Fire Resistance";
        case 52: return "Frost Resistance";
        case 53: return "Holy Resistance";
        case 54: return "Shadow Resistance";
        case 55: return "Nature Resistance";
        case 56: return "Arcane Resistance";
        case 57: return "PvP Power";
        case 59: return "Cr Amplify";
        case 60: return "Cr Multistrike";
        case 61: return "Cr Readiness";
        case 62: return "Cr Speed";
        case 63: return "Cr Lifesteal";
        case 64: return "Cr Avoidance";
        case 65: return "Cr Sturdiness";
        case 71: return "Agi/Str/Int";
        case 72: return "Agi/Str";
        case 73: return "Agi/Int";
        case 74: return "Str/Int";
        case 75: return "Versatility";
        default: return nullptr;
    }
}

// Pretty bonding type (item_template.bonding column).
char const* bondingName(uint32_t b)
{
    switch (b)
    {
        case 0: return "No bind";
        case 1: return "Bind on Pickup";
        case 2: return "Bind on Equip";
        case 3: return "Bind on Use";
        case 4: return "Quest item";
        default: return "?";
    }
}

// Pretty inventory_type (TC InventoryType enum, the slot family).
char const* inventoryTypeName(uint32_t t)
{
    switch (t)
    {
        case  0: return "Non-equippable";
        case  1: return "Head";
        case  2: return "Neck";
        case  3: return "Shoulder";
        case  4: return "Shirt";
        case  5: return "Chest";
        case  6: return "Waist";
        case  7: return "Legs";
        case  8: return "Feet";
        case  9: return "Wrist";
        case 10: return "Hands";
        case 11: return "Finger";
        case 12: return "Trinket";
        case 13: return "1H Weapon";
        case 14: return "Shield";
        case 15: return "Ranged";
        case 16: return "Back";
        case 17: return "2H Weapon";
        case 18: return "Bag";
        case 19: return "Tabard";
        case 20: return "Robe";
        case 21: return "Main hand";
        case 22: return "Off hand";
        case 23: return "Holdable";
        case 24: return "Ammo";
        case 25: return "Thrown";
        case 26: return "Ranged Right";
        case 27: return "Quiver";
        case 28: return "Relic";
        default: return "?";
    }
}

// Item class (top-level category) - TC ItemClass enum.
char const* itemClassName(uint32_t c)
{
    switch (c)
    {
        case  0: return "Consumable";
        case  1: return "Container";
        case  2: return "Weapon";
        case  3: return "Gem";
        case  4: return "Armor";
        case  5: return "Reagent";
        case  6: return "Projectile";
        case  7: return "Trade goods";
        case  8: return "Item Enhancement";
        case  9: return "Recipe";
        case 11: return "Quiver";
        case 12: return "Quest";
        case 13: return "Key";
        case 15: return "Miscellaneous";
        case 16: return "Glyph";
        case 17: return "Battle Pet";
        case 18: return "WoW Token";
        default: return "?";
    }
}

// Format BuyPrice / SellPrice (copper) as g/s/c.
QString prettyMoney(uint64_t copper)
{
    if (copper == 0) return QStringLiteral("0c");
    uint64_t const g = copper / 10000ULL;
    uint64_t const s = (copper / 100ULL) % 100ULL;
    uint64_t const c = copper % 100ULL;
    QStringList parts;
    if (g) parts << QStringLiteral("%1g").arg(g);
    if (s) parts << QStringLiteral("%1s").arg(s);
    if (c || parts.isEmpty()) parts << QStringLiteral("%1c").arg(c);
    return parts.join(QChar(' '));
}

// Damage school name from TC SpellSchools enum.
char const* damageSchoolName(uint32_t s)
{
    switch (s)
    {
        case 0: return "Physical";
        case 1: return "Holy";
        case 2: return "Fire";
        case 3: return "Nature";
        case 4: return "Frost";
        case 5: return "Shadow";
        case 6: return "Arcane";
        default: return "?";
    }
}

} // namespace

ItemInfoDock::ItemInfoDock(db::MySqlClient* dbClient, QWidget* parent)
    : QWidget(parent)
    , m_db(dbClient)
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(6, 6, 6, 6);
    root->setSpacing(4);

    m_header = new QLabel(tr("Double-click an item id in the loot or vendor dock."), this);
    m_header->setWordWrap(true);
    root->addWidget(m_header);

    // Quality-tinted item name header.  Mirrors the WoW tooltip header.
    m_nameLabel = new QLabel(this);
    m_nameLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_nameLabel->setStyleSheet(QStringLiteral("QLabel { font-weight: bold; font-size: 12pt; }"));
    root->addWidget(m_nameLabel);

    QString const mono = QStringLiteral("QLabel { font-family: monospace; }");

    m_identity = new QLabel(this);
    m_identity->setWordWrap(true);
    m_identity->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_identity->setStyleSheet(mono);
    root->addWidget(m_identity);

    m_levels = new QLabel(this);
    m_levels->setWordWrap(true);
    m_levels->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_levels->setStyleSheet(mono);
    root->addWidget(m_levels);

    m_statsHeader = new QLabel(this);
    m_statsHeader->setStyleSheet(mono);
    root->addWidget(m_statsHeader);

    m_statsTable = new QTableWidget(this);
    m_statsTable->setColumnCount(2);
    m_statsTable->setHorizontalHeaderLabels({ tr("stat"), tr("value") });
    m_statsTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_statsTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_statsTable->verticalHeader()->setVisible(false);
    m_statsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_statsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_statsTable->setAlternatingRowColors(true);
    // Stats table is short by design (max 10 rows); reserve enough
    // vertical space to render the full set without scrolling.
    m_statsTable->setMaximumHeight(220);
    root->addWidget(m_statsTable);

    m_damage = new QLabel(this);
    m_damage->setWordWrap(true);
    m_damage->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_damage->setStyleSheet(mono);
    root->addWidget(m_damage);

    m_resistances = new QLabel(this);
    m_resistances->setWordWrap(true);
    m_resistances->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_resistances->setStyleSheet(mono);
    root->addWidget(m_resistances);

    m_price = new QLabel(this);
    m_price->setWordWrap(true);
    m_price->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_price->setStyleSheet(mono);
    root->addWidget(m_price);

    m_bag = new QLabel(this);
    m_bag->setWordWrap(true);
    m_bag->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_bag->setStyleSheet(mono);
    root->addWidget(m_bag);

    m_questUse = new QLabel(this);
    m_questUse->setWordWrap(true);
    m_questUse->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_questUse->setStyleSheet(mono);
    root->addWidget(m_questUse);

    root->addStretch(1);

    clear();
}

void ItemInfoDock::clear()
{
    m_header->setText(tr("Double-click an item id in the loot or vendor dock."));
    m_nameLabel->clear();
    m_identity->clear();
    m_levels->clear();
    m_statsHeader->clear();
    m_statsTable->setRowCount(0);
    m_damage->clear();
    m_resistances->clear();
    m_price->clear();
    m_bag->clear();
    m_questUse->clear();
}

void ItemInfoDock::setItem(uint32_t itemId)
{
    clear();

    if (itemId == 0)
        return;

    if (!m_db || !m_db->isConnected())
    {
        m_header->setText(tr("DB not connected.  Item id = %1.").arg(itemId));
        return;
    }

    // Probe item_template existence with a cheap COUNT(*) so a missing
    // table surfaces as our fallback message rather than an SQL error
    // per cluster query.
    {
        std::string const probe = "SELECT entry FROM item_template WHERE entry = "
                                + std::to_string(itemId) + " LIMIT 1";
        db::QueryResult res;
        auto const err = m_db->query(probe, res);
        if (!err.ok())
        {
            if (err.code == kErrNoSuchTable)
            {
                m_header->setText(tr("Item %1 — (item_template not present; "
                    "hotfix DB not linked into editor)").arg(itemId));
                return;
            }
            m_header->setText(tr("Item %1 — query failed: %2")
                .arg(itemId)
                .arg(QString::fromStdString(err.message)));
            return;
        }
        if (res.rowCount() == 0)
        {
            m_header->setText(tr("Item %1 — no item_template row found.").arg(itemId));
            return;
        }
    }

    m_header->setText(tr("Item %1").arg(itemId));

    // Each section query is independent so a missing column in one
    // cluster (e.g. block, ContainerSlots) doesn't break the rest.
    populateIdentity   (itemId);
    populateLevels     (itemId);
    populateStats      (itemId);
    populateDamage     (itemId);
    populateResistances(itemId);
    populatePrice      (itemId);
    populateBag        (itemId);
    populateQuestUse   (itemId);
}

bool ItemInfoDock::populateIdentity(uint32_t itemId)
{
    std::string const sql =
        "SELECT name, Quality, class, subclass, InventoryType "
        "FROM item_template WHERE entry = " + std::to_string(itemId) + " LIMIT 1";
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
    uint32_t const quality   = static_cast<uint32_t>(res.asUInt64(0, 1).value_or(0));
    uint32_t const cls       = static_cast<uint32_t>(res.asUInt64(0, 2).value_or(0));
    uint32_t const sub       = static_cast<uint32_t>(res.asUInt64(0, 3).value_or(0));
    uint32_t const invType   = static_cast<uint32_t>(res.asUInt64(0, 4).value_or(0));

    // Tint the bold name header by quality.
    m_nameLabel->setText(name);
    m_nameLabel->setStyleSheet(QStringLiteral(
        "QLabel { font-weight: bold; font-size: 12pt; color: %1; }")
        .arg(QString::fromLatin1(qualityHexColor(quality))));

    m_identity->setText(
        tr("Entry:        %1\n"
           "Quality:      %2 (%3)\n"
           "Class:        %4 (%5) / sub %6\n"
           "Inv. type:    %7 (%8)")
            .arg(itemId)
            .arg(quality).arg(QString::fromLatin1(qualityName(quality)))
            .arg(cls).arg(QString::fromLatin1(itemClassName(cls))).arg(sub)
            .arg(invType).arg(QString::fromLatin1(inventoryTypeName(invType))));
    return true;
}

bool ItemInfoDock::populateLevels(uint32_t itemId)
{
    std::string const sql =
        "SELECT ItemLevel, RequiredLevel, RequiredSkill, RequiredSkillRank "
        "FROM item_template WHERE entry = " + std::to_string(itemId) + " LIMIT 1";
    db::QueryResult res;
    auto const err = m_db->query(sql, res);
    if (!err.ok())
    {
        if (err.code == kErrNoSuchColumn)
            return false;
        return false;
    }
    if (res.rowCount() == 0)
        return false;

    uint64_t const ilvl    = res.asUInt64(0, 0).value_or(0);
    uint64_t const reqLvl  = res.asUInt64(0, 1).value_or(0);
    uint64_t const reqSk   = res.asUInt64(0, 2).value_or(0);
    uint64_t const reqSkR  = res.asUInt64(0, 3).value_or(0);

    m_levels->setText(
        tr("ItemLevel:    %1\n"
           "ReqLevel:     %2\n"
           "ReqSkill:     %3 (rank %4)")
            .arg(ilvl).arg(reqLvl).arg(reqSk).arg(reqSkR));
    return true;
}

bool ItemInfoDock::populateStats(uint32_t itemId)
{
    // Stats are stored as ten parallel (StatType_i, StatValue_i) pairs.
    // Project all twenty columns in one query, then pivot non-zero
    // entries into a two-column table.
    std::string sql = "SELECT ";
    for (int i = 1; i <= 10; ++i)
    {
        sql += "stat_type"  + std::to_string(i) + ", ";
        sql += "stat_value" + std::to_string(i);
        if (i != 10) sql += ", ";
    }
    sql += " FROM item_template WHERE entry = " + std::to_string(itemId) + " LIMIT 1";

    db::QueryResult res;
    auto const err = m_db->query(sql, res);
    if (!err.ok() || res.rowCount() == 0)
    {
        // Hide the section entirely on schema mismatch.
        m_statsHeader->clear();
        m_statsTable->setRowCount(0);
        return false;
    }

    struct Row { QString name; int64_t value; };
    std::vector<Row> rows;
    rows.reserve(10);
    for (int i = 0; i < 10; ++i)
    {
        uint64_t const t = res.asUInt64(0, size_t(i * 2 + 0)).value_or(0);
        int64_t  const v = res.asInt64 (0, size_t(i * 2 + 1)).value_or(0);
        if (t == 0 && v == 0)
            continue;
        char const* name = itemModTypeName(static_cast<uint32_t>(t));
        QString display = name ? QString::fromLatin1(name)
                               : QStringLiteral("stat#%1").arg(t);
        rows.push_back({ display, v });
    }

    if (rows.empty())
    {
        m_statsHeader->clear();
        m_statsTable->setRowCount(0);
        return true;
    }

    m_statsHeader->setText(tr("Stats:"));
    m_statsTable->setRowCount(int(rows.size()));
    for (size_t r = 0; r < rows.size(); ++r)
    {
        auto* nameCell = new QTableWidgetItem(rows[r].name);
        nameCell->setFlags(nameCell->flags() & ~Qt::ItemIsEditable);
        m_statsTable->setItem(int(r), 0, nameCell);
        auto* valCell = new QTableWidgetItem(QString::number(rows[r].value));
        valCell->setFlags(valCell->flags() & ~Qt::ItemIsEditable);
        m_statsTable->setItem(int(r), 1, valCell);
    }
    return true;
}

bool ItemInfoDock::populateDamage(uint32_t itemId)
{
    std::string const sql =
        "SELECT dmg_min1, dmg_max1, dmg_type1, delay, armor, block "
        "FROM item_template WHERE entry = " + std::to_string(itemId) + " LIMIT 1";
    db::QueryResult res;
    auto const err = m_db->query(sql, res);
    if (!err.ok() || res.rowCount() == 0)
        return false;

    double  const dmgMin = res.asDouble(0, 0).value_or(0.0);
    double  const dmgMax = res.asDouble(0, 1).value_or(0.0);
    uint64_t const dType = res.asUInt64(0, 2).value_or(0);
    uint64_t const delay = res.asUInt64(0, 3).value_or(0);
    uint64_t const armor = res.asUInt64(0, 4).value_or(0);
    uint64_t const block = res.asUInt64(0, 5).value_or(0);

    QStringList lines;
    if (dmgMax > 0.0)
    {
        // Speed in seconds (delay is stored in ms).
        double const speed = double(delay) / 1000.0;
        lines << tr("Damage:       %1 - %2 %3 (speed %4s)")
            .arg(QString::number(dmgMin, 'f', 1))
            .arg(QString::number(dmgMax, 'f', 1))
            .arg(QString::fromLatin1(damageSchoolName(static_cast<uint32_t>(dType))))
            .arg(QString::number(speed, 'f', 2));
    }
    if (armor > 0) lines << tr("Armor:        %1").arg(armor);
    if (block > 0) lines << tr("Block:        %1").arg(block);

    if (lines.isEmpty())
        return false;
    m_damage->setText(lines.join(QChar('\n')));
    return true;
}

bool ItemInfoDock::populateResistances(uint32_t itemId)
{
    std::string const sql =
        "SELECT holy_res, fire_res, nature_res, frost_res, shadow_res, arcane_res "
        "FROM item_template WHERE entry = " + std::to_string(itemId) + " LIMIT 1";
    db::QueryResult res;
    auto const err = m_db->query(sql, res);
    if (!err.ok() || res.rowCount() == 0)
        return false;

    std::array<int64_t, 6> r{};
    for (int i = 0; i < 6; ++i)
        r[i] = res.asInt64(0, size_t(i)).value_or(0);

    // Skip the entire section if every school is zero - most items have
    // no resistances and an empty block is noise.
    bool any = false;
    for (int v : { (int)r[0], (int)r[1], (int)r[2], (int)r[3], (int)r[4], (int)r[5] })
        if (v != 0) { any = true; break; }
    if (!any)
        return false;

    static char const* const names[6] = {
        "Holy", "Fire", "Nature", "Frost", "Shadow", "Arcane"
    };
    QStringList lines;
    lines << tr("Resistances:");
    for (int i = 0; i < 6; ++i)
        if (r[i] != 0)
            lines << tr("  %1: %2").arg(QString::fromLatin1(names[i])).arg(r[i]);
    m_resistances->setText(lines.join(QChar('\n')));
    return true;
}

bool ItemInfoDock::populatePrice(uint32_t itemId)
{
    std::string const sql =
        "SELECT BuyPrice, SellPrice "
        "FROM item_template WHERE entry = " + std::to_string(itemId) + " LIMIT 1";
    db::QueryResult res;
    auto const err = m_db->query(sql, res);
    if (!err.ok() || res.rowCount() == 0)
        return false;

    uint64_t const buy  = res.asUInt64(0, 0).value_or(0);
    uint64_t const sell = res.asUInt64(0, 1).value_or(0);

    m_price->setText(
        tr("Buy price:    %1\n"
           "Sell price:   %2")
            .arg(prettyMoney(buy))
            .arg(prettyMoney(sell)));
    return true;
}

bool ItemInfoDock::populateBag(uint32_t itemId)
{
    std::string const sql =
        "SELECT ContainerSlots, BagFamily "
        "FROM item_template WHERE entry = " + std::to_string(itemId) + " LIMIT 1";
    db::QueryResult res;
    auto const err = m_db->query(sql, res);
    if (!err.ok() || res.rowCount() == 0)
        return false;

    // NB: `slots` is reserved by Qt's signals/slots macro - using a
    // different identifier keeps MOC's preprocessor pass happy.
    uint64_t const containerSlots = res.asUInt64(0, 0).value_or(0);
    uint64_t const bagFamily      = res.asUInt64(0, 1).value_or(0);

    // Skip the section unless either field is non-zero; most items
    // aren't bags and a blank "0 slots / family 0" block is noise.
    if (containerSlots == 0 && bagFamily == 0)
        return false;

    m_bag->setText(
        tr("ContainerSlots: %1\n"
           "BagFamily:    0x%2")
            .arg(containerSlots)
            .arg(QString::number(bagFamily, 16)));
    return true;
}

bool ItemInfoDock::populateQuestUse(uint32_t itemId)
{
    std::string const sql =
        "SELECT bonding, MaxCount, stackable, Flags "
        "FROM item_template WHERE entry = " + std::to_string(itemId) + " LIMIT 1";
    db::QueryResult res;
    auto const err = m_db->query(sql, res);
    if (!err.ok() || res.rowCount() == 0)
        return false;

    uint64_t const bond  = res.asUInt64(0, 0).value_or(0);
    uint64_t const mxCnt = res.asUInt64(0, 1).value_or(0);
    uint64_t const stack = res.asUInt64(0, 2).value_or(0);
    uint64_t const flags = res.asUInt64(0, 3).value_or(0);

    m_questUse->setText(
        tr("Bonding:      %1 (%2)\n"
           "MaxCount:     %3\n"
           "Stackable:    %4\n"
           "Flags:        0x%5")
            .arg(bond).arg(QString::fromLatin1(bondingName(static_cast<uint32_t>(bond))))
            .arg(mxCnt == 0 ? tr("unlimited") : QString::number(mxCnt))
            .arg(stack)
            .arg(QString::number(flags, 16)));
    return true;
}

} // namespace world_editor::app
