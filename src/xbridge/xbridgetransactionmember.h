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
    /**
     * @brief setLockTime - set lock time
     * @param lockTime
     */
    void setLockTime(const uint32_t lockTime)               { m_lockTime = lockTime; }
    /**
     * @brief Return the lock time.
     * @return lock time
     */
    uint32_t lockTime() const                               { return m_lockTime; }
    /**
     * @brief Assigns the payment transaction id.
     * @param payTxId
     */
    void setPayTxId(const std::string & payTxId)            { m_payTxId = payTxId; }
    /**
     * @brief Pay transaction id.
     * @return
     */
    std::string payTxId() const                             { return m_payTxId; }
    /**
     * @brief Assigns the refund transaction id.
     * @param refTxId
     */
    void setRefTxId(const std::string & refTxId)            { m_refTxId = refTxId; }
    /**
     * @brief Refund transaction id.
     * @return
     */
    std::string refTxId() const                             { return m_refTxId; }
    /**
     * @brief Assigns the refund transaction raw output.
     * @param refTx
     */
    void setRefTx(const std::string & refTx)                { m_refTx = refTx; }
    /**
     * @brief Refund transaction raw output.
     * @return
     */
    std::string refTx() const                               { return m_refTx; }

private:
    uint256                    m_id;
    std::vector<unsigned char> m_sourceAddr;
    std::vector<unsigned char> m_destAddr;
    uint256                    m_transactionHash;
    std::vector<unsigned char> m_mpubkey;
    uint32_t                   m_lockTime{0};
    std::string                m_payTxId;
    std::string                m_refTxId;
    std::string                m_refTx;
};

} // namespace xbridge

#endif // XBRIDGETRANSACTIONMEMBER_H
