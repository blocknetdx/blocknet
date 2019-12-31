// Copyright (c) 2018-2019 The Blocknet developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/blocknetlineeditwithtitle.h>

#include <qt/blocknetguiutil.h>

#include <QEvent>

BlocknetLineEditWithTitle::BlocknetLineEditWithTitle(QString title, QString placeholder, int w, QFrame *parent)
                                                    : QFrame(parent), layout(new QVBoxLayout)
{
    this->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Preferred);
    this->setContentsMargins(QMargins());
    layout->setContentsMargins(QMargins());
    layout->setSpacing(BGU::spi(3));
    this->setLayout(layout);

    titleLbl = new QLabel(title);
    titleLbl->setObjectName("lineEditTitle");
    titleLbl->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(titleLbl);

    lineEdit = new BlocknetLineEdit(w);
    lineEdit->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Preferred);
    lineEdit->setPlaceholderText(placeholder);
    layout->addWidget(lineEdit, 1);

    this->setFocusProxy(lineEdit);
}

QSize BlocknetLineEditWithTitle::sizeHint() const {
    return { lineEdit->width(), BGU::spi(30) + layout->spacing() + titleLbl->height() };
}

bool BlocknetLineEditWithTitle::isEmpty() {
    return lineEdit->text().trimmed().isEmpty();
}

void BlocknetLineEditWithTitle::setID(const QString id) {
    this->id = id;
}

QString BlocknetLineEditWithTitle::getID() {
    return this->id;
}

void BlocknetLineEditWithTitle::setError(bool flag) {
    lineEdit->setProperty("error", flag);
}

void BlocknetLineEditWithTitle::setTitle(const QString &title) {
    titleLbl->setText(title);
}

void BlocknetLineEditWithTitle::setExpanding() {
    this->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    this->lineEdit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
}