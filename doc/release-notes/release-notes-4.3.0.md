# Blocknet Comet 4.3.0 Release Notes

Notable changes
================

- Partial orders support (dxMakePartialOrder)
- dxSplitInputs rpc call
- dxSplitAddress rpc call
- dxGetUtxos rpc call
- Support for XRouter help= parameter
- Persistent XBridge orders (order database)
- Governance database for improved start times
- Fixed hang on dxLoadXBridgeConf

Changelog
================

- 9d592114d [core] incorporate last block height in chain load progress
- 169989685 [xbridge] snode should reject taker on bad price
- 951e5c175 [xbridge] partial orders should use utxos xaddr
- 88c98c563 [xbridge] do not try to repost pending partial orders in trNew state
- bdab8bb6d [xbridge] support stealthcoin
- 9b15039a1 [xbridge] support devault
- 1362c349a [xbridge] support BCD
- ca4c7ba8f Fix xr help messages
- 2152783c2 [xbridge] support BTG
- 987c0608a [xbridge] improve order finished handling
- ddee044d2 [xbridge] improve snode fee processing for dos protection
- bee4aab14 [xbridge] bch support cashaddr
- 8a6d20d5c [xbridge] remove bool return from setters
- 8f5112ddc [xbridge] bch support for forkid and replay protection
- ce39a4399 [xbridge] fix log-tx order spam on retries
- 482765a74 [xbridge] incorporate bch sighash replay protection
- ec59e5331 [xbridge] rebroadcast open orders canceled by snodes
- ce35601a6 [xbridge] snode selection fallback for pending orders
- 40e0cd831 [xbridge] add connection check fallback for failed thread
- 1e507de50 [xbridge] remove fallback min fee
- b4f16aab0 4.3.0 release notes
- 0757b245a [gov] improve initial download time while govdb is active
- dce89241d [gov] use llabs on int64 types
- 192e17f52 [gov] add spent utxo index for tracking invalidated votes
- eabf53fda Update XRouter help messages
- 85a589082 [gov] fix qt voting during cutoff
- 290724d08 [core] update estimated chain size
- 9dd6115c2 [gov] optimize listproposals rpc call
- 3120ff00f [gov] check votes in cutoff period
- 610e17c70 Update XBridge help messages
- 2e855c7b4 [gov] Governance database
- 16d061f4c [core] testnet checkpoint and chainstats
- 007047237 [core] point dockerfile to master branch
- e71d7f4c1 [core] add checkpoint 1522597:1ee481216e8
- c5f04b42b [xbridge] check output for dust to prevent p2sh submission error
- b7f719571 [xbridge] add order id to dxGetUtxos
- dff2b0035 [core] remove v3 to v4 wallet auto copy
- 3fa5e5390 [xbridge] move historical transactions to history on load orders
- c4b26736c [xbridge] avoid spending existing utxos in partial order prep tx
- 45f553a5c [xbridge] fix hang when reloading the conf with open orders
- 9fa4cf8fe [xbridge] storage state fixes
- 198873383 [xbridge] set rollback and finished states on redeem
- b362d8e43 [xbridge] dx orders persistent storage
- 75430c491 [xbridge] add inorder field to dxGetUtxos
- d5cf022f3 [xbridge] remove unused listunspent call
- 4229567b7 [xbridge] dxGetUtxos call
- 14a1ef643 [xbridge] Fix rounding error on input splitter
- 6526e4dce [xbridge] dxSplitInputs call
- 62408645a [xbridge] dxSplitAddress call
- b7f825b90 [xbridge] only broadcast orders will valid utxos
- ab2f8903e [xbridge] partial orders utxo selector
- 29b1419f9 [xbridge] fix partial orders rounding errors
- ca77438aa [xbridge] partial orders improvements
- 3d0a35f91 [core] pause staker on no outgoing peers
- 66985cfd7 [xbridge] partial orders
- 8344117f9 [dev] Gitlab docker executor
- d3349488d [xr] added support for help= parameter
- fa789aaf6 [xr] remove quotes from raw strings in xrouter calls
- 82f6943b6 [core] added wallet lock states to getstakingstatus
