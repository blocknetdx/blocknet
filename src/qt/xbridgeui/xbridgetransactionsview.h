//******************************************************************************
//******************************************************************************

#ifndef XBRIDGETRANSACTIONSVIEW_H
#define XBRIDGETRANSACTIONSVIEW_H

#include "xbridgetransactionsmodel.h"
#include "xbridgetransactiondialog.h"

// #include "../walletmodel.h"

#include <QWidget>
#include <QMenu>
#include <QSortFilterProxyModel>

class QTableView;
class QTextEdit;

//******************************************************************************
//******************************************************************************
class StateSortFilterProxyModel : public QSortFilterProxyModel
{
    Q_OBJECT

public:
    StateSortFilterProxyModel(QObject *parent = 0) : QSortFilterProxyModel(parent) {}

    void setAcceptedStates(const QList<xbridge::TransactionDescr::State> &acceptedStates)
    {
        m_acceptedStates = acceptedStates;
    }

protected:
    bool filterAcceptsRow(int source_row, const QModelIndex &source_parent) const override
    {
        QModelIndex index = sourceModel()->index(source_row, XBridgeTransactionsModel::State, source_parent);
        QVariant stateVariant = sourceModel()->data(index, XBridgeTransactionsModel::rawStateRole);

        if(stateVariant.isValid())
        {
            bool ok;
            int stateValue = stateVariant.toInt(&ok);

            if(ok)
            {
                xbridge::TransactionDescr::State transactionState = static_cast<xbridge::TransactionDescr::State>(stateValue);
                return m_acceptedStates.contains(transactionState);
            }
        }

        return false;
    }

private:
    QList<xbridge::TransactionDescr::State> m_acceptedStates;

};


//******************************************************************************
//******************************************************************************
class XBridgeTransactionsView : public QWidget
{
    Q_OBJECT
public:
    explicit XBridgeTransactionsView(QWidget *parent = 0);

private:
    void setupUi();
    QMenu * setupContextMenu(QModelIndex & index);

private slots:
    void onNewTransaction();
    void onAcceptTransaction();
    void onCancelTransaction();
    void onRollbackTransaction();

    void onContextMenu(QPoint pt);

    void onToggleHideHistoricTransactions();

private:
    // WalletModel            * m_walletModel;

    XBridgeTransactionsModel    m_txModel;
    StateSortFilterProxyModel   m_transactionsProxy;
    StateSortFilterProxyModel   m_historicTransactionsProxy;

    XBridgeTransactionDialog m_dlg;

    QTableView  * m_transactionsList;
    QTableView  * m_historicTransactionsList;

    QModelIndex   m_contextMenuIndex;
};

#endif // XBRIDGETRANSACTIONSVIEW_H
