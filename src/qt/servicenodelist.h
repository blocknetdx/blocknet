#ifndef SERVICENODELIST_H
#define SERVICENODELIST_H

#include "servicenode.h"
#include "platformstyle.h"
#include "sync.h"
#include "util.h"

#include <QMenu>
#include <QTimer>
#include <QWidget>

#define MY_SERVICENODELIST_UPDATE_SECONDS 60
#define SERVICENODELIST_UPDATE_SECONDS 15
#define SERVICENODELIST_FILTER_COOLDOWN_SECONDS 3

namespace Ui
{
class ServicenodeList;
}

class ClientModel;
class WalletModel;

QT_BEGIN_NAMESPACE
class QModelIndex;
QT_END_NAMESPACE

/** Servicenode Manager page widget */
class ServicenodeList : public QWidget
{
    Q_OBJECT

public:
    explicit ServicenodeList(QWidget* parent = 0);
    ~ServicenodeList();

    void setClientModel(ClientModel* clientModel);
    void setWalletModel(WalletModel* walletModel);
    void StartAlias(std::string strAlias);
    void StartAll(std::string strCommand = "start-all");

private:
    QMenu* contextMenu;
    int64_t nTimeFilterUpdated;
    bool fFilterUpdated;

public Q_SLOTS:
    void updateMyServicenodeInfo(QString strAlias, QString strAddr, CServicenode* pmn);
    void updateMyNodeList(bool fForce = false);

Q_SIGNALS:

private:
    QTimer* timer;
    Ui::ServicenodeList* ui;
    ClientModel* clientModel;
    WalletModel* walletModel;
    CCriticalSection cs_mnlistupdate;
    QString strCurrentFilter;

private Q_SLOTS:
    void showContextMenu(const QPoint&);
    void on_startButton_clicked();
    void on_startAllButton_clicked();
    void on_startMissingButton_clicked();
    void on_tableWidgetMyServicenodes_itemSelectionChanged();
    void on_UpdateButton_clicked();
};
#endif // SERVICENODELIST_H
