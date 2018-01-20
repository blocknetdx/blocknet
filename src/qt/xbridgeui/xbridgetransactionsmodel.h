//******************************************************************************
//******************************************************************************

#ifndef XBRIDGETRANSACTIONSMODEL_H
#define XBRIDGETRANSACTIONSMODEL_H

#include "uint256.h"
#include "xbridge/xbridgetransactiondescr.h"
#include "xbridge/util/xbridgeerror.h"

#include <QAbstractTableModel>
#include <QStringList>
#include <QTimer>
#include <QEvent>

#include <vector>
#include <string>
#include <boost/cstdint.hpp>

//******************************************************************************
//******************************************************************************

const QEvent::Type TRANSACTION_RECEIVED_EVENT = static_cast<QEvent::Type>(QEvent::User + 1);
const QEvent::Type TRANSACTION_STATE_CHANGED_EVENT = static_cast<QEvent::Type>(QEvent::User + 2);
const QEvent::Type TRANSACTION_CANCELLED_EVENT = static_cast<QEvent::Type>(QEvent::User + 3);

class TransactionReceivedEvent : public QEvent
{
    public:
        TransactionReceivedEvent(const XBridgeTransactionDescr & tx):
            QEvent(TRANSACTION_RECEIVED_EVENT),
            tx(tx)
        {
        }

        const XBridgeTransactionDescr tx;
};

class TransactionStateChangedEvent : public QEvent
{
    public:
        TransactionStateChangedEvent(const uint256 & id, const uint32_t state):
            QEvent(TRANSACTION_STATE_CHANGED_EVENT),
            id(id),
            state(state)
        {
        }

        const uint256 id;
        const uint32_t state;
};

class TransactionCancelledEvent : public QEvent
{
    public:
        TransactionCancelledEvent(const uint256 & id, const uint32_t state, const uint32_t reason):
            QEvent(TRANSACTION_CANCELLED_EVENT),
            id(id),
            state(state),
            reason(reason)
        {
        }

        const uint256 id;
        const uint32_t state;
        const uint32_t reason;
};

class XBridgeTransactionsModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    XBridgeTransactionsModel();
    ~XBridgeTransactionsModel();

    enum ColumnIndex
    {
        Total       = 0,
        FirstColumn = Total,
        Size        = 1,
        BID         = 2,
        State       = 3,
        LastColumn  = State
    };

    static const int rawStateRole = Qt::UserRole + 1;

public:
    // static QString   thisCurrency();

    virtual int      rowCount(const QModelIndex &) const;
    virtual int      columnCount(const QModelIndex &) const;
    virtual QVariant data(const QModelIndex & idx, int role) const;
    virtual QVariant headerData(int section, Qt::Orientation orientation, int role) const;

    bool isMyTransaction(const unsigned int index) const;

    xbridge::Error newTransaction(const std::string & from,
                        const std::string & to,
                        const std::string & fromCurrency,
                        const std::string & toCurrency,
                        const double fromAmount,
                        const double toAmount);
    xbridge::Error newTransactionFromPending(const uint256 & id,
                                   const std::vector<unsigned char> & hub,
                                   const std::string & from,
                                   const std::string & to);

    bool cancelTransaction(const uint256 & id);
    bool rollbackTransaction(const uint256 & id);

    XBridgeTransactionDescr item(const unsigned int index) const;

private slots:
    void onTimer();

private:

    void customEvent(QEvent * event);

    void onTransactionReceived(const XBridgeTransactionDescr & tx);
    void onTransactionStateChanged(const uint256 & id, const uint32_t state);
    void onTransactionCancelled(const uint256 & id, const uint32_t state, const uint32_t reason);

    void onTransactionReceivedExtSignal(const XBridgeTransactionDescr & tx);
    void onTransactionStateChangedExtSignal(const uint256 & id, const uint32_t state);
    void onTransactionCancelledExtSignal(const uint256 & id, const uint32_t state, const uint32_t reason);

    QString transactionState(const XBridgeTransactionDescr::State state) const;

    QStringList m_columns;

    std::vector<XBridgeTransactionDescr> m_transactions;

    QTimer m_timer;
};

#endif // XBRIDGETRANSACTIONSMODEL_H
