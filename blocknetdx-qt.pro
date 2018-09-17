TEMPLATE = app
TARGET = blocknetdx-qt
VERSION = 1.0.0

DEFINES += \
    QT_GUI \
    BOOST_THREAD_USE_LIB \
    BOOST_SPIRIT_THREADSAFE

QT += core gui network
greaterThan(QT_MAJOR_VERSION, 4) {
    QT += widgets
}

CONFIG += no_include_pwd

CONFIG(release, debug|release): DEFINES += NDEBUG

# UNCOMMENT THIS SECTION TO BUILD ON WINDOWS
# Change paths if needed, these use the foocoin/deps.git repository locations

!include($$PWD/config.pri) {
   error(Failed to include config.pri)
 }

OBJECTS_DIR = build
MOC_DIR = build
UI_DIR = build

# use: qmake "RELEASE=1"
contains(RELEASE, 1) {
    # Mac: compile for maximum compatibility (10.5, 32-bit)
    macx:QMAKE_CXXFLAGS += -mmacosx-version-min=10.5 -arch x86_64 -isysroot /Developer/SDKs/MacOSX10.5.sdk

    !windows:!macx {
        # Linux: static link
        LIBS += -Wl,-Bstatic
    }
}

!win32 {
# for extra security against potential buffer overflows: enable GCCs Stack Smashing Protection
QMAKE_CXXFLAGS *= -fstack-protector-all --param ssp-buffer-size=1
QMAKE_LFLAGS *= -fstack-protector-all --param ssp-buffer-size=1
# We need to exclude this for Windows cross compile with MinGW 4.2.x, as it will result in a non-working executable!
# This can be enabled for Windows, when we switch to MinGW >= 4.4.x.
}

# for extra security on Windows: enable ASLR and DEP via GCC linker flags
win32 {
    CONFIG(release, debug|release) {
        QMAKE_LFLAGS *= -Wl,--dynamicbase -Wl,--nxcompat
    }
}

QMAKE_CXXFLAGS *= -fpermissive -std=c++11

# use: qmake "USE_QRCODE=1"
# libqrencode (http://fukuchi.org/works/qrencode/index.en.html) must be installed for support
contains(USE_QRCODE, 1) {
    message(Building with QRCode support)
    DEFINES += USE_QRCODE
    LIBS += -lqrencode
}

# use: qmake "USE_UPNP=1" ( enabled by default; default)
#  or: qmake "USE_UPNP=0" (disabled by default)
#  or: qmake "USE_UPNP=-" (not supported)
# miniupnpc (http://miniupnp.free.fr/files/) must be installed for support
contains(USE_UPNP, -) {
    message(Building without UPNP support)
} else {
    message(Building with UPNP support)
    count(USE_UPNP, 0) {
        USE_UPNP=1
    }
    DEFINES += USE_UPNP=$$USE_UPNP STATICLIB
        DEFINES += USE_UPNP=$$USE_UPNP MINIUPNP_STATICLIB
    INCLUDEPATH += $$MINIUPNPC_INCLUDE_PATH
    LIBS += $$join(MINIUPNPC_LIB_PATH,,-L,) -lminiupnpc
    win32:LIBS += -liphlpapi
}

# use: qmake "USE_DBUS=1"
contains(USE_DBUS, 1) {
    message(Building with DBUS (Freedesktop notifications) support)
    DEFINES += USE_DBUS
    QT += dbus
}

# use: qmake "USE_IPV6=1" ( enabled by default; default)
#  or: qmake "USE_IPV6=0" (disabled by default)
#  or: qmake "USE_IPV6=-" (not supported)
contains(USE_IPV6, -) {
    message(Building without IPv6 support)
} else {
    message(Building with IPv6 support)
    count(USE_IPV6, 0) {
        USE_IPV6=1
    }
    DEFINES += USE_IPV6=$$USE_IPV6
}

contains(BITCOIN_NEED_QT_PLUGINS, 1) {
    DEFINES += BITCOIN_NEED_QT_PLUGINS
    QTPLUGIN += qcncodecs qjpcodecs qtwcodecs qkrcodecs qtaccessiblewidgets
}

INCLUDEPATH += \
    src \
    src/json \
    src/qt \
    $$BOOST_INCLUDE_PATH \
    $$BDB_INCLUDE_PATH \
    $$OPENSSL_INCLUDE_PATH \
    $$QRENCODE_INCLUDE_PATH \
    src/leveldb/include \
    src/leveldb/helpers

