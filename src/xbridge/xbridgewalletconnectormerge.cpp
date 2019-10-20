//******************************************************************************
//******************************************************************************

#include "json/json_spirit_reader_template.h"
#include "json/json_spirit_writer_template.h"
#include "json/json_spirit_utils.h"

#include "xbridgewalletconnectormerge.h"

#include "util/logger.h"
#include "util/txlog.h"

#include "xbitcoinaddress.h"
#include "xbitcointransaction.h"

//*****************************************************************************
//*****************************************************************************
namespace xbridge
{

//******************************************************************************
//******************************************************************************
MergeWalletConnector::MergeWalletConnector()
    : BtcWalletConnector()
{

}

//*****************************************************************************
bool MergeWalletConnector::init()
{
    // convert prefixes
    addrPrefix   = static_cast<char>(std::atoi(addrPrefix.data()));
    scriptPrefix = static_cast<char>(std::atoi(scriptPrefix.data()));
    secretPrefix = static_cast<char>(std::atoi(secretPrefix.data()));

    // wallet info
    rpc::WalletInfo info;
    if (!this->getInfo(info))
        return false;

    auto fallbackMinTxFee = static_cast<uint64_t>(info.relayFee * 2 * COIN);
    if (minTxFee == 0 && feePerByte == 0 && fallbackMinTxFee == 0) { // non-relay fee coin
        minTxFee = 0; // units (e.g. satoshis for btc)
        dustAmount = 0;
        WARN() << currency << " \"" << title << "\"" << " Using minimum/no fee of 0 sats";
    } else {
        minTxFee = std::max(fallbackMinTxFee, minTxFee);
        dustAmount = fallbackMinTxFee > 0 ? fallbackMinTxFee : minTxFee;
    }

    return true;
}

} // namespace xbridge
