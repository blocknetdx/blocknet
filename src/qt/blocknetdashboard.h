// Copyright (c) 2018 The Blocknet Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BLOCKNETDASHBOARD_H
#define BLOCKNETDASHBOARD_H

#include "blocknetvars.h"

#include "walletmodel.h"
#include "transactionrecord.h"

#include <QFrame>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QVector>
#include <QDateTime>
#include <QScrollArea>
#include <QVector>

class BlocknetDashboard : public QFrame
{
    Q_OBJECT

public:
    explicit BlocknetDashboard(QFrame *parent = nullptr);
    void setWalletModel(WalletModel *w);

    struct BlocknetRecentTransaction {
        int status;
        QDateTime date;
        QString alias;
        QString type;
        qint64 amount;
        QString tooltip;
        explicit BlocknetRecentTransaction(int a = 0, QDateTime b = QDateTime(), QString c = QString(),
                QString d = QString(), qint64 e = 0, QString f = QString()) : status(a), date(b), alias(c), type(d), amount(e), tooltip(f) { }
    };

    void setRecentTransactions(QVector<BlocknetRecentTransaction> transactions);

signals:
    void quicksend();
    void history();

public slots:
    void onQuickSend() { emit quicksend(); }
    void balanceChanged(CAmount walletBalance, CAmount unconfirmed, CAmount immature, CAmount anonymized, CAmount watch,
                        CAmount watchUnconfirmed, CAmount watchImmature);
    void displayUnitChanged(int displayUnit);
    void onRecentTransactions(QVector<BlocknetDashboard::BlocknetRecentTransaction> &txs);

private slots:
    void onViewAll() { emit history(); };

private:
    QLabel *titleLbl;
    QLabel *balanceLbl;
    QLabel *balanceValueLbl;
    QLabel *pendingLbl;
    QLabel *pendingValueLbl;
    QLabel *immatureLbl;
    QLabel *immatureValueLbl;
    QLabel *totalLbl;
    QLabel *totalValueLbl;
    QPushButton *viewAll;
    QLabel *recentTxsLbl;
    QFrame *recentTransactionsGrid;
    QScrollArea *recentTransactionsGridScrollArea;
    QVBoxLayout *layout;

    WalletModel *walletModel;
    int displayUnit;
    CAmount walletBalance;
    CAmount unconfirmedBalance;
    CAmount immatureBalance;
    QVector<BlocknetRecentTransaction> transactions;

    void updateBalance();
};

#endif // BLOCKNETDASHBOARD_H
