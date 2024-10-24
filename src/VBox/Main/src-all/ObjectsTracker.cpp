/* $Id$ */
/** @file
 * VirtualBox Object tracker implementation
 */

/*
 * Copyright (C) 2006-2024 Oracle and/or its affiliates.
 *
 * This file is part of VirtualBox base platform packages, as
 * available from https://www.virtualbox.org.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, in version 3 of the
 * License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <https://www.gnu.org/licenses>.
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#define LOG_GROUP LOG_GROUP_MAIN

#include "LoggingNew.h"
#include "VirtualBoxBase.h"
#include "ObjectsTracker.h"
#include <iprt/log.h>
#include <iprt/stream.h>
#include <iprt/time.h>
# include <iprt/asm.h>

typedef std::unordered_map<std::string, TrackedObjectData>::const_iterator cIterTrObjDataType;
typedef std::unordered_map<std::string, TrackedObjectData>::iterator iterTrObjDataType;

/////////////////////////////////////////////////////////////////////////////
// TrackedObjectData
/////////////////////////////////////////////////////////////////////////////
TrackedObjectData::TrackedObjectData():
    m_componentName("noname"),
    m_lifeTime(0),
    m_idleTime(1),
    m_State(TrackedObjectData::Invalid),
    m_pIface(NULL)
    {
}

TrackedObjectData::TrackedObjectData(const com::Guid &aObjId,
                                     const com::Guid &aClassIID,
                                     uint64_t aLifeTime,
                                     uint64_t aIdleTime,
                                     IUnknown* aPtr):

    m_objId(aObjId),
    m_classIID(aClassIID),
    m_componentName("noname"),
    m_lifeTime(aLifeTime),
    m_idleTime(aIdleTime),
    m_fIdleTimeStart(false),
    m_pIface(aPtr)
{
    RTTimeNow(unconst(&m_creationTime));
    m_lastAccessTime = m_creationTime;
    m_State = TrackedObjectData::Valid;
    Log2(("%s constructor \n", __FUNCTION__));
}

TrackedObjectData::~TrackedObjectData()
{
    Log2(("%s destructor\n", __FUNCTION__));
    Log2(("DELETED Object %s (class IID %s)\n",
          m_objId.toString().c_str(),
          m_classIID.toString().c_str()
          ));
}

TrackedObjectData &TrackedObjectData::operator=(const TrackedObjectData & that)
{
    LogFlowFuncEnter();
    if (this != &that)
    {
        m_objId = that.m_objId;
        m_classIID = that.m_classIID;
        m_componentName = that.m_componentName;
        m_lifeTime = that.m_lifeTime;
        m_idleTime = that.m_idleTime;
        m_pIface = that.m_pIface;
        m_creationTime = that.m_creationTime;
        m_lastAccessTime = that.m_lastAccessTime;
        m_idleTimeStart = that.m_idleTimeStart;
        m_fIdleTimeStart = that.m_fIdleTimeStart;
        m_State = that.m_State;
    }

    return *this;
}

com::Utf8Str TrackedObjectData::updateLastAccessTime()
{
    RTTimeNow(unconst(&m_lastAccessTime));

    char szTime[42];
    RTTimeSpecToString(&m_lastAccessTime, szTime, sizeof(szTime));
    return com::Utf8Str(szTime);
}

com::Utf8Str TrackedObjectData::initIdleTime()
{
    if (!m_fIdleTimeStart)
    {
        RTTimeNow(unconst(&m_idleTimeStart));
        m_fIdleTimeStart = true;
    }

    char szTime[42];
    RTTimeSpecToString(&m_idleTimeStart, szTime, sizeof(szTime));
    return com::Utf8Str(szTime);
}

com::Utf8Str TrackedObjectData::creationTimeStr() const
{
    char szCreationTime[42];
    RTTimeSpecToString(&m_creationTime, szCreationTime, sizeof(szCreationTime));

    return com::Utf8Str(szCreationTime);
}

/* No locking here, be aware */
unsigned long TrackedObjectData::i_checkRefCount(const Guid& aIID)
{
    ULONG cRefs = 0;
    if (aIID == m_classIID)
    {
        m_pIface->AddRef();
        cRefs = m_pIface->Release();

        Log2(("**** Object %s (class IID %s) (refcount %lu) ****\n",
               m_objId.toString().c_str(),
               m_classIID.toString().c_str(),
               cRefs
               ));
    }
    return cRefs;
}

