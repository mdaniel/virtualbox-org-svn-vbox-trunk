/* $Id$ */
/** @file
 * IPRT - Message-Digest Algorithm 2.
 */

/*
 * Copyright (C) 2009-2019 Oracle Corporation
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

#include <openssl/opensslconf.h>
#ifndef OPENSSL_NO_MD2
# include <openssl/md2.h>

# define RT_MD2_PRIVATE_CONTEXT
# include <iprt/md2.h>

# include <iprt/assert.h>

AssertCompile(RT_SIZEOFMEMB(RTMD2CONTEXT, abPadding) >= RT_SIZEOFMEMB(RTMD2CONTEXT, Private));


RTDECL(void) RTMd2(const void *pvBuf, size_t cbBuf, uint8_t pabDigest[RTMD2_HASH_SIZE])
{
    RTMD2CONTEXT Ctx;
    RTMd2Init(&Ctx);
    RTMd2Update(&Ctx, pvBuf, cbBuf);
    RTMd2Final(&Ctx, pabDigest);
}
RT_EXPORT_SYMBOL(RTMd2);


RTDECL(void) RTMd2Init(PRTMD2CONTEXT pCtx)
{
    MD2_Init(&pCtx->Private);
}
RT_EXPORT_SYMBOL(RTMd2Init);


RTDECL(void) RTMd2Update(PRTMD2CONTEXT pCtx, const void *pvBuf, size_t cbBuf)
{
    MD2_Update(&pCtx->Private, (const unsigned char *)pvBuf, cbBuf);
}
RT_EXPORT_SYMBOL(RTMd2Update);


RTDECL(void) RTMd2Final(PRTMD2CONTEXT pCtx, uint8_t pabDigest[RTMD2_HASH_SIZE])
{
    MD2_Final((unsigned char *)&pabDigest[0], &pCtx->Private);
}
RT_EXPORT_SYMBOL(RTMd2Final);


#else /* OPENSSL_NO_MD2 */
/*
 * If the OpenSSL build doesn't have MD2, use the IPRT implementation.
 */
# include "alt-md2.cpp"
#endif /* OPENSSL_NO_MD2 */

