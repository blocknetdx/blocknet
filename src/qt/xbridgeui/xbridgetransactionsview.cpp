//******************************************************************************
//******************************************************************************

#include "xbridgetransactionsview.h"
// #include "../xbridgeapp.h"
// #include "xbridgetransactiondialog.h"
#include "xbridge/xbridgeexchange.h"
#include "xbridge/xuiconnector.h"
#include "xbridge/util/logger.h"

#include <QTableView>
#include <QHeaderView>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QLabel>
#include <QTextEdit>
#include <QFile>

//******************************************************************************
//******************************************************************************
XBridgeTransactionsView::XBridgeTransactionsView(QWidget *parent)
    : QWidget(parent)
    // , m_walletModel(0)
    , m_dlg(m_txModel, this)
{
    setupUi();
}

//******************************************************************************
//******************************************************************************
XBridgeTransactionsView::~XBridgeTransactionsView()
{
    xuiConnector.NotifyLogMessage.disconnect
            (boost::bind(&XBridgeTransactionsView::onLogString, this, _1));
}

//******************************************************************************
//******************************************************************************
void XBridgeTransactionsView::setupUi()
{
    QVBoxLayout * vbox = new QVBoxLayout;

    QLabel * l = new QLabel(tr("Blocknet Decentralized Exchange"), this);
    vbox->addWidget(l);

    m_transactionsProxy.setSourceModel(&m_txModel);
    m_transactionsProxy.setDynamicSortFilter(true);

    QList<XBridgeTransactionDescr::State> transactionsAccetpedStates;
    transactionsAccetpedStates << XBridgeTransactionDescr::trNew
                               << XBridgeTransactionDescr::trPending
                               << XBridgeTransactionDescr::trAccepting
                               << XBridgeTransactionDescr::trHold
                               << XBridgeTransactionDescr::trCreated
                               << XBridgeTransactionDescr::trSigned
                               << XBridgeTransactionDescr::trCommited;

    m_transactionsProxy.setAcceptedStates(transactionsAccetpedStates);

    m_transactionsList = new QTableView(this);
    m_transactionsList->setModel(&m_transactionsProxy);
    m_transactionsList->setContextMenuPolicy(Qt::CustomContextMenu);
    m_transactionsList->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_transactionsList->setSortingEnabled(true);

    connect(m_transactionsList, SIGNAL(customContextMenuRequested(QPoint)),
            this,               SLOT(onContextMenu(QPoint)));


    QHeaderView * header = m_transactionsList->horizontalHeader();
#if QT_VERSION <0x050000
    header->setResizeMode(XBridgeTransactionsModel::Total, QHeaderView::Stretch);
#else
    header->setSectionResizeMode(XBridgeTransactionsModel::Total, QHeaderView::Stretch);
#endif
#if QT_VERSION <0x050000
    header->setResizeMode(XBridgeTransactionsModel::Size, QHeaderView::Stretch);
#else
    header->setSectionResizeMode(XBridgeTransactionsModel::Size, QHeaderView::Stretch);
#endif
#if QT_VERSION <0x050000
    header->setResizeMode(XBridgeTransactionsModel::BID, QHeaderView::Stretch);
#else
    header->setSectionResizeMode(XBridgeTransactionsModel::BID, QHeaderView::Stretch);
#endif

    header->resizeSection(XBridgeTransactionsModel::Total,      80);
    header->resizeSection(XBridgeTransactionsModel::Size,       80);
    header->resizeSection(XBridgeTransactionsModel::BID,        80);
    header->resizeSection(XBridgeTransactionsModel::State,      128);
    vbox->addWidget(m_transactionsList);

    QHBoxLayout * hbox = new QHBoxLayout;

    XBridgeExchange & e = XBridgeExchange::instance();
    if (!e.isEnabled())
    {
        QPushButton * addTxBtn = new QPushButton(trUtf8("New Transaction"), this);
        // addTxBtn->setIcon(QIcon("qrc://"))
        connect(addTxBtn, SIGNAL(clicked()), this, SLOT(onNewTransaction()));
        hbox->addWidget(addTxBtn);
    }
    else
    {
        QPushButton * addTxBtn = new QPushButton(trUtf8("Exchange node"), this);
        addTxBtn->setEnabled(false);
        hbox->addWidget(addTxBtn);
    }

    hbox->addStretch();

    QPushButton * showHideButton = new QPushButton("Toggle to log", this);
    connect(showHideButton, SIGNAL(clicked()), this, SLOT(onToggleHistoricLogs()));
    hbox->addWidget(showHideButton);

    vbox->addLayout(hbox);

    m_historicTransactionsProxy.setSourceModel(&m_txModel);
    m_historicTransactionsProxy.setDynamicSortFilter(true);

    QList<XBridgeTransactionDescr::State> historicTransactionsAccetpedStates;
    historicTransactionsAccetpedStates << XBridgeTransactionDescr::trExpired
                                       << XBridgeTransactionDescr::trOffline
                                       << XBridgeTransactionDescr::trFinished
                                       << XBridgeTransactionDescr::trDropped
                                       << XBridgeTransactionDescr::trCancelled
                                       << XBridgeTransactionDescr::trInvalid;

    m_historicTransactionsProxy.setAcceptedStates(historicTransactionsAccetpedStates);

    m_historicTransactionsList = new QTableView(this);
    m_historicTransactionsList->setModel(&m_historicTransactionsProxy);
    m_historicTransactionsList->setContextMenuPolicy(Qt::CustomContextMenu);
    m_historicTransactionsList->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_historicTransactionsList->setSortingEnabled(true);

    QHeaderView * historicHeader = m_historicTransactionsList->horizontalHeader();
#if QT_VERSION <0x050000
    header->setResizeMode(XBridgeTransactionsModel::Total, QHeaderView::Stretch);
#else
    historicHeader->setSectionResizeMode(XBridgeTransactionsModel::Total, QHeaderView::Stretch);
#endif
#if QT_VERSION <0x050000
    header->setResizeMode(XBridgeTransactionsModel::Size, QHeaderView::Stretch);
#else
    historicHeader->setSectionResizeMode(XBridgeTransactionsModel::Size, QHeaderView::Stretch);
#endif
#if QT_VERSION <0x050000
    header->setResizeMode(XBridgeTransactionsModel::BID, QHeaderView::Stretch);
#else
    historicHeader->setSectionResizeMode(XBridgeTransactionsModel::BID, QHeaderView::Stretch);
#endif

    historicHeader->resizeSection(XBridgeTransactionsModel::Total,      80);
    historicHeader->resizeSection(XBridgeTransactionsModel::Size,       80);
    historicHeader->resizeSection(XBridgeTransactionsModel::BID,        80);
    historicHeader->resizeSection(XBridgeTransactionsModel::State,      128);
    vbox->addWidget(m_historicTransactionsList);

    m_logStrings = new QTextEdit(this);
    m_logStrings->setMinimumHeight(64);
    m_logStrings->setReadOnly(true);
    m_logStrings->setVisible(false);
    vbox->addWidget(m_logStrings);

    setLayout(vbox);

    xuiConnector.NotifyLogMessage.connect
            (boost::bind(&XBridgeTransactionsView::onLogString, this, _1));
}

