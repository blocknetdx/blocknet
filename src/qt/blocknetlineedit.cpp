// Copyright (c) 2018-2019 The Blocknet developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/blocknetlineedit.h>

BlocknetLineEdit::BlocknetLineEdit(int w, int h, QLineEdit *parent) : QLineEdit(parent) {
    this->setMinimumSize(w, h);
}

void BlocknetLineEdit::setID(const QString id) {
    this->id = id;
}

QString BlocknetLineEdit::getID() {
    return this->id;
}

