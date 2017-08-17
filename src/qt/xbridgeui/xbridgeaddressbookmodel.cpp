//******************************************************************************
//******************************************************************************

#include "xbridgeaddressbookmodel.h"

#include "xbridge/xuiconnector.h"

#include <assert.h>

//******************************************************************************
//******************************************************************************
XBridgeAddressBookModel::XBridgeAddressBookModel()
{
    m_columns << trUtf8("Currency")
              << trUtf8("Name")
              << trUtf8("Address");

    xuiConnector.NotifyXBridgeAddressBookEntryReceived.connect
            (boost::bind(&XBridgeAddressBookModel::onAddressBookEntryReceived, this, _1, _2, _3));
}

//******************************************************************************
//******************************************************************************
XBridgeAddressBookModel::~XBridgeAddressBookModel()
{
    xuiConnector.NotifyXBridgeAddressBookEntryReceived.disconnect
            (boost::bind(&XBridgeAddressBookModel::onAddressBookEntryReceived, this, _1, _2, _3));
}

//******************************************************************************
//******************************************************************************
int XBridgeAddressBookModel::rowCount(const QModelIndex &) const
{
    return m_addressBook.size();
}

//******************************************************************************
//******************************************************************************
int XBridgeAddressBookModel::columnCount(const QModelIndex &) const
{
    return m_columns.size();
}

//******************************************************************************
//******************************************************************************
QVariant XBridgeAddressBookModel::data(const QModelIndex & idx, int role) const
{
    if (!idx.isValid())
    {
        return QVariant();
    }

    if (idx.row() < 0 || idx.row() >= static_cast<int>(m_addressBook.size()))
    {
        return QVariant();
    }

    if (role == Qt::DisplayRole)
    {
        switch (idx.column())
        {
            case Currency:
            {
                QString text = QString::fromStdString(std::get<Currency>(m_addressBook[idx.row()]));
                return QVariant(text);
            }
            case Name:
            {
                QString text = QString::fromStdString(std::get<Name>(m_addressBook[idx.row()]));
                return QVariant(text);
            }
            case Address:
            {
                QString text = QString::fromStdString(std::get<Address>(m_addressBook[idx.row()]));
                return QVariant(text);
            }
            default:
                return QVariant();
        }
    }

    return QVariant();
}

//******************************************************************************
//******************************************************************************
QVariant XBridgeAddressBookModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if(orientation == Qt::Horizontal)
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
XBridgeAddressBookModel::AddressBookEntry XBridgeAddressBookModel::entry(const int row)
{
    if (row < 0 || row >= static_cast<int>(m_addressBook.size()))
    {
        return AddressBookEntry();
    }

    return m_addressBook[row];
}

//******************************************************************************
//******************************************************************************
void XBridgeAddressBookModel::onAddressBookEntryReceived(const std::string & currency,
                                                         const std::string & name,
                                                         const std::string & address)
{
    if (m_addresses.count(address))
    {
        return;
    }

    m_addresses.insert(address);

    emit beginInsertRows(QModelIndex(), m_addressBook.size(), m_addressBook.size());
    m_addressBook.push_back(std::make_tuple(currency, name, address));
    emit endInsertRows();
}