/////////////////////////////////////////////////////////////////////////////
// TrackedObjectsCollector
/////////////////////////////////////////////////////////////////////////////
class TrackedObjectsCollector;
extern TrackedObjectsCollector gTrackedObjectsCollector;

TrackedObjectsCollector::TrackedObjectsCollector(): m_fInitialized(false)
{
    Log2(("%s constructor \n", __FUNCTION__));
}

TrackedObjectsCollector::~TrackedObjectsCollector()
{
    Log2(("%s destructor \n", __FUNCTION__));

    int vrc = i_checkInitialization();

    /*
     * Someone forgot to call uninit()
     */
    if (RT_SUCCESS(vrc))
    {
        if (m_trackedObjectsData.size() != 0)
        {
            if (m_trackedObjectsData.size() > 1)
                LogRel(("%u objects are still presented in the collector\n", m_trackedObjectsData.size()));
            else
                LogRel(("%u object is still presented in the collector\n", m_trackedObjectsData.size()));

            m_Released += m_trackedObjectsData.size();
            m_trackedObjectsData.clear();
        }

        LogRel(("The Objects Collector history data: added objects %u, released objects %u\n", m_Added, m_Released));

        /* Try to delete m_CritSectData */
        RTCritSectDelete(&m_CritSectData);
    }
}

bool TrackedObjectsCollector::init(){
    m_fInitialized = false;

    /* Create the critical section protecting the m_trackedObjectsData */
    int vrc = RTCritSectInit(&m_CritSectData);

    if (RT_SUCCESS(vrc))
    {
        /*
         * TrackedObjectsCollector initialization occurs only when new instance of VirtualBox is created.
         * At this moment nobody uses the TrackedObjectsCollector and we can call the functions without any locking.
         */
        vrc = i_clear();//just in case
        if (RT_SUCCESS(vrc))
        {
            m_fInitialized = true;
            LogRel(("The collector has been initialized.\n"));
        }
    }

    return m_fInitialized;
}

bool TrackedObjectsCollector::uninit()
{
    clear(Uninitialization);


    /* Deletion the critical section protecting the m_trackedObjectsData */
    int vrc = RTCritSectDelete(&m_CritSectData);

    if (RT_SUCCESS(vrc))
        m_fInitialized = false;

    return m_fInitialized;
}

int TrackedObjectsCollector::i_checkInitialization() const
{
    int vrc = VINF_SUCCESS;
    if (!m_fInitialized)
    {
        Log2(("The collector is in the uninitialized state...\n"));
        vrc = VERR_INVALID_STATE;
    }

    return vrc;
}

HRESULT TrackedObjectsCollector::setObj (const com::Utf8Str &aObjId,
                                         const com::Utf8Str &aClassIID,
                                         uint64_t lifeTime,
                                         uint64_t idleTime,
                                         IUnknown* ptrIface)
{
    LogFlowFuncEnter();

    HRESULT hrc = S_OK;
    int vrc = i_checkInitialization();
    if (RT_FAILURE(vrc))
        return VBOX_E_INVALID_OBJECT_STATE;

    /* Enter critical section here */
    RTCritSectEnter(&m_CritSectData);

    com::Guid idObj(aObjId);
    com::Guid classIID(aClassIID);
    std::pair < std::set<com::Utf8Str>::iterator, bool > opRes = m_trackedObjectIds.insert(aObjId);

    /*
     * The case for updating the tracked object data.
     * The Id is presented in the m_trackedObjectIds. The original object is removed from m_trackedObjectsData.
     */
    if (!opRes.second)
    {
        Log2(("UPDATING TrackedObjectData: object Id %s, class IID %s, life time %i, idle time %i\n",
                aObjId.c_str(), aClassIID.c_str(), lifeTime, idleTime));

        m_trackedObjectsData.erase(aObjId.c_str());
        /* decrease the counter */
        --m_Added;
    }
    else
    {
        char szCreationTime[42];
        RTTIMESPEC time;
        RTTimeSpecToString(RTTimeNow(&time), szCreationTime, sizeof(szCreationTime));
        Log2(("ADDED TrackedObjectData: creation time %s, object Id %s, class IID %s\n",
               szCreationTime, aObjId.c_str(), aClassIID.c_str()));
    }

    /* Data is stored in the m_trackedObjectsData under the passed Id. */
    m_trackedObjectsData.insert(std::make_pair(aObjId.c_str(),
                                               TrackedObjectData(idObj,
                                                                 classIID,
                                                                 lifeTime,
                                                                 idleTime,
                                                                 ptrIface)));

    /* increase the counter */
    ++m_Added;

    /* Leave critical section here */
    RTCritSectLeave(&m_CritSectData);

    return hrc;
}

