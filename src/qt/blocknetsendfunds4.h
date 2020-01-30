// Copyright (c) 2018-2020 The Blocknet developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BLOCKNET_QT_BLOCKNETSENDFUNDS4_H
#define BLOCKNET_QT_BLOCKNETSENDFUNDS4_H

#include <qt/blocknetformbtn.h>
#include <qt/blocknetsendfundsutil.h>

#include <qt/walletmodel.h>

#include <wallet/coincontrol.h>

#include <QFrame>
#include <QHideEvent>
#include <QLabel>
#include <QScrollArea>
#include <QShowEvent>
#include <QVBoxLayout>

class BlocknetSendFunds4 : public BlocknetSendFundsPage {
    Q_OBJECT

public:
    explicit BlocknetSendFunds4(WalletModel *w, int id, QFrame *parent = nullptr);
    void setData(BlocknetSendFundsModel *model) override;
    bool validated() override;
    void clear() override;

Q_SIGNALS:
    void edit();
    void submit();

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;

private Q_SLOTS:
    void onEdit();
    void onSubmit();
    void onDisplayUnit(int unit);
    void onEncryptionStatus();

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
    QString feeText(QString fee);
    QString totalText(QString total);
};

#endif // BLOCKNET_QT_BLOCKNETSENDFUNDS4_H
