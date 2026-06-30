/*
 * CsvImportDialog - modal dialog driving the "File -> Import spawns from CSV..." action.
 *
 * Operator picks a CSV file plus the row kind (Creature or GameObject).  The dialog parses
 * the file in tolerant mode (missing columns -> defaults; rows with bad numerics counted as
 * errors but skipped), shows a live preview + parse status, and on Import emits the
 * resulting render::Spawn rows.  MainWindow wraps the whole batch into a single undo frame
 * via UndoManager::recordOn(m_spawnModel.get(), ...) and refreshes the viewer.
 *
 * Expected CSV format (first non-comment / non-empty line is the header):
 *   entry,x,y,z,orientation,mapId,spawntimesecs,phaseId
 *   12345,-1000.5,-2500.3,150.1,0.0,0,120,0
 *
 * Empty lines and lines starting with `#` are skipped.  Header column order is free; columns
 * absent from the header keep render::Spawn defaults.  Last-used file path persists in
 * QSettings under "editor/csv_import_last_path".
 */

#pragma once

#include "../render/NavMeshView.h"

#include <QDialog>
#include <QString>

#include <vector>

class QComboBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;

namespace world_editor::app
{

class CsvImportDialog final : public QDialog
{
    Q_OBJECT

public:
    explicit CsvImportDialog(QWidget* parent = nullptr);

    // Populated by the most recent successful parse.  Empty until parseAndPreview() runs.
    [[nodiscard]] std::vector<render::Spawn> const& parsedRows() const noexcept { return m_parsedRows; }
    [[nodiscard]] QString const&                    sourcePath() const noexcept { return m_lastParsedPath; }

private slots:
    void onBrowse();
    void onPathEdited(QString const& text);
    void onKindChanged(int);
    void onImport();

private:
    // Re-reads m_pathEdit, refreshes preview/status, populates m_parsedRows.  Tolerant: every
    // row that fails numeric parsing increments the error counter; valid rows still land in
    // m_parsedRows so partial imports succeed.
    void parseAndPreview();

    QLineEdit*       m_pathEdit       = nullptr;
    QPushButton*     m_browseBtn      = nullptr;
    QComboBox*       m_kindCombo      = nullptr;
    QPlainTextEdit*  m_preview        = nullptr;
    QLabel*          m_statusLabel    = nullptr;
    QPushButton*     m_importBtn      = nullptr;

    std::vector<render::Spawn> m_parsedRows;
    QString                    m_lastParsedPath;
};

} // namespace world_editor::app
