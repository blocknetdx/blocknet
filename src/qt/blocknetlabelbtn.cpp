// Copyright (c) 2018 The Blocknet Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "blocknetlabelbtn.h"

BlocknetLabelBtn::BlocknetLabelBtn(QPushButton *parent) : QPushButton(parent) {
    this->setFlat(true);
    this->setCursor(Qt::PointingHandCursor);
}
