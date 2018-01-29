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
/**
 * @brief The XBridgeTransactionMember class - contains id of transaction,
 * source and destination addresses, hash of transaction
 */
class XBridgeTransactionMember
{
public:
    /**
     * @brief XBridgeTransactionMember - default constructor
     */
    XBridgeTransactionMember()                              {}

    XBridgeTransactionMember(const uint256 & id) : m_id(id) {}

    /**
     * @brief isEmpty
     * @return true, if source address empty or destination address empty
     */
    bool isEmpty() const { return m_sourceAddr.empty() || m_destAddr.empty(); }

    /**
     * @brief id
     * @return id of transaction
     */
    const uint256 id() const                                { return m_id; }
    /**
     * @brief source
     * @return source address
     */
    const std::vector<unsigned char> & source() const       { return m_sourceAddr; }
    /**
     * @brief setSource - set source address
     * @param addr - new address value
     */
    void setSource(const std::vector<unsigned char> & addr) { m_sourceAddr = addr; }
    /**
     * @brief dest
     * @return destination address
     */
    const std::vector<unsigned char> & dest() const         { return m_destAddr; }
    /**
     * @brief setDest - set destination address
     * @param addr - new address value
     */
    void setDest(const std::vector<unsigned char> & addr)   { m_destAddr = addr; }

private:

    /**
     * @brief m_id - id of transaction
     */
    uint256                    m_id;
    /**
     * @brief m_sourceAddr -transaction source address
     */
    std::vector<unsigned char> m_sourceAddr;
    /**
     * @brief m_destAddr - transaction destination
     */
    std::vector<unsigned char> m_destAddr;
    /**
     * @brief m_transactionHash - hash of transaction
     */
    uint256                    m_transactionHash;
};

} // namespace xbridge

#endif // XBRIDGETRANSACTIONMEMBER_H
