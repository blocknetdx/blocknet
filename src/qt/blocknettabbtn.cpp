// Copyright (c) 2018-2019 The Blocknet developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/blocknettabbtn.h>

#include <qt/blocknetguiutil.h>

#include <QPainter>
#include <QStyle>
#include <QStyleOption>

BlocknetTabBtn::BlocknetTabBtn(QPushButton *parent) : QPushButton(parent), layout(new QVBoxLayout), subLine(new QLabel) {
    this->setObjectName("tab");
    this->setFlat(true);
    this->setCursor(Qt::PointingHandCursor);
    this->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Preferred);
    this->setFixedHeight(BGU::spi(50));
    this->setCheckable(true);
    this->setLayout(layout);
    layout->setContentsMargins(QMargins());

    subLine->setObjectName("subLine");
    subLine->setFixedHeight(BGU::spi(3));
    subLine->hide();

    layout->addWidget(subLine, 1, Qt::AlignBottom);
}

bool BlocknetTabBtn::event(QEvent *event) {
    if (isChecked()) {
        subLine->show();
    } else {
        subLine->hide();
    }
    return QPushButton::event(event);
}
