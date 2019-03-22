/* $Id$ */
/** @file
 * Implementation of MachineCloneVM
 */

/*
 * Copyright (C) 2011-2017 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#include "MachineImplCloneVM.h"

#include "VirtualBoxImpl.h"
#include "MediumImpl.h"
#include "HostImpl.h"

#include <iprt/path.h>
#include <iprt/dir.h>
#include <iprt/cpp/utils.h>
#ifdef DEBUG_poetzsch
# include <iprt/stream.h>
#endif

#include <VBox/com/list.h>
#include <VBox/com/MultiResult.h>

// typedefs
/////////////////////////////////////////////////////////////////////////////

typedef struct
{
    Utf8Str                 strBaseName;
    ComPtr<IMedium>         pMedium;
    uint32_t                uIdx;
    ULONG                   uWeight;
} MEDIUMTASK;

typedef struct
{
    RTCList<MEDIUMTASK>     chain;
    DeviceType_T            devType;
    bool                    fCreateDiffs;
    bool                    fAttachLinked;
} MEDIUMTASKCHAIN;

typedef struct
{
    Guid                    snapshotUuid;
    Utf8Str                 strSaveStateFile;
    ULONG                   uWeight;
} SAVESTATETASK;

// The private class
/////////////////////////////////////////////////////////////////////////////

struct MachineCloneVMPrivate
{
    MachineCloneVMPrivate(MachineCloneVM *a_q, ComObjPtr<Machine> &a_pSrcMachine, ComObjPtr<Machine> &a_pTrgMachine,
                          CloneMode_T a_mode, const RTCList<CloneOptions_T> &opts)
      : q_ptr(a_q)
      , p(a_pSrcMachine)
      , pSrcMachine(a_pSrcMachine)
      , pTrgMachine(a_pTrgMachine)
      , mode(a_mode)
      , options(opts)
    {}

    /* Thread management */
    int startWorker()
    {
        return RTThreadCreate(NULL,
                              MachineCloneVMPrivate::workerThread,
                              static_cast<void*>(this),
                              0,
                              RTTHREADTYPE_MAIN_WORKER,
                              0,
                              "MachineClone");
    }

    static DECLCALLBACK(int) workerThread(RTTHREAD /* Thread */, void *pvUser)
    {
        MachineCloneVMPrivate *pTask = static_cast<MachineCloneVMPrivate*>(pvUser);
        AssertReturn(pTask, VERR_INVALID_POINTER);

        HRESULT rc = pTask->q_ptr->run();

        pTask->pProgress->i_notifyComplete(rc);

        pTask->q_ptr->destroy();

        return VINF_SUCCESS;
    }

    /* Private helper methods */

    /* MachineCloneVM::start helper: */
    HRESULT createMachineList(const ComPtr<ISnapshot> &pSnapshot, RTCList< ComObjPtr<Machine> > &machineList) const;
    inline void updateProgressStats(MEDIUMTASKCHAIN &mtc, bool fAttachLinked, ULONG &uCount, ULONG &uTotalWeight) const;
    inline HRESULT addSaveState(const ComObjPtr<Machine> &machine, bool fAttachCurrent, ULONG &uCount, ULONG &uTotalWeight);
    inline HRESULT queryBaseName(const ComPtr<IMedium> &pMedium, Utf8Str &strBaseName) const;
    HRESULT queryMediasForMachineState(const RTCList<ComObjPtr<Machine> > &machineList,
                                       bool fAttachLinked, ULONG &uCount, ULONG &uTotalWeight);
    HRESULT queryMediasForMachineAndChildStates(const RTCList<ComObjPtr<Machine> > &machineList,
                                                bool fAttachLinked, ULONG &uCount, ULONG &uTotalWeight);
    HRESULT queryMediasForAllStates(const RTCList<ComObjPtr<Machine> > &machineList, bool fAttachLinked, ULONG &uCount,
                                    ULONG &uTotalWeight);

    /* MachineCloneVM::run helper: */
    bool findSnapshot(const settings::SnapshotsList &snl, const Guid &id, settings::Snapshot &sn) const;
    void updateMACAddresses(settings::NetworkAdaptersList &nwl) const;
    void updateMACAddresses(settings::SnapshotsList &sl) const;
    void updateStorageLists(settings::StorageControllersList &sc, const Bstr &bstrOldId, const Bstr &bstrNewId) const;
    void updateSnapshotStorageLists(settings::SnapshotsList &sl, const Bstr &bstrOldId, const Bstr &bstrNewId) const;
    void updateStateFile(settings::SnapshotsList &snl, const Guid &id, const Utf8Str &strFile) const;
    HRESULT createDifferencingMedium(const ComObjPtr<Machine> &pMachine, const ComObjPtr<Medium> &pParent,
                                     const Utf8Str &strSnapshotFolder, RTCList<ComObjPtr<Medium> > &newMedia,
                                     ComObjPtr<Medium> *ppDiff) const;
    static DECLCALLBACK(int) copyStateFileProgress(unsigned uPercentage, void *pvUser);
    static void updateSnapshotHardwareUUIDs(settings::SnapshotsList &snapshot_list, const Guid &id);

    /* Private q and parent pointer */
    MachineCloneVM             *q_ptr;
    ComObjPtr<Machine>          p;

    /* Private helper members */
    ComObjPtr<Machine>          pSrcMachine;
    ComObjPtr<Machine>          pTrgMachine;
    ComPtr<IMachine>            pOldMachineState;
    ComObjPtr<Progress>         pProgress;
    Guid                        snapshotId;
    CloneMode_T                 mode;
    RTCList<CloneOptions_T>     options;
    RTCList<MEDIUMTASKCHAIN>    llMedias;
    RTCList<SAVESTATETASK>      llSaveStateFiles; /* Snapshot UUID -> File path */
};

HRESULT MachineCloneVMPrivate::createMachineList(const ComPtr<ISnapshot> &pSnapshot,
                                                 RTCList< ComObjPtr<Machine> > &machineList) const
{
    HRESULT rc = S_OK;
    Bstr name;
    rc = pSnapshot->COMGETTER(Name)(name.asOutParam());
    if (FAILED(rc)) return rc;

    ComPtr<IMachine> pMachine;
    rc = pSnapshot->COMGETTER(Machine)(pMachine.asOutParam());
    if (FAILED(rc)) return rc;
    machineList.append((Machine*)(IMachine*)pMachine);

    SafeIfaceArray<ISnapshot> sfaChilds;
    rc = pSnapshot->COMGETTER(Children)(ComSafeArrayAsOutParam(sfaChilds));
    if (FAILED(rc)) return rc;
    for (size_t i = 0; i < sfaChilds.size(); ++i)
    {
        rc = createMachineList(sfaChilds[i], machineList);
        if (FAILED(rc)) return rc;
    }

    return rc;
}

void MachineCloneVMPrivate::updateProgressStats(MEDIUMTASKCHAIN &mtc, bool fAttachLinked,
                                                ULONG &uCount, ULONG &uTotalWeight) const
{
    if (fAttachLinked)
    {
        /* Implicit diff creation as part of attach is a pretty cheap
         * operation, and does only need one operation per attachment. */
        ++uCount;
        uTotalWeight += 1;  /* 1MB per attachment */
    }
    else
    {
        /* Currently the copying of diff images involves reading at least
         * the biggest parent in the previous chain. So even if the new
         * diff image is small in size, it could need some time to create
         * it. Adding the biggest size in the chain should balance this a
         * little bit more, i.e. the weight is the sum of the data which
         * needs to be read and written. */
        ULONG uMaxWeight = 0;
        for (size_t e = mtc.chain.size(); e > 0; --e)
        {
            MEDIUMTASK &mt = mtc.chain.at(e - 1);
            mt.uWeight += uMaxWeight;

            /* Calculate progress data */
            ++uCount;
            uTotalWeight += mt.uWeight;

            /* Save the max size for better weighting of diff image
             * creation. */
            uMaxWeight = RT_MAX(uMaxWeight, mt.uWeight);
        }
    }
}

