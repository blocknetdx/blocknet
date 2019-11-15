// Copyright (c) 2018-2019 The Blocknet developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/blocknetwallet.h>

#include <qt/blocknetaddressbook.h>
#include <qt/blocknetfontmgr.h>
#include <qt/blocknetquicksend.h>
#include <qt/blocknetsettings.h>

#include <qt/askpassphrasedialog.h>
#include <qt/optionsmodel.h>
#include <qt/platformstyle.h>
#include <qt/receivecoinsdialog.h>
#include <qt/signverifymessagedialog.h>
#include <qt/transactionfilterproxy.h>
#include <qt/transactiontablemodel.h>
#include <qt/transactionview.h>

#include <QSettings>

BlocknetWallet::BlocknetWallet(interfaces::Node & node, const PlatformStyle *platformStyle, QFrame *parent)
                              : QFrame(parent), node(node),
                                platformStyle(platformStyle)
{
    BlocknetFontMgr::setup();

    this->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    auto *layout = new QHBoxLayout;
    layout->setContentsMargins(QMargins());
    layout->setSpacing(0);
    this->setLayout(layout);

    contentBox = new QFrame;
    contentBox->setObjectName("contentBox");
    auto *contentBoxLayout = new QVBoxLayout;
    contentBoxLayout->setContentsMargins(QMargins());
    contentBoxLayout->setSpacing(0);
    contentBox->setLayout(contentBoxLayout);

    leftMenu = new BlocknetLeftMenu;

    toolbar = new BlocknetToolBar(this);
    contentBoxLayout->addWidget(toolbar, 0, Qt::AlignTop);

    layout->addWidget(leftMenu, 0, Qt::AlignLeft);
    layout->addWidget(contentBox);

    connect(leftMenu, SIGNAL(menuChanged(BlocknetPage)), this, SLOT(setPage(BlocknetPage)));
    connect(toolbar, SIGNAL(passphrase()), this, SLOT(changePassphrase()));
    connect(toolbar, SIGNAL(lock(bool, bool)), this, SLOT(onLockRequest(bool, bool)));
}

bool BlocknetWallet::setCurrentWallet(const QString & name) {
    for (WalletModel *w : wallets.values()) {
        disconnect(w, &WalletModel::balanceChanged, this, &BlocknetWallet::balanceChanged);
        disconnect(w->getTransactionTableModel(), &TransactionTableModel::rowsInserted, this, &BlocknetWallet::processNewTransaction);
        disconnect(w->getOptionsModel(), SIGNAL(displayUnitChanged(int)), this, SLOT(displayUnitChanged(int)));
        disconnect(w, &WalletModel::requireUnlock, this, &BlocknetWallet::unlockWallet);
        disconnect(w, &WalletModel::showProgress, this, &BlocknetWallet::showProgress);
    }

    walletModel = wallets[name];
    if (!walletModel)
        return false;

    connect(walletModel->getTransactionTableModel(), &TransactionTableModel::rowsInserted, this, &BlocknetWallet::processNewTransaction);
    connect(walletModel, &WalletModel::balanceChanged, this, &BlocknetWallet::balanceChanged);
    connect(walletModel->getOptionsModel(), SIGNAL(displayUnitChanged(int)), this, SLOT(displayUnitChanged(int)));
    connect(walletModel, &WalletModel::requireUnlock, this, &BlocknetWallet::unlockWallet);
    connect(walletModel, &WalletModel::showProgress, this, &BlocknetWallet::showProgress);

    // Send funds screen
    if (sendFunds == nullptr) {
        sendFunds = new BlocknetSendFunds(walletModel);
        connect(sendFunds, SIGNAL(dashboard()), this, SLOT(goToDashboard()));
    } else
        sendFunds->setWalletModel(walletModel);

    // Dashboard screen
    if (dashboard == nullptr) {
        dashboard = new BlocknetDashboard;
        dashboard->setWalletModel(walletModel);
        connect(dashboard, SIGNAL(quicksend()), this, SLOT(goToQuickSend()));
        connect(dashboard, SIGNAL(history()), this, SLOT(goToHistory()));
        connect(this, &BlocknetWallet::balanceChanged, dashboard, &BlocknetDashboard::balanceChanged);
    } else
        dashboard->setWalletModel(walletModel);

    // wallet lock state
    setLock(walletModel->getEncryptionStatus() == WalletModel::Locked, false); // TODO Blocknet Qt locked for staking only

    // Update balances
    balanceChanged(walletModel->wallet().getBalances());

    return true;
}

