#!/bin/bash
## @file
# For development.
#

#
# Copyright (C) 2006-2017 Oracle Corporation
#
# This file is part of VirtualBox Open Source Edition (OSE), as
# available from http://www.virtualbox.org. This file is free software;
# you can redistribute it and/or modify it under the terms of the GNU
# General Public License (GPL) as published by the Free Software
# Foundation, in version 2 as it comes in the "COPYING" file of the
# VirtualBox OSE distribution. VirtualBox OSE is distributed in the
# hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
#
# The contents of this file may alternatively be used under the terms
# of the Common Development and Distribution License Version 1.0
# (CDDL) only, as it comes in the "COPYING.CDDL" file of the
# VirtualBox OSE distribution, in which case the provisions of the
# CDDL are applicable instead of those of the GPL.
#
# You may elect to license modified versions of this file under the
# terms and conditions of either the GPL or the CDDL or both.
#

SCRIPT_NAME="loadusb"
XNU_VERSION=`LC_ALL=C uname -r | LC_ALL=C cut -d . -f 1`

DRVNAME="VBoxUSB.kext"
BUNDLE="org.virtualbox.kext.VBoxUSB"

DEP_DRVNAME="VBoxDrv.kext"
DEP_BUNDLE="org.virtualbox.kext.VBoxDrv"


DIR=`dirname "$0"`
DIR=`cd "$DIR" && pwd`
DEP_DIR="$DIR/$DEP_DRVNAME"
DIR="$DIR/$DRVNAME"
if [ ! -d "$DIR" ]; then
    echo "Cannot find $DIR or it's not a directory..."
    exit 1;
fi
if [ ! -d "$DEP_DIR" ]; then
    echo "Cannot find $DEP_DIR or it's not a directory... (dependency)"
    exit 1;
fi
if [ -n "$*" ]; then
  OPTS="$*"
else
  OPTS="-t"
fi

trap "sudo chown -R `whoami` $DIR $DEP_DIR; exit 1" INT

# Try unload any existing instance first.
LOADED=`kextstat -b $BUNDLE -l`
if test -n "$LOADED"; then
    echo "${SCRIPT_NAME}.sh: Unloading $BUNDLE..."
    sudo kextunload -v 6 -b $BUNDLE
    LOADED=`kextstat -b $BUNDLE -l`
    if test -n "$LOADED"; then
        echo "${SCRIPT_NAME}.sh: failed to unload $BUNDLE, see above..."
        exit 1;
    fi
    echo "${SCRIPT_NAME}.sh: Successfully unloaded $BUNDLE"
fi

set -e

# Copy the .kext to the symbols directory and tweak the kextload options.
if test -n "$VBOX_DARWIN_SYMS"; then
    echo "${SCRIPT_NAME}.sh: copying the extension the symbol area..."
    rm -Rf "$VBOX_DARWIN_SYMS/$DRVNAME"
    mkdir -p "$VBOX_DARWIN_SYMS"
    cp -R "$DIR" "$VBOX_DARWIN_SYMS/"
    OPTS="$OPTS -s $VBOX_DARWIN_SYMS/ "
    sync
fi

# On smbfs, this might succeed just fine but make no actual changes,
# so we might have to temporarily copy the driver to a local directory.
if sudo chown -R root:wheel "$DIR" "$DEP_DIR"; then
    OWNER=`/usr/bin/stat -f "%u" "$DIR"`
else
    OWNER=1000
fi
if test "$OWNER" -ne 0; then
    TMP_DIR=/tmp/${SCRIPT_NAME}.tmp
    echo "${SCRIPT_NAME}.sh: chown didn't work on $DIR, using temp location $TMP_DIR/$DRVNAME"

    # clean up first (no sudo rm)
    if test -e "$TMP_DIR"; then
        sudo chown -R `whoami` "$TMP_DIR"
        rm -Rf "$TMP_DIR"
    fi

    # make a copy and switch over DIR
    mkdir -p "$TMP_DIR/"
    sudo cp -Rp "$DIR" "$TMP_DIR/"
    DIR="$TMP_DIR/$DRVNAME"

    # load.sh puts it here.
    DEP_DIR="/tmp/loaddrv.tmp/$DEP_DRVNAME"

    # retry
    sudo chown -R root:wheel "$DIR" "$DEP_DIR"
fi

sudo chmod -R o-rwx "$DIR"
sync
if [ "$XNU_VERSION" -ge "10" ]; then
    echo "${SCRIPT_NAME}.sh: loading $DIR... (kextutil $OPTS -d \"$DEP_DIR\" \"$DIR\")"
    sudo kextutil $OPTS -d "$DEP_DIR" "$DIR"
else
    echo "${SCRIPT_NAME}.sh: loading $DIR... (kextload $OPTS -d \"$DEP_DIR\" \"$DIR\")"
    sudo kextload $OPTS -d "$DEP_DIR" "$DIR"
fi
sync
sudo chown -R `whoami` "$DIR" "$DEP_DIR"
kextstat | grep org.virtualbox.kext

