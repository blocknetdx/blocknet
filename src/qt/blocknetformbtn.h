// Copyright (c) 2018-2019 The Blocknet developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BLOCKNETFORMBTN_H
#define BLOCKNETFORMBTN_H

#include <QPushButton>

class BlocknetFormBtn : public QPushButton
{
    Q_OBJECT
public:
    explicit BlocknetFormBtn(QPushButton *parent = nullptr);
    void setID(QString id) { this->id = id; }
    QString getID() { return this->id; }
Q_SIGNALS:

public Q_SLOTS:

private:
    QString id;
};

#endif // BLOCKNETFORMBTN_H
