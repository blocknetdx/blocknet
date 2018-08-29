// Copyright (c) 2018 The Blocknet Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BLOCKNETDIALOG_H
#define BLOCKNETDIALOG_H

#include <QDialog>
#include <QTextEdit>
#include <QBoxLayout>

#include "blocknetformbtn.h"

class BlocknetDialog : public QDialog
{
    Q_OBJECT
public:
    explicit BlocknetDialog(QString message = "", QString actionMsg = "Delete", QWidget *parent = nullptr, Qt::WindowFlags f = Qt::CustomizeWindowHint);

private:
    QTextEdit *messageLbl;
    BlocknetFormBtn *cancelBtn;
    BlocknetFormBtn *deleteBtn;
    QVBoxLayout *layout;
};

#endif // BLOCKNETDIALOG_H
