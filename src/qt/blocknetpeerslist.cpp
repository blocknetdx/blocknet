// Copyright (c) 2018-2019 The Blocknet developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/blocknetpeerslist.h>

#include <qt/blocknetguiutil.h>

#include <qt/peertablemodel.h>

#include <QHeaderView>
#include <QMessageBox>

BlocknetPeersList::BlocknetPeersList(QWidget *, int id, QFrame *parent) : BlocknetToolsPage(id, parent), layout(new QVBoxLayout) {
    this->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    this->setLayout(layout);
    layout->setContentsMargins(0, BGU::spi(10), 0, BGU::spi(10));

    titleLbl = new QLabel(tr("Peers List"));
    titleLbl->setObjectName("h2");

    table = new QTableView;
    table->setContentsMargins(QMargins());
    table->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::ExtendedSelection);
    table->setAlternatingRowColors(true);
    table->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    table->setShowGrid(false);
    table->setContextMenuPolicy(Qt::CustomContextMenu);
    table->setSortingEnabled(true);
    table->verticalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    table->verticalHeader()->setDefaultSectionSize(BGU::spi(20));
    table->verticalHeader()->setVisible(false);
    table->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft);
    table->horizontalHeader()->setSortIndicatorShown(false);
    table->horizontalHeader()->setSectionsClickable(true);

    layout->addWidget(titleLbl, 0, Qt::AlignTop | Qt::AlignLeft);
    layout->addSpacing(BGU::spi(5));
    layout->addWidget(table);
    layout->addSpacing(BGU::spi(5));
}

void BlocknetPeersList::setClientModel(ClientModel *c) {
    if (!c)
        return;
    clientModel = c;

    table->setModel(clientModel->getPeerTableModel());
    table->horizontalHeader()->setSectionResizeMode(PeerTableModel::Address, QHeaderView::Stretch);
    table->horizontalHeader()->setSectionResizeMode(PeerTableModel::Subversion, QHeaderView::Stretch);
}

void BlocknetPeersList::showEvent(QShowEvent *event) {
    QFrame::showEvent(event);
    if (clientModel)
        clientModel->getPeerTableModel()->startAutoRefresh();
}

void BlocknetPeersList::hideEvent(QHideEvent *event) {
    QFrame::hideEvent(event);
    if (clientModel)
        clientModel->getPeerTableModel()->stopAutoRefresh();
}

void BlocknetPeersList::displayPeerDetails(const QItemSelection &, const QItemSelection &) {
    if (!table->selectionModel()) {
        return;
    }

//    if (peerDetails->isHidden()) {
//        peerDetails->setFixedSize(table->width(), 250);
//        peerDetails->move(QPoint(table->pos().x() + 46, table->pos().y() + table->height() - peerDetails->height() / 2));
//        BlocknetPeerDetails::PeerDetails detailsData = {
//            tr("N/A"),
//            tr("N/A"),
//            tr("N/A"),
//            tr("N/A"),
//            tr("N/A"),
//            tr("N/A"),
//            tr("N/A"),
//            tr("N/A"),
//            tr("N/A"),
//            tr("N/A"),
//            tr("N/A"),
//            tr("N/A")
//        };
//        peerDetails->show(detailsData);
//    }
}
