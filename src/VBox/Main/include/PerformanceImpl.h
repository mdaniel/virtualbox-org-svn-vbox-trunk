/* $Id$ */

/** @file
 *
 * VBox Performance COM class implementation.
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

#ifndef ____H_PERFORMANCEIMPL
#define ____H_PERFORMANCEIMPL
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include "PerformanceCollectorWrap.h"
#include "PerformanceMetricWrap.h"

#include <VBox/com/com.h>
#include <VBox/com/array.h>
//#ifdef VBOX_WITH_RESOURCE_USAGE_API
#include <iprt/timer.h>
//#endif /* VBOX_WITH_RESOURCE_USAGE_API */

#include <list>

namespace pm
{
    class Metric;
    class BaseMetric;
    class CollectorHAL;
    class CollectorGuest;
    class CollectorGuestManager;
}

#undef min
#undef max

/* Each second we obtain new CPU load stats. */
#define VBOX_USAGE_SAMPLER_MIN_INTERVAL 1000

class ATL_NO_VTABLE PerformanceMetric :
    public PerformanceMetricWrap
{
public:

    DECLARE_EMPTY_CTOR_DTOR(PerformanceMetric)

    HRESULT FinalConstruct();
    void FinalRelease();

    // public initializer/uninitializer for internal purposes only
    HRESULT init(pm::Metric *aMetric);
    HRESULT init(pm::BaseMetric *aMetric);
    void uninit();

private:

    // wrapped IPerformanceMetric properties
    HRESULT getMetricName(com::Utf8Str &aMetricName);
    HRESULT getObject(ComPtr<IUnknown> &aObject);
    HRESULT getDescription(com::Utf8Str &aDescription);
    HRESULT getPeriod(ULONG *aPeriod);
    HRESULT getCount(ULONG *aCount);
    HRESULT getUnit(com::Utf8Str &aUnit);
    HRESULT getMinimumValue(LONG *aMinimumValue);
    HRESULT getMaximumValue(LONG *aMaximumValue);

    struct Data
    {
        /* Constructor. */
        Data()
            : period(0), count(0), min(0), max(0)
        {
        }

        Utf8Str          name;
        ComPtr<IUnknown> object;
        Utf8Str          description;
        ULONG            period;
        ULONG            count;
        Utf8Str          unit;
        LONG             min;
        LONG             max;
    };

    Data m;
};


class ATL_NO_VTABLE PerformanceCollector :
    public PerformanceCollectorWrap
{
public:

    DECLARE_EMPTY_CTOR_DTOR(PerformanceCollector)

    HRESULT FinalConstruct();
    void FinalRelease();

    // public initializers/uninitializers only for internal purposes
    HRESULT init();
    void uninit();

    // public methods only for internal purposes

    void registerBaseMetric(pm::BaseMetric *baseMetric);
    void registerMetric(pm::Metric *metric);
    void unregisterBaseMetricsFor(const ComPtr<IUnknown> &object, const Utf8Str name = "*");
    void unregisterMetricsFor(const ComPtr<IUnknown> &object, const Utf8Str name = "*");
    void registerGuest(pm::CollectorGuest* pGuest);
    void unregisterGuest(pm::CollectorGuest* pGuest);

    void suspendSampling();
    void resumeSampling();

    // public methods for internal purposes only
    // (ensure there is a caller and a read lock before calling them!)

    pm::CollectorHAL          *getHAL()          { return m.hal; };
    pm::CollectorGuestManager *getGuestManager() { return m.gm; };

private:

    // wrapped IPerformanceCollector properties
    HRESULT getMetricNames(std::vector<com::Utf8Str> &aMetricNames);

    // wrapped IPerformanceCollector methods
    HRESULT getMetrics(const std::vector<com::Utf8Str> &aMetricNames,
                       const std::vector<ComPtr<IUnknown> > &aObjects,
                       std::vector<ComPtr<IPerformanceMetric> > &aMetrics);
    HRESULT setupMetrics(const std::vector<com::Utf8Str> &aMetricNames,
                         const std::vector<ComPtr<IUnknown> > &aObjects,
                         ULONG aPeriod,
                         ULONG aCount,
                         std::vector<ComPtr<IPerformanceMetric> > &aAffectedMetrics);
    HRESULT enableMetrics(const std::vector<com::Utf8Str> &aMetricNames,
                          const std::vector<ComPtr<IUnknown> > &aObjects,
                          std::vector<ComPtr<IPerformanceMetric> > &aAffectedMetrics);
    HRESULT disableMetrics(const std::vector<com::Utf8Str> &aMetricNames,
                           const std::vector<ComPtr<IUnknown> > &aObjects,
                           std::vector<ComPtr<IPerformanceMetric> > &aAffectedMetrics);
    HRESULT queryMetricsData(const std::vector<com::Utf8Str> &aMetricNames,
                             const std::vector<ComPtr<IUnknown> > &aObjects,
                             std::vector<com::Utf8Str> &aReturnMetricNames,
                             std::vector<ComPtr<IUnknown> > &aReturnObjects,
                             std::vector<com::Utf8Str> &aReturnUnits,
                             std::vector<ULONG> &aReturnScales,
                             std::vector<ULONG> &aReturnSequenceNumbers,
                             std::vector<ULONG> &aReturnDataIndices,
                             std::vector<ULONG> &aReturnDataLengths,
                             std::vector<LONG> &aReturnData);


    HRESULT toIPerformanceMetric(pm::Metric *src, ComPtr<IPerformanceMetric> &dst);
    HRESULT toIPerformanceMetric(pm::BaseMetric *src, ComPtr<IPerformanceMetric> &dst);

    static DECLCALLBACK(void) staticSamplerCallback(RTTIMERLR hTimerLR, void *pvUser, uint64_t iTick);
    void samplerCallback(uint64_t iTick);

    const Utf8Str& getFailedGuestName();

    typedef std::list<pm::Metric*> MetricList;
    typedef std::list<pm::BaseMetric*> BaseMetricList;

/** PerformanceMetric::mMagic value. */
#define PERFORMANCE_METRIC_MAGIC    UINT32_C(0xABBA1972)
    uint32_t mMagic;
    const Utf8Str mUnknownGuest;

    struct Data
    {
        Data() : hal(0) {};

        BaseMetricList             baseMetrics;
        MetricList                 metrics;
        RTTIMERLR                  sampler;
        pm::CollectorHAL          *hal;
        pm::CollectorGuestManager *gm;
    };

    Data m;
};

#endif //!____H_PERFORMANCEIMPL
/* vi: set tabstop=4 shiftwidth=4 expandtab: */
