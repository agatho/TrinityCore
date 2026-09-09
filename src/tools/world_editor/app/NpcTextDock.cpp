#include "NpcTextDock.h"

#include "../db/MySqlClient.h"

#include <QFrame>
#include <QLabel>
#include <QScrollArea>
#include <QVBoxLayout>

#include <array>
#include <set>
#include <string>

namespace world_editor::app
{

// Single variant snapshot pulled from one npc_text row.  Eight live in
// each row (text0_* .. text7_*).
struct NpcTextVariant
{
    bool     hasRow         = false;   // set when at least one column was non-NULL.
    float    probability    = 0.0f;
    uint32_t broadcastId    = 0;
    QString  textMale;
    QString  textFemale;
    int32_t  language       = -1;      // -1 = "not present in schema"
    std::array<int32_t, 3> emotes { -1, -1, -1 };
};

// One npc_text row = up to 8 variants, packaged so renderRow doesn't
// have to walk a QueryResult.
struct NpcTextRow
{
    bool                              found = false;
    std::array<NpcTextVariant, 8>     v{};
};

namespace
{

constexpr uint32_t kErrNoSuchTable  = 1146;
constexpr uint32_t kErrNoSuchColumn = 1054;

// Probe INFORMATION_SCHEMA for the column set of `table`.  Empty result
// means "table absent in this schema".
std::set<std::string, std::less<>> discoverColumns(db::MySqlClient& db, char const* table)
{
    std::set<std::string, std::less<>> cols;
    std::string const sql =
        "SELECT COLUMN_NAME FROM INFORMATION_SCHEMA.COLUMNS "
        "WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = '" + std::string(table) + "'";
    db::QueryResult res;
    auto const err = db.query(sql, res);
    if (!err.ok()) return cols;
    for (size_t r = 0; r < res.rowCount(); ++r)
        cols.insert(res.cell(r, 0));
    return cols;
}

// Project `col AS alias` when present, "NULL AS alias" otherwise so the
// result-set keeps stable aliases independent of fork-schema deltas.
std::string projectAs(std::set<std::string, std::less<>> const& cols,
                      char const* col, char const* alias)
{
    if (cols.find(col) != cols.end())
        return std::string(col) + " AS " + alias;
    return std::string("NULL AS ") + alias;
}

QString cellByAlias(db::QueryResult const& res, char const* alias)
{
    auto idx = res.columnIndex(alias);
    if (!idx) return {};
    if (res.isNull(0, *idx)) return {};
    return QString::fromStdString(res.cell(0, *idx));
}

// Small subset of TC's Language enum.  Unknown ids render with their raw
// id appended.  Only the common worldserver languages are decoded; rarely
// used races (Forsaken/Tauren etc.) intentionally fall through to "id=N".
char const* decodeLanguage(int32_t lang)
{
    switch (lang)
    {
        case 0:  return "Universal";
        case 1:  return "Orcish";
        case 2:  return "Darnassian";
        case 3:  return "Taurahe";
        case 6:  return "Dwarvish";
        case 7:  return "Common";
        case 8:  return "Demonic";
        case 9:  return "Titan";
        case 10: return "Thalassian";
        case 11: return "Draconic";
        case 12: return "Kalimag";
        case 13: return "Gnomish";
        case 14: return "Troll";
        case 33: return "Draenei";
        case 35: return "Zombie";
        case 36: return "GnomishBinary";
        case 37: return "GoblinBinary";
        case 40: return "Worgen";
        case 41: return "Goblin";
        case 42: return "Pandaren (Neutral)";
        case 43: return "Pandaren (Alliance)";
        case 44: return "Pandaren (Horde)";
        default: return nullptr;
    }
}

QString formatLanguage(int32_t lang)
{
    if (lang < 0) return QStringLiteral("(n/a)");
    if (char const* name = decodeLanguage(lang))
        return QStringLiteral("%1 (%2)").arg(QString::fromLatin1(name)).arg(lang);
    return QStringLiteral("id=%1").arg(lang);
}

// Parse a string cell as int32, returning fallback when empty / unparsable.
int32_t parseInt(QString const& s, int32_t fallback)
{
    if (s.isEmpty()) return fallback;
    bool okParse = false;
    int32_t const v = s.toInt(&okParse);
    return okParse ? v : fallback;
}

uint32_t parseUInt(QString const& s, uint32_t fallback)
{
    if (s.isEmpty()) return fallback;
    bool okParse = false;
    uint32_t const v = s.toUInt(&okParse);
    return okParse ? v : fallback;
}

float parseFloat(QString const& s, float fallback)
{
    if (s.isEmpty()) return fallback;
    bool okParse = false;
    float const v = s.toFloat(&okParse);
    return okParse ? v : fallback;
}

} // namespace

NpcTextDock::NpcTextDock(db::MySqlClient* dbClient, QWidget* parent)
    : QWidget(parent)
    , m_db(dbClient)
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(6, 6, 6, 6);
    root->setSpacing(4);

