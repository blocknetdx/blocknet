// Copyright (c) 2018 The Blocknet Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BLOCKNETPEERSLIST_H
#define BLOCKNETPEERSLIST_H

#include "blocknettools.h"
#include "blocknetpeerdetails.h"

#include <QFrame>
#include <QVBoxLayout>
#include <QLabel>
#include <QTableWidget>
#include <QTableWidgetItem>

class BlocknetPeersList : public BlocknetToolsPage {
    Q_OBJECT
protected:

public:
    explicit BlocknetPeersList(QWidget *popup, int id, QFrame *parent = nullptr);
    void setWalletModel(WalletModel *w);

    struct Peer {
        QString address;
        QString version;
        QString pingTime;
        QString banScore;
        QString latestBlock;
    };

private slots:
    void displayPeerDetails(const QItemSelection &, const QItemSelection &);

private:
    QVBoxLayout *layout;
    QLabel *titleLbl;
    QTableWidget *table;
    QTableWidgetItem *contextItem = nullptr;
    QVector<Peer> dataModel;
    BlocknetPeerDetails *peerDetails = nullptr;
    QWidget *popupWidget;

    void initialize();
    void setData(QVector<Peer> data);
    void unwatch();
    void watch();

    enum {
        COLUMN_ADDRESS,
        COLUMN_VERSION,
        COLUMN_PING_TIME,
        COLUMN_BAN_SCORE,
        COLUMN_LATEST_BLOCK
    };
};

#endif // BLOCKNETPEERSLIST_H
