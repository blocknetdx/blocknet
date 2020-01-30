// Copyright (c) 2018-2020 The Blocknet developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BLOCKNET_QT_BLOCKNETCREATEPROPOSAL3_H
#define BLOCKNET_QT_BLOCKNETCREATEPROPOSAL3_H

#include <qt/blocknetcreateproposalutil.h>
#include <qt/blocknetformbtn.h>

#include <qt/walletmodel.h>

#include <QFrame>
#include <QLabel>
#include <QTimer>
#include <QVBoxLayout>

class BlocknetCreateProposal3 : public BlocknetCreateProposalPage {
    Q_OBJECT
public:
    explicit BlocknetCreateProposal3(int id, QFrame *parent = nullptr);
    void setModel(const BlocknetCreateProposalPageModel &model);
    void clear() override;
    bool validated() override;

protected:
    void keyPressEvent(QKeyEvent *event) override;

Q_SIGNALS:
    void done();

public Q_SLOTS:
    void onCancel() override;

private Q_SLOTS:
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
    BlocknetFormBtn *doneBtn;
    BlocknetCreateProposalPageModel model;
    QTimer *timer;

    int collateralConfirmations();
};

#endif // BLOCKNET_QT_BLOCKNETCREATEPROPOSAL3_H
