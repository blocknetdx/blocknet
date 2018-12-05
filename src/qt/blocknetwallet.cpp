// Copyright (c) 2018 The Blocknet Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "blocknetwallet.h"
#include "blocknetfontmgr.h"
#include "blocknetquicksend.h"
#include "blocknetaddressbook.h"
#include "blocknetsettings.h"
#include "blocknettools.h"

#include "optionsmodel.h"
#include "transactiontablemodel.h"
#include "transactionfilterproxy.h"
#include "transactionview.h"
#include "receivecoinsdialog.h"
#include "servicenodelist.h"

#include <QDebug>
#include <QSettings>

BlocknetWallet::BlocknetWallet(QFrame *parent) : QFrame(parent) {
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
    leftMenu->setFixedWidth(228);

    toolbar = new BlocknetToolBar(this);
    contentBoxLayout->addWidget(toolbar, 0, Qt::AlignTop);

    layout->addWidget(leftMenu, 0, Qt::AlignLeft);
    layout->addWidget(contentBox);

    connect(leftMenu, SIGNAL(menuChanged(BlocknetPage)), this, SLOT(setPage(BlocknetPage)));
    connect(toolbar, SIGNAL(passphrase()), this, SLOT(onChangePassphrase()));
    connect(toolbar, SIGNAL(lock(bool, bool)), this, SLOT(onLockRequest(bool, bool)));
}

bool BlocknetWallet::setCurrentWallet(const QString &name) {
    for (WalletModel *w : wallets.values()) {
        disconnect(w, SIGNAL(balanceChanged(CAmount, CAmount, CAmount, CAmount, CAmount, CAmount, CAmount)),
                   this, SLOT(balanceChanged(CAmount, CAmount, CAmount, CAmount, CAmount, CAmount, CAmount)));
        disconnect(w->getTransactionTableModel(), SIGNAL(rowsInserted(QModelIndex, int, int)),
                   this, SLOT(processTransaction(QModelIndex, int, int)));
        disconnect(w->getOptionsModel(), SIGNAL(displayUnitChanged(int)), this, SLOT(displayUnitChanged(int)));
    }

    walletModel = wallets[name];
    if (!walletModel)
        return false;

    connect(walletModel, SIGNAL(balanceChanged(CAmount, CAmount, CAmount, CAmount, CAmount, CAmount, CAmount)),
            this, SLOT(balanceChanged(CAmount, CAmount, CAmount, CAmount, CAmount, CAmount, CAmount)));
    connect(walletModel->getOptionsModel(), SIGNAL(displayUnitChanged(int)), this, SLOT(displayUnitChanged(int)));

    // Send funds screen
    if (sendFunds == nullptr) {
        sendFunds = new BlocknetSendFunds(walletModel);
        connect(sendFunds, SIGNAL(dashboard()), this, SLOT(goToDashboard()));
    }
    else sendFunds->setWalletModel(walletModel);

    // Dashboard screen
    if (dashboard == nullptr) {
        dashboard = new BlocknetDashboard;
        dashboard->setWalletModel(walletModel);
        connect(dashboard, SIGNAL(quicksend()), this, SLOT(goToQuickSend()));
        connect(dashboard, SIGNAL(history()), this, SLOT(goToHistory()));
        connect(this, SIGNAL(balance(CAmount, CAmount, CAmount, CAmount, CAmount, CAmount, CAmount)),
                dashboard, SLOT(balanceChanged(CAmount, CAmount, CAmount, CAmount, CAmount, CAmount, CAmount)));
    }
    else dashboard->setWalletModel(walletModel);

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
        if (screen != sendFunds && screen != dashboard && screen != createProposal)
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
            auto *recieve = new ReceiveCoinsDialog;
            recieve->setStyleSheet(GUIUtil::loadStyleSheetv1());
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
            QSettings settings;
            if (settings.value("fShowServicenodesTab").toBool()) {
                auto *snode = new ServicenodeList;
                snode->setStyleSheet(GUIUtil::loadStyleSheetv1());
                snode->setClientModel(clientModel);
                snode->setWalletModel(walletModel);
                screen = snode;
            }
            break;
        }
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
            createProposal->show();
            screen = createProposal;
            break;
        }
        case BlocknetPage::SETTINGS: {
            auto *settings = new BlocknetSettings;
            settings->setWalletModel(walletModel);
            screen = settings;
            break;
        }
//        case BlocknetPage::ANNOUNCEMENTS:
        case BlocknetPage::TOOLS: {
            auto *tools = new BlocknetTools;
            tools->setWalletModel(walletModel);
            connect(tools, &BlocknetTools::handleRestart, this, [this](QStringList args) { emit handleRestart(args); });
            screen = tools;
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

void BlocknetWallet::balanceChanged(const CAmount walletBalance, const CAmount unconfirmed, const CAmount immature,
                                    const CAmount anonymized, const CAmount watch, const CAmount watchUnconfirmed,
                                    const CAmount watchImmature) {
    emit balance(walletBalance, unconfirmed, immature, anonymized, watch, watchUnconfirmed, watchImmature);
    leftMenu->setBalance(walletBalance + unconfirmed, walletModel->getOptionsModel() ? walletModel->getOptionsModel()->getDisplayUnit() : 0);
}

void BlocknetWallet::displayUnitChanged(const int unit) {
    balanceChanged(walletModel->getBalance(), walletModel->getUnconfirmedBalance(), walletModel->getImmatureBalance(), walletModel->getAnonymizedBalance(),
                   walletModel->getWatchBalance(), walletModel->getWatchUnconfirmedBalance(), walletModel->getWatchImmatureBalance());
}
