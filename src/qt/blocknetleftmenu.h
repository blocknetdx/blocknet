// Copyright (c) 2018 The Blocknet Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BLOCKNETLEFTMENU_H
#define BLOCKNETLEFTMENU_H

#include "blocknetvars.h"
#include "blockneticonlabel.h"

#include "amount.h"

#include <QWidget>
#include <QFrame>
#include <QLabel>

class BlocknetLeftMenu : public QFrame
{
    Q_OBJECT

public:
    explicit BlocknetLeftMenu(QFrame *parent = nullptr);
    void setBalance(CAmount balance, int unit);
    void selectMenu(BlocknetPage menuType);

signals:
    void menuChanged(BlocknetPage menuType);

public slots:

private slots:
    void onMenuSelected(int menuType, bool selected);

private:
    QVBoxLayout *layout;
    QLabel *logo;
    QLabel *balanceLbl;
    QLabel *balanceAmountLbl;

    QButtonGroup *group;
    BlocknetIconLabel *dashboard;
    BlocknetIconLabel *addressBook;
    BlocknetIconLabel *sendFunds;
    BlocknetIconLabel *requestFunds;
    BlocknetIconLabel *transactionHistory;
    BlocknetIconLabel *snodes;
    BlocknetIconLabel *proposals;
    BlocknetIconLabel *announcements;
    BlocknetIconLabel *settings;
    BlocknetIconLabel *tools;

    QLabel *versionLbl;
};

#endif // BLOCKNETLEFTMENU_H
