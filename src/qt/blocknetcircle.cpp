// Copyright (c) 2018-2019 The Blocknet developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/blocknetcircle.h>

#include <QPainter>

BlocknetCircle::BlocknetCircle(qreal w, qreal h, QColor color1,
                               QColor color2, QWidget *parent) : QWidget(parent),
                                                                 w(w),
                                                                 h(h),
                                                                 color1(color1),
                                                                 color2(color2) {
    this->setFixedSize(static_cast<int>(w), static_cast<int>(h));
}

void BlocknetCircle::paintEvent(QPaintEvent *) {
    QPainter p(this);
    p.setPen(Qt::NoPen);

    // draw circle
    QPainterPath p2;
    p2.addEllipse(0, 0, w, h);
    QLinearGradient grad(0, w/2, w, w/2);
    grad.setColorAt(0, color1);
    grad.setColorAt(1, color2);
    p.fillPath(p2, grad);
}