    m_header = new QLabel(this);
    m_header->setWordWrap(true);
    m_header->setStyleSheet(QStringLiteral("QLabel { font-weight: bold; font-size: 11pt; }"));
    root->addWidget(m_header);

    // Scroll area holds the variant widgets; we rebuild its inner widget
    // on every refresh so we don't have to maintain 8 fixed slots.
    m_scroll = new QScrollArea(this);
    m_scroll->setWidgetResizable(true);
    m_scroll->setFrameShape(QFrame::NoFrame);
    root->addWidget(m_scroll, 1);

    m_variantHost = new QWidget(m_scroll);
    auto* hostLayout = new QVBoxLayout(m_variantHost);
    hostLayout->setContentsMargins(0, 0, 0, 0);
    hostLayout->setSpacing(8);
    hostLayout->addStretch(1);
    m_scroll->setWidget(m_variantHost);

    clear();
}

void NpcTextDock::clear()
{
    m_header->setText(tr("Use Tools -> Lookup NPCText by ID... to load a row."));

    // Replace the host widget so all existing variant widgets are deleted.
    auto* fresh = new QWidget(m_scroll);
    auto* layout = new QVBoxLayout(fresh);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);
    layout->addStretch(1);
    m_scroll->setWidget(fresh);
    m_variantHost = fresh;
}

void NpcTextDock::setNpcTextId(uint32_t textId)
{
    clear();

    if (textId == 0)
        return;

    if (!m_db || !m_db->isConnected())
    {
        m_header->setText(tr("DB not connected.  NPCText id = %1.").arg(textId));
        return;
    }

    auto const cols = discoverColumns(*m_db, "npc_text");
    if (cols.empty())
    {
        m_header->setText(tr("npc_text table not present in this schema (id = %1).").arg(textId));
        return;
    }

    std::string idCol;
    for (char const* c : { "ID", "Id", "id" })
        if (cols.find(c) != cols.end()) { idCol = c; break; }
    if (idCol.empty())
    {
        m_header->setText(tr("npc_text: no id column found (id = %1).").arg(textId));
        return;
    }

    // Build SELECT projecting every per-variant column with a stable alias.
    // Missing columns fall back to NULL so the result-set ordinal stays
    // stable across fork schemas.
    std::string sql = "SELECT ";
    sql += idCol + " AS npc_text_id";
    for (int i = 0; i < 8; ++i)
    {
        std::string const si = std::to_string(i);
        // Probability + BroadcastText id + male/female text.
        sql += ", " + projectAs(cols, ("prob"          + si).c_str(),
                                ("prob" + si).c_str());
        sql += ", " + projectAs(cols, ("BroadcastTextID" + si).c_str(),
                                ("bcast" + si).c_str());
        sql += ", " + projectAs(cols, ("text" + si + "_0").c_str(),
                                ("t" + si + "_0").c_str());
        sql += ", " + projectAs(cols, ("text" + si + "_1").c_str(),
                                ("t" + si + "_1").c_str());
        sql += ", " + projectAs(cols, ("lang" + si).c_str(),
                                ("lang" + si).c_str());
        for (int e = 0; e < 3; ++e)
        {
            std::string const se = std::to_string(e);
            sql += ", " + projectAs(cols, ("em" + si + "_" + se).c_str(),
                                    ("em" + si + "_" + se).c_str());
        }
    }
    sql += " FROM npc_text WHERE " + idCol + " = " + std::to_string(textId) + " LIMIT 1";

    db::QueryResult res;
    auto const err = m_db->query(sql, res);
    if (!err.ok())
    {
        if (err.code == kErrNoSuchTable || err.code == kErrNoSuchColumn)
        {
            m_header->setText(tr("npc_text schema unexpected (id = %1): %2")
                .arg(textId).arg(QString::fromStdString(err.message)));
            return;
        }
        m_header->setText(tr("npc_text query failed for id %1: %2")
            .arg(textId).arg(QString::fromStdString(err.message)));
        return;
    }

    if (res.rowCount() == 0)
    {
        m_header->setText(tr("No npc_text row found for id %1").arg(textId));
        return;
    }

    NpcTextRow row;
    row.found = true;
    for (int i = 0; i < 8; ++i)
    {
        std::string const si = std::to_string(i);
        NpcTextVariant& v = row.v[i];

        QString const probStr = cellByAlias(res, ("prob" + si).c_str());
        v.probability = parseFloat(probStr, 0.0f);
        v.broadcastId = parseUInt(cellByAlias(res, ("bcast" + si).c_str()), 0);
        v.textMale    = cellByAlias(res, ("t" + si + "_0").c_str());
        v.textFemale  = cellByAlias(res, ("t" + si + "_1").c_str());
        v.language    = parseInt(cellByAlias(res, ("lang" + si).c_str()), -1);
        for (int e = 0; e < 3; ++e)
            v.emotes[e] = parseInt(
                cellByAlias(res, ("em" + si + "_" + std::to_string(e)).c_str()),
                -1);

        // Mark "has content" iff any of the rendered fields carry data;
        // empty / zero-prob variants are suppressed entirely.
        v.hasRow = v.probability > 0.0f
                || v.broadcastId != 0
                || !v.textMale.isEmpty()
                || !v.textFemale.isEmpty();
    }

    renderRow(textId, row);
}

