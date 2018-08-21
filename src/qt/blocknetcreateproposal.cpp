// Copyright (c) 2018 The Blocknet Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "blocknetcreateproposal.h"

#include <QEvent>
#include <QLineEdit>

enum BCreateProposalCrumbs {
    CREATE = 1,
    REVIEW,
    VOTE,
};

BlocknetCreateProposal::BlocknetCreateProposal(WalletModel *w, QFrame *parent) : QFrame(parent), walletModel(w),
                                                                       layout(new QVBoxLayout) {
    this->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    layout->setContentsMargins(QMargins());
    this->setLayout(layout);

    page1 = new BlocknetCreateProposal1(walletModel, CREATE);
    page2 = new BlocknetCreateProposal2(walletModel, REVIEW);
    pages = { page1, page2 };

    //done = new BlocknetSendFundsDone;
    //done->hide();

    breadCrumb = new BlocknetBreadCrumb;
    breadCrumb->setParent(this);
    breadCrumb->addCrumb(tr("Create Proposal"), CREATE);
    breadCrumb->addCrumb(tr("Review Proposal"), REVIEW);
    breadCrumb->show();

    connect(breadCrumb, SIGNAL(crumbChanged(int)), this, SLOT(crumbChanged(int)));
    connect(page1, SIGNAL(next(int)), this, SLOT(nextCrumb(int)));
    connect(page2, SIGNAL(next(int)), this, SLOT(nextCrumb(int)));
    connect(page2, SIGNAL(back(int)), this, SLOT(prevCrumb(int)));
    connect(page1, SIGNAL(cancel(int)), this, SLOT(onCancel(int)));
    connect(page2, SIGNAL(cancel(int)), this, SLOT(onCancel(int)));

    // Estimated position
    positionCrumb(QPoint(175, -4));
    breadCrumb->goToCrumb(CREATE);
}

void BlocknetCreateProposal::crumbChanged(int crumb) {
    //if (screen && crumb > breadCrumb->getCrumb() && breadCrumb->showCrumb(breadCrumb->getCrumb()) /*&& !screen->validated()*/)
      //  return;
    breadCrumb->showCrumb(crumb);

    if (screen) {
        layout->removeWidget(screen);
        screen->hide();
    }

    /*if (!done->isHidden()) {
        layout->removeWidget(done);
        done->hide();
    }*/

    switch(crumb) {
        case CREATE:
            screen = page1;
            break;
        case REVIEW:
            screen = page2;
            break;
        default:
            return;
    }

    layout->addWidget(screen);

    positionCrumb();
    screen->show();
    screen->setFocus();
}

void BlocknetCreateProposal::nextCrumb(int crumb) {
    if (screen && crumb > breadCrumb->getCrumb() && breadCrumb->showCrumb(breadCrumb->getCrumb()) /*&& !screen->validated()*/)
        return;
    if (crumb >= REVIEW) // do nothing if at the end
        return;
    breadCrumb->goToCrumb(++crumb);
}

void BlocknetCreateProposal::prevCrumb(int crumb) {
    if (!screen)
        return;
    if (crumb <= CREATE) // do nothing if at the beginning
        return;
    breadCrumb->goToCrumb(--crumb);
}

void BlocknetCreateProposal::onCancel(int crumb) {
    clear();
    breadCrumb->goToCrumb(CREATE);
    //emit dashboard();
}

void BlocknetCreateProposal::onDoneDashboard() {
    //model->reset();
    for (BlocknetCreateProposalPage *page : pages)
        page->clear();
    breadCrumb->goToCrumb(CREATE);
    //emit dashboard();
}

bool BlocknetCreateProposal::event(QEvent *event) {
    if (screen && event->type() == QEvent::LayoutRequest) {
        positionCrumb();
    } else if (event->type() == QEvent::Type::MouseButtonPress) {
        auto *w = this->focusWidget();
        if (dynamic_cast<const QLineEdit*>(w) != nullptr && w->hasFocus() && !w->underMouse())
            w->clearFocus();
    }
    return QFrame::event(event);
}

void BlocknetCreateProposal::reset() {
    //model->reset();
    for (BlocknetCreateProposalPage *page : pages)
        page->clear();
    breadCrumb->goToCrumb(CREATE);
}

void BlocknetCreateProposal::positionCrumb(QPoint pt) {
    if (pt != QPoint() || pt.x() > 250 || pt.y() > 0) {
        breadCrumb->move(pt);
        breadCrumb->raise();
        return;
    }
    auto *pageHeading = screen->findChild<QWidget*>("h4", Qt::FindDirectChildrenOnly);
    QPoint npt = pageHeading->mapToGlobal(QPoint(pageHeading->width(), 0));
    npt = this->mapFromGlobal(npt);
    breadCrumb->move(npt.x() + 20, npt.y() + pageHeading->height() - breadCrumb->height() - 2);
    breadCrumb->raise();
}

void BlocknetCreateProposal::goToDone() {
    layout->removeWidget(screen);
    screen->hide();
    //layout->addWidget(done);
    //done->show();
}

