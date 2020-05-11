// Copyright (c) 2018-2019 The Blocknet developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BLOCKNET_XROUTER_XROUTERERROR_H
#define BLOCKNET_XROUTER_XROUTERERROR_H

#include <exception>

namespace xrouter
{
    enum Error
    {
        SUCCESS                 = 0,
        UNAUTHORIZED            = 1001,
        INTERNAL_SERVER_ERROR   = 1002,
        SERVER_TIMEOUT          = 1003,
        BAD_REQUEST             = 1004,
        BAD_VERSION             = 1005,
        BAD_CONNECTOR           = 1018,
        INVALID_PARAMETERS      = 1025,
        BAD_ADDRESS             = 1026,
        INSUFFICIENT_FUNDS      = 1027,
        INSUFFICIENT_FEE        = 1028,
        UNSUPPORTED_BLOCKCHAIN  = 1030,
        UNSUPPORTED_SERVICE     = 1031,
        NOT_ENOUGH_NODES        = 1032,
        MAXFEE_TOO_LOW          = 1033,
        TOO_MANY_REQUESTS       = 1034,
        NO_REPLIES              = 1035,
        BAD_SIGNATURE           = 1036,
    };

    class XRouterError : public std::exception {
    public:
        XRouterError(std::string message, enum Error errcode) : msg(std::move(message)), code(errcode) { }
        const char * what() const noexcept override {
            return msg.c_str();
        }
        std::string msg;
        Error code;
    };
} // namespace xrouter

#endif // BLOCKNET_XROUTER_XROUTERERROR_H
