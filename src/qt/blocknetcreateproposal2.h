// Copyright (c) 2018-2020 The Blocknet developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BLOCKNET_QT_BLOCKNETCREATEPROPOSAL2_H
#define BLOCKNET_QT_BLOCKNETCREATEPROPOSAL2_H

#include <qt/blocknetcreateproposalutil.h>
#include <qt/blocknetformbtn.h>

#include <qt/walletmodel.h>

#include <QFrame>
#include <QLabel>
#include <QVBoxLayout>

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
    QLabel *proposalAddrLbl;
    QLabel *proposalAddrValLbl;
    QLabel *urlLbl;
    QLabel *urlValLbl;
    QLabel *feeTitleLbl;
    QLabel *feeLbl;
    QLabel *descLbl;
    QLabel *descValLbl;
    BlocknetFormBtn *backBtn;
    BlocknetFormBtn *submitBtn;
    BlocknetFormBtn *cancelBtn;
    BlocknetCreateProposalPageModel model;

    void disableButtons(const bool &disable);
};

#endif // BLOCKNET_QT_BLOCKNETCREATEPROPOSAL2_H
