// Copyright (c) 2018-2019 The Blocknet developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BLOCKNET_QT_BLOCKNETCREATEPROPOSAL_H
#define BLOCKNET_QT_BLOCKNETCREATEPROPOSAL_H

#include <qt/blocknetbreadcrumb.h>
#include <qt/blocknetcreateproposalutil.h>
#include <qt/blocknetcreateproposal1.h>
#include <qt/blocknetcreateproposal2.h>
#include <qt/blocknetcreateproposal3.h>

#include <QFrame>
#include <QSet>
#include <QVBoxLayout>
#include <QWidget>

class BlocknetCreateProposal : public QFrame
{
    Q_OBJECT

public:
    explicit BlocknetCreateProposal(QWidget *parent = nullptr);
    void setWalletModel(WalletModel *w) { walletModel = w; }
    void clear() {
        page1->clear();
        page2->clear();
        page3->clear();
    }

Q_SIGNALS:
    void done();

protected:
    bool event(QEvent *event) override;
    void focusInEvent(QFocusEvent *event) override;
    void showEvent(QShowEvent *event) override;

private Q_SLOTS:
    void crumbChanged(int crumb);
    void nextCrumb(int crumb);
    void prevCrumb(int crumb);
    void onCancel(int crumb);
    void onDone();
    void reset();

private:
    QVector<BlocknetCreateProposalPage*> pages;
    WalletModel *walletModel = nullptr;

    QVBoxLayout *layout;
    BlocknetCreateProposal1 *page1;
    BlocknetCreateProposal2 *page2;
    BlocknetCreateProposal3 *page3;
    BlocknetBreadCrumb *breadCrumb;
    BlocknetCreateProposalPage *screen = nullptr;

    void positionCrumb(QPoint pt = QPoint());
    void goToDone();
};

#endif // BLOCKNET_QT_BLOCKNETCREATEPROPOSAL_H
