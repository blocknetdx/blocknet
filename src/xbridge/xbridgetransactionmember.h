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
    /**
     * @brief XBridgeTransactionMember - default constructor
     */
    XBridgeTransactionMember()                              {}
    /**
     * @brief XBridgeTransactionMember
     * @param id - id of transaction
     */
    XBridgeTransactionMember(const uint256 & id) : m_id(id) {}

    /**
     * @brief isEmpty
     * @return
     */
    bool isEmpty() const { return m_sourceAddr.empty() || m_destAddr.empty(); }

    /**
     * @brief id
     * @return id of transaction
     */
    const uint256 id() const                                { return m_id; }
    /**
     * @brief source
     * @return address of source wallet
     */
    const std::vector<unsigned char> & source() const       { return m_sourceAddr; }
    /**
     * @brief setSource - set new source address
     * @param addr
     */
    void setSource(const std::vector<unsigned char> & addr) { m_sourceAddr = addr; }
    /**
     * @brief dest
     * @return destination wallet address
     */
    const std::vector<unsigned char> & dest() const         { return m_destAddr; }
    /**
     * @brief setDest - set new destination address
     * @param addr
     */
    void setDest(const std::vector<unsigned char> & addr)   { m_destAddr = addr; }
    /**
     * @brief mpubkey
     * @return public key
     */
    const std::vector<unsigned char> & mpubkey() const      { return m_mpubkey; }
    /**
     * @brief setMPubkey - set public key
     * @param mpub -
     */
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
