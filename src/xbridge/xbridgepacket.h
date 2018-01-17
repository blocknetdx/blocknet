//******************************************************************************
//******************************************************************************

#ifndef XBRIDGEPACKET_H
#define XBRIDGEPACKET_H

#include "version.h"
#include "util/logger.h"

#include <vector>
#include <deque>
#include <memory>
#include <ctime>
#include <stdint.h>
#include <assert.h>
#include <string.h>

//******************************************************************************
//******************************************************************************
enum TxCancelReason
{
    crUnknown         = 0,
    crBadSettings     = 1,
    crUserRequest     = 2,
    crNoMoney         = 3,
    crBadUtxo         = 4,
    crDust            = 5,
    crRpcError        = 6,
    crNotSigned       = 7,
    crNotAccepted     = 8,
    crRollback        = 9,
    crRpcRequest      = 10,
    crXbridgeRejected = 11,
    crInvalidAddress  = 12,
    crBlocknetError   = 13,
    crBadADepositTx   = 14,
    crBadBDepositTx   = 15
};

//******************************************************************************
//******************************************************************************
enum XBridgeCommand
{
    xbcInvalid = 0,

    // client use this message for announce your addresses for network
    //
    // xbcAnnounceAddresses
    //     uint160 client address
    // xbcAnnounceAddresses = 1,

    // xchat message
    //
    // xbcXChatMessage
    //     uint160 destination address
    //     serialized p2p message from bitcoin network
    xbcXChatMessage = 2,

    //      client1                   hub          client2
    // xbcTransaction           -->    |
    // xbcTransaction           -->    |
    // xbcTransaction           -->    |
    // xbcTransaction           -->    |     <-- xbcTransaction
    //                                 |     --> xbcPendingTransaction
    //                                 |     --> xbcPendingTransaction
    //                                 |     --> xbcPendingTransaction
    //                                 |
    //                                 |     <-- xbcAcceptingTransaction
    //                                 |
    // xbcTransactionHold       <--    |     --> xbcTransactionHold
    // xbcTransactionHoldApply  -->    |     <-- xbcTransactionHoldApply
    //                                 |
    //                                 |
    // xbcTransactionInit       <--    |     --> xbcTransactionInit
    // xbcTransactionHoldInit-d -->    |     <-- xbcTransactionInitialized
    //                                 |
    // xbcTransactionCreateA    <--    |
    // xbcTransactionCreatedA   -->    |
    //                                 |
    //                                 |     --> xbcTransactionCreateB
    //                                 |     <-- xbcTransactionCreatedB
    //                                 |
    // xbcTransactionCommitA    <--    |
    // xbcTransactionCommitedA  -->    |
    //                                 |
    //                                 |     --> xbcTransactionCommitB
    //                                 |     <-- xbcTransactionCommitedB
    //                                 |


    // exchange transaction
    //
    // xbcTransaction  (148 bytes min)
    // clients not process this messages, only exchange
    //    uint256  client transaction id
    //    20 bytes source address
    //    8 bytes  source currency
    //    uint64   source amount
    //    20 bytes destination address
    //    8 bytes  destination currency
    //    uint64   destination amount
    //    uint32_t timestamp
    //
    //    array of unspent outputs used in transaction
    //      uint32_t count of array items
    //      array items
    //        uint256  transaction id
    //        uint32_t out idx
    xbcTransaction = 3,
    //
    // xbcPendingTransaction (88 bytes)
    // exchange broadcast send this message, send list of opened transactions
    //    uint256 transaction id
    //    8 bytes source currency
    //    uint64 source amount
    //    8 bytes destination currency
    //    uint64 destination amount
    //    uint160 hub address
    //    uint32_t timestamp
    xbcPendingTransaction = 4,
    //
    // xbcTransactionAccepting (164 bytes min)
    // client accepting opened tx
    //    uint160 hub address
    //    uint256 client transaction id
    //    20 bytes source address
    //    8 bytes source currency
    //    uint64 source amount
    //    20 bytes destination address
    //    8 bytes destination currency
    //    uint64 destination amount
    //    array of unspent outputs used in transaction
    //      uint32_t count of array items
    //      array items
    //        uint256  transaction id
    //        uint32_t out idx
    xbcTransactionAccepting = 5,

