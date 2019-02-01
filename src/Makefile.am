# Copyright (c) 2013-2016 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

DIST_SUBDIRS = secp256k1
CXXFLAGS += -fpermissive
AM_LDFLAGS = $(PTHREAD_CFLAGS) $(LIBTOOL_LDFLAGS) $(HARDENED_LDFLAGS) $(GPROF_LDFLAGS) $(SANITIZER_LDFLAGS)
AM_CXXFLAGS = $(DEBUG_CXXFLAGS) $(HARDENED_CXXFLAGS) $(WARN_CXXFLAGS) $(NOWARN_CXXFLAGS) $(ERROR_CXXFLAGS) $(GPROF_CXXFLAGS) $(SANITIZER_CXXFLAGS)
AM_CPPFLAGS = $(DEBUG_CPPFLAGS) $(HARDENED_CPPFLAGS)
AM_LIBTOOLFLAGS = --preserve-dup-deps

BITCOIN_CONFIG_INCLUDES=-I$(builddir)/config
BITCOIN_INCLUDES=-I$(builddir) -I$(builddir)/obj $(BDB_CPPFLAGS) $(BOOST_CPPFLAGS) $(LEVELDB_CPPFLAGS) $(CRYPTO_CFLAGS) $(SSL_CFLAGS)

BITCOIN_INCLUDES += -I$(srcdir)/secp256k1/include

LIBBITCOIN_SERVER=libbitcoin_server.a
LIBBITCOIN_WALLET=libbitcoin_wallet.a
LIBBITCOIN_COMMON=libbitcoin_common.a
LIBBITCOIN_CLI=libbitcoin_cli.a
LIBBITCOIN_UTIL=libbitcoin_util.a
LIBBITCOIN_CRYPTO=crypto/libbitcoin_crypto.a
LIBBITCOIN_UNIVALUE=univalue/libbitcoin_univalue.a
LIBBITCOINQT=qt/libbitcoinqt.a
LIBSECP256K1=secp256k1/libsecp256k1.la
LIBXBRIDGE_XBRIDGE=libxbridge_xbridge.a
LIBXROUTER_XROUTER=libxrouter_xrouter.a

