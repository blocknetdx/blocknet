// Copyright (c) 2018-2019 The Blocknet developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BLOCKNET_QT_BLOCKNETSENDFUNDS1_H
#define BLOCKNET_QT_BLOCKNETSENDFUNDS1_H

#include <qt/blocknetaddresseditor.h>
#include <qt/blocknetformbtn.h>
#include <qt/blocknetsendfundsutil.h>

#include <qt/walletmodel.h>

#include <QFrame>
#include <QLabel>
#include <QVBoxLayout>

class BlocknetSendFunds1 : public BlocknetSendFundsPage {
    Q_OBJECT
protected:
    void keyPressEvent(QKeyEvent *event) override;
    void focusInEvent(QFocusEvent *event) override;

public:
    explicit BlocknetSendFunds1(WalletModel *w, int id, QFrame *parent = nullptr);
    void setData(BlocknetSendFundsModel *model) override;
    bool validated() override;
    void addAddress(const QString &address);

public Q_SLOTS:
    void clear() override;
    void textChanged();
    void onAddressesChanged();
    void openAddressBook();

private:
    QVBoxLayout *layout;
    QLabel *titleLbl;
    BlocknetAddressEditor *addressTi;
    BlocknetFormBtn *continueBtn;
};

#endif // BLOCKNET_QT_BLOCKNETSENDFUNDS1_H
