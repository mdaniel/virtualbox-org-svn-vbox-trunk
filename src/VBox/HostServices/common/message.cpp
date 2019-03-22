/** @file
 * Base class for wrapping HCGM messages.
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

#include <VBox/HostServices/Service.h>

using namespace HGCM;

Message::Message(void)
    : m_uMsg(0)
    , m_cParms(0)
    , m_paParms(NULL) { }

Message::Message(uint32_t uMsg, uint32_t cParms, VBOXHGCMSVCPARM aParms[])
    : m_uMsg(0)
    , m_cParms(0)
    , m_paParms(NULL)
{
    initData(uMsg, cParms, aParms);
}

Message::~Message(void)
{
    reset();
}

/**
 * Resets the message by free'ing all allocated parameters and resetting the rest.
 */
void Message::reset(void)
{
    if (m_paParms)
    {
        for (uint32_t i = 0; i < m_cParms; ++i)
        {
            switch (m_paParms[i].type)
            {
                case VBOX_HGCM_SVC_PARM_PTR:
                    if (m_paParms[i].u.pointer.size)
                        RTMemFree(m_paParms[i].u.pointer.addr);
                    break;
            }
        }
        RTMemFree(m_paParms);
        m_paParms = 0;
    }
    m_cParms = 0;
    m_uMsg = 0;
}

/**
 * Returns the parameter count of this message.
 *
 * @returns Parameter count.
 */
uint32_t Message::GetParamCount(void) const
{
    return m_cParms;
}

/**
 * Retrieves the raw HGCM parameter data
 *
 * @returns IPRT status code.
 * @param   uMsg            Message type to retrieve the parameter data for. Needed for sanity.
 * @param   cParms          Size (in parameters) of @a aParms array.
 * @param   aParms          Where to store the HGCM parameter data.
 */
int Message::GetData(uint32_t uMsg, uint32_t cParms, VBOXHGCMSVCPARM aParms[]) const
{
    if (m_uMsg != uMsg)
    {
        LogFlowFunc(("Stored message type (%RU32) does not match request (%RU32)\n", m_uMsg, uMsg));
        return VERR_INVALID_PARAMETER;
    }
    if (m_cParms > cParms)
    {
        LogFlowFunc(("Stored parameter count (%RU32) exceeds request buffer (%RU32)\n", m_cParms, cParms));
        return VERR_INVALID_PARAMETER;
    }

    return Message::CopyParms(&aParms[0], cParms, m_paParms, m_cParms, false /* fDeepCopy */);
}

/**
 * Retrieves a specific parameter value as uint32_t.
 *
 * @returns IPRT status code.
 * @param   uParm           Index of parameter to retrieve.
 * @param   pu32Info        Where to store the parameter value.
 */
int Message::GetParmU32(uint32_t uParm, uint32_t *pu32Info) const
{
    AssertPtrNullReturn(pu32Info, VERR_INVALID_PARAMETER);
    AssertReturn(uParm < m_cParms, VERR_INVALID_PARAMETER);
    AssertReturn(m_paParms[uParm].type == VBOX_HGCM_SVC_PARM_32BIT, VERR_INVALID_PARAMETER);

    *pu32Info = m_paParms[uParm].u.uint32;

    return VINF_SUCCESS;
}

/**
 * Retrieves a specific parameter value as uint64_t.
 *
 * @returns IPRT status code.
 * @param   uParm           Index of parameter to retrieve.
 * @param   pu64Info        Where to store the parameter value.
 */
int Message::GetParmU64(uint32_t uParm, uint64_t *pu64Info) const
{
    AssertPtrNullReturn(pu64Info, VERR_INVALID_PARAMETER);
    AssertReturn(uParm < m_cParms, VERR_INVALID_PARAMETER);
    AssertReturn(m_paParms[uParm].type == VBOX_HGCM_SVC_PARM_64BIT, VERR_INVALID_PARAMETER);

    *pu64Info = m_paParms[uParm].u.uint64;

    return VINF_SUCCESS;
}

/**
 * Retrieves a specific parameter value as a data address + size.
 *
 * @returns IPRT status code.
 * @param   uParm           Index of parameter to retrieve.
 * @param   ppvAddr         Where to store the data address.
 * @param   pcbSize         Where to store the data size (in bytes).
 *
 * @remarks Does not copy (store) the actual content of the pointer (deep copy).
 */
int Message::GetParmPtr(uint32_t uParm, void **ppvAddr, uint32_t *pcbSize) const
{
    AssertPtrNullReturn(ppvAddr, VERR_INVALID_PARAMETER);
    AssertPtrNullReturn(pcbSize, VERR_INVALID_PARAMETER);
    AssertReturn(uParm < m_cParms, VERR_INVALID_PARAMETER);
    AssertReturn(m_paParms[uParm].type == VBOX_HGCM_SVC_PARM_PTR, VERR_INVALID_PARAMETER);

    *ppvAddr = m_paParms[uParm].u.pointer.addr;
    *pcbSize = m_paParms[uParm].u.pointer.size;

    return VINF_SUCCESS;
}