$(LIBSECP256K1): $(wildcard secp256k1/src/*) $(wildcard secp256k1/include/*)
	$(AM_V_at)$(MAKE) $(AM_MAKEFLAGS) -C $(@D) $(@F)

# Make is not made aware of per-object dependencies to avoid limiting building parallelization
# But to build the less dependent modules first, we manually select their order here:
EXTRA_LIBRARIES = \
  crypto/libbitcoin_crypto.a \
  libbitcoin_util.a \
  libbitcoin_common.a \
  univalue/libbitcoin_univalue.a \
  libxbridge_xbridge.a \
  libxrouter_xrouter.a \
  libbitcoin_server.a \
  libbitcoin_cli.a

if ENABLE_WALLET
BITCOIN_INCLUDES += $(BDB_CPPFLAGS)
EXTRA_LIBRARIES += libbitcoin_wallet.a
endif

if ENABLE_ZMQ
EXTRA_LIBRARIES += libbitcoin_zmq.a
endif

if BUILD_BITCOIN_LIBS
lib_LTLIBRARIES = libbitcoinconsensus.la
LIBBITCOIN_CONSENSUS=libbitcoinconsensus.la
else
LIBBITCOIN_CONSENSUS=
endif

bin_PROGRAMS =
TESTS =

if BUILD_BITCOIND
  bin_PROGRAMS += blocknetdxd
endif

if BUILD_BITCOIN_UTILS
  bin_PROGRAMS += blocknetdx-cli blocknetdx-tx
endif

.PHONY: FORCE
# blocknetdx core #
BITCOIN_CORE_H = \
  activeservicenode.h \
  addrman.h \
  alert.h \
  allocators.h \
  amount.h \
  base58.h \
  bip38.h \
  bloom.h \
  chain.h \
  chainparams.h \
  chainparamsbase.h \
  chainparamsseeds.h \
  checkpoints.h \
  checkqueue.h \
  clientversion.h \
  coincontrol.h \
  coins.h \
  compat.h \
  compat/endian.h \
  compat/byteswap.h \
  compat/sanity.h \
  compressor.h \
  primitives/block.h \
  primitives/transaction.h \
  core_io.h \
  crypter.h \
  currency.h \
  currencypair.h \
  obfuscation.h \
  obfuscation-relay.h \
  db.h \
  eccryptoverify.h \
  hash.h \
  init.h \
  kernel.h \
  swifttx.h \
  key.h \
  keystore.h \
  leveldbwrapper.h \
  limitedmap.h \
  main.h \
  servicenode.h \
  servicenode-payments.h \
  servicenode-budget.h \
  servicenode-sync.h \
  servicenodeman.h \
  servicenodeconfig.h \
  merkleblock.h \
  miner.h \
  mruset.h \
  netbase.h \
  net.h \
  noui.h \
  pow.h \
  protocol.h \
  ptr.h \
  pubkey.h \
  random.h \
  rpcclient.h \
  rpcprotocol.h \
  rpcserver.h \
  script/interpreter.h \
  script/script.h \
  script/sigcache.h \
  script/sign.h \
  script/standard.h \
  script/script_error.h \
  serialize.h \
  spork.h \
  ssliostreamdevice.h \
  streams.h \
  support/cleanse.h \
  sync.h \
  threadsafety.h \
  timedata.h \
  tinyformat.h \
  txdb.h \
  txmempool.h \
  ui_interface.h \
  uint256.h \
  undo.h \
  util.h \
  utilstrencodings.h \
  utilmoneystr.h \
  utiltime.h \
  validationinterface.h \
  validationstate.h \
  version.h \
  wallet.h \
  wallet_ismine.h \
  walletdb.h \
  zmq/zmqabstractnotifier.h \
  zmq/zmqconfig.h \
  zmq/zmqnotificationinterface.h \
  zmq/zmqpublishnotifier.h \
  FastDelegate.h \
  arith_uint256.h \
  coinvalidator.h \
  s3downloader.h

JSON_H = \
  json/json_spirit.h \
  json/json_spirit_error_position.h \
  json/json_spirit_reader.h \
  json/json_spirit_reader_template.h \
  json/json_spirit_stream_reader.h \
  json/json_spirit_utils.h \
  json/json_spirit_value.h \
  json/json_spirit_writer.h \
  json/json_spirit_writer_template.h \
  json/json_spirit_writer_options.h

obj/build.h: FORCE
	@$(MKDIR_P) $(builddir)/obj
	@$(top_srcdir)/share/genbuild.sh $(abs_top_builddir)/src/obj/build.h \
	  $(abs_top_srcdir)
libbitcoin_util_a-clientversion.$(OBJEXT): obj/build.h

# server: shared between blocknetdxd and blocknetdx-qt
libbitcoin_server_a_CPPFLAGS = $(AM_CPPFLAGS) $(BITCOIN_INCLUDES) $(MINIUPNPC_CPPFLAGS) $(EVENT_CFLAGS) $(EVENT_PTHREADS_CFLAGS)
libbitcoin_server_a_SOURCES = \
  addrman.cpp \
  alert.cpp \
  bloom.cpp \
  chain.cpp \
  checkpoints.cpp \
  init.cpp \
  leveldbwrapper.cpp \
  main.cpp \
  merkleblock.cpp \
  miner.cpp \
  net.cpp \
  noui.cpp \
  pow.cpp \
  rest.cpp \
  rpcblockchain.cpp \
  rpcservicenode.cpp \
  rpcservicenode-budget.cpp \
  rpcmining.cpp \
  rpcmisc.cpp \
  rpcnet.cpp \
  rpcrawtransaction.cpp \
  rpcserver.cpp \
  script/sigcache.cpp \
  timedata.cpp \
  txdb.cpp \
  txmempool.cpp \
  validationinterface.cpp \
  $(JSON_H) \
  $(BITCOIN_CORE_H)

if ENABLE_ZMQ
LIBBITCOIN_ZMQ=libbitcoin_zmq.a

libbitcoin_zmq_a_CPPFLAGS = $(BITCOIN_INCLUDES) $(ZMQ_CFLAGS)
libbitcoin_zmq_a_CXXFLAGS = $(AM_CXXFLAGS) $(PIE_FLAGS)
libbitcoin_zmq_a_SOURCES = \
  zmq/zmqabstractnotifier.cpp \
  zmq/zmqnotificationinterface.cpp \
  zmq/zmqpublishnotifier.cpp
endif

# wallet: shared between blocknetdxd and blocknetdx-qt, but only linked
# when wallet enabled

libbitcoin_wallet_a_CFLAGS = -fPIC
libbitcoin_wallet_a_CPPFLAGS = $(AM_CPPFLAGS) $(BITCOIN_INCLUDES)
libbitcoin_wallet_a_SOURCES = \
  activeservicenode.cpp \
  bip38.cpp \
  obfuscation.cpp \
  obfuscation-relay.cpp \
  db.cpp \
  crypter.cpp \
  swifttx.cpp \
  servicenode.cpp \
  servicenode-budget.cpp \
  servicenode-payments.cpp \
  servicenode-sync.cpp \
  servicenodeconfig.cpp \
  servicenodeman.cpp \
  rpcdump.cpp \
  rpcwallet.cpp \
  kernel.cpp \
  wallet.cpp \
  wallet_ismine.cpp \
  walletdb.cpp \
  $(BITCOIN_CORE_H)

# crypto primitives library
crypto_libbitcoin_crypto_a_CFLAGS = -fPIC
crypto_libbitcoin_crypto_a_CPPFLAGS = $(AM_CPPFLAGS) $(BITCOIN_CONFIG_INCLUDES)
crypto_libbitcoin_crypto_a_SOURCES = \
  crypto/sha1.cpp \
  crypto/sha256.cpp \
  crypto/sha512.cpp \
  crypto/chacha20.h \
  crypto/chacha20.cpp \
  crypto/hmac_sha256.cpp \
  crypto/rfc6979_hmac_sha256.cpp \
  crypto/hmac_sha512.cpp \
  crypto/scrypt.cpp \
  crypto/ripemd160.cpp \
  crypto/aes_helper.c \
  crypto/blake.c \
  crypto/bmw.c \
  crypto/groestl.c \
  crypto/jh.c \
  crypto/keccak.c \
  crypto/skein.c \
  crypto/common.h \
  crypto/sha256.h \
  crypto/sha512.h \
  crypto/hmac_sha256.h \
  crypto/rfc6979_hmac_sha256.h \
  crypto/hmac_sha512.h \
  crypto/scrypt.h \
  crypto/sha1.h \
  crypto/ripemd160.h \
  crypto/sph_blake.h \
  crypto/sph_bmw.h \
  crypto/sph_groestl.h \
  crypto/sph_jh.h \
  crypto/sph_keccak.h \
  crypto/sph_skein.h \
  crypto/sph_types.h


# univalue JSON library
univalue_libbitcoin_univalue_a_SOURCES = \
  univalue/univalue.cpp \
  univalue/univalue_read.cpp \
  univalue/univalue_write.cpp \
  univalue/univalue_escapes.h \
  univalue/univalue.h

# common: shared between blocknetdxd, and blocknetdx-qt and non-server tools
libbitcoin_common_a_CPPFLAGS = $(AM_CPPFLAGS) $(BITCOIN_INCLUDES)
libbitcoin_common_a_SOURCES = \
  allocators.cpp \
  amount.cpp \
  base58.cpp \
  bip38.cpp \
  chainparams.cpp \
  coins.cpp \
  compressor.cpp \
  primitives/block.cpp \
  primitives/transaction.cpp \
  core_read.cpp \
  core_write.cpp \
  eccryptoverify.cpp \
  hash.cpp \
  key.cpp \
  keystore.cpp \
  netbase.cpp \
  protocol.cpp \
  pubkey.cpp \
  script/interpreter.cpp \
  script/script.cpp \
  script/sign.cpp \
  script/standard.cpp \
  script/script_error.cpp \
  spork.cpp \
  coinvalidator.cpp \
  s3downloader.cpp \
  $(BITCOIN_CORE_H)

#libxbridge_xbridge_a_CPPFLAGS = $(BITCOIN_INCLUDES) $(MINIUPNPC_CPPFLAGS)
libxbridge_xbridge_a_CPPFLAGS =  $(BITCOIN_INCLUDES)
libxbridge_xbridge_a_SOURCES = \
  xbridge/util/settings.cpp \
  xbridge/util/logger.cpp \
  xbridge/util/txlog.cpp \
  xbridge/util/xseries.cpp \
  xbridge/util/xutil.cpp \
  xbridge/util/xbridgeerror.cpp \
  xbridge/bitcoinrpcconnector.cpp \
  xbridge/xbridgepacket.cpp \
  xbridge/xbridgeapp.cpp \
  xbridge/xbridgeexchange.cpp \
  xbridge/xbridgesession.cpp \
  xbridge/xbridgetransaction.cpp \
  xbridge/xbridgetransactiondescr.cpp \
  xbridge/xbridgetransactionmember.cpp \
  xbridge/xbridgewalletconnector.cpp \
  xbridge/xbridgewalletconnectorbtc.cpp \
  xbridge/xbridgecryptoproviderbtc.cpp \
  xbridge/xbridgewalletconnectorbch.cpp \
  xbridge/xbridgewalletconnectordgb.cpp \
  xbridge/xbitcoinaddress.cpp \
  xbridge/xbitcointransaction.cpp \
  xbridge/rpcxbridge.cpp \
  xbridge/posixtimeconversion.cpp \
  xbridge/bitcoinrpcconnector.h \
  xbridge/config.h \
  xbridge/version.h \
  xbridge/xbitcoinaddress.h \
  xbridge/xbitcointransaction.h \
  xbridge/xbridgedef.h \
  xbridge/xbridgeapp.h \
  xbridge/xbridgeexchange.h \
  xbridge/xbridgepacket.h \
  xbridge/xbridgerpc.h \
  xbridge/xbridgesession.h \
  xbridge/xbridgetransaction.h \
  xbridge/xbridgetransactiondescr.h \
  xbridge/xbridgetransactionmember.h \
  xbridge/xbridgewalletconnector.h \
  xbridge/xbridgewalletconnectorbtc.h \
  xbridge/xbridgecryptoproviderbtc.h \
  xbridge/xbridgewalletconnectorbch.h \
  xbridge/xbridgewalletconnectordgb.h \
  xbridge/xbridgewallet.h \
  xbridge/xuiconnector.h \
  xbridge/util/logger.h \
  xbridge/util/settings.h \
  xbridge/util/txlog.h \
  xbridge/util/xassert.h \
  xbridge/util/xseries.h \
  xbridge/util/xutil.h \
  xbridge/util/xbridgeerror.h \
  xbridge/posixtimeconversion.h \
  $(BITCOIN_CORE_H)

libxrouter_xrouter_a_CPPFLAGS =  $(BITCOIN_INCLUDES)
libxrouter_xrouter_a_SOURCES = \
  xrouter/xrouterapp.cpp \
  xrouter/xrouterapp.h \
  xrouter/xrouterserver.cpp \
  xrouter/xrouterserver.h \
  xrouter/version.h \
  xrouter/xrouterdef.h \
  xrouter/rpcxrouter.cpp \
  xrouter/xrouterpacket.cpp \
  xrouter/xrouterpacket.h \
  xrouter/xrouterconnector.h \
  xrouter/xrouterconnector.cpp \
  xrouter/xrouterconnectorbtc.h \
  xrouter/xrouterconnectorbtc.cpp \
  xrouter/xrouterconnectoreth.h \
  xrouter/xrouterconnectoreth.cpp \
  xrouter/xroutererror.h \
  xrouter/xrouterpeer.h \
  xrouter/xrouterlogger.h \
  xrouter/xrouterlogger.cpp \
  xrouter/xroutersettings.h \
  xrouter/xroutersettings.cpp \
  xrouter/xrouterutils.h \
  xrouter/utils-domains.cpp \
  xrouter/utils-misc.cpp \
  xrouter/utils-network.cpp \
  xrouter/utils-payments.cpp \
  xrouter/utils-paymentchannels.cpp \
  xrouter/utils-verify.cpp \
  $(BITCOIN_CORE_H)


# util: shared between all executables.
# This library *must* be included to make sure that the glibc
# backward-compatibility objects and their sanity checks are linked.
libbitcoin_util_a_CPPFLAGS = $(AM_CPPFLAGS) $(BITCOIN_INCLUDES)
libbitcoin_util_a_SOURCES = \
  allocators.cpp \
  compat/strnlen.cpp \
  compat/glibc_sanity.cpp \
  compat/glibcxx_sanity.cpp \
  chainparamsbase.cpp \
  clientversion.cpp \
  random.cpp \
  rpcprotocol.cpp \
  sync.cpp \
  uint256.cpp \
  util.cpp \
  utilstrencodings.cpp \
  utilmoneystr.cpp \
  utiltime.cpp \
  support/cleanse.cpp \
  $(BITCOIN_CORE_H)

if GLIBC_BACK_COMPAT
libbitcoin_util_a_SOURCES += compat/glibc_compat.cpp
libbitcoin_util_a_SOURCES += compat/glibcxx_compat.cpp
AM_LDFLAGS += $(COMPAT_LDFLAGS)
endif

# cli: shared between blocknetdx-cli and blocknetdx-qt
libbitcoin_cli_a_CPPFLAGS = $(AM_CPPFLAGS) $(BITCOIN_INCLUDES)
libbitcoin_cli_a_SOURCES = \
  rpcclient.cpp \
  $(BITCOIN_CORE_H)

nodist_libbitcoin_util_a_SOURCES = $(srcdir)/obj/build.h
#

# bitcoind binary #
blocknetdxd_LDADD = \
  $(LIBBITCOIN_SERVER) \
  $(LIBBITCOIN_COMMON) \
  $(LIBBITCOIN_UNIVALUE) \
  $(LIBBITCOIN_UTIL) \
  $(LIBBITCOIN_CRYPTO) \
  $(LIBLEVELDB) \
  $(LIBLEVELDB_SSE42) \
  $(LIBMEMENV) \
  $(LIBSECP256K1) \
  $(LIBXBRIDGE_XBRIDGE) \
  $(LIBXROUTER_XROUTER) \
  $(LIBBITCOIN_CLI)

if ENABLE_ZMQ
blocknetdxd_LDADD += $(LIBBITCOIN_ZMQ) $(ZMQ_LIBS)
endif

if ENABLE_WALLET
blocknetdxd_LDADD += libbitcoin_wallet.a
endif
blocknetdxd_SOURCES = blocknetdxd.cpp
#

if TARGET_WINDOWS
blocknetdxd_SOURCES += blocknetdxd-res.rc
endif

blocknetdxd_LDADD += $(BOOST_LIBS) $(BDB_LIBS) $(SSL_LIBS) $(CRYPTO_LIBS) $(MINIUPNPC_LIBS)
blocknetdxd_CPPFLAGS = $(BITCOIN_INCLUDES)
blocknetdxd_LDFLAGS = $(RELDFLAGS) $(AM_LDFLAGS) $(LIBTOOL_APP_LDFLAGS)


# blocknetdx-cli binary #
blocknetdx_cli_LDADD = \
  $(LIBBITCOIN_CLI) \
  $(LIBBITCOIN_UTIL) \
  $(BOOST_LIBS) \
  $(SSL_LIBS) \
  $(CRYPTO_LIBS) \
  $(LIBBITCOIN_CRYPTO)


blocknetdx_cli_SOURCES = \
  blocknetdx-cli.cpp

blocknetdx_cli_CPPFLAGS = $(BITCOIN_INCLUDES)
#

# blocknetdx-tx binary #
blocknetdx_tx_LDADD = \
  $(LIBBITCOIN_UNIVALUE) \
  $(LIBBITCOIN_COMMON) \
  $(LIBBITCOIN_UTIL) \
  $(LIBBITCOIN_CRYPTO) \
  $(LIBSECP256K1) \
  $(BOOST_LIBS) \
  $(CRYPTO_LIBS)

blocknetdx_tx_SOURCES = blocknetdx-tx.cpp
blocknetdx_tx_CPPFLAGS = $(BITCOIN_INCLUDES)
#
blocknetdx_tx_LDFLAGS = $(RELDFLAGS) $(AM_LDFLAGS) $(LIBTOOL_APP_LDFLAGS)

if TARGET_WINDOWS
blocknetdx_cli_SOURCES += blocknetdx-cli-res.rc
endif
blocknetdx_cli_LDFLAGS = $(RELDFLAGS) $(AM_LDFLAGS) $(LIBTOOL_APP_LDFLAGS)

if BUILD_BITCOIN_LIBS
include_HEADERS = script/bitcoinconsensus.h
libbitcoinconsensus_la_SOURCES = \
  allocators.cpp \
  primitives/transaction.cpp \
  crypto/hmac_sha512.cpp \
  crypto/scrypt.cpp \
  crypto/sha1.cpp \
  crypto/sha256.cpp \
  crypto/sha512.cpp \
  crypto/ripemd160.cpp \
  eccryptoverify.cpp \
  hash.cpp \
  pubkey.cpp \
  script/script.cpp \
  script/interpreter.cpp \
  script/bitcoinconsensus.cpp \
  uint256.cpp \
  utilstrencodings.cpp

if GLIBC_BACK_COMPAT
  libbitcoinconsensus_la_SOURCES += compat/glibc_compat.cpp
  libbitcoinconsensus_la_SOURCES += compat/glibcxx_compat.cpp
endif

libbitcoinconsensus_la_LDFLAGS = $(AM_LDFLAGS) -no-undefined $(RELDFLAGS)
libbitcoinconsensus_la_LIBADD = $(CRYPTO_LIBS) $(BOOST_LIBS)
libbitcoinconsensus_la_CPPFLAGS = $(CRYPTO_CFLAGS) $(BITCOIN_INCLUDES) -I$(builddir)/obj -DBUILD_BITCOIN_INTERNAL
libbitcoinconsensus_la_LIBADD += secp256k1/libsecp256k1.la
endif

CLEANFILES = leveldb/libleveldb.a leveldb/libmemenv.a
CLEANFILES += *.gcda *.gcno
CLEANFILES += compat/*.gcda compat/*.gcno
CLEANFILES += crypto/*.gcda crypto/*.gcno
CLEANFILES += primitives/*.gcda primitives/*.gcno
CLEANFILES += script/*.gcda script/*.gcno
CLEANFILES += univalue/*.gcda univalue/*.gcno
CLEANFILES += zmq/*.gcda zmq/*.gcno

DISTCLEANFILES = obj/build.h

EXTRA_DIST = leveldb

clean-local:
	-$(MAKE) -C leveldb clean
	-$(MAKE) -C secp256k1 clean
	rm -f leveldb/*/*.gcno leveldb/helpers/memenv/*.gcno
	-rm -f config.h

.rc.o:
	@test -f $(WINDRES)
	$(AM_V_GEN) $(WINDRES) -DWINDRES_PREPROC -i $< -o $@

.mm.o:
	$(AM_V_CXX) $(OBJCXX) $(DEFS) $(DEFAULT_INCLUDES) $(INCLUDES) $(AM_CPPFLAGS) \
	  $(CPPFLAGS) $(AM_CXXFLAGS) $(QT_INCLUDES) $(CXXFLAGS)  -c -o $@ $<

%.pb.cc %.pb.h: %.proto
	@test -f $(PROTOC)
	$(AM_V_GEN) $(PROTOC) --cpp_out=$(@D) --proto_path=$(abspath $(<D) $<)

if EMBEDDED_LEVELDB
include Makefile.leveldb.include
endif

if ENABLE_TESTS
include Makefile.test.include
endif

if ENABLE_QT
include Makefile.qt.include
endif

if ENABLE_QT_TESTS
include Makefile.qttest.include
endif
