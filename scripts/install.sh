#!/bin/bash

# DESTDIR is your target project's dir
DESTDIR=/media/Storage/workspace/router/rt-n56u

ROOTDIR=`pwd`

if [ ! -d "$DESTDIR" ] ; then
	echo "Target project directory not exists! Terminate."
	exit 1
fi

echo "-------------COPY-FILES---------------"

if [ -d "$ROOTDIR/trunk/user/shared/" ] ; then
	cp -fRv "$ROOTDIR/trunk/user/shared/" "$DESTDIR/trunk/user/"
fi

if [ -d "$ROOTDIR/trunk/user/www/" ] ; then
	cp -fRv "$ROOTDIR/trunk/user/www/" "$DESTDIR/trunk/user/"
fi

if [ -d "$ROOTDIR/trunk/configs/" ] ; then
	cp -fRv "$ROOTDIR/trunk/configs/" "$DESTDIR/trunk/"
fi

echo "-------------COPY-END---------------"
