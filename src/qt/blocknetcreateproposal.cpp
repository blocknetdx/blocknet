// Copyright (c) 2018-2020 The Blocknet developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/blocknetcreateproposal.h>

#include <qt/blocknetguiutil.h>

#include <QEvent>
#include <QLineEdit>

enum BlocknetCreateProposalCrumbs {
    CREATE = 1,
    REVIEW,
    SUBMIT,
};

BlocknetCreateProposal::BlocknetCreateProposal(QWidget *parent) : QFrame(parent), layout(new QVBoxLayout) {
    this->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    layout->setContentsMargins(QMargins());
    this->setLayout(layout);

    page1 = new BlocknetCreateProposal1(CREATE);
    page2 = new BlocknetCreateProposal2(REVIEW);
    page3 = new BlocknetCreateProposal3(SUBMIT);
    pages = { page1, page2, page3 };

    breadCrumb = new BlocknetBreadCrumb;
    breadCrumb->setParent(this);
    breadCrumb->addCrumb(tr("Create Proposal"), CREATE);
    breadCrumb->addCrumb(tr("Pay Submission Fee"), REVIEW);
    breadCrumb->addCrumb(tr("Submit Proposal"), SUBMIT);
    breadCrumb->show();

    connect(breadCrumb, &BlocknetBreadCrumb::crumbChanged, this, &BlocknetCreateProposal::crumbChanged);
    connect(page1, &BlocknetCreateProposalPage::next, this, &BlocknetCreateProposal::nextCrumb);
    connect(page2, &BlocknetCreateProposalPage::next, this, &BlocknetCreateProposal::nextCrumb);
    connect(page3, &BlocknetCreateProposalPage::next, this, &BlocknetCreateProposal::nextCrumb);
    connect(page2, &BlocknetCreateProposalPage::back, this, &BlocknetCreateProposal::prevCrumb);
    connect(page1, &BlocknetCreateProposalPage::cancel, this, &BlocknetCreateProposal::onCancel);
    connect(page2, &BlocknetCreateProposalPage::cancel, this, &BlocknetCreateProposal::onCancel);
    connect(page3, &BlocknetCreateProposalPage::cancel, this, &BlocknetCreateProposal::onCancel);
    connect(page3, &BlocknetCreateProposal3::done, this, &BlocknetCreateProposal::onDone);

    // Estimated position
    positionCrumb(QPoint(BGU::spi(175), BGU::spi(-4)));
    breadCrumb->goToCrumb(CREATE);
}

void BlocknetCreateProposal::focusInEvent(QFocusEvent *event) {
    QWidget::focusInEvent(event);
    if (screen)
        screen->setFocus();
}

void BlocknetCreateProposal::showEvent(QShowEvent *event) {
    QWidget::showEvent(event);
    if (screen)
        screen->setFocus();
}

void BlocknetCreateProposal::crumbChanged(int crumb) {
    // Prevent users from jumping around the crumb widget without validating previous pages
    auto validatePages = [](const int toPage, const QVector<BlocknetCreateProposalPage*> &pages) -> bool {
        if (toPage - 1 > pages.count())
            return false;
        for (int i = 0; i < toPage - 1; ++i) {
            auto *page = pages[i];
            if (!page->validated())
                return false;
        }
        return true;
    };

    if (screen && crumb > breadCrumb->getCrumb() && breadCrumb->showCrumb(breadCrumb->getCrumb()) && !validatePages(crumb, pages))
        return;
    breadCrumb->showCrumb(crumb);

    if (screen) {
        layout->removeWidget(screen);
        screen->hide();
    }

    breadCrumb->setEnabled(true);

    switch(crumb) {
        case CREATE:
            screen = page1;
            break;
        case REVIEW:
            screen = page2;
            page2->setModel(page1->getModel());
            break;
        case SUBMIT:
            screen = page3;
            page3->setModel(page2->getModel());
            breadCrumb->setEnabled(false); // don't let the user change screens here (unless explicit cancel), to prevent them from inadvertently losing fee hash
            break;
        default:
            return;
    }

    layout->addWidget(screen);

    positionCrumb();
    screen->setWalletModel(walletModel);
    screen->show();
    screen->setFocus();
}

void BlocknetCreateProposal::nextCrumb(int crumb) {
    if (screen && crumb > breadCrumb->getCrumb() && breadCrumb->showCrumb(breadCrumb->getCrumb()) && !screen->validated())
        return;
    if (crumb >= SUBMIT) // do nothing if at the end
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
    reset();
    Q_EMIT done();
}

void BlocknetCreateProposal::onDone() {
    reset();
    Q_EMIT done();
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
    for (BlocknetCreateProposalPage *page : pages)
        page->clear();
    breadCrumb->goToCrumb(CREATE);
}

void BlocknetCreateProposal::positionCrumb(QPoint pt) {
    if (pt != QPoint() || pt.x() > BGU::spi(250) || pt.y() > 0) {
        breadCrumb->move(pt);
        breadCrumb->raise();
        return;
    }
    auto *pageHeading = screen->findChild<QWidget*>("h4", Qt::FindDirectChildrenOnly);
    QPoint npt = pageHeading->mapToGlobal(QPoint(pageHeading->width(), 0));
    npt = this->mapFromGlobal(npt);
    breadCrumb->move(npt.x() + BGU::spi(20), npt.y() + pageHeading->height() - breadCrumb->height() - BGU::spi(2));
    breadCrumb->raise();
}

void BlocknetCreateProposal::goToDone() {
    layout->removeWidget(screen);
    screen->hide();
}

