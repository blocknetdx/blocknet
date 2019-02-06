// Copyright (c) 2018 The Blocknet Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BLOCKNETCLOSEBTN_H
#define BLOCKNETCLOSEBTN_H

#include <QWidget>
#include <QPushButton>
#include <QPaintEvent>

class BlocknetCloseBtn : public QPushButton {
    Q_OBJECT

public:
    explicit BlocknetCloseBtn(qreal w = 25, qreal h = 25, QColor xColor = QColor(0xFF, 0xFF, 0xFF),
            QColor fillColor = QColor(0x37, 0x47, 0x5C), QWidget *parent = nullptr);
    void setID(const QString id) {
        this->id = id;
    }
    QString getID() {
        return this->id;
    }

protected:
    void paintEvent(QPaintEvent *event) override;

signals:

public slots:

private:
    qreal w;
    qreal h;
    QColor xColor;
    QColor fillColor;
    QString id;
};

#endif // BLOCKNETCLOSEBTN_H
