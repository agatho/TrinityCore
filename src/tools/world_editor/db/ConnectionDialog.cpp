#include "ConnectionDialog.h"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QSpinBox>
#include <QVBoxLayout>

namespace world_editor::db
{

namespace
{
QString profileKey(QString const& profileName)
{
    return QStringLiteral("db/%1").arg(profileName.isEmpty() ? QStringLiteral("default") : profileName);
}
} // namespace

ConnectionDialog::ConnectionDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Database connection"));
    setModal(true);
    resize(420, 320);

    m_profileCombo = new QComboBox(this);
    m_profileCombo->setEditable(true);
    m_profileCombo->addItem(QStringLiteral("world"));
    m_profileCombo->addItem(QStringLiteral("characters"));
    m_profileCombo->addItem(QStringLiteral("hotfixes"));

    m_hostEdit      = new QLineEdit(this);
    m_portSpin      = new QSpinBox(this);
    m_portSpin->setRange(1, 65535);
    m_portSpin->setValue(3306);
    m_userEdit      = new QLineEdit(this);
    m_passEdit      = new QLineEdit(this);
    m_passEdit->setEchoMode(QLineEdit::Password);
    m_databaseEdit  = new QLineEdit(this);
    m_sharedDbEdit  = new QLineEdit(this);
    m_sharedDbEdit->setPlaceholderText(
        tr("shared playerbot schema — must match server Playerbot.SharedDatabase (e.g. wowc_playerbot)"));
    m_timeoutSpin   = new QSpinBox(this);
    m_timeoutSpin->setRange(1, 120);
    m_timeoutSpin->setSuffix(QStringLiteral(" s"));
    m_timeoutSpin->setValue(5);
    m_compressionCheck = new QCheckBox(tr("compress wire protocol"), this);

    auto* form = new QFormLayout;
    form->addRow(tr("Profile"),    m_profileCombo);
    form->addRow(tr("Host"),       m_hostEdit);
    form->addRow(tr("Port"),       m_portSpin);
    form->addRow(tr("User"),       m_userEdit);
    form->addRow(tr("Password"),   m_passEdit);
    form->addRow(tr("Database"),   m_databaseEdit);
    form->addRow(tr("Shared playerbot DB"), m_sharedDbEdit);
    form->addRow(tr("Timeout"),    m_timeoutSpin);
    form->addRow(QString{},        m_compressionCheck);

    m_testStatusLabel = new QLabel(QString{}, this);
    m_testStatusLabel->setWordWrap(true);

    auto* buttons = new QDialogButtonBox(this);
    auto* testButton = buttons->addButton(tr("&Test"), QDialogButtonBox::ActionRole);
    buttons->addButton(QDialogButtonBox::Ok);
    buttons->addButton(QDialogButtonBox::Cancel);