//******************************************************************************
//******************************************************************************
QMenu * XBridgeTransactionsView::setupContextMenu(QModelIndex & index)
{
    QMenu * contextMenu = new QMenu();

    if (!m_txModel.isMyTransaction(index.row()))
    {
        QAction * acceptTransaction = new QAction(tr("&Accept transaction"), this);
        contextMenu->addAction(acceptTransaction);

        connect(acceptTransaction,   SIGNAL(triggered()),
                this,                SLOT(onAcceptTransaction()));
    }
    else
    {
        QAction * cancelTransaction = new QAction(tr("&Cancel transaction"), this);
        contextMenu->addAction(cancelTransaction);

        connect(cancelTransaction,   SIGNAL(triggered()),
                this,                SLOT(onCancelTransaction()));
    }

    if (false)
    {
        QAction * rollbackTransaction = new QAction(tr("&Rollback transaction"), this);
        contextMenu->addAction(rollbackTransaction);

        connect(rollbackTransaction, SIGNAL(triggered()),
                this,                SLOT(onRollbackTransaction()));
    }

    return contextMenu;
}

//******************************************************************************
//******************************************************************************
void XBridgeTransactionsView::onNewTransaction()
{
    m_dlg.setPendingId(uint256(), std::vector<unsigned char>());
    m_dlg.show();
}

