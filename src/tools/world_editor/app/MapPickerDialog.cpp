#include "MapPickerDialog.h"

#include <QDialogButtonBox>
#include <QFont>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>

#include <algorithm>
#include <array>
#include <functional>

namespace world_editor::app
{

namespace
{
constexpr int kMapIdRole = Qt::UserRole;        // leaf -> map id (int)
constexpr int kSearchRole = Qt::UserRole + 1;   // leaf -> lowercase "id name" haystack

// Instance-type groups in operator-priority order: continents (the road hot
// path) first, instanced content after.
struct TypeGroup { uint8_t type; char const* label; };
constexpr std::array<TypeGroup, 7> kTypeGroups = {{
    { 0,    "Continents (open world)" },
    { 1,    "Dungeons" },
    { 2,    "Raids" },
    { 3,    "Battlegrounds" },
    { 4,    "Arenas" },
    { 5,    "Scenarios" },
    { 0xFF, "Other" },
}};

char const* expansionName(uint8_t e)
{
    static char const* const kNames[] = {
        "Classic", "The Burning Crusade", "Wrath of the Lich King", "Cataclysm",
        "Mists of Pandaria", "Warlords of Draenor", "Legion", "Battle for Azeroth",
        "Shadowlands", "Dragonflight", "The War Within"
    };
    if (e < (sizeof(kNames) / sizeof(kNames[0])))
        return kNames[e];
    return "Other / Unknown expansion";
}

// Map an arbitrary InstanceType byte to one of the group buckets above.
uint8_t groupTypeOf(uint8_t t)
{
    return (t <= 5) ? t : uint8_t(0xFF);
}
} // namespace

MapPickerDialog::MapPickerDialog(std::vector<io::MapMetadata> maps,
                                 std::set<uint32_t> availableMapIds,
                                 std::map<uint32_t, int> roadCounts,
                                 std::vector<uint32_t> recentMapIds,
                                 QWidget* parent)
    : QDialog(parent)
    , m_maps(std::move(maps))
    , m_available(std::move(availableMapIds))
    , m_roadCounts(std::move(roadCounts))
    , m_recent(std::move(recentMapIds))
{
    setWindowTitle(tr("Open map"));
    setModal(true);
    resize(560, 640);

    m_filter = new QLineEdit(this);
    m_filter->setPlaceholderText(tr("Filter by name or id..."));
    m_filter->setClearButtonEnabled(true);

    m_tree = new QTreeWidget(this);
    m_tree->setHeaderHidden(true);
    m_tree->setUniformRowHeights(true);
    m_tree->setExpandsOnDoubleClick(false);

    auto* hint = new QLabel(
        tr("Only maps with extracted mmaps are shown. ▸ = has authored roads."), this);
    hint->setStyleSheet(QStringLiteral("color: gray;"));

    m_buttons = new QDialogButtonBox(QDialogButtonBox::Open | QDialogButtonBox::Cancel, this);
    m_buttons->button(QDialogButtonBox::Open)->setEnabled(false);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(m_filter);
    layout->addWidget(m_tree, 1);
    layout->addWidget(hint);
    layout->addWidget(m_buttons);

    connect(m_filter, &QLineEdit::textChanged, this, &MapPickerDialog::onFilterChanged);
    connect(m_tree, &QTreeWidget::itemActivated, this, &MapPickerDialog::onItemActivated);
    connect(m_tree, &QTreeWidget::itemDoubleClicked, this, &MapPickerDialog::onItemActivated);
    connect(m_tree, &QTreeWidget::itemSelectionChanged, this, &MapPickerDialog::onSelectionChanged);
    connect(m_buttons, &QDialogButtonBox::accepted, this, &MapPickerDialog::onAccept);
    connect(m_buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    build();
    m_filter->setFocus();
}

void MapPickerDialog::addLeaf(QTreeWidgetItem* parent, io::MapMetadata const& m)
{
    auto it = m_roadCounts.find(m.mapId);
    int const roads = (it != m_roadCounts.end()) ? it->second : 0;

    std::string const dn = m.displayName();
    QString const nameText = dn.empty() ? tr("(unnamed)") : QString::fromStdString(dn);
    QString label = QStringLiteral("%1 — %2").arg(m.mapId).arg(nameText);
    if (roads > 0)
        label += tr("   ▸ %1 roads").arg(roads);

    auto* leaf = new QTreeWidgetItem(parent);
    leaf->setText(0, label);
    leaf->setData(0, kMapIdRole, int(m.mapId));
    leaf->setData(0, kSearchRole,
                  QStringLiteral("%1 %2").arg(m.mapId)
                      .arg(QString::fromStdString(m.displayName())).toLower());
    if (roads > 0)
    {
        QFont f = leaf->font(0);
        f.setBold(true);
        leaf->setFont(0, f);
    }
}

void MapPickerDialog::build()
{
    m_tree->clear();

    // --- Recent group (verbatim order, only still-available maps) ----------
    if (!m_recent.empty())
    {
        QTreeWidgetItem* recentNode = nullptr;
        for (uint32_t id : m_recent)
        {
            if (!m_available.empty() && m_available.count(id) == 0)
                continue;
            auto mit = std::find_if(m_maps.begin(), m_maps.end(),
                [id](io::MapMetadata const& mm) { return mm.mapId == id; });
            io::MapMetadata meta;
            if (mit != m_maps.end())
                meta = *mit;
            else { meta.mapId = id; }
            if (!recentNode)
            {
                recentNode = new QTreeWidgetItem(m_tree);
                recentNode->setText(0, tr("Recent"));
                QFont f = recentNode->font(0); f.setBold(true); recentNode->setFont(0, f);
            }
            addLeaf(recentNode, meta);
        }
        if (recentNode)
            recentNode->setExpanded(true);
    }

    // --- Type -> expansion grouping ----------------------------------------
    // bucket[groupType][expansion] -> maps
    std::map<uint8_t, std::map<uint8_t, std::vector<io::MapMetadata>>> buckets;
    for (io::MapMetadata const& m : m_maps)
    {
        // Gate to maps that can actually be loaded (have mmaps). If we have no
        // availability info at all, show everything rather than an empty tree.
        if (!m_available.empty() && m_available.count(m.mapId) == 0)
            continue;
        buckets[groupTypeOf(m.instanceType)][m.expansionId].push_back(m);
    }

    for (auto const& tg : kTypeGroups)
    {
        auto bit = buckets.find(tg.type);
        if (bit == buckets.end())
            continue;

        auto* typeNode = new QTreeWidgetItem(m_tree);
        typeNode->setText(0, tr(tg.label));
        QFont tf = typeNode->font(0); tf.setBold(true); typeNode->setFont(0, tf);

        for (auto& [exp, list] : bit->second)
        {
            auto* expNode = new QTreeWidgetItem(typeNode);
            expNode->setText(0, QString::fromLatin1(expansionName(exp)));

            std::sort(list.begin(), list.end(),
                [](io::MapMetadata const& a, io::MapMetadata const& b) { return a.mapId < b.mapId; });
            for (io::MapMetadata const& m : list)
                addLeaf(expNode, m);
        }
        // Continents expanded by default (the hot path); the rest collapsed.
        typeNode->setExpanded(tg.type == 0);
    }
}

void MapPickerDialog::onFilterChanged(QString const& text)
{
    QString const needle = text.trimmed().toLower();

    std::function<bool(QTreeWidgetItem*)> visit = [&](QTreeWidgetItem* item) -> bool
    {
        // Leaf: matches if its haystack contains the needle (or no filter).
        if (item->data(0, kMapIdRole).isValid())
        {
            bool const match = needle.isEmpty()
                || item->data(0, kSearchRole).toString().contains(needle);
            item->setHidden(!match);
            return match;
        }
        // Group: visible if any descendant is visible.
        bool anyVisible = false;
        for (int i = 0; i < item->childCount(); ++i)
            anyVisible = visit(item->child(i)) | anyVisible;
        item->setHidden(!anyVisible);
        if (!needle.isEmpty() && anyVisible)
            item->setExpanded(true);
        return anyVisible;
    };

    for (int i = 0; i < m_tree->topLevelItemCount(); ++i)
        visit(m_tree->topLevelItem(i));
}

void MapPickerDialog::onSelectionChanged()
{
    auto items = m_tree->selectedItems();
    bool const isLeaf = !items.isEmpty() && items.first()->data(0, kMapIdRole).isValid();
    m_buttons->button(QDialogButtonBox::Open)->setEnabled(isLeaf);
}

void MapPickerDialog::onItemActivated(QTreeWidgetItem* item, int /*column*/)
{
    if (!item)
        return;
    QVariant const v = item->data(0, kMapIdRole);
    if (v.isValid())
    {
        m_selected = v.toInt();
        accept();
    }
    else
    {
        item->setExpanded(!item->isExpanded());
    }
}

void MapPickerDialog::onAccept()
{
    auto items = m_tree->selectedItems();
    if (!items.isEmpty())
    {
        QVariant const v = items.first()->data(0, kMapIdRole);
        if (v.isValid())
        {
            m_selected = v.toInt();
            accept();
            return;
        }
    }
    // No leaf selected — keep the dialog open.
}

} // namespace world_editor::app
