#include "CsvImportDialog.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSettings>
#include <QStringList>
#include <QTextStream>
#include <QVBoxLayout>

#include <algorithm>
#include <cstdint>
#include <limits>

namespace world_editor::app
{

namespace
{

constexpr int kPreviewLines = 20;
constexpr char kSettingsKey[] = "editor/csv_import_last_path";

// One known CSV column.  Maps a header token to the assignment that pours the parsed value
// into a render::Spawn.  Keeps the parse loop driven by data instead of a megaswitch.
struct ColumnSetter
{
    char const* name;
    // Returns false on a non-numeric/out-of-range value so the row is rejected.
    bool (*apply)(render::Spawn& s, QString const& cell);
};

// Tolerant double parser: empty cell = leave default (returns true with no assignment).
bool parseFloatField(QString const& cell, float& out)
{
    QString const t = cell.trimmed();
    if (t.isEmpty()) return true;
    bool ok = false;
    double const v = t.toDouble(&ok);
    if (!ok) return false;
    out = static_cast<float>(v);
    return true;
}

bool parseU32Field(QString const& cell, uint32_t& out)
{
    QString const t = cell.trimmed();
    if (t.isEmpty()) return true;
    bool ok = false;
    qulonglong const v = t.toULongLong(&ok);
    if (!ok) return false;
    if (v > std::numeric_limits<uint32_t>::max()) return false;
    out = static_cast<uint32_t>(v);
    return true;
}

ColumnSetter const kColumns[] = {
    { "entry",         [](render::Spawn& s, QString const& c) { return parseU32Field(c, s.entry); } },
    { "x",             [](render::Spawn& s, QString const& c) { return parseFloatField(c, s.worldX); } },
    { "y",             [](render::Spawn& s, QString const& c) { return parseFloatField(c, s.worldY); } },
    { "z",             [](render::Spawn& s, QString const& c) { return parseFloatField(c, s.worldZ); } },
    { "orientation",   [](render::Spawn& s, QString const& c) { return parseFloatField(c, s.orientation); } },
    { "mapid",         [](render::Spawn& s, QString const& c) { return parseU32Field(c, s.mapId); } },
    { "spawntimesecs", [](render::Spawn& s, QString const& c) { return parseU32Field(c, s.spawntimesecs); } },
    { "phaseid",       [](render::Spawn& s, QString const& c) { return parseU32Field(c, s.phaseId); } },
};

// Resolve header tokens (case-insensitive) -> setter index.  -1 marks unknown columns; those
// are silently ignored to keep operator-authored CSVs forgiving.
int resolveHeader(QString const& tok)
{
    QString const norm = tok.trimmed().toLower();
    for (size_t i = 0; i < sizeof(kColumns) / sizeof(kColumns[0]); ++i)
        if (norm == QLatin1String(kColumns[i].name))
            return static_cast<int>(i);
    return -1;
}

// Split one CSV record on commas.  No quoting / escapes -- the schema is numeric-only so
// QString::split is the simplest correct implementation.
QStringList splitRow(QString const& line)
{
    return line.split(QLatin1Char(','), Qt::KeepEmptyParts);
}

} // namespace

CsvImportDialog::CsvImportDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Import spawns from CSV"));
    setModal(true);
    resize(640, 480);

    auto* form = new QFormLayout;

    // Row 1: file path + Browse button.
    auto* pathRow = new QHBoxLayout;
    m_pathEdit = new QLineEdit(this);
    m_browseBtn = new QPushButton(tr("Browse..."), this);
    pathRow->addWidget(m_pathEdit, /*stretch*/ 1);
    pathRow->addWidget(m_browseBtn);
    form->addRow(tr("CSV file:"), pathRow);

    // Row 2: kind.
    m_kindCombo = new QComboBox(this);
    m_kindCombo->addItem(tr("Creature"),   static_cast<int>(render::SpawnKind::Creature));
    m_kindCombo->addItem(tr("GameObject"), static_cast<int>(render::SpawnKind::GameObject));
    form->addRow(tr("Kind:"), m_kindCombo);

    // Row 3: preview.
    m_preview = new QPlainTextEdit(this);
    m_preview->setReadOnly(true);
    m_preview->setPlaceholderText(tr("Preview of first %1 lines appears here once a file is chosen.").arg(kPreviewLines));
    form->addRow(tr("Preview:"), m_preview);

    // Row 4: parse status.
    m_statusLabel = new QLabel(tr("No file selected."), this);
    form->addRow(tr("Status:"), m_statusLabel);

    // Dialog buttons.
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Cancel, this);
    m_importBtn = buttons->addButton(tr("Import"), QDialogButtonBox::AcceptRole);
    m_importBtn->setEnabled(false);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_importBtn, &QPushButton::clicked, this, &CsvImportDialog::onImport);

    auto* root = new QVBoxLayout(this);
    root->addLayout(form);
    root->addWidget(buttons);

    connect(m_browseBtn,  &QPushButton::clicked,       this, &CsvImportDialog::onBrowse);
    connect(m_pathEdit,   &QLineEdit::textChanged,     this, &CsvImportDialog::onPathEdited);
    connect(m_kindCombo,  qOverload<int>(&QComboBox::currentIndexChanged),
                                                       this, &CsvImportDialog::onKindChanged);

    // Restore last-used path so the next import starts where the operator left off.
    QSettings const settings;
    QString const last = settings.value(QLatin1String(kSettingsKey)).toString();
    if (!last.isEmpty())
        m_pathEdit->setText(last);  // textChanged triggers parseAndPreview()
}

