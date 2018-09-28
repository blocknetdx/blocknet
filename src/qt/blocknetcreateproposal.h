// Copyright (c) 2018 The Blocknet Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BLOCKNETCREATEPROPOSAL_H
#define BLOCKNETCREATEPROPOSAL_H

#include "blocknetbreadcrumb.h"
#include "blocknetcreateproposalutil.h"
#include "blocknetcreateproposal1.h"
#include "blocknetcreateproposal2.h"
#include "blocknetcreateproposal3.h"

#include <QFrame>
#include <QWidget>
#include <QVBoxLayout>
#include <QSet>

class BlocknetCreateProposal : public QFrame
{
    Q_OBJECT

public:
    explicit BlocknetCreateProposal(QWidget *parent = nullptr);
    void clear() {
        page1->clear();
        page2->clear();
        page3->clear();
    }

signals:
    void done();

protected:
    bool event(QEvent *event) override;
    void focusInEvent(QFocusEvent *event) override;
    void showEvent(QShowEvent *event) override;

private slots:
    void crumbChanged(int crumb);
    void nextCrumb(int crumb);
    void prevCrumb(int crumb);
    void onCancel(int crumb);
    void onDone();
    void reset();

private:
    QVector<BlocknetCreateProposalPage*> pages;

    QVBoxLayout *layout;
    BlocknetCreateProposal1 *page1;
    BlocknetCreateProposal2 *page2;
    BlocknetCreateProposal3 *page3;
    BlocknetBreadCrumb *breadCrumb;
    BlocknetCreateProposalPage *screen = nullptr;

    void positionCrumb(QPoint pt = QPoint());
    void goToDone();
};

#endif // BLOCKNETCREATEPROPOSAL_H
