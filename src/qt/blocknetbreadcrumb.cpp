// Copyright (c) 2018-2019 The Blocknet developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/blocknetbreadcrumb.h>

#include <qt/blocknetguiutil.h>

#include <QPainter>
#include <QPushButton>
#include <QtGlobal>

BlocknetBreadCrumb::BlocknetBreadCrumb(QFrame *parent) : QFrame(parent), layout(new QHBoxLayout) {
//    this->setStyleSheet("border: 1px solid red");
    this->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    layout->setContentsMargins(QMargins());
    layout->setAlignment(Qt::AlignBottom | Qt::AlignLeft);
    layout->setSpacing(BGU::spi(15));
    layout->setSizeConstraint(QLayout::SetFixedSize);
    this->setLayout(layout);

    group = new QButtonGroup;
    group->setExclusive(true);

    connect(group, static_cast<void(QButtonGroup::*)(int)>(&QButtonGroup::buttonClicked), this, &BlocknetBreadCrumb::goToCrumb);
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
        crumbBtn->setCheckable(true);
        crumbBtn->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Preferred);
        layout->addWidget(crumbBtn);
        group->addButton(crumbBtn, c.crumb);
    }
}

void BlocknetBreadCrumb::goToCrumb(int crumb) {
    Q_EMIT crumbChanged(crumb);
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
    this->setFixedSize(BGU::spi(6), BGU::spi(6));
}

void BlocknetArrow::paintEvent(QPaintEvent*) {
    QPainterPath p;
    p.lineTo(BGU::spi(6), BGU::spi(3));
    p.lineTo(0, BGU::spi(6));
    p.lineTo(0, 0);
    QPainter painter(this);
    QColor color; color.setNamedColor("#6F8097");
    painter.fillPath(p, color);
}

BlocknetBreadCrumb::~BlocknetBreadCrumb() = default;
