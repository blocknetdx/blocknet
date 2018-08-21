// Copyright (c) 2018 The Blocknet Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BLOCKNETCREATEPROPOSAL1_H
#define BLOCKNETCREATEPROPOSAL1_H

#include "blocknetcreateproposalutil.h"
#include "blocknetformbtn.h"
#include "blocknetlineeditwithtitle.h"

#include "walletmodel.h"

#include <QFrame>
#include <QVBoxLayout>
#include <QLabel>

class BlocknetCreateProposal1 : public BlocknetCreateProposalPage {
    Q_OBJECT
public:
    explicit BlocknetCreateProposal1(WalletModel *w, int id, QFrame *parent = nullptr);

public slots:
    void clear() override;

protected:
    void keyPressEvent(QKeyEvent *event) override;

private:
    QVBoxLayout *layout;
    QLabel *titleLbl;
    QLabel *feeTitleLbl;
    QLabel *feeLbl;
    BlocknetLineEditWithTitle *proposalTi;
    BlocknetLineEditWithTitle *urlTi;
    BlocknetLineEditWithTitle *paymentCountTi;
    BlocknetLineEditWithTitle *superBlockTi;
    BlocknetLineEditWithTitle *amountTi;
    BlocknetFormBtn *continueBtn;
    BlocknetFormBtn *cancelBtn;
};

#endif // BLOCKNETCREATEPROPOSAL1_H
