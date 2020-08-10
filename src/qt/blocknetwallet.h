// Copyright (c) 2018-2020 The Blocknet developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BLOCKNET_QT_BLOCKNETWALLET_H
#define BLOCKNET_QT_BLOCKNETWALLET_H

#include <qt/blocknetcreateproposal.h>
#include <qt/blocknetdashboard.h>
#include <qt/blocknetleftmenu.h>
#include <qt/blocknetproposals.h>
#include <qt/blocknetsendfunds.h>
#include <qt/blocknettoolbar.h>
#include <qt/blocknettools.h>
#include <qt/blocknettransactionhistory.h>
#include <qt/blocknetvars.h>

#include <qt/clientmodel.h>
#include <qt/platformstyle.h>
#include <qt/walletmodel.h>

#include <QHash>
#include <QList>
#include <QProgressDialog>

#include <boost/thread.hpp>

class BlocknetWallet : public QFrame {
    Q_OBJECT

public:
    explicit BlocknetWallet(interfaces::Node & node, const PlatformStyle *platformStyle, QFrame *parent = nullptr);
    ~BlocknetWallet() override;

    void setClientModel(ClientModel *c) { this->clientModel = c; }

    bool addWallet(WalletModel *w) { this->wallets[w->getWalletName()] = w; return true; };
    bool setCurrentWallet(const QString &name);
    bool setCurrentWallet(WalletModel *w) {
        if (!this->wallets.count(w->getWalletName()))
            this->wallets[w->getWalletName()] = w;
        return setCurrentWallet(w->getWalletName());
    };
    bool removeWallet(const QString &name) { this->wallets.remove(name); return true; };
    bool removeWallet(WalletModel *w) { return removeWallet(w->getWalletName()); };
    void removeAllWallets() { this->wallets.clear(); };
    bool handlePaymentRequest(const SendCoinsRecipient & recipient) { return false; }; // TODO Blocknet Qt payment requests
    void showOutOfSyncWarning(bool fShow) { Q_EMIT requestedSyncWarningInfo(); };

    WalletModel* currentWalletModel() const { return walletModel; };

    void setProgress(int progress, const QString &msg, int maximum = 100);
    void updateStakingStatus(WalletModel *w);
    void setPeers(int peers);
    void setLock(bool lock, bool stakingOnly = false); // TODO Handle timed unlock

Q_SIGNALS:
    void encryptionStatusChanged(WalletModel::EncryptionStatus encryptStatus);
    void addressbook();
    void request();
    void tools();
    void handleRestart(QStringList args);
    void requestedSyncWarningInfo();
    /** Notify that a new transaction appeared */
    void incomingTransaction(const QString& date, int unit, const CAmount& amount, const QString& type,
            const QString& address, const QString& label, const QString& walletName);
    void progressClicked();


public Q_SLOTS:
    void setPage(BlocknetPage page);
    void gotoOverviewPage() { goToDashboard(); };
    void gotoHistoryPage() { goToHistory(); };
    void gotoReceiveCoinsPage() { }; // TODO Blocknet Qt wire up receive coins page
    void gotoSendCoinsPage(QString addr = "") { onSendToAddress(addr); };
    void gotoSettingsPage() { setPage(BlocknetPage::SETTINGS); };
    void gotoSignMessageTab(QString addr);
    void gotoVerifyMessageTab(QString addr);
    void encryptWallet(bool status);
    void backupWallet();
    void changePassphrase();
    void unlockWallet();
    void showProgress(const QString &title, int nProgress);
    void usedSendingAddresses();
    void usedReceivingAddresses();
    void onLockRequest(bool locked, bool stakingOnly);

protected Q_SLOTS:
    void onSendFunds();
    void onSendToAddress(const QString &);
    void onRescanRequest(const std::string &);
    void goToDashboard();
    void goToQuickSend();
    void goToHistory();
    void goToCreateProposal();
    void goToProposals();
    void balanceChanged(const interfaces::WalletBalances & balances);
    void displayUnitChanged(int unit);
    void processNewTransaction(const QModelIndex& parent, int start, int /*end*/);
    void onEncryptionStatus();

private:
    interfaces::Node &node;
    const PlatformStyle *platformStyle;
    QHash<QString, WalletModel*> wallets;
    ClientModel *clientModel;
    WalletModel *walletModel;
    BlocknetPage page;
    boost::thread_group tg;

    BlocknetLeftMenu *leftMenu;
    QFrame *contentBox;
    BlocknetToolBar *toolbar;
    BlocknetSendFunds *sendFunds = nullptr;
    BlocknetDashboard *dashboard = nullptr;
    BlocknetCreateProposal *createProposal = nullptr;
    BlocknetTools *btools = nullptr;
    QWidget *screen = nullptr;
    QProgressDialog *progressDialog = nullptr;
};

#endif // BLOCKNET_QT_BLOCKNETWALLET_H
