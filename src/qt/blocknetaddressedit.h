// Copyright (c) 2018 The Blocknet Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BLOCKNETADDRESSEDIT_H
#define BLOCKNETADDRESSEDIT_H

#include "blocknetformbtn.h"
#include "blocknetlineeditwithtitle.h"

#include "walletmodel.h"

#include <QFrame>
#include <QVBoxLayout>
#include <QLabel>
#include <QRadioButton>

class BlocknetAddressEdit : public QFrame {
    Q_OBJECT
public:
    explicit BlocknetAddressEdit(WalletModel *w, bool editMode = false, QFrame *parent = nullptr);

public slots:
    void clear();

private:
    WalletModel *walletModel;
    bool editMode;
    QVBoxLayout *layout;
    QLabel *addressLbl;
    QLabel *titleLbl;
    BlocknetLineEditWithTitle *addressTi;
    BlocknetLineEditWithTitle *aliasTi;
    QRadioButton *myAddressBtn;
    QRadioButton *otherUserBtn;
    BlocknetFormBtn *confirmBtn;
    BlocknetFormBtn *cancelBtn;
};

#endif // BLOCKNETADDRESSEDIT_H