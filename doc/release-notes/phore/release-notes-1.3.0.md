1.3.0 Release notes
====================

**THIS IS A MANDATORY UPDATE. UPGRADE YOUR CLIENTS BEFORE BLOCK 400,000.**

Phore Core version 1.3.0 is now available from:

  https://phore.io/

Please report bugs using the issue tracker at github:

  https://github.com/phoreproject/phore/issues


How to Upgrade
--------------

If you are running an older version, shut it down. Wait until it has completely
shut down (which might take a few minutes for older versions), then run the
installer (on Windows) or just copy over /Applications/Phore-Qt (on Mac) or
phored/phore-qt (on Linux).


1.3.0 changelog
----------------

- Add Segregated Witness support
  - Add SPORK_17_SEGWIT
  - Update messaging system to current Bitcoin Core 0.16
  - Update GetTransactionSigOpCost to count witness transactions at 1/4 the normal cost
  - Add NODE_WITNESS version flag for nodes
  - Fix RPC tests and RegTest networks
    - Add tests for wallet
    - Add tests for segwit
  - Update wallet to find SegWit transactions
- Merge with PIVX upstream v3.1.0
  - Make Zerocoin deterministic
  - Fix wallet bugs causing corruption
  - Fix Zerocoin modulus bug
  - Fix bugs in RPC commands causing incorrect responses
- Update logos
- Add CHECKLOCKTIMEVERIFY support
- Add CHECKSEQUENCEVERIFY support
- Add bech32 addresses


Credits
--------

- BlankGT
- Julian Meyer
- barrystyle
- tohsnoom
- wqking

As well as the entire Bitcoin, Dash, and PIVX teams!