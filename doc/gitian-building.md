Gitian building
================

*Setup instructions for a Gitian build of Phore Core using a VM or physical system.*

Gitian is the deterministic build process that is used to build the Phore
Core executables. It provides a way to be reasonably sure that the
executables are really built from the git source. It also makes sure that
the same, tested dependencies are used and statically built into the executable.

Multiple developers build the source code by following a specific descriptor
("recipe"), cryptographically sign the result, and upload the resulting signature.
These results are compared and only if they match, the build is accepted and provided
for download.

More independent Gitian builders are needed, which is why this guide exists.
It is preferred you follow these steps yourself instead of using someone else's
VM image to avoid 'contaminating' the build.

Table of Contents
------------------

- [Preparing the Gitian builder host](#preparing-the-gitian-builder-host)
- [Getting and building the inputs](#getting-and-building-the-inputs)
- [Building Phore Core](#building-phore-core)
- [Building an alternative repository](#building-an-alternative-repository)
- [Signing externally](#signing-externally)
- [Uploading signatures](#uploading-signatures)

Preparing the Gitian builder host
---------------------------------

The first step is to prepare the host environment that will be used to perform the Gitian builds.
This guide explains how to set up the environment, and how to start the builds.

Gitian builds are known to be working on recent versions of Debian, Ubuntu and Fedora.
If your machine is already running one of those operating systems, you can perform Gitian builds on the actual hardware.
Alternatively, you can install one of the supported operating systems in a virtual machine.

Any kind of virtualization can be used, for example:
- [VirtualBox](https://www.virtualbox.org/) (covered by this guide)
- [KVM](http://www.linux-kvm.org/page/Main_Page)
- [LXC](https://linuxcontainers.org/), see also [Gitian host docker container](https://github.com/gdm85/tenku/tree/master/docker/gitian-bitcoin-host/README.md).

Please refer to the following documents to set up the operating systems and Gitian.

|                                   | Debian                                                                             | Fedora                                                                             |
|-----------------------------------|------------------------------------------------------------------------------------|------------------------------------------------------------------------------------|
| Setup virtual machine (optional)  | [Create Debian VirtualBox](./gitian-building/gitian-building-create-vm-debian.md) | [Create Fedora VirtualBox](./gitian-building/gitian-building-create-vm-fedora.md) |
| Setup Gitian                      | [Setup Gitian on Debian](./gitian-building/gitian-building-setup-gitian-debian.md) | [Setup Gitian on Fedora](./gitian-building/gitian-building-setup-gitian-fedora.md) |


Getting and building the inputs
--------------------------------

At this point you have two options, you can either use the automated script (found in [https://github.com/phoreproject/phore/blob/master/contrib/gitian-build.sh](https://github.com/phoreproject/phore/blob/master/contrib/gitian-build.sh), only works in Debian/Ubuntu) or you could manually do everything by following this guide.
If you are using the automated script, then run it with the `--setup` command. Afterwards, run it with the `--build` command (example: `contrib/gitian-build.sh -b signer 0.15.0`). Otherwise ignore this.

Follow the instructions in [https://github.com/phoreproject/phore/blob/master/doc/release-process.md](https://github.com/phoreproject/phore/blob/master/doc/release-process.md#fetch-and-create-inputs-first-time-or-when-dependency-versions-change)
in the phore repository under 'Fetch and create inputs' to install sources which require
manual intervention. Also optionally follow the next step: 'Seed the Gitian sources cache
and offline git repositories' which will fetch the remaining files required for building
offline.

Building Phore Core
----------------

To build Phore Core (for Linux, OS X and Windows) just follow the steps under 'perform
Gitian builds' in [https://github.com/phoreproject/phore/blob/master/doc/release-process.md](https://github.com/phoreproject/phore/blob/master/doc/release-process.md#setup-and-perform-gitian-builds) in the phore repository.

This may take some time as it will build all the dependencies needed for each descriptor.
These dependencies will be cached after a successful build to avoid rebuilding them when possible.

At any time you can check the package installation and build progress with

```bash
tail -f var/install.log
tail -f var/build.log
```

Output from `gbuild` will look something like

    Initialized empty Git repository in /home/gitianuser/gitian-builder/inputs/phore/.git/
    remote: Counting objects: 57959, done.
    remote: Total 57959 (delta 0), reused 0 (delta 0), pack-reused 57958
    Receiving objects: 100% (57959/57959), 53.76 MiB | 484.00 KiB/s, done.
    Resolving deltas: 100% (41590/41590), done.
    From https://github.com/phoreproject/phore
    ... (new tags, new branch etc)
    --- Building for trusty amd64 ---
    Stopping target if it is up
    Making a new image copy
    stdin: is not a tty
    Starting target
    Checking if target is up
    Preparing build environment
    Updating apt-get repository (log in var/install.log)
    Installing additional packages (log in var/install.log)
    Grabbing package manifest
    stdin: is not a tty
    Creating build script (var/build-script)
    lxc-start: Connection refused - inotify event with no name (mask 32768)
    Running build script (log in var/build.log)

Building an alternative repository
-----------------------------------

If you want to do a test build of a pull on GitHub it can be useful to point
the Gitian builder at an alternative repository, using the same descriptors
and inputs.

For example:
```bash
URL=https://github.com/laanwj/bitcoin.git
COMMIT=2014_03_windows_unicode_path
./bin/gbuild --commit phore=${COMMIT} --url phore=${URL} ../phore/contrib/gitian-descriptors/gitian-linux.yml
./bin/gbuild --commit phore=${COMMIT} --url phore=${URL} ../phore/contrib/gitian-descriptors/gitian-win.yml
./bin/gbuild --commit phore=${COMMIT} --url phore=${URL} ../phore/contrib/gitian-descriptors/gitian-osx.yml
```

Building fully offline
-----------------------

For building fully offline including attaching signatures to unsigned builds, the detached-sigs repository
and the phore git repository with the desired tag must both be available locally, and then gbuild must be
told where to find them. It also requires an apt-cacher-ng which is fully-populated but set to offline mode, or
manually disabling gitian-builder's use of apt-get to update the VM build environment.

To configure apt-cacher-ng as an offline cacher, you will need to first populate its cache with the relevant
files. You must additionally patch target-bin/bootstrap-fixup to set its apt sources to something other than
plain archive.ubuntu.com: us.archive.ubuntu.com works.

So, if you use LXC:

```bash
export PATH="$PATH":/path/to/gitian-builder/libexec
export USE_LXC=1
cd /path/to/gitian-builder
./libexec/make-clean-vm --suite trusty --arch amd64

LXC_ARCH=amd64 LXC_SUITE=trusty on-target -u root apt-get update
LXC_ARCH=amd64 LXC_SUITE=trusty on-target -u root \
  -e DEBIAN_FRONTEND=noninteractive apt-get --no-install-recommends -y install \
  $( sed -ne '/^packages:/,/[^-] .*/ {/^- .*/{s/"//g;s/- //;p}}' ../phore/contrib/gitian-descriptors/*|sort|uniq )
LXC_ARCH=amd64 LXC_SUITE=trusty on-target -u root apt-get -q -y purge grub
LXC_ARCH=amd64 LXC_SUITE=trusty on-target -u root -e DEBIAN_FRONTEND=noninteractive apt-get -y dist-upgrade
```

And then set offline mode for apt-cacher-ng:

```
/etc/apt-cacher-ng/acng.conf
[...]
Offlinemode: 1
[...]

service apt-cacher-ng restart
```

Then when building, override the remote URLs that gbuild would otherwise pull from the Gitian descriptors::
```bash

cd /some/root/path/
git clone https://github.com/phoreproject/phore-detached-sigs.git

BTCPATH=/some/root/path/phore
SIGPATH=/some/root/path/phore-detached-sigs

./bin/gbuild --url phore=${BTCPATH},signature=${SIGPATH} ../phore/contrib/gitian-descriptors/gitian-win-signer.yml
```

Signing externally
-------------------

If you want to do the PGP signing on another device, that's also possible; just define `SIGNER` as mentioned
and follow the steps in the build process as normal.

    gpg: skipped "laanwj": secret key not available

When you execute `gsign` you will get an error from GPG, which can be ignored. Copy the resulting `.assert` files
in `gitian.sigs` to your signing machine and do

```bash
    gpg --detach-sign ${VERSION}-linux/${SIGNER}/phore-linux-build.assert
    gpg --detach-sign ${VERSION}-win/${SIGNER}/phore-win-build.assert
    gpg --detach-sign ${VERSION}-osx-unsigned/${SIGNER}/phore-osx-build.assert
```

This will create the `.sig` files that can be committed together with the `.assert` files to assert your
Gitian build.

Uploading signatures
---------------------

After building and signing you can push your signatures (both the `.assert` and `.assert.sig` files) to the
[phoreproject/gitian.sigs](https://github.com/phoreproject/gitian.sigs/) repository, or if that's not possible create a pull
request. You can also mail the files to Wladimir (laanwj@gmail.com) and he will commit them.
