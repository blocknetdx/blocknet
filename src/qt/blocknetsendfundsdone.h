// Copyright (c) 2018 The Blocknet Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BLOCKNETSENDFUNDSDONE_H
#define BLOCKNETSENDFUNDSDONE_H

#include "blocknetformbtn.h"

#include <QFrame>
#include <QVBoxLayout>
#include <QLabel>

class BlocknetSendFundsDone : public QFrame
{
    Q_OBJECT
public:
    explicit BlocknetSendFundsDone(QFrame *parent = nullptr);

signals:
    void dashboard();
    void payment();

public slots:
    void onReturnToDashboard() {
        emit dashboard();
    };
    void onSendAnotherPayment() {
        emit payment();
    };

private:
    QVBoxLayout *layout;
    QLabel *titleLbl;
    BlocknetFormBtn *returnBtn;
    BlocknetFormBtn *sendBtn;
};

#endif // BLOCKNETSENDFUNDSDONE_H