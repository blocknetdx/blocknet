//******************************************************************************
//******************************************************************************

#include "xbridgetransactionsmodel.h"
#include "xbridge/xbridgetransaction.h"
#include "xbridge/xbridgeapp.h"
// #include "xbridgeconnector.h"
#include "xbridge/xuiconnector.h"
#include "xbridge/util/xutil.h"
#include "xbridge/xbridgedef.h"

#include <boost/date_time/posix_time/posix_time.hpp>

//******************************************************************************
//******************************************************************************
XBridgeTransactionsModel::XBridgeTransactionsModel()
{
    m_columns << trUtf8("TOTAL")
              << trUtf8("SIZE")
              << trUtf8("BID")
              << trUtf8("STATE");

    xuiConnector.NotifyXBridgeTransactionReceived.connect
            (boost::bind(&XBridgeTransactionsModel::onTransactionReceived, this, _1));

    xuiConnector.NotifyXBridgeTransactionStateChanged.connect
            (boost::bind(&XBridgeTransactionsModel::onTransactionStateChanged, this, _1));

    connect(&m_timer, SIGNAL(timeout()), this, SLOT(onTimer()));
    m_timer.start(3000);
}

//******************************************************************************
//******************************************************************************
XBridgeTransactionsModel::~XBridgeTransactionsModel()
{
    xuiConnector.NotifyXBridgeTransactionReceived.disconnect
            (boost::bind(&XBridgeTransactionsModel::onTransactionReceived, this, _1));

    xuiConnector.NotifyXBridgeTransactionStateChanged.disconnect
            (boost::bind(&XBridgeTransactionsModel::onTransactionStateChanged, this, _1));
}

//******************************************************************************
//******************************************************************************
int XBridgeTransactionsModel::rowCount(const QModelIndex &) const
{
    return m_transactions.size();
}

//******************************************************************************
//******************************************************************************
int XBridgeTransactionsModel::columnCount(const QModelIndex &) const
{
    return m_columns.size();
}

//******************************************************************************
//******************************************************************************
QVariant XBridgeTransactionsModel::data(const QModelIndex & idx, int role) const
{
    if (!idx.isValid())
    {
        return QVariant();
    }

    if (idx.row() < 0 || idx.row() >= static_cast<int>(m_transactions.size()))
    {
        return QVariant();
    }

    const xbridge::TransactionDescrPtr & d = m_transactions[idx.row()];

    if (role == Qt::DisplayRole)
    {
        switch (idx.column())
        {
            case Total:
            {
                double amount = (double)d->fromAmount / xbridge::TransactionDescr::COIN;
                QString text = QString("%1 %2").arg(QString::number(amount, 'f', 12).remove(QRegExp("\\.?0+$"))).arg(QString::fromStdString(d->fromCurrency));

                return QVariant(text);
            }
            case Size:
            {
                double amount = (double)d->toAmount / xbridge::TransactionDescr::COIN;
                QString text = QString("%1 %2").arg(QString::number(amount, 'f', 12).remove(QRegExp("\\.?0+$"))).arg(QString::fromStdString(d->toCurrency));

                return QVariant(text);
            }
            case BID:
            {
                double amountTotal = (double)d->fromAmount / xbridge::TransactionDescr::COIN;
                double amountSize = (double)d->toAmount / xbridge::TransactionDescr::COIN;
                double bid = amountTotal / amountSize;
                QString text = QString::number(bid, 'f', 12).remove(QRegExp("\\.?0+$"));

                return QVariant(text);
            }
            case State:
            {
                return QVariant(transactionState(d->state));
            }

            default:
                return QVariant();
        }
    }

    if(role == rawStateRole)
    {
        if(idx.column() == State)
            return QVariant(d->state);
        else
            return QVariant();
    }

    return QVariant();
}

//******************************************************************************
//******************************************************************************
QVariant XBridgeTransactionsModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal)
    {
        if (role == Qt::DisplayRole)
        {
            return m_columns[section];
        }
    }
    return QVariant();
}

//******************************************************************************
//******************************************************************************
xbridge::TransactionDescrPtr XBridgeTransactionsModel::item(const unsigned int index) const
{
    if (index >= m_transactions.size())
    {
        xbridge::TransactionDescrPtr dummy;
        dummy->state = xbridge::TransactionDescr::trInvalid;
        return dummy;
    }

    return m_transactions[index];
}