HRESULT MachineCloneVMPrivate::addSaveState(const ComObjPtr<Machine> &machine, bool fAttachCurrent, ULONG &uCount, ULONG &uTotalWeight)
{
    Bstr bstrSrcSaveStatePath;
    HRESULT rc = machine->COMGETTER(StateFilePath)(bstrSrcSaveStatePath.asOutParam());
    if (FAILED(rc)) return rc;
    if (!bstrSrcSaveStatePath.isEmpty())
    {
        SAVESTATETASK sst;
        if (fAttachCurrent)
        {
            /* Make this saved state part of "current state" of the target
             * machine, whether it is part of a snapshot or not. */
            sst.snapshotUuid.clear();
        }
        else
            sst.snapshotUuid = machine->i_getSnapshotId();
        sst.strSaveStateFile = bstrSrcSaveStatePath;
        uint64_t cbSize;
        int vrc = RTFileQuerySize(sst.strSaveStateFile.c_str(), &cbSize);
        if (RT_FAILURE(vrc))
            return p->setErrorBoth(VBOX_E_IPRT_ERROR, vrc, p->tr("Could not query file size of '%s' (%Rrc)"),
                                   sst.strSaveStateFile.c_str(), vrc);
        /*  same rule as above: count both the data which needs to
         * be read and written */
        sst.uWeight = (ULONG)(2 * (cbSize + _1M - 1) / _1M);
        llSaveStateFiles.append(sst);
        ++uCount;
        uTotalWeight += sst.uWeight;
    }
    return S_OK;
}

HRESULT MachineCloneVMPrivate::queryBaseName(const ComPtr<IMedium> &pMedium, Utf8Str &strBaseName) const
{
    ComPtr<IMedium> pBaseMedium;
    HRESULT rc = pMedium->COMGETTER(Base)(pBaseMedium.asOutParam());
    if (FAILED(rc)) return rc;
    Bstr bstrBaseName;
    rc = pBaseMedium->COMGETTER(Name)(bstrBaseName.asOutParam());
    if (FAILED(rc)) return rc;
    strBaseName = bstrBaseName;
    return rc;
}

HRESULT MachineCloneVMPrivate::queryMediasForMachineState(const RTCList<ComObjPtr<Machine> > &machineList,
                                                          bool fAttachLinked, ULONG &uCount, ULONG &uTotalWeight)
{
    /* This mode is pretty straightforward. We didn't need to know about any
     * parent/children relationship and therefore simply adding all directly
     * attached images of the source VM as cloning targets. The IMedium code
     * take than care to merge any (possibly) existing parents into the new
     * image. */
    HRESULT rc = S_OK;
    for (size_t i = 0; i < machineList.size(); ++i)
    {
        const ComObjPtr<Machine> &machine = machineList.at(i);
        /* If this is the Snapshot Machine we want to clone, we need to
         * create a new diff file for the new "current state". */
        const bool fCreateDiffs = (machine == pOldMachineState);
        /* Add all attachments of the different machines to a worker list. */
        SafeIfaceArray<IMediumAttachment> sfaAttachments;
        rc = machine->COMGETTER(MediumAttachments)(ComSafeArrayAsOutParam(sfaAttachments));
        if (FAILED(rc)) return rc;
        for (size_t a = 0; a < sfaAttachments.size(); ++a)
        {
            const ComPtr<IMediumAttachment> &pAtt = sfaAttachments[a];
            DeviceType_T type;
            rc = pAtt->COMGETTER(Type)(&type);
            if (FAILED(rc)) return rc;

            /* Only harddisks and floppies are of interest. */
            if (   type != DeviceType_HardDisk
                && type != DeviceType_Floppy)
                continue;

            /* Valid medium attached? */
            ComPtr<IMedium> pSrcMedium;
            rc = pAtt->COMGETTER(Medium)(pSrcMedium.asOutParam());
            if (FAILED(rc)) return rc;

            if (pSrcMedium.isNull())
                continue;

            /* Create the medium task chain. In this case it will always
             * contain one image only. */
            MEDIUMTASKCHAIN mtc;
            mtc.devType       = type;
            mtc.fCreateDiffs  = fCreateDiffs;
            mtc.fAttachLinked = fAttachLinked;

            /* Refresh the state so that the file size get read. */
            MediumState_T e;
            rc = pSrcMedium->RefreshState(&e);
            if (FAILED(rc)) return rc;
            LONG64 lSize;
            rc = pSrcMedium->COMGETTER(Size)(&lSize);
            if (FAILED(rc)) return rc;

            MEDIUMTASK mt;
            mt.uIdx = UINT32_MAX; /* No read/write optimization possible. */

            /* Save the base name. */
            rc = queryBaseName(pSrcMedium, mt.strBaseName);
            if (FAILED(rc)) return rc;

            /* Save the current medium, for later cloning. */
            mt.pMedium = pSrcMedium;
            if (fAttachLinked)
                mt.uWeight = 0; /* dummy */
            else
                mt.uWeight = (ULONG)((lSize + _1M - 1) / _1M);
            mtc.chain.append(mt);

            /* Update the progress info. */
            updateProgressStats(mtc, fAttachLinked, uCount, uTotalWeight);
            /* Append the list of images which have  to be cloned. */
            llMedias.append(mtc);
        }
        /* Add the save state files of this machine if there is one. */
        rc = addSaveState(machine, true /*fAttachCurrent*/, uCount, uTotalWeight);
        if (FAILED(rc)) return rc;
    }

    return rc;
}

