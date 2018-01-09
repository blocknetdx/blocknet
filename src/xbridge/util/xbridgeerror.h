#ifndef XBRIDGEERROR_H
#define XBRIDGEERROR_H
#include <string>
#include <vector>

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
    const std::string xbridgeErrorText(const Error error, const std::string &argument = "");
    const std::string xbridgeErrorText(const Error error, const std::vector<unsigned char> &argument);
}
#endif // XBRIDGEERROR_H
