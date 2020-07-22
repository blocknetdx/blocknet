// Copyright (c) 2018-2019 The Blocknet developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BLOCKNET_QT_BLOCKNETLEFTMENU_H
#define BLOCKNET_QT_BLOCKNETLEFTMENU_H

#include <qt/blockneticonlabel.h>
#include <qt/blocknetvars.h>

#include <amount.h>

#include <QAbstractButton>
#include <QFrame>
#include <QLabel>
#include <QWidget>

class BlocknetLeftMenu : public QFrame
{
    Q_OBJECT

public:
    explicit BlocknetLeftMenu(QFrame *parent = nullptr);
    void setBalance(CAmount balance, int unit);
    void selectMenu(BlocknetPage menuType);

Q_SIGNALS:
    void menuChanged(BlocknetPage menuType);

public Q_SLOTS:

private Q_SLOTS:
    void onMenuClicked(bool);

private:
    QVBoxLayout *layout;
    QLabel *logo;
    QLabel *balanceLbl;
    QLabel *balanceAmountLbl;

    QButtonGroup *group;
    BlocknetIconLabel *dashboard;
    BlocknetIconLabel *addressBook;
    BlocknetIconLabel *accounts;
    BlocknetIconLabel *sendFunds;
    BlocknetIconLabel *requestFunds;
    BlocknetIconLabel *transactionHistory;
    BlocknetIconLabel *snodes;
    BlocknetIconLabel *proposals;
    BlocknetIconLabel *announcements;
    BlocknetIconLabel *settings;
    BlocknetIconLabel *tools;
    QList<QAbstractButton*> btns;

    QLabel *versionLbl;
    int selected;
};

#endif // BLOCKNET_QT_BLOCKNETLEFTMENU_H
