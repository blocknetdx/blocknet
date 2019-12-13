// Copyright (c) 2018-2019 The Blocknet developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BLOCKNET_QT_BLOCKNETPEERSLIST_H
#define BLOCKNET_QT_BLOCKNETPEERSLIST_H

#include <qt/blocknetpeerdetails.h>
#include <qt/blocknettoolspage.h>

#include <qt/clientmodel.h>

#include <QFrame>
#include <QHideEvent>
#include <QLabel>
#include <QShowEvent>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

class BlocknetPeersList : public BlocknetToolsPage {
    Q_OBJECT
protected:

public:
    explicit BlocknetPeersList(QWidget *, int id, QFrame *parent = nullptr);
    void setClientModel(ClientModel *c);

protected:
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;

private Q_SLOTS:
    void displayPeerDetails(const QItemSelection &, const QItemSelection &);

private:
    QVBoxLayout *layout;
    QLabel *titleLbl;
    QTableView *table;
    ClientModel *clientModel = nullptr;
    BlocknetPeerDetails *peerDetails = nullptr;

    enum {
        COLUMN_ADDRESS,
        COLUMN_VERSION,
        COLUMN_PING_TIME,
        COLUMN_BAN_SCORE,
        COLUMN_LATEST_BLOCK
    };
};

#endif // BLOCKNET_QT_BLOCKNETPEERSLIST_H
