// Copyright (c) 2018 The Blocknet Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <QEvent>
#include "blocknetlineeditwithtitle.h"

BlocknetLineEditWithTitle::BlocknetLineEditWithTitle(QString title, QString placeholder, int w, int h, QFrame *parent) : QFrame(parent), layout(new QVBoxLayout) {
<<<<<<< HEAD
=======
    this->setMinimumSize(w, h);
>>>>>>> abe48f5f5... Implemented Create Proposal screens
    this->setLayout(layout);

    titleLbl = new QLabel(title);
    titleLbl->setObjectName("h1");
    layout->addWidget(titleLbl);

<<<<<<< HEAD
    lineEdit = new BlocknetLineEdit(w, h);
=======
    lineEdit = new BlocknetLineEdit;
>>>>>>> abe48f5f5... Implemented Create Proposal screens
    lineEdit->setPlaceholderText(placeholder);
    layout->addWidget(lineEdit);
}

void BlocknetLineEditWithTitle::setID(const QString id) {
    this->id = id;
}

QString BlocknetLineEditWithTitle::getID() {
    return this->id;
}

