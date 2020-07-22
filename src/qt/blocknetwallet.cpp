// Copyright (c) 2018-2020 The Blocknet developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/blocknetwallet.h>

#include <qt/blocknetaccounts.h>
#include <qt/blocknetaddressbook.h>
#include <qt/blocknetfontmgr.h>
#include <qt/blocknetquicksend.h>
#include <qt/blocknetservicenodes.h>
#include <qt/blocknetsettings.h>

#include <qt/askpassphrasedialog.h>
#include <qt/optionsmodel.h>
#include <qt/platformstyle.h>
#include <qt/receivecoinsdialog.h>
#include <qt/signverifymessagedialog.h>
#include <qt/transactionfilterproxy.h>
#include <qt/transactiontablemodel.h>
#include <qt/transactionview.h>

#include <util/system.h>

#include <QSettings>

BlocknetWallet::BlocknetWallet(interfaces::Node & node, const PlatformStyle *platformStyle, QFrame *parent)
                              : QFrame(parent), node(node), platformStyle(platformStyle)
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

    layout->addWidget(leftMenu, 0);
    layout->addWidget(contentBox, 1);

    connect(leftMenu, &BlocknetLeftMenu::menuChanged, this, &BlocknetWallet::setPage);
    connect(toolbar, &BlocknetToolBar::passphrase, this, &BlocknetWallet::changePassphrase);
    connect(toolbar, &BlocknetToolBar::lock, this, &BlocknetWallet::onLockRequest);
    connect(toolbar, &BlocknetToolBar::progressClicked, this, [this]{
        Q_EMIT progressClicked();
    });
}

bool BlocknetWallet::setCurrentWallet(const QString & name) {
    for (WalletModel *w : wallets.values()) {
        disconnect(w, &WalletModel::balanceChanged, this, &BlocknetWallet::balanceChanged);
        disconnect(w->getTransactionTableModel(), &TransactionTableModel::rowsInserted, this, &BlocknetWallet::processNewTransaction);
        disconnect(w->getOptionsModel(), &OptionsModel::displayUnitChanged, this, &BlocknetWallet::displayUnitChanged);
        disconnect(w, &WalletModel::requireUnlock, this, &BlocknetWallet::unlockWallet);
        disconnect(w, &WalletModel::showProgress, this, &BlocknetWallet::showProgress);
        disconnect(w, &WalletModel::encryptionStatusChanged, this, &BlocknetWallet::onEncryptionStatus);
    }

    walletModel = wallets[name];
    if (!walletModel)
        return false;

    connect(walletModel, &WalletModel::balanceChanged, this, &BlocknetWallet::balanceChanged);
    connect(walletModel->getTransactionTableModel(), &TransactionTableModel::rowsInserted, this, &BlocknetWallet::processNewTransaction);
    connect(walletModel->getOptionsModel(), &OptionsModel::displayUnitChanged, this, &BlocknetWallet::displayUnitChanged);
    connect(walletModel, &WalletModel::requireUnlock, this, &BlocknetWallet::unlockWallet);
    connect(walletModel, &WalletModel::showProgress, this, &BlocknetWallet::showProgress);
    connect(walletModel, &WalletModel::encryptionStatusChanged, this, &BlocknetWallet::onEncryptionStatus);

    // Send funds screen
    if (sendFunds == nullptr) {
        sendFunds = new BlocknetSendFunds(walletModel);
        connect(sendFunds, &BlocknetSendFunds::dashboard, this, &BlocknetWallet::goToDashboard);
    } else
        sendFunds->setWalletModel(walletModel);

    // Dashboard screen
    if (dashboard == nullptr) {
        dashboard = new BlocknetDashboard;
        dashboard->setWalletModel(walletModel);
        connect(dashboard, &BlocknetDashboard::quicksend, this, &BlocknetWallet::goToQuickSend);
        connect(dashboard, &BlocknetDashboard::history, this, &BlocknetWallet::goToHistory);
    } else
        dashboard->setWalletModel(walletModel);

    // wallet lock state
    setLock(walletModel->getEncryptionStatus() == WalletModel::Locked, util::unlockedForStakingOnly);

    // Update balances
    balanceChanged(walletModel->wallet().getBalances());

    // Staking
    updateStakingStatus(walletModel);

    return true;
}

void BlocknetWallet::setProgress(int progress, const QString &msg, int maximum) {
    toolbar->setProgress(progress, msg, maximum);
}

