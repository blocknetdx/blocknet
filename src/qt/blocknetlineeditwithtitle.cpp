// Copyright (c) 2018 The Blocknet Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <QEvent>
#include "blocknetlineeditwithtitle.h"

BlocknetLineEditWithTitle::BlocknetLineEditWithTitle(QString title, QString placeholder, int w, int h, QFrame *parent) : QFrame(parent), layout(new QVBoxLayout) {
    this->setMinimumSize(w, h);
    this->setLayout(layout);

    titleLbl = new QLabel(title);
    titleLbl->setObjectName("h1");
    layout->addWidget(titleLbl);

    lineEdit = new BlocknetLineEdit;
    lineEdit->setPlaceholderText(placeholder);
    layout->addWidget(lineEdit);
}

void BlocknetLineEditWithTitle::setID(const QString id) {
    this->id = id;
}

QString BlocknetLineEditWithTitle::getID() {
    return this->id;
}

