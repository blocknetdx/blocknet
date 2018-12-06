// Copyright (c) 2018 The Blocknet Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BLOCKNETPEERDETAILS_H
#define BLOCKNETPEERDETAILS_H

#include <QFrame>
#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QEvent>

class BlocknetPeerDetails : public QFrame {
    Q_OBJECT
public:
    explicit BlocknetPeerDetails(QFrame *parent = nullptr);
    ~BlocknetPeerDetails() override;

    struct PeerDetails {
        QString direction;
        QString protocol;
        QString version;
        QString startingHeight;
        QString syncHeight;
        QString banScore;
        QString connectionTime;
        QString lastSend;
        QString lastReceive;
        QString bytesSent;
        QString bytesReceived;
        QString pingTime;
    };

    void setDisplayWidget(QWidget *widget);
    void show(PeerDetails details);

public slots:
    void removeSelf(bool kill = true);

private:
    QVBoxLayout *layout;
    QGridLayout *detailsLayout;
    QFrame *detailsFrame;
    QWidget *displayWidget = nullptr;
    QLabel *titleLbl;
    QLabel *directionLbl;
    QLabel *protocolLbl;
    QLabel *versionLbl;
    QLabel *startHeightLbl;
    QLabel *syncHeightLbl;
    QLabel *banScoreLbl;
    QLabel *connTimeLbl;
    QLabel *lastSendLbl;
    QLabel *lastReceiveLbl;
    QLabel *bytesSentLbl;
    QLabel *bytesReceivedLbl;
    QLabel *pingTimeLbl;

    bool eventFilter(QObject *obj, QEvent *event) override;
};

#endif // BLOCKNETPEERDETAILS_H