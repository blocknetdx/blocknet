CMAKE_MINIMUM_REQUIRED(VERSION 2.8)

project(tests)

# One # means compiler error
# Two ## means linker error

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/../../")

add_definitions(-DBOOST_TEST_DYN_LINK)

set(UNUSED_TESTS
Checkpoints_tests
##DoS_tests
accounting_tests
#alert_tests
allocator_tests
###arith_uint256_tests
base32_tests
#base58_tests
base64_tests
#bip32_tests
bloom_tests
checkblock_tests
coins_tests
compress_tests
crypto_tests
getarg_tests
hash_tests
key_tests
libzerocoin_tests
main_tests
##mempool_tests
##miner_tests
##mruset_tests
##multisig_tests
#netbase_tests
pmt_tests
rpc_tests
##rpc_wallet_tests
sanity_tests
##script_P2SH_tests
#script_tests
scriptnum_tests
serialize_tests
#sighash_tests
##sigopcount_tests
skiplist_tests
test_pivx
timedata_tests
#transaction_tests
tutorial_zerocoin
#uint256_tests
univalue_tests
util_tests
wallet_tests
)

set(ZERO_SOURCES
  benchmark_zerocoin.cpp
  libzerocoin_tests.cpp
  tutorial_zerocoin.cpp
  zerocoin_transactions_tests.cpp
  zerocoin_implementation_tests.cpp)

find_package(Boost COMPONENTS system filesystem thread program_options unit_test_framework REQUIRED)

include_directories(
	${include_directories} 
  ${Boost_INCLUDE_DIR}
  ${Boost_INCLUDE_DIRS}
	${CMAKE_CURRENT_SOURCE_DIR}/../libzerocoin
	${CMAKE_CURRENT_SOURCE_DIR}
	)

add_executable(zerocoin_unit_tests test_zerocoin.cpp ${ZERO_SOURCES})
target_link_libraries(zerocoin_unit_tests zerocoin coin db_cxx db secp256k1 gmp bitcoin_crypto leveldb ${OPENSSL_LIBRARIES} ${Boost_LIBRARIES} ${ANL} pthread)
  



