// Copyright (c) 2018 The Blocknet Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BLOCKNETSENDFUNDS3_H
#define BLOCKNETSENDFUNDS3_H

#include "blocknetsendfundsutil.h"
#include "blocknetformbtn.h"
#include "blocknetlineedit.h"

#include "walletmodel.h"
#include "coincontrol.h"

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

signals:
    void feeOption(double fee);

private slots:
    void onFeeDesignation();
    void onSpecificFee();
    void onEncryptionStatus(int encStatus);
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
    void updateTxFees(CAmount fee);
    void showZeroFeeMsg(bool show = true);
};

#endif // BLOCKNETSENDFUNDS3_H