//******************************************************************************
//******************************************************************************
bool XBridgeTransactionsModel::isMyTransaction(const unsigned int index) const
{
    if (index >= m_transactions.size())
    {
        return false;
    }

    return m_transactions[index]->from.size();
}

//******************************************************************************
//******************************************************************************
bool XBridgeTransactionsModel::newTransaction(const std::string & from,
                                              const std::string & to,
                                              const std::string & fromCurrency,
                                              const std::string & toCurrency,
                                              const double fromAmount,
                                              const double toAmount)
{
    xbridge::App & xapp = xbridge::App::instance();
    xbridge::WalletConnectorPtr connFrom = xapp.connectorByCurrency(fromCurrency);
    xbridge::WalletConnectorPtr connTo   = xapp.connectorByCurrency(toCurrency);
    if (!connFrom || !connTo)
    {
        return false;
    }

    if (connFrom->minAmount() > fromAmount)
    {
        return false;
    }

    // TODO check amount
    uint256 id = xbridge::App::instance().sendXBridgeTransaction
            (from, fromCurrency, (uint64_t)(fromAmount * xbridge::TransactionDescr::COIN),
             to,   toCurrency,   (uint64_t)(toAmount * xbridge::TransactionDescr::COIN));

    if (id != uint256())
    {
        xbridge::TransactionDescrPtr d = xapp.transaction(id);
        onTransactionReceived(d);
    }

    return true;
}

//******************************************************************************
//******************************************************************************
bool XBridgeTransactionsModel::newTransactionFromPending(const uint256 & id,
                                                         const std::vector<unsigned char> & hub,
                                                         const std::string & from,
                                                         const std::string & to)
{
    unsigned int i = 0;
    for (; i < m_transactions.size(); ++i)
    {
        if (m_transactions[i]->id == id && m_transactions[i]->hubAddress == hub)
        {
            // found
            xbridge::TransactionDescrPtr & d = m_transactions[i];

            std::swap(d->fromCurrency, d->toCurrency);
            std::swap(d->fromAmount, d->toAmount);

            xbridge::App & xapp = xbridge::App::instance();
            xbridge::WalletConnectorPtr connFrom = xapp.connectorByCurrency(d->fromCurrency);
            xbridge::WalletConnectorPtr connTo   = xapp.connectorByCurrency(d->toCurrency);
            if (!connFrom || !connTo)
            {
                continue;
            }

            d->from  = connFrom->toXAddr(from);
            d->to    = connTo->toXAddr(to);

            emit dataChanged(index(i, FirstColumn), index(i, LastColumn));

            // send tx
            d->id = xbridge::App::instance().acceptXBridgeTransaction(d->id, from, to);

            d->txtime = boost::posix_time::second_clock::universal_time();

            break;
        }
    }

    if (i == m_transactions.size())
    {
        // not found...assert ?
        return false;
    }

    // remove all other tx with this id
    for (unsigned int i = 0; i < m_transactions.size(); ++i)
    {
        if (m_transactions[i]->id == id && m_transactions[i]->hubAddress != hub)
        {
            emit beginRemoveRows(QModelIndex(), i, i);
            m_transactions.erase(m_transactions.begin() + i);
            emit endRemoveRows();

            --i;
        }
    }

    return true;
}

//******************************************************************************
//******************************************************************************
bool XBridgeTransactionsModel::cancelTransaction(const uint256 & id)
{
    return xbridge::App::instance().cancelXBridgeTransaction(id, crUserRequest);
}

//******************************************************************************
//******************************************************************************
bool XBridgeTransactionsModel::rollbackTransaction(const uint256 & id)
{
    return xbridge::App::instance().rollbackXBridgeTransaction(id);
}

