// Copyright (c) 2018 The Blocknet Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BLOCKNETSENDFUNDS1_H
#define BLOCKNETSENDFUNDS1_H

#include "blocknetsendfundsutil.h"
#include "blocknetformbtn.h"
#include "blocknetaddresseditor.h"

#include "walletmodel.h"

#include <QFrame>
#include <QVBoxLayout>
#include <QLabel>

class BlocknetSendFunds1 : public BlocknetSendFundsPage {
    Q_OBJECT
protected:
    void keyPressEvent(QKeyEvent *event) override;
    void focusInEvent(QFocusEvent *event) override;

public:
    explicit BlocknetSendFunds1(WalletModel *w, int id, QFrame *parent = nullptr);
    void setData(BlocknetSendFundsModel *model) override;
    bool validated() override;

public slots:
    void clear() override;
    void textChanged();
    void onAddressesChanged();

private:
    QVBoxLayout *layout;
    QLabel *titleLbl;
    BlocknetAddressEditor *addressTi;
    BlocknetFormBtn *continueBtn;
};

#endif // BLOCKNETSENDFUNDS1_H
