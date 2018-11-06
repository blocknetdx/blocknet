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
//#include <assert.h>
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
    crBadBDepositTx   = 15,
    crTimeout         = 16
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
    // xbcTransaction  (152 bytes min)
    // clients not process this messages, only exchange
    //    uint256  client transaction id
    //    20 bytes source address
    //    8 bytes  source currency
    //    uint64   source amount
    //    20 bytes destination address
    //    8 bytes  destination currency
    //    uint64   destination amount
    //    uint64   timestamp
    //
    //    array of unspent outputs used in transaction
    //      uint32_t count of array items
    //      array items
    //        uint256  transaction id
    //        uint32_t out idx
    xbcTransaction = 3,
    //
    // xbcPendingTransaction (124 bytes)
    // exchange broadcast send this message, send list of opened transactions
    //    uint256 transaction id
    //    8 bytes source currency
    //    uint64  source amount
    //    8 bytes destination currency
    //    uint64  destination amount
    //    uint160 hub address
    //    uint64  timestamp
    //    uint256 block hash
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
    // xbcTransactionHold (52 bytes)
    //    uint160 hub address
    //    uint256 transaction id
    xbcTransactionHold = 6,
    //
    // xbcTransactionHoldApply (72 bytes)
    //    uint160 hub address
    //    uint160 client address
    //    uint256 hub transaction id
    xbcTransactionHoldApply = 7,

    //
    // xbcTransactionInit (144 bytes min)
    //    uint160 client address
    //    uint160 hub address
    //    uint256 hub transaction id
    //    20 bytes source address
    //    8 bytes source currency
    //    uint64 source amount
    //    20 bytes source address
    //    8 bytes destination currency
    //    uint64 destination amount
    xbcTransactionInit = 8,
    //
    // xbcTransactionInitialized (104 bytes)
    //    uint160 hub address
    //    uint160 client address
    //    uint256 hub transaction id
    //    uint256 fee transaction id
    xbcTransactionInitialized = 9,

    //
    // xbcTransactionCreateA (105 bytes min)
    //    uint160  client address
    //    uint160  hub address
    //    uint256  hub transaction id
    //    bytes    B public key (33 bytes)
    xbcTransactionCreateA = 10,
    //
    // xbcTransactionCreatedA (100 bytes min)
    //    uint160 hub address
    //    uint160 client address
    //    uint256 hub transaction id
    //    string  A deposit tx id
    //    bytes   hashed secret (20 bytes)
    //    uint32  A lock time
    //    uint32  B lock time
    xbcTransactionCreatedA = 11,
    //
    // xbcTransactionCreateB (133 bytes min)
    //    uint160  client address
    //    uint160  hub address
    //    uint256  hub transaction id
    //    bytes    A public key (33 bytes)
    //    string   A deposit tx id
    //    bytes    hashed secret (20 bytes)
    //    uint32   A lock time
    //    uint32   B lock time
    xbcTransactionCreateB = 12,
    //
    // xbcTransactionCreatedB (72 bytes min)
    //    uint160 hub address
    //    uint160 client address
    //    uint256 hub transaction id
    //    string  B deposit tx id
    xbcTransactionCreatedB = 13,

    //
    // xbcTransactionConfirmA (72 bytes min)
    //    uint160 client address
    //    uint160 hub address
    //    uint256 hub transaction id
    //    string  B deposit tx id
    xbcTransactionConfirmA = 18,
    //
    // xbcTransactionConfirmedA (72 bytes min)
    //    uint160  hub address
    //    uint160  client address
    //    uint256  hub transaction id
    //    x public key, 33 bytes
    xbcTransactionConfirmedA = 19,
    //
    // xbcTransactionConfirmB (105 bytes min)
    //    uint160  client address
    //    uint160  hub address
    //    uint256  hub transaction id
    //    x public key, 33 bytes
    //    string   A deposit tx id
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
    // xbcTransactionFinished
    //    uint160 client address
    //    uint256 hub transaction id
    //
    xbcTransactionFinished = 24,

    //
    // xbcServicesPing
    //    array of supported services
    //        string Service name
    //        ... (max 10000 bytes)
    //
    xbcServicesPing = 50
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
// boost::uint32_t extsize (backward compatibility)
// boost::uint32_t crc
//
// boost::uint32_t rezerved
// boost::uint32_t rezerved
//******************************************************************************
class XBridgePacket
{
    std::vector<unsigned char> m_body;

public:
    enum
    {
        // header, size, version, command, timestamp, pubkey, signature
        headerSize                = 8*sizeof(uint32_t)+33+64,
        commandSize               = sizeof(uint32_t),
        timestampSize             = sizeof(uint32_t),
        addressSize               = 20,

