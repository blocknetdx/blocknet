#include "xbridgeerror.h"
namespace xbridge {
    const std::string xbridgeErrorText(const Error error, const std::string &argument)
    {
        switch (error)
        {
        case Error::INVALID_CURRENCY:
            return "invalid currency " + argument;
        case Error::NO_SESSION:
            return "no session for currency " + argument;
        case Error::INSIFFICIENT_FUNDS:
            return "Insufficient funds for " + argument;
        case Error::TRANSACTION_NOT_FOUND:
            return "Transaction " + argument + " not found";
        case Error::UNKNOWN_SESSION:
            return "Unknown session for " + argument;
        case Error::REVERT_TX_FAILED:
            return "revert tx failed for "  + argument;
        case Error::SUCCESS:
            return "";
        case Error::UNKNOWN_ERROR:
            return "unknown error";
        case Error::INVALID_ADDRESS:
            return "invalid address";
        case Error::INVALID_PARAMETERS:
            return "";
        case Error::INVALID_AMOUNT:
            return "invalid amount " + argument;
        }
        return "invalid error value";
    }
    const std::string xbridgeErrorText(const Error error, const std::vector<unsigned char> &argument)
    {
        return xbridgeErrorText(error, std::string(argument.begin(), argument.end()));
    }
}
