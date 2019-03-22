/* $Id$ */
/** @file
 * IPRT - Mini C++ string class.
 *
 * This is a base for both Utf8Str and other places where IPRT may want to use
 * a lean C++ string class.
 */

/*
 * Copyright (C) 2007-2016 Oracle Corporation
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
#include <iprt/cpp/ministring.h>
#include <iprt/ctype.h>


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/
const size_t RTCString::npos = ~(size_t)0;


/*********************************************************************************************************************************
*   Defined Constants And Macros                                                                                                 *
*********************************************************************************************************************************/
/** Allocation block alignment used when appending bytes to a string. */
#define IPRT_MINISTRING_APPEND_ALIGNMENT    64


RTCString &RTCString::printf(const char *pszFormat, ...)
{
    va_list va;
    va_start(va, pszFormat);
    printfV(pszFormat, va);
    va_end(va);
    return *this;
}

/**
 * Callback used with RTStrFormatV by RTCString::printfV.
 *
 * @returns The number of bytes added (not used).
 *
 * @param   pvArg           The string object.
 * @param   pachChars       The characters to append.
 * @param   cbChars         The number of characters.  0 on the final callback.
 */
/*static*/ DECLCALLBACK(size_t)
RTCString::printfOutputCallback(void *pvArg, const char *pachChars, size_t cbChars)
{
    RTCString *pThis = (RTCString *)pvArg;
    if (cbChars)
    {
        size_t cchBoth = pThis->m_cch + cbChars;
        if (cchBoth >= pThis->m_cbAllocated)
        {
            /* Double the buffer size, if it's less that _4M. Align sizes like
               for append. */
            size_t cbAlloc = RT_ALIGN_Z(pThis->m_cbAllocated, IPRT_MINISTRING_APPEND_ALIGNMENT);
            cbAlloc += RT_MIN(cbAlloc, _4M);
            if (cbAlloc <= cchBoth)
                cbAlloc = RT_ALIGN_Z(cchBoth + 1, IPRT_MINISTRING_APPEND_ALIGNMENT);
            pThis->reserve(cbAlloc);
#ifndef RT_EXCEPTIONS_ENABLED
            AssertReleaseReturn(pThis->capacity() > cchBoth, 0);
#endif
        }

        memcpy(&pThis->m_psz[pThis->m_cch], pachChars, cbChars);
        pThis->m_cch = cchBoth;
        pThis->m_psz[cchBoth] = '\0';
    }
    return cbChars;
}

RTCString &RTCString::printfV(const char *pszFormat, va_list va)
{
    cleanup();
    RTStrFormatV(printfOutputCallback, this, NULL, NULL, pszFormat, va);
    return *this;
}

RTCString &RTCString::append(const RTCString &that)
{
    size_t cchThat = that.length();
    if (cchThat)
    {
        size_t cchThis = length();
        size_t cchBoth = cchThis + cchThat;

        if (cchBoth >= m_cbAllocated)
        {
            reserve(RT_ALIGN_Z(cchBoth + 1, IPRT_MINISTRING_APPEND_ALIGNMENT));
            // calls realloc(cchBoth + 1) and sets m_cbAllocated; may throw bad_alloc.
#ifndef RT_EXCEPTIONS_ENABLED
            AssertRelease(capacity() > cchBoth);
#endif
        }

        memcpy(m_psz + cchThis, that.m_psz, cchThat);
        m_psz[cchBoth] = '\0';
        m_cch = cchBoth;
    }
    return *this;
}

RTCString &RTCString::append(const char *pszThat)
{
    size_t cchThat = strlen(pszThat);
    if (cchThat)
    {
        size_t cchThis = length();
        size_t cchBoth = cchThis + cchThat;

        if (cchBoth >= m_cbAllocated)
        {
            reserve(RT_ALIGN_Z(cchBoth + 1, IPRT_MINISTRING_APPEND_ALIGNMENT));
            // calls realloc(cchBoth + 1) and sets m_cbAllocated; may throw bad_alloc.
#ifndef RT_EXCEPTIONS_ENABLED
            AssertRelease(capacity() > cchBoth);
#endif
        }

        memcpy(&m_psz[cchThis], pszThat, cchThat);
        m_psz[cchBoth] = '\0';
        m_cch = cchBoth;
    }
    return *this;
}

