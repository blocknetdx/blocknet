// Copyright (c) 2018 The Blocknet Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BLOCKNETLABELBTN_H
#define BLOCKNETLABELBTN_H

#include <QPushButton>

class BlocknetLabelBtn : public QPushButton
{
    Q_OBJECT
public:
    explicit BlocknetLabelBtn(QPushButton *parent = nullptr);
    void setID(QString id) { this->id = id; }
    QString getID() { return this->id; }
signals:

public slots:

private:
    QString id;
};

#endif // BLOCKNETLABELBTN_H
