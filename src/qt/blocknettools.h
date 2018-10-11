// Copyright (c) 2018 The Blocknet Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BLOCKNETTOOLS_H
#define BLOCKNETTOOLS_H

#include "blocknettabbar.h"

#include "walletmodel.h"

#include <QFrame>
#include <QVBoxLayout>
#include <QSet>

class BlocknetToolsPage : public QFrame {
    Q_OBJECT
public:
    explicit BlocknetToolsPage(int id, QFrame *parent = nullptr);

signals:
    void next(int pageID);
    void back(int pageID);

public slots:
    void onNext();
    void onBack();

protected:
    WalletModel *walletModel;
    int pageID{0};
};

class BlocknetPeersList;

class BlocknetTools : public QFrame {
    Q_OBJECT
public:
    explicit BlocknetTools(QFrame *parent = nullptr);
    void setWalletModel(WalletModel *w);

signals:

public slots:

protected:

private slots:
    void tabChanged(int crumb);
    void nextTab(int crumb);
    void prevTab(int crumb);

private:
    WalletModel *walletModel;
    QVector<BlocknetToolsPage*> pages;

    QVBoxLayout *layout;
    QLabel *titleLbl;
    BlocknetPeersList *debugConsole;
    BlocknetPeersList *networkMonitor;
    BlocknetPeersList *peersList;
    BlocknetPeersList *bip38Tool;
    BlocknetPeersList *walletRepair;
    BlocknetPeersList *multisend;
    BlocknetTabBar *tabBar;
    BlocknetToolsPage *screen = nullptr;

    void positionTab(QPoint pt = QPoint());
};

#endif // BLOCKNETTOOLS_H
