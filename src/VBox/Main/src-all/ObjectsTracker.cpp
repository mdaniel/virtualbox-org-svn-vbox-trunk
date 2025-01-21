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

/*
 * Sometimes user wants to check or retrieve data or information about objects that may not exist at the time
 * the user requests such objects. For example, some action was completed some time ago and all data related to
 * this action was deleted, but the user missed this moment and later still wants to know how the action was
 * completed. If it were possible to store such objects for some time, it would help the user.
 *
 * Example with Progress object
 * 1. When Progress object is created it’s added into TrackedObjectCollector (call setTracked() in Progress::FinalConstruct()).
 *
 * 2. User keeps the Progress Id. VirtualBox API returns Progress Id to user everywhere where its’ needed.
 * When user wants to know the status of action he makes a request to VirtualBox calling the function
 * VirtualBox::getTrackedObject(). This function looks through TrackedObjectCollector and retrieves the information
 * about a requested object and if one exists there returns a pointer to IUnknown interface.
 * This pointer should be converted to the appropriate interface type (IProgress in this case). Next the data are
 * extracted using the interface functions.
 *
 * 2.1. Approach 1.
 * Getting information about a tracked object using VirtualBox API (VirtualBox frontend or any external client)
 *  - Call VirtualBox::getTrackedObjectIds() with a requested interface name and get back the list of tracked objects.
 *  - Go through the list, call VirtualBox::getTrackedObject() for each Id from the list.
 *  - Convert IUnknown interface to the requested interface.
 *  - Retrieve information about an object.
 *
 *  See the full example 1 below.
 *
 * 2.2. Approach 2
 * Getting information about a tracked object on server side (VBoxSVC) is:
 *  - Call TrackedObjectsCollector::getObjIdsByClassIID() with the interface name IID (our case is IID_IProgress)
 *    and get back the list of tracked objects.
 *  - Go through the list, call TrackedObjectsCollectorState::getObj() for each Id from the list.
 *  - Convert IUnknown interface to the requested interface.
 *  - Retrieve information about an object.
 *
 * See the full example 2 below.
 *
 * 3. Class TrackedObjectData keeps some additional data about the tracked object as creation time, last access time,
 * life time and etc.
 *
 * 4. The last access time is updated for the requested object if it's needed (in the class TrackedObjectData).
 *
 * 5. For managing the items stored in the TrackedObjectCollector, a new thread TrackedObjectsThread is launched
 * in VirtualBox::init() function.
 *
 * 6. Periodically (1 sec at present), the thread TrackedObjectsThread goes through the list of tracked objects,
 * measures the difference between the current time and the creation time, if this value is greater than the life
 * time the object is marked as "lifetime expired" and next the idletime is started for this object. When the idle
 * time is expired the object is removed from the TrackedObjectsCollector.
 *
 * 7. There is a special case for an object with lifetime = 0. This means that the object has an infinite lifetime.
 * How to handle this case? The idea is that the reference count of an unused object is 1, since the object is only
 * represented inside the TrackedObjectCollector. When the count is 1, we mark the lifetime as expired and update
 * the object with new data. After this action, the logic becomes standard (see point 6).
 *
 * Addon.
 * Example 1. Getting information about a Progress tracked object on server side (VBoxSVC)
 *
 *  std::vector<com::Utf8Str> lObjIdMap;
 *  gTrackedObjectsCollector.getObjIdsByClassIID(IID_IProgress, lObjIdMap);
 *  for (const com::Utf8Str &item : lObjIdMap)
 *  {
 *      if (gTrackedObjectsCollector.checkObj(item.c_str()))
 *      {
 *          TrackedObjectData temp;
 *          gTrackedObjectsCollector.getObj(item.c_str(), temp);
 *          Log2(("Tracked Progress Object with objectId %s was found\n", temp.objectIdStr().c_str()));
 *
 *          ComPtr<IProgress> pProgress;
 *          temp.getInterface()->QueryInterface(IID_IProgress, (void **)pProgress.asOutParam());
 *          if (pProgress.isNotNull())
 *          {
 *              com::Bstr reqId(aId.toString());
 *              Bstr foundId;
 *              pProgress->COMGETTER(Id)(foundId.asOutParam());
 *              if (reqId == foundId)
 *              {
 *                  BOOL aCompleted;
 *                  pProgress->COMGETTER(Completed)(&aCompleted);
 *
 *                  BOOL aCanceled;
 *                  pProgress->COMGETTER(Canceled)(&aCanceled);
 *
 *                  aProgressObject = pProgress;
 *                  return S_OK;
 *              }
 *          }
 *      }
 *  }
 *
 * Example 2. Getting information about a Progress tracked object using VirtualBox API
 *
 *  Utf8Str strObjUuid; // passed from the client
 *  Utf8Str strIfaceName;// passed from the client
 *  ComPtr<IVirtualBox> pVirtualBox;// set before. Usually each client has own ComPtr<IVirtualBox>.
 *
 *  com::SafeArray<BSTR> ObjIDsList;
 *  hrc = pVirtualBox->GetTrackedObjectIds(Bstr(strIfaceName).raw(), ComSafeArrayAsOutParam(ObjIDsList));
 *  if (SUCCEEDED(hrc))
 *  {
 *      map < Bstr, TrackedObjInfo_T > lObjInfoMap;
 *      for (size_t i = 0; i < ObjIDsList.size(); ++i)
 *      {
 *          Bstr bstrObjId = ObjIDsList[i];
 *          if (bstrObjId.equals(strObjUuid.c_str()))
 *          {
 *              TrackedObjInfo_T objInfo;
 *              hrc = pVirtualBox->GetTrackedObject(bstrObjId.raw(),
 *                                                  objInfo.pIUnknown.asOutParam(),
 *                                                  &objInfo.enmState,
 *                                                  &objInfo.creationTime,
 *                                                  &objInfo.deletionTime);
 *
 *              // print tracked object information as state, creation time, deletion time
 *              // objInfo.enmState, objInfo.creationTime, objInfo.deletionTime
 *
 *              if (objInfo.enmState != TrackedObjectState_Invalid)
 *              {
 *                  // Conversion IUnknown -> IProgress
 *                  ComPtr<IProgress> pObj;
 *                  objInfo.pIUnknown->QueryInterface(IID_IProgress, (void **)pObj.asOutParam());
 *                  if (pObj.isNotNull())
 *                      //print Progress Object data as description, completion, cancellation etc.
 *              }
 *              break;
 *          }
 *      }
 *  }
*/

