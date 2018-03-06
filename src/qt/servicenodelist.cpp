#include "servicenodelist.h"
#include "ui_servicenodelist.h"

#include "activeservicenode.h"
#include "clientmodel.h"
#include "guiutil.h"
#include "init.h"
#include "servicenode-sync.h"
#include "servicenodeconfig.h"
#include "servicenodeman.h"
#include "sync.h"
#include "wallet.h"
#include "walletmodel.h"
#include "xbridge/xbridgeexchange.h"

#include <QMessageBox>
#include <QTimer>

CCriticalSection cs_servicenodes;

ServicenodeList::ServicenodeList(QWidget* parent) : QWidget(parent),
                                                  ui(new Ui::ServicenodeList),
                                                  clientModel(0),
                                                  walletModel(0)
{
    ui->setupUi(this);

    ui->startButton->setEnabled(false);

    int columnAliasWidth = 100;
    int columnAddressWidth = 200;
    int columnProtocolWidth = 60;
    int columnStatusWidth = 80;
    int columnActiveWidth = 130;
    int columnLastSeenWidth = 130;

    ui->tableWidgetMyServicenodes->setColumnWidth(0, columnAliasWidth);
    ui->tableWidgetMyServicenodes->setColumnWidth(1, columnAddressWidth);
    ui->tableWidgetMyServicenodes->setColumnWidth(2, columnProtocolWidth);
    ui->tableWidgetMyServicenodes->setColumnWidth(3, columnStatusWidth);
    ui->tableWidgetMyServicenodes->setColumnWidth(4, columnActiveWidth);
    ui->tableWidgetMyServicenodes->setColumnWidth(5, columnLastSeenWidth);

    ui->tableWidgetMyServicenodes->setContextMenuPolicy(Qt::CustomContextMenu);

    //hided start alias buttons until fix/testing
    ui->startAllButton->setVisible(false);
    ui->startButton->setVisible(false);
    ui->startMissingButton->setVisible(false);

    QAction* startAliasAction = new QAction(tr("Start alias"), this);
    contextMenu = new QMenu();
    contextMenu->addAction(startAliasAction);

    //hided hided start alias context menu until fix/testing
//    connect(ui->tableWidgetMyServicenodes, SIGNAL(customContextMenuRequested(const QPoint&)), this, SLOT(showContextMenu(const QPoint&)));
//    connect(startAliasAction, SIGNAL(triggered()), this, SLOT(on_startButton_clicked()));

    timer = new QTimer(this);
    connect(timer, SIGNAL(timeout()), this, SLOT(updateMyNodeList()));
    timer->start(1000);

    // Fill MN list
    fFilterUpdated = true;
    nTimeFilterUpdated = GetTime();
}

ServicenodeList::~ServicenodeList()
{
    delete ui;
}

void ServicenodeList::setClientModel(ClientModel* model)
{
    this->clientModel = model;
}

void ServicenodeList::setWalletModel(WalletModel* model)
{
    this->walletModel = model;
}

void ServicenodeList::showContextMenu(const QPoint& point)
{
    QTableWidgetItem* item = ui->tableWidgetMyServicenodes->itemAt(point);
    if (item) contextMenu->exec(QCursor::pos());
}

void ServicenodeList::StartAlias(std::string strAlias)
{
    std::string strStatusHtml;
    strStatusHtml += "<center>Alias: " + strAlias;

    BOOST_FOREACH (CServicenodeConfig::CServicenodeEntry mne, servicenodeConfig.getEntries()) {
        if (mne.getAlias() == strAlias) {
            std::string strError;
            CServicenodeBroadcast mnb;

            xbridge::Exchange & e = xbridge::Exchange::instance();

            bool fSuccess = CServicenodeBroadcast::Create(mne.getIp(), mne.getPrivKey(),
                                                          mne.getTxHash(), mne.getOutputIndex(),
                                                          e.isEnabled() ? e.connectedWallets() : std::vector<std::string>(),
                                                          strError, mnb);

            if (fSuccess) {
                strStatusHtml += "<br>Successfully started servicenode.";
                mnodeman.UpdateServicenodeList(mnb);
                mnb.Relay();
            } else {
                strStatusHtml += "<br>Failed to start servicenode.<br>Error: " + strError;
            }
            break;
        }
    }
    strStatusHtml += "</center>";

    QMessageBox msg;
    msg.setText(QString::fromStdString(strStatusHtml));
    msg.exec();

    updateMyNodeList(true);
}

