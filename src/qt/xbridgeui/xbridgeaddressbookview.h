//******************************************************************************
//******************************************************************************

#ifndef XBRIDGEADDRESSBOOKVIEW_H
#define XBRIDGEADDRESSBOOKVIEW_H

#include "xbridgeaddressbookmodel.h"

#include <QDialog>
#include <QTableView>

//******************************************************************************
//******************************************************************************
class XBridgeAddressBookView : public QDialog
{
    Q_OBJECT
public:
    explicit XBridgeAddressBookView(QWidget *parent = 0);
    ~XBridgeAddressBookView();

    std::string selectedAddress() const { return m_selectedAddress; }
    std::string selectedCurrency() const { return m_selectedCurrency; }


private slots:
    void onAddressSelect(QModelIndex index);

private:
    void setupUi();

private:
    std::string m_selectedAddress;
    std::string m_selectedCurrency;

    XBridgeAddressBookModel m_model;

    QTableView  * m_entryList;
};

#endif // XBRIDGEADDRESSBOOKVIEW_H