        hashSize                  = 32,

        privkeySize               = 32,

        pubkeySizeRaw             = 32,
        pubkeySize                = 33,
        uncompressedPubkeySizeRaw = 64,
        uncompressedPubkeySize    = 65,

        rawSignatureSize          = 64,
        signatureSize             = 65
    };

    uint32_t     size()    const     { return sizeField(); }
    uint32_t     allSize() const     { return static_cast<uint32_t>(m_body.size()); }

    crc_t        crc()     const
    {
        // TODO implement this
        ERR() << "not implemented " << __FUNCTION__;
//        assert(!"not implemented");
        return 0;
        // return crcField();
    }

    uint32_t version() const                { return versionField(); }

    XBridgeCommand  command() const         { return static_cast<XBridgeCommand>(commandField()); }

    const unsigned char * pubkey() const    { return pubkeyField(); }
    const unsigned char * signature() const { return signatureField(); }

    void    alloc()                         { m_body.resize(headerSize + size()); }

    const std::vector<unsigned char> & body() const
                                            { return m_body; }
    unsigned char  * header()               { return &m_body[0]; }
    unsigned char  * data()                 { return &m_body[headerSize]; }

    void    clear()
    {
        m_body.resize(headerSize);
        commandField()   = 0;
        sizeField()      = 0;
        __oldSizeField() = 0;

        // TODO crc
        // crcField() = 0;
    }

    void resize(const uint32_t size)
    {
        m_body.resize(size+headerSize);
        sizeField() = size;
        __oldSizeField() = sizeField()+__headerDifference;
    }

    void    setData(const unsigned char data)
    {
        m_body.resize(sizeof(data) + headerSize);
        sizeField() = sizeof(data);
        m_body[headerSize] = data;
        __oldSizeField() = sizeField()+__headerDifference;
    }

//    void    setData(const int32_t data)
//    {
//        m_body.resize(sizeof(data) + headerSize);
//        sizeField() = sizeof(data);
//        field32<2>() = data;
//        __oldSizeField() = sizeField()+__headerDifference;
//    }

