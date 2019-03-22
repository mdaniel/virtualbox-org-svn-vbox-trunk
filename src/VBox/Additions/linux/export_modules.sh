#!/bin/sh
# $Id$
## @file
# Create a tar archive containing the sources of the Linux guest kernel modules.
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

# The below is GNU-specific.  See VBox.sh for the longer Solaris/OS X version.
TARGET=`readlink -e -- "${0}"` || exit 1
MY_DIR="${TARGET%/[!/]*}"

if [ -z "$1" ]; then
    echo "Usage: $0 <filename.tar.gz>"
    echo "  Export VirtualBox kernel modules to <filename.tar.gz>"
    exit 1
fi

PATH_TMP="`cd \`dirname $1\`; pwd`/.vbox_modules"
PATH_OUT=$PATH_TMP
FILE_OUT="`cd \`dirname $1\`; pwd`/`basename $1`"
PATH_ROOT="`cd ${MY_DIR}/../../../..; pwd`"
PATH_LOG=/tmp/vbox-export-guest.log
PATH_LINUX="$PATH_ROOT/src/VBox/Additions/linux"
PATH_VBOXGUEST="$PATH_ROOT/src/VBox/Additions/common/VBoxGuest"
PATH_VBOXSF="$PATH_ROOT/src/VBox/Additions/linux/sharedfolders"
PATH_VBOXVIDEO="$PATH_ROOT/src/VBox/Additions/linux/drm"

VBOX_VERSION_MAJOR=`sed -e "s/^ *VBOX_VERSION_MAJOR *= \+\([0-9]\+\)/\1/;t;d" $PATH_ROOT/Version.kmk`
VBOX_VERSION_MINOR=`sed -e "s/^ *VBOX_VERSION_MINOR *= \+\([0-9]\+\)/\1/;t;d" $PATH_ROOT/Version.kmk`
VBOX_VERSION_BUILD=`sed -e "s/^ *VBOX_VERSION_BUILD *= \+\([0-9]\+\)/\1/;t;d" $PATH_ROOT/Version.kmk`
VBOX_SVN_REV=`sed -e 's/^ *VBOX_SVN_REV_FALLBACK *:= \+\$(patsubst *%:,, *\$Rev: *\([0-9]\+\) *\$ *) */\1/;t;d' $PATH_ROOT/Config.kmk`
VBOX_VENDOR=`sed -e 's/^ *VBOX_VENDOR *= \+\(.\+\)/\1/;t;d' $PATH_ROOT/Config.kmk`
VBOX_VENDOR_SHORT=`sed -e 's/^ *VBOX_VENDOR_SHORT *= \+\(.\+\)/\1/;t;d' $PATH_ROOT/Config.kmk`
VBOX_PRODUCT=`sed -e 's/^ *VBOX_PRODUCT *= \+\(.\+\)/\1/;t;d' $PATH_ROOT/Config.kmk`
VBOX_C_YEAR=`date +%Y`

. $PATH_VBOXGUEST/linux/files_vboxguest
. $PATH_VBOXSF/files_vboxsf
. $PATH_VBOXVIDEO/files_vboxvideo_drv

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
echo "#define VBOX_VERSION_STRING \"$VBOX_VERSION_MAJOR.$VBOX_VERSION_MINOR.$VBOX_VERSION_BUILD\"" >> $PATH_TMP/version-generated.h
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

# vboxguest (VirtualBox guest kernel module)
mkdir $PATH_TMP/vboxguest || exit 1
for f in $FILES_VBOXGUEST_NOBIN; do
    install -D -m 0644 `echo $f|cut -d'=' -f1` "$PATH_TMP/vboxguest/`echo $f|cut -d'>' -f2`"
done
for f in $FILES_VBOXGUEST_BIN; do
    install -D -m 0755 `echo $f|cut -d'=' -f1` "$PATH_TMP/vboxguest/`echo $f|cut -d'>' -f2`"
done

# vboxsf (VirtualBox guest kernel module for shared folders)
mkdir $PATH_TMP/vboxsf || exit 1
for f in $FILES_VBOXSF_NOBIN; do
    install -D -m 0644 `echo $f|cut -d'=' -f1` "$PATH_TMP/vboxsf/`echo $f|cut -d'>' -f2`"
done
for f in $FILES_VBOXSF_BIN; do
    install -D -m 0755 `echo $f|cut -d'=' -f1` "$PATH_TMP/vboxsf/`echo $f|cut -d'>' -f2`"
done

# vboxvideo (VirtualBox guest kernel module for drm support)
mkdir $PATH_TMP/vboxvideo || exit 1
for f in $FILES_VBOXVIDEO_DRM_NOBIN; do
    install -D -m 0644 `echo $f|cut -d'=' -f1` "$PATH_TMP/vboxvideo/`echo $f|cut -d'>' -f2`"
done
for f in $FILES_VBOXVIDEO_DRM_BIN; do
    install -D -m 0755 `echo $f|cut -d'=' -f1` "$PATH_TMP/vboxvideo/`echo $f|cut -d'>' -f2`"
done
sed -f $PATH_VBOXVIDEO/indent.sed -i $PATH_TMP/vboxvideo/*.[ch]

# convenience Makefile
install -D -m 0644 $PATH_LINUX/Makefile "$PATH_TMP/Makefile"

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

