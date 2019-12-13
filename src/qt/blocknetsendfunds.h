// Copyright (c) 2018-2019 The Blocknet developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BLOCKNET_QT_BLOCKNETSENDFUNDS_H
#define BLOCKNET_QT_BLOCKNETSENDFUNDS_H

#include <qt/blocknetbreadcrumb.h>
#include <qt/blocknetsendfunds1.h>
#include <qt/blocknetsendfunds2.h>
#include <qt/blocknetsendfunds3.h>
#include <qt/blocknetsendfunds4.h>
#include <qt/blocknetsendfundsdone.h>
#include <qt/blocknetsendfundsutil.h>

#include <qt/walletmodel.h>

#include <wallet/coincontrol.h>

#include <QFrame>
#include <QSet>
#include <QVBoxLayout>

class BlocknetSendFunds : public QFrame {
    Q_OBJECT
public:
    explicit BlocknetSendFunds(WalletModel *w, QFrame *parent = nullptr);
    void setWalletModel(WalletModel *w) {
        walletModel = w;
        page1->setWalletModel(walletModel);
        page2->setWalletModel(walletModel);
        page3->setWalletModel(walletModel);
        page4->setWalletModel(walletModel);
    }
    void clear() {
        this->model->reset();
        page1->clear();
        page2->clear();
        page3->clear();
        page4->clear();
    }
    void addAddress(const QString &address);

Q_SIGNALS:
    void sent();
    void dashboard();

public Q_SLOTS:

protected:
    bool event(QEvent *event) override;

    void focusInEvent(QFocusEvent *event) override;

private Q_SLOTS:
    void crumbChanged(int crumb);
    void nextCrumb(int crumb);
    void prevCrumb(int crumb);
    void onCancel(int crumb);
    void onEdit();
    void onSendFunds();
    void goToDashboard() { Q_EMIT dashboard(); };
    void onDoneDashboard();
    void reset();

private:
    WalletModel *walletModel;
    BlocknetSendFundsModel *model;
    QVector<BlocknetSendFundsPage*> pages;

    QVBoxLayout *layout;
    BlocknetSendFunds1 *page1;
    BlocknetSendFunds2 *page2;
    BlocknetSendFunds3 *page3;
    BlocknetSendFunds4 *page4;
    BlocknetSendFundsDone *done;
    BlocknetBreadCrumb *breadCrumb;
    BlocknetSendFundsPage *screen = nullptr;

    void positionCrumb(QPoint pt = QPoint());
    void goToDone();
};

#endif // BLOCKNET_QT_BLOCKNETSENDFUNDS_H
