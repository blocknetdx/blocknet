// Copyright (c) 2011-2015 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_PROPOSALLIST_H
#define BITCOIN_QT_PROPOSALLIST_H

#include "guiutil.h"
#include "proposaltablemodel.h"
#include "columnalignedlayout.h"
#include <QWidget>
#include <QKeyEvent>
#include <QTimer>

class ProposalFilterProxy;

QT_BEGIN_NAMESPACE
class QComboBox;
class QDateTimeEdit;
class QFrame;
class QItemSelectionModel;
class QLineEdit;
class QMenu;
class QModelIndex;
class QSignalMapper;
class QTableView;
QT_END_NAMESPACE

#define PROPOSALLIST_UPDATE_SECONDS 30

class ProposalList : public QWidget
{
    Q_OBJECT

public:
    explicit ProposalList(QWidget *parent = 0);

    void setModel();

    enum DateEnum
    {
        All,
        Today,
        ThisWeek,
        ThisMonth,
        LastMonth,
        ThisYear,
        Range
    };

    enum ColumnWidths {
        PROPOSAL_COLUMN_WIDTH = 150,
        AMOUNT_COLUMN_WIDTH = 150,		
        START_DATE_COLUMN_WIDTH = 100,
        END_DATE_COLUMN_WIDTH = 100,
        YES_VOTES_COLUMN_WIDTH = 100,
        NO_VOTES_COLUMN_WIDTH = 100,
        ABSTAIN_COLUMN_WIDTH = 100,
        VOTES_NEEDED_COLUMN_WIDTH = 150,
        MINIMUM_COLUMN_WIDTH = 23
    };

private:
    ProposalTableModel *proposalTableModel;
    ProposalFilterProxy *proposalProxyModel;
    QTableView *proposalList;
    int64_t nLastUpdate = 0;

    QLineEdit *proposalWidget;
    QLineEdit *startDateWidget;
    QLineEdit *endDateWidget;
    QTimer *timer;

    QLineEdit *yesVotesWidget;
    QLineEdit *noVotesWidget;
    QLineEdit *abstainVotesWidget;
    QLineEdit *amountWidget;
    QLineEdit *votesNeededWidget;
    QLabel *secondsLabel;

    QMenu *contextMenu;

    //LineEdit *startDateRangeWidget;
    QLineEdit *proposalStartDate;

    //QLineEdit *endDateRangeWidget;
    QLineEdit *proposalEndDate;
    ColumnAlignedLayout *hlayout;

    //QWidget *createStartDateRangeWidget();
    //QWidget *createEndDateRangeWidget();
    void vote_click_handler(const std::string voteString);

    GUIUtil::TableViewLastColumnResizingFixer *columnResizingFixer;

    virtual void resizeEvent(QResizeEvent* event);

private Q_SLOTS:
    void contextualMenu(const QPoint &);
    //void startDateRangeChanged();
    //void endDateRangeChanged();
    void voteYes();
    void voteNo();
    void voteAbstain();
    void openProposalUrl();
    void invalidateAlignedLayout();

Q_SIGNALS:
    void doubleClicked(const QModelIndex&);

public Q_SLOTS:
    void refreshProposals(bool force = false);
    void changedProposal(const QString &proposal);
    void chooseStartDate(const QString &startDate);
    void chooseEndDate(const QString &endDate);
    void changedYesVotes(const QString &minYesVotes);
    void changedNoVotes(const QString &minNoVotes);
    void changedAbstainVotes(const QString &minAbstainVotes);
    void changedVotesNeeded(const QString &votesNeeded);
    void changedAmount(const QString &minAmount);

};

#endif // BITCOIN_QT_PROPOSALLIST_H
