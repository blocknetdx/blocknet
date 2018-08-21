// Copyright (c) 2018 The Blocknet Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BLOCKNETCREATEPROPOSAL2_H
#define BLOCKNETCREATEPROPOSAL2_H

#include "blocknetcreateproposalutil.h"
#include "blocknetformbtn.h"

#include "walletmodel.h"

#include <QFrame>
#include <QVBoxLayout>
#include <QLabel>

class BlocknetCreateProposal2 : public BlocknetCreateProposalPage {
    Q_OBJECT
public:
    explicit BlocknetCreateProposal2(WalletModel *w, int id, QFrame *parent = nullptr);
    void clear() override;

protected:
    void keyPressEvent(QKeyEvent *event) override;

private:
    QVBoxLayout *layout;
    QLabel *titleLbl;
    QLabel *proposalTitleLbl;
    QLabel *proposalLbl;
    QLabel *proposalDetailTitleLbl;
    QLabel *proposalDetailLbl;
    QLabel *feeTitleLbl;
    QLabel *feeLbl;
    BlocknetFormBtn *submitBtn;
    BlocknetFormBtn *cancelBtn;
};

#endif // BLOCKNETCREATEPROPOSAL2_H
