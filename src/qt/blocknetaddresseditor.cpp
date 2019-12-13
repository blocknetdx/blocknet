// Copyright (c) 2018-2019 The Blocknet developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/blocknetaddresseditor.h>

#include <qt/blocknetguiutil.h>

#include <QApplication>
#include <QKeyEvent>
#include <QMimeData>
#include <QPainter>
#include <QTextDocumentFragment>
#include <QTextFragment>
#include <QXmlStreamReader>

/**
 * @brief The requirement for this Text Edit widget is that it utilizes html to display the
 *        addresses. Valid Blocknet addresses inserted into the widget are parsed into
 *        <img>'s and displayed inline. This allows for copy/cut/paste actions users would
 *        normally expect. Regarding copy actions from within the text edit, the clipboard
 *        data is mutated on copy, by inserting the actual address values in place of the
 *        copied image placeholders. The widget is draw every time new data is added, this
 *        includes single characters. Most users will be pasting addresses into the widget,
 *        so the aggressive re-rendering should be negligible for most users.
 *
 *        Clipboard events are only enabled if the user explicitly selects content in the
 *        text edit. Clipboard events are disabled on the focus out event.
 *
 * @param width
 * @param parent
 */
BlocknetAddressEditor::BlocknetAddressEditor(int width, QTextEdit *parent) : QTextEdit(parent), minHeight(BGU::spi(46)) {
    this->setFixedSize(width, minHeight);
    this->setAcceptRichText(true);
    this->setCursor(Qt::IBeamCursor);
    clipboard = QApplication::clipboard();
    connect(this, &BlocknetAddressEditor::textChanged, this, &BlocknetAddressEditor::onTextChanged);
    connect(this, &BlocknetAddressEditor::selectionChanged, this, &BlocknetAddressEditor::onSelectionChanged);
}

void BlocknetAddressEditor::keyPressEvent(QKeyEvent *event) {
    if (event->key() == Qt::Key_Enter || event->key() == Qt::Key_Return) {
        Q_EMIT returnPressed();
        return;
    } else if (event->key() == Qt::Key_Backspace) {
        backspacePressed = true;
    }
    QTextEdit::keyPressEvent(event);
}

/**
 * @brief Adds an address to the text edit. The string representation of the address is
 *        converted to a QImage in order to properly display parsed addresses inline.
 * @param addr
 */
void BlocknetAddressEditor::addAddress(QString addr) {
    addrs << addr;

    const qreal r = BGU::spr(16); // border radius
    const qreal pad = BGU::spr(4);
    const qreal cw = BGU::spr(24); // left circle's width
    const qreal cwpad = BGU::spr(5);

    QFont font("Roboto", 11);
    QFontMetrics fm(font);
    qreal wf = fm.width(addr);
    qreal bgw = wf + cw + cwpad*2 + pad*2;
    const qreal bgh = BGU::spr(32);

    // total image width/height
    qreal w = bgw + pad*2;
    qreal h = bgh;

    // starting positions
    const qreal sx = pad;
    const qreal sy = 0;

    qreal dpr = BGU::spr(1.0);
    auto img = QImage(int(w*dpr), int(h*dpr), QImage::Format_ARGB32);
    img.fill(Qt::transparent);
    img.setDevicePixelRatio(dpr);

    QPainter p(&img);
    p.setPen(Qt::NoPen);

    // draw background
    QPainterPath p1;
    p1.addRoundedRect(sx, sy, bgw, bgh, r, r);
    p.fillPath(p1, QColor(0x17, 0x2F, 0x49, 255));

    // draw circle
    QPainterPath p2;
    p2.addEllipse(sx+pad, h/2-cw/2, cw, cw);
    QLinearGradient grad(0, cw/2, cw, cw/2);
    grad.setColorAt(0, QColor(0xFB, 0x7F, 0x70));
    grad.setColorAt(1, QColor(0xF6, 0x50, 0x8A));
    p.fillPath(p2, grad);

    // draw text
    qreal textsx = sx+pad+cw+cwpad;
    p.setFont(font);
    p.setPen(Qt::white);
    p.drawText(int(textsx), int(h-h/2+pad), addr);

    QTextCursor cursor = this->textCursor();
    QTextDocument *document = this->document();
    document->addResource(QTextDocument::ImageResource, QUrl(addr), img);
    QTextImageFormat imageFormat;
    imageFormat.setName(addr);
    cursor.insertImage(imageFormat);
    cursor.insertText(" "); // add space between addresses

    // resize
    this->setFixedHeight(this->optimalSize().height());
}

