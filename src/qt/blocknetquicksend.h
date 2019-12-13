// Copyright (c) 2018-2019 The Blocknet developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BLOCKNET_QT_BLOCKNETQUICKSEND_H
#define BLOCKNET_QT_BLOCKNETQUICKSEND_H

#include <qt/blocknetformbtn.h>
#include <qt/blocknetlineedit.h>
#include <qt/blocknetsendfundsutil.h>

#include <qt/walletmodel.h>

#include <QFrame>
#include <QHideEvent>
#include <QLabel>
#include <QList>
#include <QShowEvent>
#include <QVBoxLayout>
#include <QWidget>

class BlocknetQuickSend : public QFrame
{
    Q_OBJECT
public:
    explicit BlocknetQuickSend(WalletModel *w, QWidget *parent = nullptr);
    bool validated();

Q_SIGNALS:
    void dashboard();
    void submit();

public Q_SLOTS:
    void onSubmit();
    void onCancel() { Q_EMIT dashboard(); }

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    bool focusNextPrevChild(bool next) override;

private Q_SLOTS:
    void onAmountChanged();
    void onDisplayUnit(int);
    void onEncryptionStatus();
    void openAddressBook();

private:
    WalletModel *walletModel;
    int displayUnit;
    CAmount totalAmount{0};
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

    void addAddress(const QString &address);
    void updateLabels(const WalletModel::SendCoinsReturn & result);
    WalletModel::SendCoinsReturn submitFunds();
};

#endif // BLOCKNET_QT_BLOCKNETQUICKSEND_H