HRESULT MachineCloneVMPrivate::queryMediasForMachineAndChildStates(const RTCList<ComObjPtr<Machine> > &machineList,
                                                                   bool fAttachLinked, ULONG &uCount, ULONG &uTotalWeight)
{
    /* This is basically a three step approach. First select all medias
     * directly or indirectly involved in the clone. Second create a histogram
     * of the usage of all that medias. Third select the medias which are
     * directly attached or have more than one directly/indirectly used child
     * in the new clone. Step one and two are done in the first loop.
     *
     * Example of the histogram counts after going through 3 attachments from
     * bottom to top:
     *
     *           3
     *           |
     *        -> 3
     *          / \
     *         2   1 <-
     *        /
     *    -> 2
     *      / \
     *  -> 1   1
     *          \
     *           1 <-
     *
     * Whenever the histogram count is changing compared to the previous one we
     * need to include that image in the cloning step (Marked with <-). If we
     * start at zero even the directly attached images are automatically
     * included.
     *
     * Note: This still leads to media chains which can have the same medium
     * included. This case is handled in "run" and therefore not critical, but
     * it leads to wrong progress infos which isn't nice. */

    Assert(!fAttachLinked);
    HRESULT rc = S_OK;
    std::map<ComPtr<IMedium>, uint32_t> mediaHist; /* Our usage histogram for the medias */
    for (size_t i = 0; i < machineList.size(); ++i)
    {
        const ComObjPtr<Machine> &machine = machineList.at(i);
        /* If this is the Snapshot Machine we want to clone, we need to
         * create a new diff file for the new "current state". */
        const bool fCreateDiffs = (machine == pOldMachineState);
        /* Add all attachments (and their parents) of the different
         * machines to a worker list. */
        SafeIfaceArray<IMediumAttachment> sfaAttachments;
        rc = machine->COMGETTER(MediumAttachments)(ComSafeArrayAsOutParam(sfaAttachments));
        if (FAILED(rc)) return rc;
        for (size_t a = 0; a < sfaAttachments.size(); ++a)
        {
            const ComPtr<IMediumAttachment> &pAtt = sfaAttachments[a];
            DeviceType_T type;
            rc = pAtt->COMGETTER(Type)(&type);
            if (FAILED(rc)) return rc;

            /* Only harddisks and floppies are of interest. */
            if (   type != DeviceType_HardDisk
                && type != DeviceType_Floppy)
                continue;

            /* Valid medium attached? */
            ComPtr<IMedium> pSrcMedium;
            rc = pAtt->COMGETTER(Medium)(pSrcMedium.asOutParam());
            if (FAILED(rc)) return rc;

            if (pSrcMedium.isNull())
                continue;

            MEDIUMTASKCHAIN mtc;
            mtc.devType       = type;
            mtc.fCreateDiffs  = fCreateDiffs;
            mtc.fAttachLinked = fAttachLinked;

            while (!pSrcMedium.isNull())
            {
                /* Build a histogram of used medias and the parent chain. */
                ++mediaHist[pSrcMedium];

                /* Refresh the state so that the file size get read. */
                MediumState_T e;
                rc = pSrcMedium->RefreshState(&e);
                if (FAILED(rc)) return rc;
                LONG64 lSize;
                rc = pSrcMedium->COMGETTER(Size)(&lSize);
                if (FAILED(rc)) return rc;

                MEDIUMTASK mt;
                mt.uIdx    = UINT32_MAX;
                mt.pMedium = pSrcMedium;
                mt.uWeight = (ULONG)((lSize + _1M - 1) / _1M);
                mtc.chain.append(mt);

                /* Query next parent. */
                rc = pSrcMedium->COMGETTER(Parent)(pSrcMedium.asOutParam());
                if (FAILED(rc)) return rc;
            }

            llMedias.append(mtc);
        }
        /* Add the save state files of this machine if there is one. */
        rc = addSaveState(machine, false /*fAttachCurrent*/, uCount, uTotalWeight);
        if (FAILED(rc)) return rc;
        /* If this is the newly created current state, make sure that the
         * saved state is also attached to it. */
        if (fCreateDiffs)
        {
            rc = addSaveState(machine, true /*fAttachCurrent*/, uCount, uTotalWeight);
            if (FAILED(rc)) return rc;
        }
    }
    /* Build up the index list of the image chain. Unfortunately we can't do
     * that in the previous loop, cause there we go from child -> parent and
     * didn't know how many are between. */
    for (size_t i = 0; i < llMedias.size(); ++i)
    {
        uint32_t uIdx = 0;
        MEDIUMTASKCHAIN &mtc = llMedias.at(i);
        for (size_t a = mtc.chain.size(); a > 0; --a)
            mtc.chain[a - 1].uIdx = uIdx++;
    }
#ifdef DEBUG_poetzsch
    /* Print the histogram */
    std::map<ComPtr<IMedium>, uint32_t>::iterator it;
    for (it = mediaHist.begin(); it != mediaHist.end(); ++it)
    {
        Bstr bstrSrcName;
        rc = (*it).first->COMGETTER(Name)(bstrSrcName.asOutParam());
        if (FAILED(rc)) return rc;
        RTPrintf("%ls: %d\n", bstrSrcName.raw(), (*it).second);
    }
#endif
    /* Go over every medium in the list and check if it either a directly
     * attached disk or has more than one children. If so it needs to be
     * replicated. Also we have to make sure that any direct or indirect
     * children knows of the new parent (which doesn't necessarily mean it
     * is a direct children in the source chain). */
    for (size_t i = 0; i < llMedias.size(); ++i)
    {
        MEDIUMTASKCHAIN &mtc = llMedias.at(i);
        RTCList<MEDIUMTASK> newChain;
        uint32_t used = 0;
        for (size_t a = 0; a < mtc.chain.size(); ++a)
        {
            const MEDIUMTASK &mt = mtc.chain.at(a);
            uint32_t hist = mediaHist[mt.pMedium];
#ifdef DEBUG_poetzsch
            Bstr bstrSrcName;
            rc = mt.pMedium->COMGETTER(Name)(bstrSrcName.asOutParam());
            if (FAILED(rc)) return rc;
            RTPrintf("%ls: %d (%d)\n", bstrSrcName.raw(), hist, used);
#endif
            /* Check if there is a "step" in the histogram when going the chain
             * upwards. If so, we need this image, cause there is another branch
             * from here in the cloned VM. */
            if (hist > used)
            {
                newChain.append(mt);
                used = hist;
            }
        }
        /* Make sure we always using the old base name as new base name, even
         * if the base is a differencing image in the source VM (with the UUID
         * as name). */
        rc = queryBaseName(newChain.last().pMedium, newChain.last().strBaseName);
        if (FAILED(rc)) return rc;
        /* Update the old medium chain with the updated one. */
        mtc.chain = newChain;
        /* Update the progress info. */
        updateProgressStats(mtc, fAttachLinked, uCount, uTotalWeight);
    }

    return rc;
}

HRESULT MachineCloneVMPrivate::queryMediasForAllStates(const RTCList<ComObjPtr<Machine> > &machineList,
                                                       bool fAttachLinked, ULONG &uCount, ULONG &uTotalWeight)
{
    /* In this case we create a exact copy of the original VM. This means just
     * adding all directly and indirectly attached disk images to the worker
     * list. */
    Assert(!fAttachLinked);
    HRESULT rc = S_OK;
    for (size_t i = 0; i < machineList.size(); ++i)
    {
        const ComObjPtr<Machine> &machine = machineList.at(i);
        /* If this is the Snapshot Machine we want to clone, we need to
         * create a new diff file for the new "current state". */
        const bool fCreateDiffs = (machine == pOldMachineState);
        /* Add all attachments (and their parents) of the different
         * machines to a worker list. */
        SafeIfaceArray<IMediumAttachment> sfaAttachments;
        rc = machine->COMGETTER(MediumAttachments)(ComSafeArrayAsOutParam(sfaAttachments));
        if (FAILED(rc)) return rc;
        for (size_t a = 0; a < sfaAttachments.size(); ++a)
        {
            const ComPtr<IMediumAttachment> &pAtt = sfaAttachments[a];
            DeviceType_T type;
            rc = pAtt->COMGETTER(Type)(&type);
            if (FAILED(rc)) return rc;

            /* Only harddisks and floppies are of interest. */
            if (   type != DeviceType_HardDisk
                && type != DeviceType_Floppy)
                continue;

            /* Valid medium attached? */
            ComPtr<IMedium> pSrcMedium;
            rc = pAtt->COMGETTER(Medium)(pSrcMedium.asOutParam());
            if (FAILED(rc)) return rc;

            if (pSrcMedium.isNull())
                continue;

            /* Build up a child->parent list of this attachment. (Note: we are
             * not interested of any child's not attached to this VM. So this
             * will not create a full copy of the base/child relationship.) */
            MEDIUMTASKCHAIN mtc;
            mtc.devType       = type;
            mtc.fCreateDiffs  = fCreateDiffs;
            mtc.fAttachLinked = fAttachLinked;

            while (!pSrcMedium.isNull())
            {
                /* Refresh the state so that the file size get read. */
                MediumState_T e;
                rc = pSrcMedium->RefreshState(&e);
                if (FAILED(rc)) return rc;
                LONG64 lSize;
                rc = pSrcMedium->COMGETTER(Size)(&lSize);
                if (FAILED(rc)) return rc;

                /* Save the current medium, for later cloning. */
                MEDIUMTASK mt;
                mt.uIdx    = UINT32_MAX;
                mt.pMedium = pSrcMedium;
                mt.uWeight = (ULONG)((lSize + _1M - 1) / _1M);
                mtc.chain.append(mt);

                /* Query next parent. */
                rc = pSrcMedium->COMGETTER(Parent)(pSrcMedium.asOutParam());
                if (FAILED(rc)) return rc;
            }
            /* Update the progress info. */
            updateProgressStats(mtc, fAttachLinked, uCount, uTotalWeight);
            /* Append the list of images which have  to be cloned. */
            llMedias.append(mtc);
        }
        /* Add the save state files of this machine if there is one. */
        rc = addSaveState(machine, false /*fAttachCurrent*/, uCount, uTotalWeight);
        if (FAILED(rc)) return rc;
        /* If this is the newly created current state, make sure that the
         * saved state is also attached to it. */
        if (fCreateDiffs)
        {
            rc = addSaveState(machine, true /*fAttachCurrent*/, uCount, uTotalWeight);
            if (FAILED(rc)) return rc;
        }
    }
    /* Build up the index list of the image chain. Unfortunately we can't do
     * that in the previous loop, cause there we go from child -> parent and
     * didn't know how many are between. */
    for (size_t i = 0; i < llMedias.size(); ++i)
    {
        uint32_t uIdx = 0;
        MEDIUMTASKCHAIN &mtc = llMedias.at(i);
        for (size_t a = mtc.chain.size(); a > 0; --a)
            mtc.chain[a - 1].uIdx = uIdx++;
    }

    return rc;
}

