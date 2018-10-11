// Copyright (c) 2011-2013 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_PROPOSALRECORD_H
#define BITCOIN_QT_PROPOSALRECORD_H

#include "amount.h"
#include "uint256.h"

#include <QList>
#include <QString>

class CWallet;

class ProposalRecord
{
public:
    ProposalRecord():
            hash(""), start_epoch(0), end_epoch(0), url(""), name(""), yesVotes(0), noVotes(0), abstainVotes(0), amount(0), votesNeeded(0)
    {
    }

    ProposalRecord(QString hash, const CAmount& start_epoch, const CAmount& end_epoch,
                QString url, QString name,
                const CAmount& yesVotes, const CAmount& noVotes, const CAmount& abstainVotes,
                const CAmount& amount, const CAmount& votesNeeded):
            hash(hash), start_epoch(start_epoch), end_epoch(end_epoch), url(url), name(name), yesVotes(yesVotes), noVotes(noVotes),
            abstainVotes(abstainVotes), amount(amount), votesNeeded(votesNeeded)
    {
    }

    QString hash;
    CAmount start_epoch;
    CAmount end_epoch;
    QString url;
    QString name;
    CAmount yesVotes;
    CAmount noVotes;
    CAmount abstainVotes;
    CAmount amount;
    CAmount votesNeeded;
};

#endif // BITCOIN_QT_PROPOSALRECORD_H
