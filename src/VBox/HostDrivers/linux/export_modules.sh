#!/bin/sh
# $Id$
## @file
# Create a tar archive containing the sources of the vboxdrv kernel module.
#

#
# Copyright (C) 2007-2017 Oracle Corporation
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

# The below is GNU-specific.  See VBox.sh for the longer Solaris/OS X version.
TARGET=`readlink -e -- "${0}"` || exit 1
MY_DIR="${TARGET%/[!/]*}"

if [ -z "$1" ]; then
    echo "Usage: $0 <filename.tar.gz> [--without-hardening]"
    echo "  Export VirtualBox kernel modules to <filename.tar.gz>"
    exit 1
fi

VBOX_WITH_HARDENING=1
if [ "$2" = "--without-hardening" ]; then
    VBOX_WITH_HARDENING=
fi

PATH_TMP="`cd \`dirname $1\`; pwd`/.vbox_modules"
PATH_OUT=$PATH_TMP
FILE_OUT="`cd \`dirname $1\`; pwd`/`basename $1`"
PATH_ROOT="`cd ${MY_DIR}/../../../..; pwd`"
PATH_LOG=/tmp/vbox-export-host.log
PATH_LINUX="$PATH_ROOT/src/VBox/HostDrivers/linux"
PATH_VBOXDRV="$PATH_ROOT/src/VBox/HostDrivers/Support"
PATH_VBOXNET="$PATH_ROOT/src/VBox/HostDrivers/VBoxNetFlt"
PATH_VBOXADP="$PATH_ROOT/src/VBox/HostDrivers/VBoxNetAdp"
PATH_VBOXPCI="$PATH_ROOT/src/VBox/HostDrivers/VBoxPci"

VBOX_VERSION_MAJOR=`sed -e "s/^ *VBOX_VERSION_MAJOR *= \+\([0-9]\+\)/\1/;t;d" $PATH_ROOT/Version.kmk`
VBOX_VERSION_MINOR=`sed -e "s/^ *VBOX_VERSION_MINOR *= \+\([0-9]\+\)/\1/;t;d" $PATH_ROOT/Version.kmk`
VBOX_VERSION_BUILD=`sed -e "s/^ *VBOX_VERSION_BUILD *= \+\([0-9]\+\)/\1/;t;d" $PATH_ROOT/Version.kmk`
VBOX_VERSION_STRING=$VBOX_VERSION_MAJOR.$VBOX_VERSION_MINOR.$VBOX_VERSION_BUILD
VBOX_SVN_REV=`sed -e 's/^ *VBOX_SVN_REV_FALLBACK *:= \+\$(patsubst *%:,, *\$Rev: *\([0-9]\+\) *\$ *) */\1/;t;d' $PATH_ROOT/Config.kmk`                                                                                                        VBOX_VENDOR=`sed -e 's/^ *VBOX_VENDOR *= \+\(.\+\)/\1/;t;d' $PATH_ROOT/Config.kmk`                                     VBOX_VENDOR_SHORT=`sed -e 's/^ *VBOX_VENDOR_SHORT *= \+\(.\+\)/\1/;t;d' $PATH_ROOT/Config.kmk`                         VBOX_PRODUCT=`sed -e 's/^ *VBOX_PRODUCT *= \+\(.\+\)/\1/;t;d' $PATH_ROOT/Config.kmk`                                   VBOX_C_YEAR=`date +%Y`

. $PATH_VBOXDRV/linux/files_vboxdrv
. $PATH_VBOXNET/linux/files_vboxnetflt
. $PATH_VBOXADP/linux/files_vboxnetadp
. $PATH_VBOXPCI/linux/files_vboxpci

# Temporary path for creating the modules, will be removed later
mkdir $PATH_TMP || exit 1

# Create auto-generated version file, needed by all modules
echo "#ifndef ___version_generated_h___" > $PATH_TMP/version-generated.h
echo "#define ___version_generated_h___" >> $PATH_TMP/version-generated.h
echo "" >> $PATH_TMP/version-generated.h
echo "#define VBOX_VERSION_MAJOR $VBOX_VERSION_MAJOR" >> $PATH_TMP/version-generated.h
echo "#define VBOX_VERSION_MINOR $VBOX_VERSION_MINOR" >> $PATH_TMP/version-generated.h
echo "#define VBOX_VERSION_BUILD $VBOX_VERSION_BUILD" >> $PATH_TMP/version-generated.h
echo "#define VBOX_VERSION_STRING_RAW \"$VBOX_VERSION_MAJOR.$VBOX_VERSION_MINOR.$VBOX_VERSION_BUILD\"" >> $PATH_TMP/version-generated.h
echo "#define VBOX_VERSION_STRING \"$VBOX_VERSION_STRING\"" >> $PATH_TMP/version-generated.h
echo "#define VBOX_API_VERSION_STRING \"${VBOX_VERSION_MAJOR}_${VBOX_VERSION_MINOR}\"" >> $PATH_TMP/version-generated.h
echo "#define VBOX_PRIVATE_BUILD_DESC \"Private build with export_modules\"" >> $PATH_TMP/version-generated.h
echo "" >> $PATH_TMP/version-generated.h
echo "#endif" >> $PATH_TMP/version-generated.h

# Create auto-generated revision file, needed by all modules
echo "#ifndef __revision_generated_h__" > $PATH_TMP/revision-generated.h
echo "#define __revision_generated_h__" >> $PATH_TMP/revision-generated.h
echo "" >> $PATH_TMP/revision-generated.h
echo "#define VBOX_SVN_REV $VBOX_SVN_REV" >> $PATH_TMP/revision-generated.h
echo "" >> $PATH_TMP/revision-generated.h
echo "#endif" >> $PATH_TMP/revision-generated.h

