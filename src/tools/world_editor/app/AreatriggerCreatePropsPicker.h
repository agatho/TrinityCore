/*
 * AreatriggerCreatePropsPicker - searchable picker for rows in
 * `areatrigger_create_properties`.  Each row in that table defines a
 * shape + visual + script combo that one or more `areatrigger` spawns
 * point at via (AreaTriggerCreatePropertiesId, IsCustom).
 *
 * Used by the new-areatrigger-placement workflow (Phase 7b INSERT).
 * The picker is the gatekeeper - placement is only enabled once the
 * operator has chosen an existing create-properties row, so the editor
 * cannot create rows with broken FK references (HANDOFF 10.7).
 */

#pragma once

#include "../db/MySqlClient.h"

#include <QDialog>
#include <QString>

#include <cstdint>

class QLineEdit;
class QTableView;
class QLabel;
class QPushButton;
class QStandardItemModel;
class QSortFilterProxyModel;

namespace world_editor::app
{

struct PickedAreatriggerProps
{
    bool     valid       = false;
    uint32_t id          = 0;
    uint8_t  isCustom    = 0;
    uint8_t  shape       = 0;
    float    shapeData[8] = {0,0,0,0,0,0,0,0};
    QString  scriptName;
};

class AreatriggerCreatePropsPicker final : public QDialog
{
    Q_OBJECT

public:
    explicit AreatriggerCreatePropsPicker(db::MySqlClient* dbClient,
                                          QWidget* parent = nullptr);

    [[nodiscard]] PickedAreatriggerProps picked() const noexcept { return m_picked; }

private slots:
    void onFilterTextChanged(QString const& text);
    void onAccept();

private:
    void loadRows();
    [[nodiscard]] PickedAreatriggerProps currentSelection() const;

    db::MySqlClient*       m_dbClient = nullptr;
    QLineEdit*             m_filterEdit = nullptr;
    QTableView*            m_view       = nullptr;
    QLabel*                m_statusLbl  = nullptr;
    QStandardItemModel*    m_model      = nullptr;
    QSortFilterProxyModel* m_proxy      = nullptr;
    QPushButton*           m_okButton   = nullptr;

    PickedAreatriggerProps m_picked;
};

} // namespace world_editor::app
