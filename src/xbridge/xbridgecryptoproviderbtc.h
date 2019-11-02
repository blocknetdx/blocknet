// Copyright (c) 2017-2019 The Blocknet developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

//******************************************************************************
//******************************************************************************

#ifndef BLOCKNET_XBRIDGE_XBRIDGECRYPTOPROVIDERBTC_H
#define BLOCKNET_XBRIDGE_XBRIDGECRYPTOPROVIDERBTC_H

#include <secp256k1.h>
#include <uint256.h>

#include <vector>

//******************************************************************************
//******************************************************************************
namespace xbridge
{

//******************************************************************************
//******************************************************************************
class BtcCryptoProvider
{
public:
    BtcCryptoProvider();
    ~BtcCryptoProvider();

public:
    bool check(const std::vector<unsigned char> & key);
    void makeNewKey(std::vector<unsigned char> & key);
    bool getPubKey(const std::vector<unsigned char> & key, std::vector<unsigned char> & pub);

    bool sign(const std::vector<unsigned char> & key,
              const uint256 & data,
              std::vector<unsigned char> & signature);
    bool verify(const std::vector<unsigned char> & pubkey,
                const uint256 & data,
                const std::vector<unsigned char> & signature);

private:
    secp256k1_context * context;
};

} // namespace xbridge

#endif // BLOCKNET_XBRIDGE_XBRIDGECRYPTOPROVIDERBTC_H
