/* $Id$ */
/** @file
 * VirtualBox COM class implementation
 */

/*
 * Copyright (C) 2006-2016 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ____H_SNAPSHOTIMPL
#define ____H_SNAPSHOTIMPL

#include "SnapshotWrap.h"

class SnapshotMachine;

namespace settings
{
    struct Snapshot;
}

class ATL_NO_VTABLE Snapshot :
    public SnapshotWrap
{
public:
    DECLARE_EMPTY_CTOR_DTOR(Snapshot)

    HRESULT FinalConstruct();
    void FinalRelease();

    // public initializer/uninitializer only for internal purposes
    HRESULT init(VirtualBox *aVirtualBox,
                 const Guid &aId,
                 const com::Utf8Str &aName,
                 const com::Utf8Str &aDescription,
                 const RTTIMESPEC &aTimeStamp,
                 SnapshotMachine *aMachine,
                 Snapshot *aParent);
    void uninit();

    void i_beginSnapshotDelete();

    void i_deparent();

    // public methods only for internal purposes

    /**
     * Override of the default locking class to be used for validating lock
     * order with the standard member lock handle.
     */
    virtual VBoxLockingClass getLockingClass() const
    {
        return LOCKCLASS_SNAPSHOTOBJECT;
    }

    const ComObjPtr<Snapshot>& i_getParent() const;
    const ComObjPtr<Snapshot> i_getFirstChild() const;

    const Utf8Str& i_getStateFilePath() const;

    uint32_t i_getDepth();

    ULONG i_getChildrenCount();
    ULONG i_getAllChildrenCount();
    ULONG i_getAllChildrenCountImpl();

    const ComObjPtr<SnapshotMachine>& i_getSnapshotMachine() const;

    Guid i_getId() const;
    const Utf8Str& i_getName() const;
    RTTIMESPEC i_getTimeStamp() const;

    ComObjPtr<Snapshot> i_findChildOrSelf(IN_GUID aId);
    ComObjPtr<Snapshot> i_findChildOrSelf(const com::Utf8Str &aName);

    void i_updateSavedStatePaths(const Utf8Str &strOldPath,
                                 const Utf8Str &strNewPath);
    void i_updateSavedStatePathsImpl(const Utf8Str &strOldPath,
                                     const Utf8Str &strNewPath);

    bool i_sharesSavedStateFile(const Utf8Str &strPath,
                                Snapshot *pSnapshotToIgnore);

    HRESULT i_saveSnapshot(settings::Snapshot &data) const;
    HRESULT i_saveSnapshotImpl(settings::Snapshot &data) const;
    HRESULT i_saveSnapshotImplOne(settings::Snapshot &data) const;

    HRESULT i_uninitOne(AutoWriteLock &writeLock,
                        CleanupMode_T cleanupMode,
                        MediaList &llMedia,
                        std::list<Utf8Str> &llFilenames);
    HRESULT i_uninitRecursively(AutoWriteLock &writeLock,
                                CleanupMode_T cleanupMode,
                                MediaList &llMedia,
                                std::list<Utf8Str> &llFilenames);


private:

    struct Data;            // opaque, defined in SnapshotImpl.cpp

    // wrapped ISnapshot properties
    HRESULT getId(com::Guid &aId);
    HRESULT getName(com::Utf8Str &aName);
    HRESULT setName(const com::Utf8Str &aName);
    HRESULT getDescription(com::Utf8Str &aDescription);
    HRESULT setDescription(const com::Utf8Str &aDescription);
    HRESULT getTimeStamp(LONG64 *aTimeStamp);
    HRESULT getOnline(BOOL *aOnline);
    HRESULT getMachine(ComPtr<IMachine> &aMachine);
    HRESULT getParent(ComPtr<ISnapshot> &aParent);
    HRESULT getChildren(std::vector<ComPtr<ISnapshot> > &aChildren);

    // wrapped ISnapshot methods
    HRESULT getChildrenCount(ULONG *aChildrenCount);

    Data *m;
};

#endif // ____H_SNAPSHOTIMPL

/* vi: set tabstop=4 shiftwidth=4 expandtab: */