    void    setData(const std::string & data)
    {
        m_body.resize(data.size() + headerSize);
        sizeField() = static_cast<uint32_t>(data.size());
        if (data.size())
        {
            data.copy((char *)(&m_body[headerSize]), data.size());
        }
        __oldSizeField() = sizeField()+__headerDifference;
    }

//    void    setData(const std::vector<unsigned char> & data, const unsigned int offset = 0)
//    {
//        setData(&data[0], static_cast<uint32_t>(data.size()), offset);
//    }

//    void    setData(const unsigned char * data, const uint32_t size, const uint32_t offset = 0)
//    {
//        unsigned int off = offset + headerSize;
//        if (size)
//        {
//            if (m_body.size() < size+off)
//            {
//                m_body.resize(size+off);
//                sizeField() = size+off-headerSize;
//            }
//            memcpy(&m_body[off], data, size);
//            __oldSizeField() = sizeField()+__headerDifference;
//        }
//    }

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
        __oldSizeField() = sizeField()+__headerDifference;
    }

    void append(const uint32_t data)
    {
        m_body.reserve(m_body.size() + sizeof(data));
        unsigned char * ptr = (unsigned char *)&data;
        std::copy(ptr, ptr+sizeof(data), std::back_inserter(m_body));
        sizeField() = static_cast<uint32_t>(m_body.size()) - headerSize;
        __oldSizeField() = sizeField()+__headerDifference;
    }

    void append(const uint64_t data)
    {
        m_body.reserve(m_body.size() + sizeof(data));
        unsigned char * ptr = (unsigned char *)&data;
        std::copy(ptr, ptr+sizeof(data), std::back_inserter(m_body));
        sizeField() = static_cast<uint32_t>(m_body.size()) - headerSize;
        __oldSizeField() = sizeField()+__headerDifference;
    }

    void append(const unsigned char * data, const int size)
    {
        m_body.reserve(m_body.size() + size);
        std::copy(data, data+size, std::back_inserter(m_body));
        sizeField() = static_cast<uint32_t>(m_body.size()) - headerSize;
        __oldSizeField() = sizeField()+__headerDifference;
    }

    void append(const std::string & data)
    {
        m_body.reserve(m_body.size() + data.size()+1);
        std::copy(data.begin(), data.end(), std::back_inserter(m_body));
        m_body.push_back(0);
        sizeField() = static_cast<uint32_t>(m_body.size()) - headerSize;
        __oldSizeField() = sizeField()+__headerDifference;
    }

    void append(const std::vector<unsigned char> & data)
    {
        m_body.reserve(m_body.size() + data.size());
        std::copy(data.begin(), data.end(), std::back_inserter(m_body));
        sizeField() = static_cast<uint32_t>(m_body.size()) - headerSize;
        __oldSizeField() = sizeField()+__headerDifference;
    }

    bool copyFrom(const std::vector<unsigned char> & data)
    {
        if (data.size() < headerSize)
        {
            ERR() << "received data size less than packet header size " << __FUNCTION__;
            return false;
        }

        m_body = data;

        if (sizeField() != static_cast<uint32_t>(data.size())-headerSize)
        {
            ERR() << "incorrect data size " << __FUNCTION__;
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

    bool sign(const std::vector<unsigned char> & pubkey,
              const std::vector<unsigned char> & privkey);
    bool verify();
    bool verify(const std::vector<unsigned char> & pubkey);

protected:
    template<uint32_t INDEX>
    uint32_t & field32()
        { return *static_cast<uint32_t *>(static_cast<void *>(&m_body[INDEX * 4])); }

    template<uint32_t INDEX>
    uint32_t const& field32() const
        { return *static_cast<uint32_t const*>(static_cast<void const*>(&m_body[INDEX * 4])); }

    uint32_t       & versionField()              { return field32<0>(); }
    uint32_t const & versionField() const        { return field32<0>(); }
    uint32_t &       commandField()              { return field32<1>(); }
    uint32_t const & commandField() const        { return field32<1>(); }
    uint32_t &       timestampField()            { return field32<2>(); }
    uint32_t const & timestampField() const      { return field32<2>(); }
    uint32_t &       sizeField()                 { return field32<4>(); }
    uint32_t const & sizeField() const           { return field32<4>(); }
    uint32_t &       crcField()                  { return field32<5>(); }
    uint32_t const & crcField() const            { return field32<5>(); }

    unsigned char *       pubkeyField()          { return &m_body[20]; }
    const unsigned char * pubkeyField() const    { return &m_body[20]; }
    unsigned char *       signatureField()       { return &m_body[53]; }
    const unsigned char * signatureField() const { return &m_body[53]; }

private:
    // TODO temporary constants for backward compatibility
    enum
    {
        // header: size, version, command, timestamp, rezerved
        __oldHeaderSize = 8*sizeof(uint32_t),
        __headerDifference = headerSize - __oldHeaderSize
    };

    // save size field for backward compatibility
    uint32_t &       __oldSizeField()              { return field32<3>(); }
    uint32_t const & __oldSizeField() const        { return field32<3>(); }
};

typedef std::shared_ptr<XBridgePacket> XBridgePacketPtr;
typedef std::deque<XBridgePacketPtr>   XBridgePacketQueue;

#endif // XBRIDGEPACKET_H
