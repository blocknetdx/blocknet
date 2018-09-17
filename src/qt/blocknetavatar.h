// Copyright (c) 2018 The Blocknet Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BLOCKNETAVATAR_H
#define BLOCKNETAVATAR_H

#include <QWidget>

class BlocknetAvatar : public QWidget
{
    Q_OBJECT
protected:
    void paintEvent(QPaintEvent *event) override;

public:
    explicit BlocknetAvatar(QString title = "",
                            qreal w = 40, qreal h = 40,
                            QColor color1 = QColor(0xFB, 0x7F, 0x70), QColor color2 = QColor(0xF6, 0x50, 0x8A),
                            QWidget *parent = nullptr);

signals:

public slots:

private:
    QString title;
    QColor color1;
    QColor color2;
    qreal w;
    qreal h;
};

#endif // BLOCKNETAVATAR_H