LIBS += \
    $$join(BOOST_LIB_PATH,,-L,) \
    $$join(BDB_LIB_PATH,,-L,) \
    $$join(OPENSSL_LIB_PATH,,-L,) \
    $$join(QRENCODE_LIB_PATH,,-L,) \
    -L$$PWD/src/leveldb


LIBS += \
    -lleveldb \
    -lmemenv \
    -lsecp256k1 \
    -lprotobuf \
    -lssl \
    -lcrypto \
    -ldb_cxx$$BDB_LIB_SUFFIX \
    -lpthread

windows {
    LIBS += \
        -lshlwapi \
        -lws2_32 \
        -lole32 \
        -loleaut32 \
        -luuid \
        -lcrypt32 \
        -lgdi32
}

unix:!macx {
    LIBS += \
        -lboost_system \
        -lboost_filesystem \
        -lboost_program_options \
        -lboost_thread \
        -lboost_date_time
}

SOURCES += \
    src/bloom.cpp \
    src/hash.cpp \
    src/activeservicenode.cpp \
    src/allocators.cpp \
    src/amount.cpp \
    src/arith_uint256.cpp \
    src/base58.cpp \
    src/chain.cpp \
    src/chainparams.cpp \
    src/chainparamsbase.cpp \
    src/clientversion.cpp \
    src/coins.cpp \
    src/compressor.cpp \
    src/core_read.cpp \
    src/core_write.cpp \
    src/eccryptoverify.cpp \
    src/leveldbwrapper.cpp \
    src/merkleblock.cpp \
    src/obfuscation.cpp \
    src/obfuscation-relay.cpp \
    src/pow.cpp \
    src/pubkey.cpp \
    src/random.cpp \
    src/rest.cpp \
    src/rpcblockchain.cpp \
    src/rpcclient.cpp \
    src/rpcdump.cpp \
    src/rpcmining.cpp \
    src/rpcmisc.cpp \
    src/rpcnet.cpp \
    src/rpcprotocol.cpp \
    src/rpcrawtransaction.cpp \
    src/rpcserver.cpp \
    src/rpcservicenode.cpp \
    src/rpcservicenode-budget.cpp \
    src/rpcwallet.cpp \
    src/servicenode.cpp \
    src/servicenode-budget.cpp \
    src/servicenodeconfig.cpp \
    src/servicenodeman.cpp \
    src/servicenode-payments.cpp \
    src/servicenode-sync.cpp \
    src/spork.cpp \
    src/swifttx.cpp \
    src/timedata.cpp \
    src/txdb.cpp \
    src/txmempool.cpp \
    src/uint256.cpp \
    src/utilmoneystr.cpp \
    src/utilstrencodings.cpp \
    src/utiltime.cpp \
    src/validationinterface.cpp \
    src/wallet_ismine.cpp \
    src/qt/bip38tooldialog.cpp \
    src/qt/blockexplorer.cpp \
    src/qt/blocknetdx.cpp \
    src/qt/blocknetdxstrings.cpp \
    src/qt/intro.cpp \
    src/qt/multisenddialog.cpp \
    src/qt/networkstyle.cpp \
    src/qt/obfuscationconfig.cpp \
    src/qt/openuridialog.cpp \
    src/qt/paymentrequestplus.cpp \
    src/qt/paymentserver.cpp \
    src/qt/peertablemodel.cpp \
    src/qt/platformstyle.cpp \
    src/qt/receivecoinsdialog.cpp \
    src/qt/receiverequestdialog.cpp \
    src/qt/recentrequeststablemodel.cpp \
    src/qt/servicenodelist.cpp \
    src/qt/splashscreen.cpp \
    src/qt/trafficgraphwidget.cpp \
    src/qt/utilitydialog.cpp \
    src/qt/walletframe.cpp \
    src/qt/walletmodeltransaction.cpp \
    src/qt/walletview.cpp \
    src/qt/winshutdownmonitor.cpp \
    src/compat/strnlen.cpp \
    src/crypto/hmac_sha256.cpp \
    src/crypto/hmac_sha512.cpp \
    src/crypto/rfc6979_hmac_sha256.cpp \
    src/crypto/ripemd160.cpp \
    src/crypto/scrypt.cpp \
    src/crypto/sha1.cpp \
    src/crypto/sha256.cpp \
    src/crypto/sha512.cpp \
    src/crypto/aes_helper.c \
    src/crypto/blake.c \
    src/crypto/bmw.c \
    src/crypto/cubehash.c \
    src/crypto/echo.c \
    src/crypto/groestl.c \
    src/crypto/jh.c \
    src/crypto/keccak.c \
    src/crypto/luffa.c \
    src/crypto/shavite.c \
    src/crypto/simd.c \
    src/crypto/skein.c \
    src/primitives/block.cpp \
    src/primitives/transaction.cpp \
    src/script/bitcoinconsensus.cpp \
    src/script/interpreter.cpp \
    src/script/script.cpp \
    src/script/script_error.cpp \
    src/script/sigcache.cpp \
    src/script/sign.cpp \
    src/script/standard.cpp \
    src/univalue/univalue.cpp \
    src/univalue/univalue_read.cpp \
    src/univalue/univalue_write.cpp \
    src/compat/glibc_sanity.cpp \
    src/compat/glibcxx_sanity.cpp \
    src/xbridge/util/logger.cpp \
    src/xbridge/util/settings.cpp \
    src/xbridge/util/txlog.cpp \
    src/xbridge/util/xutil.cpp \
    src/xbridge/bitcoinrpcconnector.cpp \
    src/xbridge/xbridgeapp.cpp \
    src/xbridge/xbridgeexchange.cpp \
    src/xbridge/xbridgesession.cpp \
    src/xbridge/xbridgetransaction.cpp \
    src/xbridge/xbridgetransactiondescr.cpp \
    src/xbridge/xbridgetransactionmember.cpp \
    src/support/cleanse.cpp \
    src/crypto/chacha20.cpp \
    src/bip38.cpp \
    src/s3downloader.cpp \
    src/coinvalidator.cpp \
    src/xbridge/xbitcoinaddress.cpp \
    src/qt/xbridgeui/xbridgeaddressbookmodel.cpp \
    src/qt/xbridgeui/xbridgeaddressbookview.cpp \
    src/qt/xbridgeui/xbridgetransactiondialog.cpp \
    src/qt/xbridgeui/xbridgetransactionsmodel.cpp \
    src/qt/xbridgeui/xbridgetransactionsview.cpp \
    src/xbridge/xbitcointransaction.cpp \
    src/xbridge/rpcxbridge.cpp \
    src/xbridge/util/xbridgeerror.cpp \
    src/xbridge/xbridgewalletconnector.cpp \
    src/xbridge/xbridgewalletconnectorbtc.cpp \
    src/xbridge/xbridgewalletconnectordgb.cpp \
    src/xbridge/xbridgewalletconnectorbch.cpp \
    src/xbridge/xbridgecryptoproviderbtc.cpp \
    src/xbridge/xbridgepacket.cpp \
    src/qt/blocknetdxstrings.cpp \
    src/qt/blocknetfontmgr.cpp \
    src/qt/blocknetformbtn.cpp \
    src/qt/blocknethdiv.cpp \
    src/qt/blocknetdropdown.cpp \
    src/qt/blockneticonbtn.cpp \
    src/qt/blockneticonaltbtn.cpp \
    src/qt/blockneticonlabel.cpp \
    src/qt/blocknetleftmenu.cpp \
    src/qt/blocknetlineedit.cpp \
    src/qt/blocknetlockmenu.cpp \
    src/qt/blocknetquicksend.cpp \
    src/qt/blocknetsendfunds.cpp \
    src/qt/blocknetsendfunds1.cpp \
    src/qt/blocknetsendfunds2.cpp \
    src/qt/blocknetsendfunds3.cpp \
    src/qt/blocknetsendfunds4.cpp \
    src/qt/blocknetsendfundsdone.cpp \
    src/qt/blocknettoolbar.cpp \
    src/qt/blocknetcircle.cpp \
    src/qt/blocknetclosebtn.cpp \
    src/qt/blocknetwallet.cpp \
    src/qt/blocknetcoincontrol.cpp \
    src/qt/blocknetsendfundsrequest.cpp

