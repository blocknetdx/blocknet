// Copyright (c) 2018 The Blocknet Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BLOCKNETADDRESSBOOK_H
#define BLOCKNETADDRESSBOOK_H

#include "walletmodel.h"
#include "blocknetdropdown.h"

#include <QFrame>
#include <QVBoxLayout>
#include <QLabel>

class BlocknetAddressBook : public QFrame {
    Q_OBJECT
public:
    explicit BlocknetAddressBook(WalletModel *w, QFrame *parent = nullptr);
    void setWalletModel(WalletModel *w);

private:
    WalletModel *walletModel;
    QVBoxLayout *layout;
    QLabel *titleLbl;
    QLabel *addButtonLbl;
    QLabel *createButtonLbl;
    QLabel *filterLbl;
    BlocknetDropdown *addressDropdown;
};

#endif // BLOCKNETADDRESSBOOK_H
