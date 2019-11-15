// Copyright (c) 2018-2019 The Blocknet developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/blocknetavatar.h>

#include <QLabel>
#include <QPainter>

BlocknetAvatar::BlocknetAvatar(QString title, qreal w, qreal h, QColor color1,
                               QColor color2, QWidget *parent) : QFrame(parent),
                                                                title(title),
                                                                 w(w),
                                                                 h(h),
                                                                 color1(color1),
                                                                 color2(color2) {
    this->setFixedSize(w, h);
    auto *lbl = new QLabel(this);
    lbl->setObjectName("title");
    lbl->setText(title.size() < 2 ? QString() : QString(title.left(2)));
    lbl->setAlignment(Qt::AlignCenter | Qt::AlignVCenter);
    lbl->setFixedSize(w, h);
}

BlocknetAvatarBlue::BlocknetAvatarBlue(QString title, qreal w, qreal h, QColor color1,
                                       QColor color2, QWidget *parent) : BlocknetAvatar(title, w, h,
                                                                                        color1, color2, parent) { }

void BlocknetAvatar::paintEvent(QPaintEvent *) {
    QPainter p(this);
    p.setRenderHint(QPainter::HighQualityAntialiasing);
    p.setPen(Qt::NoPen);
    // draw gradient
    QPainterPath p2;
    p2.addEllipse(0, 0, w, h);
    QLinearGradient grad(0, w/2, w, w/2);
    grad.setColorAt(0, color1);
    grad.setColorAt(1, color2);
    p.fillPath(p2, grad);
}