#protobuf generated
SOURCES += \
    src/qt/paymentrequest.pb.cc

#compat
#    src/compat/glibc_compat.cpp \
#    src/compat/glibcxx_compat.cpp \

#ENABLE_ZMQ
#    src/zmq/zmqabstractnotifier.cpp \
#    src/zmq/zmqnotificationinterface.cpp \
#    src/zmq/zmqpublishnotifier.cpp

#mac
#    src/qt/macdockiconhandler.mm \
#    src/qt/macnotificationhandler.mm \

!win32 {
    # we use QMAKE_CXXFLAGS_RELEASE even without RELEASE=1 because we use RELEASE to indicate linking preferences not -O preferences
    genleveldb.commands = cd $$PWD/src/leveldb && CC=$$QMAKE_CC CXX=$$QMAKE_CXX $(MAKE) OPT=\"$$QMAKE_CXXFLAGS $$QMAKE_CXXFLAGS_RELEASE\" libleveldb.a libmemenv.a
} else {
    # make an educated guess about what the ranlib command is called
    isEmpty(QMAKE_RANLIB) {
        QMAKE_RANLIB = $$replace(QMAKE_STRIP, strip, ranlib)
    }
    LIBS += -lshlwapi
    genleveldb.commands = cd $$PWD/src/leveldb && CC=$$QMAKE_CC CXX=$$QMAKE_CXX TARGET_OS=OS_WINDOWS_CROSSCOMPILE $(MAKE) OPT=\"$$QMAKE_CXXFLAGS $$QMAKE_CXXFLAGS_RELEASE\" libleveldb.a libmemenv.a && $$QMAKE_RANLIB $$PWD/src/leveldb/libleveldb.a && $$QMAKE_RANLIB $$PWD/src/leveldb/libmemenv.a
}
genleveldb.target = $$PWD/src/leveldb/libleveldb.a
genleveldb.depends = FORCE