    //
    // xbcTransactionHold (85 or 117 bytes)
    //    uint160 hub address
    //    uint256 transaction id
    //    public key, 33 or 65 bytes, servicenode public key
    xbcTransactionHold = 6,
    //
    // xbcTransactionHoldApply (72 bytes)
    //    uint160 hub address
    //    uint160 client address
    //    uint256 hub transaction id
    xbcTransactionHoldApply = 7,

    //
    // xbcTransactionInit (188 or 221 bytes min)
    //    uint160 client address
    //    uint160 hub address
    //    uint256 hub transaction id
    //    public key, 33 or 65 bytes, servicenode public key
    //    uint16_t  role ( 'A' (Alice) or 'B' (Bob) :) )
    //    20 bytes source address
    //    8 bytes source currency
    //    uint64 source amount
    //    20 bytes source address
    //    8 bytes destination currency
    //    uint64 destination amount
    xbcTransactionInit = 8,
    //
    // xbcTransactionInitialized (137 bytes)
    //    uint160 hub address
    //    uint160 client address
    //    uint256 hub transaction id
    //    uint256 data transaction id
    //    public key, 33 bytes
    xbcTransactionInitialized = 9,

    //
    // xbcTransactionCreateA (157 bytes min)
    //    uint160  client address
    //    uint160  hub address
    //    uint256  hub transaction id
    //    20 bytes source address
    //    uint256 data tx id, 32 bytes
    //    opponent public key, 33 bytes
    xbcTransactionCreateA = 10,
    //
    // xbcTransactionCreatedA (72 bytes min)
    //    uint160 hub address
    //    uint160 client address
    //    uint256 hub transaction id
    //    string deposit tx id
    //    string inner script (TODO delete later)
    xbcTransactionCreatedA = 11,
    //
    // xbcTransactionCreateB (205 bytes min)
    //    uint160  client address
    //    uint160  hub address
    //    uint256  hub transaction id
    //    string destination address (33-34 byte + 0)
    //    string hub wallet address (33-34 byte + 0)
    //    uint32_t fee in percent, *1000 (0.3% == 300)
    //    uint256 data tx id, 32 bytes
    //    opponent public key, 33 bytes
    //    string A deposit tx id
    xbcTransactionCreateB = 12,
    //
    // xbcTransactionCreatedB (72 bytes min)
    //    uint160 hub address
    //    uint160 client address
    //    uint256 hub transaction id
    //    string deposit tx id
    //    string inner script (TODO delete later)
    xbcTransactionCreatedB = 13,

    //
    // xbcTransactionConfirmA (72 bytes min)
    //    uint160 client address
    //    uint160 hub address
    //    uint256 hub transaction id
    //    string B deposit tx id
    //    string B inner script (TODO delete later)
    xbcTransactionConfirmA = 18,
    //
    // xbcTransactionConfirmedA (72 bytes min)
    //    uint160 hub address
    //    uint160 client address
    //    uint256 hub transaction id
    //    x public key, 33 bytes
    xbcTransactionConfirmedA = 19,
    //
    // xbcTransactionConfirmB (105 bytes min)
    //    uint160 client address
    //    uint160 hub address
    //    uint256 hub transaction id
    //    x public key, 33 bytes
    //    string A deposit tx id
    //    string A inner script (TODO delete later)
    xbcTransactionConfirmB = 20,
    //
    // xbcTransactionConfirmedB (72 bytes min)
    //    uint160 hub address
    //    uint160 client address
    //    uint256 hub transaction id
    xbcTransactionConfirmedB = 21,

