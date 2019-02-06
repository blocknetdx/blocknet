// Copyright (c) 2018 The Blocknet Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BLOCKNETWALLET_H
#define BLOCKNETWALLET_H

#include "blocknetvars.h"
#include "blocknetleftmenu.h"
#include "blocknetdashboard.h"
#include "blocknetsendfunds.h"
#include "blocknetcreateproposal.h"
#include "blocknetproposals.h"
#include "blocknettransactionhistory.h"
#include "blocknettoolbar.h"
#include "blocknettools.h"

#include "clientmodel.h"
#include "walletmodel.h"

#include <QList>
#include <QHash>

class BlocknetWallet : public QFrame {
    Q_OBJECT

public:
    explicit BlocknetWallet(QFrame *parent = nullptr);
    void setProgress(int progress, const QString &msg, int maximum = 100);
    void setStakingStatus(bool on, const QString &msg);
    void setPeers(int peers);
    void setLock(bool lock, bool stakingOnly = false); // TODO Handle timed unlock
    void setClientModel(ClientModel *clientModel) { this->clientModel = clientModel; }
    bool setCurrentWallet(const QString &name);

    bool addWallet(const QString &name, WalletModel *walletModel) { this->wallets[name] = walletModel; return true; };
    bool removeWallet(const QString &name) { this->wallets.remove(name); return true; };
    void removeAllWallets() { this->wallets.clear(); };
    bool handlePaymentRequest(const SendCoinsRecipient &recipient) { return false; };
    void showOutOfSyncWarning(bool fShow) { };

signals:
    void lock(bool locked, bool stakingOnly);
    void passphrase();
    void balance(CAmount walletBalance, CAmount unconfirmed, CAmount immature, CAmount anonymized, CAmount watch,
                 CAmount watchUnconfirmed, CAmount watchImmature);
    void addressbook();
    void request();
    void settings();
    void tools();
    void handleRestart(QStringList args);

public slots:
    void setPage(BlocknetPage page);
    void gotoOverviewPage() { };
    void gotoHistoryPage() { };
    void gotoXBridgePage() { };
    void gotoServicenodePage() { };
    void gotoReceiveCoinsPage() { };
    void gotoSendCoinsPage(QString addr = "") { };

protected slots:
    void onSendFunds();
    void onSendToAddress(const QString &);
    void goToDashboard();
    void goToQuickSend();
    void goToHistory();
    void goToCreateProposal();
    void goToProposals();
    void onChangePassphrase() { emit passphrase(); };
    void onLockRequest(bool locked, bool stakingOnly) { emit lock(locked, stakingOnly); }
    void balanceChanged(CAmount walletBalance, CAmount unconfirmed, CAmount immature, CAmount anonymized,
                        CAmount watch, CAmount watchUnconfirmed, CAmount watchImmature);
    void displayUnitChanged(int unit);

private:
    QHash<QString, WalletModel*> wallets;
    ClientModel *clientModel;
    WalletModel *walletModel;
    BlocknetPage page;

    BlocknetLeftMenu *leftMenu;
    QFrame *contentBox;
    BlocknetToolBar *toolbar;
    BlocknetSendFunds *sendFunds = nullptr;
    BlocknetDashboard *dashboard = nullptr;
    BlocknetCreateProposal *createProposal = nullptr;
    BlocknetTools *btools = nullptr;
    QWidget *screen = nullptr;
};

#endif // BLOCKNETWALLET_H