bool MachineCloneVMPrivate::findSnapshot(const settings::SnapshotsList &snl, const Guid &id, settings::Snapshot &sn) const
{
    settings::SnapshotsList::const_iterator it;
    for (it = snl.begin(); it != snl.end(); ++it)
    {
        if (it->uuid == id)
        {
            sn = (*it);
            return true;
        }
        else if (!it->llChildSnapshots.empty())
        {
            if (findSnapshot(it->llChildSnapshots, id, sn))
                return true;
        }
    }
    return false;
}

void MachineCloneVMPrivate::updateMACAddresses(settings::NetworkAdaptersList &nwl) const
{
    const bool fNotNAT = options.contains(CloneOptions_KeepNATMACs);
    settings::NetworkAdaptersList::iterator it;
    for (it = nwl.begin(); it != nwl.end(); ++it)
    {
        if (   fNotNAT
            && it->mode == NetworkAttachmentType_NAT)
            continue;
        Host::i_generateMACAddress(it->strMACAddress);
    }
}

void MachineCloneVMPrivate::updateMACAddresses(settings::SnapshotsList &sl) const
{
    settings::SnapshotsList::iterator it;
    for (it = sl.begin(); it != sl.end(); ++it)
    {
        updateMACAddresses(it->hardware.llNetworkAdapters);
        if (!it->llChildSnapshots.empty())
            updateMACAddresses(it->llChildSnapshots);
    }
}

void MachineCloneVMPrivate::updateStorageLists(settings::StorageControllersList &sc,
                                               const Bstr &bstrOldId, const Bstr &bstrNewId) const
{
    settings::StorageControllersList::iterator it3;
    for (it3 = sc.begin();
         it3 != sc.end();
         ++it3)
    {
        settings::AttachedDevicesList &llAttachments = it3->llAttachedDevices;
        settings::AttachedDevicesList::iterator it4;
        for (it4 = llAttachments.begin();
             it4 != llAttachments.end();
             ++it4)
        {
            if (   (   it4->deviceType == DeviceType_HardDisk
                    || it4->deviceType == DeviceType_Floppy)
                && it4->uuid == bstrOldId)
            {
                it4->uuid = bstrNewId;
            }
        }
    }
}

void MachineCloneVMPrivate::updateSnapshotStorageLists(settings::SnapshotsList &sl, const Bstr &bstrOldId,
                                                       const Bstr &bstrNewId) const
{
    settings::SnapshotsList::iterator it;
    for (  it  = sl.begin();
           it != sl.end();
         ++it)
    {
        updateStorageLists(it->hardware.storage.llStorageControllers, bstrOldId, bstrNewId);
        if (!it->llChildSnapshots.empty())
            updateSnapshotStorageLists(it->llChildSnapshots, bstrOldId, bstrNewId);
    }
}

void MachineCloneVMPrivate::updateStateFile(settings::SnapshotsList &snl, const Guid &id, const Utf8Str &strFile) const
{
    settings::SnapshotsList::iterator it;
    for (it = snl.begin(); it != snl.end(); ++it)
    {
        if (it->uuid == id)
            it->strStateFile = strFile;
        else if (!it->llChildSnapshots.empty())
            updateStateFile(it->llChildSnapshots, id, strFile);
    }
}

HRESULT MachineCloneVMPrivate::createDifferencingMedium(const ComObjPtr<Machine> &pMachine, const ComObjPtr<Medium> &pParent,
                                                        const Utf8Str &strSnapshotFolder, RTCList<ComObjPtr<Medium> > &newMedia,
                                                        ComObjPtr<Medium> *ppDiff) const
{
    HRESULT rc = S_OK;
    try
    {
        // check validity of parent object
        {
            AutoReadLock alock(pParent COMMA_LOCKVAL_SRC_POS);
            Bstr bstrSrcId;
            rc = pParent->COMGETTER(Id)(bstrSrcId.asOutParam());
            if (FAILED(rc)) throw rc;
        }
        ComObjPtr<Medium> diff;
        diff.createObject();
        rc = diff->init(p->i_getVirtualBox(),
                        pParent->i_getPreferredDiffFormat(),
                        Utf8StrFmt("%s%c", strSnapshotFolder.c_str(), RTPATH_DELIMITER),
                                   Guid::Empty /* empty media registry */,
                                   DeviceType_HardDisk);
        if (FAILED(rc)) throw rc;

        MediumLockList *pMediumLockList(new MediumLockList());
        rc = diff->i_createMediumLockList(true /* fFailIfInaccessible */,
                                          diff /* pToLockWrite */,
                                          false /* fMediumLockWriteAll */,
                                          pParent,
                                          *pMediumLockList);
        if (FAILED(rc)) throw rc;
        rc = pMediumLockList->Lock();
        if (FAILED(rc)) throw rc;

        /* this already registers the new diff image */
        rc = pParent->i_createDiffStorage(diff,
                                          pParent->i_getPreferredDiffVariant(),
                                          pMediumLockList,
                                          NULL /* aProgress */,
                                          true /* aWait */);
        delete pMediumLockList;
        if (FAILED(rc)) throw rc;
        /* Remember created medium. */
        newMedia.append(diff);
        *ppDiff = diff;
    }
    catch (HRESULT rc2)
    {
        rc = rc2;
    }
    catch (...)
    {
        rc = VirtualBoxBase::handleUnexpectedExceptions(pMachine, RT_SRC_POS);
    }

    return rc;
}

/* static */
DECLCALLBACK(int) MachineCloneVMPrivate::copyStateFileProgress(unsigned uPercentage, void *pvUser)
{
    ComObjPtr<Progress> pProgress = *static_cast< ComObjPtr<Progress>* >(pvUser);

    BOOL fCanceled = false;
    HRESULT rc = pProgress->COMGETTER(Canceled)(&fCanceled);
    if (FAILED(rc)) return VERR_GENERAL_FAILURE;
    /* If canceled by the user tell it to the copy operation. */
    if (fCanceled) return VERR_CANCELLED;
    /* Set the new process. */
    rc = pProgress->SetCurrentOperationProgress(uPercentage);
    if (FAILED(rc)) return VERR_GENERAL_FAILURE;

    return VINF_SUCCESS;
}

void MachineCloneVMPrivate::updateSnapshotHardwareUUIDs(settings::SnapshotsList &snapshot_list, const Guid &id)
{
    for (settings::SnapshotsList::iterator snapshot_it = snapshot_list.begin();
         snapshot_it != snapshot_list.end();
         ++snapshot_it)
    {
        if (!snapshot_it->hardware.uuid.isValid() || snapshot_it->hardware.uuid.isZero())
            snapshot_it->hardware.uuid = id;
        updateSnapshotHardwareUUIDs(snapshot_it->llChildSnapshots, id);
    }
}

// The public class
/////////////////////////////////////////////////////////////////////////////

MachineCloneVM::MachineCloneVM(ComObjPtr<Machine> pSrcMachine, ComObjPtr<Machine> pTrgMachine, CloneMode_T mode,
                               const RTCList<CloneOptions_T> &opts) :
                               d_ptr(new MachineCloneVMPrivate(this, pSrcMachine, pTrgMachine, mode, opts))
{
}

MachineCloneVM::~MachineCloneVM()
{
    delete d_ptr;
}