#define LOG_GROUP LOG_GROUP_MAIN

#include "LoggingNew.h"
#include "VirtualBoxBase.h"
#include "ObjectsTracker.h"
#include <iprt/log.h>
#include <iprt/stream.h>
#include <iprt/time.h>
#include <iprt/asm.h>
#include <stdexcept>

typedef std::map<com::Utf8Str, TrackedObjectData>::iterator IterTrObjData_T;
typedef std::map<com::Utf8Str, TrackedObjectData>::const_iterator ConstIterTrObjData_T;

static Utf8Str trackedObjectStateToStr(TrackedObjectState_T aState)
{
    Utf8Str strState("None");
    switch (aState)
    {
        case TrackedObjectState_Alive:
            strState = "Alive";
            break;
        case TrackedObjectState_Deleted:
            strState = "Deleted";
            break;
        case TrackedObjectState_Invalid:
            strState = "Invalid";
            break;
        case TrackedObjectState_None:
        default:
            strState = "None";
            break;
    }

    return strState;
}

/////////////////////////////////////////////////////////////////////////////
// TrackedObjectData
/////////////////////////////////////////////////////////////////////////////
TrackedObjectData::TrackedObjectData():
    m_componentName("noname"),
    m_lifeTime(0),
    m_idleTime(1),
    m_state(TrackedObjectState_None),
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
    m_fLifeTimeExpired(false),
    m_pIface(aPtr)
{
    RTTimeNow(unconst(&m_creationTime));
    m_lastAccessTime = m_creationTime;
    m_state = TrackedObjectState_Alive;
}

TrackedObjectData::TrackedObjectData(const TrackedObjectData & that)
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
        m_deletionTime = that.m_deletionTime;
        m_lastAccessTime = that.m_lastAccessTime;
        m_idleTimeStart = that.m_idleTimeStart;
        m_fIdleTimeStart = that.m_fIdleTimeStart;
        m_fLifeTimeExpired = that.m_fLifeTimeExpired;
        m_state = that.m_state;
    }
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
        m_deletionTime = that.m_deletionTime;
        m_lastAccessTime = that.m_lastAccessTime;
        m_idleTimeStart = that.m_idleTimeStart;
        m_fIdleTimeStart = that.m_fIdleTimeStart;
        m_fLifeTimeExpired = that.m_fLifeTimeExpired;
        m_state = that.m_state;
    }

    return *this;
}

