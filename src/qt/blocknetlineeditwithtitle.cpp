// Copyright (c) 2018 The Blocknet Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <QEvent>
#include "blocknetlineeditwithtitle.h"

BlocknetLineEditWithTitle::BlocknetLineEditWithTitle(QString title, QString placeholder, int w, int h, QFrame *parent) : QFrame(parent), layout(new QVBoxLayout) {
    this->setMinimumSize(w, h);
    this->setContentsMargins(QMargins());
    layout->setContentsMargins(QMargins());
    layout->setSpacing(3);
    this->setLayout(layout);

    titleLbl = new QLabel(title);
    titleLbl->setObjectName("h1");
    titleLbl->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(titleLbl);

    lineEdit = new BlocknetLineEdit(w, h - 2);
    lineEdit->setPlaceholderText(placeholder);
    layout->addWidget(lineEdit);

    this->setFocusProxy(lineEdit);
}

bool BlocknetLineEditWithTitle::isEmpty() {
    return lineEdit->text().trimmed().isEmpty();
}

void BlocknetLineEditWithTitle::setTitle(const QString &title) {
    titleLbl->setText(title);
}

void BlocknetLineEditWithTitle::setID(const QString id) {
    this->id = id;
}

QString BlocknetLineEditWithTitle::getID() {
    return this->id;
}
