Release Process
====================

* * *

###update (commit) version in sources

	darkcoin-qt.pro
	contrib/verifysfbinaries/verify.sh
	doc/README*
	share/setup.nsi
	src/clientversion.h (change CLIENT_VERSION_IS_RELEASE to true)

###tag version in git

	git tag -s v(new version, e.g. 0.9.11.0)

###write release notes. git shortlog helps a lot, for example:

	git shortlog --no-merges v(current version, e.g. 0.9.10.0)..v(new version, e.g. 0.9.11.0)

* * *

##perform gitian builds

 From a directory containing the darkcoin source, gitian-builder and gitian.sigs
  
	export SIGNER=(your gitian key, ie bluematt, sipa, etc)
	export VERSION=(new version, e.g. 0.9.11.0)
	pushd ./darkcoin
	git checkout v${VERSION}
	popd
	pushd ./gitian-builder
        mkdir -p inputs; cd inputs/

 Register and download the Apple SDK (see OSX Readme for details)
	visit https://developer.apple.com/downloads/download.action?path=Developer_Tools/xcode_4.6.3/xcode4630916281a.dmg
 
 Using a Mac, create a tarball for the 10.7 SDK
	tar -C /Volumes/Xcode/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/ -czf MacOSX10.7.sdk.tar.gz MacOSX10.7.sdk

 Fetch and build inputs: (first time, or when dependency versions change)

	wget 'http://miniupnp.free.fr/files/download.php?file=miniupnpc-1.9.tar.gz' -O miniupnpc-1.9.tar.gz
	wget 'https://www.openssl.org/source/openssl-1.0.1h.tar.gz'
	wget 'http://download.oracle.com/berkeley-db/db-4.8.30.NC.tar.gz'
	wget 'http://zlib.net/zlib-1.2.8.tar.gz'
	wget 'ftp://ftp.simplesystems.org/pub/png/src/history/libpng16/libpng-1.6.8.tar.gz'
	wget 'https://fukuchi.org/works/qrencode/qrencode-3.4.3.tar.bz2'
	wget 'https://downloads.sourceforge.net/project/boost/boost/1.55.0/boost_1_55_0.tar.bz2'
	wget 'https://svn.boost.org/trac/boost/raw-attachment/ticket/7262/boost-mingw.patch' -O boost-mingw-gas-cross-compile-2013-03-03.patch
	wget 'https://download.qt-project.org/official_releases/qt/5.2/5.2.0/single/qt-everywhere-opensource-src-5.2.0.tar.gz'
	wget 'https://download.qt-project.org/official_releases/qt/5.2/5.2.1/single/qt-everywhere-opensource-src-5.2.1.tar.gz'
	wget 'https://download.qt-project.org/archive/qt/4.6/qt-everywhere-opensource-src-4.6.4.tar.gz'
	wget 'https://download.qt-project.org/archive/qt/4.8/4.8.5/qt-everywhere-opensource-src-4.8.5.tar.gz'
	wget 'https://protobuf.googlecode.com/files/protobuf-2.5.0.tar.bz2'
	wget 'https://github.com/mingwandroid/toolchain4/archive/10cc648683617cca8bcbeae507888099b41b530c.tar.gz'
	wget 'http://www.opensource.apple.com/tarballs/cctools/cctools-809.tar.gz'
	wget 'http://www.opensource.apple.com/tarballs/dyld/dyld-195.5.tar.gz'
	wget 'http://www.opensource.apple.com/tarballs/ld64/ld64-127.2.tar.gz'
	wget 'http://cdrkit.org/releases/cdrkit-1.1.11.tar.gz'
	wget 'https://github.com/theuni/libdmg-hfsplus/archive/libdmg-hfsplus-v0.1.tar.gz'
	wget 'http://llvm.org/releases/3.2/clang+llvm-3.2-x86-linux-ubuntu-12.04.tar.gz' -O clang-llvm-3.2-x86-linux-ubuntu-12.04.tar.gz
	wget 'https://raw.githubusercontent.com/theuni/osx-cross-depends/master/patches/cdrtools/genisoimage.diff' -O cdrkit-deterministic.patch
	cd ..
	./bin/gbuild ../darkcoin/contrib/gitian-descriptors/linux/gitian-linux-boost.yml
	mv build/out/boost-*.zip inputs/
	./bin/gbuild ../darkcoin/contrib/gitian-descriptors/linux/gitian-linux-deps.yml
	mv build/out/bitcoin-deps-*.zip inputs/
	./bin/gbuild ../darkcoin/contrib/gitian-descriptors/linux/gitian-linux-qt.yml
	mv build/out/qt-*.tar.gz inputs/
	./bin/gbuild ../darkcoin/contrib/gitian-descriptors/windows/gitian-win-boost.yml
	mv build/out/boost-*.zip inputs/
	./bin/gbuild ../darkcoin/contrib/gitian-descriptors/windows/gitian-win-deps.yml
	mv build/out/bitcoin-deps-*.zip inputs/
	./bin/gbuild ../darkcoin/contrib/gitian-descriptors/windows/gitian-win-qt4.yml
	mv build/out/qt-*.zip inputs/
	./bin/gbuild ../darkcoin/contrib/gitian-descriptors/windows/gitian-win-protobuf.yml
	mv build/out/protobuf-*.zip inputs/
	./bin/gbuild ../darkcoin/contrib/gitian-descriptors/osx/gitian-osx-native.yml
	mv build/out/osx-*.tar.gz inputs/
	./bin/gbuild ../darkcoin/contrib/gitian-descriptors/osx/gitian-osx-depends.yml
	mv build/out/osx-*.tar.gz inputs/
	./bin/gbuild ../darkcoin/contrib/gitian-descriptors/osx/gitian-osx-qt.yml
	mv build/out/osx-*.tar.gz inputs/

 The expected SHA256 hashes of the intermediate inputs are:

    f29b7d9577417333fb56e023c2977f5726a7c297f320b175a4108cf7cd4c2d29  boost-linux32-1.55.0-gitian-r1.zip
    88232451c4104f7eb16e469ac6474fd1231bd485687253f7b2bdf46c0781d535  boost-linux64-1.55.0-gitian-r1.zip
    46710f673467e367738d8806e45b4cb5931aaeea61f4b6b55a68eea56d5006c5  bitcoin-deps-linux32-gitian-r6.zip
    f03be39fb26670243d3a659e64d18e19d03dec5c11e9912011107768390b5268  bitcoin-deps-linux64-gitian-r6.zip
    57e57dbdadc818cd270e7e00500a5e1085b3bcbdef69a885f0fb7573a8d987e1  qt-linux32-4.6.4-gitian-r1.tar.gz
    60eb4b9c5779580b7d66529efa5b2836ba1a70edde2a0f3f696d647906a826be  qt-linux64-4.6.4-gitian-r1.tar.gz
    60dc2d3b61e9c7d5dbe2f90d5955772ad748a47918ff2d8b74e8db9b1b91c909  boost-win32-1.55.0-gitian-r6.zip
    f65fcaf346bc7b73bc8db3a8614f4f6bee2f61fcbe495e9881133a7c2612a167  boost-win64-1.55.0-gitian-r6.zip
    70de248cd0dd7e7476194129e818402e974ca9c5751cbf591644dc9f332d3b59  bitcoin-deps-win32-gitian-r13.zip
    9eace4c76f639f4f3580a478eee4f50246e1bbb5ccdcf37a158261a5a3fa3e65  bitcoin-deps-win64-gitian-r13.zip
    8016f165c92b001ecc4a192109e2237f4de1c93e99a97a19f1ac2ad9f7a4d146  qt-win32-4.8.5-gitian-r5.zip
    22c25d40a49c6782d4ed6c42563eb336d4aadbf77de92f1b09d8e3214855237f  qt-win64-4.8.5-gitian-r5.zip
    e2e403e1a08869c7eed4d4293bce13d51ec6a63592918b90ae215a0eceb44cb4  protobuf-win32-2.5.0-gitian-r4.zip
    a0999037e8b0ef9ade13efd88fee261ba401f5ca910068b7e0cd3262ba667db0  protobuf-win64-2.5.0-gitian-r4.zip
    512bc0622c883e2e0f4cbc3fedfd8c2402d06c004ce6fb32303cc2a6f405b6df  osx-native-depends-r3.tar.gz
    560b17ef30607f4552c7f520b4192b742774c55a71f26eb6b938debc8d5f9491  osx-depends-r4.tar.gz
    d6bec84c7ac8c3aa5aa2ea728bc3561f6fdfb4c58bc616ddfca757d6f4b03198  osx-depends-qt-5.2.1-r4.tar.gz


 Build darkcoind and darkcoin-qt on Linux32, Linux64, Win32 and OSX:
  
	./bin/gbuild --commit darkcoin=v${VERSION} ../darkcoin/contrib/gitian-descriptors/linux/gitian-linux-darkcoin.yml
	./bin/gsign --signer $SIGNER --release ${VERSION} --destination ../gitian.sigs/ ../darkcoin/contrib/gitian-descriptors/linux/gitian-linux-darkcoin.yml
	pushd build/out
	zip -r darkcoin-${VERSION}-linux-gitian.zip *
	mv darkcoin-${VERSION}-linux-gitian.zip ../../../
	popd
	./bin/gbuild --commit darkcoin=v${VERSION} ../darkcoin/contrib/gitian-descriptors/windows/gitian-win-darkcoin.yml
	./bin/gsign --signer $SIGNER --release ${VERSION}-win --destination ../gitian.sigs/ ../darkcoin/contrib/gitian-descriptors/windows/gitian-win-darkcoin.yml
	pushd build/out
	zip -r darkcoin-${VERSION}-win-gitian.zip *
	mv darkcoin-${VERSION}-win-gitian.zip ../../../
	popd
        ./bin/gbuild --commit darkcoin=v${VERSION} ../darkcoin/contrib/gitian-descriptors/osx/gitian-osx-darkcoin.yml
        ./bin/gsign --signer $SIGNER --release ${VERSION}-osx --destination ../gitian.sigs/ ../darkcoin/contrib/gitian-descriptors/osx/gitian-osx-darkcoin.yml
	pushd build/out
	mv DarkCoin-Qt.dmg ../../../
	popd
	popd

  Build output expected:

  1. linux 32-bit and 64-bit binaries + source (darkcoin-${VERSION}-linux-gitian.zip)
  2. windows 32-bit binaries + installer + source (darkcoin-${VERSION}-win-gitian.zip)
  3. OSX installer (DarkCoin-Qt.dmg)
  4. Gitian signatures (in gitian.sigs/${VERSION}[-win|-osx]/(your gitian key)/

repackage gitian builds for release as stand-alone zip/tar/installer exe

**Linux .tar.gz:**

	unzip darkcoin-${VERSION}-linux-gitian.zip -d darkcoin-${VERSION}-linux
	tar czvf darkcoin-${VERSION}-linux.tar.gz darkcoin-${VERSION}-linux
	rm -rf darkcoin-${VERSION}-linux

**Windows .zip and setup.exe:**

	unzip darkcoin-${VERSION}-win-gitian.zip -d darkcoin-${VERSION}-win
	mv darkcoin-${VERSION}-win/darkcoin-*-setup.exe .
	zip -r darkcoin-${VERSION}-win.zip darkcoin-${VERSION}-win
	rm -rf darkcoin-${VERSION}-win

###Next steps:

* Code-sign Windows -setup.exe (in a Windows virtual machine using signtool)

* upload builds to github

* create SHA256SUMS for builds, and PGP-sign it

* update darkcoin.io version
  make sure all OS download links go to the right versions
  
* update download sizes on darkcoin.io

* update forum version

* update wiki download links

* update wiki changelog

Commit your signature to gitian.sigs:

	pushd gitian.sigs
	git add ${VERSION}/${SIGNER}
	git add ${VERSION}-win/${SIGNER}
	git commit -a
	git push  # Assuming you can push to the gitian.sigs tree
	popd

-------------------------------------------------------------------------

### After 3 or more people have gitian-built, repackage gitian-signed zips:

- Upload gitian zips to github

- Announce the release:

  - Add the release to darkcoin.io: https://github.com/darkcoinproject/darkcoin.io/tree/master/_releases

  - Release sticky on darkcointalk

  - Darkcoin-development mailing list

  - Optionally reddit /r/DRKCoin, ...

- Celebrate 
