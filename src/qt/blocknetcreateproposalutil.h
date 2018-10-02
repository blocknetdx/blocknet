// Copyright (c) 2018 The Blocknet Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BLOCKNETCREATEPROPOSALUTIL_H
#define BLOCKNETCREATEPROPOSALUTIL_H

#include "uint256.h"
#include "base58.h"

#include <QFrame>
#include <QWidget>
#include <QMessageBox>

struct BlocknetCreateProposalPageModel {
    std::string name;
    std::string url;
    int paymentCount;
    int superblock;
    int amount;
    CBitcoinAddress address;
    uint256 collateral;
};

class BlocknetCreateProposalPage : public QFrame {
    Q_OBJECT
public:
    explicit BlocknetCreateProposalPage(int id, QWidget *parent = nullptr) : QFrame(parent), pageID(id) { }
    virtual void clear() {};
    virtual bool validated() { return true; };

signals:
    void next(int pageID);
    void back(int pageID);
    void cancel(int pageID);

public slots:
    virtual void onNext() { emit next(pageID); }
    virtual void onBack() { emit back(pageID); }
    virtual void onCancel() {
        QMessageBox::StandardButton retval = QMessageBox::question(this, tr("Cancel Proposal Submission"),
                                                tr("Are you sure you want to cancel this proposal submission?"),
                                                QMessageBox::Yes | QMessageBox::No,
                                                QMessageBox::No);

        if (retval != QMessageBox::Yes)
            return;

        emit cancel(pageID);
    }

protected:
    int pageID{0};
};

#endif //BLOCKNETCREATEPROPOSALUTIL_H
