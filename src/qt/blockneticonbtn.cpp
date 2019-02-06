// Copyright (c) 2018 The Blocknet Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "blockneticonbtn.h"

#include <QPainter>
#include <QMouseEvent>

BlocknetIconBtn::BlocknetIconBtn(const QString &title, const QString &img, QFrame *parent) : QFrame(parent),
                                                                                             circlew(84),
                                                                                             circleh(84),
                                                                                             hoverState(false) {
//    this->setStyleSheet("border: 1px solid red");
    const int w = 120;
    const int h = title.isEmpty() ? 90 : 130;
    this->setCursor(Qt::PointingHandCursor);
    this->setFixedSize(w, h);

    QPixmap pm(img);
    pm.setDevicePixelRatio(2); // TODO HDPI

    auto *icon = new QLabel(this);
    icon->setFixedSize(pm.width()/2, pm.height()/2);
    icon->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
    icon->setFixedSize(QSize(icon->width(), icon->height()));
    icon->setPixmap(pm.scaled(icon->width()*pm.devicePixelRatio(), icon->height()*pm.devicePixelRatio(),
                              Qt::KeepAspectRatio, Qt::SmoothTransformation));
    icon->move(w/2 - icon->width()/2, circleh/2 - icon->height()/2);
    icon->show();

    iconLbl = new QLabel(title, this);
    iconLbl->setObjectName("title");
    iconLbl->setAlignment(Qt::AlignCenter);
    iconLbl->setWordWrap(true);
    iconLbl->setFixedWidth(circlew);
    iconLbl->move(w/2 - iconLbl->width()/2, circleh + 5);
    iconLbl->show();
}

QSize BlocknetIconBtn::sizeHint() const {
    return { circlew + 1, circleh + 15 + iconLbl->height() + 1 };
}

void BlocknetIconBtn::paintEvent(QPaintEvent *event) {
    QFrame::paintEvent(event);

    const int linew = 2;
    const int linew2 = linew/2;
    auto w = static_cast<qreal>(this->width());
    auto cw = static_cast<qreal>(circlew);
    auto ch = static_cast<qreal>(circleh);

    QPainter p(this);
    p.setRenderHint(QPainter::HighQualityAntialiasing);
    QPen pen(QColor(0x74, 0xB2, 0xFE), linew, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    p.setPen(pen);

    QPainterPath path;
    path.addEllipse(w/2 - cw/2 + linew2, linew2,
            cw - linew2, ch - linew2);

    if (hoverState)
        p.fillPath(path, QColor(0x01, 0x6A, 0xFF));

    p.drawPath(path);
}

void BlocknetIconBtn::mouseReleaseEvent(QMouseEvent *event) {
    QWidget::mouseReleaseEvent(event);
    QRect rect(0, 0, this->width(), this->height());
    if (rect.intersects({static_cast<int>(event->localPos().x()), static_cast<int>(event->localPos().y()), 1, 1 }))
        emit clicked();
}
