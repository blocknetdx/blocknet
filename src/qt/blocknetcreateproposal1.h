// Copyright (c) 2018-2019 The Blocknet developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BLOCKNETCREATEPROPOSAL1_H
#define BLOCKNETCREATEPROPOSAL1_H

#include <qt/blocknetcreateproposalutil.h>
#include <qt/blocknetformbtn.h>
#include <qt/blocknetlineeditwithtitle.h>

#include <qt/walletmodel.h>

#include <base58.h>
#include <key_io.h>

#include <QFrame>
#include <QLabel>
#include <QVBoxLayout>

class BlocknetCreateProposal1 : public BlocknetCreateProposalPage {
    Q_OBJECT
public:
    explicit BlocknetCreateProposal1(int id, QFrame *parent = nullptr);
    bool validated() override;
    BlocknetCreateProposalPageModel getModel() {
        auto proposalAddr = paymentAddrTi->lineEdit->text().toStdString();
        auto proposalDest = DecodeDestination(proposalAddr);
        return {
            proposalTi->lineEdit->text().toStdString(),
            urlTi->lineEdit->text().toStdString(),
            descriptionTi->lineEdit->text().toStdString(),
            superBlockTi->lineEdit->text().toInt(),
            amountTi->lineEdit->text().toInt(),
            proposalDest,
            uint256()
        };
    }

public Q_SLOTS:
    void clear() override;

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void focusInEvent(QFocusEvent *event) override;
    void showEvent(QShowEvent *event) override;

private:
    QVBoxLayout *layout;
    QLabel *titleLbl;
    QLabel *feeTitleLbl;
    QLabel *feeLbl;
    BlocknetLineEditWithTitle *proposalTi;
    BlocknetLineEditWithTitle *urlTi;
    BlocknetLineEditWithTitle *descriptionTi;
    BlocknetLineEditWithTitle *superBlockTi;
    BlocknetLineEditWithTitle *amountTi;
    BlocknetLineEditWithTitle *paymentAddrTi;
    BlocknetFormBtn *continueBtn;
    BlocknetFormBtn *cancelBtn;

    static int nextSuperblock();
};

#endif // BLOCKNETCREATEPROPOSAL1_H
