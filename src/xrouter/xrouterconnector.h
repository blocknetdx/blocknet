// Copyright (c) 2018-2020 The Blocknet developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BLOCKNET_XROUTER_XROUTERCONNECTOR_H
#define BLOCKNET_XROUTER_XROUTERCONNECTOR_H

#include <xrouter/xrouterutils.h>

#include <cstdint>
#include <string>
#include <vector>

namespace xrouter
{

struct UtxoEntry
{
    std::string txId;
    uint32_t vout;
    double amount{0};
    std::string address;
    std::string scriptPubKey;
    uint32_t confirmations{0};
    bool hasConfirmations{false};

    std::vector<unsigned char> rawAddress;
    std::vector<unsigned char> signature;

    std::string toString() const {
        std::ostringstream o;
        o << txId << ":" << vout << ":" << amount << ":" << address;
        return o.str();
    }

    bool operator < (const UtxoEntry & r) const {
        return (txId < r.txId) || ((txId == r.txId) && (vout < r.vout));
    }

    bool operator == (const UtxoEntry & r) const {
        return (txId == r.txId) && (vout ==r.vout);
    }

    void setConfirmations(const uint32_t confs) {
        confirmations = confs;
        hasConfirmations = true;
    }
};

class WalletParam
{
public:
    WalletParam()
        : txVersion(1)
        , COIN(0)
        , minTxFee(0)
        , feePerByte(0)
        , dustAmount(0)
        , blockTime(0)
        , blockSize(1024)
        , requiredConfirmations(0)
        , serviceNodeFee(.015)
        , txWithTimeField(false)
        , isLockCoinsSupported(false)
    {
        addrPrefix.resize(1, '\0');
        scriptPrefix.resize(1, '\0');
        secretPrefix.resize(1, '\0');
    }

    WalletParam & operator = (const WalletParam & other)
    {
        title                   = other.title;
        currency                = other.currency;
        address                 = other.address;

        m_ip                    = other.m_ip;
        m_port                  = other.m_port;
        m_user                  = other.m_user;
        m_passwd                = other.m_passwd;

        addrPrefix              = other.addrPrefix;
        scriptPrefix            = other.scriptPrefix;
        secretPrefix            = other.secretPrefix;

        txVersion               = other.txVersion;
        COIN                    = other.COIN;
        minTxFee                = other.minTxFee;
        feePerByte              = other.feePerByte;
        dustAmount              = other.dustAmount;
        method                  = other.method;
        blockTime               = other.blockTime;
        blockSize               = other.blockSize;
        requiredConfirmations   = other.requiredConfirmations;
        txWithTimeField         = other.txWithTimeField;
        isLockCoinsSupported    = other.isLockCoinsSupported;
        jsonver                 = other.jsonver;
        contenttype             = other.contenttype;

        return *this;
    }

// TODO temporary public
public:
    std::string                  title;
    std::string                  currency;

    std::string                  address;

    std::string                  m_ip;
    std::string                  m_port;
    std::string                  m_user;
    std::string                  m_passwd;

    std::string                  addrPrefix;
    std::string                  scriptPrefix;
    std::string                  secretPrefix;
    uint32_t                     txVersion;
    uint64_t                     COIN;
    uint64_t                     minTxFee;
    uint64_t                     feePerByte;
    uint64_t                     dustAmount;
    std::string                  method;

    // block time in seconds
    uint32_t                     blockTime;

    // block size in megabytes
    uint32_t                     blockSize;

    // required confirmations for tx
    uint32_t                     requiredConfirmations;

    //service node fee, see rpc::createFeeTransaction
    const double                 serviceNodeFee;

    // serialized transaction contains time field (default not)
    bool                         txWithTimeField;

    // support for lock/unlock coins (default off)
    bool                         isLockCoinsSupported;
    mutable CCriticalSection     lockedCoinsLocker;
    std::set<UtxoEntry>          lockedCoins;

    // json version for use with rpc
    std::string                  jsonver;
    // content type
    std::string                  contenttype;
};

class WalletConnectorXRouter : public WalletParam
{
public:
    WalletConnectorXRouter() = default;

    WalletConnectorXRouter & operator = (const WalletParam & other)
    {
        *(WalletParam *)this = other;
        return *this;
    }

    virtual std::string              getBlockCount() const = 0;
    virtual std::string              getBlockHash(const int & block) const = 0;
    virtual std::string              getBlock(const std::string & blockHash) const = 0;
    virtual std::vector<std::string> getBlocks(const std::vector<std::string> & blockHashes) const = 0;
    virtual std::string              getTransaction(const std::string & hash) const = 0;
    virtual std::vector<std::string> getTransactions(const std::vector<std::string> & txHashes) const = 0;
    virtual std::vector<std::string> getTransactionsBloomFilter(const int & number, CDataStream & stream, const int & fetchlimit=0) const = 0;
    virtual std::string              sendTransaction(const std::string & transaction) const = 0;
    virtual std::string              decodeRawTransaction(const std::string & hex) const = 0;
    virtual std::string              convertTimeToBlockCount(const std::string & timestamp) const = 0;
    virtual std::string              getBalance(const std::string & address) const = 0;
};

} // namespace xrouter


#endif