HRESULT MachineCloneVM::start(IProgress **pProgress)
{
    DPTR(MachineCloneVM);
    ComObjPtr<Machine> &p = d->p;

    HRESULT rc;
    try
    {
        /** @todo r=klaus this code cannot deal with someone crazy specifying
         * IMachine corresponding to a mutable machine as d->pSrcMachine */
        if (d->pSrcMachine->i_isSessionMachine())
            throw p->setError(E_INVALIDARG, "The source machine is mutable");

        /* Handle the special case that someone is requesting a _full_ clone
         * with all snapshots (and the current state), but uses a snapshot
         * machine (and not the current one) as source machine. In this case we
         * just replace the source (snapshot) machine with the current machine. */
        if (   d->mode == CloneMode_AllStates
            && d->pSrcMachine->i_isSnapshotMachine())
        {
            Bstr bstrSrcMachineId;
            rc = d->pSrcMachine->COMGETTER(Id)(bstrSrcMachineId.asOutParam());
            if (FAILED(rc)) throw rc;
            ComPtr<IMachine> newSrcMachine;
            rc = d->pSrcMachine->i_getVirtualBox()->FindMachine(bstrSrcMachineId.raw(), newSrcMachine.asOutParam());
            if (FAILED(rc)) throw rc;
            d->pSrcMachine = (Machine*)(IMachine*)newSrcMachine;
        }
        bool fSubtreeIncludesCurrent = false;
        ComObjPtr<Machine> pCurrState;
        if (d->mode == CloneMode_MachineAndChildStates)
        {
            if (d->pSrcMachine->i_isSnapshotMachine())
            {
                /* find machine object for current snapshot of current state */
                Bstr bstrSrcMachineId;
                rc = d->pSrcMachine->COMGETTER(Id)(bstrSrcMachineId.asOutParam());
                if (FAILED(rc)) throw rc;
                ComPtr<IMachine> pCurr;
                rc = d->pSrcMachine->i_getVirtualBox()->FindMachine(bstrSrcMachineId.raw(), pCurr.asOutParam());
                if (FAILED(rc)) throw rc;
                if (pCurr.isNull())
                    throw p->setError(VBOX_E_OBJECT_NOT_FOUND);
                pCurrState = (Machine *)(IMachine *)pCurr;
                ComPtr<ISnapshot> pSnapshot;
                rc = pCurrState->COMGETTER(CurrentSnapshot)(pSnapshot.asOutParam());
                if (FAILED(rc)) throw rc;
                if (pSnapshot.isNull())
                    throw p->setError(VBOX_E_OBJECT_NOT_FOUND);
                ComPtr<IMachine> pCurrSnapMachine;
                rc = pSnapshot->COMGETTER(Machine)(pCurrSnapMachine.asOutParam());
                if (FAILED(rc)) throw rc;
                if (pCurrSnapMachine.isNull())
                    throw p->setError(VBOX_E_OBJECT_NOT_FOUND);

                /* now check if there is a parent chain which leads to the
                 * snapshot machine defining the subtree. */
                while (!pSnapshot.isNull())
                {
                    ComPtr<IMachine> pSnapMachine;
                    rc = pSnapshot->COMGETTER(Machine)(pSnapMachine.asOutParam());
                    if (FAILED(rc)) throw rc;
                    if (pSnapMachine.isNull())
                        throw p->setError(VBOX_E_OBJECT_NOT_FOUND);
                    if (pSnapMachine == d->pSrcMachine)
                    {
                        fSubtreeIncludesCurrent = true;
                        break;
                    }
                    rc = pSnapshot->COMGETTER(Parent)(pSnapshot.asOutParam());
                    if (FAILED(rc)) throw rc;
                }
            }
            else
            {
                /* If the subtree is only the Current State simply use the
                 * 'machine' case for cloning. It is easier to understand. */
                d->mode = CloneMode_MachineState;
            }
        }

        /* Lock the target machine early (so nobody mess around with it in the meantime). */
        AutoWriteLock trgLock(d->pTrgMachine COMMA_LOCKVAL_SRC_POS);

        if (d->pSrcMachine->i_isSnapshotMachine())
            d->snapshotId = d->pSrcMachine->i_getSnapshotId();

        /* Add the current machine and all snapshot machines below this machine
         * in a list for further processing. */
        RTCList< ComObjPtr<Machine> > machineList;

        /* Include current state? */
        if (   d->mode == CloneMode_MachineState
            || d->mode == CloneMode_AllStates)
            machineList.append(d->pSrcMachine);
        /* Should be done a depth copy with all child snapshots? */
        if (   d->mode == CloneMode_MachineAndChildStates
            || d->mode == CloneMode_AllStates)
        {
            ULONG cSnapshots = 0;
            rc = d->pSrcMachine->COMGETTER(SnapshotCount)(&cSnapshots);
            if (FAILED(rc)) throw rc;
            if (cSnapshots > 0)
            {
                Utf8Str id;
                if (d->mode == CloneMode_MachineAndChildStates)
                    id = d->snapshotId.toString();
                ComPtr<ISnapshot> pSnapshot;
                rc = d->pSrcMachine->FindSnapshot(Bstr(id).raw(), pSnapshot.asOutParam());
                if (FAILED(rc)) throw rc;
                rc = d->createMachineList(pSnapshot, machineList);
                if (FAILED(rc)) throw rc;
                if (d->mode == CloneMode_MachineAndChildStates)
                {
                    if (fSubtreeIncludesCurrent)
                    {
                        if (pCurrState.isNull())
                            throw p->setError(VBOX_E_OBJECT_NOT_FOUND);
                        machineList.append(pCurrState);
                    }
                    else
                    {
                        rc = pSnapshot->COMGETTER(Machine)(d->pOldMachineState.asOutParam());
                        if (FAILED(rc)) throw rc;
                    }
                }
            }
        }

        /* We have different approaches for getting the medias which needs to
         * be replicated based on the clone mode the user requested (this is
         * mostly about the full clone mode).
         * MachineState:
         * - Only the images which are directly attached to an source VM will
         *   be cloned. Any parent disks in the original chain will be merged
         *   into the final cloned disk.
         * MachineAndChildStates:
         * - In this case we search for images which have more than one
         *   children in the cloned VM or are directly attached to the new VM.
         *   All others will be merged into the remaining images which are
         *   cloned.
         *   This case is the most complicated one and needs several iterations
         *   to make sure we are only cloning images which are really
         *   necessary.
         * AllStates:
         * - All disks which are directly or indirectly attached to the
         *   original VM are cloned.
         *
         * Note: If you change something generic in one of the methods its
         * likely that it need to be changed in the others as well! */
        ULONG uCount       = 2; /* One init task and the machine creation. */
        ULONG uTotalWeight = 2; /* The init task and the machine creation is worth one. */
        bool fAttachLinked = d->options.contains(CloneOptions_Link); /* Linked clones requested? */
        switch (d->mode)
        {
            case CloneMode_MachineState:
                d->queryMediasForMachineState(machineList, fAttachLinked, uCount, uTotalWeight);
                break;
            case CloneMode_MachineAndChildStates:
                d->queryMediasForMachineAndChildStates(machineList, fAttachLinked, uCount, uTotalWeight);
                break;
            case CloneMode_AllStates:
                d->queryMediasForAllStates(machineList, fAttachLinked, uCount, uTotalWeight);
                break;
#ifdef VBOX_WITH_XPCOM_CPP_ENUM_HACK
            case CloneMode_32BitHack: /* (compiler warnings) */
                AssertFailedBreak();
#endif
        }

        /* Now create the progress project, so the user knows whats going on. */
        rc = d->pProgress.createObject();
        if (FAILED(rc)) throw rc;
        rc = d->pProgress->init(p->i_getVirtualBox(),
                                static_cast<IMachine*>(d->pSrcMachine) /* aInitiator */,
                                Bstr(p->tr("Cloning Machine")).raw(),
                                true /* fCancellable */,
                                uCount,
                                uTotalWeight,
                                Bstr(p->tr("Initialize Cloning")).raw(),
                                1);
        if (FAILED(rc)) throw rc;

        int vrc = d->startWorker();

        if (RT_FAILURE(vrc))
            p->setErrorBoth(VBOX_E_IPRT_ERROR, vrc, "Could not create machine clone thread (%Rrc)", vrc);
    }
    catch (HRESULT rc2)
    {
        rc = rc2;
    }

    if (SUCCEEDED(rc))
        d->pProgress.queryInterfaceTo(pProgress);

    return rc;
}