void CsvImportDialog::onBrowse()
{
    QSettings settings;
    QString const start = !m_pathEdit->text().isEmpty()
        ? QFileInfo(m_pathEdit->text()).absolutePath()
        : settings.value(QLatin1String(kSettingsKey)).toString();
    QString const picked = QFileDialog::getOpenFileName(
        this, tr("Select spawn CSV"), start,
        tr("CSV files (*.csv);;All files (*)"));
    if (picked.isEmpty()) return;
    m_pathEdit->setText(picked);
}

void CsvImportDialog::onPathEdited(QString const& /*text*/)
{
    parseAndPreview();
}

void CsvImportDialog::onKindChanged(int)
{
    // Kind feeds straight into the produced render::Spawn rows; re-parse so the preview
    // counters stay accurate after switching Creature <-> GameObject.
    parseAndPreview();
}

void CsvImportDialog::parseAndPreview()
{
    m_parsedRows.clear();
    m_lastParsedPath.clear();
    m_importBtn->setEnabled(false);

    QString const path = m_pathEdit->text().trimmed();
    if (path.isEmpty())
    {
        m_preview->clear();
        m_statusLabel->setText(tr("No file selected."));
        return;
    }

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        m_preview->clear();
        m_statusLabel->setText(tr("Cannot open file: %1").arg(file.errorString()));
        return;
    }

    QTextStream in(&file);
    in.setEncoding(QStringConverter::Utf8);

    // Build preview text by buffering the first N significant lines as we go.
    QStringList previewLines;
    previewLines.reserve(kPreviewLines);

    // Stage 1: locate header.  Skip comments and empty lines.
    std::vector<int> headerMap;  // headerMap[col] -> index into kColumns, or -1 if unknown
    bool headerFound = false;
    int  lineNumber  = 0;
    int  errorCount  = 0;
    int  rowCount    = 0;
    auto const kind = static_cast<render::SpawnKind>(
        m_kindCombo->currentData().toInt());

    while (!in.atEnd())
    {
        QString const raw = in.readLine();
        ++lineNumber;

        if (previewLines.size() < kPreviewLines)
            previewLines << raw;

        QString const trimmed = raw.trimmed();
        if (trimmed.isEmpty() || trimmed.startsWith(QLatin1Char('#')))
            continue;

        if (!headerFound)
        {
            QStringList const cols = splitRow(trimmed);
            headerMap.reserve(cols.size());
            for (QString const& c : cols)
                headerMap.push_back(resolveHeader(c));
            headerFound = true;
            continue;
        }

        // Data row.  Tolerate short rows: missing trailing columns keep defaults.
        QStringList const cells = splitRow(raw);
        render::Spawn s;
        s.kind = kind;
        s.guid = 0;  // MainWindow assigns the real reserved guid post-parse.
        // GameObject spawns default to a sensible orientation quaternion (1,0,0,0 -> 0,0,0,1
        // is already set by struct defaults).  No extra work needed here.

        bool rowOk = true;
        int const n = std::min<int>(static_cast<int>(headerMap.size()), cells.size());
        for (int col = 0; col < n; ++col)
        {
            int const setterIdx = headerMap[col];
            if (setterIdx < 0) continue;  // unknown header column -> skip silently
            if (!kColumns[setterIdx].apply(s, cells[col]))
            {
                rowOk = false;
                break;
            }
        }

        if (!rowOk)
        {
            ++errorCount;
            continue;
        }

        // mapId validation.  Schema-wise 0 is Eastern Kingdoms (valid); negatives are not
        // representable in uint32_t so the existing parser already rejects them.  We accept
        // any uint32_t.  The explicit comment captures the spec's "mapId must be > 0 or 0".
        // No row is rejected solely on mapId.

        m_parsedRows.push_back(s);
        ++rowCount;
    }
    file.close();

    m_preview->setPlainText(previewLines.join(QLatin1Char('\n')));

    if (!headerFound)
    {
        m_statusLabel->setText(tr("No header line found (file empty or all comments)."));
        return;
    }

    m_statusLabel->setText(tr("%1 rows parsed, %2 errors").arg(rowCount).arg(errorCount));
    m_lastParsedPath = path;
    m_importBtn->setEnabled(rowCount > 0);
}

void CsvImportDialog::onImport()
{
    if (m_parsedRows.empty()) return;

    // Persist path so the next session starts where this one ended.
    QSettings settings;
    settings.setValue(QLatin1String(kSettingsKey), m_lastParsedPath);

    accept();
}

} // namespace world_editor::app
