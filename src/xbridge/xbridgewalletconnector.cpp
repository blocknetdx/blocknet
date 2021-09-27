// Copyright (c) 2017-2019 The Blocknet developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

//*****************************************************************************
//*****************************************************************************

#include <xbridge/xbridgewallet.h>

#include <base58.h>

//*****************************************************************************
//*****************************************************************************
namespace xbridge
{

//*****************************************************************************
//*****************************************************************************
namespace wallet
{

//*****************************************************************************
//*****************************************************************************
std::string UtxoEntry::toString() const
{
    std::ostringstream o;
    o << txId << ":" << vout << ":" << amount << ":" << address;
    return o.str();
}

} // namespace wallet

} // namespace xbridge
