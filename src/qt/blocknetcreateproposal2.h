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
    explicit BlocknetCreateProposal2(int id, QFrame *parent = nullptr);
    void setModel(const BlocknetCreateProposalPageModel &model);
    void clear() override;
    bool validated() override;
    BlocknetCreateProposalPageModel getModel() {
        return model;
    }

protected:
    void keyPressEvent(QKeyEvent *event) override;

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
    QLabel *proposalAddrLbl;
    QLabel *proposalAddrValLbl;
    QLabel *urlLbl;
    QLabel *urlValLbl;
    QLabel *feeTitleLbl;
    QLabel *feeLbl;
    BlocknetFormBtn *backBtn;
    BlocknetFormBtn *submitBtn;
    BlocknetFormBtn *cancelBtn;
    BlocknetCreateProposalPageModel model;

    void disableButtons(const bool &disable);
};

#endif // BLOCKNETCREATEPROPOSAL2_H
