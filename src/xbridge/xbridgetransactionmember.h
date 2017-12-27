//*****************************************************************************
//*****************************************************************************

#ifndef XBRIDGETRANSACTIONMEMBER_H
#define XBRIDGETRANSACTIONMEMBER_H

#include "uint256.h"

#include <string>
#include <vector>

#include <boost/cstdint.hpp>

//*****************************************************************************
//*****************************************************************************
class XBridgeTransactionMember
{
public:
    XBridgeTransactionMember()                              {}
    XBridgeTransactionMember(const uint256 & id) : m_id(id) {}

    bool isEmpty() const { return m_sourceAddr.empty() || m_destAddr.empty(); }

    const uint256 id() const                           { return m_id; }

    const std::string & source() const                 { return m_sourceAddr; }
    const std::vector<unsigned char> & xsource() const { return m_sourceXAddr; }
    void setSource(const std::string & addr, const std::vector<unsigned char> & xaddr)
    {
        m_sourceAddr  = addr;
        m_sourceXAddr = xaddr;
    }

    const std::string & dest() const                   { return m_destAddr; }
    const std::vector<unsigned char> & xdest() const   { return m_destXAddr; }
    void setDest(const std::string & addr, const std::vector<unsigned char> & xaddr)
    {
        m_destAddr  = addr;
        m_destXAddr = xaddr;
    }

private:
    uint256                    m_id;
    std::string                m_sourceAddr;
    std::vector<unsigned char> m_sourceXAddr;
    std::string                m_destAddr;
    std::vector<unsigned char> m_destXAddr;
    uint256                    m_transactionHash;
};

#endif // XBRIDGETRANSACTIONMEMBER_H
