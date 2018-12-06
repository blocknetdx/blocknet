// Copyright (c) 2018 The Blocknet Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BLOCKNETTOOLS_H
#define BLOCKNETTOOLS_H

#include "blocknettabbar.h"

#include "walletmodel.h"
#include "clientmodel.h"

#include <QFrame>
#include <QVBoxLayout>
#include <QSet>
#include <QFocusEvent>

class BlocknetToolsPage : public QFrame {
    Q_OBJECT
public:
    explicit BlocknetToolsPage(int id, QFrame *parent = nullptr);

protected:
    WalletModel *walletModel;
    int pageID{0};
};

class BlocknetDebugConsole;
class BlocknetPeersList;
class BlocknetBIP38Tool;
class BlocknetWalletRepair;

class BlocknetTools : public QFrame {
    Q_OBJECT
public:
    explicit BlocknetTools(QFrame *parent = nullptr);
    void setModels(WalletModel *w, ClientModel *c);

signals:
    /** Get restart command-line parameters and handle restart */
    void handleRestart(QStringList args);

protected:
    void focusInEvent(QFocusEvent *evt);

private slots:
    void tabChanged(int tab);

private:
    WalletModel *walletModel;
    ClientModel *clientModel;
    QVector<BlocknetToolsPage*> pages;

    QVBoxLayout *layout;
    QLabel *titleLbl;
    BlocknetDebugConsole *debugConsole;
    BlocknetPeersList *networkMonitor;
    BlocknetPeersList *peersList;
    BlocknetBIP38Tool *bip38Tool;
    BlocknetWalletRepair *walletRepair;
    BlocknetPeersList *multisend;
    BlocknetTabBar *tabBar;
    BlocknetToolsPage *screen = nullptr;
};

#endif // BLOCKNETTOOLS_H
