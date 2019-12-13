// Copyright (c) 2018-2019 The Blocknet developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BLOCKNET_QT_BLOCKNETACTIONBTN_H
#define BLOCKNET_QT_BLOCKNETACTIONBTN_H

#include <QPushButton>
#include <QStyle>

class BlocknetActionBtn : public QPushButton
{
    Q_OBJECT
public:
    explicit BlocknetActionBtn(QPushButton *parent = nullptr);
    void setID(QString id) { this->id = id; }
    QString getID() { return this->id; }

protected:
    void paintEvent(QPaintEvent *event) override;

Q_SIGNALS:

public Q_SLOTS:

private:
    QString id;
    qreal s;
};

#endif // BLOCKNET_QT_BLOCKNETACTIONBTN_H
