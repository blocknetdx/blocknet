// Copyright (c) 2018 The Blocknet Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BLOCKNETPEERSLIST_H
#define BLOCKNETPEERSLIST_H

#include "blocknettools.h"
#include "blocknetpeerdetails.h"
#include "clientmodel.h"

#include <QFrame>
#include <QVBoxLayout>
#include <QLabel>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QShowEvent>
#include <QHideEvent>

class BlocknetPeersList : public BlocknetToolsPage {
    Q_OBJECT
protected:

public:
    explicit BlocknetPeersList(QWidget *, int id, QFrame *parent = nullptr);
    void setClientModel(ClientModel *c);

protected:
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;

private slots:
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

#endif // BLOCKNETPEERSLIST_H
