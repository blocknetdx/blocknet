// Copyright (c) 2018 The Blocknet Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BLOCKNETDEBUGCONSOLE_H
#define BLOCKNETDEBUGCONSOLE_H

#include "blocknettools.h"
#include "blocknetpeerdetails.h"

#include <QFrame>
#include <QVBoxLayout>
#include <QLabel>
#include <QTableWidget>
#include <QTableWidgetItem>

class BlocknetDebugConsole : public BlocknetToolsPage {
    Q_OBJECT
protected:

public:
    explicit BlocknetDebugConsole(QWidget *popup, int id, QFrame *parent = nullptr);
    void setWalletModel(WalletModel *w);

private:
    QVBoxLayout *layout;
    QLabel *titleLbl;
    QWidget *popupWidget;
};

#endif // BLOCKNETDEBUGCONSOLE_H
