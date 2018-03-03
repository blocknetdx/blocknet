Mac OS X Build Instructions and Notes
====================================
This guide will show you how to build phored (headless client) for OSX.

Notes
-----

* Tested on OS X 10.7 through 10.10 on 64-bit Intel processors only. Please read carefully if you are building on High Sierra (10.13), there are special instructions.

* All of the commands should be executed in a Terminal application. The
built-in one is located in `/Applications/Utilities`.

Preparation
-----------

You need to install XCode with all the options checked so that the compiler
and everything is available in /usr not just /Developer. XCode should be
available on your OS X installation media, but if not, you can get the
current version from https://developer.apple.com/xcode/. If you install
Xcode 4.3 or later, you'll need to install its command line tools. This can
be done in `Xcode > Preferences > Downloads > Components` and generally must
be re-done or updated every time Xcode is updated.

There's also an assumption that you already have `git` installed. If
not, it's the path of least resistance to install [Github for Mac](https://mac.github.com/)
(OS X 10.7+) or
[Git for OS X](https://code.google.com/p/git-osx-installer/). It is also
available via Homebrew.

You will also need to install [Homebrew](http://brew.sh) in order to install library
dependencies.

The installation of the actual dependencies is covered in the Instructions
sections below.

Instructions: Homebrew
----------------------

#### Install dependencies using Homebrew

        brew install autoconf automake berkeley-db4 libtool boost miniupnpc openssl pkg-config protobuf qt5 libzmq
        
        Note: On High Sierra (or when libzmq cannot be found), libzmq should be replaced with zeromq

### Building `phored`

1. Clone the github tree to get the source code and go into the directory.

        git clone https://github.com/phoreproject/Phore.git
        cd Phore

2.  Build phored:
        
        chmod +x share/genbuild.sh autogen.sh 
        ./autogen.sh
        ./configure --with-gui=qt5 
        make
(note: if configure fails with libprotobuf not found see [Troubleshooting](#trouble) at the bottom)


3.  It is also a good idea to build and run the unit tests:

        make check

4.  (Optional) You can also install phored to your path:

        make install

Use Qt Creator as IDE
------------------------
You can use Qt Creator as IDE, for debugging and for manipulating forms, etc.
Download Qt Creator from http://www.qt.io/download/. Download the "community edition" and only install Qt Creator (uncheck the rest during the installation process).

1. Make sure you installed everything through homebrew mentioned above
2. Do a proper ./configure --with-gui=qt5 --enable-debug
3. In Qt Creator do "New Project" -> Import Project -> Import Existing Project
4. Enter "phore-qt" as project name, enter src/qt as location
5. Leave the file selection as it is
6. Confirm the "summary page"
7. In the "Projects" tab select "Manage Kits..."
8. Select the default "Desktop" kit and select "Clang (x86 64bit in /usr/bin)" as compiler
9. Select LLDB as debugger (you might need to set the path to your installtion)
10. Start debugging with Qt Creator

Creating a release build
------------------------
You can ignore this section if you are building `phored` for your own use.

phored/phore-cli binaries are not included in the phore-Qt.app bundle.

If you are building `phored` or `phore-qt` for others, your build machine should be set up
as follows for maximum compatibility:

All dependencies should be compiled with these flags:

 -mmacosx-version-min=10.7
 -arch x86_64
 -isysroot $(xcode-select --print-path)/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.7.sdk

Once dependencies are compiled, see release-process.md for how the Phore-Qt.app
bundle is packaged and signed to create the .dmg disk image that is distributed.

Running
-------

It's now available at `./phored`, provided that you are still in the `src`
directory. We have to first create the RPC configuration file, though.

Run `./phored` to get the filename where it should be put, or just try these
commands:

    echo -e "rpcuser=phorerpc\nrpcpassword=$(xxd -l 16 -p /dev/urandom)" > "/Users/${USER}/Library/Application Support/Phore/phore.conf"
    chmod 600 "/Users/${USER}/Library/Application Support/Phore/phore.conf"

The next time you run it, it will start downloading the blockchain, but it won't
output anything while it's doing this. This process may take several hours;
you can monitor its process by looking at the debug.log file, like this:

    tail -f $HOME/Library/Application\ Support/Phore/debug.log

Other commands:
-------

    ./phored -daemon # to start the phore daemon.
    ./phore-cli --help  # for a list of command-line options.
    ./phore-cli help    # When the daemon is running, to get a list of RPC commands
    
Troubleshooting:<a name="trouble"></a>
---------
* brew install not working? Try replacing libzmq with zeromq in the brew install command
                
* libprotobuf not found during ./configure? Make sure you have installed protobuf with `brew install protobuf` and then run `export PKG_CONFIG_PATH=/usr/local/lib/pkgconfig` and try again
                
* Database errors have been seen in builds on High Sierra. One solution is to build Berkeley DB from source.
        
        cd ~
        wget 'http://download.oracle.com/berkeley-db/db-4.8.30.NC.tar.gz'
        tar -xzvf db-4.8.30.NC.tar.gz
        cd db-4.8.30.NC/build_unix/
        ../dist/configure --enable-cxx
        make
        sudo make install

        Then configure Phore with this build of BerkeleyDB,
        ./configure --with-gui=qt5  LDFLAGS="-L/usr/local/BerkeleyDB.4.8/lib/" CPPFLAGS="-I/usr/local/BerkeleyDB.4.8/include/"
                
        
* In the case you see: `configure: error: OpenSSL ec header missing`, run the following commands:

        export LDFLAGS=-L/usr/local/opt/openssl/lib
        export CPPFLAGS=-I/usr/local/opt/openssl/include

### Building Qt wallet for OSX High Sierra

Currently the gitian build is not supported for Mac OSX High Sierra, but a Qt wallet can be built natively on a OSX High Sierra machine. These instructions provide the steps to perform that build from source code.

If you do not have XCode instlled, go to the Mac App Store and install it.

If you already had homebrew installed, you likely have a newer version that we need of boost, which will cause problems. Uninstall boost first. We need version 1.57 to compile the wallet.

Otherwise, open Terminal and type in the command to install homebrew:

```/usr/bin/ruby -e "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/master/install)"```

The use homebrew to install a number of unix programs and libraries needed to build the Phore wallet:

```brew install autoconf automake berkeley-db@4 boost@1.57 git libevent libtool miniupnpc openssl pkg-config protobuf qt zeromq```

To have the build process use the proper version of boost, link that version as follows:

```brew link boost@1.57 --force```

Next, switch into your Downloads folder:

```cd ~/Downloads```

The next step is to download the current version of the wallet from Github and go into that directory:

```git clone https://github.com/phoreproject/phore.git```
```cd Phore```

Now set some configuration flags:

export LDFLAGS=-L/usr/local/opt/openssl/lib;export CPPFLAGS=-I/usr/local/opt/openssl/include

Then we begin the build process:

```./autogen.sh```
```./configure```
```make```

You have the choice to build the GUI Phore wallet as a Mac OSX app, described in “How to build the Phore-Qt App”. If, for whatever reason, you prefer to use the command line tools, continue with “Command line tools”.

### How to build the Phore-Qt App:

After make is finished, you can create an App bundle inside a disk image with:

```make deploy```

Once this is done, you’ll find Phore-Qt.dmg inside your Phore folder. Open and install the wallet like any typical Mac app.

### Command line tools

Once the build is complete, switch into the src/qt subdirectory:

```cd src/qt```

And there you have your wallet – you can start it by running:

```./phore-qt```

You can move the wallet app to another more permanent location. If you have not moved it and want to start your wallet in the future, open Terminal and run this command:

~/Downloads/Phore/src/qt/phore-qt