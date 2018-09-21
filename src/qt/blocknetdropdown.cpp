// Copyright (c) 2018 The Blocknet Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "blocknetdropdown.h"

#include <QGridLayout>
#include <QPixmap>
#include <QLabel>
#include <QAbstractItemView>
#include <QStyleFactory>
#include <QPoint>

BlocknetDropdown::BlocknetDropdown(const QStringList &list, QWidget* parent) : BlocknetDropdown(parent) {
    for (int i = 0; i < list.size(); i++)
        this->addItem(list[i]);
}

BlocknetDropdown::BlocknetDropdown(QWidget* parent) : QComboBox(parent) {
#if defined(Q_OS_MAC)
    this->setStyle(QStyleFactory::create("Windows"));
#endif
    this->setObjectName("dropdown");
    this->setFixedSize(ddW, ddH);
    auto *layout = new QGridLayout;
    QLabel *label = new QLabel;
    QImage *icon = new QImage(":/icons/accordion-arrow");
    label->setPixmap(QPixmap::fromImage(*icon));
    label->adjustSize();
    layout->addWidget(label, 0, 0, Qt::AlignRight);
    this->setLayout(layout);
    connect(this, SIGNAL(currentIndexChanged(int)), this, SLOT(handleSelectionChanged(int)));
}

QVariant BlocknetDropdown::value() const {
    return itemData(currentIndex());
}

void BlocknetDropdown::setValue(const QVariant& value) {
    setCurrentIndex(findData(value));
}

void BlocknetDropdown::handleSelectionChanged(int idx) {
    setCurrentIndex(idx);
    emit valueChanged();
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