RTCString& RTCString::append(char ch)
{
    Assert((unsigned char)ch < 0x80);                  /* Don't create invalid UTF-8. */
    if (ch)
    {
        // allocate in chunks of 20 in case this gets called several times
        if (m_cch + 1 >= m_cbAllocated)
        {
            reserve(RT_ALIGN_Z(m_cch + 2, IPRT_MINISTRING_APPEND_ALIGNMENT));
            // calls realloc(cbBoth) and sets m_cbAllocated; may throw bad_alloc.
#ifndef RT_EXCEPTIONS_ENABLED
            AssertRelease(capacity() > m_cch + 1);
#endif
        }

        m_psz[m_cch] = ch;
        m_psz[++m_cch] = '\0';
    }
    return *this;
}

RTCString &RTCString::appendCodePoint(RTUNICP uc)
{
    /*
     * Single byte encoding.
     */
    if (uc < 0x80)
        return RTCString::append((char)uc);

    /*
     * Multibyte encoding.
     * Assume max encoding length when resizing the string, that's simpler.
     */
    AssertReturn(uc <= UINT32_C(0x7fffffff), *this);

    if (m_cch + 6 >= m_cbAllocated)
    {
        reserve(RT_ALIGN_Z(m_cch + 6 + 1, IPRT_MINISTRING_APPEND_ALIGNMENT));
        // calls realloc(cbBoth) and sets m_cbAllocated; may throw bad_alloc.
#ifndef RT_EXCEPTIONS_ENABLED
        AssertRelease(capacity() > m_cch + 6);
#endif
    }

    char *pszNext = RTStrPutCp(&m_psz[m_cch], uc);
    m_cch = pszNext - m_psz;
    *pszNext = '\0';

    return *this;
}

size_t RTCString::find(const char *pszNeedle, size_t offStart /*= 0*/) const
{
    if (offStart < length())
    {
        const char *pszThis = c_str();
        if (pszThis)
        {
            if (pszNeedle && *pszNeedle != '\0')
            {
                const char *pszHit = strstr(pszThis + offStart, pszNeedle);
                if (pszHit)
                    return pszHit - pszThis;
            }
        }
    }

    return npos;
}

size_t RTCString::find(const RTCString *pStrNeedle, size_t offStart /*= 0*/) const
{
    if (offStart < length())
    {
        const char *pszThis = c_str();
        if (pszThis)
        {
            if (pStrNeedle)
            {
                const char *pszNeedle = pStrNeedle->c_str();
                if (pszNeedle && *pszNeedle != '\0')
                {
                    const char *pszHit = strstr(pszThis + offStart, pszNeedle);
                    if (pszHit)
                        return pszHit - pszThis;
                }
            }
        }
    }

    return npos;
}

void RTCString::findReplace(char chFind, char chReplace)
{
    Assert((unsigned int)chFind    < 128U);
    Assert((unsigned int)chReplace < 128U);

    for (size_t i = 0; i < length(); ++i)
    {
        char *p = &m_psz[i];
        if (*p == chFind)
            *p = chReplace;
    }
}

size_t RTCString::count(char ch) const
{
    Assert((unsigned int)ch < 128U);

    size_t      c   = 0;
    const char *psz = m_psz;
    if (psz)
    {
        char    chCur;
        while ((chCur = *psz++) != '\0')
            if (chCur == ch)
                c++;
    }
    return c;
}

#if 0  /** @todo implement these when needed. */
size_t RTCString::count(const char *psz, CaseSensitivity cs = CaseSensitive) const
{
}