void BlocknetWallet::setProgress(int progress, const QString &msg, int maximum) {
    toolbar->setProgress(progress, msg, maximum);
}

void BlocknetWallet::setStakingStatus(bool on, const QString &msg) {
    toolbar->setStaking(on, msg);
}

void BlocknetWallet::setPeers(const int peers) {
    toolbar->setPeers(peers);
}

void BlocknetWallet::setLock(const bool lock, const bool stakingOnly) {
    toolbar->setLock(lock, stakingOnly);
}

void BlocknetWallet::setPage(BlocknetPage page) {
    bool same = this->page == page;
    leftMenu->selectMenu(page);

    if (same)
        return;
    this->page = page;

    if (screen) {
        screen->hide();
        contentBox->layout()->removeWidget(screen);
        if (screen != sendFunds && screen != dashboard && screen != createProposal && screen != btools)
            screen->deleteLater();
    }

    switch (page) {
        case BlocknetPage::DASHBOARD: {
            dashboard->show();
            screen = dashboard;
            break;
        }
        case BlocknetPage::ADDRESSBOOK: {
            auto *addressBook = new BlocknetAddressBook;
            addressBook->setWalletModel(walletModel);
            connect(addressBook, SIGNAL(send(const QString &)), this, SLOT(onSendToAddress(const QString &)));
            screen = addressBook;
            break;
        }
        case BlocknetPage::SEND: {
            sendFunds->show();
            screen = sendFunds;
            break;
        }
        case BlocknetPage::QUICKSEND: {
            auto *quickSend = new BlocknetQuickSend(walletModel);
            connect(quickSend, SIGNAL(submit()), this, SLOT(onSendFunds()));
            connect(quickSend, SIGNAL(dashboard()), this, SLOT(goToDashboard()));
            screen = quickSend;
            break;
        }
        case BlocknetPage::REQUEST: {
            std::string platformName = gArgs.GetArg("-uiplatform", "other");
            auto platformStyle = PlatformStyle::instantiate(QString::fromStdString(platformName));
            if (!platformStyle) // Fall back to "other" if specified name not found
                platformStyle = PlatformStyle::instantiate("other");
            auto *recieve = new ReceiveCoinsDialog(platformStyle);
            recieve->setModel(walletModel);
            screen = recieve;
            break;
        }
        case BlocknetPage::HISTORY: {
            auto *transactionView = new BlocknetTransactionHistory(walletModel);
            screen = transactionView;
            break;
        }
//        case BlocknetPage::SNODES: { // TODO Blocknet Qt implement snode list
//            QSettings settings;
//            if (settings.value("fShowServicenodesTab").toBool()) {
//                auto *snode = new ServicenodeList;
//                snode->setClientModel(clientModel);
//                snode->setWalletModel(walletModel);
//                screen = snode;
//            }
//            break;
//        }
        case BlocknetPage::PROPOSALS: {
            auto *proposals = new BlocknetProposals;
            connect(proposals, SIGNAL(createProposal()), this, SLOT(goToCreateProposal()));
            proposals->setWalletModel(walletModel);
            screen = proposals;
            break;
        }
        case BlocknetPage::CREATEPROPOSAL: {
            if (createProposal == nullptr) {
                createProposal = new BlocknetCreateProposal;
                connect(createProposal, SIGNAL(done()), this, SLOT(goToProposals()));
            }
            createProposal->setWalletModel(walletModel);
            createProposal->show();
            screen = createProposal;
            break;
        }
        case BlocknetPage::SETTINGS: {
            auto *settings = new BlocknetSettings(node);
            settings->setWalletModel(walletModel);
            screen = settings;
            break;
        }
//        case BlocknetPage::ANNOUNCEMENTS:
        case BlocknetPage::TOOLS: {
            if (btools == nullptr) {
                btools = new BlocknetTools(node, platformStyle);
                connect(btools, &BlocknetTools::handleRestart, this, [this](QStringList args) { Q_EMIT handleRestart(args); });
            }
            btools->setModels(clientModel, walletModel);
            btools->show();
            screen = btools;
            break;
        }
        default:
            screen = new QFrame;
            break;
    }

    contentBox->layout()->addWidget(screen);
    screen->setFocus();
}