HRESULT MachineCloneVM::run()
{
    DPTR(MachineCloneVM);
    ComObjPtr<Machine> &p = d->p;

    AutoCaller autoCaller(p);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock  srcLock(p COMMA_LOCKVAL_SRC_POS);
    AutoWriteLock trgLock(d->pTrgMachine COMMA_LOCKVAL_SRC_POS);

    HRESULT rc = S_OK;

    /*
     * Todo:
     * - What about log files?
     */

    /* Where should all the media go? */
    Utf8Str strTrgSnapshotFolder;
    Utf8Str strTrgMachineFolder = d->pTrgMachine->i_getSettingsFileFull();
    strTrgMachineFolder.stripFilename();

    RTCList<ComObjPtr<Medium> > newMedia;   /* All created images */
    RTCList<Utf8Str> newFiles;              /* All extra created files (save states, ...) */
    try
    {
        /* Copy all the configuration from this machine to an empty
         * configuration dataset. */
        settings::MachineConfigFile trgMCF = *d->pSrcMachine->mData->pMachineConfigFile;

        /* keep source machine hardware UUID if enabled*/
        if (d->options.contains(CloneOptions_KeepHwUUIDs))
        {
            /* because HW UUIDs must be preserved including snapshots by the option,
             * just fill zero UUIDs with corresponding machine UUID before any snapshot
             * processing will take place, while all uuids are from source machine */
            if (!trgMCF.hardwareMachine.uuid.isValid() || trgMCF.hardwareMachine.uuid.isZero())
                trgMCF.hardwareMachine.uuid = trgMCF.uuid;

            MachineCloneVMPrivate::updateSnapshotHardwareUUIDs(trgMCF.llFirstSnapshot, trgMCF.uuid);
        }


        /* Reset media registry. */
        trgMCF.mediaRegistry.llHardDisks.clear();
        trgMCF.mediaRegistry.llDvdImages.clear();
        trgMCF.mediaRegistry.llFloppyImages.clear();
        /* If we got a valid snapshot id, replace the hardware/storage section
         * with the stuff from the snapshot. */
        settings::Snapshot sn;

        if (d->snapshotId.isValid() && !d->snapshotId.isZero())
            if (!d->findSnapshot(trgMCF.llFirstSnapshot, d->snapshotId, sn))
                throw p->setError(E_FAIL,
                                  p->tr("Could not find data to snapshots '%s'"), d->snapshotId.toString().c_str());

        if (d->mode == CloneMode_MachineState)
        {
            if (sn.uuid.isValid() && !sn.uuid.isZero())
                trgMCF.hardwareMachine = sn.hardware;

            /* Remove any hint on snapshots. */
            trgMCF.llFirstSnapshot.clear();
            trgMCF.uuidCurrentSnapshot.clear();
        }
        else if (   d->mode == CloneMode_MachineAndChildStates
                    && sn.uuid.isValid()
                    && !sn.uuid.isZero())
        {
            if (!d->pOldMachineState.isNull())
            {
                /* Copy the snapshot data to the current machine. */
                trgMCF.hardwareMachine = sn.hardware;

                /* Current state is under root snapshot. */
                trgMCF.uuidCurrentSnapshot = sn.uuid;
            }
            /* The snapshot will be the root one. */
            trgMCF.llFirstSnapshot.clear();
            trgMCF.llFirstSnapshot.push_back(sn);
        }

        /* Generate new MAC addresses for all machines when not forbidden. */
        if (!d->options.contains(CloneOptions_KeepAllMACs))
        {
            d->updateMACAddresses(trgMCF.hardwareMachine.llNetworkAdapters);
            d->updateMACAddresses(trgMCF.llFirstSnapshot);
        }

        /* When the current snapshot folder is absolute we reset it to the
         * default relative folder. */
        if (RTPathStartsWithRoot(trgMCF.machineUserData.strSnapshotFolder.c_str()))
            trgMCF.machineUserData.strSnapshotFolder = "Snapshots";
        trgMCF.strStateFile = "";
        /* Set the new name. */
        const Utf8Str strOldVMName = trgMCF.machineUserData.strName;
        trgMCF.machineUserData.strName = d->pTrgMachine->mUserData->s.strName;
        trgMCF.uuid = d->pTrgMachine->mData->mUuid;

        Bstr bstrSrcSnapshotFolder;
        rc = d->pSrcMachine->COMGETTER(SnapshotFolder)(bstrSrcSnapshotFolder.asOutParam());
        if (FAILED(rc)) throw rc;
        /* The absolute name of the snapshot folder. */
        strTrgSnapshotFolder = Utf8StrFmt("%s%c%s", strTrgMachineFolder.c_str(), RTPATH_DELIMITER,
                                          trgMCF.machineUserData.strSnapshotFolder.c_str());

        /* Should we rename the disk names. */
        bool fKeepDiskNames = d->options.contains(CloneOptions_KeepDiskNames);

        /* We need to create a map with the already created medias. This is
         * necessary, cause different snapshots could have the same
         * parents/parent chain. If a medium is in this map already, it isn't
         * cloned a second time, but simply used. */
        typedef std::map<Utf8Str, ComObjPtr<Medium> > TStrMediumMap;
        typedef std::pair<Utf8Str, ComObjPtr<Medium> > TStrMediumPair;
        TStrMediumMap map;
        size_t cDisks = 0;
        for (size_t i = 0; i < d->llMedias.size(); ++i)
        {
            const MEDIUMTASKCHAIN &mtc = d->llMedias.at(i);
            ComObjPtr<Medium> pNewParent;
            uint32_t uSrcParentIdx = UINT32_MAX;
            uint32_t uTrgParentIdx = UINT32_MAX;
            for (size_t a = mtc.chain.size(); a > 0; --a)
            {
                const MEDIUMTASK &mt = mtc.chain.at(a - 1);
                ComPtr<IMedium> pMedium = mt.pMedium;

                Bstr bstrSrcName;
                rc = pMedium->COMGETTER(Name)(bstrSrcName.asOutParam());
                if (FAILED(rc)) throw rc;

                rc = d->pProgress->SetNextOperation(BstrFmt(p->tr("Cloning Disk '%ls' ..."), bstrSrcName.raw()).raw(),
                                                    mt.uWeight);
                if (FAILED(rc)) throw rc;

                Bstr bstrSrcId;
                rc = pMedium->COMGETTER(Id)(bstrSrcId.asOutParam());
                if (FAILED(rc)) throw rc;

                if (mtc.fAttachLinked)
                {
                    IMedium *pTmp = pMedium;
                    ComObjPtr<Medium> pLMedium = static_cast<Medium*>(pTmp);
                    if (pLMedium.isNull())
                        throw p->setError(VBOX_E_OBJECT_NOT_FOUND);
                    ComObjPtr<Medium> pBase = pLMedium->i_getBase();
                    if (pBase->i_isReadOnly())
                    {
                        ComObjPtr<Medium> pDiff;
                        /* create the diff under the snapshot medium */
                        trgLock.release();
                        srcLock.release();
                        rc = d->createDifferencingMedium(p, pLMedium, strTrgSnapshotFolder,
                                                         newMedia, &pDiff);
                        srcLock.acquire();
                        trgLock.acquire();
                        if (FAILED(rc)) throw rc;
                        map.insert(TStrMediumPair(Utf8Str(bstrSrcId), pDiff));
                        /* diff image has to be used... */
                        pNewParent = pDiff;
                    }
                    else
                    {
                        /* Attach the medium directly, as its type is not
                         * subject to diff creation. */
                        newMedia.append(pLMedium);
                        map.insert(TStrMediumPair(Utf8Str(bstrSrcId), pLMedium));
                        pNewParent = pLMedium;
                    }
                }
                else
                {
                    /* Is a clone already there? */
                    TStrMediumMap::iterator it = map.find(Utf8Str(bstrSrcId));
                    if (it != map.end())
                        pNewParent = it->second;
                    else
                    {
                        ComPtr<IMediumFormat> pSrcFormat;
                        rc = pMedium->COMGETTER(MediumFormat)(pSrcFormat.asOutParam());
                        ULONG uSrcCaps = 0;
                        com::SafeArray <MediumFormatCapabilities_T> mediumFormatCap;
                        rc = pSrcFormat->COMGETTER(Capabilities)(ComSafeArrayAsOutParam(mediumFormatCap));

                        if (FAILED(rc)) throw rc;
                        else
                        {
                            for (ULONG j = 0; j < mediumFormatCap.size(); j++)
                                uSrcCaps |= mediumFormatCap[j];
                        }

                        /* Default format? */
                        Utf8Str strDefaultFormat;
                        if (mtc.devType == DeviceType_HardDisk)
                            p->mParent->i_getDefaultHardDiskFormat(strDefaultFormat);
                        else
                            strDefaultFormat = "RAW";

                        Bstr bstrSrcFormat(strDefaultFormat);

                        ULONG srcVar = MediumVariant_Standard;
                        com::SafeArray <MediumVariant_T> mediumVariant;

                        /* Is the source file based? */
                        if ((uSrcCaps & MediumFormatCapabilities_File) == MediumFormatCapabilities_File)
                        {
                            /* Yes, just use the source format. Otherwise the defaults
                             * will be used. */
                            rc = pMedium->COMGETTER(Format)(bstrSrcFormat.asOutParam());
                            if (FAILED(rc)) throw rc;

                            rc = pMedium->COMGETTER(Variant)(ComSafeArrayAsOutParam(mediumVariant));
                            if (FAILED(rc)) throw rc;
                            else
                            {
                                for (size_t j = 0; j < mediumVariant.size(); j++)
                                    srcVar |= mediumVariant[j];
                            }
                        }

                        Guid newId;
                        newId.create();
                        Utf8Str strNewName(bstrSrcName);
                        if (!fKeepDiskNames)
                        {
                            Utf8Str strSrcTest = bstrSrcName;
                            /* Check if we have to use another name. */
                            if (!mt.strBaseName.isEmpty())
                                strSrcTest = mt.strBaseName;
                            strSrcTest.stripSuffix();
                            /* If the old disk name was in {uuid} format we also
                             * want the new name in this format, but with the
                             * updated id of course. If the old disk was called
                             * like the VM name, we change it to the new VM name.
                             * For all other disks we rename them with this
                             * template: "new name-disk1.vdi". */
                            if (strSrcTest == strOldVMName)
                                strNewName = Utf8StrFmt("%s%s", trgMCF.machineUserData.strName.c_str(),
                                                        RTPathSuffix(Utf8Str(bstrSrcName).c_str()));
                            else if (   strSrcTest.startsWith("{")
                                     && strSrcTest.endsWith("}"))
                            {
                                strSrcTest = strSrcTest.substr(1, strSrcTest.length() - 2);

                                Guid temp_guid(strSrcTest);
                                if (temp_guid.isValid() && !temp_guid.isZero())
                                    strNewName = Utf8StrFmt("%s%s", newId.toStringCurly().c_str(),
                                                            RTPathSuffix(strNewName.c_str()));
                            }
                            else
                                strNewName = Utf8StrFmt("%s-disk%d%s", trgMCF.machineUserData.strName.c_str(), ++cDisks,
                                                        RTPathSuffix(Utf8Str(bstrSrcName).c_str()));
                        }

                        /* Check if this medium comes from the snapshot folder, if
                         * so, put it there in the cloned machine as well.
                         * Otherwise it goes to the machine folder. */
                        Bstr bstrSrcPath;
                        Utf8Str strFile = Utf8StrFmt("%s%c%s", strTrgMachineFolder.c_str(), RTPATH_DELIMITER, strNewName.c_str());
                        rc = pMedium->COMGETTER(Location)(bstrSrcPath.asOutParam());
                        if (FAILED(rc)) throw rc;
                        if (   !bstrSrcPath.isEmpty()
                            &&  RTPathStartsWith(Utf8Str(bstrSrcPath).c_str(), Utf8Str(bstrSrcSnapshotFolder).c_str())
                            && (fKeepDiskNames || mt.strBaseName.isEmpty()))
                            strFile = Utf8StrFmt("%s%c%s", strTrgSnapshotFolder.c_str(), RTPATH_DELIMITER, strNewName.c_str());

                        /* Start creating the clone. */
                        ComObjPtr<Medium> pTarget;
                        rc = pTarget.createObject();
                        if (FAILED(rc)) throw rc;

                        rc = pTarget->init(p->mParent,
                                           Utf8Str(bstrSrcFormat),
                                           strFile,
                                           Guid::Empty /* empty media registry */,
                                           mtc.devType);
                        if (FAILED(rc)) throw rc;

                        /* Update the new uuid. */
                        pTarget->i_updateId(newId);

                        /* Do the disk cloning. */
                        ComPtr<IProgress> progress2;

                        ComObjPtr<Medium> pLMedium = static_cast<Medium*>((IMedium*)pMedium);
                        srcLock.release();
                        rc = pLMedium->i_cloneToEx(pTarget,
                                                   (MediumVariant_T)srcVar,
                                                   pNewParent,
                                                   progress2.asOutParam(),
                                                   uSrcParentIdx,
                                                   uTrgParentIdx);
                        srcLock.acquire();
                        if (FAILED(rc)) throw rc;

                        /* Wait until the async process has finished. */
                        srcLock.release();
                        rc = d->pProgress->WaitForAsyncProgressCompletion(progress2);
                        srcLock.acquire();
                        if (FAILED(rc)) throw rc;

                        /* Check the result of the async process. */
                        LONG iRc;
                        rc = progress2->COMGETTER(ResultCode)(&iRc);
                        if (FAILED(rc)) throw rc;
                        /* If the thread of the progress object has an error, then
                         * retrieve the error info from there, or it'll be lost. */
                        if (FAILED(iRc))
                            throw p->setError(ProgressErrorInfo(progress2));
                        /* Remember created medium. */
                        newMedia.append(pTarget);
                        /* Get the medium type from the source and set it to the
                         * new medium. */
                        MediumType_T type;
                        rc = pMedium->COMGETTER(Type)(&type);
                        if (FAILED(rc)) throw rc;
                        trgLock.release();
                        srcLock.release();
                        rc = pTarget->COMSETTER(Type)(type);
                        srcLock.acquire();
                        trgLock.acquire();
                        if (FAILED(rc)) throw rc;
                        map.insert(TStrMediumPair(Utf8Str(bstrSrcId), pTarget));
                        /* register the new medium */
                        {
                            AutoWriteLock tlock(p->mParent->i_getMediaTreeLockHandle() COMMA_LOCKVAL_SRC_POS);
                            rc = p->mParent->i_registerMedium(pTarget, &pTarget,
                                                              tlock);
                            if (FAILED(rc)) throw rc;
                        }
                        /* This medium becomes the parent of the next medium in the
                         * chain. */
                        pNewParent = pTarget;
                    }
                }
                /* Save the current source medium index as the new parent
                 * medium index. */
                uSrcParentIdx = mt.uIdx;
                /* Simply increase the target index. */
                ++uTrgParentIdx;
            }

            Bstr bstrSrcId;
            rc = mtc.chain.first().pMedium->COMGETTER(Id)(bstrSrcId.asOutParam());
            if (FAILED(rc)) throw rc;
            Bstr bstrTrgId;
            rc = pNewParent->COMGETTER(Id)(bstrTrgId.asOutParam());
            if (FAILED(rc)) throw rc;
            /* update snapshot configuration */
            d->updateSnapshotStorageLists(trgMCF.llFirstSnapshot, bstrSrcId, bstrTrgId);

            /* create new 'Current State' diff for caller defined place */
            if (mtc.fCreateDiffs)
            {
                const MEDIUMTASK &mt = mtc.chain.first();
                ComObjPtr<Medium> pLMedium = static_cast<Medium*>((IMedium*)mt.pMedium);
                if (pLMedium.isNull())
                    throw p->setError(VBOX_E_OBJECT_NOT_FOUND);
                ComObjPtr<Medium> pBase = pLMedium->i_getBase();
                if (pBase->i_isReadOnly())
                {
                    ComObjPtr<Medium> pDiff;
                    trgLock.release();
                    srcLock.release();
                    rc = d->createDifferencingMedium(p, pNewParent, strTrgSnapshotFolder,
                                                     newMedia, &pDiff);
                    srcLock.acquire();
                    trgLock.acquire();
                    if (FAILED(rc)) throw rc;
                    /* diff image has to be used... */
                    pNewParent = pDiff;
                }
                else
                {
                    /* Attach the medium directly, as its type is not
                     * subject to diff creation. */
                    newMedia.append(pNewParent);
                }

                rc = pNewParent->COMGETTER(Id)(bstrTrgId.asOutParam());
                if (FAILED(rc)) throw rc;
            }
            /* update 'Current State' configuration */
            d->updateStorageLists(trgMCF.hardwareMachine.storage.llStorageControllers, bstrSrcId, bstrTrgId);
        }
        /* Make sure all disks know of the new machine uuid. We do this last to
         * be able to change the medium type above. */
        for (size_t i = newMedia.size(); i > 0; --i)
        {
            const ComObjPtr<Medium> &pMedium = newMedia.at(i - 1);
            AutoCaller mac(pMedium);
            if (FAILED(mac.rc())) throw mac.rc();
            AutoWriteLock mlock(pMedium COMMA_LOCKVAL_SRC_POS);
            Guid uuid = d->pTrgMachine->mData->mUuid;
            if (d->options.contains(CloneOptions_Link))
            {
                ComObjPtr<Medium> pParent = pMedium->i_getParent();
                mlock.release();
                if (!pParent.isNull())
                {
                    AutoCaller mac2(pParent);
                    if (FAILED(mac2.rc())) throw mac2.rc();
                    AutoReadLock mlock2(pParent COMMA_LOCKVAL_SRC_POS);
                    if (pParent->i_getFirstRegistryMachineId(uuid))
                    {
                        mlock2.release();
                        trgLock.release();
                        srcLock.release();
                        p->mParent->i_markRegistryModified(uuid);
                        srcLock.acquire();
                        trgLock.acquire();
                        mlock2.acquire();
                    }
                }
                mlock.acquire();
            }
            pMedium->i_removeRegistry(p->i_getVirtualBox()->i_getGlobalRegistryId());
            pMedium->i_addRegistry(uuid);
        }
        /* Check if a snapshot folder is necessary and if so doesn't already
         * exists. */
        if (   !d->llSaveStateFiles.isEmpty()
            && !RTDirExists(strTrgSnapshotFolder.c_str()))
        {
            int vrc = RTDirCreateFullPath(strTrgSnapshotFolder.c_str(), 0700);
            if (RT_FAILURE(vrc))
                throw p->setErrorBoth(VBOX_E_IPRT_ERROR, vrc,
                                      p->tr("Could not create snapshots folder '%s' (%Rrc)"),
                                            strTrgSnapshotFolder.c_str(), vrc);
        }
        /* Clone all save state files. */
        for (size_t i = 0; i < d->llSaveStateFiles.size(); ++i)
        {
            SAVESTATETASK sst = d->llSaveStateFiles.at(i);
            const Utf8Str &strTrgSaveState = Utf8StrFmt("%s%c%s", strTrgSnapshotFolder.c_str(), RTPATH_DELIMITER,
                                                        RTPathFilename(sst.strSaveStateFile.c_str()));

            /* Move to next sub-operation. */
            rc = d->pProgress->SetNextOperation(BstrFmt(p->tr("Copy save state file '%s' ..."),
                                                        RTPathFilename(sst.strSaveStateFile.c_str())).raw(), sst.uWeight);
            if (FAILED(rc)) throw rc;
            /* Copy the file only if it was not copied already. */
            if (!newFiles.contains(strTrgSaveState.c_str()))
            {
                int vrc = RTFileCopyEx(sst.strSaveStateFile.c_str(), strTrgSaveState.c_str(), 0,
                                       MachineCloneVMPrivate::copyStateFileProgress, &d->pProgress);
                if (RT_FAILURE(vrc))
                    throw p->setErrorBoth(VBOX_E_IPRT_ERROR, vrc,
                                          p->tr("Could not copy state file '%s' to '%s' (%Rrc)"),
                                          sst.strSaveStateFile.c_str(), strTrgSaveState.c_str(), vrc);
                newFiles.append(strTrgSaveState);
            }
            /* Update the path in the configuration either for the current
             * machine state or the snapshots. */
            if (!sst.snapshotUuid.isValid() || sst.snapshotUuid.isZero())
                trgMCF.strStateFile = strTrgSaveState;
            else
                d->updateStateFile(trgMCF.llFirstSnapshot, sst.snapshotUuid, strTrgSaveState);
        }

        {
            rc = d->pProgress->SetNextOperation(BstrFmt(p->tr("Create Machine Clone '%s' ..."),
                                                trgMCF.machineUserData.strName.c_str()).raw(), 1);
            if (FAILED(rc)) throw rc;
            /* After modifying the new machine config, we can copy the stuff
             * over to the new machine. The machine have to be mutable for
             * this. */
            rc = d->pTrgMachine->i_checkStateDependency(p->MutableStateDep);
            if (FAILED(rc)) throw rc;
            rc = d->pTrgMachine->i_loadMachineDataFromSettings(trgMCF, &d->pTrgMachine->mData->mUuid);
            if (FAILED(rc)) throw rc;

            /* Fix up the "current state modified" flag to what it should be,
             * as the value guessed in i_loadMachineDataFromSettings can be
             * quite far off the logical value for the cloned VM. */
            if (d->mode == CloneMode_MachineState)
                d->pTrgMachine->mData->mCurrentStateModified = FALSE;
            else if (   d->mode == CloneMode_MachineAndChildStates
                && sn.uuid.isValid()
                && !sn.uuid.isZero())
            {
                if (!d->pOldMachineState.isNull())
                {
                    /* There will be created a new differencing image based on
                     * this snapshot. So reset the modified state. */
                    d->pTrgMachine->mData->mCurrentStateModified = FALSE;
                }
                else
                    d->pTrgMachine->mData->mCurrentStateModified = p->mData->mCurrentStateModified;
            }
            else if (d->mode == CloneMode_AllStates)
                d->pTrgMachine->mData->mCurrentStateModified = p->mData->mCurrentStateModified;

            /* If the target machine has saved state we MUST adjust the machine
             * state, otherwise saving settings will drop the information. */
            if (trgMCF.strStateFile.isNotEmpty())
                d->pTrgMachine->i_setMachineState(MachineState_Saved);

            /* save all VM data */
            bool fNeedsGlobalSaveSettings = false;
            rc = d->pTrgMachine->i_saveSettings(&fNeedsGlobalSaveSettings, Machine::SaveS_Force);
            if (FAILED(rc)) throw rc;
            /* Release all locks */
            trgLock.release();
            srcLock.release();
            if (fNeedsGlobalSaveSettings)
            {
                /* save the global settings; for that we should hold only the
                 * VirtualBox lock */
                AutoWriteLock vlock(p->mParent COMMA_LOCKVAL_SRC_POS);
                rc = p->mParent->i_saveSettings();
                if (FAILED(rc)) throw rc;
            }
        }

        /* Any additional machines need saving? */
        p->mParent->i_saveModifiedRegistries();
    }
    catch (HRESULT rc2)
    {
        /* Error handling code only works correctly without locks held. */
        trgLock.release();
        srcLock.release();
        rc = rc2;
    }
    catch (...)
    {
        rc = VirtualBoxBase::handleUnexpectedExceptions(p, RT_SRC_POS);
    }

    MultiResult mrc(rc);
    /* Cleanup on failure (CANCEL also) */
    if (FAILED(rc))
    {
        int vrc = VINF_SUCCESS;
        /* Delete all created files. */
        for (size_t i = 0; i < newFiles.size(); ++i)
        {
            vrc = RTFileDelete(newFiles.at(i).c_str());
            if (RT_FAILURE(vrc))
                mrc = p->setErrorBoth(VBOX_E_IPRT_ERROR, vrc,
                                      p->tr("Could not delete file '%s' (%Rrc)"), newFiles.at(i).c_str(), vrc);
        }
        /* Delete all already created medias. (Reverse, cause there could be
         * parent->child relations.) */
        for (size_t i = newMedia.size(); i > 0; --i)
        {
            const ComObjPtr<Medium> &pMedium = newMedia.at(i - 1);
            mrc = pMedium->i_deleteStorage(NULL /* aProgress */,
                                           true /* aWait */);
            pMedium->Close();
        }
        /* Delete the snapshot folder when not empty. */
        if (!strTrgSnapshotFolder.isEmpty())
            RTDirRemove(strTrgSnapshotFolder.c_str());
        /* Delete the machine folder when not empty. */
        RTDirRemove(strTrgMachineFolder.c_str());

        /* Must save the modified registries */
        p->mParent->i_saveModifiedRegistries();
    }

    return mrc;
}

void MachineCloneVM::destroy()
{
    delete this;
}

