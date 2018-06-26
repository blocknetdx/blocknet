// Copyright (c) 2018 The Blocknet Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "blocknetformbtn.h"

BlocknetFormBtn::BlocknetFormBtn(QPushButton *parent) : QPushButton(parent) {
    this->setFixedSize(160, 40);
    this->setFlat(true);
    this->setCursor(Qt::PointingHandCursor);
}
