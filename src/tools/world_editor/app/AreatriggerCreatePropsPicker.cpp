#include "AreatriggerCreatePropsPicker.h"

#include <QApplication>
#include <QDialogButtonBox>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSortFilterProxyModel>
#include <QStandardItemModel>
#include <QTableView>
#include <QTimer>
#include <QVBoxLayout>

#include <cstdio>

namespace world_editor::app
{

namespace
{
constexpr int COL_ID         = 0;
constexpr int COL_IS_CUSTOM  = 1;
constexpr int COL_SHAPE      = 2;
constexpr int COL_DATA0      = 3;
constexpr int COL_DATA1      = 4;
constexpr int COL_SCRIPT     = 5;
constexpr int COL_COUNT      = 6;

QString shapeLabel(uint8_t shape)
{
    switch (shape)
    {
    case 0: return QStringLiteral("Sphere");
    case 1: return QStringLiteral("Box");
    case 2: return QStringLiteral("Polygon");
    case 3: return QStringLiteral("Cylinder");
    case 4: return QStringLiteral("Disk");
    default: return QStringLiteral("?(%1)").arg(int(shape));
    }
}
} // namespace

AreatriggerCreatePropsPicker::AreatriggerCreatePropsPicker(db::MySqlClient* dbClient,
                                                           QWidget* parent)
    : QDialog(parent), m_dbClient(dbClient)
{
    setWindowTitle(tr("Pick areatrigger create-properties"));
    setModal(true);
    resize(720, 600);

    m_filterEdit = new QLineEdit(this);
    m_filterEdit->setPlaceholderText(tr("Filter by Id, Shape (Sphere/Box/...), or script..."));
    m_view = new QTableView(this);
    m_view->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_view->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_view->setSelectionMode(QAbstractItemView::SingleSelection);
    m_view->setSortingEnabled(true);
    m_statusLbl = new QLabel(tr("loading..."), this);

    m_model = new QStandardItemModel(this);
    m_model->setHorizontalHeaderLabels(
        { tr("Id"), tr("IsCustom"), tr("Shape"), tr("Data0"), tr("Data1"), tr("Script") });

    m_proxy = new QSortFilterProxyModel(this);
    m_proxy->setFilterCaseSensitivity(Qt::CaseInsensitive);
    m_proxy->setFilterKeyColumn(-1);
    m_proxy->setSourceModel(m_model);
    m_view->setModel(m_proxy);
    m_view->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);

    auto* buttons = new QDialogButtonBox(this);
    m_okButton = buttons->addButton(tr("Use selected"), QDialogButtonBox::AcceptRole);
    m_okButton->setEnabled(false);
    buttons->addButton(QDialogButtonBox::Cancel);

