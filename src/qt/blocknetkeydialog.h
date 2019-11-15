// Copyright (c) 2018-2019 The Blocknet developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BLOCKNETKEYDIALOG_H
#define BLOCKNETKEYDIALOG_H

#include <qt/blocknetformbtn.h>
#include <qt/blocknetclosebtn.h>

#include <QBoxLayout>
#include <QDialog>
#include <QLabel>

class BlocknetKeyDialog : public QDialog
{
    Q_OBJECT
public:
    explicit BlocknetKeyDialog(QString highlight = "", QString note = "", QString action = "Add to Address Book", QString title = "Your Decrypted Key", QWidget *parent = nullptr, Qt::WindowFlags f = Qt::CustomizeWindowHint);

private:
    QLabel *titleLbl;
    QLabel *highlightLbl;
    QLabel *noteLbl;
    BlocknetCloseBtn *cancelBtn;
    BlocknetFormBtn *okBtn;
    QVBoxLayout *layout;
};

#endif // BLOCKNETKEYDIALOG_H
