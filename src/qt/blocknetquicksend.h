// Copyright (c) 2018 The Blocknet Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BLOCKNETQUICKSEND_H
#define BLOCKNETQUICKSEND_H

#include "blocknetformbtn.h"
#include "blocknetlineedit.h"
#include "blocknetsendfundsutil.h"

#include "walletmodel.h"

#include <QFrame>
#include <QLabel>
#include <QVBoxLayout>
#include <QList>
#include <QShowEvent>
#include <QHideEvent>

class BlocknetQuickSend : public QFrame
{
    Q_OBJECT
public:
    explicit BlocknetQuickSend(WalletModel *w, QFrame *parent = nullptr);
    bool validated();

signals:
    void dashboard();
    void submit();

public slots:
    void onSubmit();
    void onCancel() { emit dashboard(); }

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;

private slots:
    void onAmountChanged(const QString &text = QString());
    void onDisplayUnit(int);
    void onEncryptionStatus(int encStatus);

private:
    WalletModel *walletModel;
    int displayUnit;
    CAmount lastAmount{0};
    CAmount totalAmount{0};
    CAmount txAmount{0};
    CAmount txFees{0};

    QVBoxLayout *layout;
    QLabel *titleLbl;
    BlocknetLineEdit *addressTi;
    BlocknetLineEdit *amountTi;
    QLabel *feeValueLbl;
    QLabel *totalValueLbl;
    QLabel *warningLbl;
    BlocknetFormBtn *cancelBtn;
    BlocknetFormBtn *confirmBtn;

    WalletModel::SendCoinsReturn processFunds(bool submitFunds = false);
};

#endif // BLOCKNETQUICKSEND_H