//******************************************************************************
//******************************************************************************
void XBridgeTransactionsModel::onTimer()
{
    // check pending transactions
    for (unsigned int i = 0; i < m_transactions.size(); ++i)
    {
        boost::posix_time::time_duration td =
                boost::posix_time::second_clock::universal_time() -
                m_transactions[i]->txtime;

        if (m_transactions[i]->state == xbridge::TransactionDescr::trNew &&
                td.total_seconds() > xbridge::Transaction::TTL/60)
        {
            m_transactions[i]->state = xbridge::TransactionDescr::trOffline;
            emit dataChanged(index(i, FirstColumn), index(i, LastColumn));
        }
        else if (m_transactions[i]->state == xbridge::TransactionDescr::trPending &&
                td.total_seconds() > xbridge::Transaction::TTL/6)
        {
            m_transactions[i]->state = xbridge::TransactionDescr::trExpired;
            emit dataChanged(index(i, FirstColumn), index(i, LastColumn));
        }
        else if ((m_transactions[i]->state == xbridge::TransactionDescr::trExpired ||
                  m_transactions[i]->state == xbridge::TransactionDescr::trOffline) &&
                         td.total_seconds() < xbridge::Transaction::TTL/6)
        {
            m_transactions[i]->state = xbridge::TransactionDescr::trPending;
            emit dataChanged(index(i, FirstColumn), index(i, LastColumn));
        }
        else if (m_transactions[i]->state == xbridge::TransactionDescr::trExpired &&
                td.total_seconds() > xbridge::Transaction::TTL)
        {
            emit beginRemoveRows(QModelIndex(), i, i);
            m_transactions.erase(m_transactions.begin()+i);
            emit endRemoveRows();
            --i;
        }
    }
}

//******************************************************************************
//******************************************************************************
void XBridgeTransactionsModel::onTransactionReceived(const xbridge::TransactionDescrPtr & tx)
{
    for (unsigned int i = 0; i < m_transactions.size(); ++i)
    {
        const xbridge::TransactionDescrPtr & descr = m_transactions.at(i);
        if (descr->id != tx->id)
        {
            continue;
        }

        if (isMyTransaction(i))
        {
            // transaction with id - is owned, not processed more
            return;
        }

        // found
//        if (descr->from.size() == 0)
//        {
//            m_transactions[i] = tx;
//        }

//        else if (descr->state < tx->state)
//        {
//            m_transactions[i]->state = tx->state;
//        }

//        // update timestamp
//        m_transactions[i]->txtime = tx->txtime;

        emit dataChanged(index(i, FirstColumn), index(i, LastColumn));
        return;
    }

    // skip tx with other currencies
    // std::string tmp = thisCurrency().toStdString();
    // if (tx.fromCurrency != tmp && tx.toCurrency != tmp)
    // {
    //     return;
    // }

    emit beginInsertRows(QModelIndex(), 0, 0);
    m_transactions.insert(m_transactions.begin(), 1, tx);
    // std::sort(m_transactions.begin(), m_transactions.end(), std::greater<XBridgeTransactionDescr>());
    emit endInsertRows();
}

//******************************************************************************
//******************************************************************************
void XBridgeTransactionsModel::onTransactionStateChanged(const uint256 & id)
{
    for (unsigned int i = 0; i < m_transactions.size(); ++i)
    {
        if (m_transactions[i]->id == id)
        {
            // found
            emit dataChanged(index(i, FirstColumn), index(i, LastColumn));
            return;
        }
    }
}

//******************************************************************************
//******************************************************************************
QString XBridgeTransactionsModel::transactionState(const xbridge::TransactionDescr::State state) const
{
    switch (state)
    {
        case xbridge::TransactionDescr::trInvalid:   return trUtf8("Invalid");
        case xbridge::TransactionDescr::trNew:       return trUtf8("New");
        case xbridge::TransactionDescr::trPending:   return trUtf8("Open");
        case xbridge::TransactionDescr::trAccepting: return trUtf8("Accepting");
        case xbridge::TransactionDescr::trHold:      return trUtf8("Hold");
        case xbridge::TransactionDescr::trCreated:   return trUtf8("Created");
        case xbridge::TransactionDescr::trSigned:    return trUtf8("Signed");
        case xbridge::TransactionDescr::trCommited:  return trUtf8("Commited");
        case xbridge::TransactionDescr::trFinished:  return trUtf8("Finished");
        case xbridge::TransactionDescr::trCancelled: return trUtf8("Cancelled");
        case xbridge::TransactionDescr::trRollback:  return trUtf8("Rolled Back");
        case xbridge::TransactionDescr::trDropped:   return trUtf8("Dropped");
        case xbridge::TransactionDescr::trExpired:   return trUtf8("Expired");
        case xbridge::TransactionDescr::trOffline:   return trUtf8("Offline");
        default:                                     return trUtf8("Unknown");
    }
}
