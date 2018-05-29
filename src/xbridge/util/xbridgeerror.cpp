//******************************************************************************
//******************************************************************************

#include "xbridgeerror.h"

//******************************************************************************
//******************************************************************************
namespace xbridge
{

//******************************************************************************
//******************************************************************************
const std::string xbridgeErrorText(const Error & error, const std::string & argument)
{
    switch (error)
    {
        case Error::INVALID_CURRENCY:
            return "Invalid coin " + argument;
        case Error::INVALID_STATE:
            return "invalid transaction state " + argument;
        case Error::NO_SESSION:
            return "No session for currency " + argument;
        case Error::INSIFFICIENT_FUNDS:
            return "Insufficient funds for " + argument;
        case Error::FUNDS_NOT_SIGNED:
            return "Funds not signed for " + argument;
        case Error::TRANSACTION_NOT_FOUND:
            return "Transaction " + argument + " not found";
        case Error::UNKNOWN_SESSION:
            return "Unknown session for " + argument;
        case Error::REVERT_TX_FAILED:
            return "Revert tx failed for "  + argument;
        case Error::SUCCESS:
            return "";
        case Error::UNKNOWN_ERROR:
            return "Internal Server Error";
        case Error::INVALID_ADDRESS:
            return "Bad address " + argument;
        case Error::INVALID_PARAMETERS:
            return "Invalid parameters: " + argument;
        case Error::INVALID_AMOUNT:
            return "Invalid amount " + argument;
        case INVALID_SIGNATURE:
            return "Invalid signature " + argument;
        case UNAUTHORIZED:
            return "Unauthorized " + argument;
        case BAD_REQUEST:
            return  "Bad Request " + argument;
        case INVALID_MAKE_SYMBOL:
            return "Invalid maker symbol " + argument;
        case INVALID_TAKE_SYMBOL:
            return "Invalid taker symbol " + argument;
        case INVALID_DETAIL_LEVEL:
            return "Invalid detail level, possible values: 1 - 3";
        case INVALID_TIME:
            return "Invalid time format, ISO 8601 date format required";
        case NOT_EXCHANGE_NODE:
            return "Blocknet is not running as an exchange node";
        case DUST:
            return "Amount is dust (very small)";
        case INSIFFICIENT_FUNDS_DX:
            return "Blocknet wallet amount is too small to cover the fee payment";
    }
    return "invalid error value";
}

//******************************************************************************
//******************************************************************************
const std::string xbridgeErrorText(const Error &error, const std::vector<unsigned char> &argument)
{
    return xbridgeErrorText(error, std::string(argument.begin(), argument.end()));
}

} // namespace xbridge
