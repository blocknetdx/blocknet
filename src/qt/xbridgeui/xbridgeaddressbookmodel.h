//******************************************************************************
//******************************************************************************

#ifndef XBRIDGEADDRESSBOOKMODEL_H
#define XBRIDGEADDRESSBOOKMODEL_H

#include <QAbstractTableModel>
#include <QStringList>

#include <string>
#include <tuple>
#include <vector>
#include <set>

//******************************************************************************
//******************************************************************************
class XBridgeAddressBookModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    XBridgeAddressBookModel();
    ~XBridgeAddressBookModel();

    typedef std::tuple<std::string, std::string, std::string> AddressBookEntry;
    typedef std::vector<AddressBookEntry> AddressBook;

    enum ColumnIndex
    {
        Currency     = 0,
        FirstColumn  = Currency,
        Name         = 1,
        Address      = 2,
        LastColumn   = Address
    };

    virtual int      rowCount(const QModelIndex &) const;
    virtual int      columnCount(const QModelIndex &) const;
    virtual QVariant data(const QModelIndex & idx, int role) const;
    virtual QVariant headerData(int section, Qt::Orientation orientation, int role) const;

    AddressBookEntry entry(const int row);

private:
    void onAddressBookEntryReceived(const std::string & currency,
                                    const std::string & name,
                                    const std::string & address);

private:
    QStringList m_columns;

    std::set<std::string> m_addresses;
    AddressBook m_addressBook;
};

#endif // XBRIDGEADDRESSBOOKMODEL_H