void BlocknetWallet::updateStakingStatus(WalletModel *w) {
    const auto staking = gArgs.GetBoolArg("-staking", true);
    auto msg = tr("Staking is off");
    const auto canStake = staking && w->wallet().getBalance() > 0
            && (util::unlockedForStakingOnly || w->getEncryptionStatus() == WalletModel::EncryptionStatus::Unlocked
                                             || w->getEncryptionStatus() == WalletModel::EncryptionStatus::Unencrypted);
    if (canStake)
        msg = tr("Staking is active");
    else if (staking && w->wallet().getBalance() <= 0)
        msg = tr("Staking is off, your staking balance is 0");
    else if (staking)
        msg = tr("Staking is pending, please unlock the wallet to stake funds");
    toolbar->setStaking(canStake , msg);
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
            connect(addressBook, &BlocknetAddressBook::send, this, &BlocknetWallet::onSendToAddress);
            connect(addressBook, &BlocknetAddressBook::rescan, this, &BlocknetWallet::onRescanRequest);
            screen = addressBook;
            break;
        }
        case BlocknetPage::ACCOUNTS: {
            auto *accounts = new BlocknetAccounts;
            accounts->setWalletModel(walletModel);
            connect(accounts, &BlocknetAccounts::rescan, this, &BlocknetWallet::onRescanRequest);
            screen = accounts;
            break;
        }
        case BlocknetPage::SEND: {
            sendFunds->show();
            screen = sendFunds;
            break;
        }
        case BlocknetPage::QUICKSEND: {
            auto *quickSend = new BlocknetQuickSend(walletModel);
            connect(quickSend, &BlocknetQuickSend::submit, this, &BlocknetWallet::onSendFunds);
            connect(quickSend, &BlocknetQuickSend::dashboard, this, &BlocknetWallet::goToDashboard);
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
        case BlocknetPage::SNODES: {
            auto *snode = new BlocknetServiceNodes;
            snode->setClientModel(clientModel);
            screen = snode;
            break;
        }
        case BlocknetPage::PROPOSALS: {
            auto *proposals = new BlocknetProposals;
            connect(proposals, &BlocknetProposals::createProposal, this, &BlocknetWallet::goToCreateProposal);
            proposals->setModels(clientModel, walletModel);
            screen = proposals;
            break;
        }
        case BlocknetPage::CREATEPROPOSAL: {
            if (createProposal == nullptr) {
                createProposal = new BlocknetCreateProposal;
                connect(createProposal, &BlocknetCreateProposal::done, this, &BlocknetWallet::goToProposals);
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

void BlocknetWallet::onRescanRequest(const std::string & walletName) {
    tg.create_thread([walletName]() {
        RenameThread("blocknet-rescan");
        auto ws = GetWallets();
        CWallet *pwallet = nullptr;
        for (auto & w : ws) {
            if (walletName == w->GetName()) {
                pwallet = w.get();
                break;
            }
        }
        if (!pwallet)
            return;
        WalletRescanReserver reserver(pwallet);
        if (!reserver.reserve())
            return;
        auto locked_chain = pwallet->chain().lock();
        LOCK(pwallet->cs_wallet);
        pwallet->RescanFromTime(0, reserver, true);
    });
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
    leftMenu->setBalance(balances.balance, walletModel->getOptionsModel() ? walletModel->getOptionsModel()->getDisplayUnit() : 0);
    if (dashboard)
        dashboard->balanceChanged(balances);
}

void BlocknetWallet::displayUnitChanged(const int unit) {
    balanceChanged(walletModel->wallet().getBalances());
}

void BlocknetWallet::changePassphrase() {
    if (!walletModel)
        return;
    AskPassphraseDialog dlg(AskPassphraseDialog::ChangePass, this);
    dlg.setObjectName("redesign");
    dlg.setModel(walletModel);
    dlg.exec();
}

void BlocknetWallet::encryptWallet(bool status) {
    if (!walletModel)
        return;
    AskPassphraseDialog dlg(status ? AskPassphraseDialog::Encrypt : AskPassphraseDialog::Decrypt, this);
    dlg.setObjectName("redesign");
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
        util::unlockedForStakingOnly = false;
        walletModel->setWalletLocked(locked);
    } else {
        // Unlock wallet when requested by wallet model
        AskPassphraseDialog dlg(AskPassphraseDialog::Unlock, this);
        dlg.setObjectName("redesign");
        dlg.setModel(walletModel);
        if (stakingOnly)
            dlg.stakingOnly();
        dlg.exec();
    }
    Q_EMIT encryptionStatusChanged(walletModel->getEncryptionStatus());
}

void BlocknetWallet::onEncryptionStatus() {
    setLock(walletModel->getEncryptionStatus() == WalletModel::EncryptionStatus::Locked, util::unlockedForStakingOnly);
    updateStakingStatus(walletModel);
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
    if (walletModel->getEncryptionStatus() == WalletModel::Locked || util::unlockedForStakingOnly) {
        AskPassphraseDialog dlg(AskPassphraseDialog::Unlock, this);
        dlg.setObjectName("redesign");
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

BlocknetWallet::~BlocknetWallet() {
    tg.interrupt_all();
    tg.join_all();
}