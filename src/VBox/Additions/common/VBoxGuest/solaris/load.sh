#!/bin/bash
# $Id$
## @file
# For GA development.
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

DRVNAME="vboxguest"
DRIVERS_USING_IT="vboxfs"

DRVFILE=`dirname "$0"`
DRVFILE=`cd "$DRVFILE" && pwd`
DRVFILE="$DRVFILE/$DRVNAME"
if [ ! -f "$DRVFILE" ]; then
    echo "load.sh: Cannot find $DRVFILE or it's not a file..."
    exit 1;
fi

SUDO=sudo
#set -x

# Unload driver that may depend on the driver we're going to (re-)load
# as well as the driver itself.
for drv in $DRIVERS_USING_IT $DRVNAME;
do
    LOADED=`modinfo | grep -w "$drv"`
    if test -n "$LOADED"; then
        MODID=`echo "$LOADED" | cut -d ' ' -f 1`
        $SUDO modunload -i $MODID;
        LOADED=`modinfo | grep -w "$drv"`;
        if test -n "$LOADED"; then
            echo "load.sh: failed to unload $drv";
            dmesg | tail
            exit 1;
        fi
    fi
done

#
# Update the devlink.tab file so we get a /dev/vboxguest node.
#
set -e
sed -e '/name=vboxguest/d' /etc/devlink.tab > /tmp/devlink.vbox
echo "type=ddi_pseudo;name=vboxguest	\D" >> /tmp/devlink.vbox
$SUDO cp /tmp/devlink.vbox /etc/devlink.tab
$SUDO ln -fs ../devices/pci@0,0/pci80ee,cafe@4:vboxguest /dev/vboxguest
#/usr/sbin/installf -c none $PKGINST /dev/vboxms=../devices/pseudo/vboxms@0:vboxms s
set +e

#
# The add_drv command will load the driver, so we need to temporarily put it
# in a place that is searched in order to load it.
#
MY_RC=1
set -e
$SUDO rm -f \
    "/usr/kernel/drv/${DRVNAME}" \
    "/usr/kernel/drv/amd64/${DRVNAME}"
sync
$SUDO cp "${DRVFILE}"      /platform/i86pc/kernel/drv/amd64/
set +e

$SUDO rem_drv $DRVNAME
if $SUDO add_drv -ipci80ee,cafe -m"* 0666 root sys" -v $DRVNAME; then
    sync
    $SUDO /usr/sbin/devfsadm -i $DRVNAME
    MY_RC=0
else
    dmesg | tail
    echo "load.sh: add_drv failed."
fi

$SUDO rm -f \
    "/usr/kernel/drv/${DRVNAME}" \
    "/usr/kernel/drv/amd64/${DRVNAME}"
sync

exit $MY_RC;