    connect(m_filterEdit, &QLineEdit::textChanged, this, &AreatriggerCreatePropsPicker::onFilterTextChanged);
    connect(m_view, &QTableView::doubleClicked, this,
            [this](QModelIndex const&) { onAccept(); });
    connect(m_view->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, [this](QItemSelection const&, QItemSelection const&) {
                m_okButton->setEnabled(m_view->selectionModel()->hasSelection());
            });
    connect(buttons, &QDialogButtonBox::accepted, this, &AreatriggerCreatePropsPicker::onAccept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto* outer = new QVBoxLayout(this);
    outer->addWidget(m_filterEdit);
    outer->addWidget(m_view, 1);
    outer->addWidget(m_statusLbl);
    outer->addWidget(buttons);

    QTimer::singleShot(0, this, &AreatriggerCreatePropsPicker::loadRows);
}

void AreatriggerCreatePropsPicker::loadRows()
{
    if (!m_dbClient || !m_dbClient->isConnected())
    {
        m_statusLbl->setText(tr("not connected to DB"));
        return;
    }
    char const* sql =
        "SELECT Id, IsCustom, Shape, ShapeData0, ShapeData1, ShapeData2, "
        "       ShapeData3, ShapeData4, ShapeData5, ShapeData6, ShapeData7, "
        "       ScriptName "
        "FROM areatrigger_create_properties "
        "ORDER BY Id, IsCustom";
    db::QueryResult res;
    auto const err = m_dbClient->query(sql, res);
    if (!err.ok())
    {
        m_statusLbl->setText(tr("query failed: %1").arg(QString::fromStdString(err.message)));
        return;
    }

    QApplication::setOverrideCursor(Qt::WaitCursor);
    m_model->setRowCount(int(res.rowCount()));
    for (size_t r = 0; r < res.rowCount(); ++r)
    {
        uint32_t const id        = uint32_t(res.asUInt64(r, 0).value_or(0));
        uint8_t  const isCustom  = uint8_t(res.asUInt64(r, 1).value_or(0));
        uint8_t  const shape     = uint8_t(res.asUInt64(r, 2).value_or(0));
        double   const data0     = res.asDouble(r, 3).value_or(0.0);
        double   const data1     = res.asDouble(r, 4).value_or(0.0);
        QString  const script    = QString::fromStdString(res.cell(r, 11));

        auto* idItem     = new QStandardItem(QString::number(id));
        idItem->setData(qulonglong(id),       Qt::UserRole + 0);
        idItem->setData(qulonglong(isCustom), Qt::UserRole + 1);
        idItem->setData(qulonglong(shape),    Qt::UserRole + 2);
        for (int k = 0; k < 8; ++k)
            idItem->setData(double(res.asDouble(r, 3 + k).value_or(0.0)),
                            Qt::UserRole + 3 + k);
        idItem->setData(script, Qt::UserRole + 11);

        m_model->setItem(int(r), COL_ID,        idItem);
        m_model->setItem(int(r), COL_IS_CUSTOM, new QStandardItem(QString::number(isCustom)));
        m_model->setItem(int(r), COL_SHAPE,
                         new QStandardItem(shapeLabel(shape)));
        m_model->setItem(int(r), COL_DATA0,
                         new QStandardItem(QString::number(data0, 'f', 3)));
        m_model->setItem(int(r), COL_DATA1,
                         new QStandardItem(QString::number(data1, 'f', 3)));
        m_model->setItem(int(r), COL_SCRIPT, new QStandardItem(script));
    }
    QApplication::restoreOverrideCursor();
    m_statusLbl->setText(tr("loaded %1 create-properties rows").arg(res.rowCount()));
    Q_UNUSED(COL_COUNT);
}

void AreatriggerCreatePropsPicker::onFilterTextChanged(QString const& text)
{
    m_proxy->setFilterFixedString(text);
}

PickedAreatriggerProps AreatriggerCreatePropsPicker::currentSelection() const
{
    PickedAreatriggerProps out;
    QModelIndex const proxyIdx = m_view->currentIndex();
    if (!proxyIdx.isValid()) return out;
    QModelIndex const srcIdx = m_proxy->mapToSource(proxyIdx);
    QStandardItem* idItem = m_model->item(srcIdx.row(), COL_ID);
    if (!idItem) return out;
    out.valid      = true;
    out.id         = uint32_t(idItem->data(Qt::UserRole + 0).toULongLong());
    out.isCustom   = uint8_t(idItem->data(Qt::UserRole + 1).toULongLong());
    out.shape      = uint8_t(idItem->data(Qt::UserRole + 2).toULongLong());
    for (int k = 0; k < 8; ++k)
        out.shapeData[k] = float(idItem->data(Qt::UserRole + 3 + k).toDouble());
    out.scriptName = idItem->data(Qt::UserRole + 11).toString();
    return out;
}

void AreatriggerCreatePropsPicker::onAccept()
{
    PickedAreatriggerProps sel = currentSelection();
    if (!sel.valid) return;
    m_picked = sel;
    accept();
}

} // namespace world_editor::app
