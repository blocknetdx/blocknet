// Copyright (c) 2018 The Blocknet Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BLOCKNETLINEEDITWITHTITLE_H
#define BLOCKNETLINEEDITWITHTITLE_H

#include "blocknetlineedit.h"

#include <QFrame>
#include <QLabel>
#include <QBoxLayout>

class BlocknetLineEditWithTitle : public QFrame
{
    Q_OBJECT
public:
    explicit BlocknetLineEditWithTitle(QString title = "", QString placeholder = "", int w = 250, int h = 40, QFrame *parent = nullptr);
    void setID(QString id);
    QString getID();
    bool isEmpty();
    BlocknetLineEdit *lineEdit;

signals:

public slots:

private:
    QString id;
    QLabel *titleLbl;
    QVBoxLayout *layout;
};

#endif // BLOCKNETLINEEDITWITHTITLE_H
