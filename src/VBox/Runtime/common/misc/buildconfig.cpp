/* $Id$ */
/** @file
 * IPRT - Build Configuration Information.
 */

/*
 * Copyright (C) 2009-2017 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL) only, as it comes in the "COPYING.CDDL" file of the
 * VirtualBox OSE distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#include "internal/iprt.h"
#include <iprt/buildconfig.h>



#ifdef IPRT_BLDCFG_SCM_REV
RTDECL(uint32_t) RTBldCfgRevision(void)
{
    return IPRT_BLDCFG_SCM_REV;
}


RTDECL(const char *) RTBldCfgRevisionStr(void)
{
    return RT_XSTR(IPRT_BLDCFG_SCM_REV);
}
#endif


#ifdef IPRT_BLDCFG_VERSION_STRING
RTDECL(const char *) RTBldCfgVersion(void)
{
    return IPRT_BLDCFG_VERSION_STRING;
}
#endif


#ifdef IPRT_BLDCFG_VERSION_MAJOR
RTDECL(uint32_t) RTBldCfgVersionMajor(void)
{
    return IPRT_BLDCFG_VERSION_MAJOR;
}
#endif


#ifdef IPRT_BLDCFG_VERSION_MINOR
RTDECL(uint32_t) RTBldCfgVersionMinor(void)
{
    return IPRT_BLDCFG_VERSION_MINOR;
}
#endif


#ifdef IPRT_BLDCFG_VERSION_BUILD
RTDECL(uint32_t) RTBldCfgVersionBuild(void)
{
    return IPRT_BLDCFG_VERSION_BUILD;
}
#endif


#ifdef IPRT_BLDCFG_TARGET
RTDECL(const char *) RTBldCfgTarget(void)
{
    return IPRT_BLDCFG_TARGET;
}
#endif


#ifdef IPRT_BLDCFG_TARGET_ARCH
RTDECL(const char *) RTBldCfgTargetArch(void)
{
    return IPRT_BLDCFG_TARGET_ARCH;
}
#endif


#if defined(IPRT_BLDCFG_TARGET) && defined(IPRT_BLDCFG_TARGET_ARCH)
RTDECL(const char *) RTBldCfgTargetDotArch(void)
{
    return IPRT_BLDCFG_TARGET "." IPRT_BLDCFG_TARGET_ARCH;
}
#endif


#ifdef IPRT_BLDCFG_TYPE
RTDECL(const char *) RTBldCfgType(void)
{
    return IPRT_BLDCFG_TYPE;
}
#endif


RTDECL(const char *) RTBldCfgCompiler(void)
{
#ifdef IPRT_BLDCFG_COMPILER
    return IPRT_BLDCFG_COMPILER;
#elif defined(__INTEL_COMPILER)
    return "intel";
#elif defined(__GNUC__)
    return "gcc";
#elif defined(__llvm__)
    return "llvm";
#elif defined(__SUNPRO_CC) || defined(__SUNPRO_C)
    return "sunpro";
#elif defined(__IBMCPP__) || defined(__IBMC__)
# if defined(__COMPILER_VER__)
    return "ibmzosc";
# elif defined(__xlC__) || defined(__xlc__)
    return "ibmxlc";
# else
    return "vac";
# endif
#elif defined(_MSC_VER)
    return "vcc";
#elif defined(__WATCOMC__)
    return "watcom";
#else
# error "Unknown compiler"
#endif
}

