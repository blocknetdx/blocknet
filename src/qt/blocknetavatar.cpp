// Copyright (c) 2018 The Blocknet Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "blocknetavatar.h"

#include <QPainter>

BlocknetAvatar::BlocknetAvatar(QString title, qreal w, qreal h, QColor color1,
                               QColor color2, QWidget *parent) : QWidget(parent),
                                                                title(title),
                                                                 w(w),
                                                                 h(h),
                                                                 color1(color1),
                                                                 color2(color2) {
    this->setFixedSize(static_cast<int>(w), static_cast<int>(h));
}

void BlocknetAvatar::paintEvent(QPaintEvent *) {
    QPainter p(this);
    p.setPen(Qt::NoPen);

    // draw circle
    QPainterPath p2;
    p2.addEllipse(0, 0, w, h);
    QLinearGradient grad(0, w/2, w, w/2);
    grad.setColorAt(0, color1);
    grad.setColorAt(1, color2);
    p.fillPath(p2, grad);

    // draw text
    QColor cross1(Qt::white);
    p.setPen(cross1);
    QFont font("Helvetica", 18, QFont::Light);
    p.setFont(font);
    QStringList list = title.split(" ", QString::SkipEmptyParts);
    QString s = "";
    int size = 0;
    if (list.size() > 0) {
        for (auto& c: list) {
            s.append(c.at(0).toUpper());
            size++;
        }
    }
    else if (title.size() > 0) {
        s = QString(title.at(0).toUpper());
        size = title.size();
    }
    p.drawText(w/2 - size * 5, h/1.5, s);
}
