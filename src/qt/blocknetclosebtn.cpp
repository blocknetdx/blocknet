// Copyright (c) 2018 The Blocknet Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "blocknetclosebtn.h"

#include <QPainter>
#include <QPainterPath>

BlocknetCloseBtn::BlocknetCloseBtn(qreal w, qreal h, QColor xColor, QColor fillColor, QWidget *parent) : QPushButton(parent),
                                                                     w(w), h(h), xColor(xColor), fillColor(fillColor),
                                                                     id(QString()) {
    this->setFixedSize(static_cast<int>(w), static_cast<int>(h));
    this->setCursor(Qt::PointingHandCursor);
}

void BlocknetCloseBtn::paintEvent(QPaintEvent *) {
    QPainter p(this);

    // draw circle
    QPainterPath p2;
    p2.addEllipse(0, 0, w, h);
    p.fillPath(p2, fillColor);

    // draw close X
    QPainterPath p3;
    qreal pad = w * 0.35;
    p3.moveTo(pad, pad);
    p3.lineTo(w - pad, h - pad);
    p3.moveTo(pad, h - pad);
    p3.lineTo(w - pad, pad);
    QPen pen(xColor);
    pen.setStyle(Qt::SolidLine);
    pen.setWidth(1);
    QPainterPathStroker stroke(pen);
    stroke.createStroke(p3);
    p.strokePath(p3, pen);
}
