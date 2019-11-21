// Copyright (c) 2019 The Blocknet developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/blocknetcheckbox.h>
#include <qt/blocknetguiutil.h>

BlocknetCheckBox::BlocknetCheckBox(const QString & text, QWidget *parent) : QCheckBox(text, parent) {
    this->setContentsMargins(QMargins());
#ifndef Q_OS_MAC
    this->setStyleSheet("BlocknetCheckBox::indicator:checked { image: url(:/redesign/checkmark-on); background-color: #016AFF; border: 1px solid #cccccc; } "
            "BlocknetCheckBox::indicator:unchecked { image: url(:/redesign/checkmark-off); background-color: #016AFF; border: 1px solid #cccccc; }");
#endif
}

BlocknetCheckBox::BlocknetCheckBox(QWidget *parent) : BlocknetCheckBox(QString(), parent) { }
