//******************************************************************************
//******************************************************************************

#include "xbridgeaddressbookview.h"

#include <QHBoxLayout>
#include <QHeaderView>

//******************************************************************************
//******************************************************************************
XBridgeAddressBookView::XBridgeAddressBookView(QWidget *parent)
    : QDialog(parent, Qt::Dialog)
{
    setupUi();
}

//******************************************************************************
//******************************************************************************
XBridgeAddressBookView::~XBridgeAddressBookView()
{

}

//******************************************************************************
//******************************************************************************
void XBridgeAddressBookView::onAddressSelect(QModelIndex index)
{
    m_selectedAddress  = std::get<XBridgeAddressBookModel::Address>(m_model.entry(index.row()));
    m_selectedCurrency = std::get<XBridgeAddressBookModel::Currency>(m_model.entry(index.row()));
    accept();
}

//******************************************************************************
//******************************************************************************
void XBridgeAddressBookView::setupUi()
{
    m_entryList = new QTableView(this);
    m_entryList->setMinimumWidth(600);
    m_entryList->setModel(&m_model);
    m_entryList->setSelectionBehavior(QAbstractItemView::SelectRows);

    connect(m_entryList, SIGNAL(doubleClicked(QModelIndex)),
            this,        SLOT(onAddressSelect(QModelIndex)));

    QHeaderView * header = m_entryList->horizontalHeader();
    header->resizeSection(XBridgeAddressBookModel::Currency,  80);
    header->resizeSection(XBridgeAddressBookModel::Name,  200);
#if QT_VERSION <0x050000
    header->setResizeMode(XBridgeAddressBookModel::Address, QHeaderView::Stretch);
#else
    header->setSectionResizeMode(XBridgeAddressBookModel::Address, QHeaderView::Stretch);
#endif

    QHBoxLayout * hbox = new QHBoxLayout;
    hbox->addWidget(m_entryList);

    setLayout(hbox);
}
