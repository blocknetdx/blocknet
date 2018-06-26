// Copyright (c) 2018 The Blocknet Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BLOCKNETSENDFUNDS4_H
#define BLOCKNETSENDFUNDS4_H

#include "blocknetsendfundsutil.h"
#include "blocknetformbtn.h"

#include "walletmodel.h"
#include "coincontrol.h"

#include <QFrame>
#include <QVBoxLayout>
#include <QLabel>
#include <QScrollArea>

class BlocknetSendFunds4 : public BlocknetSendFundsPage {
    Q_OBJECT

public:
    explicit BlocknetSendFunds4(WalletModel *w, int id, QFrame *parent = nullptr);
    void setData(BlocknetSendFundsModel *model) override;
    bool validated() override;
    void clear() override;

signals:
    void edit();
    void submit();

protected:
    void keyPressEvent(QKeyEvent *event) override;

private slots:
    void onEdit();
    void onSubmit();
    void onDisplayUnit(int unit);
    void onEncryptionStatus(int encStatus);

private:
    int displayUnit;

    QVBoxLayout *layout;
    QLabel *titleLbl;
    BlocknetFormBtn *continueBtn;
    BlocknetFormBtn *cancelBtn;
    QFrame *content;
    QVBoxLayout *contentLayout;
    QFrame *recipients = nullptr;
    QScrollArea *scrollArea = nullptr;
    QLabel *feeValueLbl = nullptr;
    QLabel *totalValueLbl = nullptr;
    QLabel *warningLbl = nullptr;

    void fillWalletData();
    void displayMultiple();
    void clearRecipients();
};

#endif // BLOCKNETSENDFUNDS4_H
