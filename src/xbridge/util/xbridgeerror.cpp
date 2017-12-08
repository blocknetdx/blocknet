#include "xbridgeerror.h"
namespace xbridge {
    const std::string xbridgeErrorText(const Error error, const std::string &argumrnt)
    {
        switch (error)
        {
        case Error::INVALID_CURRENCY:
            return "invalid currency " + argumrnt;
        case Error::NO_SESSION:
            return "no session for currency " + argumrnt;
        case Error::INSIFFICIENT_FUNDS:
            return "Insufficient funds for " + argumrnt;
        case Error::TRANSACTION_NOT_FOUND:
            return "Transaction " + argumrnt + " not found";
        case Error::UNKNOWN_SESSION:
            return "Unknown session for " + argumrnt;
        case Error::REVERT_TX_FAILED:
            return "revert tx failed for "  + argumrnt;
        case Error::SUCCESS:
            return "";
        case Error::UNKNOWN_ERROR:
            return "unknown error";
        case Error::INVALID_ADDRESS:
            return "invalid address";
        case Error::INVALID_PARAMETERS:
            return "";
        case Error::INVALID_AMOUNT:
            return "invalid amount " + argumrnt;
        }
        return "invalid error value";
    }
}
