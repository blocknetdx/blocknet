// Copyright (c) 2018 The Blocknet Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BLOCKNETCREATEPROPOSAL3_H
#define BLOCKNETCREATEPROPOSAL3_H

#include "blocknetcreateproposalutil.h"
#include "blocknetformbtn.h"

#include "walletmodel.h"

#include <QFrame>
#include <QVBoxLayout>
#include <QLabel>
#include <QTimer>

class BlocknetCreateProposal3 : public BlocknetCreateProposalPage {
    Q_OBJECT
public:
    explicit BlocknetCreateProposal3(int id, QFrame *parent = nullptr);
    void setModel(const BlocknetCreateProposalPageModel &model);
    void clear() override;
    bool validated() override;

protected:
    void keyPressEvent(QKeyEvent *event) override;

signals:
    void done();

public slots:
    void onCancel() override;

private slots:
    void onSubmit();

private:
    QVBoxLayout *layout;
    QLabel *titleLbl;
    QLabel *subtitleLbl;
    QLabel *proposalTitleLbl;
    QLabel *proposalLbl;
    QLabel *proposalDetailTitleLbl;
    QLabel *proposalDetailLbl;
    QLabel *feeHashLbl;
    QLabel *feeHashValLbl;
    QLabel *feeTitleLbl;
    QLabel *feeLbl;
    BlocknetFormBtn *submitBtn;
    BlocknetCreateProposalPageModel model;
    QTimer *timer;

    int collateralConfirmations();
};

#endif // BLOCKNETCREATEPROPOSAL3_H
