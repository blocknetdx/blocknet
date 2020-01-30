// Copyright (c) 2018-2020 The Blocknet developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BLOCKNET_QT_BLOCKNETCREATEPROPOSALUTIL_H
#define BLOCKNET_QT_BLOCKNETCREATEPROPOSALUTIL_H

#include <qt/walletmodel.h>

#include <base58.h>
#include <pubkey.h>
#include <script/standard.h>
#include <uint256.h>

#include <QFrame>
#include <QMessageBox>
#include <QWidget>

struct BlocknetCreateProposalPageModel {
    std::string name;
    std::string url;
    std::string description;
    int superblock;
    int amount;
    CTxDestination address;
    uint256 feehash;
};

class BlocknetCreateProposalPage : public QFrame {
    Q_OBJECT
public:
    explicit BlocknetCreateProposalPage(int id, QWidget *parent = nullptr) : QFrame(parent), pageID(id) { }
    virtual void setWalletModel(WalletModel *w) { walletModel = w; };
    virtual void clear() {};
    virtual bool validated() { return true; };

Q_SIGNALS:
    void next(int pageID);
    void back(int pageID);
    void cancel(int pageID);

public Q_SLOTS:
    virtual void onNext() { Q_EMIT next(pageID); }
    virtual void onBack() { Q_EMIT back(pageID); }
    virtual void onCancel() {
        QMessageBox::StandardButton retval = QMessageBox::question(this, tr("Cancel Proposal Submission"),
                                                tr("Are you sure you want to cancel this proposal submission?"),
                                                QMessageBox::Yes | QMessageBox::No,
                                                QMessageBox::No);

        if (retval != QMessageBox::Yes)
            return;

        Q_EMIT cancel(pageID);
    }

protected:
    int pageID{0};
    WalletModel *walletModel = nullptr;
};

#endif //BLOCKNET_QT_BLOCKNETCREATEPROPOSALUTIL_H