unix {
    PRE_TARGETDEPS += $$PWD/src/leveldb/libleveldb.a
    QMAKE_EXTRA_TARGETS += genleveldb
    # Gross ugly hack that depends on qmake internals, unfortunately there is no other way to do it.
    QMAKE_CLEAN += $$PWD/src/leveldb/libleveldb.a; cd $$PWD/src/leveldb ; $(MAKE) clean
}

# regenerate src/build.h
#!windows|contains(USE_BUILD_INFO, 1) {
#    genbuild.depends = FORCE
#    genbuild.commands = cd $$PWD; /bin/sh share/genbuild.sh $$OUT_PWD/build/build.h
#    genbuild.target = $$OUT_PWD/build/build.h
#    PRE_TARGETDEPS += $$OUT_PWD/build/build.h
#    QMAKE_EXTRA_TARGETS += genbuild
#    DEFINES += HAVE_BUILD_INFO
#}

contains(USE_O3, 1) {
    message(Building O3 optimization flag)
    QMAKE_CXXFLAGS_RELEASE -= -O2
    QMAKE_CFLAGS_RELEASE -= -O2
    QMAKE_CXXFLAGS += -O3
    QMAKE_CFLAGS += -O3
}

*-g++-32 {
    message("32 platform, adding -msse2 flag")

    QMAKE_CXXFLAGS += -msse2
    QMAKE_CFLAGS += -msse2
}

QMAKE_CXXFLAGS_WARN_ON = \
        -fdiagnostics-show-option \
        -Wall \
        -Wextra \
        -Wformat \
        -Wformat-security \
        -Wstack-protector \
        -Wno-deprecated-declarations

# Input
DEPENDPATH += \
    src \
    src/json \
    src/qt

