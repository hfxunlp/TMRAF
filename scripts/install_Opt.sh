#!/bin/bash

# DESTDIR is your target project's dir
DESTDIR=~/router/rt-n56u

ROOTDIR=`pwd`

if [ ! -d "$DESTDIR" ] ; then
	echo "Target project directory not exists! Terminate."
	exit 1
fi

echo "-------------COPY-FILES---------------"

if [ -d "$ROOTDIR/trunk/user/" ] ; then

	# cleaning before bump openvpn to 2.4.5, nfs-utils to 2.6.2, wpa_supplicant to 2.9, dnsmasq to 2.87, libxml2 to 2.10.2, libcurl to 7.85.0, dropbear to 2019.78.
	rm -fr "$DESTDIR/trunk/user/openvpn/openvpn-2.4.x/"
	rm -fr "$DESTDIR/trunk/user/nfsd/nfs-utils-1.2.3/"
	rm -fr "$DESTDIR/trunk/user/wpa_supplicant/"
	rm -fr "$DESTDIR/trunk/user/dropbear/dropbear-201X.XX/"
	rm -fr "$DESTDIR/trunk/user/dnsmasq/"
	rm -fr "$DESTDIR/trunk/libs/libxml2/"
	rm -fr "$DESTDIR/trunk/libs/libcurl/"

	cp -fRv "$ROOTDIR/trunk/user/" "$DESTDIR/trunk/"
fi

if [ -d "$ROOTDIR/trunk/configs/" ] ; then
	cp -fRv "$ROOTDIR/trunk/configs/" "$DESTDIR/trunk/"
fi

if [ -d "$ROOTDIR/trunk/libs/libssl/" ] ; then
	cp -fRv "$ROOTDIR/trunk/libs/libssl/" "$DESTDIR/trunk/libs/"
fi

if [ -d "$ROOTDIR/trunk/libs/libxml2/" ] ; then
	cp -fRv "$ROOTDIR/trunk/libs/libxml2/" "$DESTDIR/trunk/libs/"
fi

if [ -d "$ROOTDIR/trunk/libs/libcurl/" ] ; then
	cp -fRv "$ROOTDIR/trunk/libs/libcurl/" "$DESTDIR/trunk/libs/"
fi

if [ -d "$ROOTDIR/trunk/libc/" ] ; then
	cp -fRv "$ROOTDIR/trunk/libc/" "$DESTDIR/trunk/"
fi

if [ -d "$ROOTDIR/trunk/linux-3.4.x/" ] ; then
	cp -fRv "$ROOTDIR/trunk/linux-3.4.x/" "$DESTDIR/trunk/"
fi

if [ -d "$ROOTDIR/trunk/linux-3.0.x/" ] ; then
	cp -fRv "$ROOTDIR/trunk/linux-3.0.x/" "$DESTDIR/trunk/"
fi

if [ -f "$ROOTDIR/trunk/versions.inc" ] ; then
	cp -fv "$ROOTDIR/trunk/versions.inc" "$DESTDIR/trunk/"
fi

if [ -d "$ROOTDIR/trunk/tools/" ] ; then
	cp -fRv "$ROOTDIR/trunk/tools/" "$DESTDIR/trunk/"
fi

if [ -d "$ROOTDIR/trunk/user/busybox/busybox-1.24.x/scripts/kconfig/" ] ; then
	cp -fRv "$ROOTDIR/trunk/user/busybox/busybox-1.24.x/scripts/kconfig/" "$DESTDIR/trunk/user/busybox/busybox-1.24.x/scripts/"
fi

echo "-------------COPY-END---------------"