com::Utf8Str TrackedObjectData::updateLastAccessTime()
{
    RTTimeNow(&m_lastAccessTime);

    char szTime[RTTIME_STR_LEN];
    RTTimeSpecToString(&m_lastAccessTime, szTime, sizeof(szTime));
    return com::Utf8Str(szTime);
}

/** @todo r=bird: why on earth does this return a string? */
com::Utf8Str TrackedObjectData::initIdleTime()
{
    if (!m_fIdleTimeStart)
    {
        RTTimeNow(unconst(&m_deletionTime));
        updateState(TrackedObjectState_Deleted);//Alive -> Deleted
        RTTimeNow(unconst(&m_idleTimeStart));
        m_fIdleTimeStart = true;
        m_fLifeTimeExpired = true;
    }

    char szTime[RTTIME_STR_LEN];
    RTTimeSpecToString(&m_idleTimeStart, szTime, sizeof(szTime));
    return com::Utf8Str(szTime);
}

com::Utf8Str TrackedObjectData::creationTimeStr() const
{
    char szCreationTime[RTTIME_STR_LEN];
    RTTimeSpecToString(&m_creationTime, szCreationTime, sizeof(szCreationTime));

    return com::Utf8Str(szCreationTime);
}

TrackedObjectState_T TrackedObjectData::deletionTime(PRTTIMESPEC aTime) const
{
    if (m_state != TrackedObjectState_Alive)
        *aTime = m_deletionTime;
    return m_state;
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

TrackedObjectState_T TrackedObjectData::updateState(TrackedObjectState_T aNewState)
{
    return m_state < aNewState ? m_state = aNewState : m_state;
}

/////////////////////////////////////////////////////////////////////////////
// TrackedObjectsCollector
/////////////////////////////////////////////////////////////////////////////
class TrackedObjectsCollector;
extern TrackedObjectsCollector gTrackedObjectsCollector;

TrackedObjectsCollector::TrackedObjectsCollector(): m_fInitialized(false)
{
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
    else if (LogIs2Enabled())
    {
        char szCreationTime[RTTIME_STR_LEN];
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


HRESULT TrackedObjectsCollector::updateObj (const TrackedObjectData& aObjData)
{
    LogFlowFuncEnter();

    HRESULT hrc = S_OK;
    int vrc = i_checkInitialization();
    if (RT_FAILURE(vrc))
        return VBOX_E_INVALID_OBJECT_STATE;

    /* Enter critical section here */
    RTCritSectEnter(&m_CritSectData);

    std::pair < std::set<com::Utf8Str>::iterator, bool > opRes = m_trackedObjectIds.insert(aObjData.objectIdStr());

    /*
     * The case for updating the tracked object data.
     * The Id is presented in the m_trackedObjectIds. The original object is removed from m_trackedObjectsData.
     */
    if (!opRes.second)
    {
        Utf8Str strState = trackedObjectStateToStr(aObjData.state());
        Log2(("UPDATING TrackedObjectData:\n state %s\n object Id %s\n class IID %s\n life time %i\n idle time %i"
                "\n life time expired - %s\n idle time started - %s\n",
                strState.c_str(),
                aObjData.objectIdStr().c_str(),
                aObjData.classIIDStr().c_str(),
                aObjData.lifeTime(),
                aObjData.idleTime(),
                (aObjData.isLifeTimeExpired() == true ? "True" : "False"),
                (aObjData.isIdleTimeStarted() == true ? "True" : "False")));

        m_trackedObjectsData.erase(aObjData.objectIdStr().c_str());
        /* decrease the counter */
        --m_Added;

        /* Data is stored in the m_trackedObjectsData under the passed Id. */
        m_trackedObjectsData.insert(std::make_pair(aObjData.objectIdStr(), aObjData));

        /* increase the counter */
        ++m_Added;
    }
    else
    {
        Log2(("UPDATING failed because the object Id %s hasn't existed.\n", aObjData.objectIdStr().c_str()));
        m_trackedObjectIds.erase(aObjData.objectIdStr());
    }

    /* Leave critical section here */
    RTCritSectLeave(&m_CritSectData);

    return hrc;
}

HRESULT TrackedObjectsCollector::getObj(const com::Utf8Str &aObjId,
                                        TrackedObjectData &aObjData,
                                        bool fUpdate)
{
    LogFlowFuncEnter();

    int vrc = i_checkInitialization();
    if (RT_FAILURE(vrc))
        return VBOX_E_INVALID_OBJECT_STATE;

    /* Enter critical section here */
    RTCritSectEnter(&m_CritSectData);

    HRESULT hrc = E_FAIL;

    IterTrObjData_T pIter = m_trackedObjectsData.find(aObjId);
    if (pIter != m_trackedObjectsData.end())
    {
        if (fUpdate == true)
        {
            /* Update some fields in the found object if needed. in instance, the last access time */
            com::Utf8Str lat = pIter->second.updateLastAccessTime(); /* Update the access time */
            Log2(("The updated last access time is %s\n", lat.c_str()));
        }
        hrc = S_OK;
    }
    else
        hrc = VBOX_E_OBJECT_NOT_FOUND;

    if (SUCCEEDED(hrc))
        aObjData = i_getObj(aObjId);

    /* Leave critical section here */
    RTCritSectLeave(&m_CritSectData);

    return hrc;
}

const TrackedObjectData &TrackedObjectsCollector::i_getObj(const com::Utf8Str &aObjId) const
{
    /* No check for existence of aObjId */
#if 0 /* the solaris VM's stl_map.h code doesn't have any at() function. */
    return m_trackedObjectsData.at(aObjId);
#else
    ConstIterTrObjData_T const Iter = m_trackedObjectsData.find(aObjId);
    if (Iter == m_trackedObjectsData.end())
        throw std::out_of_range(aObjId.c_str());
    return (*Iter).second;
#endif
}

HRESULT TrackedObjectsCollector::initObjIdleTime(const com::Utf8Str &aObjId)
{
    LogFlowFuncEnter();

    HRESULT hrc = S_OK;
    int vrc = i_checkInitialization();
    if (RT_FAILURE(vrc))
        return VBOX_E_INVALID_OBJECT_STATE;

    vrc = VERR_NOT_FOUND;
    /* Enter critical section here */
    RTCritSectEnter(&m_CritSectData);

    IterTrObjData_T pIter = m_trackedObjectsData.find(aObjId);
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

HRESULT TrackedObjectsCollector::removeObj(const com::Utf8Str &aObjId)
{
    Log2(("%s: object Id %s\n", __FUNCTION__, aObjId.c_str()));

    HRESULT hrc = S_OK;
    int vrc = i_checkInitialization();
    if (RT_FAILURE(vrc))
        return VBOX_E_INVALID_OBJECT_STATE;

    vrc = VERR_NOT_FOUND;

    /* Enter critical section here */
    RTCritSectEnter(&m_CritSectData);

    IterTrObjData_T Iter = m_trackedObjectsData.find(aObjId);
    if (Iter != m_trackedObjectsData.end())
    {
        Log2(("RELEASED TrackedObjectData: creation time %s, object Id %s, class IID %s\n",
               Iter->second.creationTimeStr().c_str(), Iter->second.objectIdStr().c_str(), Iter->second.classIIDStr().c_str()));

        m_trackedObjectsData.erase(Iter);
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

int TrackedObjectsCollector::i_getAllObjIds(std::vector<com::Utf8Str> &aObjIdMap) const
{
    //for (const com::Utf8Str &item : m_trackedObjectIds) - the gcc in the solaris VM doesn't grok this.
    for (std::set<com::Utf8Str>::const_iterator Iter = m_trackedObjectIds.begin();
         Iter != m_trackedObjectIds.end();
         ++Iter)
            aObjIdMap.push_back(*Iter);

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

int TrackedObjectsCollector::i_getObjIdsByClassIID(const Guid &aIId, std::vector<com::Utf8Str> &aObjIdMap) const
{
    //for (const std::pair<const com::Utf8Str, TrackedObjectData> &item : m_trackedObjectsData)  - the gcc in the solaris VM doesn't grok this.
    for (ConstIterTrObjData_T Iter = m_trackedObjectsData.begin();
         Iter != m_trackedObjectsData.end();
         ++Iter)
    {
        if (Iter->second.classIID() == aIId)
            aObjIdMap.push_back(Iter->first);
    }

    return aObjIdMap.size() > 0 ? VINF_SUCCESS : VERR_NOT_FOUND;
}

bool TrackedObjectsCollector::checkObj(const com::Utf8Str &aObjId)
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

    IterTrObjData_T pIter = m_trackedObjectsData.find(aObjId.c_str());
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

    int vrc = i_checkInitialization();
    if (RT_FAILURE(vrc))
        return VBOX_E_INVALID_OBJECT_STATE;

    /* Enter critical section here */
    RTCritSectEnter(&m_CritSectData);

    HRESULT hrc = VBOX_E_OBJECT_NOT_FOUND;
    IterTrObjData_T pIter = m_trackedObjectsData.find(aObjId);
    if (pIter != m_trackedObjectsData.end())
    {
        pIter->second.updateState(TrackedObjectState_Invalid);//Deleted -> Invalid
        m_trackedInvalidObjectIds.insert(aObjId);
        hrc = S_OK;
    }

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
    return true;
}

bool ObjectTracker::isFinished()
{
    LogFlowFuncEnter();
    return ASMAtomicReadBool(&fFinish);
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

        //for (const com::Utf8Str& item : lObjIdMap) - the gcc in the solaris VM doesn't grok this.
        for (std::vector<com::Utf8Str>::const_iterator Iter = lObjIdMap.begin(); Iter != lObjIdMap.end(); ++Iter)
        {
            TrackedObjectData temp;
            if(gTrackedObjectsCollector.checkObj(*Iter))
            {
                hrc = gTrackedObjectsCollector.getObj(*Iter, temp);
                if (SUCCEEDED(hrc))
                {
                    Log2(("Tracked Object with ID %s was found:\n", temp.m_objId.toString().c_str()));

                    RTTIMESPEC now;
                    int64_t currTime = RTTimeSpecGetMilli(RTTimeNow(&now));
                    int64_t creationTime = RTTimeSpecGetMilli(&temp.m_creationTime);
                    int64_t lifeTime = (int64_t)temp.m_lifeTime*1000; //convert to milliseconds

                    int64_t remainingLifeTime = lifeTime == 0 ? 0 : ((creationTime + lifeTime) - currTime)/1000;

                    bool fLifeTimeExpired = temp.m_fLifeTimeExpired;
                    if (!fLifeTimeExpired)
                        fLifeTimeExpired = (remainingLifeTime == 0 && lifeTime != 0) ? true : false;

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

                    if (fLifeTimeExpired)
                    {
                        {
                            if (temp.m_fIdleTimeStart == false)
                            {
                                gTrackedObjectsCollector.initObjIdleTime(*Iter);
                                Log2(("Idle time for the object with Id %s has been started\n", Iter->c_str()));
                                Log2(("  class IID %s, refcount %lu\n", temp.m_classIID.toString().c_str(), cRefs - 1));
                            }
                            else
                            {
                                int64_t idleTime = (int64_t)temp.m_idleTime*1000; //convert to milliseconds
                                int64_t idleTimeStart = RTTimeSpecGetMilli(&temp.m_idleTimeStart);
                                bool fIdleTimeExpired = (currTime - idleTimeStart) > idleTime ? true : false;
                                if (fIdleTimeExpired)
                                {
                                    Log2(("Object with Id %s removed from Object Collector "
                                            "(recount is %u, idle time exceeded %u sec)\n", Iter->c_str(), cRefs - 1, temp.m_idleTime));
                                    gTrackedObjectsCollector.removeObj(*Iter);
                                }
                            }
                        }
                    }
                    else
                    {
                        if (cRefs <= 2)
                        {
                            /*
                             * Special case for objects with the original lifeTime equal to 0 (0 means endless).
                             * For these objects the idle time starts when user deletes it.
                             */
                            if (lifeTime == 0)
                            {
                                temp.m_fLifeTimeExpired = true;

                                /* Updating the object data */
                                gTrackedObjectsCollector.updateObj(temp);
                            }
                        }
                    }
                }
            }
            else
                Log2(("Tracked Object with ID %s was not found\n", Iter->c_str()));
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

