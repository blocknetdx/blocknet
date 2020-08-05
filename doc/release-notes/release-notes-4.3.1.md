# Blocknet Comet 4.3.1 Release Notes

Notable changes
================

- Improved partial orders support
- dxPartialOrderChainDetails rpc call
- dxGetMyPartialOrderChain rpc call
- Fixed order cancellation issues
- Coin Control tree view
- Coin Control keyboard navigation added 
- New Balances screen
- Address book improvements
- Dashboard transaction list fixes
- Support lowercase rpc calls

Changelog
================

- 6af51a592 4.3.1 release
- b6d2e5ceb [xbridge] improved order amount checks
- 9b5b49446 [xbridge] version bump 55
- 974e9d638 [xbridge] partial orders drift check on amounts
- 6804d82bf [xbridge] fix dxPartialOrderChainDetails
- dad60112f [xbridge] use ints in partial order price checks
- 21fd33a0b [xbridge] use correct conversion to xbridge float
- f3169f0e5 Update partial order data help msgs
- 96cf3b6a8 [xbridge] fix partial orders utxo selector
- d7f31014f [xbridge] log partial prep tx
- 4918db724 [core] support case insensitive rpc calls
- 73d7977d6 [xbridge] partial order improvements
- 6d6015978 [core] shellcheck linter update to v0.7.1
- e7199f8cb [gui] fix dashboard table issues
- e4de38e61 [gui] fix duplicate addresses on create
- 651129099 [gui] addressbook create new address title
- 753eb1b07 [gui] show proper address entry on double-click
- d3c636a0a [gui] fix rescan after privkey import
- a7a2e2509 [gui] add blocknet balances page; improve addressbook table
- 38928f64d [gui] display warning on bad pubkey import; fix private key import
- 726873367 [gui] improved coin control tree selection states; context menu fixes
- 33e3d6f84 [xbridge] fix order cancel when order is not mine
- 093003a47 [gui] set table focus on mode switch
- feb31f047 [gui] refactor coin control state mgmt
- fdec95670 [gui] add coin control keyboard navigation
- 4d4b85186 [core] adjust timing of client loading progress
- 768910060 [gui] fix disappearing address label
- 43a417b8d [gui] added coin control tree view to redesign
- 0e5953e7d 4.3.1