void BlocknetWallet::onSendFunds() {
    goToDashboard();
}

void BlocknetWallet::onSendToAddress(const QString &address) {
    setPage(BlocknetPage::SEND);
    if (sendFunds != nullptr)
        sendFunds->addAddress(address);
}

void BlocknetWallet::goToDashboard() {
    setPage(BlocknetPage::DASHBOARD);
}

void BlocknetWallet::goToQuickSend() {
    setPage(BlocknetPage::QUICKSEND);
}

void BlocknetWallet::goToHistory() {
    setPage(BlocknetPage::HISTORY);
}

void BlocknetWallet::goToProposals() {
    setPage(BlocknetPage::PROPOSALS);
}

void BlocknetWallet::goToCreateProposal() {
    setPage(BlocknetPage::CREATEPROPOSAL);
}

void BlocknetWallet::balanceChanged(const interfaces::WalletBalances & balances) {
    Q_EMIT balance(balances);
    leftMenu->setBalance(balances.balance, walletModel->getOptionsModel() ? walletModel->getOptionsModel()->getDisplayUnit() : 0);
}

void BlocknetWallet::displayUnitChanged(const int unit) {
    balanceChanged(walletModel->wallet().getBalances());
}

void BlocknetWallet::changePassphrase() {
    if (!walletModel)
        return;
    AskPassphraseDialog dlg(AskPassphraseDialog::ChangePass, this);
    dlg.setModel(walletModel);
    dlg.exec();
}

void BlocknetWallet::encryptWallet(bool status) {
    if (!walletModel)
        return;
    AskPassphraseDialog dlg(status ? AskPassphraseDialog::Encrypt : AskPassphraseDialog::Decrypt, this);
    dlg.setModel(walletModel);
    dlg.exec();

    Q_EMIT encryptionStatusChanged(walletModel->getEncryptionStatus());
}

void BlocknetWallet::backupWallet() {
    QString filename = GUIUtil::getSaveFileName(this, tr("Backup Wallet"), QString(),
            tr("Wallet Data (*.dat)"), nullptr);

    if (filename.isEmpty())
        return;

    if (!walletModel->wallet().backupWallet(filename.toLocal8Bit().data()))
        QMessageBox::warning(this, tr("Backup Failed"), tr("There was an error trying to save the wallet data to %1.").arg(filename));
    else
        QMessageBox::information(this, tr("Backup Successful"), tr("The wallet data was successfully saved to %1.").arg(filename));
}

void BlocknetWallet::onLockRequest(bool locked, bool stakingOnly) {
    if (locked) {
        walletModel->setWalletLocked(locked);
    } else {
        // Unlock wallet when requested by wallet model
        AskPassphraseDialog dlg(AskPassphraseDialog::Unlock, this);
        dlg.setModel(walletModel);
        dlg.exec();
    }
    Q_EMIT encryptionStatusChanged(walletModel->getEncryptionStatus());
}

