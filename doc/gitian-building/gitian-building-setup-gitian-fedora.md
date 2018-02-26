Table of Contents
------------------

- [Setting up Fedora for Gitian building](#setting-up-fedora-for-gitian-building)
- [Installing Gitian](#installing-gitian)
- [Setting up the Gitian image](#setting-up-the-gitian-image)


Setting up Fedora for Gitian building
--------------------------------------

In this section we will be setting up the Fedora installation for Gitian building.
We assume that a user `gitianuser` was previously created and added to the `wheel` group.

To repeat ths step, log in as `root` and type:

```bash
usermod gitianuser -a -G wheel
```

Log in as `root` and set up the required dependencies.
Type/paste the following in the terminal:

```bash
dnf install git python ruby apt-cacher-ng qemu dpkg debootstrap python-cheetah gnupg tar rsync wget curl lxc libvirt
```

Then set up LXC and the rest with the following, which is a complex jumble of settings and workarounds:

```bash
# Enable the apt-cacher-ng service
systemctl enable apt-cacher-ng.service
# the version of lxc-start in Fedora needs to run as root, so make sure
# that the build script can execute it without providing a password
echo "%wheel ALL=NOPASSWD: /usr/bin/lxc-start" > /etc/sudoers.d/gitian-lxc
echo "%wheel ALL=NOPASSWD: /usr/bin/lxc-execute" >> /etc/sudoers.d/gitian-lxc
# make /etc/rc.d/rc.local script that sets up bridge between guest and host
echo '#!/bin/sh -e' > /etc/rc.d/rc.local
echo 'brctl addbr br0' >> /etc/rc.d/rc.local
echo 'ip addr add 10.0.3.2/24 broadcast 10.0.3.255 dev br0' >> /etc/rc.d/rc.local
echo 'ip link set br0 up' >> /etc/rc.d/rc.local
echo 'firewall-cmd --zone=trusted --add-interface=br0' >> /etc/rc.d/rc.local
echo 'exit 0' >> /etc/rc.d/rc.local
# mark executable so that the rc.local-service unit gets pulled automatically
# into multi-user.target
chmod +x /etc/rc.d/rc.local
# make sure that USE_LXC is always set when logging in as gitianuser,
# and configure LXC IP addresses
echo 'export USE_LXC=1' >> /home/gitianuser/.bash_profile
echo 'export GITIAN_HOST_IP=10.0.3.2' >> /home/gitianuser/.bash_profile
echo 'export LXC_GUEST_IP=10.0.3.5' >> /home/gitianuser/.bash_profile
reboot
```

At the end Fedora is rebooted to make sure that the changes take effect. The steps in this
section only need to be performed once.

Installing Gitian
------------------

Login as the user `gitianuser` that was created during installation.
The rest of the steps in this guide will be performed as that user.

There is no `python-vm-builder` package in Fedora, so we need to install it from source ourselves,

```bash
wget http://archive.ubuntu.com/ubuntu/pool/universe/v/vm-builder/vm-builder_0.12.4+bzr494.orig.tar.gz
echo "76cbf8c52c391160b2641e7120dbade5afded713afaa6032f733a261f13e6a8e  vm-builder_0.12.4+bzr494.orig.tar.gz" | sha256sum -c
# (verification -- must return OK)
tar -zxvf vm-builder_0.12.4+bzr494.orig.tar.gz
cd vm-builder-0.12.4+bzr494
sudo python setup.py install
cd ..
```

**Note**: When sudo asks for a password, enter the password for the user `gitianuser` not for `root`.

Clone the git repositories for phore and Gitian.

```bash
git clone https://github.com/devrandom/gitian-builder.git
git clone https://github.com/phore/phore
git clone https://github.com/phoreproject/gitian.sigs.git
git clone https://github.com/phoreproject/phore-detached-sigs.git
```

Setting up the Gitian image
-------------------------

Gitian needs a virtual image of the operating system to build in.
Currently this is Ubuntu Trusty x86_64.
This image will be copied and used every time that a build is started to
make sure that the build is deterministic.
Creating the image will take a while, but only has to be done once.

Execute the following as user `gitianuser`:

```bash
cd gitian-builder
git checkout 686a00ad712e30ba3a7850c16ef32f650df5b5dc # This version seems to work better than master
bin/make-base-vm --lxc --arch amd64 --suite trusty
```

There will be a lot of warnings printed during the build of the image. These can be ignored.

**Note**: When sudo asks for a password, enter the password for the user `gitianuser` not for `root`.
