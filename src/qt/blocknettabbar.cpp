// Copyright (c) 2018 The Blocknet Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "blocknettabbar.h"

#include "blocknethdiv.h"
#include "blocknettabbtn.h"
#include "blocknetlabelbtn.h"

#include <QPainter>
#include <QPushButton>
#include <QDebug>

BlocknetTabBar::BlocknetTabBar(QFrame *parent) : QFrame(parent), mainLayout(new QVBoxLayout), layout(new QHBoxLayout) {
    this->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Preferred);
    this->setFixedHeight(60);
    mainLayout->setContentsMargins(QMargins());
    mainLayout->setSpacing(0);
    this->setLayout(mainLayout);

    auto *tabFrame = new QFrame;
    layout->setContentsMargins(QMargins());
    layout->setAlignment(Qt::AlignBottom | Qt::AlignLeft);
    layout->setSpacing(15);
    tabFrame->setLayout(layout);

    group = new QButtonGroup;
    group->setExclusive(true);

    auto *hdiv = new BlocknetHDiv;

    mainLayout->addWidget(tabFrame, 0, Qt::AlignBottom);
    mainLayout->addWidget(hdiv, 0, Qt::AlignTop);

    connect(group, SIGNAL(buttonClicked(int)), this, SLOT(goToTab(int)));
}

/**
 * Add a button. Buttons are disabled by default. Use goToTab to activate a button.
 * @param title
 * @param tab
 */
void BlocknetTabBar::addTab(QString title, int tab) {
    tabs.append({ tab, std::move(title)});

    // Remove buttons from group first
    for (QAbstractButton *btn : group->buttons())
        group->removeButton(btn);

    // Clear existing buttons
    QLayoutItem *item;
    while ((item = layout->takeAt(0)) != nullptr)
        item->widget()->deleteLater();

    for (int i = 0; i < tabs.size(); ++i) {
        BlocknetTab &c = tabs[i];
        auto *tabBtn = new BlocknetTabBtn;
        tabBtn->setText(c.title);
        layout->addWidget(tabBtn, 0, Qt::AlignBottom);
        group->addButton(tabBtn, c.tab);
    }

    this->adjustSize();
}

void BlocknetTabBar::goToTab(int tab){
    emit tabChanged(tab);
}

bool BlocknetTabBar::showTab(int tab) {
    currentTab = tab;
    QAbstractButton *btn = group->button(tab);
    if (btn)
        btn->setChecked(true);
    return true;
}

QSize BlocknetTabBar::sizeHint() const {
    QSize r;
    for (int i = 0; i < layout->count(); ++i) {
        QWidget *w = layout->itemAt(i)->widget();
        int maxX = w->pos().x() + w->width();
        int maxY = w->pos().y() + w->height() + 3;
        if (r.width() < maxX && maxX > 0)
            r.setWidth(maxX);
        if (r.height() < maxY && maxY > 0)
            r.setHeight(maxY);
    }
    return r;
}

BlocknetTabBar::~BlocknetTabBar() = default;
