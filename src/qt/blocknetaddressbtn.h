// Copyright (c) 2018-2019 The Blocknet developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BLOCKNET_QT_BLOCKNETADDRESSBTN_H
#define BLOCKNET_QT_BLOCKNETADDRESSBTN_H

#include <QPushButton>

class BlocknetAddressBtn : public QPushButton
{
    Q_OBJECT
public:
    explicit BlocknetAddressBtn(QPushButton *parent = nullptr);
    void setID(QString id) { this->id = id; }
    QString getID() { return this->id; }

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QString id;
};

#endif // BLOCKNET_QT_BLOCKNETADDRESSBTN_H
