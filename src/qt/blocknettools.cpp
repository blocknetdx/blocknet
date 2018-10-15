// Copyright (c) 2018 The Blocknet Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "blocknettools.h"
#include "blocknetpeerslist.h"

#include <QTimer>
#include <QEvent>
#include <QMessageBox>
#include <QList>
#include <QDebug>

enum BToolsTabs {
    DEBUG_CONSOLE = 1,
    NETWORK_MONITOR,
    PEERS_LIST,
    BIP38_TOOL,
    WALLET_REPAIR,
    MULTISEND,
};

BlocknetToolsPage::BlocknetToolsPage(int id, QFrame *parent) : QFrame(parent), pageID(id) { }

void BlocknetToolsPage::onNext() {
    emit next(pageID);
}

void BlocknetToolsPage::onBack() {
    emit back(pageID);
}

BlocknetTools::BlocknetTools(QFrame *parent) : QFrame(parent), layout(new QVBoxLayout) {
//    this->setStyleSheet("border: 1px solid red");
    this->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    layout->setContentsMargins(46, 10, 50, 0);
    this->setLayout(layout);

    titleLbl = new QLabel(tr("Tools"));
    titleLbl->setObjectName("h4");
    titleLbl->setFixedHeight(26);

    debugConsole = new BlocknetPeersList(this, DEBUG_CONSOLE);
    networkMonitor = new BlocknetPeersList(this, NETWORK_MONITOR);
    peersList = new BlocknetPeersList(this, PEERS_LIST);
    bip38Tool = new BlocknetPeersList(this, BIP38_TOOL);
    walletRepair = new BlocknetPeersList(this, WALLET_REPAIR);
    multisend = new BlocknetPeersList(this, MULTISEND);
    pages = { debugConsole, networkMonitor, peersList, bip38Tool, walletRepair, multisend };

    tabBar = new BlocknetTabBar;
    tabBar->setParent(this);
    tabBar->addTab(tr("Debug Console"), DEBUG_CONSOLE);
    tabBar->addTab(tr("Network Monitor"), NETWORK_MONITOR);
    tabBar->addTab(tr("Peers List"), PEERS_LIST);
    tabBar->addTab(tr("BIP38 Tool"), BIP38_TOOL);
    tabBar->addTab(tr("Wallet Repair"), WALLET_REPAIR);
    tabBar->addTab(tr("Multisend"), MULTISEND);
    tabBar->show();

    connect(tabBar, SIGNAL(crumbChanged(int)), this, SLOT(crumbChanged(int)));
    connect(debugConsole, SIGNAL(next(int)), this, SLOT(nextCrumb(int)));
    connect(networkMonitor, SIGNAL(next(int)), this, SLOT(nextCrumb(int)));
    connect(peersList, SIGNAL(next(int)), this, SLOT(nextCrumb(int)));
    connect(networkMonitor, SIGNAL(back(int)), this, SLOT(prevCrumb(int)));
    connect(peersList, SIGNAL(back(int)), this, SLOT(prevCrumb(int)));
    connect(bip38Tool, SIGNAL(back(int)), this, SLOT(prevCrumb(int)));

    layout->addWidget(titleLbl);
    layout->addWidget(tabBar);
    layout->addWidget(debugConsole);
    screen = debugConsole;
}

void BlocknetTools::setWalletModel(WalletModel *w) {
    if (walletModel == w)
        return;
    walletModel = w;
    debugConsole->setWalletModel(walletModel);
    networkMonitor->setWalletModel(walletModel);
    peersList->setWalletModel(walletModel);
    bip38Tool->setWalletModel(walletModel);
    walletRepair->setWalletModel(walletModel);
    multisend->setWalletModel(walletModel);
}

void BlocknetTools::tabChanged(int tab) {
    if (screen && tab > tabBar->getTab() && tabBar->showTab(tabBar->getTab()))
        return;
    tabBar->showTab(tab);

    if (screen) {
        layout->removeWidget(screen);
        screen->hide();
    }

    switch(tab) {
        case DEBUG_CONSOLE:
            screen = debugConsole;
            break;
        case NETWORK_MONITOR:
            screen = networkMonitor;
            break;
        case PEERS_LIST:
            screen = peersList;
            break;
        case BIP38_TOOL:
            screen = bip38Tool;
            break;
        case WALLET_REPAIR:
            screen = walletRepair;
            break;
        case MULTISEND:
            screen = multisend;
            break;
        default:
            return;
    }

    layout->addWidget(screen);

    //positionTab();
    screen->show();
    screen->setFocus();
}

void BlocknetTools::nextTab(int tab) {
    if (screen && tab > tabBar->getTab() && tabBar->showTab(tabBar->getTab()))
        return;
    if (tab >= MULTISEND) // do nothing if at the end
        return;
    tabBar->goToTab(++tab);
}

void BlocknetTools::prevTab(int tab) {
    if (!screen)
        return;
    if (tab <= DEBUG_CONSOLE) // do nothing if at the beginning
        return;
    tabBar->goToTab(--tab);
}

void BlocknetTools::positionTab(QPoint pt) {
    if (pt != QPoint() || pt.x() > 250 || pt.y() > 0) {
        tabBar->move(pt);
        tabBar->raise();
        return;
    }
    auto *pageHeading = screen->findChild<QWidget*>("h4", Qt::FindDirectChildrenOnly);
    QPoint npt = pageHeading->mapToGlobal(QPoint(pageHeading->width(), 0));
    npt = this->mapFromGlobal(npt);
    tabBar->move(npt.x() + 20, npt.y() + pageHeading->height() - tabBar->height() - 2);
    tabBar->raise();
}

