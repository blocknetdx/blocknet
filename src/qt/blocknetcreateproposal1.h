// Copyright (c) 2018 The Blocknet Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BLOCKNETCREATEPROPOSAL1_H
#define BLOCKNETCREATEPROPOSAL1_H

#include "blocknetcreateproposalutil.h"
#include "blocknetformbtn.h"
#include "blocknetlineeditwithtitle.h"

#include "walletmodel.h"
#include "base58.h"

#include <QFrame>
#include <QVBoxLayout>
#include <QLabel>

class BlocknetCreateProposal1 : public BlocknetCreateProposalPage {
    Q_OBJECT
public:
    explicit BlocknetCreateProposal1(int id, QFrame *parent = nullptr);
    bool validated() override;
    BlocknetCreateProposalPageModel getModel() {
        auto proposalAddr = paymentAddrTi->lineEdit->text().toStdString();
        CBitcoinAddress address(proposalAddr);
        return {
            proposalTi->lineEdit->text().toStdString(),
            urlTi->lineEdit->text().toStdString(),
            paymentCountTi->lineEdit->text().toInt(),
            superBlockTi->lineEdit->text().toInt(),
            amountTi->lineEdit->text().toInt(),
            address,
            uint256()
        };
    }

public slots:
    void clear() override;

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void focusInEvent(QFocusEvent *event) override;

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
    BlocknetLineEditWithTitle *paymentAddrTi;
    BlocknetFormBtn *continueBtn;
    BlocknetFormBtn *cancelBtn;
};

#endif // BLOCKNETCREATEPROPOSAL1_H
