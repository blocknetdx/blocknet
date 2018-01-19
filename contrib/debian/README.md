
Debian
====================
This directory contains files used to package phored/phore-qt
for Debian-based Linux systems. If you compile phored/phore-qt yourself, there are some useful files here.

## phore: URI support ##


phore-qt.desktop  (Gnome / Open Desktop)
To install:

	sudo desktop-file-install phore-qt.desktop
	sudo update-desktop-database

If you build yourself, you will either need to modify the paths in
the .desktop file or copy or symlink your phoreqt binary to `/usr/bin`
and the `../../share/pixmaps/phore128.png` to `/usr/share/pixmaps`

phore-qt.protocol (KDE)

