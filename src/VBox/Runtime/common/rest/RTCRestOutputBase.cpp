/* $Id$ */
/** @file
 * IPRT - C++ REST, RTCRestOutputBase implementation.
 */

/*
 * Copyright (C) 2018 Oracle Corporation
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
#define LOG_GROUP RTLOGGROUP_REST
#include <iprt/cpp/restoutput.h>

#include <iprt/errcore.h>
#include <iprt/string.h>


RTCRestOutputBase::RTCRestOutputBase() RT_NOEXCEPT
    : m_uState(0)
{
}


RTCRestOutputBase::~RTCRestOutputBase()
{
}


size_t RTCRestOutputBase::vprintf(const char *pszFormat, va_list va) RT_NOEXCEPT
{
    return RTStrFormatV(printfOutputCallback, this, NULL, NULL, pszFormat, va);
}


/*static*/ DECLCALLBACK(size_t) RTCRestOutputBase::printfOutputCallback(void *pvArg, const char *pachChars, size_t cbChars) RT_NOEXCEPT
{
    return ((RTCRestOutputBase *)pvArg)->output(pachChars, cbChars);
}


uint32_t RTCRestOutputBase::beginArray() RT_NOEXCEPT
{
    output(RT_STR_TUPLE("["));
    uint32_t const uOldState = m_uState;
    m_uState = (uOldState & 0xffff) + 1;
    return uOldState;
}


void RTCRestOutputBase::endArray(uint32_t a_uOldState) RT_NOEXCEPT
{
    m_uState = a_uOldState;
    output(RT_STR_TUPLE("]"));
}


uint32_t RTCRestOutputBase::beginObject() RT_NOEXCEPT
{
    output(RT_STR_TUPLE("{"));
    uint32_t const uOldState = m_uState;
    m_uState = (uOldState & 0xffff) + 1;
    return uOldState;
}


void RTCRestOutputBase::endObject(uint32_t a_uOldState) RT_NOEXCEPT
{
    m_uState = a_uOldState;
    output(RT_STR_TUPLE("}"));
}


void RTCRestOutputBase::valueSeparator() RT_NOEXCEPT
{
    if (m_uState & RT_BIT_32(31))
        output(RT_STR_TUPLE(","));
    else
        m_uState |= RT_BIT_32(31);
}


void RTCRestOutputBase::valueSeparatorAndName(const char *a_pszName, size_t a_cchName) RT_NOEXCEPT
{
    RT_NOREF(a_cchName);
    if (m_uState & RT_BIT_32(31))
        printf(",%RMjs:", a_pszName);
    else
    {
        m_uState |= RT_BIT_32(31);
        printf("%RMjs:", a_pszName);
    }
}


void RTCRestOutputBase::nullValue() RT_NOEXCEPT
{
    output(RT_STR_TUPLE("null"));
}

