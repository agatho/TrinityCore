/*
 * ConnectionDialog - modal MySQL connection sheet for the world editor.
 *
 * The editor needs the operator to point at three databases: world (TC
 * canonical, target of all spawn/path edits), characters (playerbot
 * V2 world metadata), and optionally hotfixes (read-only).  This
 * dialog covers ONE connection at a time; the MainWindow drives it
 * once per DB.  Connection profiles persist to QSettings so the
 * operator doesn't retype passwords every launch (the password field
 * uses QSettings group "db/<profileName>" and is stored cleartext per
 * Qt convention - this is an operator tool, not a production secret
 * store).
 *
 * The dialog returns ConnectionParams ready to be fed to
 * MySqlClient::connect().  A "Test" button performs a live connect
 * before the operator commits.
 */

#pragma once

#include "MySqlClient.h"

#include <QDialog>
#include <QString>

class QLineEdit;
class QSpinBox;
class QCheckBox;
class QLabel;
class QComboBox;

namespace world_editor::db
{

class ConnectionDialog final : public QDialog
{
    Q_OBJECT

public:
    explicit ConnectionDialog(QWidget* parent = nullptr);

    // Pre-populate the dialog fields.
    void setInitialParams(ConnectionParams const& params);

    // Read the operator's choices back out (only meaningful when the
    // dialog returns QDialog::Accepted).
    [[nodiscard]] ConnectionParams params() const;

    // Profile name (used as the QSettings sub-key for persistence).
    [[nodiscard]] QString profileName() const;
    void setProfileName(QString const& name);

    // Convenience: load/store the named profile from QSettings.
    static ConnectionParams loadProfile(QString const& profileName);
    static void             saveProfile(QString const& profileName, ConnectionParams const& params);

private slots:
    void onTestConnection();
    void onAccept();

private:
    QString    m_profileName;

    QComboBox* m_profileCombo  = nullptr;
    QLineEdit* m_hostEdit      = nullptr;
    QSpinBox*  m_portSpin      = nullptr;
    QLineEdit* m_userEdit      = nullptr;
    QLineEdit* m_passEdit      = nullptr;
    QLineEdit* m_databaseEdit  = nullptr;
    QLineEdit* m_sharedDbEdit  = nullptr;   // shared playerbot schema (roads/metadata)
    QSpinBox*  m_timeoutSpin   = nullptr;
    QCheckBox* m_compressionCheck = nullptr;
    QLabel*    m_testStatusLabel  = nullptr;
};

} // namespace world_editor::db
