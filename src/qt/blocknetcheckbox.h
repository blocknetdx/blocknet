// Copyright (c) 2019 The Blocknet developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BLOCKNET_QT_BLOCKNETCHECKBOX_H
#define BLOCKNET_QT_BLOCKNETCHECKBOX_H

#include <QCheckBox>

class BlocknetCheckBox : public QCheckBox {
    Q_OBJECT
public:
    explicit BlocknetCheckBox(const QString & text, QWidget* parent = nullptr);
    explicit BlocknetCheckBox(QWidget* parent = nullptr);
};


#endif //BLOCKNET_QT_BLOCKNETCHECKBOX_H
