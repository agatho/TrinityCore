#include "ConfirmSqlDialog.h"

#include <QApplication>
#include <QDialogButtonBox>
#include <QFontDatabase>
#include <QLabel>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>

namespace world_editor::app
{

ConfirmSqlDialog::ConfirmSqlDialog(db::MySqlClient* dbClient,
                                   QString const& summary,
                                   QString const& sql,
                                   QWidget* parent)
    : QDialog(parent), m_dbClient(dbClient), m_sql(sql)
{
    setWindowTitle(tr("Confirm SQL"));
    setModal(true);
    resize(600, 320);

    auto* outer = new QVBoxLayout(this);
    outer->addWidget(new QLabel(summary, this));

    m_sqlView = new QPlainTextEdit(this);
    m_sqlView->setReadOnly(true);
    m_sqlView->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    m_sqlView->setPlainText(sql);
    outer->addWidget(m_sqlView, 1);

    m_statusLabel = new QLabel(QString{}, this);
    outer->addWidget(m_statusLabel);

    auto* buttons = new QDialogButtonBox(this);
    m_applyBtn = buttons->addButton(tr("Apply"), QDialogButtonBox::AcceptRole);
    m_applyBtn->setDefault(true);
    buttons->addButton(QDialogButtonBox::Cancel);
    connect(m_applyBtn, &QPushButton::clicked, this, &ConfirmSqlDialog::onApply);
    connect(buttons,    &QDialogButtonBox::rejected, this, &QDialog::reject);
    outer->addWidget(buttons);
}

void ConfirmSqlDialog::onApply()
{
    if (!m_dbClient || !m_dbClient->isConnected())
    {
        m_statusLabel->setText(tr("Not connected to DB."));
        return;
    }
    m_applyBtn->setEnabled(false);
    QApplication::processEvents();

    auto err = m_dbClient->exec("START TRANSACTION");
    if (!err.ok())
    {
        QMessageBox::critical(this, tr("BEGIN failed"),
            QString::fromStdString(err.message));
        m_applyBtn->setEnabled(true);
        return;
    }
    err = m_dbClient->exec(m_sql.toStdString(), &m_affected);
    if (!err.ok())
    {
        (void)m_dbClient->exec("ROLLBACK");
        QMessageBox::critical(this, tr("SQL failed"),
            tr("[%1] %2").arg(err.code).arg(QString::fromStdString(err.message)));
        m_applyBtn->setEnabled(true);
        return;
    }
    err = m_dbClient->exec("COMMIT");
    if (!err.ok())
    {
        (void)m_dbClient->exec("ROLLBACK");
        QMessageBox::critical(this, tr("COMMIT failed"),
            QString::fromStdString(err.message));
        m_applyBtn->setEnabled(true);
        return;
    }
    m_applied = true;
    accept();
}

} // namespace world_editor::app