/**
 * @brief This method modifies the text edit's text which triggers the event. The textChanged
 *        signal is disconnected to prevent infinite recursion. The signal is reconnect at the
 *        end of this handler. This method redraws the text edit content on each call, removing
 *        duplicates in any pasted or inserted text.
 */
void BlocknetAddressEditor::onTextChanged() {
    auto a = this->toHtml().replace(" ", "");
    if (backspacePressed && a == prevText.replace(" ", "")) {
        backspacePressed = false;
        return;
    }

    disconnect(this, &BlocknetAddressEditor::textChanged, this, &BlocknetAddressEditor::onTextChanged);

    // clear existing addresses
    addrs.clear();

    QSet<QString> uniqueAddrs;
    QVector<QString> selectedAddrs;
    QRegularExpression potentialAddrExp("^\\s*[a-zA-Z0-9]+\\s*$");
    QRegularExpression cleanExp("[^\\sa-zA-Z0-9+]*([a-zA-Z0-9]+)[^\\sa-zA-Z0-9+]*");

    // Find the user's current cursor position, to retain typing experience
    int moveToPosition = this->textCursor().position();
    bool end = this->textCursor().atEnd();

    // Setup a new cursor to determine the best location for the existing cursor location.
    QTextCursor tc(this->document());
    tc.setPosition(this->textCursor().position());
    tc.select(QTextCursor::WordUnderCursor);
    auto matches = cleanExp.match(tc.selectedText());
    auto selectedText = QString();
    if (matches.hasMatch())
        selectedText = matches.captured(1);

    auto htmlText = this->toHtml();

    // Skipping gibberish is a state mechanism that's used to skip parsing <html> <head> <style> <body> tags.
    // The assumption is that the <p> and <img> tags that we want are after all these.
    bool skipGibberish = true;

    // Grab all the Blocknet addresses represented both as images and text.
    QXmlStreamReader xml(htmlText);
    while (!xml.atEnd()) {
        auto t = xml.readNext();

        if (skipGibberish && t == QXmlStreamReader::StartElement
            && (xml.name() == QLatin1String("img") || xml.name() == QLatin1String("p")))
            skipGibberish = false;
        if (skipGibberish)
            continue;

        if (t == QXmlStreamReader::StartElement && xml.name() == QLatin1String("img")) { // grab existing addresses
            QXmlStreamAttributes attrs = xml.attributes();
            for (const QXmlStreamAttribute &a : attrs) {
                QString attrVal = a.value().toString();
                if (a.name() == QLatin1String("src") && !uniqueAddrs.contains(attrVal)) {
                    uniqueAddrs << attrVal;
                    selectedAddrs.push_back(attrVal);
                }
            }
        } else if (t == QXmlStreamReader::Characters) { // check for incomplete addresses and text
            auto trimmed = xml.text().toString().simplified();
            auto potentialAddresses = trimmed.split(" ", QString::SkipEmptyParts);
            for (QString &text : potentialAddresses) {
                if (potentialAddrExp.match(text).hasMatch() && !uniqueAddrs.contains(text)) {
                    uniqueAddrs << text;
                    selectedAddrs.push_back(text);
                }
            }
        }
    }

    // Remove existing content from the text edit
    this->clear();

    // At this point we want to insert all new and existing parsed addresses into the text edit
    // The cursor is positioned to the closest location it was when the user started typing
    // or pasting.

    // Insert addresses
    for (QString &addr : selectedAddrs) {
        if (isValidAddress(addr)) {
            addAddress(addr);
        }
        else {
            this->textCursor().insertText(addr);
        }
        if (equalS(addr, selectedText))
            moveToPosition = this->textCursor().position();
    }

    if (end)
        tc.movePosition(QTextCursor::End);
    else
        tc.setPosition(moveToPosition);
    this->setTextCursor(tc);

    prevText = this->toHtml();
    backspacePressed = false; // reset backspace state

    // Notify addresses changed
    Q_EMIT addresses();

    connect(this, &BlocknetAddressEditor::textChanged, this, &BlocknetAddressEditor::onTextChanged);
}

