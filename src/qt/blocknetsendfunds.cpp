// Copyright (c) 2018 The Blocknet Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "blocknetsendfunds.h"
#include "blocknetsendfundsrequest.h"

#include "optionsmodel.h"

#include <QTimer>
#include <QEvent>
#include <QMessageBox>
#include <QList>
#include <QDebug>

enum BSendFundsCrumbs {
    RECIPIENT = 1,
    AMOUNT,
    TRANSACTION_FEE,
    REVIEW_PAYMENT,
    DONE,
};

BlocknetSendFunds::BlocknetSendFunds(WalletModel *w, QFrame *parent) : QFrame(parent), walletModel(w),
                                                                       layout(new QVBoxLayout),
                                                                       model(new BlocknetSendFundsModel) {
//    this->setStyleSheet("border: 1px solid red");
    this->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    layout->setContentsMargins(QMargins());
    this->setLayout(layout);

    page1 = new BlocknetSendFunds1(walletModel, RECIPIENT);
    page2 = new BlocknetSendFunds2(walletModel, AMOUNT);
    page3 = new BlocknetSendFunds3(walletModel, TRANSACTION_FEE);
    page4 = new BlocknetSendFunds4(walletModel, REVIEW_PAYMENT);
    pages = { page1, page2, page3, page4 };

    done = new BlocknetSendFundsDone;
    done->hide();

    breadCrumb = new BlocknetBreadCrumb;
    breadCrumb->setParent(this);
    breadCrumb->addCrumb(tr("Recipient"), RECIPIENT);
    breadCrumb->addCrumb(tr("Amount"), AMOUNT);
    breadCrumb->addCrumb(tr("Transaction Fee"), TRANSACTION_FEE);
    breadCrumb->addCrumb(tr("Review Payment"), REVIEW_PAYMENT);
    breadCrumb->show();

    connect(breadCrumb, SIGNAL(crumbChanged(int)), this, SLOT(crumbChanged(int)));
    connect(page1, SIGNAL(next(int)), this, SLOT(nextCrumb(int)));
    connect(page2, SIGNAL(next(int)), this, SLOT(nextCrumb(int)));
    connect(page3, SIGNAL(next(int)), this, SLOT(nextCrumb(int)));
    connect(page2, SIGNAL(back(int)), this, SLOT(prevCrumb(int)));
    connect(page3, SIGNAL(back(int)), this, SLOT(prevCrumb(int)));
    connect(page4, SIGNAL(back(int)), this, SLOT(prevCrumb(int)));
    connect(page1, SIGNAL(cancel(int)), this, SLOT(onCancel(int)));
    connect(page2, SIGNAL(cancel(int)), this, SLOT(onCancel(int)));
    connect(page3, SIGNAL(cancel(int)), this, SLOT(onCancel(int)));
    connect(page4, SIGNAL(cancel(int)), this, SLOT(onCancel(int)));
    connect(page4, SIGNAL(edit()), this, SLOT(onEdit()));
    connect(page4, SIGNAL(submit()), this, SLOT(onSendFunds()));
    connect(done, SIGNAL(dashboard()), this, SLOT(onDoneDashboard()));
    connect(done, SIGNAL(payment()), this, SLOT(reset()));

    // Estimated position
    positionCrumb(QPoint(175, -4));
    breadCrumb->goToCrumb(RECIPIENT);
}

void BlocknetSendFunds::addAddress(const QString &address) {
    breadCrumb->goToCrumb(RECIPIENT);
    page1->addAddress(address);
}

void BlocknetSendFunds::focusInEvent(QFocusEvent *event) {
    QWidget::focusInEvent(event);
    if (screen)
        screen->setFocus();
}

void BlocknetSendFunds::crumbChanged(int crumb) {
    // Prevent users from jumping around the crumb widget without validating previous pages
    auto validatePages = [](const int toPage, const QVector<BlocknetSendFundsPage*> &pages) -> bool {
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

    if (!done->isHidden()) {
        layout->removeWidget(done);
        done->hide();
    }

    switch(crumb) {
        case RECIPIENT:
            screen = page1;
            break;
        case AMOUNT:
            screen = page2;
            break;
        case TRANSACTION_FEE:
            screen = page3;
            break;
        case REVIEW_PAYMENT:
            screen = page4;
            break;
        default:
            return;
    }
    layout->addWidget(screen);

    screen->setData(model);
    positionCrumb();
    screen->show();
    screen->setFocus();
}

void BlocknetSendFunds::nextCrumb(int crumb) {
    if (screen && crumb > breadCrumb->getCrumb() && breadCrumb->showCrumb(breadCrumb->getCrumb()) && !screen->validated())
        return;
    if (crumb >= REVIEW_PAYMENT) // do nothing if at the end
        return;
    breadCrumb->goToCrumb(++crumb);
}

void BlocknetSendFunds::prevCrumb(int crumb) {
    if (!screen)
        return;
    if (crumb <= RECIPIENT) // do nothing if at the beginning
        return;
    breadCrumb->goToCrumb(--crumb);
}

void BlocknetSendFunds::onCancel(int crumb) {
    auto ret = QMessageBox::warning(this->parentWidget(), tr("Issue"), tr("Are you sure you want to cancel this transaction?"), QMessageBox::Yes, QMessageBox::No);
    if (ret == QMessageBox::Yes) {
        clear();
        breadCrumb->goToCrumb(RECIPIENT);
        emit dashboard();
    }
}

void BlocknetSendFunds::onEdit() {
    breadCrumb->goToCrumb(AMOUNT);
}

void BlocknetSendFunds::onSendFunds() {
    clear();
    goToDone();
}

void BlocknetSendFunds::onDoneDashboard() {
    model->reset();
    for (BlocknetSendFundsPage *page : pages)
        page->clear();
    breadCrumb->goToCrumb(RECIPIENT);
    emit dashboard();
}

bool BlocknetSendFunds::event(QEvent *event) {
    if (screen && event->type() == QEvent::LayoutRequest) {
        positionCrumb();
    } else if (event->type() == QEvent::Type::MouseButtonPress) {
        auto *w = this->focusWidget();
        if (dynamic_cast<const QLineEdit*>(w) != nullptr && w->hasFocus() && !w->underMouse())
            w->clearFocus();
    }
    return QFrame::event(event);
}

void BlocknetSendFunds::reset() {
    model->reset();
    for (BlocknetSendFundsPage *page : pages)
        page->clear();
    breadCrumb->goToCrumb(RECIPIENT);
}

void BlocknetSendFunds::positionCrumb(QPoint pt) {
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

void BlocknetSendFunds::goToDone() {
    layout->removeWidget(screen);
    screen->hide();
    layout->addWidget(done);
    done->show();
}

