/* $Id$ */
/** @file
 * IPRT Testcase - RTGetOpt
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
#include <iprt/net.h>
#include <iprt/getopt.h>

#include <iprt/err.h>
#include <iprt/string.h>
#include <iprt/test.h>


int main()
{
    RTTEST hTest;
    int rc = RTTestInitAndCreate("tstRTGetOpt", &hTest);
    if (rc)
        return rc;

    RTGETOPTSTATE GetState;
    RTGETOPTUNION Val;
#define CHECK(expr)  do { if (!(expr)) { RTTestIFailed("error line %d (iNext=%d): %s\n", __LINE__, GetState.iNext, #expr); } } while (0)
#define CHECK2(expr, fmt) \
    do { \
        if (!(expr)) { \
            RTTestIFailed("error line %d (iNext=%d): %s\n", __LINE__, GetState.iNext, #expr); \
            RTTestIFailureDetails fmt; \
         } \
    } while (0)

#define CHECK_pDef(paOpts, i) \
    CHECK2(Val.pDef == &(paOpts)[(i)], ("Got #%d (%p) expected #%d\n", (int)(Val.pDef - &(paOpts)[0]), Val.pDef, i));

#define CHECK_GETOPT(expr, chRet, iInc) \
    do { \
        const int iPrev = GetState.iNext; \
        const int rcGetOpt = (expr); \
        CHECK2(rcGetOpt == (chRet), ("got %d, expected %d\n", rcGetOpt, (chRet))); \
        CHECK2(GetState.iNext == (iInc) + iPrev, ("iNext=%d expected %d\n", GetState.iNext, (iInc) + iPrev)); \
        GetState.iNext = (iInc) + iPrev; \
    } while (0)

#define CHECK_GETOPT_STR(expr, chRet, iInc, str) \
    do { \
        const int iPrev = GetState.iNext; \
        const int rcGetOpt = (expr); \
        CHECK2(rcGetOpt == (chRet), ("got %d, expected %d\n", rcGetOpt, (chRet))); \
        CHECK2(GetState.iNext == (iInc) + iPrev, ("iNext=%d expected %d\n", GetState.iNext, (iInc) + iPrev)); \
        CHECK2(VALID_PTR(Val.psz) && !strcmp(Val.psz, (str)), ("got %s, expected %s\n", Val.psz, (str))); \
        GetState.iNext = (iInc) + iPrev; \
    } while (0)


    /*
     * The basics.
     */
    RTTestSub(hTest, "Basics");
    static const RTGETOPTDEF s_aOpts2[] =
    {
        { "--optwithstring",    's', RTGETOPT_REQ_STRING },
        { "--optwithint",       'i', RTGETOPT_REQ_INT32 },
        { "--verbose",          'v', RTGETOPT_REQ_NOTHING },
        { NULL,                 'q', RTGETOPT_REQ_NOTHING },
        { "--quiet",            384, RTGETOPT_REQ_NOTHING },
        { "-novalue",           385, RTGETOPT_REQ_NOTHING },
        { "-startvm",           386, RTGETOPT_REQ_STRING },
        { "nodash",             387, RTGETOPT_REQ_NOTHING },
        { "nodashval",          388, RTGETOPT_REQ_STRING },
        { "--gateway",          'g', RTGETOPT_REQ_IPV4ADDR },
        { "--mac",              'm', RTGETOPT_REQ_MACADDR },
        { "--strindex",         400, RTGETOPT_REQ_STRING  | RTGETOPT_FLAG_INDEX },
        { "strindex",           400, RTGETOPT_REQ_STRING  | RTGETOPT_FLAG_INDEX },
        { "--intindex",         401, RTGETOPT_REQ_INT32   | RTGETOPT_FLAG_INDEX },
        { "--macindex",         402, RTGETOPT_REQ_MACADDR | RTGETOPT_FLAG_INDEX },
        { "--indexnovalue",     403, RTGETOPT_REQ_NOTHING | RTGETOPT_FLAG_INDEX },
        { "--macindexnegative", 404, RTGETOPT_REQ_NOTHING },
        { "--twovalues",        405, RTGETOPT_REQ_STRING },
        { "--twovaluesindex",   406, RTGETOPT_REQ_UINT32 | RTGETOPT_FLAG_INDEX },
        { "--threevalues",      407, RTGETOPT_REQ_UINT32 },
        { "--boolean",          408, RTGETOPT_REQ_BOOL_ONOFF },
        { "--booleanindex",     409, RTGETOPT_REQ_BOOL_ONOFF | RTGETOPT_FLAG_INDEX },
        { "--pair32",           410, RTGETOPT_REQ_UINT32_PAIR  },
        { "--optpair32",        411, RTGETOPT_REQ_UINT32_OPTIONAL_PAIR  },
        { "--optpair64",        412, RTGETOPT_REQ_UINT64_OPTIONAL_PAIR  },
    };

    const char *argv2[] =
    {
        "-s",               "string1",
        "-sstring2",
        "-s:string3",
        "-s=string4",
        "-s:",
        "-s=",
        "--optwithstring",  "string5",
        "--optwithstring:string6",
        "--optwithstring=string7",
        "--optwithstring:",
        "--optwithstring=",

        "-i",               "-42",
        "-i:-42",
        "-i=-42",

        "--optwithint",     "42",
        "--optwithint:42",
        "--optwithint=42",

        "-v",
        "--verbose",
        "-q",
        "--quiet",

        "-novalue",
        "-startvm",         "myvm",

        "nodash",
        "nodashval",        "string9",

        "filename1",
        "-q",
        "filename2",

        "-vqi999",

        "-g192.168.1.1",

        "-m08:0:27:00:ab:f3",
        "--mac:1:::::c",

        "--strindex786",    "string10",
        "--strindex786:string11",
        "--strindex786=string12",
        "strindex687",      "string13",
        "strindex687:string14",
        "strindex687=string15",
        "strindex688:",
        "strindex689=",
        "--intindex137",    "1000",
        "--macindex138",    "08:0:27:00:ab:f3",
        "--indexnovalue1",
        "--macindexnegative",

        "--twovalues",       "firstvalue", "secondvalue",
        "--twovalues:firstvalue",          "secondvalue",
        "--twovaluesindex4", "1",          "0xA",
        "--twovaluesindex5=2",             "0xB",
        "--threevalues",     "1",          "0xC",          "thirdvalue",

        /* bool on/off */
        "--boolean",         "on",
        "--boolean",         "off",
        "--boolean",         "invalid",
        "--booleanindex2",   "on",
        "--booleanindex7",   "off",
        "--booleanindex9",   "invalid",

        /* standard options */
        "--help",
        "-help",
        "-?",
        "-h",
        "--version",
        "-version",
        "-V",

        /* 32-bit pairs */
        "--pair32", "1536:0x1536",
        "--optpair32", "0x42:042",
        "--optpair32", "0128",
        "--optpair64", "0x128 0x42",
        "--optpair64", "0x128 :0x42",
        "--optpair64", "0x128",

        /* done */
        NULL
    };
    int argc2 = (int)RT_ELEMENTS(argv2) - 1;

    CHECK(RT_SUCCESS(RTGetOptInit(&GetState, argc2, (char **)argv2, &s_aOpts2[0], RT_ELEMENTS(s_aOpts2), 0, 0 /* fFlags */)));

    CHECK_GETOPT(RTGetOpt(&GetState, &Val), 's', 2);
    CHECK(VALID_PTR(Val.psz) && !strcmp(Val.psz, "string1"));
    CHECK(GetState.uIndex == UINT32_MAX);
    CHECK_GETOPT(RTGetOpt(&GetState, &Val), 's', 1);
    CHECK(VALID_PTR(Val.psz) && !strcmp(Val.psz, "string2"));
    CHECK(GetState.uIndex == UINT32_MAX);
    CHECK_GETOPT(RTGetOpt(&GetState, &Val), 's', 1);
    CHECK(VALID_PTR(Val.psz) && !strcmp(Val.psz, "string3"));
    CHECK(GetState.uIndex == UINT32_MAX);
    CHECK_GETOPT(RTGetOpt(&GetState, &Val), 's', 1);
    CHECK(VALID_PTR(Val.psz) && !strcmp(Val.psz, "string4"));
    CHECK(GetState.uIndex == UINT32_MAX);
    CHECK_GETOPT(RTGetOpt(&GetState, &Val), 's', 1);
    CHECK(VALID_PTR(Val.psz) && !strcmp(Val.psz, ""));
    CHECK(GetState.uIndex == UINT32_MAX);
    CHECK_GETOPT(RTGetOpt(&GetState, &Val), 's', 1);
    CHECK(VALID_PTR(Val.psz) && !strcmp(Val.psz, ""));
    CHECK(GetState.uIndex == UINT32_MAX);
    CHECK_GETOPT(RTGetOpt(&GetState, &Val), 's', 2);
    CHECK(VALID_PTR(Val.psz) && !strcmp(Val.psz, "string5"));
    CHECK(GetState.uIndex == UINT32_MAX);
    CHECK_GETOPT(RTGetOpt(&GetState, &Val), 's', 1);
    CHECK(VALID_PTR(Val.psz) && !strcmp(Val.psz, "string6"));
    CHECK(GetState.uIndex == UINT32_MAX);
    CHECK_GETOPT(RTGetOpt(&GetState, &Val), 's', 1);
    CHECK(VALID_PTR(Val.psz) && !strcmp(Val.psz, "string7"));
    CHECK(GetState.uIndex == UINT32_MAX);
    CHECK_GETOPT(RTGetOpt(&GetState, &Val), 's', 1);
    CHECK(VALID_PTR(Val.psz) && !strcmp(Val.psz, ""));
    CHECK(GetState.uIndex == UINT32_MAX);
    CHECK_GETOPT(RTGetOpt(&GetState, &Val), 's', 1);
    CHECK(VALID_PTR(Val.psz) && !strcmp(Val.psz, ""));
    CHECK(GetState.uIndex == UINT32_MAX);

    /* -i */
    CHECK_GETOPT(RTGetOpt(&GetState, &Val), 'i', 2);
    CHECK(Val.i32 == -42);
    CHECK_GETOPT(RTGetOpt(&GetState, &Val), 'i', 1);
    CHECK(Val.i32 == -42);
    CHECK_GETOPT(RTGetOpt(&GetState, &Val), 'i', 1);
    CHECK(Val.i32 == -42);

    /* --optwithint */
    CHECK_GETOPT(RTGetOpt(&GetState, &Val), 'i', 2);
    CHECK(Val.i32 == 42);
    CHECK_GETOPT(RTGetOpt(&GetState, &Val), 'i', 1);
    CHECK(Val.i32 == 42);
    CHECK_GETOPT(RTGetOpt(&GetState, &Val), 'i', 1);
    CHECK(Val.i32 == 42);

    CHECK_GETOPT(RTGetOpt(&GetState, &Val), 'v', 1);
    CHECK_pDef(s_aOpts2, 2);
    CHECK_GETOPT(RTGetOpt(&GetState, &Val), 'v', 1);
    CHECK_pDef(s_aOpts2, 2);

    CHECK_GETOPT(RTGetOpt(&GetState, &Val), 'q', 1);
    CHECK_pDef(s_aOpts2, 3);
    CHECK_GETOPT(RTGetOpt(&GetState, &Val), 384, 1);
    CHECK_pDef(s_aOpts2, 4);

    /* -novalue / -startvm (single dash long options) */
    CHECK_GETOPT(RTGetOpt(&GetState, &Val), 385, 1);
    CHECK_pDef(s_aOpts2, 5);
    CHECK_GETOPT(RTGetOpt(&GetState, &Val), 386, 2);
    CHECK(VALID_PTR(Val.psz) && !strcmp(Val.psz, "myvm"));

    /* no-dash options */
    CHECK_GETOPT(RTGetOpt(&GetState, &Val), 387, 1);
    CHECK_pDef(s_aOpts2, 7);
    CHECK_GETOPT(RTGetOpt(&GetState, &Val), 388, 2);
    CHECK(VALID_PTR(Val.psz) && !strcmp(Val.psz, "string9"));

    /* non-option, option, non-option  */
    CHECK_GETOPT(RTGetOpt(&GetState, &Val), VINF_GETOPT_NOT_OPTION, 1);
    CHECK(Val.psz && !strcmp(Val.psz, "filename1"));
    CHECK_GETOPT(RTGetOpt(&GetState, &Val), 'q', 1);
    CHECK_pDef(s_aOpts2, 3);
    CHECK_GETOPT(RTGetOpt(&GetState, &Val), VINF_GETOPT_NOT_OPTION, 1);
    CHECK(Val.psz && !strcmp(Val.psz, "filename2"));

    /* compress short options */
    CHECK_GETOPT(RTGetOpt(&GetState, &Val), 'v', 0);
    CHECK_pDef(s_aOpts2, 2);
    CHECK_GETOPT(RTGetOpt(&GetState, &Val), 'q', 0);
    CHECK_pDef(s_aOpts2, 3);
    CHECK_GETOPT(RTGetOpt(&GetState, &Val), 'i', 1);
    CHECK(Val.i32 == 999);

    /* IPv4 */
    RTTestSub(hTest, "RTGetOpt - IPv4");
    CHECK_GETOPT(RTGetOpt(&GetState, &Val), 'g', 1);
    CHECK(Val.IPv4Addr.u == RT_H2N_U32_C(RT_BSWAP_U32_C(RT_MAKE_U32_FROM_U8(192,168,1,1))));

    /* Ethernet MAC address. */
    RTTestSub(hTest, "RTGetOpt - MAC Address");
    CHECK_GETOPT(RTGetOpt(&GetState, &Val), 'm', 1);
    CHECK(   Val.MacAddr.au8[0] == 0x08
          && Val.MacAddr.au8[1] == 0x00
          && Val.MacAddr.au8[2] == 0x27
          && Val.MacAddr.au8[3] == 0x00
          && Val.MacAddr.au8[4] == 0xab
          && Val.MacAddr.au8[5] == 0xf3);
    CHECK_GETOPT(RTGetOpt(&GetState, &Val), 'm', 1);
    CHECK(   Val.MacAddr.au8[0] == 0x01
          && Val.MacAddr.au8[1] == 0x00
          && Val.MacAddr.au8[2] == 0x00
          && Val.MacAddr.au8[3] == 0x00
          && Val.MacAddr.au8[4] == 0x00
          && Val.MacAddr.au8[5] == 0x0c);

    /* string with indexed argument */
    RTTestSub(hTest, "RTGetOpt - Option w/ Index");
    CHECK_GETOPT(RTGetOpt(&GetState, &Val), 400, 2);
    CHECK(VALID_PTR(Val.psz) && !strcmp(Val.psz, "string10"));
    CHECK(GetState.uIndex == 786);

    CHECK_GETOPT(RTGetOpt(&GetState, &Val), 400, 1);
    CHECK(VALID_PTR(Val.psz) && !strcmp(Val.psz, "string11"));
    CHECK(GetState.uIndex == 786);

    CHECK_GETOPT(RTGetOpt(&GetState, &Val), 400, 1);
    CHECK(VALID_PTR(Val.psz) && !strcmp(Val.psz, "string12"));
    CHECK(GetState.uIndex == 786);

    CHECK_GETOPT(RTGetOpt(&GetState, &Val), 400, 2);
    CHECK(VALID_PTR(Val.psz) && !strcmp(Val.psz, "string13"));
    CHECK(GetState.uIndex == 687);

    CHECK_GETOPT(RTGetOpt(&GetState, &Val), 400, 1);
    CHECK(VALID_PTR(Val.psz) && !strcmp(Val.psz, "string14"));
    CHECK(GetState.uIndex == 687);

    CHECK_GETOPT(RTGetOpt(&GetState, &Val), 400, 1);
    CHECK(VALID_PTR(Val.psz) && !strcmp(Val.psz, "string15"));
    CHECK(GetState.uIndex == 687);

    CHECK_GETOPT(RTGetOpt(&GetState, &Val), 400, 1);
    CHECK(VALID_PTR(Val.psz) && !strcmp(Val.psz, ""));
    CHECK(GetState.uIndex == 688);

    CHECK_GETOPT(RTGetOpt(&GetState, &Val), 400, 1);
    CHECK(VALID_PTR(Val.psz) && !strcmp(Val.psz, ""));
    CHECK(GetState.uIndex == 689);

    CHECK_GETOPT(RTGetOpt(&GetState, &Val), 401, 2);
    CHECK(Val.i32 == 1000);
    CHECK(GetState.uIndex == 137);

    CHECK_GETOPT(RTGetOpt(&GetState, &Val), 402, 2);
    CHECK(   Val.MacAddr.au8[0] == 0x08
          && Val.MacAddr.au8[1] == 0x00
          && Val.MacAddr.au8[2] == 0x27
          && Val.MacAddr.au8[3] == 0x00
          && Val.MacAddr.au8[4] == 0xab
          && Val.MacAddr.au8[5] == 0xf3);
    CHECK(GetState.uIndex == 138);

    CHECK_GETOPT(RTGetOpt(&GetState, &Val), 403, 1);
    CHECK(GetState.uIndex == 1);

    CHECK_GETOPT(RTGetOpt(&GetState, &Val), 404, 1);
    CHECK(GetState.uIndex == UINT32_MAX);

    /* RTGetOptFetchValue tests */
    RTTestSub(hTest, "RTGetOptFetchValue");
    CHECK_GETOPT(RTGetOpt(&GetState, &Val), 405, 2);
    CHECK(VALID_PTR(Val.psz) && !strcmp(Val.psz, "firstvalue"));
    CHECK(GetState.uIndex == UINT32_MAX);
    CHECK_GETOPT(RTGetOptFetchValue(&GetState, &Val, RTGETOPT_REQ_STRING), VINF_SUCCESS, 1);
    CHECK(VALID_PTR(Val.psz) && !strcmp(Val.psz, "secondvalue"));
    CHECK(GetState.uIndex == UINT32_MAX);

    CHECK_GETOPT(RTGetOpt(&GetState, &Val), 405, 1);
    CHECK(VALID_PTR(Val.psz) && !strcmp(Val.psz, "firstvalue"));
    CHECK(GetState.uIndex == UINT32_MAX);
    CHECK_GETOPT(RTGetOptFetchValue(&GetState, &Val, RTGETOPT_REQ_STRING), VINF_SUCCESS, 1);
    CHECK(VALID_PTR(Val.psz) && !strcmp(Val.psz, "secondvalue"));
    CHECK(GetState.uIndex == UINT32_MAX);

    CHECK_GETOPT(RTGetOpt(&GetState, &Val), 406, 2);
    CHECK(Val.u32 == 1);
    CHECK(GetState.uIndex == 4);
    CHECK_GETOPT(RTGetOptFetchValue(&GetState, &Val, RTGETOPT_REQ_UINT32), VINF_SUCCESS, 1);
    CHECK(Val.u32 == 10);
    CHECK(GetState.uIndex == 4);

    CHECK_GETOPT(RTGetOpt(&GetState, &Val), 406, 1);
    CHECK(Val.u32 == 2);
    CHECK(GetState.uIndex == 5);
    CHECK_GETOPT(RTGetOptFetchValue(&GetState, &Val, RTGETOPT_REQ_UINT32), VINF_SUCCESS, 1);
    CHECK(Val.u32 == 11);
    CHECK(GetState.uIndex == 5);

    CHECK_GETOPT(RTGetOpt(&GetState, &Val), 407, 2);
    CHECK(Val.u32 == 1);
    CHECK(GetState.uIndex == UINT32_MAX);
    CHECK_GETOPT(RTGetOptFetchValue(&GetState, &Val, RTGETOPT_REQ_UINT32), VINF_SUCCESS, 1);
    CHECK(Val.u32 == 12);
    CHECK(GetState.uIndex == UINT32_MAX);
    CHECK_GETOPT(RTGetOptFetchValue(&GetState, &Val, RTGETOPT_REQ_STRING), VINF_SUCCESS, 1);
    CHECK(VALID_PTR(Val.psz) && !strcmp(Val.psz, "thirdvalue"));
    CHECK(GetState.uIndex == UINT32_MAX);

    /* bool on/off tests */
    RTTestSub(hTest, "RTGetOpt - bool on/off");
    CHECK_GETOPT(RTGetOpt(&GetState, &Val), 408, 2);
    CHECK(Val.f);
    CHECK_GETOPT(RTGetOpt(&GetState, &Val), 408, 2);
    CHECK(!Val.f);
    CHECK_GETOPT(RTGetOpt(&GetState, &Val), VERR_GETOPT_UNKNOWN_OPTION, 2);
    CHECK(VALID_PTR(Val.psz) && !strcmp(Val.psz, "invalid"));

    /* bool on/off with indexed argument */
    CHECK_GETOPT(RTGetOpt(&GetState, &Val), 409, 2);
    CHECK(Val.f);
    CHECK(GetState.uIndex == 2);
    CHECK_GETOPT(RTGetOpt(&GetState, &Val), 409, 2);
    CHECK(!Val.f);
    CHECK(GetState.uIndex == 7);
    CHECK_GETOPT(RTGetOpt(&GetState, &Val), VERR_GETOPT_UNKNOWN_OPTION, 2);
    CHECK(VALID_PTR(Val.psz) && !strcmp(Val.psz, "invalid"));

    /* standard options. */
    RTTestSub(hTest, "Standard options");
    CHECK_GETOPT(RTGetOpt(&GetState, &Val), 'h', 1);
    CHECK_GETOPT(RTGetOpt(&GetState, &Val), 'h', 1);
    CHECK_GETOPT(RTGetOpt(&GetState, &Val), 'h', 1);
    CHECK_GETOPT(RTGetOpt(&GetState, &Val), 'h', 1);
    CHECK_GETOPT(RTGetOpt(&GetState, &Val), 'V', 1);
    CHECK_GETOPT(RTGetOpt(&GetState, &Val), 'V', 1);
    CHECK_GETOPT(RTGetOpt(&GetState, &Val), 'V', 1);

    /* 32-bit pairs */
    RTTestSub(hTest, "RTGetOpt - pairs");
    CHECK_GETOPT(RTGetOpt(&GetState, &Val), 410, 2);
    CHECK(Val.PairU32.uFirst == 1536);
    CHECK(Val.PairU32.uSecond == 0x1536);
    CHECK_GETOPT(RTGetOpt(&GetState, &Val), 411, 2);
    CHECK(Val.PairU32.uFirst == 0x42);
    CHECK(Val.PairU32.uSecond == 42);
    CHECK_GETOPT(RTGetOpt(&GetState, &Val), 411, 2);
    CHECK(Val.PairU32.uFirst == 128);
    CHECK(Val.PairU32.uSecond == UINT32_MAX);
    CHECK_GETOPT(RTGetOpt(&GetState, &Val), 412, 2);
    CHECK(Val.PairU64.uFirst == 0x128);
    CHECK(Val.PairU64.uSecond == 0x42);
    CHECK_GETOPT(RTGetOpt(&GetState, &Val), 412, 2);
    CHECK(Val.PairU64.uFirst == 0x128);
    CHECK(Val.PairU64.uSecond == 0x42);
    CHECK_GETOPT(RTGetOpt(&GetState, &Val), 412, 2);
    CHECK(Val.PairU64.uFirst == 0x128);
    CHECK(Val.PairU64.uSecond == UINT64_MAX);

    /* the end */
    CHECK_GETOPT(RTGetOpt(&GetState, &Val), 0, 0);
    CHECK(Val.pDef == NULL);
    CHECK(argc2 == GetState.iNext);

    /*
     * Options first.
     */
    RTTestSub(hTest, "Options first");
    const char *argv3[] =
    {
        "foo1",
        "-s",               "string1",
        "foo2",
        "--optwithstring",  "string2",
        "foo3",
        "-i",               "-42",
        "foo4",
        "-i:-42",
        "-i=-42",
        "foo5",
        "foo6",
        "foo7",
        "-i:-42",
        "-i=-42",
        "foo8",
        "--twovalues",       "firstvalue", "secondvalue",
        "foo9",
        "--twovalues:firstvalue",          "secondvalue",
        "foo10",
        "--",
        "--optwithstring",
        "-s",
        "-i",
        "foo11",
        "foo12",

        /* done */
        NULL
    };
    int argc3 = (int)RT_ELEMENTS(argv3) - 1;

    CHECK(RT_SUCCESS(RTGetOptInit(&GetState, argc3, (char **)argv3, &s_aOpts2[0], RT_ELEMENTS(s_aOpts2), 0,
                                  RTGETOPTINIT_FLAGS_OPTS_FIRST)));

    /* -s */
    CHECK_GETOPT_STR(RTGetOpt(&GetState, &Val), 's', 2, "string1");
    CHECK(GetState.uIndex == UINT32_MAX);
    CHECK_GETOPT_STR(RTGetOpt(&GetState, &Val), 's', 2, "string2");
    CHECK(GetState.uIndex == UINT32_MAX);

    /* -i */
    CHECK_GETOPT(RTGetOpt(&GetState, &Val), 'i', 2);
    CHECK(Val.i32 == -42);
    CHECK_GETOPT(RTGetOpt(&GetState, &Val), 'i', 1);
    CHECK(Val.i32 == -42);
    CHECK_GETOPT(RTGetOpt(&GetState, &Val), 'i', 1);
    CHECK(Val.i32 == -42);
    CHECK_GETOPT(RTGetOpt(&GetState, &Val), 'i', 1);
    CHECK(Val.i32 == -42);
    CHECK_GETOPT(RTGetOpt(&GetState, &Val), 'i', 1);
    CHECK(Val.i32 == -42);

    /* --twovalues */
    CHECK_GETOPT_STR(RTGetOpt(&GetState, &Val), 405, 2, "firstvalue");
    CHECK(GetState.uIndex == UINT32_MAX);
    CHECK_GETOPT_STR(RTGetOptFetchValue(&GetState, &Val, RTGETOPT_REQ_STRING), VINF_SUCCESS, 1, "secondvalue");
    CHECK(GetState.uIndex == UINT32_MAX);

    CHECK_GETOPT_STR(RTGetOpt(&GetState, &Val), 405, 1, "firstvalue");
    CHECK(GetState.uIndex == UINT32_MAX);
    CHECK_GETOPT_STR(RTGetOptFetchValue(&GetState, &Val, RTGETOPT_REQ_STRING), VINF_SUCCESS, 1, "secondvalue");
    CHECK(GetState.uIndex == UINT32_MAX);

    /* -- */
    CHECK_GETOPT_STR(RTGetOpt(&GetState, &Val), VINF_GETOPT_NOT_OPTION, 2, "foo1");
    CHECK_GETOPT_STR(RTGetOpt(&GetState, &Val), VINF_GETOPT_NOT_OPTION, 1, "foo2");
    CHECK_GETOPT_STR(RTGetOpt(&GetState, &Val), VINF_GETOPT_NOT_OPTION, 1, "foo3");
    CHECK_GETOPT_STR(RTGetOpt(&GetState, &Val), VINF_GETOPT_NOT_OPTION, 1, "foo4");
    CHECK_GETOPT_STR(RTGetOpt(&GetState, &Val), VINF_GETOPT_NOT_OPTION, 1, "foo5");
    CHECK_GETOPT_STR(RTGetOpt(&GetState, &Val), VINF_GETOPT_NOT_OPTION, 1, "foo6");
    CHECK_GETOPT_STR(RTGetOpt(&GetState, &Val), VINF_GETOPT_NOT_OPTION, 1, "foo7");
    CHECK_GETOPT_STR(RTGetOpt(&GetState, &Val), VINF_GETOPT_NOT_OPTION, 1, "foo8");
    CHECK_GETOPT_STR(RTGetOpt(&GetState, &Val), VINF_GETOPT_NOT_OPTION, 1, "foo9");
    CHECK_GETOPT_STR(RTGetOpt(&GetState, &Val), VINF_GETOPT_NOT_OPTION, 1, "foo10");
    CHECK_GETOPT_STR(RTGetOpt(&GetState, &Val), VINF_GETOPT_NOT_OPTION, 1, "--optwithstring");
    CHECK_GETOPT_STR(RTGetOpt(&GetState, &Val), VINF_GETOPT_NOT_OPTION, 1, "-s");
    CHECK_GETOPT_STR(RTGetOpt(&GetState, &Val), VINF_GETOPT_NOT_OPTION, 1, "-i");
    CHECK_GETOPT_STR(RTGetOpt(&GetState, &Val), VINF_GETOPT_NOT_OPTION, 1, "foo11");
    CHECK_GETOPT_STR(RTGetOpt(&GetState, &Val), VINF_GETOPT_NOT_OPTION, 1, "foo12");

    /* the end */
    CHECK_GETOPT(RTGetOpt(&GetState, &Val), 0, 0);
    CHECK(Val.pDef == NULL);
    CHECK(argc3 == GetState.iNext);

    /*
     * Options first, part 2: No dash-dash.
     */
    const char *argv4[] =
    {
        "foo1",
        "-s",               "string1",
        "foo2",
        "--optwithstring",  "string2",
        "foo3",
        "-i",               "-42",
        "foo4",
        "-i:-42",
        "-i=-42",
        "foo5",
        "foo6",
        "foo7",
        "-i:-42",
        "-i=-42",
        "foo8",
        "--twovalues",       "firstvalue", "secondvalue",
        "foo9",
        "--twovalues:firstvalue",          "secondvalue",
        "foo10",
        "foo11",
        "foo12",

        /* done */
        NULL
    };
    int argc4 = (int)RT_ELEMENTS(argv4) - 1;

    CHECK(RT_SUCCESS(RTGetOptInit(&GetState, argc4, (char **)argv4, &s_aOpts2[0], RT_ELEMENTS(s_aOpts2), 0,
                                  RTGETOPTINIT_FLAGS_OPTS_FIRST)));

    /* -s */
    CHECK_GETOPT_STR(RTGetOpt(&GetState, &Val), 's', 2, "string1");
    CHECK(GetState.uIndex == UINT32_MAX);
    CHECK_GETOPT_STR(RTGetOpt(&GetState, &Val), 's', 2, "string2");
    CHECK(GetState.uIndex == UINT32_MAX);

    /* -i */
    CHECK_GETOPT(RTGetOpt(&GetState, &Val), 'i', 2);
    CHECK(Val.i32 == -42);
    CHECK_GETOPT(RTGetOpt(&GetState, &Val), 'i', 1);
    CHECK(Val.i32 == -42);
    CHECK_GETOPT(RTGetOpt(&GetState, &Val), 'i', 1);
    CHECK(Val.i32 == -42);
    CHECK_GETOPT(RTGetOpt(&GetState, &Val), 'i', 1);
    CHECK(Val.i32 == -42);
    CHECK_GETOPT(RTGetOpt(&GetState, &Val), 'i', 1);
    CHECK(Val.i32 == -42);

    /* --twovalues */
    CHECK_GETOPT_STR(RTGetOpt(&GetState, &Val), 405, 2, "firstvalue");
    CHECK(GetState.uIndex == UINT32_MAX);
    CHECK_GETOPT_STR(RTGetOptFetchValue(&GetState, &Val, RTGETOPT_REQ_STRING), VINF_SUCCESS, 1, "secondvalue");
    CHECK(GetState.uIndex == UINT32_MAX);

    CHECK_GETOPT_STR(RTGetOpt(&GetState, &Val), 405, 1, "firstvalue");
    CHECK(GetState.uIndex == UINT32_MAX);
    CHECK_GETOPT_STR(RTGetOptFetchValue(&GetState, &Val, RTGETOPT_REQ_STRING), VINF_SUCCESS, 1, "secondvalue");
    CHECK(GetState.uIndex == UINT32_MAX);

    /* -- */
    CHECK_GETOPT_STR(RTGetOpt(&GetState, &Val), VINF_GETOPT_NOT_OPTION, 1, "foo1");
    CHECK_GETOPT_STR(RTGetOpt(&GetState, &Val), VINF_GETOPT_NOT_OPTION, 1, "foo2");
    CHECK_GETOPT_STR(RTGetOpt(&GetState, &Val), VINF_GETOPT_NOT_OPTION, 1, "foo3");
    CHECK_GETOPT_STR(RTGetOpt(&GetState, &Val), VINF_GETOPT_NOT_OPTION, 1, "foo4");
    CHECK_GETOPT_STR(RTGetOpt(&GetState, &Val), VINF_GETOPT_NOT_OPTION, 1, "foo5");
    CHECK_GETOPT_STR(RTGetOpt(&GetState, &Val), VINF_GETOPT_NOT_OPTION, 1, "foo6");
    CHECK_GETOPT_STR(RTGetOpt(&GetState, &Val), VINF_GETOPT_NOT_OPTION, 1, "foo7");
    CHECK_GETOPT_STR(RTGetOpt(&GetState, &Val), VINF_GETOPT_NOT_OPTION, 1, "foo8");
    CHECK_GETOPT_STR(RTGetOpt(&GetState, &Val), VINF_GETOPT_NOT_OPTION, 1, "foo9");
    CHECK_GETOPT_STR(RTGetOpt(&GetState, &Val), VINF_GETOPT_NOT_OPTION, 1, "foo10");
    CHECK_GETOPT_STR(RTGetOpt(&GetState, &Val), VINF_GETOPT_NOT_OPTION, 1, "foo11");
    CHECK_GETOPT_STR(RTGetOpt(&GetState, &Val), VINF_GETOPT_NOT_OPTION, 1, "foo12");

    /* the end */
    CHECK_GETOPT(RTGetOpt(&GetState, &Val), 0, 0);
    CHECK(Val.pDef == NULL);
    CHECK(argc4 == GetState.iNext);



    /*
     * Summary.
     */
    return RTTestSummaryAndDestroy(hTest);
}

