// Copyright (c) 2018-2019 The Blocknet developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BLOCKNETHDIV_H
#define BLOCKNETHDIV_H

#include <QLabel>

class BlocknetHDiv : public QLabel
{
    Q_OBJECT
public:
    explicit BlocknetHDiv(QLabel *parent = nullptr);

Q_SIGNALS:

public Q_SLOTS:
};

#endif // BLOCKNETHDIV_H
