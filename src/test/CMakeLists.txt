CMAKE_MINIMUM_REQUIRED(VERSION 2.8)

project(tests)

# One # means compiler error
# Two ## means linker error

set(TESTP 
Checkpoints_tests
##DoS_tests
accounting_tests
#alert_tests
allocator_tests
arith_uint256_tests
base32_tests
#base58_tests
base64_tests
benchmark_zerocoin
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

include_directories(
	${include_directories} 
	${CMAKE_CURRENT_SOURCE_DIR}/../libzerocoin
	${CMAKE_CURRENT_SOURCE_DIR}
	)

find_package(Boost COMPONENTS system filesystem thread program_options unit_test_framework REQUIRED)

FOREACH(t ${TESTP})
	add_executable(${t} ${t}.cpp)
	target_link_libraries(${t} coin db_cxx db secp256k1 gmp bitcoin_crypto leveldb zerocoin ${OPENSSL_LIBRARIES} ${Boost_LIBRARIES} ${ANL} pthread)
ENDFOREACH(t ${TESTP})




