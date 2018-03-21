// Copyright (c) 2011-2013 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.


#include "proposalvotemodel.h"


ProposalVoteModel::ProposalVoteModel(QObject *parent):QAbstractTableModel(parent)
{
    numProposals = 0;
}


void ProposalVoteModel::timerHit()
{
    //we identify the top left cell
    QModelIndex topLeft = createIndex(0,0);
    //emit a signal to make the view reread identified data
    emit dataChanged(topLeft, topLeft);
}


int ProposalVoteModel::rowCount(const QModelIndex & /*parent*/) const
{
   return numProposals;
}


int ProposalVoteModel::columnCount(const QModelIndex & /*parent*/) const
{
    return 20;
}


QVariant ProposalVoteModel::data(const QModelIndex &index, int role) const
{
    int row = index.row();
    int col = index.column();

    if (numProposals == 0) return QVariant();

    if (role == Qt::DisplayRole)
    {
        switch (col) {
            case 0:
            {
                return propsData[row].Name;
            }
            case 1:
            {
                return propsData[row].URL;
            }
            case 2:
            {
                return propsData[row].Hash;
            }
            case 3:
            {
                return propsData[row].FeeHash;
            }
            case 4:
            {
                return QVariant((int)propsData[row].BlockStart).toString();
            }
            case 5:
            {
                return QVariant((int)propsData[row].BlockEnd).toString();
            }
            case 6:
            {
                return QVariant((int)propsData[row].TotalPaymentCount).toString();
            }

            case 7:
            {
                return QVariant((int)propsData[row].RemainingPaymentCount).toString();
            }
            case 8:
            {
                return propsData[row].PaymentAddress;
            }
            case 9:
            {
                return QVariant((double)propsData[row].Ratio).toString();
            }
            case 10:
            {
                return QVariant((int)propsData[row].Yeas).toString(); //QString::number(propsData[row].Yeas);
            }
            case 11:
            {
                return QVariant((int)propsData[row].Nays).toString(); //QString::number(propsData[row].Abstains);
            }
            case 12:
            {
                return QVariant((int)propsData[row].Abstains).toString(); //QString::number(propsData[row].Abstains);
            }
            case 13:
            {
                return QVariant((float)propsData[row].TotalPayment).toString();
            }
            case 14:
            {
                return QVariant((float)propsData[row].MonthlyPayment).toString();
            }
            case 15:
            {
                return QVariant((float)propsData[row].Alloted).toString();
            }
            case 16:
            {
                return QVariant((bool)propsData[row].IsEstablished).toString();
            }
            case 17:
            {
                return QVariant((bool)propsData[row].IsValid).toString();
            }
            case 18:
            {
                return propsData[row].IsValidReason;
            }
            case 19:
            {
                return QVariant((bool)propsData[row].fValid).toString();
            }

        }

    }
    return QVariant();
}


bool ProposalVoteModel::updateData()
{
    // Update data...

    QModelIndex topLeft = index(0, 0);
    QModelIndex bottomRight = index(rowCount() - 1, columnCount() - 1);

    emit dataChanged(topLeft, bottomRight);
    emit layoutChanged();

    return true;
}


QVariant ProposalVoteModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if(role == Qt::DisplayRole)
    {
        std::stringstream ss;
        if (orientation == Qt::Horizontal)
        {
        switch (section)
        {
            case 0:
                ss << "Name";
                return QString(ss.str().c_str());
            case 1:
                return QString("URL");
            case 2:
                return QString("Hash");
            case 3:
                return QString("FeeHash");
            case 4:
                return QString("BlockStart");
            case 5:
                return QString("BlockEnd");
            case 6:
                return QString("TotalPaymentCount");
            case 7:
                return QString("RemainingPaymentCount");
            case 8:
                return QString("PaymentAddress");
            case 9:
                return QString("Ratio");
            case 10:
                return QString("Yeas");
            case 11:
                return QString("Nays");
            case 12:
                return QString("Abstains");
            case 13:
                return QString("TotalPayment");
            case 14:
                return QString("MonthlyPayment");
            case 15:
                return QString("Alloted");
            case 16:
                return QString("IsEstablished");
            case 17:
                return QString("IsValid");
            case 18:
                return QString("IsValidReason");
            case 19:
                return QString("fValid");

        }

      }

        else if(orientation == Qt::Vertical)
        {
            ss << "V_" << section;
            return QString(ss.str().c_str());
        }

    }

  return QVariant::Invalid;
}
