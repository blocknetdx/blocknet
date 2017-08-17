# uncomment this line if build with gui
DEFINES += QT_GUI ENABLE_WALLET

#USE_O3=1

windows {

    #CONFIG += xproxy_enabled

    DEFINES += HAVE_CXX_STDHEADERS
    DEFINES -= __NO_SYSTEM_INCLUDES

    #mingw
    #BOOST_LIB_SUFFIX=-mgw49-mt-d-1_63
    BOOST_LIB_SUFFIX=-mgw53-mt-d-1_64
    #vc
    #BOOST_LIB_SUFFIX=-vc120-mt-1_55

    #BOOST_INCLUDE_PATH=f:/work/boost/boost_1_63_0
    BOOST_INCLUDE_PATH=c:/boost/boost_1_64_0
    #BOOST_LIB_PATH=f:/work/boost/boost_1_63_0/stage/mingw/lib
    BOOST_LIB_PATH=C:/boost/boost_1_64_0/stage/mingw/lib

    BDB_INCLUDE_PATH=d:/work/bitcoin/db-6.0.30/build_unix
    BDB_LIB_PATH=d:/work/bitcoin/db-6.0.30/build_unix
    #BDB_LIB_PATH=D:/work/bitcoin/db-6.0.30/build_windows/Win32/Release

    OPENSSL_INCLUDE_PATH=d:/work/openssl/openssl-1.0.2a-mgw/include
    OPENSSL_LIB_PATH=d:/work/openssl/openssl-1.0.2a-mgw

    MINIUPNPC_INCLUDE_PATH=d:/work/bitcoin/
    MINIUPNPC_LIB_PATH=d:/work/bitcoin/miniupnpc

    LIBPNG_INCLUDE_PATH=c:/deps/libpng-1.6.9
    LIBPNG_LIB_PATH=c:/deps/libpng-1.6.9/.libs

    QRENCODE_INCLUDE_PATH=c:/deps/qrencode-3.4.3
    QRENCODE_LIB_PATH=c:/deps/qrencode-3.4.3/.libs

    QMAKE_CXXFLAGS += -std=c++11

#INCLUDEPATH += \
#    D:/work/bitcoin/secp256k1/src
INCLUDEPATH += \
    D:/work/bitcoin/blockdx/BlockDX/src/secp256k1/include \
    D:/work/protobuf/pb/src/

LIBS += \
    -LD:/work/bitcoin/blockdx/BlockDX/src/secp256k1/.libs \
    -LD:/work/protobuf/protobuf-3.3.0/src/.libs
}

macx {
    BOOST_LIB_SUFFIX = -mt
    BOOST_LIB_PATH = /opt/local/lib
    BOOST_INCLUDE_PATH = /opt/local/include

    BDB_LIB_SUFFIX = -4.8
    BDB_LIB_PATH = /opt/local/lib/db48
    BDB_INCLUDE_PATH = /opt/local/include/db48
}
