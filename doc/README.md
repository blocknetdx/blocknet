Blocknet
=====================

Setup
---------------------
[Blocknet](https://github.com/BlocknetDX/blocknet/releases/tag/v3.9.22) is the original Blocknet client and it builds the backbone of the network. However, it downloads and stores the entire history of Blocknet transactions; depending on the speed of your computer and network connection, the synchronization process can take anywhere from a few hours to a day or more. Thankfully you only have to do this once.

Running
---------------------
The following are some helpful notes on how to run Blocknet on your native platform.

### Unix

Unpack the files into a directory and run:

- bin/32/blocknetdx-qt (GUI, 32-bit) or bin/32/blocknetdxd (headless, 32-bit)
- bin/64/blocknetdx-qt (GUI, 64-bit) or bin/64/blocknetdxd (headless, 64-bit)

### Windows

Unpack the files into a directory, and then run blocknetdx-qt.exe.

### OSX

Drag Blocknet to your applications folder, and then run Blocknet.

### Need Help?

* Join the [Blocknet Discord](https://discord.gg/2e6s7H8) and ask for help in the #wallet channel.

Building
---------------------
The following are developer notes on how to build Blocknet on your native platform. They are not complete guides, but include notes on the necessary libraries, compile flags, etc.

- [OSX Build Notes](build-osx.md)
- [Unix Build Notes](build-unix.md)
- [Gitian Building Guide](gitian-building.md)

Development
---------------------
The BlocknetDX/BlockDX repo's [root README](https://github.com/BlocknetDX/blocknet/blob/master/README.md) contains relevant information on the development process and automated testing.

- [Developer Notes](developer-notes.md)
- [Multiwallet Qt Development](multiwallet-qt.md)
- [Release Notes](release-notes.md)
- [Release Process](release-process.md)
- [Source Code Documentation (External Link)](https://dev.visucore.com/bitcoin/doxygen/) ***TODO***
- [Translation Process](translation_process.md)
- [Unit Tests](unit-tests.md)
- [Unauthenticated REST Interface](REST-interface.md)
- [Dnsseed Policy](dnsseed-policy.md)

### Resources

* Website: [https://blocknet.co](https://blocknet.co)
* Discord: [https://discord.gg/2e6s7H8](https://discord.gg/2e6s7H8)
* API Docs: [https://api.blocknet.co](https://api.blocknet.co)
* Documentation Portal: [https://docs.blocknet.co](https://docs.blocknet.co)

### Miscellaneous
- [Assets Attribution](assets-attribution.md)
- [Files](files.md)
- [Tor Support](tor.md)
- [Init Scripts (systemd/upstart/openrc)](init.md)

License
---------------------
Distributed under the [MIT/X11 software license](http://www.opensource.org/licenses/mit-license.php).
This product includes software developed by the OpenSSL Project for use in the [OpenSSL Toolkit](https://www.openssl.org/). This product includes
cryptographic software written by Eric Young ([eay@cryptsoft.com](mailto:eay@cryptsoft.com)), and UPnP software written by Thomas Bernard.