size_t RTCString::count(const RTCString *pStr, CaseSensitivity cs = CaseSensitive) const
{

}
#endif


RTCString &RTCString::strip()
{
    stripRight();
    return stripLeft();
}


RTCString &RTCString::stripLeft()
{
    char        *psz = m_psz;
    size_t const cch = m_cch;
    size_t       off = 0;
    while (off < cch && RT_C_IS_SPACE(psz[off]))
        off++;
    if (off > 0)
    {
        if (off != cch)
        {
            memmove(psz, &psz[off], cch - off + 1);
            m_cch = cch - off;
        }
        else
            setNull();
    }
    return *this;
}


RTCString &RTCString::stripRight()
{
    char  *psz = m_psz;
    size_t cch = m_cch;
    while (cch > 0 && RT_C_IS_SPACE(psz[cch - 1]))
        cch--;
    if (m_cch != cch)
    {
        m_cch = cch;
        psz[cch] = '\0';
    }
    return *this;
}



RTCString RTCString::substrCP(size_t pos /*= 0*/, size_t n /*= npos*/) const
{
    RTCString ret;

    if (n)
    {
        const char *psz;

        if ((psz = c_str()))
        {
            RTUNICP cp;

            // walk the UTF-8 characters until where the caller wants to start
            size_t i = pos;
            while (*psz && i--)
                if (RT_FAILURE(RTStrGetCpEx(&psz, &cp)))
                    return ret;     // return empty string on bad encoding

            const char *pFirst = psz;

            if (n == npos)
                // all the rest:
                ret = pFirst;
            else
            {
                i = n;
                while (*psz && i--)
                    if (RT_FAILURE(RTStrGetCpEx(&psz, &cp)))
                        return ret;     // return empty string on bad encoding

                size_t cbCopy = psz - pFirst;
                if (cbCopy)
                {
                    ret.reserve(cbCopy + 1); // may throw bad_alloc
#ifndef RT_EXCEPTIONS_ENABLED
                    AssertRelease(capacity() >= cbCopy + 1);
#endif
                    memcpy(ret.m_psz, pFirst, cbCopy);
                    ret.m_cch = cbCopy;
                    ret.m_psz[cbCopy] = '\0';
                }
            }
        }
    }

    return ret;
}

bool RTCString::endsWith(const RTCString &that, CaseSensitivity cs /*= CaseSensitive*/) const
{
    size_t l1 = length();
    if (l1 == 0)
        return false;

    size_t l2 = that.length();
    if (l1 < l2)
        return false;
    /** @todo r=bird: If l2 is 0, then m_psz can be NULL and we will crash. See
     *        also handling of l2 == in startsWith. */

    size_t l = l1 - l2;
    if (cs == CaseSensitive)
        return ::RTStrCmp(&m_psz[l], that.m_psz) == 0;
    return ::RTStrICmp(&m_psz[l], that.m_psz) == 0;
}

bool RTCString::startsWith(const RTCString &that, CaseSensitivity cs /*= CaseSensitive*/) const
{
    size_t l1 = length();
    size_t l2 = that.length();
    if (l1 == 0 || l2 == 0) /** @todo r=bird: this differs from endsWith, and I think other IPRT code. If l2 == 0, it matches anything. */
        return false;

    if (l1 < l2)
        return false;

    if (cs == CaseSensitive)
        return ::RTStrNCmp(m_psz, that.m_psz, l2) == 0;
    return ::RTStrNICmp(m_psz, that.m_psz, l2) == 0;
}

bool RTCString::contains(const RTCString &that, CaseSensitivity cs /*= CaseSensitive*/) const
{
    /** @todo r-bird: Not checking for NULL strings like startsWith does (and
     *        endsWith only does half way). */
    if (cs == CaseSensitive)
        return ::RTStrStr(m_psz, that.m_psz) != NULL;
    return ::RTStrIStr(m_psz, that.m_psz) != NULL;
}