    //
    // xbcTransactionCancel (36 bytes)
    //    uint256  hub transaction id
    //    uint32_t reason
    xbcTransactionCancel = 22,
    //
    // xbcTransactionRollback
    //    uint256 hub transaction id
    xbcTransactionRollback = 23,
    //
    // xbcTransactionFinished
    //    uint160 client address
    //    uint256 hub transaction id
    //
    xbcTransactionFinished = 24
};

//******************************************************************************
//******************************************************************************
typedef uint32_t crc_t;

//******************************************************************************
// header 8*4 bytes
//
// boost::uint32_t version
// boost::uint32_t command
// boost::uint32_t timestamp
// boost::uint32_t size
// boost::uint32_t crc
//
// boost::uint32_t rezerved
// boost::uint32_t rezerved
// boost::uint32_t rezerved
//******************************************************************************
class XBridgePacket
{
    std::vector<unsigned char> m_body;

public:
    enum
    {
        headerSize    = 8*sizeof(uint32_t),
        commandSize   = sizeof(uint32_t),
        timestampSize = sizeof(uint32_t)
    };

    uint32_t     size()    const     { return sizeField(); }
    uint32_t     allSize() const     { return static_cast<uint32_t>(m_body.size()); }

    crc_t        crc()     const
    {
        // TODO implement this
        assert(!"not implemented");
        return 0;
        // return crcField();
    }

    uint32_t version() const       { return versionField(); }

    XBridgeCommand  command() const       { return static_cast<XBridgeCommand>(commandField()); }

    void    alloc()                       { m_body.resize(headerSize + size()); }

    const std::vector<unsigned char> & body() const
                                          { return m_body; }
    unsigned char  * header()             { return &m_body[0]; }
    unsigned char  * data()               { return &m_body[headerSize]; }

    // boost::int32_t int32Data() const { return field32<2>(); }

    void    clear()
    {
        m_body.resize(headerSize);
        commandField() = 0;
        sizeField() = 0;

        // TODO crc
        // crcField() = 0;
    }

    void resize(const uint32_t size)
    {
        m_body.resize(size+headerSize);
        sizeField() = size;
    }

    void    setData(const unsigned char data)
    {
        m_body.resize(sizeof(data) + headerSize);
        sizeField() = sizeof(data);
        m_body[headerSize] = data;
    }

    void    setData(const int32_t data)
    {
        m_body.resize(sizeof(data) + headerSize);
        sizeField() = sizeof(data);
        field32<2>() = data;
    }

    void    setData(const std::string & data)
    {
        m_body.resize(data.size() + headerSize);
        sizeField() = static_cast<uint32_t>(data.size());
        if (data.size())
        {
            data.copy((char *)(&m_body[headerSize]), data.size());
        }
    }

    void    setData(const std::vector<unsigned char> & data, const unsigned int offset = 0)
    {
        setData(&data[0], static_cast<uint32_t>(data.size()), offset);
    }

    void    setData(const unsigned char * data, const uint32_t size, const uint32_t offset = 0)
    {
        unsigned int off = offset + headerSize;
        if (size)
        {
            if (m_body.size() < size+off)
            {
                m_body.resize(size+off);
                sizeField() = size+off-headerSize;
            }
            memcpy(&m_body[off], data, size);
        }
    }

//    template<typename _T>
//    void append(const _T data)
//    {
//        m_body.reserve(m_body.size() + sizeof(data));
//        unsigned char * ptr = (unsigned char *)&data;
//        std::copy(ptr, ptr+sizeof(data), std::back_inserter(m_body));
//        sizeField() = static_cast<uint32_t>(m_body.size()) - headerSize;
//    }

    void append(const uint16_t data)
    {
        m_body.reserve(m_body.size() + sizeof(data));
        unsigned char * ptr = (unsigned char *)&data;
        std::copy(ptr, ptr+sizeof(data), std::back_inserter(m_body));
        sizeField() = static_cast<uint32_t>(m_body.size()) - headerSize;
    }

