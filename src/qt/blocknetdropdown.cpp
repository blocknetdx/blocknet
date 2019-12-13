// Copyright (c) 2018-2019 The Blocknet developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/blocknetdropdown.h>

#include <qt/blocknetguiutil.h>

#include <QAbstractItemView>
#include <QGridLayout>
#include <QLabel>
#include <QPixmap>
#include <QPoint>
#include <QStyleFactory>

BlocknetDropdown::BlocknetDropdown(const QStringList &list, QWidget* parent) : BlocknetDropdown(parent) {
    for (int i = 0; i < list.size(); i++)
        this->addItem(list[i]);
}

BlocknetDropdown::BlocknetDropdown(QWidget* parent) : QComboBox(parent), ddW(BGU::spi(180)), ddH(BGU::spi(40)) {
#if defined(Q_OS_MAC)
    this->setStyle(QStyleFactory::create("Windows"));
#endif
    this->setObjectName("dropdown");
    this->setFixedSize(ddW, ddH);
    auto *layout = new QGridLayout;
    auto *label = new QLabel;
    auto *icon = new QImage(":/icons/accordion-arrow");
    label->setPixmap(QPixmap::fromImage(*icon));
    label->adjustSize();
    layout->addWidget(label, 0, 0, Qt::AlignRight);
    this->setLayout(layout);
    connect(this, static_cast<void(QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this, &BlocknetDropdown::handleSelectionChanged);
}

QVariant BlocknetDropdown::value() const {
    return itemData(currentIndex());
}

void BlocknetDropdown::setValue(const QVariant& value) {
    setCurrentIndex(findData(value));
}

void BlocknetDropdown::handleSelectionChanged(int idx) {
    setCurrentIndex(idx);
    Q_EMIT valueChanged();
}

void BlocknetDropdown::showPopup() {
    QComboBox::showPopup();
    QList<QFrame *> widgets = this->findChildren<QFrame*>();
    QWidget *popup = widgets[1];
    popup->setMinimumWidth(ddW);
}

void BlocknetDropdown::wheelEvent(QWheelEvent *e) {
    e->ignore();
}