void ServicenodeList::StartAll(std::string strCommand)
{
    int nCountSuccessful = 0;
    int nCountFailed = 0;
    std::string strFailedHtml;

    BOOST_FOREACH (CServicenodeConfig::CServicenodeEntry mne, servicenodeConfig.getEntries()) {
        std::string strError;
        CServicenodeBroadcast mnb;

        int nIndex;
        if(!mne.castOutputIndex(nIndex))
            continue;

        CTxIn txin = CTxIn(uint256S(mne.getTxHash()), uint32_t(nIndex));
        CServicenode* pmn = mnodeman.Find(txin);

        if (strCommand == "start-missing" && pmn)
        {
            continue;
        }

        xbridge::Exchange & e = xbridge::Exchange::instance();

        bool fSuccess = CServicenodeBroadcast::Create(mne.getIp(), mne.getPrivKey(),
                                                      mne.getTxHash(), mne.getOutputIndex(),
                                                      e.isEnabled() ? e.connectedWallets() : std::vector<std::string>(),
                                                      strError, mnb);

        if (fSuccess) {
            nCountSuccessful++;
            mnodeman.UpdateServicenodeList(mnb);
            mnb.Relay();
        } else {
            nCountFailed++;
            strFailedHtml += "\nFailed to start " + mne.getAlias() + ". Error: " + strError;
        }
    }
    pwalletMain->Lock();

    std::string returnObj;
    returnObj = strprintf("Successfully started %d servicenodes, failed to start %d, total %d", nCountSuccessful, nCountFailed, nCountFailed + nCountSuccessful);
    if (nCountFailed > 0) {
        returnObj += strFailedHtml;
    }

    QMessageBox msg;
    msg.setText(QString::fromStdString(returnObj));
    msg.exec();

    updateMyNodeList(true);
}

void ServicenodeList::updateMyServicenodeInfo(QString strAlias, QString strAddr, CServicenode* pmn)
{
    LOCK(cs_mnlistupdate);
    bool fOldRowFound = false;
    int nNewRow = 0;

    for (int i = 0; i < ui->tableWidgetMyServicenodes->rowCount(); i++) {
        if (ui->tableWidgetMyServicenodes->item(i, 0)->text() == strAlias) {
            fOldRowFound = true;
            nNewRow = i;
            break;
        }
    }

    if (nNewRow == 0 && !fOldRowFound) {
        nNewRow = ui->tableWidgetMyServicenodes->rowCount();
        ui->tableWidgetMyServicenodes->insertRow(nNewRow);
    }

    QTableWidgetItem* aliasItem = new QTableWidgetItem(strAlias);
    QTableWidgetItem* addrItem = new QTableWidgetItem(pmn ? QString::fromStdString(pmn->addr.ToString()) : strAddr);
    QTableWidgetItem* protocolItem = new QTableWidgetItem(QString::number(pmn ? pmn->protocolVersion : -1));
    QTableWidgetItem* statusItem = new QTableWidgetItem(QString::fromStdString(pmn ? pmn->GetStatus() : "MISSING"));
    GUIUtil::DHMSTableWidgetItem* activeSecondsItem = new GUIUtil::DHMSTableWidgetItem(pmn ? (pmn->lastPing.sigTime - pmn->sigTime) : 0);
    QTableWidgetItem* lastSeenItem = new QTableWidgetItem(QString::fromStdString(DateTimeStrFormat("%Y-%m-%d %H:%M", pmn ? pmn->lastPing.sigTime : 0)));
    QTableWidgetItem* pubkeyItem = new QTableWidgetItem(QString::fromStdString(pmn ? CBitcoinAddress(pmn->pubKeyCollateralAddress.GetID()).ToString() : ""));

    ui->tableWidgetMyServicenodes->setItem(nNewRow, 0, aliasItem);
    ui->tableWidgetMyServicenodes->setItem(nNewRow, 1, addrItem);
    ui->tableWidgetMyServicenodes->setItem(nNewRow, 2, protocolItem);
    ui->tableWidgetMyServicenodes->setItem(nNewRow, 3, statusItem);
    ui->tableWidgetMyServicenodes->setItem(nNewRow, 4, activeSecondsItem);
    ui->tableWidgetMyServicenodes->setItem(nNewRow, 5, lastSeenItem);
    ui->tableWidgetMyServicenodes->setItem(nNewRow, 6, pubkeyItem);
}

