/*
 * TemplatePickerDialog - searchable picker for creature_template /
 * gameobject_template rows.
 *
 * Used by the spawn-placement workflow (HANDOFF 8.3): the operator
 * picks an entry, the dialog returns (kind, entry, name); MainWindow
 * enters spawn-placement mode in the viewer; left-clicks drop new
 * INSERT-pending rows referencing that template.
 *
 * Per HANDOFF 10.7 (validation), the picker is the gatekeeper - only
 * entries that actually exist in *_template can be picked, so the
 * editor cannot create rows with broken FK references.
 *
 * The template tables can be large (TC ships 70k+ creature_template
 * rows). We load entry+name only (no flags/stats) into a model on
 * dialog open and filter client-side. Total payload is a few MiB
 * which Qt handles fine.
 */

#pragma once

#include "../db/MySqlClient.h"
#include "../render/NavMeshView.h"

#include <QDialog>
#include <QString>
#include <QVector>

class QLineEdit;
class QTabWidget;
class QListView;
class QLabel;
class QPushButton;
class QStandardItemModel;
class QSortFilterProxyModel;

namespace world_editor::app
{

struct PickedTemplate
{
    render::SpawnKind kind  = render::SpawnKind::Creature;
    uint32_t          entry = 0;
    QString           name;
};

class TemplatePickerDialog final : public QDialog
{
    Q_OBJECT

public:
    TemplatePickerDialog(db::MySqlClient* dbClient,
                         QString const& worldDbName,
                         QWidget* parent = nullptr);

    [[nodiscard]] PickedTemplate picked() const noexcept { return m_picked; }

private slots:
    void onTabChanged(int idx);
    void onFilterTextChanged(QString const& text);
    void onDoubleClicked(QModelIndex const& idx);
    void onAccept();

private:
    void loadTemplates();
    void populateModel(render::SpawnKind kind);
    [[nodiscard]] PickedTemplate currentSelection() const;

    db::MySqlClient* m_dbClient = nullptr;
    QString          m_worldDb;

    QTabWidget*       m_tabs       = nullptr;
    QLineEdit*        m_filterEdit = nullptr;
    QListView*        m_view       = nullptr;
    QLabel*           m_statusLbl  = nullptr;
    QStandardItemModel* m_creatureModel = nullptr;
    QStandardItemModel* m_goModel       = nullptr;
    QSortFilterProxyModel* m_proxy      = nullptr;
    QPushButton*      m_okButton   = nullptr;

    PickedTemplate m_picked;
    bool           m_loaded = false;
};

} // namespace world_editor::app
