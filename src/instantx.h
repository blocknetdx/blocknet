
// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2012 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#ifndef INSTANTX_H
#define INSTANTX_H

#include "bignum.h"
#include "sync.h"
#include "net.h"
#include "key.h"
#include "core.h"
#include "util.h"
#include "script.h"
#include "hashblock.h"
#include "base58.h"
#include "main.h"

using namespace std;
using namespace boost;

void ProcessMessageInstantX(CNode* pfrom, std::string& strCommand, CDataStream& vRecv);


#endif