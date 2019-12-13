// Copyright (c) 2018-2019 The Blocknet developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BLOCKNET_QT_BLOCKNETFONTMGR_H
#define BLOCKNET_QT_BLOCKNETFONTMGR_H

#include <QFont>
#include <QHash>

class BlocknetFontMgr
{
public:
    BlocknetFontMgr() = default;
    enum Fonts { Roboto, RobotoMono };
    static void setup();
    static QFont getFont(Fonts font);
private:
    static QHash<Fonts, int> fonts;
};

#endif // BLOCKNET_QT_BLOCKNETFONTMGR_H
