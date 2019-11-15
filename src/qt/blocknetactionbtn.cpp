// Copyright (c) 2018-2019 The Blocknet developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/blocknetactionbtn.h>

#include <qt/blocknetguiutil.h>

#include <QPainter>
#include <QStyle>
#include <QStyleOption>

BlocknetActionBtn::BlocknetActionBtn(QPushButton *parent) : QPushButton(parent), id(QString()), s(BGU::spi(30)) {
    this->setFlat(true);
    this->setCursor(Qt::PointingHandCursor);
    this->setFixedSize(s, s);
}

void BlocknetActionBtn::paintEvent(QPaintEvent *) {
    QPainter p(this);
    p.setPen(Qt::NoPen);
    QStyleOptionButton option;
    option.initFrom(this);
    option.state = isDown() ? QStyle::State_Sunken : QStyle::State_Raised;

    if (option.state == QStyle::State_Sunken) {
        // draw circle
        p.setBrush(QBrush(QColor(5, 25, 55)));
        p.drawEllipse(0, 0, s, s);
        // draw line
        QColor cross1(Qt::white);
        p.setPen(cross1);
    }
    else if (this->underMouse()) {
        // draw circle
        p.setBrush(QBrush(QColor(55, 71, 92)));
        p.drawEllipse(0, 0, s, s);
        // draw line
        QColor cross1(Qt::white);
        p.setPen(cross1);
    } 
    else {
        p.setPen(QColor(116, 178, 254));
    }
    p.drawLine(s/2, s/3, s/2, s - s/3);
    p.drawLine(s/3, s/2, s - s/3, s/2);
}
