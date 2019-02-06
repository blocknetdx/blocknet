// Copyright (c) 2018 The Blocknet Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BLOCKNETLINEEDIT_H
#define BLOCKNETLINEEDIT_H

#include <QLineEdit>

class BlocknetLineEdit : public QLineEdit
{
    Q_OBJECT
public:
    explicit BlocknetLineEdit(int w = 250, int h = 40, QLineEdit *parent = nullptr);
    void setID(QString id);
    QString getID();

signals:

public slots:

private:
    QString id;
};

#endif // BLOCKNETLINEEDIT_H
