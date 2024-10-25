/* $Id$ */
/** @file
 * VirtualBox Object tracker definitions
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

#ifndef MAIN_INCLUDED_ObjectsTracker_h
#define MAIN_INCLUDED_ObjectsTracker_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <set>
#include <unordered_map>
#include <vector>
#include <string>

#include <iprt/time.h>
#include <iprt/cpp/utils.h>

#include "ThreadTask.h"

class ThreadTask;
class ObjectTracker;
class TrackedObjectsCollector;

/////////////////////////////////////////////////////////////////////////////
// ObjectTracker
/////////////////////////////////////////////////////////////////////////////
class ObjectTracker
{
    volatile bool fFinish;
    RTTHREAD m_Thread;
    Utf8Str m_strTaskName;

public:
    ObjectTracker(): fFinish(false), m_Thread(NIL_RTTHREAD), m_strTaskName("ObjTracker"){};
    ObjectTracker(PRTTHREAD pThread): fFinish(false), m_Thread(*pThread){};

    ~ObjectTracker();

    inline Utf8Str getTaskName() const { return m_strTaskName; }
    bool init();
    bool finish();
    bool isFinished();
    int createThread();

    static DECLCALLBACK(int) objectTrackerTask(RTTHREAD ThreadSelf, void *pvUser);
};

/////////////////////////////////////////////////////////////////////////////
// TrackedObjectData
/////////////////////////////////////////////////////////////////////////////
class TrackedObjectData
{
public:
    enum State { Invalid, Valid };

    TrackedObjectData();

    explicit
    TrackedObjectData(const com::Guid &aObjId,
                      const com::Guid &aClassIID,
                      uint64_t aLifeTime,
                      uint64_t aIdleTime,
                      IUnknown* aPtr);

    ~TrackedObjectData();
    TrackedObjectData& operator =(const TrackedObjectData &that);

    inline const ComPtr<IUnknown>& getInterface() const
    {
        return m_pIface;
    }

    inline com::Guid objectId() const
    {
        return m_objId;
    }

    inline com::Guid classIID() const
    {
        return m_classIID;
    }

    inline com::Utf8Str objectIdStr() const
    {
        return m_objId.toString();
    }

    inline com::Utf8Str classIIDStr() const
    {
        return m_classIID.toString();
    }

    inline State state() const
    {
        return m_State;
    }

    inline State resetState()
    {
        return m_State = Invalid;
    }

    com::Utf8Str updateLastAccessTime();
    com::Utf8Str initIdleTime();
    com::Utf8Str creationTimeStr() const;

private:
    com::Guid m_objId;
    com::Guid m_classIID;
    com::Utf8Str m_componentName;
    RTTIMESPEC m_creationTime;//creation time
    RTTIMESPEC m_idleTimeStart;//idle time beginning (ref counter is 1)
    RTTIMESPEC m_lastAccessTime;//last access time
    uint64_t m_lifeTime;//lifetime after creation in seconds, 0 - live till the VBoxSVC lives
    uint64_t m_idleTime;//lifetime after out of usage in seconds, 0 - keep forever
    bool m_fIdleTimeStart;//when ref counter of m_pIface is 1 or m_lifeTime exceeded
    State m_State;//state may have only 2 variants Valid or Invalid. State has only one transition from Valid to Invalid
    ComPtr<IUnknown> m_pIface;//keeps a reference to a tracked object

private:
    unsigned long i_checkRefCount(const Guid& aIID);

    friend DECLCALLBACK(int) ObjectTracker::objectTrackerTask(RTTHREAD ThreadSelf, void *pvUser);
};

/////////////////////////////////////////////////////////////////////////////
// TrackedObjectsCollector
/////////////////////////////////////////////////////////////////////////////
class TrackedObjectsCollector
{
    /** Critical section protecting the data in TrackedObjectsCollector */
    RTCRITSECT m_CritSectData;

    std::set<com::Utf8Str> m_trackedObjectIds;//Full list of valid + invalid objects
    std::set<com::Utf8Str> m_trackedInvalidObjectIds;//List of invalid objects only
    std::unordered_map<std::string, TrackedObjectData> m_trackedObjectsData;//Mapping Object Id -> Object Data

    uint64_t m_Added;//Counter of the added objects
    uint64_t m_Released;//Counter of the released objects
    bool m_fInitialized;//Sign whether TrackedObjectsCollector is initialized or not

public:
    TrackedObjectsCollector();
    ~TrackedObjectsCollector();

    bool init();//must be called after creation and before usage
    bool uninit();

    HRESULT setObj (const com::Utf8Str &aObjId,
                    const com::Utf8Str &aClassIID,
                    uint64_t lifeTime,
                    uint64_t afterLifeTime,
                    IUnknown* ptrIface);

    HRESULT getObj (const com::Utf8Str &aObjId,
                    TrackedObjectData &aObjData,
                    bool fUpdate = true);

    HRESULT initObjIdleTime (const com::Utf8Str& aObjId);

    HRESULT removeObj (const com::Utf8Str &aObjId);

    HRESULT getAllObjIds (std::vector<com::Utf8Str>& aObjIdMap);

    HRESULT getObjIdsByClassIID (const com::Guid& iid, std::vector<com::Utf8Str>& aObjIdMap);

    bool checkObj(const com::Utf8Str& aObjId);

    HRESULT tryToRemoveObj(const com::Utf8Str& aObjId);

    enum TrackedObjectsCollectorState { NormalOperation, ForceClearing, Deletion, Uninitialization };

    HRESULT clear(TrackedObjectsCollectorState aState = NormalOperation);

    HRESULT invalidateObj(const com::Utf8Str &aObjId);

private:
    int i_checkInitialization() const;
    const TrackedObjectData& i_getObj (const com::Utf8Str& aObjId) const;
    int i_getAllObjIds (std::vector<com::Utf8Str>& aObjIdMap) const;
    int i_getObjIdsByClassIID (const com::Guid& iid, std::vector<com::Utf8Str>& aObjIdMap) const;
    bool i_checkObj(const com::Utf8Str& aObjId) const;
    int i_clear();
};
#endif /* MAIN_INCLUDED_ObjectsTracker_h */
