#!/bin/bash
###############################################################
### Safely rename bitcoin to litecoin in src/qt/locale/*.ts ###
### Direction: Run this within src/qt/locale/               ###
###############################################################

PROTECT="s/BitcoinGUI/FoobarGUI/ s/bitcoin.cpp/foobar.cpp/ s/bitcoingui.cpp/foobargui.cpp/ s/bitcoin-core/foobar-core/ s/bitcoinstrings.cpp/foobarstrings.cpp/ s/bitcoingui.cpp/foobargui.cpp/"
for regex in $PROTECT; do sed -i "$regex" *.ts; done

REPLACE="s/bitcoin/litecoin/g s/Bitcoin/Litecoin/g s/BITCOIN/LITECOIN/g"
for regex in $REPLACE; do sed -i "$regex" *.ts; done

# Cyrillic languages prefer Litecoin instead of Лайткоин
REPLACE="s/Биткоин/Litecoin/g"
for regex in $REPLACE; do sed -i "$regex" *.ts; done

# Chinese Simplified 比特币 and 位元币 => 莱特币
REPLACE="s/比特币/莱特币/g s/位元币/莱特幣/g"
for regex in $REPLACE; do sed -i "$regex" bitcoin_zh_CN.ts; done

# Chinese Traditional 比特幣 and 位元幣 => 莱特幣
REPLACE="s/比特幣/莱特幣/g s/位元幣/莱特幣/g"
for regex in $REPLACE; do sed -i "$regex" bitcoin_zh_TW.ts; done

UNPROTECT="s/FoobarGUI/BitcoinGUI/ s/foobar.cpp/bitcoin.cpp/ s/foobargui.cpp/bitcoingui.cpp/ s/foobar-core/bitcoin-core/ s/foobarstrings.cpp/bitcoinstrings.cpp/ s/foobargui.cpp/bitcoingui.cpp/"
for regex in $UNPROTECT; do sed -i "$regex" *.ts; done