void ServicenodeList::updateMyNodeList(bool fForce)
{
    static int64_t nTimeMyListUpdated = 0;

    // automatically update my servicenode list only once in MY_SERVICENODELIST_UPDATE_SECONDS seconds,
    // this update still can be triggered manually at any time via button click
    int64_t nSecondsTillUpdate = nTimeMyListUpdated + MY_SERVICENODELIST_UPDATE_SECONDS - GetTime();
    ui->secondsLabel->setText(QString::number(nSecondsTillUpdate));

    if (nSecondsTillUpdate > 0 && !fForce) return;
    nTimeMyListUpdated = GetTime();

    ui->tableWidgetMyServicenodes->setSortingEnabled(false);
    BOOST_FOREACH (CServicenodeConfig::CServicenodeEntry mne, servicenodeConfig.getEntries()) {
        int nIndex;
        if(!mne.castOutputIndex(nIndex))
            continue;

        CTxIn txin = CTxIn(uint256S(mne.getTxHash()), uint32_t(nIndex));
        CServicenode* pmn = mnodeman.Find(txin);

        updateMyServicenodeInfo(QString::fromStdString(mne.getAlias()), QString::fromStdString(mne.getIp()), pmn);
    }
    ui->tableWidgetMyServicenodes->setSortingEnabled(true);

    // reset "timer"
    ui->secondsLabel->setText("0");
}

void ServicenodeList::on_startButton_clicked()
{
    // Find selected node alias
    QItemSelectionModel* selectionModel = ui->tableWidgetMyServicenodes->selectionModel();
    QModelIndexList selected = selectionModel->selectedRows();

    if (selected.count() == 0) return;

    QModelIndex index = selected.at(0);
    int nSelectedRow = index.row();
    std::string strAlias = ui->tableWidgetMyServicenodes->item(nSelectedRow, 0)->text().toStdString();

    // Display message box
    QMessageBox::StandardButton retval = QMessageBox::question(this, tr("Confirm servicenode start"),
        tr("Are you sure you want to start servicenode %1?").arg(QString::fromStdString(strAlias)),
        QMessageBox::Yes | QMessageBox::Cancel,
        QMessageBox::Cancel);

    if (retval != QMessageBox::Yes) return;

    WalletModel::EncryptionStatus encStatus = walletModel->getEncryptionStatus();

    if (encStatus == walletModel->Locked || encStatus == walletModel->UnlockedForAnonymizationOnly) {
        WalletModel::UnlockContext ctx(walletModel->requestUnlock());

        if (!ctx.isValid()) return; // Unlock wallet was cancelled

        StartAlias(strAlias);
        return;
    }

    StartAlias(strAlias);
}

void ServicenodeList::on_startAllButton_clicked()
{
    // Display message box
    QMessageBox::StandardButton retval = QMessageBox::question(this, tr("Confirm all servicenodes start"),
        tr("Are you sure you want to start ALL servicenodes?"),
        QMessageBox::Yes | QMessageBox::Cancel,
        QMessageBox::Cancel);

    if (retval != QMessageBox::Yes) return;

    WalletModel::EncryptionStatus encStatus = walletModel->getEncryptionStatus();

    if (encStatus == walletModel->Locked || encStatus == walletModel->UnlockedForAnonymizationOnly) {
        WalletModel::UnlockContext ctx(walletModel->requestUnlock());

        if (!ctx.isValid()) return; // Unlock wallet was cancelled

        StartAll();
        return;
    }

    StartAll();
}

void ServicenodeList::on_startMissingButton_clicked()
{
    if (!servicenodeSync.IsServicenodeListSynced()) {
        QMessageBox::critical(this, tr("Command is not available right now"),
            tr("You can't use this command until servicenode list is synced"));
        return;
    }

    // Display message box
    QMessageBox::StandardButton retval = QMessageBox::question(this,
        tr("Confirm missing servicenodes start"),
        tr("Are you sure you want to start MISSING servicenodes?"),
        QMessageBox::Yes | QMessageBox::Cancel,
        QMessageBox::Cancel);

    if (retval != QMessageBox::Yes) return;

    WalletModel::EncryptionStatus encStatus = walletModel->getEncryptionStatus();

    if (encStatus == walletModel->Locked || encStatus == walletModel->UnlockedForAnonymizationOnly) {
        WalletModel::UnlockContext ctx(walletModel->requestUnlock());

        if (!ctx.isValid()) return; // Unlock wallet was cancelled

        StartAll("start-missing");
        return;
    }

    StartAll("start-missing");
}

void ServicenodeList::on_tableWidgetMyServicenodes_itemSelectionChanged()
{
    if (ui->tableWidgetMyServicenodes->selectedItems().count() > 0) {
        ui->startButton->setEnabled(true);
    }
}

void ServicenodeList::on_UpdateButton_clicked()
{
    updateMyNodeList(true);
}
