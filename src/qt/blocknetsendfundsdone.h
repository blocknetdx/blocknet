// Copyright (c) 2018-2019 The Blocknet developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BLOCKNET_QT_BLOCKNETSENDFUNDSDONE_H
#define BLOCKNET_QT_BLOCKNETSENDFUNDSDONE_H

#include <qt/blocknetformbtn.h>

#include <QFrame>
#include <QLabel>
#include <QVBoxLayout>

class BlocknetSendFundsDone : public QFrame
{
    Q_OBJECT
public:
    explicit BlocknetSendFundsDone(QFrame *parent = nullptr);

Q_SIGNALS:
    void dashboard();
    void payment();

public Q_SLOTS:
    void onReturnToDashboard() {
        Q_EMIT dashboard();
    };
    void onSendAnotherPayment() {
        Q_EMIT payment();
    };

private:
    QVBoxLayout *layout;
    QLabel *titleLbl;
    BlocknetFormBtn *returnBtn;
    BlocknetFormBtn *sendBtn;
};

#endif // BLOCKNET_QT_BLOCKNETSENDFUNDSDONE_H