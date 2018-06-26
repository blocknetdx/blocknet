// Copyright (c) 2018 The Blocknet Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BLOCKNETSENDFUNDSREQUEST_H
#define BLOCKNETSENDFUNDSREQUEST_H

#include "walletmodel.h"
#include "coincontrol.h"

#include <QObject>
#include <QWidget>

class BlocknetSendFundsRequest : public QObject {
    Q_OBJECT

public:
    explicit BlocknetSendFundsRequest(QWidget *widget, WalletModel *w, CCoinControl *coinControl = nullptr, QObject *parent = nullptr);
    WalletModel::SendCoinsReturn send(QList<SendCoinsRecipient> &recipients, CAmount &txFees, CAmount &txAmount);
    QString sendStatusMsg(const WalletModel::SendCoinsReturn &scr, const QString &txFeeStr, int displayUnit);

private:
    WalletModel *walletModel;
    CCoinControl *coinControl = nullptr;

    QWidget *widget;
};

#endif //BLOCKNETSENDFUNDSREQUEST_H
