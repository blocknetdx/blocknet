// Copyright (c) 2018 The Blocknet Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "blockneticonaltbtn.h"

#include <QPainter>
#include <QMouseEvent>

BlocknetIconAltBtn::BlocknetIconAltBtn(const QString &img, int padding, QFrame *parent) : QFrame(parent), layout(new QVBoxLayout) {
    this->setCursor(Qt::PointingHandCursor);
    this->setFixedSize(86, 86);
    this->setLayout(layout);

    QPixmap pm(img);
    pm.setDevicePixelRatio(2); // TODO HDPI

    auto *icon = new QLabel();
    icon->setFixedSize(pm.width()/2, pm.height()/2);
    icon->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
    icon->setFixedSize(QSize(icon->width() + 5 - padding, icon->height()));
    icon->setPixmap(pm.scaled(icon->width()*pm.devicePixelRatio(), icon->height()*pm.devicePixelRatio(),
                              Qt::KeepAspectRatio, Qt::SmoothTransformation));

    layout->setContentsMargins(0, (circleh - padding)/2 - icon->height(), circlew/2 - icon->width(), 0);
    layout->addWidget(icon, 0, Qt::AlignCenter | Qt::AlignVCenter);
}

QSize BlocknetIconAltBtn::sizeHint() const {
    return { circlew + 1, circleh + 1 };
}

void BlocknetIconAltBtn::paintEvent(QPaintEvent *event) {
    QFrame::paintEvent(event);

    const int linew = 2;
    const int linew2 = linew/2;

    QPainter p(this);
    p.setRenderHint(QPainter::HighQualityAntialiasing);
    QPen pen(QColor(0x74, 0xB2, 0xFE), linew, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    p.setPen(pen);

    QPainterPath path;
    path.addEllipse(linew2, linew2, circlew - linew2, circleh - linew2);

    if (hoverState)
        p.fillPath(path, QColor(0x01, 0x6A, 0xFF));

    p.drawPath(path);
}

void BlocknetIconAltBtn::mouseReleaseEvent(QMouseEvent *event) {
    QWidget::mouseReleaseEvent(event);
    QRect rect(0, 0, this->width(), this->height());
    if (rect.intersects({static_cast<int>(event->localPos().x()), static_cast<int>(event->localPos().y()), 1, 1 }))
        emit clicked();
}
