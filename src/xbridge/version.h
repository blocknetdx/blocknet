// Copyright (c) 2017-2019 The Blocknet developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BLOCKNET_XBRIDGE_VERSION_H
#define BLOCKNET_XBRIDGE_VERSION_H

#define XBRIDGE_VERSION_MAJOR 0
#define XBRIDGE_VERSION_MINOR 72
#define XBRIDGE_VERSION_DESCR "blockdx"

#define MAKE_VERSION(major,minor) (( major << 16 ) + minor )
#define XBRIDGE_VERSION MAKE_VERSION(XBRIDGE_VERSION_MAJOR, XBRIDGE_VERSION_MINOR)

#define XBRIDGE_PROTOCOL_VERSION 0xff000042

#endif // BLOCKNET_XBRIDGE_VERSION_H

