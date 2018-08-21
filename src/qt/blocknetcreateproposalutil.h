// Copyright (c) 2018 The Blocknet Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BLOCKNETCREATEPROPOSALUTIL_H
#define BLOCKNETCREATEPROPOSALUTIL_H

#include "walletmodel.h"

#include <QFrame>

class BlocknetCreateProposalPage : public QFrame {
    Q_OBJECT
public:
    explicit BlocknetCreateProposalPage(WalletModel *w, int id, QFrame *parent = nullptr) : QFrame(parent), walletModel(w), pageID(id) { }
    void setWalletModel(WalletModel *w) { walletModel = w; }
    virtual void clear() {};

signals:
    void next(int pageID);
    void back(int pageID);
    void cancel(int pageID);

public slots:
    void onNext() { emit next(pageID); }
    void onBack() { emit back(pageID); }
    void onCancel() { emit cancel(pageID); }

protected:
    WalletModel *walletModel;
    int pageID{0};
};

#endif //BLOCKNETCREATEPROPOSALUTIL_H