    void append(const uint32_t data)
    {
        m_body.reserve(m_body.size() + sizeof(data));
        unsigned char * ptr = (unsigned char *)&data;
        std::copy(ptr, ptr+sizeof(data), std::back_inserter(m_body));
        sizeField() = static_cast<uint32_t>(m_body.size()) - headerSize;
    }

    void append(const uint64_t data)
    {
        m_body.reserve(m_body.size() + sizeof(data));
        unsigned char * ptr = (unsigned char *)&data;
        std::copy(ptr, ptr+sizeof(data), std::back_inserter(m_body));
        sizeField() = static_cast<uint32_t>(m_body.size()) - headerSize;
    }

    void append(const unsigned char * data, const int size)
    {
        m_body.reserve(m_body.size() + size);
        std::copy(data, data+size, std::back_inserter(m_body));
        sizeField() = static_cast<uint32_t>(m_body.size()) - headerSize;
    }

    void append(const std::string & data)
    {
        m_body.reserve(m_body.size() + data.size()+1);
        std::copy(data.begin(), data.end(), std::back_inserter(m_body));
        m_body.push_back(0);
        sizeField() = static_cast<uint32_t>(m_body.size()) - headerSize;
    }

    void append(const std::vector<unsigned char> & data)
    {
        m_body.reserve(m_body.size() + data.size());
        std::copy(data.begin(), data.end(), std::back_inserter(m_body));
        sizeField() = static_cast<uint32_t>(m_body.size()) - headerSize;
    }

    bool copyFrom(const std::vector<unsigned char> & data)
    {
        m_body = data;

        if (sizeField() != static_cast<uint32_t>(data.size())-headerSize)
        {
            ERR() << "incorrect data size in XBridgePacket::copyFrom";
            assert(!"incorrect data size in XBridgePacket::copyFrom");
            return false;
        }

        // TODO check packet crc
        return true;
    }

    XBridgePacket() : m_body(headerSize, 0)
    {
        versionField()   = static_cast<uint32_t>(XBRIDGE_PROTOCOL_VERSION);
        timestampField() = static_cast<uint32_t>(time(0));
    }

    explicit XBridgePacket(const std::string& raw) : m_body(raw.begin(), raw.end())
    {
        timestampField() = static_cast<uint32_t>(time(0));
    }

    XBridgePacket(const XBridgePacket & other)
    {
        m_body = other.m_body;
    }

    XBridgePacket(XBridgeCommand c) : m_body(headerSize, 0)
    {
        versionField()   = static_cast<uint32_t>(XBRIDGE_PROTOCOL_VERSION);
        commandField()   = static_cast<uint32_t>(c);
        timestampField() = static_cast<uint32_t>(time(0));
    }

    XBridgePacket & operator = (const XBridgePacket & other)
    {
        m_body    = other.m_body;

        return *this;
    }

private:
    template<uint32_t INDEX>
    uint32_t & field32()
        { return *static_cast<uint32_t *>(static_cast<void *>(&m_body[INDEX * 4])); }

    template<uint32_t INDEX>
    uint32_t const& field32() const
        { return *static_cast<uint32_t const*>(static_cast<void const*>(&m_body[INDEX * 4])); }

    uint32_t       & versionField()         { return field32<0>(); }
    uint32_t const & versionField() const   { return field32<0>(); }
    uint32_t &       commandField()         { return field32<1>(); }
    uint32_t const & commandField() const   { return field32<1>(); }
    uint32_t &       timestampField()       { return field32<2>(); }
    uint32_t const & timestampField() const { return field32<2>(); }
    uint32_t &       sizeField()            { return field32<3>(); }
    uint32_t const & sizeField() const      { return field32<3>(); }
    uint32_t &       crcField()             { return field32<4>(); }
    uint32_t const & crcField() const       { return field32<4>(); }
};

typedef std::shared_ptr<XBridgePacket> XBridgePacketPtr;
typedef std::deque<XBridgePacketPtr>   XBridgePacketQueue;

#endif // XBRIDGEPACKET_H
