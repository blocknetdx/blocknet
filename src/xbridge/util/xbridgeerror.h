// Copyright (c) 2017-2019 The Blocknet developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

//******************************************************************************
//******************************************************************************

#ifndef BLOCKNET_XBRIDGE_UTIL_XBRIDGEERROR_H
#define BLOCKNET_XBRIDGE_UTIL_XBRIDGEERROR_H

//******************************************************************************
//******************************************************************************
#include <string>
#include <vector>

//******************************************************************************
//******************************************************************************
namespace xbridge
{
    enum Error
    {
        // 'NO_ERROR' is defined (probably by some windows headers) when compiling windows targets
        SUCCESS                 = 0,
        UNAUTHORIZED            = 1001,
        UNKNOWN_ERROR           = 1002,
        BAD_REQUEST             = 1004,
        INVALID_MAKE_SYMBOL     = 1011,
        INVALID_TAKE_SYMBOL     = 1012,
        INVALID_DETAIL_LEVEL    = 1015,
        INVALID_TIME            = 1016,
        INVALID_CURRENCY        = 1017,
        NO_SESSION              = 1018,
        INSUFFICIENT_FUNDS      = 1019,
        FUNDS_NOT_SIGNED        = 1020,
        TRANSACTION_NOT_FOUND   = 1021,
        UNKNOWN_SESSION         = 1022,
        REVERT_TX_FAILED        = 1023,
        INVALID_AMOUNT          = 1024,
        INVALID_PARAMETERS      = 1025,
        INVALID_ADDRESS         = 1026,
        INVALID_SIGNATURE       = 1027,
        INVALID_STATE           = 1028,
        NOT_EXCHANGE_NODE       = 1029,
        DUST                    = 1030,
        INSUFFICIENT_FUNDS_DX   = 1031,
        NO_SERVICE_NODE         = 1032,
        INVALID_ONCHAIN_HISTORY = 1033,
        INVALID_PARTIAL_ORDER   = 1034
    };

    /**
     * @brief xbridgeErrorText
     * @param error
     * @param argumrnt
     * @return
     */
    const std::string xbridgeErrorText(const Error & error, const std::string & argument = "");
    const std::string xbridgeErrorText(const Error & error, const std::vector<unsigned char> & argument);

} // namespace xbridge

#endif // BLOCKNET_XBRIDGE_UTIL_XBRIDGEERROR_H
