// Copyright (c) 2018-2019 The Blocknet developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BLOCKNET_QT_BLOCKNETLINEEDIT_H
#define BLOCKNET_QT_BLOCKNETLINEEDIT_H

#include <qt/blocknetguiutil.h>

#include <QLineEdit>

class BlocknetLineEdit : public QLineEdit
{
    Q_OBJECT
public:
    explicit BlocknetLineEdit(int w = BGU::spi(250), int h = BGU::spi(40), QLineEdit *parent = nullptr);
    void setID(QString id);
    QString getID();

Q_SIGNALS:

public Q_SLOTS:

private:
    QString id;
};

#endif // BLOCKNET_QT_BLOCKNETLINEEDIT_H
