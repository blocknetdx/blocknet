(note: this is a temporary file, to be added-to by anybody, and moved to release-notes at release time)

PIVX Core version *version* is now available from:

  <https://github.com/pivx-project/pivx/releases>

This is a new major version release, including various bug fixes and
performance improvements, as well as updated translations.

Please report bugs using the issue tracker at github:

  <https://github.com/pivx-project/pivx/issues>

Compatibility
==============

PIVX Core is extensively tested on multiple operating systems using
the Linux kernel, macOS 10.8+, and Windows Vista and later.

Microsoft ended support for Windows XP on [April 8th, 2014](https://www.microsoft.com/en-us/WindowsForBusiness/end-of-xp-support),
No attempt is made to prevent installing or running the software on Windows XP, you
can still do so at your own risk but be aware that there are known instabilities and issues.
Please do not report issues about Windows XP to the issue tracker.

PIVX Core should also work on most other Unix-like systems but is not
frequently tested on them.

Notable Changes
===============

RPC changes
--------------

#### masternode command
The `masternode` RPC command has been re-worked to ease
it's usage and return valid JSON in it's results. The following is an overview of the changed command parameters:

| Command Parameter | Changes |
| --- | --- |
| `budget` | Removed (did nothing) |
| `count` | The optional "mode" paramater has been removed. Command now always outputs full details in JSON format. |
| `current` | Result fields changed: IP:Port removed, vin renamed to txhash |
| `list-conf` | Result is now an array of objects, instead of an object of objects |
| `outputs` | Result is now an array of objects instead of a list of *n* objects |
| `status` | Added additional fields for txhash, outputidx, netaddr, and message |
| `winners` | Result is now an array of objects instead of a list of *n* objects. See below |
| `list` | Remove all optional "modes" and standardized the results. Note: `masternode list` is the same as `masternodelist`. See below |

For the `winners` parameter, the results are now in a standard JSON format as follows:

```
[
  {
    nHeight: n,           (int) block height
    winner: {
        address: addr,    (string) PIVX MN Address,
        nVotes: n,        (int) Number of votes for winner,
    }
  },
  ...
]
```

In the case of multiple winners being associated with a single block, the results are in the following format (the `winner` object becomes an array of objects):

```
[
  {
    nHeight: n,           (int) block height,
    winner: [
      {
        address: addr,    (string) PIVX MN Address,
        nVotes: n,        (int) Number of votes for winner,
      },
      ...
    ]
  },
  ...
]
```

For the `list` (aka `masternodelist`) parameter, the vairous "modes" have been removed in favor of a unified and standardized result format. The result is now an array of objects instead of an object of objects. Further, the individual objects now have a standard JSON format. The result format is as follows:

```
[
  {
    "rank": n,         (numeric) Masternode rank (or 0 if not enabled)
    "txhash": hash,    (string) Collateral transaction hash
    "outidx": n,       (numeric) Collateral transaction output index
    "status": s,       (string) Status (ENABLED/EXPIRED/REMOVE/etc)
    "addr": addr,      (string) Masternode PIVX address
    "version": v,      (numeric) Masternode Protocol version
    "lastseen": ttt,   (numeric) The time in seconds since epoch (Jan 1 1970 GMT) the masternode was last seen
    "activetime": ttt, (numeric) The time in seconds since epoch (Jan 1 1970 GMT) masternode has been active
    "lastpaid": ttt,   (numeric) The time in seconds since epoch (Jan 1 1970 GMT) masternode was last paid
  },
  ...
]
```

#### mnbudget command

An additional parameter has been added to `mnbudget` to allow a controller wallet to issue per-MN votes. The new parameter is `vote-alias` and it's use format is as follows:

`mnbudget vote-alias <proposal-hash> <yes|no> <alias>`

All fields are required to successfully vote.

#### walletpassphrase command

CLI users that are staking their coins will now have the option of unlocking the wallet with no re-lock timeout. Similar to using `9999999` as the timeout, the `walletpassphrase` command now accepts `0` as a timeout to indicate that no re-locking should occur based on elapsed time.

Usage: `walletpassphrase <passphrase> 0 <true|false>`

The third parameter indicates if the wallet should be unlocked for staking and anonymization only (true), or to allow send operations (false, full unlock).

ZeroMQ (ZMQ) Notifications
--------------

pivxd can now (optionally) asynchronously notify clients through a ZMQ-based PUB socket of the arrival of new transactions and blocks. This feature requires installation of the ZMQ C API library 4.x and configuring its use through the command line or configuration file. Please see [docs/zmq.md](/doc/zmq.md) for details of operation.

**All** Masternodes List GUI Removal
--------------

With the standardization and reformatting of the `masternode list` (`masternodelist`) RPC command, there is no real use case to keep the full list of masternodes in the GUI. This GUI element causes a great deal of extra overhead, even when it is not being actively displayed. The removal of this list has also proven to resolve a number of linux-based errors

Note that the GUI list of masternodes associated with a controller wallet remains intact.

*version* Change log
=================

Detailed release notes follow. This overview includes changes that affect
behavior, not code moves, refactors and string updates. For convenience in locating
the code changes and accompanying discussion, both the pull request and
git merge commit are mentioned.

### RPC and other APIs


### Block and Transaction Handling


### P2P Protocol and Network Code


### GUI


### Miscellaneous


Credits
=======

Thanks to everyone who directly contributed to this release:


As well as everyone that helped translating on [Transifex](https://www.transifex.com/projects/p/pivx-project-translations/).
