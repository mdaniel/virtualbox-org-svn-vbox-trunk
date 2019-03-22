/* $Id$ */
/** @file
 * VMM testcase - Helper stuff.
 */

/*
 * Copyright (C) 2006-2019 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___tstHelp_h
#define ___tstHelp_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <VBox/cdefs.h>
#include <VBox/vmm/cpum.h>

RT_C_DECLS_BEGIN
void tstDumpCtx(PCPUMCTX pCtx, const char *pszComment);
RT_C_DECLS_END


/**
 * Checks the offset of a data member.
 * @param   type    Type.
 * @param   off     Correct offset.
 * @param   m       Member name.
 */
#define CHECK_OFF(type, off, m) \
    do { \
        if (off != RT_OFFSETOF(type, m)) \
        { \
            printf("error! %#010x %s  Off by %d!! (expected off=%#x)\n", \
                   RT_OFFSETOF(type, m), #type "." #m, off - RT_OFFSETOF(type, m), (int)off); \
            rc++; \
        } \
        /*else */ \
            /*printf("%#08x %s\n", RT_OFFSETOF(type, m), #m);*/ \
    } while (0)

/**
 * Checks the size of type.
 * @param   type    Type.
 * @param   size    Correct size.
 */
#define CHECK_SIZE(type, size) \
    do { \
        if (size != sizeof(type)) \
        { \
            printf("error! sizeof(%s): %#x (%d)  Off by %d!! (expected %#x)\n", \
                   #type, (int)sizeof(type), (int)sizeof(type), (int)sizeof(type) - (int)size, (int)size); \
            rc++; \
        } \
        else \
            printf("info: sizeof(%s): %#x (%d)\n", #type, (int)sizeof(type), (int)sizeof(type)); \
    } while (0)

/**
 * Checks the alignment of a struct member.
 */
#define CHECK_MEMBER_ALIGNMENT(strct, member, align) \
    do \
    { \
        if (RT_UOFFSETOF(strct, member) & ((align) - 1) ) \
        { \
            printf("error! %s::%s offset=%#x (%u) expected alignment %#x, meaning %#x (%u) off\n", \
                   #strct, #member, \
                   (unsigned)RT_OFFSETOF(strct, member), \
                   (unsigned)RT_OFFSETOF(strct, member), \
                   (unsigned)(align), \
                   (unsigned)(((align) - RT_OFFSETOF(strct, member)) & ((align) - 1)), \
                   (unsigned)(((align) - RT_OFFSETOF(strct, member)) & ((align) - 1)) ); \
            rc++; \
        } \
    } while (0)

/**
 * Checks that the size of a type is aligned correctly.
 */
#define CHECK_SIZE_ALIGNMENT(type, align) \
    do { \
        if (RT_ALIGN_Z(sizeof(type), (align)) != sizeof(type)) \
        { \
            printf("error! %s size=%#x (%u), align=%#x %#x (%u) bytes off\n", \
                   #type, \
                   (unsigned)sizeof(type), \
                   (unsigned)sizeof(type), \
                   (align), \
                   (unsigned)RT_ALIGN_Z(sizeof(type), align) - (unsigned)sizeof(type), \
                   (unsigned)RT_ALIGN_Z(sizeof(type), align) - (unsigned)sizeof(type)); \
            rc++; \
        } \
    } while (0)

/**
 * Checks that a internal struct padding is big enough.
 */
#define CHECK_PADDING(strct, member, align) \
    do \
    { \
        strct *p = NULL; NOREF(p); \
        if (sizeof(p->member.s) > sizeof(p->member.padding)) \
        { \
            printf("error! padding of %s::%s is too small, padding=%d struct=%d correct=%d\n", #strct, #member, \
                   (int)sizeof(p->member.padding), (int)sizeof(p->member.s), (int)RT_ALIGN_Z(sizeof(p->member.s), (align))); \
            rc++; \
        } \
        else if (RT_ALIGN_Z(sizeof(p->member.padding), (align)) != sizeof(p->member.padding)) \
        { \
            printf("error! padding of %s::%s is misaligned, padding=%d correct=%d\n", #strct, #member, \
                   (int)sizeof(p->member.padding), (int)RT_ALIGN_Z(sizeof(p->member.s), (align))); \
            rc++; \
        } \
    } while (0)

/**
 * Checks that a internal struct padding is big enough.
 */
#define CHECK_PADDING2(strct) \
    do \
    { \
        strct *p = NULL; NOREF(p); \
        if (sizeof(p->s) > sizeof(p->padding)) \
        { \
            printf("error! padding of %s is too small, padding=%d struct=%d correct=%d\n", #strct, \
                   (int)sizeof(p->padding), (int)sizeof(p->s), (int)RT_ALIGN_Z(sizeof(p->s), 64)); \
            rc++; \
        } \
    } while (0)

/**
 * Checks that a internal struct padding is big enough.
 */
#define CHECK_PADDING3(strct, member, pad_member) \
    do \
    { \
        strct *p = NULL; NOREF(p); \
        if (sizeof(p->member) > sizeof(p->pad_member)) \
        { \
            printf("error! padding of %s::%s is too small, padding=%d struct=%d\n", #strct, #member, \
                   (int)sizeof(p->pad_member), (int)sizeof(p->member)); \
            rc++; \
        } \
    } while (0)

/**
 * Checks that an expression is true.
 */
#define CHECK_EXPR(expr) \
    do \
    { \
        if (!(expr)) \
        { \
            printf("error! '%s' failed! (line %d)\n", #expr, __LINE__); \
            rc++; \
        } \
    } while (0)


#endif
