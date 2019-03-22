/* $Id$ */
/** @file
 * Shared folders - Haiku Guest Additions, header.
 */

/*
 * Copyright (C) 2012-2017 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/*
 * This code is based on:
 *
 * VirtualBox Guest Additions for Haiku.
 * Copyright (c) 2011 Mike Smith <mike@scgtrp.net>
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef ___vboxsf_h
#define ___vboxsf_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <malloc.h>
#include <dirent.h>
#include <fs_info.h>
#include <sys/stat.h>
#include <fs_interface.h>
#include <KernelExport.h>
#include <VBoxGuest-haiku.h>
#include <VBox/VBoxGuestLibSharedFolders.h>
#include "lock.h"

typedef struct vboxsf_volume
{
    VBGLSFMAP map;
    ino_t rootid;
} vboxsf_volume;

typedef struct vboxsf_vnode
{
    PVBGLSFMAP map;
    PSHFLSTRING name;
    PSHFLSTRING path;
    ino_t vnode;
    struct vboxsf_vnode* next;
} vboxsf_vnode;

typedef struct vboxsf_dir_cookie
{
    SHFLHANDLE handle;
    PSHFLSTRING path;
    uint32_t index;
    bool has_more_files;
    PSHFLDIRINFO buffer_start, buffer;
    uint32_t buffer_length, num_files;
} vboxsf_dir_cookie;

typedef struct vboxsf_file_cookie
{
    SHFLHANDLE handle;
    PSHFLSTRING path;
} vboxsf_file_cookie;

#ifdef __cplusplus
extern "C" {
#endif

status_t vboxsf_new_vnode(PVBGLSFMAP map, PSHFLSTRING path, PSHFLSTRING name, vboxsf_vnode** p);
status_t vboxsf_get_vnode(fs_volume* volume, ino_t id, fs_vnode* vnode, int* _type, uint32* _flags, bool reenter);
status_t vboxsf_put_vnode(fs_volume* volume, fs_vnode* vnode, bool reenter);
PSHFLSTRING make_shflstring(const char* const s);
mode_t mode_from_fmode(RTFMODE fMode);
status_t vbox_err_to_haiku_err(int rc);
extern mutex g_vnodeCacheLock;

#ifdef __cplusplus
}
#endif

#endif /* ___vboxsf_h */

