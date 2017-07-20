
Debian
====================
This directory contains files used to package blocknetdxd/blocknetdx-qt
for Debian-based Linux systems. If you compile blocknetdxd/blocknetdx-qt yourself, there are some useful files here.

## blocknetdx: URI support ##


blocknetdx-qt.desktop  (Gnome / Open Desktop)
To install:

	sudo desktop-file-install blocknetdx-qt.desktop
	sudo update-desktop-database

If you build yourself, you will either need to modify the paths in
the .desktop file or copy or symlink your blocknetdxqt binary to `/usr/bin`
and the `../../share/pixmaps/blocknetdx128.png` to `/usr/share/pixmaps`

blocknetdx-qt.protocol (KDE)

