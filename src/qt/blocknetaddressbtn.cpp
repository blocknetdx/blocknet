// Copyright (c) 2018-2019 The Blocknet developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/blocknetaddressbtn.h>

#include <qt/blocknetguiutil.h>

#include <QPainter>
#include <QPixmap>

BlocknetAddressBtn::BlocknetAddressBtn(QPushButton *parent) : QPushButton(parent) {
    this->setFixedSize(BGU::spi(40), BGU::spi(40));
    this->setFlat(true);
    this->setCursor(Qt::PointingHandCursor);
}

void BlocknetAddressBtn::paintEvent(QPaintEvent *) {
    QPainter p(this);
    setBackgroundRole(QPalette::Base);
    setAutoFillBackground(true);
    p.drawImage(QRect(BGU::spi(10), BGU::spi(7), BGU::spi(20), BGU::spi(25)), QImage(":/redesign/Active/AddressBookIcon.png"));
}