    connect(testButton, &QPushButton::clicked, this, &ConnectionDialog::onTestConnection);
    connect(buttons,    &QDialogButtonBox::accepted, this, &ConnectionDialog::onAccept);
    connect(buttons,    &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto* outer = new QVBoxLayout(this);
    outer->addLayout(form);
    outer->addWidget(m_testStatusLabel);
    outer->addWidget(buttons);

    // Re-populate when the operator picks a different profile.
    connect(m_profileCombo, &QComboBox::currentTextChanged, this, [this](QString const& name) {
        m_profileName = name;
        setInitialParams(loadProfile(name));
    });

    // Seed defaults.
    m_profileName = QStringLiteral("world");
    setInitialParams(loadProfile(m_profileName));
}

void ConnectionDialog::setInitialParams(ConnectionParams const& params)
{
    m_hostEdit->setText(QString::fromStdString(params.host));
    m_portSpin->setValue(params.port);
    m_userEdit->setText(QString::fromStdString(params.user));
    m_passEdit->setText(QString::fromStdString(params.password));
    m_databaseEdit->setText(QString::fromStdString(params.database));
    m_sharedDbEdit->setText(QString::fromStdString(params.sharedDatabase));
    m_timeoutSpin->setValue(static_cast<int>(params.timeoutSecs));
    m_compressionCheck->setChecked(params.useCompression);
    m_testStatusLabel->clear();
}

ConnectionParams ConnectionDialog::params() const
{
    ConnectionParams p;
    p.host           = m_hostEdit->text().toStdString();
    p.port           = static_cast<uint16_t>(m_portSpin->value());
    p.user           = m_userEdit->text().toStdString();
    p.password       = m_passEdit->text().toStdString();
    p.database       = m_databaseEdit->text().toStdString();
    p.sharedDatabase = m_sharedDbEdit->text().toStdString();
    p.timeoutSecs    = static_cast<uint32_t>(m_timeoutSpin->value());
    p.useCompression = m_compressionCheck->isChecked();
    return p;
}

QString ConnectionDialog::profileName() const
{
    return m_profileName;
}

void ConnectionDialog::setProfileName(QString const& name)
{
    m_profileName = name;
    int const idx = m_profileCombo->findText(name);
    if (idx >= 0)
        m_profileCombo->setCurrentIndex(idx);
    else
        m_profileCombo->setCurrentText(name);
}

ConnectionParams ConnectionDialog::loadProfile(QString const& profileName)
{
    QSettings settings;
    settings.beginGroup(profileKey(profileName));
    ConnectionParams p;
    p.host        = settings.value(QStringLiteral("host"),        QStringLiteral("127.0.0.1")).toString().toStdString();
    p.port        = static_cast<uint16_t>(settings.value(QStringLiteral("port"), 3306).toUInt());
    p.user        = settings.value(QStringLiteral("user"),        QStringLiteral("root")).toString().toStdString();
    p.password    = settings.value(QStringLiteral("password"),    QString{}).toString().toStdString();
    p.database    = settings.value(QStringLiteral("database"),    profileName).toString().toStdString();
    p.sharedDatabase = settings.value(QStringLiteral("shared_database"), QString{}).toString().toStdString();
    p.timeoutSecs = settings.value(QStringLiteral("timeout"),     5).toUInt();
    p.useCompression = settings.value(QStringLiteral("compression"), false).toBool();
    settings.endGroup();
    return p;
}

void ConnectionDialog::saveProfile(QString const& profileName, ConnectionParams const& params)
{
    QSettings settings;
    settings.beginGroup(profileKey(profileName));
    settings.setValue(QStringLiteral("host"),        QString::fromStdString(params.host));
    settings.setValue(QStringLiteral("port"),        params.port);
    settings.setValue(QStringLiteral("user"),        QString::fromStdString(params.user));
    settings.setValue(QStringLiteral("password"),    QString::fromStdString(params.password));
    settings.setValue(QStringLiteral("database"),    QString::fromStdString(params.database));
    settings.setValue(QStringLiteral("shared_database"), QString::fromStdString(params.sharedDatabase));
    settings.setValue(QStringLiteral("timeout"),     params.timeoutSecs);
    settings.setValue(QStringLiteral("compression"), params.useCompression);
    settings.endGroup();
}

void ConnectionDialog::onTestConnection()
{
    m_testStatusLabel->setText(tr("connecting..."));
    QApplication::processEvents();

    MySqlClient client;
    QueryError const err = client.connect(params());
    if (err.ok())
    {
        QueryResult versionRes;
        QueryError const verErr = client.query(QStringLiteral("SELECT VERSION()").toStdString(), versionRes);
        if (verErr.ok() && versionRes.rowCount() > 0)
        {
            m_testStatusLabel->setText(tr("OK — server version %1")
                .arg(QString::fromStdString(versionRes.cell(0, 0))));
        }
        else
        {
            m_testStatusLabel->setText(tr("Connected, but SELECT VERSION() failed: %1")
                .arg(QString::fromStdString(verErr.message)));
        }
    }
    else
    {
        m_testStatusLabel->setText(tr("Failed: [%1] %2")
            .arg(err.code).arg(QString::fromStdString(err.message)));
    }
}

void ConnectionDialog::onAccept()
{
    m_profileName = m_profileCombo->currentText();
    saveProfile(m_profileName, params());
    accept();
}

} // namespace world_editor::db