bool RTCString::contains(const char *pszNeedle, CaseSensitivity cs /*= CaseSensitive*/) const
{
    /** @todo r-bird: Not checking for NULL strings like startsWith does (and
     *        endsWith only does half way). */
    if (cs == CaseSensitive)
        return ::RTStrStr(m_psz, pszNeedle) != NULL;
    return ::RTStrIStr(m_psz, pszNeedle) != NULL;
}

int RTCString::toInt(uint64_t &i) const
{
    if (!m_psz)
        return VERR_NO_DIGITS;
    return RTStrToUInt64Ex(m_psz, NULL, 0, &i);
}

int RTCString::toInt(uint32_t &i) const
{
    if (!m_psz)
        return VERR_NO_DIGITS;
    return RTStrToUInt32Ex(m_psz, NULL, 0, &i);
}

RTCList<RTCString, RTCString *>
RTCString::split(const RTCString &a_rstrSep, SplitMode mode /* = RemoveEmptyParts */) const
{
    RTCList<RTCString> strRet;
    if (!m_psz)
        return strRet;
    if (a_rstrSep.isEmpty())
    {
        strRet.append(RTCString(m_psz));
        return strRet;
    }

    size_t      cch    = m_cch;
    char const *pszTmp = m_psz;
    while (cch > 0)
    {
        char const *pszNext = strstr(pszTmp, a_rstrSep.c_str());
        if (!pszNext)
        {
            strRet.append(RTCString(pszTmp, cch));
            break;
        }
        size_t cchNext = pszNext - pszTmp;
        if (   cchNext > 0
            || mode == KeepEmptyParts)
            strRet.append(RTCString(pszTmp, cchNext));
        pszTmp += cchNext + a_rstrSep.length();
        cch    -= cchNext + a_rstrSep.length();
    }

    return strRet;
}

/* static */
RTCString
RTCString::joinEx(const RTCList<RTCString, RTCString *> &a_rList,
                  const RTCString &a_rstrPrefix /* = "" */,
                  const RTCString &a_rstrSep /* = "" */)
{
    RTCString strRet;
    if (a_rList.size() > 1)
    {
        /* calc the required size */
        size_t cbNeeded = a_rstrSep.length() * (a_rList.size() - 1) + 1;
        cbNeeded += a_rstrPrefix.length() * (a_rList.size() - 1) + 1;
        for (size_t i = 0; i < a_rList.size(); ++i)
            cbNeeded += a_rList.at(i).length();
        strRet.reserve(cbNeeded);

        /* do the appending. */
        for (size_t i = 0; i < a_rList.size() - 1; ++i)
        {
            if (a_rstrPrefix.isNotEmpty())
                strRet.append(a_rstrPrefix);
            strRet.append(a_rList.at(i));
            strRet.append(a_rstrSep);
        }
        strRet.append(a_rList.last());
    }
    /* special case: one list item. */
    else if (a_rList.size() > 0)
    {
        if (a_rstrPrefix.isNotEmpty())
            strRet.append(a_rstrPrefix);
        strRet.append(a_rList.last());
    }

    return strRet;
}

/* static */
RTCString
RTCString::join(const RTCList<RTCString, RTCString *> &a_rList,
                const RTCString &a_rstrSep /* = "" */)
{
    return RTCString::joinEx(a_rList,
                             "" /* a_rstrPrefix */, a_rstrSep);
}

const RTCString operator+(const RTCString &a_rStr1, const RTCString &a_rStr2)
{
    RTCString strRet(a_rStr1);
    strRet += a_rStr2;
    return strRet;
}

const RTCString operator+(const RTCString &a_rStr1, const char *a_pszStr2)
{
    RTCString strRet(a_rStr1);
    strRet += a_pszStr2;
    return strRet;
}

const RTCString operator+(const char *a_psz1, const RTCString &a_rStr2)
{
    RTCString strRet(a_psz1);
    strRet += a_rStr2;
    return strRet;
}

