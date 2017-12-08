#ifndef XBRIDGEERROR_H
#define XBRIDGEERROR_H
#include <string>

namespace xbridge
{
    enum Error
    {
        // 'NO_ERROR' is defined (probably by some windows headers) when compiling windows targets
        SUCCESS = 0,
        INVALID_CURRENCY,
        NO_SESSION,
        INSIFFICIENT_FUNDS,
        TRANSACTION_NOT_FOUND,
        UNKNOWN_SESSION,
        REVERT_TX_FAILED,
        INVALID_AMOUNT,
        INVALID_PARAMETERS,
        INVALID_ADDRESS,
        UNKNOWN_ERROR
    };

    /**
     * @brief xbridgeErrorText
     * @param error
     * @param argumrnt
     * @return
     */
    const std::string xbridgeErrorText(const Error error, const std::string &argumrnt = "");
}
#endif // XBRIDGEERROR_H