/**
 * @brief  Returns the optimal size for the widget.
 * @return
 */
QSize BlocknetAddressEditor::optimalSize() const {
    // resize the text edit to fit the content
    int newH = static_cast<int>(this->document()->size().height()) + BGU::spi(5);
    if (newH > minHeight)
        return { this->width(), newH };
    else if (newH <= minHeight && newH != minHeight)
        return { this->width(), minHeight };
    return { this->width(), this->height() };
}

/**
 * @brief The clipboard is modified on copy because the text edit is using images to represent valid
 *        addresses. The copied images are converted to their text representations and assigned to
 *        the clipboard in place of the image placeholders.
 *
 *        The clipboard copy formatting is "address1 address2 address3 ":
 *        BaxhG1onYJxfNUiXLCCJojEg4HthZW4UAL BaxhG1onYJxfNUiXLCCJojEg4HthZW4UAL
 *
 *        Note the space between addresses. Duplicates are also removed from the copied text.
 */
void BlocknetAddressEditor::onClipboard() {
    QTextCursor tc = this->textCursor();
    auto sel = tc.selection();
    auto selHtml = sel.toHtml();

    QSet<QString> uniqueAddrs;
    QVector<QString> selectedAddrs;
    QRegularExpression potentialAddrExp("^\\s*[a-zA-Z0-9]+\\s*$");

    // Grab all the addresses represented by text
    auto trimmedText = this->toPlainText().simplified();
    auto addresses = trimmedText.split(" ", QString::SkipEmptyParts);
    for (QString &addr : addresses) {
        // Check that address is valid
        if (potentialAddrExp.match(addr).hasMatch() && !uniqueAddrs.contains(addr)) {
            uniqueAddrs << addr;
            selectedAddrs.push_back(addr);
        }
    }

    // Parse the html text, grab all the addresses represented as images
    QXmlStreamReader xml(selHtml);
    while (!xml.atEnd()) {
        if (xml.readNext() == QXmlStreamReader::StartElement && xml.name() == QLatin1String("img")) {
            QXmlStreamAttributes attrs = xml.attributes();
            for (const QXmlStreamAttribute &a : attrs) {
                QString attrVal = a.value().toString();
                if (a.name() == QLatin1String("src") && !uniqueAddrs.contains(attrVal)) {
                    uniqueAddrs << attrVal;
                    selectedAddrs.push_back(attrVal);
                }
            }
        }
    }

    // Format the addresses with a space between each one
    QString t;
    for (QString &s : selectedAddrs.toList())
        t.append(s + " ");
    t = t.simplified();
    cbOn(false); // disable events to prevent infinite recursion
    clipboard->setText(t);
    cbOn();
}

void BlocknetAddressEditor::onSelectionChanged() {
    cbOn();
}

void BlocknetAddressEditor::focusOutEvent(QFocusEvent *e) {
    QTextEdit::focusOutEvent(e);
    cbOn(false);
}

/**
 * @brief Turn the clipboard events on or off.
 * @param on [default=true]
 */
void BlocknetAddressEditor::cbOn(bool on) {
    if (on)
        connect(clipboard, &QClipboard::dataChanged, this, &BlocknetAddressEditor::onClipboard);
    else
        disconnect(clipboard, &QClipboard::dataChanged, this, &BlocknetAddressEditor::onClipboard);
}

/**
 * @brief Returns true if the specified Blocknet address is valid.
 * @param addr
 * @return
 */
bool BlocknetAddressEditor::isValidAddress(QString &addr) {
    return this->validator(addr);
}

bool BlocknetAddressEditor::equalS(QString s1, QString s2, Qt::CaseSensitivity sensitivity) {
    return QString::compare(s1, s2, sensitivity) == 0;
}
