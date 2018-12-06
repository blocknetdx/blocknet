// Copyright (c) 2018 The Blocknet Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BLOCKNETADDRESSBTN_H
#define BLOCKNETADDRESSBTN_H

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

#endif // BLOCKNETADDRESSBTN_H
