// Copyright (c) 2018 The Blocknet Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BLOCKNETCREATEPROPOSAL_H
#define BLOCKNETCREATEPROPOSAL_H

#include "blocknetbreadcrumb.h"
#include "blocknetcreateproposalutil.h"
#include "blocknetcreateproposal1.h"
#include "blocknetcreateproposal2.h"
#include "walletmodel.h"

#include <QFrame>
#include <QVBoxLayout>
#include <QSet>

class BlocknetCreateProposal : public QFrame
{
    Q_OBJECT

public:
    explicit BlocknetCreateProposal(WalletModel *w, QFrame *parent = nullptr);
    void setWalletModel(WalletModel *w) {
        walletModel = w;
        page1->setWalletModel(walletModel);
        page2->setWalletModel(walletModel);
    }
    void clear() {
        //this->model->reset();
        page1->clear();
        page2->clear();
    }

protected:
    bool event(QEvent *event) override;

private slots:
    void crumbChanged(int crumb);
    void nextCrumb(int crumb);
    void prevCrumb(int crumb);
    void onCancel(int crumb);
    void goToDashboard() { /*emit dashboard();*/ };
    void onDoneDashboard();
    void reset();

private:
    WalletModel *walletModel;
    QVector<BlocknetCreateProposalPage*> pages;

    QVBoxLayout *layout;
    BlocknetCreateProposal1 *page1;
    BlocknetCreateProposal2 *page2;
    BlocknetBreadCrumb *breadCrumb;
    BlocknetCreateProposalPage *screen = nullptr;

    void positionCrumb(QPoint pt = QPoint());
    void goToDone();
};

#endif // BLOCKNETCREATEPROPOSAL_H
