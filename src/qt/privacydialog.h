// Copyright (c) 2011-2014 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_PRIVACYDIALOG_H
#define BITCOIN_QT_PRIVACYDIALOG_H

#include "guiutil.h"

#include <QDialog>
#include <QHeaderView>
#include <QItemSelection>
#include <QKeyEvent>
#include <QMenu>
#include <QPoint>
#include <QTimer>
#include <QVariant>

class OptionsModel;
class WalletModel;

namespace Ui
{
class PrivacyDialog;
}

QT_BEGIN_NAMESPACE
class QModelIndex;
QT_END_NAMESPACE

/** Dialog for requesting payment of bitcoins */
class PrivacyDialog : public QDialog
{
    Q_OBJECT

public:
    enum ColumnWidths {
        DATE_COLUMN_WIDTH = 130,
        LABEL_COLUMN_WIDTH = 120,
        AMOUNT_MINIMUM_COLUMN_WIDTH = 160,
        MINIMUM_COLUMN_WIDTH = 130
    };

    explicit PrivacyDialog(QWidget* parent = 0);
    ~PrivacyDialog();

    void setModel(WalletModel* model);
    void updateObfuscationProgress();

public slots:
    void setBalance(const CAmount& balance, const CAmount& anonymizedBalance);
    void obfuScationStatus();

protected:
//    virtual void keyPressEvent(QKeyEvent* event);

private:
    Ui::PrivacyDialog* ui;
    QTimer* timer;
    GUIUtil::TableViewLastColumnResizingFixer* columnResizingFixer;
    WalletModel* walletModel;
    QMenu* contextMenu;
    CAmount currentBalance;
    CAmount currentAnonymizedBalance;
    int nDisplayUnit;

private slots:
    void toggleObfuscation();
    void obfuscationAuto();
    void obfuscationReset();
    void updateDisplayUnit();
};

#endif // BITCOIN_QT_PRIVACYDIALOG_H
