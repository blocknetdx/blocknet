// Copyright (c) 2017-2019 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

// https://github.com/Bitcoin-ABC/bitcoin-abc/blob/f40d38e63e1c8a39a9d4a143ec53999e2f174dce/src/cashaddrenc.h#L4
#ifndef BLOCKNET_XBRIDGE_CASHADDR_CASHADDRENC_H
#define BLOCKNET_XBRIDGE_CASHADDR_CASHADDRENC_H

#include <script/standard.h>

#include <string>
#include <vector>

struct CashParams {
    std::string prefix;
    std::string CashAddrPrefix() const { return prefix; }
};

struct PKHash : public uint160 {
    PKHash() : uint160() {}
    explicit PKHash(const uint160 &hash) : uint160(hash) {}
    explicit PKHash(const CPubKey &pubkey);
    using uint160::uint160;
};

struct ScriptHash : public uint160 {
    ScriptHash() : uint160() {}
    explicit ScriptHash(const uint160 &hash) : uint160(hash) {}
    explicit ScriptHash(const CScript &script);
    using uint160::uint160;
};

typedef boost::variant<CNoDestination, PKHash, ScriptHash> CashTxDestination;

enum CashAddrType : uint8_t { PUBKEY_TYPE = 0, SCRIPT_TYPE = 1 };

struct CashAddrContent {
    CashAddrType type;
    std::vector<uint8_t> hash;
};

template <class T>
std::vector<uint8_t> PackAddrData(const T &id, uint8_t type);

std::string EncodeCashAddr(const CashTxDestination &, const CashParams &);
std::string EncodeCashAddr(const std::string &prefix,
                           const CashAddrContent &content);

CashTxDestination DecodeCashAddr(const std::string &addr,
                              const CashParams &params);
CashAddrContent DecodeCashAddrContent(const std::string &addr,
                                      const std::string &prefix);
CashTxDestination DecodeCashAddrDestination(const CashAddrContent &content);

std::vector<uint8_t> PackCashAddrContent(const CashAddrContent &content);

bool IsValidCashDestination(const CashTxDestination& dest);
#endif // BLOCKNET_XBRIDGE_CASHADDR_CASHADDRENC_H