HRESULT TrackedObjectsCollector::getObj (const com::Utf8Str& aObjId,
                                         TrackedObjectData& aObjData,
                                         bool fUpdate)
{
    LogFlowFuncEnter();

    int vrc = i_checkInitialization();
    if (RT_FAILURE(vrc))
        return VBOX_E_INVALID_OBJECT_STATE;

    /* Enter critical section here */
    RTCritSectEnter(&m_CritSectData);

    std::string sTemp(aObjId.c_str());
    HRESULT hrc = E_FAIL;

    iterTrObjDataType pIter = m_trackedObjectsData.find(sTemp);
    if (pIter != m_trackedObjectsData.end() && fUpdate == true)
    {
        /* Update some fields in the found object if needed. in instance, the last access time */
        com::Utf8Str lat = pIter->second.updateLastAccessTime(); /* Update the access time */
        Log2(("The updated last access time is %s\n", lat.c_str()));
        vrc = VINF_SUCCESS;
    }

    if (RT_SUCCESS(vrc))
    {
        if ( i_getObj(aObjId).getInterface().isNotNull() )
        {
            /* Excessive check because user may get only the valid objects Ids. But for 200% assurance it's here */
            if (i_getObj(aObjId).state() == TrackedObjectData::Valid)
            {
                aObjData = i_getObj(aObjId);
                hrc = S_OK;
            }
            else
                hrc = VBOX_E_INVALID_OBJECT_STATE;
        }
        else
            hrc = VBOX_E_OBJECT_NOT_FOUND;
    }

    /* Leave critical section here */
    RTCritSectLeave(&m_CritSectData);

    return hrc;
}

const TrackedObjectData& TrackedObjectsCollector::i_getObj (const com::Utf8Str& aObjId) const
{
    /* No check for existence of aObjId */
    return m_trackedObjectsData.at(aObjId.c_str());
}

HRESULT TrackedObjectsCollector::initObjIdleTime (const com::Utf8Str& aObjId)
{
    LogFlowFuncEnter();

    HRESULT hrc = S_OK;
    int vrc = i_checkInitialization();
    if (RT_FAILURE(vrc))
        return VBOX_E_INVALID_OBJECT_STATE;

    vrc = VERR_NOT_FOUND;
    /* Enter critical section here */
    RTCritSectEnter(&m_CritSectData);

    std::string sTemp(aObjId.c_str());

    iterTrObjDataType pIter = m_trackedObjectsData.find(sTemp);
    if (pIter != m_trackedObjectsData.end())
    {
        /* Init idle time only once, next time returns the initialization time */
        com::Utf8Str strTime = pIter->second.initIdleTime();
        Log2(("The idle time start is %s\n", strTime.c_str()));
        vrc = VINF_SUCCESS;
    }

    if (RT_FAILURE(vrc))
        hrc = VBOX_E_OBJECT_NOT_FOUND;

    /* Leave critical section here */
    RTCritSectLeave(&m_CritSectData);

    return hrc;
}

HRESULT TrackedObjectsCollector::removeObj (const com::Utf8Str& aObjId)
{
    Log2(("%s: object Id %s\n", __FUNCTION__, aObjId.c_str()));

    HRESULT hrc = S_OK;
    int vrc = i_checkInitialization();
    if (RT_FAILURE(vrc))
        return VBOX_E_INVALID_OBJECT_STATE;

    std::string sTemp(aObjId.c_str());

    vrc = VERR_NOT_FOUND;

    /* Enter critical section here */
    RTCritSectEnter(&m_CritSectData);

    cIterTrObjDataType pIter = m_trackedObjectsData.find(sTemp);
    if (pIter != m_trackedObjectsData.end())
    {
        Log2(("RELEASED TrackedObjectData: creation time %s, object Id %s, class IID %s\n",
               pIter->second.creationTimeStr().c_str(), pIter->second.objectIdStr().c_str(), pIter->second.classIIDStr().c_str()));

        m_trackedObjectsData.erase(pIter);
        m_trackedObjectIds.erase(aObjId);
        m_trackedInvalidObjectIds.erase(aObjId);

        /* increase the counter */
        ++m_Released;

        vrc = VINF_SUCCESS;
    }

    if (RT_FAILURE(vrc))
        hrc = VBOX_E_OBJECT_NOT_FOUND;

    /* Leave critical section here */
    RTCritSectLeave(&m_CritSectData);

    return hrc;
}