void NpcTextDock::renderRow(uint32_t textId, NpcTextRow const& row)
{
    m_header->setText(tr("NPCText id %1").arg(textId));

    // Fresh host widget so we don't accumulate orphan layouts across refreshes.
    auto* fresh = new QWidget(m_scroll);
    auto* hostLayout = new QVBoxLayout(fresh);
    hostLayout->setContentsMargins(0, 0, 0, 0);
    hostLayout->setSpacing(8);

    QString const mono = QStringLiteral("QLabel { font-family: monospace; }");

    int rendered = 0;
    for (int i = 0; i < 8; ++i)
    {
        NpcTextVariant const& v = row.v[i];
        if (!v.hasRow)
            continue;
        ++rendered;

        // Section frame so each variant is visually grouped.
        auto* section = new QFrame(fresh);
        section->setFrameShape(QFrame::StyledPanel);
        section->setFrameShadow(QFrame::Sunken);
        auto* sLayout = new QVBoxLayout(section);
        sLayout->setContentsMargins(6, 4, 6, 6);
        sLayout->setSpacing(2);

        auto* title = new QLabel(section);
        title->setText(tr("Variant %1").arg(i));
        title->setStyleSheet(QStringLiteral("QLabel { font-weight: bold; }"));
        sLayout->addWidget(title);

        auto* meta = new QLabel(section);
        meta->setWordWrap(true);
        meta->setStyleSheet(mono);
        meta->setTextInteractionFlags(Qt::TextSelectableByMouse);
        meta->setText(
            tr("Probability:  %1\n"
               "BroadcastText: %2 (client-side)\n"
               "Language:     %3")
                .arg(v.probability, 0, 'f', 3)
                .arg(v.broadcastId)
                .arg(formatLanguage(v.language)));
        sLayout->addWidget(meta);

        // Male text - rendered as a multi-line read-only label.  Empty is
        // shown as a parenthesized hint so the operator can tell "no text"
        // from "no schema".
        auto* textM = new QLabel(section);
        textM->setWordWrap(true);
        textM->setTextInteractionFlags(Qt::TextSelectableByMouse);
        textM->setStyleSheet(QStringLiteral(
            "QLabel { background-color: rgba(0,0,0,0.06); padding: 4px; }"));
        textM->setText(tr("Male:\n%1").arg(
            v.textMale.isEmpty() ? QStringLiteral("(empty)") : v.textMale));
        sLayout->addWidget(textM);

        // Female text - suppressed entirely when identical to male (the
        // common case) so the dock isn't cluttered with duplicates.
        if (!v.textFemale.isEmpty() && v.textFemale != v.textMale)
        {
            auto* textF = new QLabel(section);
            textF->setWordWrap(true);
            textF->setTextInteractionFlags(Qt::TextSelectableByMouse);
            textF->setStyleSheet(QStringLiteral(
                "QLabel { background-color: rgba(0,0,0,0.06); padding: 4px; }"));
            textF->setText(tr("Female:\n%1").arg(v.textFemale));
            sLayout->addWidget(textF);
        }

        // Raw emote ids; not decoded (Emotes.db2 hotfix not mirrored).
        auto* emoteLabel = new QLabel(section);
        emoteLabel->setStyleSheet(mono);
        emoteLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
        auto emoteCell = [&](int32_t id) -> QString {
            return id < 0 ? QStringLiteral("(n/a)") : QString::number(id);
        };
        emoteLabel->setText(
            tr("Emotes:       %1, %2, %3")
                .arg(emoteCell(v.emotes[0]))
                .arg(emoteCell(v.emotes[1]))
                .arg(emoteCell(v.emotes[2])));
        sLayout->addWidget(emoteLabel);

        hostLayout->addWidget(section);
    }

    if (rendered == 0)
    {
        auto* note = new QLabel(fresh);
        note->setText(tr("(row exists but every variant is empty / zero-prob)"));
        note->setStyleSheet(QStringLiteral("QLabel { color: gray; font-style: italic; }"));
        hostLayout->addWidget(note);
    }

    hostLayout->addStretch(1);
    m_scroll->setWidget(fresh);
    m_variantHost = fresh;
}

} // namespace world_editor::app
