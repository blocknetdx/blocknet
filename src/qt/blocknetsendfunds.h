// Copyright (c) 2018 The Blocknet Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BLOCKNETSENDFUNDS_H
#define BLOCKNETSENDFUNDS_H

#include "blocknetbreadcrumb.h"
#include "blocknetsendfundsutil.h"
#include "blocknetsendfunds1.h"
#include "blocknetsendfunds2.h"
#include "blocknetsendfunds3.h"
#include "blocknetsendfunds4.h"
#include "blocknetsendfundsdone.h"

#include "walletmodel.h"
#include "coincontrol.h"

#include <QFrame>
#include <QVBoxLayout>
#include <QSet>

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

signals:
    void sent();
    void dashboard();

public slots:

protected:
    bool event(QEvent *event) override;

    void focusInEvent(QFocusEvent *event) override;

private slots:
    void crumbChanged(int crumb);
    void nextCrumb(int crumb);
    void prevCrumb(int crumb);
    void onCancel(int crumb);
    void onEdit();
    void onSendFunds();
    void goToDashboard() { emit dashboard(); };
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

#endif // BLOCKNETSENDFUNDS_H
