// Copyright (c) 2018 The Blocknet Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BLOCKNETFONTMGR_H
#define BLOCKNETFONTMGR_H

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

#endif // BLOCKNETFONTMGR_H