HEADERS += \
    src/qt/bitcoingui.h \
    src/qt/transactiontablemodel.h \
    src/qt/addresstablemodel.h \
    src/qt/optionsdialog.h \
    src/qt/coincontroldialog.h \
    src/qt/coincontroltreewidget.h \
    src/qt/sendcoinsdialog.h \
    src/qt/addressbookpage.h \
    src/qt/signverifymessagedialog.h \
    src/qt/editaddressdialog.h \
    src/qt/bitcoinaddressvalidator.h \
    src/alert.h \
    src/addrman.h \
    src/base58.h \
    src/checkpoints.h \
    src/compat.h \
    src/coincontrol.h \
    src/sync.h \
    src/util.h \
    src/uint256.h \
    src/kernel.h \
    src/serialize.h \
    src/main.h \
    src/miner.h \
    src/net.h \
    src/key.h \
    src/db.h \
    src/txdb.h \
    src/walletdb.h \
    src/init.h \
    src/mruset.h \
    src/json/json_spirit_writer_template.h \
    src/json/json_spirit_writer.h \
    src/json/json_spirit_value.h \
    src/json/json_spirit_utils.h \
    src/json/json_spirit_stream_reader.h \
    src/json/json_spirit_reader_template.h \
    src/json/json_spirit_reader.h \
    src/json/json_spirit_error_position.h \
    src/json/json_spirit.h \
    src/qt/clientmodel.h \
    src/qt/guiutil.h \
    src/qt/transactionrecord.h \
    src/qt/guiconstants.h \
    src/qt/optionsmodel.h \
    src/qt/transactiondesc.h \
    src/qt/transactiondescdialog.h \
    src/qt/bitcoinamountfield.h \
    src/wallet.h \
    src/keystore.h \
    src/qt/transactionfilterproxy.h \
    src/qt/transactionview.h \
    src/qt/walletmodel.h \
    src/qt/overviewpage.h \
    src/qt/csvmodelwriter.h \
    src/crypter.h \
    src/qt/sendcoinsentry.h \
    src/qt/qvalidatedlineedit.h \
    src/qt/bitcoinunits.h \
    src/qt/qvaluecombobox.h \
    src/qt/askpassphrasedialog.h \
    src/protocol.h \
    src/qt/notificator.h \
    src/allocators.h \
    src/ui_interface.h \
    src/qt/rpcconsole.h \
    src/version.h \
    src/netbase.h \
    src/clientversion.h \
    src/bloom.h \
    src/checkqueue.h \
    src/hash.h \
    src/limitedmap.h \
    src/threadsafety.h \
    src/qt/macnotificationhandler.h \
    src/tinyformat.h \
    src/activeservicenode.h \
    src/amount.h \
    src/arith_uint256.h \
    src/bip38.h \
    src/chain.h \
    src/chainparams.h \
    src/chainparamsbase.h \
    src/chainparamsseeds.h \
    src/coins.h \
    src/compressor.h \
    src/core_io.h \
    src/eccryptoverify.h \
    src/leveldbwrapper.h \
    src/merkleblock.h \
    src/noui.h \
    src/obfuscation.h \
    src/obfuscation-relay.h \
    src/pow.h \
    src/pubkey.h \
    src/random.h \
    src/rpcclient.h \
    src/rpcprotocol.h \
    src/rpcserver.h \
    src/servicenode.h \
    src/servicenode-budget.h \
    src/servicenodeconfig.h \
    src/servicenodeman.h \
    src/servicenode-payments.h \
    src/servicenode-sync.h \
    src/spork.h \
    src/streams.h \
    src/swifttx.h \
    src/timedata.h \
    src/txmempool.h \
    src/undo.h \
    src/utilmoneystr.h \
    src/utilstrencodings.h \
    src/utiltime.h \
    src/validationinterface.h \
    src/wallet_ismine.h \
    src/qt/bip38tooldialog.h \
    src/qt/blockexplorer.h \
    src/qt/intro.h \
    src/qt/multisenddialog.h \
    src/qt/networkstyle.h \
    src/qt/obfuscationconfig.h \
    src/qt/openuridialog.h \
    src/qt/paymentrequestplus.h \
    src/qt/paymentserver.h \
    src/qt/peertablemodel.h \
    src/qt/platformstyle.h \
    src/qt/receivecoinsdialog.h \
    src/qt/receiverequestdialog.h \
    src/qt/recentrequeststablemodel.h \
    src/qt/servicenodelist.h \
    src/qt/splashscreen.h \
    src/qt/trafficgraphwidget.h \
    src/qt/utilitydialog.h \
    src/qt/walletframe.h \
    src/qt/walletmodeltransaction.h \
    src/qt/walletview.h \
    src/qt/winshutdownmonitor.h \
    src/compat/sanity.h \
    src/crypto/common.h \
    src/crypto/hmac_sha256.h \
    src/crypto/hmac_sha512.h \
    src/crypto/rfc6979_hmac_sha256.h \
    src/crypto/ripemd160.h \
    src/crypto/scrypt.h \
    src/crypto/sha1.h \
    src/crypto/sha256.h \
    src/crypto/sha512.h \
    src/crypto/sph_blake.h \
    src/crypto/sph_bmw.h \
    src/crypto/sph_cubehash.h \
    src/crypto/sph_echo.h \
    src/crypto/sph_groestl.h \
    src/crypto/sph_jh.h \
    src/crypto/sph_keccak.h \
    src/crypto/sph_luffa.h \
    src/crypto/sph_shavite.h \
    src/crypto/sph_simd.h \
    src/crypto/sph_skein.h \
    src/crypto/sph_types.h \
    src/primitives/block.h \
    src/primitives/transaction.h \
    src/script/bitcoinconsensus.h \
    src/script/interpreter.h \
    src/script/script.h \
    src/script/script_error.h \
    src/script/sigcache.h \
    src/script/sign.h \
    src/script/standard.h \
    src/univalue/univalue.h \
    src/univalue/univalue_escapes.h \
    src/xbridge/util/logger.h \
    src/xbridge/util/settings.h \
    src/xbridge/util/txlog.h \
    src/xbridge/util/xutil.h \
    src/xbridge/bitcoinrpcconnector.h \
    src/xbridge/version.h \
    src/xbridge/xbridgeapp.h \
    src/xbridge/xbridgeexchange.h \
    src/xbridge/xbridgepacket.h \
    src/xbridge/xbridgesession.h \
    src/xbridge/xbridgetransaction.h \
    src/xbridge/xbridgetransactiondescr.h \
    src/xbridge/xbridgetransactionmember.h \
    src/xbridge/xuiconnector.h \
    src/FastDelegate.h \
    src/support/cleanse.h \
    src/ptr.h \
    src/crypto/chacha20.h \
    src/compat/endian.h \
    src/compat/byteswap.h \
    src/s3downloader.h \
    src/coinvalidator.h \
    src/xbridge/xkey.h \
    src/xbridge/xpubkey.h \
    src/xbridge/xbitcoinaddress.h \
    src/xbridge/xbitcoinsecret.h \
    src/qt/xbridgeui/xbridgeaddressbookmodel.h \
    src/qt/xbridgeui/xbridgeaddressbookview.h \
    src/qt/xbridgeui/xbridgetransactiondialog.h \
    src/qt/xbridgeui/xbridgetransactionsmodel.h \
    src/qt/xbridgeui/xbridgetransactionsview.h \
    src/xbridge/xbitcointransaction.h \
    src/xbridge/util/xbridgeerror.h \
    src/validationstate.h \
    src/xbridge/xbridgewalletconnector.h \
    src/xbridge/xbridgewalletconnectorbtc.h \
    src/xbridge/xbridgewalletconnectordgb.h \
    src/xbridge/xbridgewalletconnectorbch.h \
    src/xbridge/xbridgedef.h \
    src/xbridge/xbridgecryptoproviderbtc.h \
    src/qt/blocknetfontmgr.h \
    src/qt/blocknetformbtn.h \
    src/qt/blocknethdiv.h \
    src/qt/blocknetdropdown.h \
    src/qt/blockneticonbtn.h \
    src/qt/blockneticonaltbtn.h \
    src/qt/blockneticonlabel.h \
    src/qt/blocknetleftmenu.h \
    src/qt/blocknetlineedit.h \
    src/qt/blocknetlockmenu.h \
    src/qt/blocknetquicksend.h \
    src/qt/blocknetsendfunds.h \
    src/qt/blocknetsendfunds1.h \
    src/qt/blocknetsendfunds2.h \
    src/qt/blocknetsendfunds3.h \
    src/qt/blocknetsendfunds4.h \
    src/qt/blocknetsendfundsdone.h \
    src/qt/blocknetsendfundsutil.h \
    src/qt/blocknettoolbar.h \
    src/qt/blocknetcircle.h \
    src/qt/blocknetclosebtn.h \
    src/qt/blocknetvars.h \
    src/qt/blocknetwallet.h \
    src/qt/blocknetcoincontrol.h \
    src/qt/blocknetsendfundsrequest.h

