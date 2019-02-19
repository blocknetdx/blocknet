//******************************************************************************
//******************************************************************************

#ifndef XROUTERPACKET_H
#define XROUTERPACKET_H

#include "version.h"

#include <vector>
#include <deque>
#include <memory>
#include <ctime>
#include <cstdio>
#include <stdint.h>
#include <iostream>
#include <string.h>
#include "keystore.h"
#include <boost/preprocessor.hpp>

//*****************************************************************************
//*****************************************************************************
namespace xrouter
{
//******************************************************************************
//******************************************************************************
enum TxCancelReason
{
    crUnknown         = 0
};

//******************************************************************************
//******************************************************************************

enum XRouterCommand
{
    xbcInvalid                       = 0,
    xrReply                          = 1,
    xrConfigReply                    = 2,
    xrFetchReply                     = 3,
    xrGetBlockCount                  = 4,
    xrGetBlockHash                   = 5,
    xrGetBlock                       = 6,
    xrGetTransaction                 = 7,
    xrGetAllBlocks                   = 8,
    xrGetAllTransactions             = 9,
    xrGetBalance                     = 10,
    xrGetBalanceUpdate               = 11,
    xrGetTransactionsBloomFilter     = 12,
    xrSendTransaction                = 13,
    xrTimeToBlockNumber              = 14,
    xrCustomCall                     = 15,
    xrGetPaymentAddress              = 16,
    xrGetXrouterConfig               = 17
};

inline const char* XRouterCommand_ToString(enum XRouterCommand c)
{
    switch (c)
    {
        case xbcInvalid                   : return "xbcInvalid";
        case xrReply                      : return "xrReply";
        case xrConfigReply                : return "xrConfigReply";
        case xrFetchReply                 : return "xrFetchReply";
        case xrGetBlockCount              : return "xrGetBlockCount";
        case xrGetBlockHash               : return "xrGetBlockHash";
        case xrGetBlock                   : return "xrGetBlock";
        case xrGetTransaction             : return "xrGetTransaction";
        case xrGetAllBlocks               : return "xrGetAllBlocks";
        case xrGetAllTransactions         : return "xrGetAllTransactions";
        case xrGetBalance                 : return "xrGetBalance";
        case xrGetBalanceUpdate           : return "xrGetBalanceUpdate";
        case xrGetTransactionsBloomFilter : return "xrGetTransactionsBloomFilter";
        case xrSendTransaction            : return "xrSendTransaction";
        case xrTimeToBlockNumber          : return "xrTimeToBlockNumber";
        case xrCustomCall                 : return "xrCustomCall";
        case xrGetPaymentAddress          : return "xrGetPaymentAddress";
        case xrGetXrouterConfig           : return "xrGetXrouterConfig";
        default: {
            char* s;
            sprintf(s, "[Unknown XRouterCommand] %u", c);
            return s;
        }
    }
};

inline bool XRouterCommand_IsValid(const char* c)
{
    if (XRouterCommand_ToString(xbcInvalid)                   ||
        XRouterCommand_ToString(xrReply)                      ||
        XRouterCommand_ToString(xrConfigReply)                ||
        XRouterCommand_ToString(xrFetchReply)                 ||
        XRouterCommand_ToString(xrGetBlockCount)              ||
        XRouterCommand_ToString(xrGetBlockHash)               ||
        XRouterCommand_ToString(xrGetBlock)                   ||
        XRouterCommand_ToString(xrGetTransaction)             ||
        XRouterCommand_ToString(xrGetAllBlocks)               ||
        XRouterCommand_ToString(xrGetAllTransactions)         ||
        XRouterCommand_ToString(xrGetBalance)                 ||
        XRouterCommand_ToString(xrGetBalanceUpdate)           ||
        XRouterCommand_ToString(xrGetTransactionsBloomFilter) ||
        XRouterCommand_ToString(xrSendTransaction)            ||
        XRouterCommand_ToString(xrTimeToBlockNumber)          ||
        XRouterCommand_ToString(xrCustomCall)                 ||
        XRouterCommand_ToString(xrGetPaymentAddress)          ||
        XRouterCommand_ToString(xrGetXrouterConfig))
        return true;

    return false;
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
class XRouterPacket
{
    std::vector<unsigned char> m_body;

public:
    enum
    {
        // header, size, version, command, timestamp, pubkey, signature
        headerSize       = 8*sizeof(uint32_t)+33+64,
        commandSize      = sizeof(uint32_t),
        timestampSize    = sizeof(uint32_t),
        addressSize      = 20,
        hashSize         = 32,
        privkeySize      = 32,
        pubkeySize       = 33,
        rawSignatureSize = 64,
        signatureSize    = 65
    };

    uint32_t     size()    const     { return sizeField(); }
    uint32_t     allSize() const     { return static_cast<uint32_t>(m_body.size()); }

    crc_t        crc()     const
    {
        // TODO implement this
        std::cerr << "not implemented " << __FUNCTION__;
//        assert(!"not implemented");
        return 0;
        // return crcField();
    }

    uint32_t version() const                { return versionField(); }

    XRouterCommand  command() const         { return static_cast<XRouterCommand>(commandField()); }

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

    bool copyFrom(const std::vector<unsigned char> & data);

    XRouterPacket() : m_body(headerSize, 0)
    {
        versionField()   = static_cast<uint32_t>(XROUTER_PROTOCOL_VERSION);
        timestampField() = static_cast<uint32_t>(time(0));
    }

    explicit XRouterPacket(const std::string& raw) : m_body(raw.begin(), raw.end())
    {
        timestampField() = static_cast<uint32_t>(time(0));
    }

    XRouterPacket(const XRouterPacket & other)
    {
        m_body = other.m_body;
    }

    XRouterPacket(XRouterCommand c) : m_body(headerSize, 0)
    {
        versionField()   = static_cast<uint32_t>(XROUTER_PROTOCOL_VERSION);
        commandField()   = static_cast<uint32_t>(c);
        timestampField() = static_cast<uint32_t>(time(0));
    }

    XRouterPacket & operator = (const XRouterPacket & other)
    {
        m_body    = other.m_body;

        return *this;
    }

    bool sign(const std::vector<unsigned char> & pubkey,
              const std::vector<unsigned char> & privkey);
    bool sign(CKey key);
    bool verify();
    bool verify(const std::vector<unsigned char> & pubkey);

private:
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

typedef std::shared_ptr<XRouterPacket> XRouterPacketPtr;
typedef std::deque<XRouterPacketPtr>   XRouterPacketQueue;
} // namespace xrouter


#endif // XROUTERPACKET_H
