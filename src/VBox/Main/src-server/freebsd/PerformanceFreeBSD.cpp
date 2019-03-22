/* $Id$ */
/** @file
 * VirtualBox Performance Collector, FreeBSD Specialization.
 */

/*
 * Copyright (C) 2008-2019 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#include <sys/types.h>
#include <sys/sysctl.h>
#include "Performance.h"

namespace pm {

class CollectorFreeBSD : public CollectorHAL
{
public:
    virtual int getHostCpuLoad(ULONG *user, ULONG *kernel, ULONG *idle);
    virtual int getHostCpuMHz(ULONG *mhz);
    virtual int getHostMemoryUsage(ULONG *total, ULONG *used, ULONG *available);
    virtual int getProcessCpuLoad(RTPROCESS process, ULONG *user, ULONG *kernel);
    virtual int getProcessMemoryUsage(RTPROCESS process, ULONG *used);
};


CollectorHAL *createHAL()
{
    return new CollectorFreeBSD();
}

int CollectorFreeBSD::getHostCpuLoad(ULONG *user, ULONG *kernel, ULONG *idle)
{
    return VERR_NOT_IMPLEMENTED;
}

int CollectorFreeBSD::getHostCpuMHz(ULONG *mhz)
{
    int CpuMHz = 0;
    size_t cbParameter = sizeof(CpuMHz);

    /** @todo Howto support more than one CPU? */
    if (sysctlbyname("dev.cpu.0.freq", &CpuMHz, &cbParameter, NULL, 0))
        return VERR_NOT_SUPPORTED;

    *mhz = CpuMHz;

    return VINF_SUCCESS;
}

int CollectorFreeBSD::getHostMemoryUsage(ULONG *total, ULONG *used, ULONG *available)
{
    int rc = VINF_SUCCESS;
    u_long cbMemPhys = 0;
    u_int cPagesMemFree = 0;
    u_int cPagesMemInactive = 0;
    u_int cPagesMemCached = 0;
    u_int cPagesMemUsed = 0;
    int cbPage = 0;
    size_t cbParameter = sizeof(cbMemPhys);
    int cProcessed = 0;

    if (!sysctlbyname("hw.physmem", &cbMemPhys, &cbParameter, NULL, 0))
        cProcessed++;

    cbParameter = sizeof(cPagesMemFree);
    if (!sysctlbyname("vm.stats.vm.v_free_count", &cPagesMemFree, &cbParameter, NULL, 0))
        cProcessed++;
    cbParameter = sizeof(cPagesMemUsed);
    if (!sysctlbyname("vm.stats.vm.v_active_count", &cPagesMemUsed, &cbParameter, NULL, 0))
        cProcessed++;
    cbParameter = sizeof(cPagesMemInactive);
    if (!sysctlbyname("vm.stats.vm.v_inactive_count", &cPagesMemInactive, &cbParameter, NULL, 0))
        cProcessed++;
    cbParameter = sizeof(cPagesMemCached);
    if (!sysctlbyname("vm.stats.vm.v_cache_count", &cPagesMemCached, &cbParameter, NULL, 0))
        cProcessed++;
    cbParameter = sizeof(cbPage);
    if (!sysctlbyname("hw.pagesize", &cbPage, &cbParameter, NULL, 0))
        cProcessed++;

    if (cProcessed == 6)
    {
        *total     = cbMemPhys / _1K;
        *used      = cPagesMemUsed * (cbPage / _1K);
        *available = (cPagesMemFree + cPagesMemInactive + cPagesMemCached ) * (cbPage / _1K);
    }
    else
        rc = VERR_NOT_SUPPORTED;

    return rc;
}

int CollectorFreeBSD::getProcessCpuLoad(RTPROCESS process, ULONG *user, ULONG *kernel)
{
    return VERR_NOT_IMPLEMENTED;
}

int CollectorFreeBSD::getProcessMemoryUsage(RTPROCESS process, ULONG *used)
{
    return VERR_NOT_IMPLEMENTED;
}

int getDiskListByFs(const char *name, DiskList& list)
{
    return VERR_NOT_IMPLEMENTED;
}

} /* namespace pm */