#ENABLE_ZMQ
#    src/zmq/zmqabstractnotifier.h \
#    src/zmq/zmqconfig.h \
#    src/zmq/zmqnotificationinterface.h \
#    src/zmq/zmqpublishnotifier.h


SOURCES += \
    src/qt/bitcoingui.cpp \
    src/qt/transactiontablemodel.cpp \
    src/qt/addresstablemodel.cpp \
    src/qt/optionsdialog.cpp \
    src/qt/sendcoinsdialog.cpp \
    src/qt/coincontroldialog.cpp \
    src/qt/coincontroltreewidget.cpp \
    src/qt/addressbookpage.cpp \
    src/qt/signverifymessagedialog.cpp \
    src/qt/editaddressdialog.cpp \
    src/qt/bitcoinaddressvalidator.cpp \
    src/alert.cpp \
    src/sync.cpp \
    src/util.cpp \
    src/netbase.cpp \
    src/key.cpp \
    src/main.cpp \
    src/miner.cpp \
    src/init.cpp \
    src/net.cpp \
    src/checkpoints.cpp \
    src/addrman.cpp \
    src/db.cpp \
    src/walletdb.cpp \
    src/qt/clientmodel.cpp \
    src/qt/guiutil.cpp \
    src/qt/transactionrecord.cpp \
    src/qt/optionsmodel.cpp \
    src/qt/transactiondesc.cpp \
    src/qt/transactiondescdialog.cpp \
    src/qt/bitcoinamountfield.cpp \
    src/wallet.cpp \
    src/keystore.cpp \
    src/qt/transactionfilterproxy.cpp \
    src/qt/transactionview.cpp \
    src/qt/walletmodel.cpp \
    src/qt/overviewpage.cpp \
    src/qt/csvmodelwriter.cpp \
    src/crypter.cpp \
    src/qt/sendcoinsentry.cpp \
    src/qt/qvalidatedlineedit.cpp \
    src/qt/bitcoinunits.cpp \
    src/qt/qvaluecombobox.cpp \
    src/qt/askpassphrasedialog.cpp \
    src/protocol.cpp \
    src/qt/notificator.cpp \
    src/qt/rpcconsole.cpp \
    src/noui.cpp \
    src/kernel.cpp