HRESULT TrackedObjectsCollector::getAllObjIds (std::vector<com::Utf8Str>& aObjIdMap)
{
    HRESULT hrc = S_OK;
    int vrc = i_checkInitialization();
    if (RT_FAILURE(vrc))
        return VBOX_E_INVALID_OBJECT_STATE;

    /* Enter critical section here */
    RTCritSectEnter(&m_CritSectData);
    vrc = i_getAllObjIds(aObjIdMap);
    if (RT_FAILURE(vrc))
        hrc = VBOX_E_OBJECT_NOT_FOUND;
    /* Leave critical section here */
    RTCritSectLeave(&m_CritSectData);

    return hrc;
}

int TrackedObjectsCollector::i_getAllObjIds (std::vector<com::Utf8Str>& aObjIdMap) const
{
    for (const com::Utf8Str& item : m_trackedObjectIds)
    {
        if (!m_trackedInvalidObjectIds.count(item))
            aObjIdMap.push_back(item);
    }

    return aObjIdMap.size() > 0 ? VINF_SUCCESS : VERR_NOT_FOUND;
}

HRESULT TrackedObjectsCollector::getObjIdsByClassIID (const Guid& iid,
                                                      std::vector<com::Utf8Str>& aObjIdMap)
{
    Log2(("%s: Getting all objects Ids with Class IID %s\n", __FUNCTION__, iid.toString().c_str()));

    HRESULT hrc = S_OK;
    int vrc = i_checkInitialization();
    if (RT_FAILURE(vrc))
        return VBOX_E_INVALID_OBJECT_STATE;

    /* Enter critical section here */
    RTCritSectEnter(&m_CritSectData);

    vrc = i_getObjIdsByClassIID (iid, aObjIdMap);
    if (RT_FAILURE(vrc))
        hrc = VBOX_E_OBJECT_NOT_FOUND;
    /* Leave critical section here */
    RTCritSectLeave(&m_CritSectData);

    return hrc;
}

int TrackedObjectsCollector::i_getObjIdsByClassIID (const Guid& iid,
                                                        std::vector<com::Utf8Str>& aObjIdMap) const
{
    for (const std::pair<const std::string, TrackedObjectData>& item : m_trackedObjectsData)
    {
        /* IID found and the object is valid */
        if (item.second.classIID() == iid && !m_trackedInvalidObjectIds.count(item.first.c_str()))
            aObjIdMap.push_back(item.first.c_str());
    }

    return aObjIdMap.size() > 0 ? VINF_SUCCESS : VERR_NOT_FOUND;
}

bool TrackedObjectsCollector::checkObj(const com::Utf8Str& aObjId)
{
    Log2(("%s: Checking object with Id %s\n", __FUNCTION__, aObjId.c_str()));

    int vrc = i_checkInitialization();
    if (RT_FAILURE(vrc))
        return false;

    RTCritSectEnter(&m_CritSectData);
    bool res = i_checkObj(aObjId);
    RTCritSectLeave(&m_CritSectData);
    return res;
}

bool TrackedObjectsCollector::i_checkObj(const com::Utf8Str& aObjId) const
{
    return m_trackedObjectIds.count(aObjId.c_str()) > 0 ? true : false;
}

HRESULT TrackedObjectsCollector::clear(TrackedObjectsCollectorState aState)
{
    LogFlowFuncEnter();

    HRESULT hrc = S_OK;
    int vrc = i_checkInitialization();
    if (RT_FAILURE(vrc))
        return VBOX_E_INVALID_OBJECT_STATE;

    if (aState != Uninitialization)
    {
        /* Enter critical section here */
        vrc = RTCritSectEnter(&m_CritSectData);
        if (RT_FAILURE(vrc))
        {
            Log2(("%s: Coudn't enter into the critical section (%Rrc)\n", __FUNCTION__, vrc));
            return E_ABORT;
        }
    }

    if (m_trackedObjectsData.size() != 0)
    {
        if (m_trackedObjectsData.size() > 1)
            Log2(("%u objects are still presented in the Objects Collector, clearing...\n", m_trackedObjectsData.size()));
        else
            Log2(("%u object is still presented in the Objects Collector, clearing...\n", m_trackedObjectsData.size()));

        vrc = i_clear();
        /* Ignore setting hrc */
        if (RT_FAILURE(vrc))
            LogRel(("Something wrong with clearing the Objects Collector\n"));
    }

    Log2(("The Objects Collector history data: added objects %u, released objects %u\n", m_Added, m_Released));

    if (aState != Uninitialization)
    {
        /* Leave critical section here */
        RTCritSectLeave(&m_CritSectData);
    }

    return hrc;
}