# Create auto-generated product file, needed by all modules
echo "#ifndef ___product_generated_h___" > $PATH_TMP/product-generated.h
echo "#define ___product_generated_h___" >> $PATH_TMP/product-generated.h
echo "" >> $PATH_TMP/product-generated.h
echo "#define VBOX_VENDOR \"$VBOX_VENDOR\"" >> $PATH_TMP/product-generated.h
echo "#define VBOX_VENDOR_SHORT \"$VBOX_VENDOR_SHORT\"" >> $PATH_TMP/product-generated.h
echo "" >> $PATH_TMP/product-generated.h
echo "#define VBOX_PRODUCT \"$VBOX_PRODUCT\"" >> $PATH_TMP/product-generated.h
echo "#define VBOX_C_YEAR \"$VBOX_C_YEAR\"" >> $PATH_TMP/product-generated.h
echo "" >> $PATH_TMP/product-generated.h
echo "#endif" >> $PATH_TMP/product-generated.h

# vboxdrv (VirtualBox host kernel module)
mkdir $PATH_TMP/vboxdrv || exit 1
for f in $FILES_VBOXDRV_NOBIN; do
    install -D -m 0644 `echo $f|cut -d'=' -f1` "$PATH_TMP/vboxdrv/`echo $f|cut -d'>' -f2`"
done
for f in $FILES_VBOXDRV_BIN; do
    install -D -m 0755 `echo $f|cut -d'=' -f1` "$PATH_TMP/vboxdrv/`echo $f|cut -d'>' -f2`"
done
if [ -n "$VBOX_WITH_HARDENING" ]; then
    sed -e "s;-DVBOX_WITH_EFLAGS_AC_SET_IN_VBOXDRV;;g" \
        -e "s;-DIPRT_WITH_EFLAGS_AC_PRESERVING;;g" \
        < $PATH_VBOXDRV/linux/Makefile > $PATH_TMP/vboxdrv/Makefile
else
    sed -e "s;-DVBOX_WITH_HARDENING;;g" \
        -e "s;-DVBOX_WITH_EFLAGS_AC_SET_IN_VBOXDRV;;g" \
        -e "s;-DIPRT_WITH_EFLAGS_AC_PRESERVING;;g" \
        < $PATH_VBOXDRV/linux/Makefile > $PATH_TMP/vboxdrv/Makefile
fi

# vboxnetflt (VirtualBox netfilter kernel module)
mkdir $PATH_TMP/vboxnetflt || exit 1
for f in $VBOX_VBOXNETFLT_SOURCES; do
    install -D -m 0644 `echo $f|cut -d'=' -f1` "$PATH_TMP/vboxnetflt/`echo $f|cut -d'>' -f2`"
done
if [ -n "$VBOX_WITH_HARDENING" ]; then
    cat                                   $PATH_VBOXNET/linux/Makefile > $PATH_TMP/vboxnetflt/Makefile
else
    sed -e "s;-DVBOX_WITH_HARDENING;;g" < $PATH_VBOXNET/linux/Makefile > $PATH_TMP/vboxnetflt/Makefile
fi

# vboxnetadp (VirtualBox network adapter kernel module)
mkdir $PATH_TMP/vboxnetadp || exit 1
for f in $VBOX_VBOXNETADP_SOURCES; do
    install -D -m 0644 `echo $f|cut -d'=' -f1` "$PATH_TMP/vboxnetadp/`echo $f|cut -d'>' -f2`"
done
if [ -n "$VBOX_WITH_HARDENING" ]; then
    cat                                   $PATH_VBOXADP/linux/Makefile > $PATH_TMP/vboxnetadp/Makefile
else
    sed -e "s;-DVBOX_WITH_HARDENING;;g" < $PATH_VBOXADP/linux/Makefile > $PATH_TMP/vboxnetadp/Makefile
fi

# vboxpci (VirtualBox host PCI access kernel module)
mkdir $PATH_TMP/vboxpci || exit 1
for f in $VBOX_VBOXPCI_SOURCES; do
    install -D -m 0644 `echo $f|cut -d'=' -f1` "$PATH_TMP/vboxpci/`echo $f|cut -d'>' -f2`"
done
if [ -n "$VBOX_WITH_HARDENING" ]; then
    cat                                   $PATH_VBOXPCI/linux/Makefile > $PATH_TMP/vboxpci/Makefile
else
    sed -e "s;-DVBOX_WITH_HARDENING;;g" < $PATH_VBOXPCI/linux/Makefile > $PATH_TMP/vboxpci/Makefile
fi

install -D -m 0644 $PATH_LINUX/Makefile $PATH_TMP/Makefile
install -D -m 0755 $PATH_LINUX/build_in_tmp $PATH_TMP/build_in_tmp

# Only temporary, omit from archive
rm $PATH_TMP/version-generated.h
rm $PATH_TMP/revision-generated.h
rm $PATH_TMP/product-generated.h

# Do a test build
echo Doing a test build, this may take a while.
make -C $PATH_TMP > $PATH_LOG 2>&1 &&
    make -C $PATH_TMP clean >> $PATH_LOG 2>&1 ||
    echo "Warning: test build failed.  Please check $PATH_LOG"

# Create the archive
tar -czf $FILE_OUT -C $PATH_TMP . || exit 1

# Remove the temporary directory
rm -r $PATH_TMP