RESOURCES += \
    src/qt/blocknetdx.qrc \
    src/qt/blocknetdx_locale.qrc

FORMS += \
    src/qt/forms/coincontroldialog.ui \
    src/qt/forms/sendcoinsdialog.ui \
    src/qt/forms/addressbookpage.ui \
    src/qt/forms/signverifymessagedialog.ui \
    src/qt/forms/editaddressdialog.ui \
    src/qt/forms/transactiondescdialog.ui \
    src/qt/forms/overviewpage.ui \
    src/qt/forms/sendcoinsentry.ui \
    src/qt/forms/askpassphrasedialog.ui \
    src/qt/forms/rpcconsole.ui \
    src/qt/forms/optionsdialog.ui \
    src/qt/forms/bip38tooldialog.ui \
    src/qt/forms/blockexplorer.ui \
    src/qt/forms/helpmessagedialog.ui \
    src/qt/forms/intro.ui \
    src/qt/forms/multisenddialog.ui \
    src/qt/forms/obfuscationconfig.ui \
    src/qt/forms/openuridialog.ui \
    src/qt/forms/receivecoinsdialog.ui \
    src/qt/forms/receiverequestdialog.ui \
    src/qt/forms/servicenodelist.ui \
    src/qt/forms/tradingdialog.ui

contains(USE_QRCODE, 1) {
HEADERS += src/qt/qrcodedialog.h
SOURCES += src/qt/qrcodedialog.cpp
FORMS += src/qt/forms/qrcodedialog.ui
}

CODECFORTR = UTF-8

# for lrelease/lupdate
# also add new translations to src/qt/bitcoin.qrc under translations/
TRANSLATIONS = $$files(src/qt/locale/blocknetdx_*.ts)

isEmpty(QMAKE_LRELEASE) {
    win32:QMAKE_LRELEASE = $$[QT_INSTALL_BINS]\\lrelease.exe
    else:QMAKE_LRELEASE = $$[QT_INSTALL_BINS]/lrelease
}
isEmpty(QM_DIR):QM_DIR = $$PWD/src/qt/locale
# automatically build translations, so they can be included in resource file
TSQM.name = lrelease ${QMAKE_FILE_IN}
TSQM.input = TRANSLATIONS
TSQM.output = $$QM_DIR/${QMAKE_FILE_BASE}.qm
TSQM.commands = $$QMAKE_LRELEASE ${QMAKE_FILE_IN} -qm ${QMAKE_FILE_OUT}
TSQM.CONFIG = no_link
QMAKE_EXTRA_COMPILERS += TSQM

