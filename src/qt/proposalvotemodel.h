// Copyright (c) 2011-2013 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef PROPOSALVOTEMODEL_H
#define PROPOSALVOTEMODEL_H

#include <QAbstractTableModel>
#include <QtGui>

#include <iostream>
#include <sstream>


class ProposalVoteModel : public QAbstractTableModel
{
    Q_OBJECT
public:
    ProposalVoteModel(QObject *parent = 0);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    bool updateData();
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

protected slots:
    void timerHit();

protected:
     std::vector<std::vector<float> > Columns;

public:
     typedef struct propDataTypeTag {
         QString Name;
         QString URL;
         QString Hash;
         QString FeeHash;
         int64_t BlockStart;
         int64_t BlockEnd;
         int64_t TotalPaymentCount;
         int64_t RemainingPaymentCount;
         QString PaymentAddress;
         double Ratio;
         int64_t Yeas;
         int64_t Nays;
         int64_t Abstains;
         float TotalPayment;
         float MonthlyPayment;
         float Alloted;
         float TotalBudgetAlloted;
         bool IsEstablished;
         bool IsValid;
         QString IsValidReason;
         bool fValid;
     }propDataType;

     propDataType propsData[200];

     QString currentSelectionHash;
     int numProposals;

};

#endif // PROPOSALVOTEMODEL_H