int TrackedObjectsCollector::i_clear()
{
    int vrc = VINF_SUCCESS;
    try
    {
        m_Released += m_trackedObjectsData.size();
        m_trackedObjectsData.clear();
        m_trackedObjectIds.clear();
        m_trackedInvalidObjectIds.clear();
    }
    catch (...)
    {
        vrc = VERR_GENERAL_FAILURE;
    }
    return vrc;
}

HRESULT TrackedObjectsCollector::tryToRemoveObj(const com::Utf8Str& aObjId)
{
    LogFlowFuncEnter();

    HRESULT hrc = S_OK;
    int vrc = i_checkInitialization();
    if (RT_FAILURE(vrc))
        return VBOX_E_INVALID_OBJECT_STATE;

    /* Enter critical section here */
    RTCritSectEnter(&m_CritSectData);

    iterTrObjDataType pIter = m_trackedObjectsData.find(aObjId.c_str());
    if (pIter != m_trackedObjectsData.end())
    {
        pIter->second.getInterface()->AddRef();
        ULONG cRefs = pIter->second.getInterface()->Release();

        if (cRefs > 1)
        {
            Log2(("Object %s with class IID %s can't be released if refcount is more than 1 (now %lu)\n",
                    pIter->second.objectIdStr().c_str(),
                    pIter->second.classIIDStr().c_str(),
                    cRefs
                    ));
            hrc = E_FAIL;
        }
        else
        {
            Log2(("Object %s with class IID %s is released (refcount %lu)\n",
                    pIter->second.objectIdStr().c_str(),
                    pIter->second.classIIDStr().c_str(),
                    cRefs
                    ));
            m_trackedObjectsData.erase(pIter);
            m_trackedObjectIds.erase(aObjId);
            m_trackedInvalidObjectIds.erase(aObjId);

            /* increase the counter */
            ++m_Released;
        }
    }
    else
        hrc = VBOX_E_OBJECT_NOT_FOUND;

    /* Leave critical section here */
    RTCritSectLeave(&m_CritSectData);

    return hrc;
}

/**
 * Invalidate the tracked object.
 * Works ONLY in conjunction with setTracked()!
 */
HRESULT TrackedObjectsCollector::invalidateObj(const com::Utf8Str &aObjId)
{
    LogFlowFuncEnter();

    HRESULT hrc = S_OK;
    int vrc = i_checkInitialization();
    if (RT_FAILURE(vrc))
        return VBOX_E_INVALID_OBJECT_STATE;

    /* Enter critical section here */
    RTCritSectEnter(&m_CritSectData);

    std::string sTemp(aObjId.c_str());
    vrc = VERR_NOT_FOUND;

    iterTrObjDataType pIter = m_trackedObjectsData.find(sTemp);
    if (pIter != m_trackedObjectsData.end())
    {
        pIter->second.resetState();
        m_trackedInvalidObjectIds.insert(aObjId);
        vrc = VINF_SUCCESS;
    }

    if (RT_FAILURE(vrc))
        hrc = VBOX_E_OBJECT_NOT_FOUND;

    /* Leave critical section here */
    RTCritSectLeave(&m_CritSectData);

    return hrc;
}

/////////////////////////////////////////////////////////////////////////////
// ObjectTracker
/////////////////////////////////////////////////////////////////////////////
ObjectTracker::~ObjectTracker()
{
    LogFlowFuncEnter();
    LogRel(("Start waiting the ObjectTracker thread termination\n"));
    RTThreadWait(m_Thread, 30000, NULL);
    LogRel(("Finished waiting the ObjectTracker thread termination\n"));
    m_Thread = NIL_RTTHREAD;
}

bool ObjectTracker::init()
{
    LogFlowFuncEnter();
    return true;
}

bool ObjectTracker::finish()
{
    LogFlowFuncEnter();
    ASMAtomicWriteBool(&fFinish, true);
    return fFinish;
}

bool ObjectTracker::isFinished()
{
    LogFlowFuncEnter();
    ASMAtomicReadBool(&fFinish);
    return fFinish;
}