/**
 * Returns the type of this message.
 *
 * @returns Message type.
 */
uint32_t Message::GetType(void) const
{
    return m_uMsg;
}

/**
 * Copies HGCM parameters from source to destination.
 *
 * @returns IPRT status code.
 * @param   paParmsDst      Destination array to copy parameters to.
 * @param   cParmsDst       Size (in parameters) of destination array.
 * @param   paParmsSrc      Source array to copy parameters from.
 * @param   cParmsSrc       Size (in parameters) of source array.
 * @param   fDeepCopy       Whether to perform a deep copy of pointer parameters or not.
 *
 * @remark Static convenience function.
 */
/* static */
int Message::CopyParms(PVBOXHGCMSVCPARM paParmsDst, uint32_t cParmsDst,
                       PVBOXHGCMSVCPARM paParmsSrc, uint32_t cParmsSrc,
                       bool fDeepCopy)
{
    AssertPtrReturn(paParmsSrc, VERR_INVALID_POINTER);
    AssertPtrReturn(paParmsDst, VERR_INVALID_POINTER);

    if (cParmsSrc > cParmsDst)
        return VERR_BUFFER_OVERFLOW;

    int rc = VINF_SUCCESS;
    for (uint32_t i = 0; i < cParmsSrc; i++)
    {
        paParmsDst[i].type = paParmsSrc[i].type;
        switch (paParmsSrc[i].type)
        {
            case VBOX_HGCM_SVC_PARM_32BIT:
            {
                paParmsDst[i].u.uint32 = paParmsSrc[i].u.uint32;
                break;
            }
            case VBOX_HGCM_SVC_PARM_64BIT:
            {
                paParmsDst[i].u.uint64 = paParmsSrc[i].u.uint64;
                break;
            }
            case VBOX_HGCM_SVC_PARM_PTR:
            {
                /* Do we have to perform a deep copy? */
                if (fDeepCopy)
                {
                    /* Yes, do so. */
                    paParmsDst[i].u.pointer.size = paParmsSrc[i].u.pointer.size;
                    if (paParmsDst[i].u.pointer.size > 0)
                    {
                        paParmsDst[i].u.pointer.addr = RTMemAlloc(paParmsDst[i].u.pointer.size);
                        if (!paParmsDst[i].u.pointer.addr)
                        {
                            rc = VERR_NO_MEMORY;
                            break;
                        }
                    }
                }
                else
                {
                    /* No, but we have to check if there is enough room. */
                    if (paParmsDst[i].u.pointer.size < paParmsSrc[i].u.pointer.size)
                    {
                        rc = VERR_BUFFER_OVERFLOW;
                        break;
                    }
                }

                if (paParmsSrc[i].u.pointer.size)
                {
                    if (   paParmsDst[i].u.pointer.addr
                        && paParmsDst[i].u.pointer.size)
                    {
                        memcpy(paParmsDst[i].u.pointer.addr,
                               paParmsSrc[i].u.pointer.addr,
                               RT_MIN(paParmsDst[i].u.pointer.size, paParmsSrc[i].u.pointer.size));
                    }
                    else
                        rc = VERR_INVALID_POINTER;
                }
                break;
            }
            default:
            {
                AssertMsgFailed(("Unknown HGCM type %u\n", paParmsSrc[i].type));
                rc = VERR_INVALID_PARAMETER;
                break;
            }
        }
        if (RT_FAILURE(rc))
            break;
    }
    return rc;
}

/**
 * Initializes the message with a message type and parameters.
 *
 * @returns IPRT status code.
 * @param   uMsg            Message type to set.
 * @param   cParms          Number of parameters to set.
 * @param   aParms          Array of parameters to set.
 */
int Message::initData(uint32_t uMsg, uint32_t cParms, VBOXHGCMSVCPARM aParms[])
{
    AssertReturn(cParms < 256, VERR_INVALID_PARAMETER);
    AssertPtrNullReturn(aParms, VERR_INVALID_PARAMETER);

    /* Cleanup any eventual old stuff. */
    reset();

    m_uMsg   = uMsg;
    m_cParms = cParms;

    int rc = VINF_SUCCESS;

    if (cParms)
    {
        m_paParms = (VBOXHGCMSVCPARM*)RTMemAllocZ(sizeof(VBOXHGCMSVCPARM) * m_cParms);
        if (m_paParms)
        {
            rc = Message::CopyParms(m_paParms, m_cParms, &aParms[0], cParms, true /* fDeepCopy */);
            if (RT_FAILURE(rc))
                reset();
        }
        else
            rc = VERR_NO_MEMORY;
    }

    return rc;
}

