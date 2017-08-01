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

    const uint256 id() const                 { return m_id; }
    const std::string & source() const       { return m_sourceAddr; }
    void setSource(const std::string & addr) { m_sourceAddr = addr; }
    const std::string & dest() const         { return m_destAddr; }
    void setDest(const std::string & addr)   { m_destAddr = addr; }

private:
    uint256                    m_id;
    std::string                m_sourceAddr;
    std::string                m_destAddr;
    uint256                    m_transactionHash;
};

#endif // XBRIDGETRANSACTIONMEMBER_H
