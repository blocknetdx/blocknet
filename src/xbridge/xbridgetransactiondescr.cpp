#include "xbridgetransactiondescr.h"
#include "xbridgeapp.h"
#include "util/settings.h"

namespace xbridge
{

ostream & operator << (ostream& out, const TransactionDescrPtr& tx)
{
    if(!settings().isFullLog())
    {
        out << std::endl << "ORDER ID: " << tx->id.GetHex() << std::endl;

        return out;
    }

    xbridge::WalletConnectorPtr connFrom = xbridge::App::instance().connectorByCurrency(tx->fromCurrency);
    xbridge::WalletConnectorPtr connTo   = xbridge::App::instance().connectorByCurrency(tx->toCurrency);

    if (!connFrom || !connTo)
        out << "MISSING SOME CONNECTOR, NOT ALL ORDER INFO WILL BE LOGGED";

    std::ostringstream inputsStream;
    uint32_t count = 0;
    for(const xbridge::wallet::UtxoEntry & entry : tx->usedCoins)
    {
        inputsStream << "    INDEX: " << count << std::endl
                     << "    ID: " << entry.txId << std::endl
                     << "    VOUT: " << boost::lexical_cast<std::string>(entry.vout) << std::endl
                     << "    AMOUNT: " << entry.amount << std::endl
                     << "    ADDRESS: " << entry.address << std::endl;

        ++count;
    }

    out << std::endl
        << "LOG ORDER BODY" << std::endl
        << "ID: " << tx->id.GetHex() << std::endl
        << "MAKER: " << tx->fromCurrency << std::endl
        << "MAKER SIZE: " << util::xBridgeStringValueFromAmount(tx->fromAmount) << std::endl
        << "MAKER ADDR: " << (!tx->from.empty() && connFrom ? connFrom->fromXAddr(tx->from) : "") << std::endl
        << "TAKER: " << tx->toCurrency << std::endl
        << "TAKER SIZE: " << util::xBridgeStringValueFromAmount(tx->toAmount) << std::endl
        << "TAKER ADDR: " << (!tx->to.empty() && connTo ? connTo->fromXAddr(tx->to) : "") << std::endl
        << "STATE: " << tx->strState() << std::endl
        << "BLOCK HASH: " << tx->blockHash.GetHex() << std::endl
        << "UPDATED AT: " << util::iso8601(tx->txtime) << std::endl
        << "CREATED AT: " << util::iso8601(tx->created) << std::endl
        << "USED INPUTS: " << std::endl << inputsStream.str();

    return out;
}

}
