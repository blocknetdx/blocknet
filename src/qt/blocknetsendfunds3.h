// Copyright (c) 2018-2020 The Blocknet developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BLOCKNET_QT_BLOCKNETSENDFUNDS3_H
#define BLOCKNET_QT_BLOCKNETSENDFUNDS3_H

#include <qt/blocknetformbtn.h>
#include <qt/blocknetlineedit.h>
#include <qt/blocknetsendfundsutil.h>

#include <qt/walletmodel.h>

#include <wallet/coincontrol.h>

#include <QFrame>
#include <QVBoxLayout>
#include <QLabel>
#include <QRadioButton>
#include <QShowEvent>
#include <QHideEvent>

class BlocknetSendFunds3 : public BlocknetSendFundsPage {
    Q_OBJECT
protected:
    void keyPressEvent(QKeyEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;

public:
    explicit BlocknetSendFunds3(WalletModel *w, int id, QFrame *parent = nullptr);
    void setData(BlocknetSendFundsModel *model) override;
    bool validated() override;
    void clear() override;

Q_SIGNALS:
    void feeOption(double fee);

private Q_SLOTS:
    void onFeeDesignation();
    void onSpecificFee();
    void onEncryptionStatus();
    void onSubtractFee();
    void onDisplayUnit(int unit);

private:
    int displayUnit;

    QVBoxLayout *layout;
    QLabel *titleLbl;
    QLabel *recommendedDescLbl;
    QRadioButton *recommendedRb;
    QRadioButton *specificRb;
    BlocknetLineEdit *specificFeeTi;
    QLabel *specificFeeLbl;
    QLabel *totalFeeLbl;
    BlocknetFormBtn *continueBtn;
    BlocknetFormBtn *cancelBtn;
    QLabel *transactionFeeDesc;
    QCheckBox *subtractFeeCb;
    QLabel *warningLbl;

    void updateFee();
    void updateModelTxFees(CAmount fee);
};

#endif // BLOCKNET_QT_BLOCKNETSENDFUNDS3_H
