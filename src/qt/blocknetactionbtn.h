// Copyright (c) 2018 The Blocknet Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BLOCKNETACTIONBTN_H
#define BLOCKNETACTIONBTN_H

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

signals:

public slots:

private:
    QString id;
    qreal s = 30;
};

#endif // BLOCKNETACTIONBTN_H