void BlocknetWallet::usedSendingAddresses() {
    if (!walletModel)
        return;
    BlocknetAddressBookDialog dlg(walletModel, Qt::WindowSystemMenuHint | Qt::WindowTitleHint, BlocknetAddressBook::FILTER_SENDING);
    dlg.exec();
}

void BlocknetWallet::usedReceivingAddresses() {
    if (!walletModel)
        return;
    BlocknetAddressBookDialog dlg(walletModel, Qt::WindowSystemMenuHint | Qt::WindowTitleHint, BlocknetAddressBook::FILTER_RECEIVING);
    dlg.exec();
}

void BlocknetWallet::unlockWallet() {
    if (!walletModel)
        return;
    // Unlock wallet when requested by wallet model
    if (walletModel->getEncryptionStatus() == WalletModel::Locked) {
        AskPassphraseDialog dlg(AskPassphraseDialog::Unlock, this);
        dlg.setModel(walletModel);
        dlg.exec();
    }
}

void BlocknetWallet::showProgress(const QString &title, int nProgress) {
    if (nProgress == 0) {
        progressDialog = new QProgressDialog(title, tr("Cancel"), 0, 100);
        GUIUtil::PolishProgressDialog(progressDialog);
        progressDialog->setWindowModality(Qt::ApplicationModal);
        progressDialog->setMinimumDuration(0);
        progressDialog->setAutoClose(false);
        progressDialog->setValue(0);
    } else if (nProgress == 100) {
        if (progressDialog) {
            progressDialog->close();
            progressDialog->deleteLater();
        }
    } else if (progressDialog) {
        if (progressDialog->wasCanceled()) {
            walletModel->wallet().abortRescan();
        } else {
            progressDialog->setValue(nProgress);
        }
    }
}

void BlocknetWallet::gotoSignMessageTab(QString addr) {
    // calls show() in showTab_VM()
    auto *signVerifyMessageDialog = new SignVerifyMessageDialog(platformStyle, this);
    signVerifyMessageDialog->setAttribute(Qt::WA_DeleteOnClose);
    signVerifyMessageDialog->setModel(walletModel);
    signVerifyMessageDialog->showTab_SM(true);

    if (!addr.isEmpty())
        signVerifyMessageDialog->setAddress_SM(addr);
}

void BlocknetWallet::gotoVerifyMessageTab(QString addr) {
    // calls show() in showTab_VM()
    auto *signVerifyMessageDialog = new SignVerifyMessageDialog(platformStyle, this);
    signVerifyMessageDialog->setAttribute(Qt::WA_DeleteOnClose);
    signVerifyMessageDialog->setModel(walletModel);
    signVerifyMessageDialog->showTab_VM(true);

    if (!addr.isEmpty())
        signVerifyMessageDialog->setAddress_VM(addr);
}

void BlocknetWallet::processNewTransaction(const QModelIndex& parent, int start, int /*end*/) {
    // Prevent balloon-spam when initial block download is in progress
    if (!walletModel || !clientModel || clientModel->node().isInitialBlockDownload())
        return;

    TransactionTableModel *ttm = walletModel->getTransactionTableModel();
    if (!ttm || ttm->processingQueuedTransactions())
        return;

    QString date = ttm->index(start, TransactionTableModel::Date, parent).data().toString();
    qint64 amount = ttm->index(start, TransactionTableModel::Amount, parent).data(Qt::EditRole).toULongLong();
    QString type = ttm->index(start, TransactionTableModel::Type, parent).data().toString();
    QModelIndex index = ttm->index(start, 0, parent);
    QString address = ttm->data(index, TransactionTableModel::AddressRole).toString();
    QString label = ttm->data(index, TransactionTableModel::LabelRole).toString();

    Q_EMIT incomingTransaction(date, walletModel->getOptionsModel()->getDisplayUnit(), amount, type, address,
            label, walletModel->getWalletName());
}