// Copyright (c) 2018 The Blocknet Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "blocknetaddressbtn.h"

#include <QPainter>
#include <QPixmap>

BlocknetAddressBtn::BlocknetAddressBtn(QPushButton *parent) : QPushButton(parent) {
    this->setFixedSize(40, 40);
    this->setFlat(true);
    this->setCursor(Qt::PointingHandCursor);
}

void BlocknetAddressBtn::paintEvent(QPaintEvent *) {
    QPainter p(this);
    setBackgroundRole(QPalette::Base);
    setAutoFillBackground(true);
    p.drawImage(QRect(10, 7, 20, 25), QImage(":/redesign/Active/AddressBookIcon.png"));
}