# "Other files" to show in Qt Creator
OTHER_FILES += \
    doc/*.rst \
    doc/*.txt \
    doc/README \
    README.md \
    res/blocknetdx-qt-res.rc \
    configure.ac


# platform specific defaults, if not overridden on command line
#isEmpty(BOOST_LIB_SUFFIX) {
#    macx:BOOST_LIB_SUFFIX = -mt
#    windows:BOOST_LIB_SUFFIX = -mgw48-mt-s-1_55
#}

isEmpty(BOOST_THREAD_LIB_SUFFIX) {
    BOOST_THREAD_LIB_SUFFIX = $$BOOST_LIB_SUFFIX
}

isEmpty(BDB_LIB_PATH) {
    macx:BDB_LIB_PATH = /opt/local/lib/db48
}

isEmpty(BDB_LIB_SUFFIX) {
    macx:BDB_LIB_SUFFIX = -4.8
}

isEmpty(BDB_INCLUDE_PATH) {
    macx:BDB_INCLUDE_PATH = /opt/local/include/db48
}

isEmpty(BOOST_LIB_PATH) {
    macx:BOOST_LIB_PATH = /opt/local/lib
}

isEmpty(BOOST_INCLUDE_PATH) {
    macx:BOOST_INCLUDE_PATH = /opt/local/include
}

windows:DEFINES += WIN32
windows:QMAKE_RC = windres -DWINDRES_PREPROC
windows:RC_FILE = src/qt/res/blocknetdx-qt-res.rc

windows:!contains(MINGW_THREAD_BUGFIX, 0) {
    # At least qmake's win32-g++-cross profile is missing the -lmingwthrd
    # thread-safety flag. GCC has -mthreads to enable this, but it doesn't
    # work with static linking. -lmingwthrd must come BEFORE -lmingw, so
    # it is prepended to QMAKE_LIBS_QT_ENTRY.
    # It can be turned off with MINGW_THREAD_BUGFIX=0, just in case it causes
    # any problems on some untested qmake profile now or in the future.
    DEFINES += _MT BOOST_THREAD_PROVIDES_GENERIC_SHARED_MUTEX_ON_WIN
    QMAKE_LIBS_QT_ENTRY = -lmingwthrd $$QMAKE_LIBS_QT_ENTRY
}

!windows:!macx {
    DEFINES += LINUX
    LIBS += -lrt
}

macx:HEADERS += src/qt/macdockiconhandler.h src/qt/macnotificationhandler.h
macx:OBJECTIVE_SOURCES += src/qt/macdockiconhandler.mm src/qt/macnotificationhandler.mm
macx:LIBS += -framework Foundation -framework ApplicationServices -framework AppKit
macx:DEFINES += MAC_OSX MSG_NOSIGNAL=0
macx:ICON = src/qt/res/icons/blocknet.icns
macx:TARGET = "blocknet-Qt"
macx:QMAKE_CFLAGS_THREAD += -pthread
macx:QMAKE_LFLAGS_THREAD += -pthread
macx:QMAKE_CXXFLAGS_THREAD += -pthread

# Set libraries and includes at end, to use platform-defined defaults if not overridden

# -lgdi32 has to happen after -lcrypto (see  #681)
windows:LIBS += -lws2_32 -lshlwapi -lmswsock -lole32 -loleaut32 -luuid -lgdi32
LIBS += \
    -lboost_system$$BOOST_LIB_SUFFIX \
    -lboost_filesystem$$BOOST_LIB_SUFFIX \
    -lboost_program_options$$BOOST_LIB_SUFFIX \
    -lboost_thread$$BOOST_THREAD_LIB_SUFFIX \
    -lboost_date_time$$BOOST_THREAD_LIB_SUFFIX

windows:LIBS += -lboost_chrono$$BOOST_LIB_SUFFIX

contains(RELEASE, 1) {
    !windows:!macx {
        # Linux: turn dynamic linking back on for c/c++ runtime libraries
        LIBS += -Wl,-Bdynamic
    }
}

system($$QMAKE_LRELEASE -silent $$_PRO_FILE_)

DISTFILES += \
    src/qt/paymentrequest.proto \
    src/Makefile.am