/*static*/
DECLCALLBACK(int) ObjectTracker::objectTrackerTask(RTTHREAD ThreadSelf, void *pvUser)
{
    HRESULT hrc = S_OK;

    NOREF(ThreadSelf);
    ObjectTracker* const master = (ObjectTracker*)pvUser;

    LogRel(("Starting the ObjectTracker thread %s\n", master->getTaskName().c_str()));

    while (master != NULL && master->isFinished() != true)
    {
        std::vector<com::Utf8Str> lObjIdMap;

        hrc = gTrackedObjectsCollector.getAllObjIds(lObjIdMap);
        for (const com::Utf8Str& item : lObjIdMap)
        {
            TrackedObjectData temp;
            if(gTrackedObjectsCollector.checkObj(item.c_str()))
            {
                hrc = gTrackedObjectsCollector.getObj(item.c_str(), temp);
                if (SUCCEEDED(hrc))
                {
                    Log2(("Tracked Object with ID %s was found:\n", temp.m_objId.toString().c_str()));

                    RTTIMESPEC now;
                    int64_t currTime = RTTimeSpecGetMilli(RTTimeNow(&now));
                    int64_t creationTime = RTTimeSpecGetMilli(&temp.m_creationTime);
                    int64_t lifeTime = temp.m_lifeTime*1000; //convert to milliseconds

                    int64_t remainingLifeTime = ((creationTime + lifeTime) - currTime)/1000;

                    /* lock? */
                    temp.m_pIface->AddRef();
                    unsigned long cRefs = temp.m_pIface->Release();

                    /* cRefs > 2 because we created the temporarily object temp */
                    Log2(("Object %s (class IID %s): refcount %lu, remaining life time is %ld sec\n",
                          temp.m_objId.toString().c_str(),
                          temp.m_classIID.toString().c_str(),
                          cRefs - 1,
                          remainingLifeTime
                          ));

                    bool fLifeTimeEnd = (currTime - creationTime) > lifeTime ? true : false;

                    if (!fLifeTimeEnd)
                    {
                        if (cRefs <= 2 || remainingLifeTime <= 0)
                        {
                            if (temp.m_fIdleTimeStart == false)
                            {
                                gTrackedObjectsCollector.initObjIdleTime(item);
                                Log2(("Idle time for the object with Id %s has been started\n", item.c_str()));
                            }
                            else
                            {
                                int64_t idleTime = temp.m_idleTime*1000; //convert to milliseconds
                                int64_t idleTimeStart = RTTimeSpecGetMilli(&temp.m_idleTimeStart);
                                bool fObsolete = (currTime - idleTimeStart) > idleTime ? true : false;
                                if (fObsolete)
                                {
                                    Log2(("Object with Id %s removed from Object Collector "
                                            "(recount is %u, idle time exceeded %u sec)\n", item.c_str(), cRefs - 1, temp.m_idleTime));
                                    gTrackedObjectsCollector.removeObj(item.c_str());
                                }
                            }
                        }
                    }
                    else
                    {
                        if (cRefs <= 2)
                        {
                            /*
                             * Special case for the objects with lifeTime == 0.
                             * It's intended for such objects like Mediums or Machines or others.
                             * The objects which live from the beginning but may be deleted by user manually.
                             * for this object the idle time starts when user deletes it.
                             */
                            if (lifeTime == 0)
                            {
                                lifeTime = currTime - creationTime;//in milliseconds
                                /* if lifeTime < 1000 msec (1 sec) set minimal lifeTime to 60 sec (1 min)*/
                                lifeTime = lifeTime < 1000 ? 60 : lifeTime/1000;
                                /* Updating the object data */
                                gTrackedObjectsCollector.setObj(temp.objectIdStr(),
                                                                temp.classIIDStr(),
                                                                lifeTime,
                                                                temp.m_idleTime,
                                                                temp.m_pIface);
                            }
                            else
                            {
                                Log2(("Object with Id %s removed from Object Collector "
                                        "(lifetime exceeded %u sec)\n", item.c_str(), temp.m_lifeTime));
                                gTrackedObjectsCollector.removeObj(item.c_str());
                            }
                        }
                    }
                }
            }
            else
                Log2(("Tracked Object with ID %s was not found\n", item.c_str()));
        }

        //sleep 1 sec
        RTThreadSleep(RT_MS_1SEC);//1 sec
    }

    LogRel(("Finishing the object tracker thread %s\n", master->getTaskName().c_str()));

    return 0;
}

int ObjectTracker::createThread()
{
    int vrc = RTThreadCreate(&m_Thread, objectTrackerTask, this, 0,
                             RTTHREADTYPE_INFREQUENT_POLLER,
                             RTTHREADFLAGS_WAITABLE,
                             "ObjTracker");

    return vrc;
}

