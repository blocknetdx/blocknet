//*****************************************************************************
//*****************************************************************************

#ifndef XBRIDGETRANSACTIONMEMBER_H
#define XBRIDGETRANSACTIONMEMBER_H

#include "uint256.h"

#include <string>
#include <vector>

#include <boost/cstdint.hpp>

//******************************************************************************
//******************************************************************************
namespace xbridge
{

//*****************************************************************************
//*****************************************************************************
class XBridgeTransactionMember
{
public:
    XBridgeTransactionMember()                              {}
    XBridgeTransactionMember(const uint256 & id) : m_id(id) {}

    bool isEmpty() const { return m_sourceAddr.empty() || m_destAddr.empty(); }

    const uint256 id() const                                { return m_id; }
    const std::vector<unsigned char> & source() const       { return m_sourceAddr; }
    void setSource(const std::vector<unsigned char> & addr) { m_sourceAddr = addr; }
    const std::vector<unsigned char> & dest() const         { return m_destAddr; }
    void setDest(const std::vector<unsigned char> & addr)   { m_destAddr = addr; }
    const std::vector<unsigned char> & mpubkey() const      { return m_mpubkey; }
    void setMPubkey(const std::vector<unsigned char> & mpub){ m_mpubkey = mpub; }

private:
    uint256                    m_id;
    std::vector<unsigned char> m_sourceAddr;
    std::vector<unsigned char> m_destAddr;
    uint256                    m_transactionHash;
    std::vector<unsigned char> m_mpubkey;
};

} // namespace xbridge

#endif // XBRIDGETRANSACTIONMEMBER_H
