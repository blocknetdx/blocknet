#!/bin/bash
###############################################################
### Safely rename bitcoin to litecoin in src/qt/locale/*.ts ###
### Direction: Run this within src/qt/locale/               ###
###############################################################

PROTECT="s/BitcoinGUI/FoobarGUI/ s/bitcoin.cpp/foobar.cpp/ s/bitcoingui.cpp/foobargui.cpp/ s/bitcoin-core/foobar-core/ s/bitcoinstrings.cpp/foobarstrings.cpp/ s/bitcoingui.cpp/foobargui.cpp/"
for regex in $PROTECT; do sed -i "$regex" *.ts; done

REPLACE="s/bitcoin/litecoin/g s/Bitcoin/Litecoin/g s/BITCOIN/LITECOIN/g"
for regex in $REPLACE; do sed -i "$regex" *.ts; done

UNPROTECT="s/FoobarGUI/BitcoinGUI/ s/foobar.cpp/bitcoin.cpp/ s/foobargui.cpp/bitcoingui.cpp/ s/foobar-core/bitcoin-core/ s/foobarstrings.cpp/bitcoinstrings.cpp/ s/foobargui.cpp/bitcoingui.cpp/"
for regex in $UNPROTECT; do sed -i "$regex" *.ts; done