//******************************************************************************
//******************************************************************************
void XBridgeTransactionsView::onAcceptTransaction()
{
    if (!m_contextMenuIndex.isValid())
    {
        return;
    }

    if (m_txModel.isMyTransaction(m_contextMenuIndex.row()))
    {
        return;
    }

    XBridgeTransactionDescr d = m_txModel.item(m_contextMenuIndex.row());
    if (d.state != XBridgeTransactionDescr::trPending)
    {
        return;
    }

    m_dlg.setPendingId(d.id, d.hubAddress);
    m_dlg.setFromAmount((double)d.toAmount / XBridgeTransactionDescr::COIN);
    m_dlg.setToAmount((double)d.fromAmount / XBridgeTransactionDescr::COIN);
    m_dlg.setFromCurrency(QString::fromStdString(d.toCurrency));
    m_dlg.setToCurrency(QString::fromStdString(d.fromCurrency));
    m_dlg.show();
}

//******************************************************************************
//******************************************************************************
void XBridgeTransactionsView::onCancelTransaction()
{
    if (!m_contextMenuIndex.isValid())
    {
        return;
    }

    if (QMessageBox::warning(this,
                             trUtf8("Cancel transaction"),
                             trUtf8("Are you sure?"),
                             QMessageBox::Yes | QMessageBox::Cancel,
                             QMessageBox::Cancel) != QMessageBox::Yes)
    {
        return;
    }

    if (!m_txModel.cancelTransaction(m_txModel.item(m_contextMenuIndex.row()).id))
    {
        QMessageBox::warning(this,
                             trUtf8("Cancel transaction"),
                             trUtf8("Error send cancel request"));
    }
}

//******************************************************************************
//******************************************************************************
void XBridgeTransactionsView::onRollbackTransaction()
{
    if (!m_contextMenuIndex.isValid())
    {
        return;
    }

    if (QMessageBox::warning(this,
                             trUtf8("Rollback transaction"),
                             trUtf8("Are you sure?"),
                             QMessageBox::Yes | QMessageBox::Cancel,
                             QMessageBox::Cancel) != QMessageBox::Yes)
    {
        return;
    }

    if (!m_txModel.cancelTransaction(m_txModel.item(m_contextMenuIndex.row()).id))
    {
        QMessageBox::warning(this,
                             trUtf8("Cancel transaction"),
                             trUtf8("Error send cancel request"));
    }
}

//******************************************************************************
//******************************************************************************
void XBridgeTransactionsView::onContextMenu(QPoint /*pt*/)
{
    XBridgeExchange & e = XBridgeExchange::instance();
    if (e.isEnabled())
    {
        return;
    }

    m_contextMenuIndex = m_transactionsList->selectionModel()->currentIndex();
    if (!m_contextMenuIndex.isValid())
    {
        return;
    }

    m_contextMenuIndex = m_transactionsProxy.mapToSource(m_contextMenuIndex);
    if (!m_contextMenuIndex.isValid())
    {
        return;
    }

    QMenu * contextMenu = setupContextMenu(m_contextMenuIndex);

    contextMenu->exec(QCursor::pos());
    contextMenu->deleteLater();
}

//******************************************************************************
//******************************************************************************
void XBridgeTransactionsView::onToggleHistoricLogs()
{
    QPushButton * btn = qobject_cast<QPushButton *>(sender());

    bool logsVisible = m_logStrings->isVisible();
    bool historicTrVisible = m_historicTransactionsList->isVisible();

    if(logsVisible)
        btn->setText("Toggle to log");
    else if(historicTrVisible)
        btn->setText("Toggle to history");

    m_logStrings->setVisible(!logsVisible);
    m_historicTransactionsList->setVisible(!historicTrVisible);

    if (!logsVisible)
    {
        m_logStrings->clear();

        // show, load all logs
        QFile f(QString::fromStdString(LOG::logFileName()));
        if (f.open(QIODevice::ReadOnly))
        {
            m_logStrings->insertPlainText(f.readAll());
        }
    }
}

//******************************************************************************
//******************************************************************************
void XBridgeTransactionsView::onLogString(const std::string str)
{
    const QString qstr = QString::fromStdString(str);
    m_logStrings->insertPlainText(qstr);

//    QTextCursor c = m_logStrings->textCursor();
//    c.movePosition(QTextCursor::End);
//    m_logStrings->setTextCursor(c);

//    m_logStrings->ensureCursorVisible();
}
