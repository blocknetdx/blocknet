// Copyright (c) 2018-2019 The Blocknet developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/blocknettabbar.h>

#include <qt/blocknetguiutil.h>
#include <qt/blocknethdiv.h>
#include <qt/blocknettabbtn.h>

#include <QPainter>
#include <QPushButton>

BlocknetTabBar::BlocknetTabBar(QFrame *parent) : QFrame(parent), mainLayout(new QVBoxLayout), layout(new QHBoxLayout) {
    this->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Preferred);
    this->setFixedHeight(BGU::spi(60));
    mainLayout->setContentsMargins(QMargins());
    mainLayout->setSpacing(0);
    this->setLayout(mainLayout);

    auto *tabFrame = new QFrame;
    tabFrame->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    layout->setContentsMargins(QMargins());
    layout->setAlignment(Qt::AlignBottom);
    tabFrame->setLayout(layout);

    group = new QButtonGroup;
    group->setExclusive(true);

    auto *hdiv = new BlocknetHDiv;

    mainLayout->addWidget(tabFrame);
    mainLayout->addWidget(hdiv, 0, Qt::AlignTop);

    connect(group, static_cast<void(QButtonGroup::*)(int)>(&QButtonGroup::buttonClicked), this, &BlocknetTabBar::goToTab);
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
        layout->addWidget(tabBtn);
        group->addButton(tabBtn, c.tab);
    }
}

void BlocknetTabBar::goToTab(int tab){
    Q_EMIT tabChanged(tab);
}

bool BlocknetTabBar::showTab(int tab) {
    currentTab = tab;
    QAbstractButton *btn = group->button(tab);
    if (btn)
        btn->setChecked(true);
    return true;
}

BlocknetTabBar::~BlocknetTabBar() = default;
