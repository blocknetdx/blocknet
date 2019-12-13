// Copyright (c) 2018-2019 The Blocknet developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BLOCKNET_QT_BLOCKNETSENDFUNDSREQUEST_H
#define BLOCKNET_QT_BLOCKNETSENDFUNDSREQUEST_H

#include <qt/walletmodel.h>

#include <wallet/coincontrol.h>

#include <QObject>
#include <QWidget>

class BlocknetSendFundsRequest : public QObject {
    Q_OBJECT

public:
    explicit BlocknetSendFundsRequest(QWidget *widget, WalletModel *w, CCoinControl *coinControl = nullptr, QObject *parent = nullptr);
    WalletModel::SendCoinsReturn send(QList<SendCoinsRecipient> &recipients, CAmount &txFees, CAmount &txAmount, bool &walletWasUnlocked);
    static QString sendStatusMsg(const WalletModel::SendCoinsReturn &scr, const QString &txFeeStr, int displayUnit);

private:
    QWidget *widget;
    WalletModel *walletModel;
    CCoinControl *coinControl = nullptr;
};

#endif //BLOCKNET_QT_BLOCKNETSENDFUNDSREQUEST_H
