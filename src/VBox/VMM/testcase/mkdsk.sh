#!/bin/sh
## @file
# Obsolete?
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

if [ "x$3" == "x" ]; then

    echo "syntax error"
    echo "syntax: $0 imagename <size-in-KBs> <init prog> [tar files]"
    echo ""
    echo "Simples qemu boot image is archived by only specifying an statically"
    echo "linked init program and using the dev.tar.gz file to create devices."
    echo "The boot linux in qemu specifying the image as -hda. Use the -kernel"
    echo "option to specify a bzImage kernel image to use, and specify"
    echo "-append root=/dev/hda so the kernel will mount /dev/hda and look"
    echo "for /sbin/init there."
    echo ""
    echo "Example:"
    echo "  sh ./mkdsk.sh foo.img 2048 ~/VBox/Tree/out/linux/debug/bin/tstProg1 dev.tar.gz"
    echo "  qemu -hda foo.img -m 32  -kernel ~/qemutest/linux-test/bzImage-2.4.21 -append root=/dev/hda"
    exit 1
fi

image=$1
size=$2
init=$3

sizebytes=`expr $size '*' 1024`
cyls=`expr 8225280 / $sizebytes`
echo $cyls

echo "* Creating $image of $size kb...."
rm -f $image
dd if=/dev/zero of=$image count=$size bs=1024 || exit 1

echo "* Formatting with ext2..."
/sbin/mkfs.ext2 $image || exit 1

echo "* Mounting temporarily at ./tmpmnt..."
mkdir -p tmpmnt
sudo mount $image ./tmpmnt -t ext2 -o loop=/dev/loop7 || exit 1

# init
echo "* Copying $init to sbin/init..."
mkdir tmpmnt/sbin
sudo cp $init tmpmnt/sbin/init
sudo chmod 755 tmpmnt/sbin/init

shift
shift
shift
while [ "x$1" != "x" ];
do
    echo "* Untarring $1 to disk..."
    sudo tar -xzv -C tmpmnt -f $1
    shift
done

echo "* Unmounting    tmpmnt..."
sudo umount tmpmnt
rmdir tmpmnt
echo "* Done! (Perhaps even successfully so...)"
echo "  'root=/dev/hda' remember :-)"
exit 0
