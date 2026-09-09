#include "FlagPickerDialog.h"

#include "FlagMetadata.gen.h"   // build-time codegen from core UnitDefines.h

#include <QCheckBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QLabel>
#include <QLineEdit>
#include <QScrollArea>
#include <QString>
#include <QVBoxLayout>
#include <QWidget>

#include <vector>

namespace world_editor::app
{
namespace
{

// One selectable flag: its 64-bit value (already shifted for the high group),
// the friendly label and the raw enum name (shown as a tooltip / search hint).
struct Item
{
    uint64_t    value;
    QString     label;
    QString     name;
};

// Build an Item list from a generated FlagEntry[] table, shifting each value
// left by `shift` bits (32 for the NPCFlags2 high half of npcflag).
std::vector<Item> itemsFrom(world_editor::flags::FlagEntry const* arr, int count, int shift)
{
    std::vector<Item> out;
    out.reserve(size_t(count));
    for (int i = 0; i < count; ++i)
        out.push_back({ uint64_t(arr[i].value) << shift,
                        QString::fromLatin1(arr[i].label),
                        QString::fromLatin1(arr[i].name) });
    return out;
}

// Modal checkbox list.  Returns the edited mask via result(); preserves any
// bits of `current` that have no checkbox so unknown/reserved flags survive.
class PickerDialog final : public QDialog
{
public:
    PickerDialog(QString const& title, std::vector<Item> items,
                 uint64_t current, QWidget* parent)
        : QDialog(parent), m_items(std::move(items)), m_current(current)
    {
        setWindowTitle(title);
        setModal(true);
        resize(420, 540);

        auto* outer = new QVBoxLayout(this);

        auto* filter = new QLineEdit(this);
        filter->setPlaceholderText(tr("Filter flags..."));
        outer->addWidget(filter);

        auto* scroll = new QScrollArea(this);
        scroll->setWidgetResizable(true);
        auto* host = new QWidget(scroll);
        auto* col  = new QVBoxLayout(host);

        uint64_t listed = 0;
        m_boxes.reserve(m_items.size());
        for (Item const& it : m_items)
        {
            listed |= it.value;
            auto* cb = new QCheckBox(QStringLiteral("%1   (%2 = 0x%3)")
                .arg(it.label, it.name, QString::number(it.value, 16)), host);
            cb->setChecked((current & it.value) == it.value && it.value != 0);
            cb->setToolTip(it.name);
            col->addWidget(cb);
            m_boxes.push_back(cb);
        }
        col->addStretch(1);
        scroll->setWidget(host);
        outer->addWidget(scroll, 1);

        // Bits set in `current` with no checkbox -- carried through verbatim.
        m_unknownBits = current & ~listed;

        auto* note = new QLabel(this);
        if (m_unknownBits)
            note->setText(tr("Note: 0x%1 has no named flag and will be preserved.")
                .arg(QString::number(m_unknownBits, 16)));
        note->setStyleSheet(QStringLiteral("color: #888; font-style: italic;"));
        outer->addWidget(note);

        auto* buttons = new QDialogButtonBox(
            QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
        outer->addWidget(buttons);
        connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

        // Live text filter over label + enum name.
        connect(filter, &QLineEdit::textChanged, this, [this](QString const& t) {
            for (size_t i = 0; i < m_boxes.size(); ++i)
            {
                bool const match = t.isEmpty()
                    || m_items[i].label.contains(t, Qt::CaseInsensitive)
                    || m_items[i].name.contains(t, Qt::CaseInsensitive);
                m_boxes[i]->setVisible(match);
            }
        });
    }

    [[nodiscard]] uint64_t result() const
    {
        uint64_t v = m_unknownBits;
        for (size_t i = 0; i < m_boxes.size(); ++i)
            if (m_boxes[i]->isChecked())
                v |= m_items[i].value;
        return v;
    }

private:
    std::vector<Item>        m_items;
    std::vector<QCheckBox*>  m_boxes;
    uint64_t                 m_current     = 0;
    uint64_t                 m_unknownBits = 0;
};

std::optional<uint64_t> runPicker(QString const& title, std::vector<Item> items,
                                  uint64_t current, QWidget* parent)
{
    PickerDialog dlg(title, std::move(items), current, parent);
    if (dlg.exec() != QDialog::Accepted)
        return std::nullopt;
    return dlg.result();
}

} // namespace

std::optional<uint64_t> pickNpcFlags(QWidget* parent, uint64_t current)
{
    using namespace world_editor::flags;
    std::vector<Item> items = itemsFrom(kNpcFlags, kNpcFlagsCount, 0);
    // NPCFlags2 occupy the high 32 bits of creature.npcflag.
    std::vector<Item> hi = itemsFrom(kNpcFlags2, kNpcFlags2Count, 32);
    items.insert(items.end(), hi.begin(), hi.end());
    return runPicker(QObject::tr("Edit npcflag"), std::move(items), current, parent);
}

std::optional<uint32_t> pickUnitFlags(QWidget* parent, uint32_t current)
{
    using namespace world_editor::flags;
    auto r = runPicker(QObject::tr("Edit unit_flags"),
                       itemsFrom(kUnitFlags, kUnitFlagsCount, 0), current, parent);
    if (!r) return std::nullopt;
    return uint32_t(*r);
}

std::optional<uint32_t> pickUnitFlags2(QWidget* parent, uint32_t current)
{
    using namespace world_editor::flags;
    auto r = runPicker(QObject::tr("Edit unit_flags2"),
                       itemsFrom(kUnitFlags2, kUnitFlags2Count, 0), current, parent);
    if (!r) return std::nullopt;
    return uint32_t(*r);
}

std::optional<uint32_t> pickUnitFlags3(QWidget* parent, uint32_t current)
{
    using namespace world_editor::flags;
    auto r = runPicker(QObject::tr("Edit unit_flags3"),
                       itemsFrom(kUnitFlags3, kUnitFlags3Count, 0), current, parent);
    if (!r) return std::nullopt;
    return uint32_t(*r);
}

} // namespace world_editor::app
