// Copyright (c) 2018-2019 The Blocknet developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BLOCKNET_QT_BLOCKNETVARS_H
#define BLOCKNET_QT_BLOCKNETVARS_H

#include <amount.h>

#include <QDateTime>
#include <QString>

#define BLOCKNETGUI_FUNDS_PRECISION 8
#define BLOCKNETGUI_FUNDS_MAX 100000000
#define BLOCKNETGUI_MAXCHARS 17 // string len(BLOCKNETGUI_FUNDS_MAX-1)*2+1, i.e. length of "99999999.99999999"

enum BlocknetPage {
    DASHBOARD = 1,
    ADDRESSBOOK,
    ACCOUNTS,
    SEND,
    QUICKSEND,
    REQUEST,
    HISTORY,
    SNODES,
    PROPOSALS,
    CREATEPROPOSAL,
    ANNOUNCEMENTS,
    SETTINGS,
    TOOLS,
};

#endif // BLOCKNET_QT_BLOCKNETVARS_H
