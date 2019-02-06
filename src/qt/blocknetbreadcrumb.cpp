// Copyright (c) 2018 The Blocknet Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "blocknetbreadcrumb.h"

#include <QPainter>
#include <QPushButton>
#include <QDebug>

BlocknetBreadCrumb::BlocknetBreadCrumb(QFrame *parent) : QFrame(parent), layout(new QHBoxLayout) {
    this->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Preferred);
    layout->setContentsMargins(QMargins());
    layout->setAlignment(Qt::AlignBottom | Qt::AlignLeft);
    layout->setSpacing(15);
    this->setLayout(layout);

    group = new QButtonGroup;
    group->setExclusive(true);

    connect(group, SIGNAL(buttonClicked(int)), this, SLOT(goToCrumb(int)));
}

/**
 * Add a button. Buttons are disabled by default. Use goToCrumb to activate a button.
 * @param title
 * @param crumb
 */
void BlocknetBreadCrumb::addCrumb(QString title, int crumb) {
    crumbs.append({ crumb, std::move(title)});

    // Remove buttons from group first
    for (QAbstractButton *btn : group->buttons())
        group->removeButton(btn);

    // Clear existing buttons
    QLayoutItem *item;
    while ((item = layout->takeAt(0)) != nullptr)
        item->widget()->deleteLater();

    for (int i = 0; i < crumbs.size(); ++i) {
        BlocknetCrumb &c = crumbs[i];
        if (i > 0) {
            auto *arrow = new BlocknetArrow;
            layout->addWidget(arrow);
        }

        auto *crumbBtn = new QPushButton(c.title);
        crumbBtn->setObjectName("crumb");
        crumbBtn->setFlat(true);
        crumbBtn->setCursor(Qt::PointingHandCursor);
        crumbBtn->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Preferred);
        crumbBtn->setMaximumHeight(40);
        crumbBtn->setCheckable(true);
        layout->addWidget(crumbBtn);
        group->addButton(crumbBtn, c.crumb);
    }

    this->adjustSize();
}

void BlocknetBreadCrumb::goToCrumb(int crumb){
    emit crumbChanged(crumb);
}

bool BlocknetBreadCrumb::showCrumb(int crumb) {
    currentCrumb = crumb;
    QAbstractButton *btn = group->button(crumb);
    if (btn)
        btn->setChecked(true);
    return true;
}

QSize BlocknetBreadCrumb::sizeHint() const {
    QSize r;
    for (int i = 0; i < layout->count(); ++i) {
        QWidget *w = layout->itemAt(i)->widget();
        int maxX = w->pos().x() + w->width();
        int maxY = w->pos().y() + w->height();
        if (r.width() < maxX && maxX > 0)
            r.setWidth(maxX);
        if (r.height() < maxY && maxY > 0)
            r.setHeight(maxY);
    }
    return r;
}

BlocknetArrow::BlocknetArrow(QWidget*) {
    this->setFixedSize(6, 6);
}

void BlocknetArrow::paintEvent(QPaintEvent*) {
    QPainterPath p;
    p.lineTo(6, 3);
    p.lineTo(0, 6);
    p.lineTo(0, 0);
    QPainter painter(this);
    QColor color; color.setNamedColor("#6F8097");
    painter.fillPath(p, color);
}

BlocknetBreadCrumb::~BlocknetBreadCrumb() = default;
