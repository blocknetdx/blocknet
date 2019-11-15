// Copyright (c) 2018-2019 The Blocknet developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/blocknetformbtn.h>

#include <qt/blocknetguiutil.h>

BlocknetFormBtn::BlocknetFormBtn(QPushButton *parent) : QPushButton(parent) {
    this->setFixedSize(BGU::spi(160), BGU::spi(40));
    this->setFlat(true);
    this->setCursor(Qt::PointingHandCursor);
}
