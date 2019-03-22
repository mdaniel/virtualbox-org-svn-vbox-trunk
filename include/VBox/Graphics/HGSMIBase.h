/* $Id$ */
/** @file
 * VBox Host Guest Shared Memory Interface (HGSMI) - buffer management.
 */

/*
 * Copyright (C) 2006-2019 Oracle Corporation
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
 * NONINFRINGEMENT.  IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef ___VBox_Graphics_HGSMIBase_h___
#define ___VBox_Graphics_HGSMIBase_h___
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include "HGSMI.h"
#include "HGSMIContext.h"
#include "VBoxVideoIPRT.h"

RT_C_DECLS_BEGIN

/** @name Base HGSMI Buffer APIs
 * @{ */

/** Acknowlege an IRQ. */
DECLINLINE(void) VBoxHGSMIClearIrq(PHGSMIHOSTCOMMANDCONTEXT pCtx)
{
    VBVO_PORT_WRITE_U32(pCtx->port, HGSMIOFFSET_VOID);
}

DECLHIDDEN(void RT_UNTRUSTED_VOLATILE_HOST *) VBoxHGSMIBufferAlloc(PHGSMIGUESTCOMMANDCONTEXT pCtx, HGSMISIZE cbData,
                                                                   uint8_t u8Ch, uint16_t u16Op);
DECLHIDDEN(void) VBoxHGSMIBufferFree(PHGSMIGUESTCOMMANDCONTEXT pCtx, void RT_UNTRUSTED_VOLATILE_HOST *pvBuffer);
DECLHIDDEN(int)  VBoxHGSMIBufferSubmit(PHGSMIGUESTCOMMANDCONTEXT pCtx, void RT_UNTRUSTED_VOLATILE_HOST *pvBuffer);
/** @}  */

RT_C_DECLS_END

#endif
