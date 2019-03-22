/* $Id$ */
/** @file
 * Implementation of IMachine in VBoxSVC - Header.
 */

/*
 * Copyright (C) 2006-2019 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef MAIN_INCLUDED_MachineImpl_h
#define MAIN_INCLUDED_MachineImpl_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include "AuthLibrary.h"
#include "VirtualBoxBase.h"
#include "SnapshotImpl.h"
#include "ProgressImpl.h"
#include "VRDEServerImpl.h"
#include "MediumAttachmentImpl.h"
#include "PCIDeviceAttachmentImpl.h"
#include "MediumLock.h"
#include "NetworkAdapterImpl.h"
#include "AudioAdapterImpl.h"
#include "SerialPortImpl.h"
#include "ParallelPortImpl.h"
#include "BIOSSettingsImpl.h"
#include "RecordingSettingsImpl.h"
#include "StorageControllerImpl.h"          // required for MachineImpl.h to compile on Windows
#include "USBControllerImpl.h"              // required for MachineImpl.h to compile on Windows
#include "BandwidthControlImpl.h"
#include "BandwidthGroupImpl.h"
#ifdef VBOX_WITH_RESOURCE_USAGE_API
# include "Performance.h"
# include "PerformanceImpl.h"
# include "ThreadTask.h"
#endif

// generated header
#include "SchemaDefs.h"

#include "VBox/com/ErrorInfo.h"

#include <iprt/file.h>
#include <iprt/thread.h>
#include <iprt/time.h>

#include <list>
#include <vector>

#include "MachineWrap.h"

/** @todo r=klaus after moving the various Machine settings structs to
 * MachineImpl.cpp it should be possible to eliminate this include. */
#include <VBox/settings.h>

// defines
////////////////////////////////////////////////////////////////////////////////

// helper declarations
////////////////////////////////////////////////////////////////////////////////

class Progress;
class ProgressProxy;
class Keyboard;
class Mouse;
class Display;
class MachineDebugger;
class USBController;
class USBDeviceFilters;
class Snapshot;
class SharedFolder;
class HostUSBDevice;
class StorageController;
class SessionMachine;
#ifdef VBOX_WITH_UNATTENDED
class Unattended;
#endif

// Machine class
////////////////////////////////////////////////////////////////////////////////
//
class ATL_NO_VTABLE Machine :
    public MachineWrap
{

public:

    enum StateDependency
    {
        AnyStateDep = 0,
        MutableStateDep,
        MutableOrSavedStateDep,
        MutableOrRunningStateDep,
        MutableOrSavedOrRunningStateDep,
    };

    /**
     * Internal machine data.
     *
     * Only one instance of this data exists per every machine -- it is shared
     * by the Machine, SessionMachine and all SnapshotMachine instances
     * associated with the given machine using the util::Shareable template
     * through the mData variable.
     *
     * @note |const| members are persistent during lifetime so can be
     * accessed without locking.
     *
     * @note There is no need to lock anything inside init() or uninit()
     * methods, because they are always serialized (see AutoCaller).
     */
    struct Data
    {
        /**
         * Data structure to hold information about sessions opened for the
         * given machine.
         */
        struct Session
        {
            /** Type of lock which created this session */
            LockType_T mLockType;

            /** Control of the direct session opened by lockMachine() */
            ComPtr<IInternalSessionControl> mDirectControl;

            typedef std::list<ComPtr<IInternalSessionControl> > RemoteControlList;

            /** list of controls of all opened remote sessions */
            RemoteControlList mRemoteControls;

            /** launchVMProcess() and OnSessionEnd() progress indicator */
            ComObjPtr<ProgressProxy> mProgress;

            /**
             * PID of the session object that must be passed to openSession()
             * to finalize the launchVMProcess() request (i.e., PID of the
             * process created by launchVMProcess())
             */
            RTPROCESS mPID;

            /** Current session state */
            SessionState_T mState;

            /** Session name string (of the primary session) */
            Utf8Str mName;

            /** Session machine object */
            ComObjPtr<SessionMachine> mMachine;

            /** Medium object lock collection. */
            MediumLockListMap mLockedMedia;
        };

        Data();
        ~Data();

        const Guid          mUuid;
        BOOL                mRegistered;

        Utf8Str             m_strConfigFile;
        Utf8Str             m_strConfigFileFull;

        // machine settings XML file
        settings::MachineConfigFile *pMachineConfigFile;
        uint32_t            flModifications;
        bool                m_fAllowStateModification;

        BOOL                mAccessible;
        com::ErrorInfo      mAccessError;

        MachineState_T      mMachineState;
        RTTIMESPEC          mLastStateChange;

        /* Note: These are guarded by VirtualBoxBase::stateLockHandle() */
        uint32_t            mMachineStateDeps;
        RTSEMEVENTMULTI     mMachineStateDepsSem;
        uint32_t            mMachineStateChangePending;

        BOOL                mCurrentStateModified;
        /** Guest properties have been modified and need saving since the
         * machine was started, or there are transient properties which need
         * deleting and the machine is being shut down. */
        BOOL                mGuestPropertiesModified;

        Session             mSession;

        ComObjPtr<Snapshot> mFirstSnapshot;
        ComObjPtr<Snapshot> mCurrentSnapshot;

        // list of files to delete in Delete(); this list is filled by Unregister()
        std::list<Utf8Str>  llFilesToDelete;
    };

    /**
     *  Saved state data.
     *
     *  It's actually only the state file path string, but it needs to be
     *  separate from Data, because Machine and SessionMachine instances
     *  share it, while SnapshotMachine does not.
     *
     *  The data variable is |mSSData|.
     */
    struct SSData
    {
        Utf8Str strStateFilePath;
    };

    /**
     *  User changeable machine data.
     *
     *  This data is common for all machine snapshots, i.e. it is shared
     *  by all SnapshotMachine instances associated with the given machine
     *  using the util::Backupable template through the |mUserData| variable.
     *
     *  SessionMachine instances can alter this data and discard changes.
     *
     *  @note There is no need to lock anything inside init() or uninit()
     *  methods, because they are always serialized (see AutoCaller).
     */
    struct UserData
    {
        settings::MachineUserData s;
    };

    /**
     *  Hardware data.
     *
     *  This data is unique for a machine and for every machine snapshot.
     *  Stored using the util::Backupable template in the |mHWData| variable.
     *
     *  SessionMachine instances can alter this data and discard changes.
     *
     *  @todo r=klaus move all "pointer" objects out of this struct, as they
     *  need non-obvious handling when creating a new session or when taking
     *  a snapshot. Better do this right straight away, not relying on the
     *  template magic which doesn't work right in this case.
     */
    struct HWData
    {
        /**
         * Data structure to hold information about a guest property.
         */
        struct GuestProperty {
            /** Property value */
            Utf8Str strValue;
            /** Property timestamp */
            LONG64 mTimestamp;
            /** Property flags */
            ULONG mFlags;
        };

        HWData();
        ~HWData();

        Bstr                mHWVersion;
        Guid                mHardwareUUID;  /**< If Null, use mData.mUuid. */
        ULONG               mMemorySize;
        ULONG               mMemoryBalloonSize;
        BOOL                mPageFusionEnabled;
        GraphicsControllerType_T mGraphicsControllerType;
        ULONG               mVRAMSize;
        settings::RecordingSettings mRecordSettings;
        ULONG               mMonitorCount;
        BOOL                mHWVirtExEnabled;
        BOOL                mHWVirtExNestedPagingEnabled;
        BOOL                mHWVirtExLargePagesEnabled;
        BOOL                mHWVirtExVPIDEnabled;
        BOOL                mHWVirtExUXEnabled;
        BOOL                mHWVirtExForceEnabled;
        BOOL                mHWVirtExUseNativeApi;
        BOOL                mAccelerate2DVideoEnabled;
        BOOL                mPAEEnabled;
        settings::Hardware::LongModeType mLongMode;
        BOOL                mTripleFaultReset;
        BOOL                mAPIC;
        BOOL                mX2APIC;
        BOOL                mIBPBOnVMExit;
        BOOL                mIBPBOnVMEntry;
        BOOL                mSpecCtrl;
        BOOL                mSpecCtrlByHost;
        BOOL                mL1DFlushOnSched;
        BOOL                mL1DFlushOnVMEntry;
        BOOL                mNestedHWVirt;
        ULONG               mCPUCount;
        BOOL                mCPUHotPlugEnabled;
        ULONG               mCpuExecutionCap;
        uint32_t            mCpuIdPortabilityLevel;
        Utf8Str             mCpuProfile;
        BOOL                mAccelerate3DEnabled;
        BOOL                mHPETEnabled;

        BOOL                mCPUAttached[SchemaDefs::MaxCPUCount];

        std::list<settings::CpuIdLeaf> mCpuIdLeafList;

        DeviceType_T        mBootOrder[SchemaDefs::MaxBootPosition];

        typedef std::list<ComObjPtr<SharedFolder> > SharedFolderList;
        SharedFolderList    mSharedFolders;

        ClipboardMode_T     mClipboardMode;
        DnDMode_T           mDnDMode;

        typedef std::map<Utf8Str, GuestProperty> GuestPropertyMap;
        GuestPropertyMap    mGuestProperties;

        FirmwareType_T      mFirmwareType;
        KeyboardHIDType_T   mKeyboardHIDType;
        PointingHIDType_T   mPointingHIDType;
        ChipsetType_T       mChipsetType;
        ParavirtProvider_T  mParavirtProvider;
        Utf8Str             mParavirtDebug;
        BOOL                mEmulatedUSBCardReaderEnabled;

        BOOL                mIOCacheEnabled;
        ULONG               mIOCacheSize;

        typedef std::list<ComObjPtr<PCIDeviceAttachment> > PCIDeviceAssignmentList;
        PCIDeviceAssignmentList mPCIDeviceAssignments;

        settings::Debugging mDebugging;
        settings::Autostart mAutostart;

        Utf8Str             mDefaultFrontend;
    };

    typedef std::list<ComObjPtr<MediumAttachment> > MediumAttachmentList;

    DECLARE_EMPTY_CTOR_DTOR(Machine)

    HRESULT FinalConstruct();
    void FinalRelease();

    // public initializer/uninitializer for internal purposes only:

    // initializer for creating a new, empty machine
    HRESULT init(VirtualBox *aParent,
                 const Utf8Str &strConfigFile,
                 const Utf8Str &strName,
                 const StringsList &llGroups,
                 const Utf8Str &strOsTypeId,
                 GuestOSType *aOsType,
                 const Guid &aId,
                 bool fForceOverwrite,
                 bool fDirectoryIncludesUUID);

    // initializer for loading existing machine XML (either registered or not)
    HRESULT initFromSettings(VirtualBox *aParent,
                             const Utf8Str &strConfigFile,
                             const Guid *aId);

    // initializer for machine config in memory (OVF import)
    HRESULT init(VirtualBox *aParent,
                 const Utf8Str &strName,
                 const Utf8Str &strSettingsFilename,
                 const settings::MachineConfigFile &config);

    void uninit();

#ifdef VBOX_WITH_RESOURCE_USAGE_API
    // Needed from VirtualBox, for the delayed metrics cleanup.
    void i_unregisterMetrics(PerformanceCollector *aCollector, Machine *aMachine);
#endif /* VBOX_WITH_RESOURCE_USAGE_API */

protected:
    HRESULT initImpl(VirtualBox *aParent,
                     const Utf8Str &strConfigFile);
    HRESULT initDataAndChildObjects();
    HRESULT i_registeredInit();
    HRESULT i_tryCreateMachineConfigFile(bool fForceOverwrite);
    void uninitDataAndChildObjects();

public:


    // public methods only for internal purposes

    virtual bool i_isSnapshotMachine() const
    {
        return false;
    }

    virtual bool i_isSessionMachine() const
    {
        return false;
    }

    /**
     * Override of the default locking class to be used for validating lock
     * order with the standard member lock handle.
     */
    virtual VBoxLockingClass getLockingClass() const
    {
        return LOCKCLASS_MACHINEOBJECT;
    }

    /// @todo (dmik) add lock and make non-inlined after revising classes
    //  that use it. Note: they should enter Machine lock to keep the returned
    //  information valid!
    bool i_isRegistered() { return !!mData->mRegistered; }

    // unsafe inline public methods for internal purposes only (ensure there is
    // a caller and a read lock before calling them!)

    /**
     * Returns the VirtualBox object this machine belongs to.
     *
     * @note This method doesn't check this object's readiness. Intended to be
     * used by ready Machine children (whose readiness is bound to the parent's
     * one) or after doing addCaller() manually.
     */
    VirtualBox* i_getVirtualBox() const { return mParent; }

    /**
     * Checks if this machine is accessible, without attempting to load the
     * config file.
     *
     * @note This method doesn't check this object's readiness. Intended to be
     * used by ready Machine children (whose readiness is bound to the parent's
     * one) or after doing addCaller() manually.
     */
    bool i_isAccessible() const { return !!mData->mAccessible; }

    /**
     * Returns this machine ID.
     *
     * @note This method doesn't check this object's readiness. Intended to be
     * used by ready Machine children (whose readiness is bound to the parent's
     * one) or after adding a caller manually.
     */
    const Guid& i_getId() const { return mData->mUuid; }

    /**
     * Returns the snapshot ID this machine represents or an empty UUID if this
     * instance is not SnapshotMachine.
     *
     * @note This method doesn't check this object's readiness. Intended to be
     * used by ready Machine children (whose readiness is bound to the parent's
     * one) or after adding a caller manually.
     */
    inline const Guid& i_getSnapshotId() const;

    /**
     * Returns this machine's full settings file path.
     *
     * @note This method doesn't lock this object or check its readiness.
     * Intended to be used only after doing addCaller() manually and locking it
     * for reading.
     */
    const Utf8Str& i_getSettingsFileFull() const { return mData->m_strConfigFileFull; }

    /**
     * Returns this machine name.
     *
     * @note This method doesn't lock this object or check its readiness.
     * Intended to be used only after doing addCaller() manually and locking it
     * for reading.
     */
    const Utf8Str& i_getName() const { return mUserData->s.strName; }

    enum
    {
        IsModified_MachineData          = 0x0001,
        IsModified_Storage              = 0x0002,
        IsModified_NetworkAdapters      = 0x0008,
        IsModified_SerialPorts          = 0x0010,
        IsModified_ParallelPorts        = 0x0020,
        IsModified_VRDEServer           = 0x0040,
        IsModified_AudioAdapter         = 0x0080,
        IsModified_USB                  = 0x0100,
        IsModified_BIOS                 = 0x0200,
        IsModified_SharedFolders        = 0x0400,
        IsModified_Snapshots            = 0x0800,
        IsModified_BandwidthControl     = 0x1000,
        IsModified_Recording            = 0x2000
    };

    /**
     * Returns various information about this machine.
     *
     * @note This method doesn't lock this object or check its readiness.
     * Intended to be used only after doing addCaller() manually and locking it
     * for reading.
     */
    Utf8Str i_getOSTypeId() const { return mUserData->s.strOsType; }
    ChipsetType_T i_getChipsetType() const { return mHWData->mChipsetType; }
    ULONG i_getMonitorCount() const { return mHWData->mMonitorCount; }
    ParavirtProvider_T i_getParavirtProvider() const { return mHWData->mParavirtProvider; }
    Utf8Str i_getParavirtDebug() const { return mHWData->mParavirtDebug; }

    void i_setModified(uint32_t fl, bool fAllowStateModification = true);
    void i_setModifiedLock(uint32_t fl, bool fAllowStateModification = true);

    MachineState_T i_getMachineState() const { return mData->mMachineState; }

    bool i_isStateModificationAllowed() const { return mData->m_fAllowStateModification; }
    void i_allowStateModification()           { mData->m_fAllowStateModification = true; }
    void i_disallowStateModification()        { mData->m_fAllowStateModification = false; }

    const StringsList &i_getGroups() const { return mUserData->s.llGroups; }

    // callback handlers
    virtual HRESULT i_onNetworkAdapterChange(INetworkAdapter * /* networkAdapter */, BOOL /* changeAdapter */) { return S_OK; }
    virtual HRESULT i_onNATRedirectRuleChange(ULONG /* slot */, BOOL /* fRemove */ , IN_BSTR /* name */,
                                              NATProtocol_T /* protocol */, IN_BSTR /* host ip */, LONG /* host port */,
                                              IN_BSTR /* guest port */, LONG /* guest port */ ) { return S_OK; }
    virtual HRESULT i_onAudioAdapterChange(IAudioAdapter * /* audioAdapter */) { return S_OK; }
    virtual HRESULT i_onSerialPortChange(ISerialPort * /* serialPort */) { return S_OK; }
    virtual HRESULT i_onParallelPortChange(IParallelPort * /* parallelPort */) { return S_OK; }
    virtual HRESULT i_onVRDEServerChange(BOOL /* aRestart */) { return S_OK; }
    virtual HRESULT i_onUSBControllerChange() { return S_OK; }
    virtual HRESULT i_onStorageControllerChange() { return S_OK; }
    virtual HRESULT i_onCPUChange(ULONG /* aCPU */, BOOL /* aRemove */) { return S_OK; }
    virtual HRESULT i_onCPUExecutionCapChange(ULONG /* aExecutionCap */) { return S_OK; }
    virtual HRESULT i_onMediumChange(IMediumAttachment * /* mediumAttachment */, BOOL /* force */) { return S_OK; }
    virtual HRESULT i_onSharedFolderChange() { return S_OK; }
    virtual HRESULT i_onClipboardModeChange(ClipboardMode_T /* aClipboardMode */) { return S_OK; }
    virtual HRESULT i_onDnDModeChange(DnDMode_T /* aDnDMode */) { return S_OK; }
    virtual HRESULT i_onBandwidthGroupChange(IBandwidthGroup * /* aBandwidthGroup */) { return S_OK; }
    virtual HRESULT i_onStorageDeviceChange(IMediumAttachment * /* mediumAttachment */, BOOL /* remove */,
                                            BOOL /* silent */) { return S_OK; }
    virtual HRESULT i_onRecordingChange(BOOL /* aEnable */) { return S_OK; }

    HRESULT i_saveRegistryEntry(settings::MachineRegistryEntry &data);

    int i_calculateFullPath(const Utf8Str &strPath, Utf8Str &aResult);
    void i_copyPathRelativeToMachine(const Utf8Str &strSource, Utf8Str &strTarget);

    void i_getLogFolder(Utf8Str &aLogFolder);
    Utf8Str i_getLogFilename(ULONG idx);
    Utf8Str i_getHardeningLogFilename(void);

    void i_composeSavedStateFilename(Utf8Str &strStateFilePath);

    bool i_isUSBControllerPresent();

    HRESULT i_launchVMProcess(IInternalSessionControl *aControl,
                              const Utf8Str &strType,
                              const Utf8Str &strEnvironment,
                              ProgressProxy *aProgress);

    HRESULT i_getDirectControl(ComPtr<IInternalSessionControl> *directControl)
    {
        HRESULT rc;
        *directControl = mData->mSession.mDirectControl;

        if (!*directControl)
            rc = E_ACCESSDENIED;
        else
            rc = S_OK;

        return rc;
    }

    bool i_isSessionOpen(ComObjPtr<SessionMachine> &aMachine,
                         ComPtr<IInternalSessionControl> *aControl = NULL,
                         bool aRequireVM = false,
                         bool aAllowClosing = false);
    bool i_isSessionSpawning();

    bool i_isSessionOpenOrClosing(ComObjPtr<SessionMachine> &aMachine,
                                  ComPtr<IInternalSessionControl> *aControl = NULL)
    { return i_isSessionOpen(aMachine, aControl, false /* aRequireVM */, true /* aAllowClosing */); }

    bool i_isSessionOpenVM(ComObjPtr<SessionMachine> &aMachine,
                           ComPtr<IInternalSessionControl> *aControl = NULL)
    { return i_isSessionOpen(aMachine, aControl, true /* aRequireVM */, false /* aAllowClosing */); }

    bool i_checkForSpawnFailure();

    HRESULT i_prepareRegister();

    HRESULT i_getSharedFolder(const Utf8Str &aName,
                              ComObjPtr<SharedFolder> &aSharedFolder,
                              bool aSetError = false)
    {
        AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);
        return i_findSharedFolder(aName, aSharedFolder, aSetError);
    }

    HRESULT i_addStateDependency(StateDependency aDepType = AnyStateDep,
                                 MachineState_T *aState = NULL,
                                 BOOL *aRegistered = NULL);
    void i_releaseStateDependency();

    HRESULT i_getStorageControllerByName(const Utf8Str &aName,
                                         ComObjPtr<StorageController> &aStorageController,
                                         bool aSetError = false);

    HRESULT i_getMediumAttachmentsOfController(const Utf8Str &aName,
                                               MediumAttachmentList &aAttachments);

    HRESULT i_getUSBControllerByName(const Utf8Str &aName,
                                     ComObjPtr<USBController> &aUSBController,
                                     bool aSetError = false);

    HRESULT i_getBandwidthGroup(const Utf8Str &strBandwidthGroup,
                                ComObjPtr<BandwidthGroup> &pBandwidthGroup,
                                bool fSetError = false)
    {
        return mBandwidthControl->i_getBandwidthGroupByName(strBandwidthGroup,
                                                            pBandwidthGroup,
                                                            fSetError);
    }

    static HRESULT i_setErrorStatic(HRESULT aResultCode, const char *pcszMsg, ...);

protected:

    class ClientToken;

    HRESULT i_checkStateDependency(StateDependency aDepType);

    Machine *i_getMachine();

    void i_ensureNoStateDependencies();

    virtual HRESULT i_setMachineState(MachineState_T aMachineState);

    HRESULT i_findSharedFolder(const Utf8Str &aName,
                               ComObjPtr<SharedFolder> &aSharedFolder,
                               bool aSetError = false);

    HRESULT i_loadSettings(bool aRegistered);
    HRESULT i_loadMachineDataFromSettings(const settings::MachineConfigFile &config,
                                          const Guid *puuidRegistry);
    HRESULT i_loadSnapshot(const settings::Snapshot &data,
                           const Guid &aCurSnapshotId,
                           Snapshot *aParentSnapshot);
    HRESULT i_loadHardware(const Guid *puuidRegistry,
                           const Guid *puuidSnapshot,
                           const settings::Hardware &data,
                           const settings::Debugging *pDbg,
                           const settings::Autostart *pAutostart);
    HRESULT i_loadDebugging(const settings::Debugging *pDbg);
    HRESULT i_loadAutostart(const settings::Autostart *pAutostart);
    HRESULT i_loadStorageControllers(const settings::Storage &data,
                                     const Guid *puuidRegistry,
                                     const Guid *puuidSnapshot);
    HRESULT i_loadStorageDevices(StorageController *aStorageController,
                                 const settings::StorageController &data,
                                 const Guid *puuidRegistry,
                                 const Guid *puuidSnapshot);

    HRESULT i_findSnapshotById(const Guid &aId,
                               ComObjPtr<Snapshot> &aSnapshot,
                               bool aSetError = false);
    HRESULT i_findSnapshotByName(const Utf8Str &strName,
                                 ComObjPtr<Snapshot> &aSnapshot,
                                 bool aSetError = false);

    ULONG   i_getUSBControllerCountByType(USBControllerType_T enmType);

    enum
    {
        /* flags for #saveSettings() */
        SaveS_ResetCurStateModified = 0x01,
        SaveS_Force = 0x04,
        /* flags for #saveStateSettings() */
        SaveSTS_CurStateModified = 0x20,
        SaveSTS_StateFilePath = 0x40,
        SaveSTS_StateTimeStamp = 0x80
    };

    HRESULT i_prepareSaveSettings(bool *pfNeedsGlobalSaveSettings);
    HRESULT i_saveSettings(bool *pfNeedsGlobalSaveSettings, int aFlags = 0);

    void i_copyMachineDataToSettings(settings::MachineConfigFile &config);
    HRESULT i_saveAllSnapshots(settings::MachineConfigFile &config);
    HRESULT i_saveHardware(settings::Hardware &data, settings::Debugging *pDbg,
                           settings::Autostart *pAutostart);
    HRESULT i_saveStorageControllers(settings::Storage &data);
    HRESULT i_saveStorageDevices(ComObjPtr<StorageController> aStorageController,
                                 settings::StorageController &data);
    HRESULT i_saveStateSettings(int aFlags);

    void i_addMediumToRegistry(ComObjPtr<Medium> &pMedium);

    HRESULT i_createImplicitDiffs(IProgress *aProgress,
                                  ULONG aWeight,
                                  bool aOnline);
    HRESULT i_deleteImplicitDiffs(bool aOnline);

    MediumAttachment* i_findAttachment(const MediumAttachmentList &ll,
                                       const Utf8Str &aControllerName,
                                       LONG aControllerPort,
                                       LONG aDevice);
    MediumAttachment* i_findAttachment(const MediumAttachmentList &ll,
                                       ComObjPtr<Medium> pMedium);
    MediumAttachment* i_findAttachment(const MediumAttachmentList &ll,
                                       Guid &id);

    HRESULT i_detachDevice(MediumAttachment *pAttach,
                           AutoWriteLock &writeLock,
                           Snapshot *pSnapshot);

    HRESULT i_detachAllMedia(AutoWriteLock &writeLock,
                             Snapshot *pSnapshot,
                             CleanupMode_T cleanupMode,
                             MediaList &llMedia);

    void i_commitMedia(bool aOnline = false);
    void i_rollbackMedia();

    bool i_isInOwnDir(Utf8Str *aSettingsDir = NULL) const;

    void i_rollback(bool aNotify);
    void i_commit();
    void i_copyFrom(Machine *aThat);
    bool i_isControllerHotplugCapable(StorageControllerType_T enmCtrlType);

    Utf8Str i_getExtraData(const Utf8Str &strKey);

#ifdef VBOX_WITH_GUEST_PROPS
    HRESULT i_getGuestPropertyFromService(const com::Utf8Str &aName, com::Utf8Str &aValue,
                                          LONG64 *aTimestamp, com::Utf8Str &aFlags) const;
    HRESULT i_setGuestPropertyToService(const com::Utf8Str &aName, const com::Utf8Str &aValue,
                                        const com::Utf8Str &aFlags, bool fDelete);
    HRESULT i_getGuestPropertyFromVM(const com::Utf8Str &aName, com::Utf8Str &aValue,
                                     LONG64 *aTimestamp, com::Utf8Str &aFlags) const;
    HRESULT i_setGuestPropertyToVM(const com::Utf8Str &aName, const com::Utf8Str &aValue,
                                   const com::Utf8Str &aFlags, bool fDelete);
    HRESULT i_enumerateGuestPropertiesInService(const com::Utf8Str &aPatterns,
                                                std::vector<com::Utf8Str> &aNames,
                                                std::vector<com::Utf8Str> &aValues,
                                                std::vector<LONG64> &aTimestamps,
                                                std::vector<com::Utf8Str> &aFlags);
    HRESULT i_enumerateGuestPropertiesOnVM(const com::Utf8Str &aPatterns,
                                           std::vector<com::Utf8Str> &aNames,
                                           std::vector<com::Utf8Str> &aValues,
                                           std::vector<LONG64> &aTimestamps,
                                           std::vector<com::Utf8Str> &aFlags);

#endif /* VBOX_WITH_GUEST_PROPS */

#ifdef VBOX_WITH_RESOURCE_USAGE_API
    void i_getDiskList(MediaList &list);
    void i_registerMetrics(PerformanceCollector *aCollector, Machine *aMachine, RTPROCESS pid);

    pm::CollectorGuest     *mCollectorGuest;
#endif /* VBOX_WITH_RESOURCE_USAGE_API */

    Machine * const         mPeer;

    VirtualBox * const      mParent;

    Shareable<Data>         mData;
    Shareable<SSData>       mSSData;

    Backupable<UserData>    mUserData;
    Backupable<HWData>      mHWData;

    /**
     * Hard disk and other media data.
     *
     * The usage policy is the same as for mHWData, but a separate field
     * is necessary because hard disk data requires different procedures when
     * taking or deleting snapshots, etc.
     *
     * @todo r=klaus change this to a regular list and use the normal way to
     * handle the settings when creating a session or taking a snapshot.
     * Same thing applies to mStorageControllers and mUSBControllers.
     */
    Backupable<MediumAttachmentList> mMediumAttachments;

    // the following fields need special backup/rollback/commit handling,
    // so they cannot be a part of HWData

    const ComObjPtr<VRDEServer>        mVRDEServer;
    const ComObjPtr<SerialPort>        mSerialPorts[SchemaDefs::SerialPortCount];
    const ComObjPtr<ParallelPort>      mParallelPorts[SchemaDefs::ParallelPortCount];
    const ComObjPtr<AudioAdapter>      mAudioAdapter;
    const ComObjPtr<USBDeviceFilters>  mUSBDeviceFilters;
    const ComObjPtr<BIOSSettings>      mBIOSSettings;
    const ComObjPtr<RecordingSettings> mRecordingSettings;
    const ComObjPtr<BandwidthControl>  mBandwidthControl;

    typedef std::vector<ComObjPtr<NetworkAdapter> > NetworkAdapterVector;
    NetworkAdapterVector               mNetworkAdapters;

    typedef std::list<ComObjPtr<StorageController> > StorageControllerList;
    Backupable<StorageControllerList>  mStorageControllers;

    typedef std::list<ComObjPtr<USBController> > USBControllerList;
    Backupable<USBControllerList>      mUSBControllers;

    uint64_t                           uRegistryNeedsSaving;

    /**
     * Abstract base class for all Machine or SessionMachine related
     * asynchronous tasks. This is necessary since RTThreadCreate cannot call
     * a (non-static) method as its thread function, so instead we have it call
     * the static Machine::taskHandler, which then calls the handler() method
     * in here (implemented by the subclasses).
     */
    class Task : public ThreadTask
    {
    public:
        Task(Machine *m, Progress *p, const Utf8Str &t)
            : ThreadTask(t),
              m_pMachine(m),
              m_machineCaller(m),
              m_pProgress(p),
              m_machineStateBackup(m->mData->mMachineState) // save the current machine state
        {}
        virtual ~Task(){}

        void modifyBackedUpState(MachineState_T s)
        {
            *const_cast<MachineState_T *>(&m_machineStateBackup) = s;
        }

        ComObjPtr<Machine>              m_pMachine;
        AutoCaller                      m_machineCaller;
        ComObjPtr<Progress>             m_pProgress;
        const MachineState_T            m_machineStateBackup;
    };

    class DeleteConfigTask;
    void i_deleteConfigHandler(DeleteConfigTask &task);

    friend class Appliance;
    friend class RecordingSettings;
    friend class RecordingScreenSettings;
    friend class SessionMachine;
    friend class SnapshotMachine;
    friend class VirtualBox;

    friend class MachineCloneVM;
    friend class MachineMoveVM;
private:
    // wrapped IMachine properties
    HRESULT getParent(ComPtr<IVirtualBox> &aParent);
    HRESULT getIcon(std::vector<BYTE> &aIcon);
    HRESULT setIcon(const std::vector<BYTE> &aIcon);
    HRESULT getAccessible(BOOL *aAccessible);
    HRESULT getAccessError(ComPtr<IVirtualBoxErrorInfo> &aAccessError);
    HRESULT getName(com::Utf8Str &aName);
    HRESULT setName(const com::Utf8Str &aName);
    HRESULT getDescription(com::Utf8Str &aDescription);
    HRESULT setDescription(const com::Utf8Str &aDescription);
    HRESULT getId(com::Guid &aId);
    HRESULT getGroups(std::vector<com::Utf8Str> &aGroups);
    HRESULT setGroups(const std::vector<com::Utf8Str> &aGroups);
    HRESULT getOSTypeId(com::Utf8Str &aOSTypeId);
    HRESULT setOSTypeId(const com::Utf8Str &aOSTypeId);
    HRESULT getHardwareVersion(com::Utf8Str &aHardwareVersion);
    HRESULT setHardwareVersion(const com::Utf8Str &aHardwareVersion);
    HRESULT getHardwareUUID(com::Guid &aHardwareUUID);
    HRESULT setHardwareUUID(const com::Guid &aHardwareUUID);
    HRESULT getCPUCount(ULONG *aCPUCount);
    HRESULT setCPUCount(ULONG aCPUCount);
    HRESULT getCPUHotPlugEnabled(BOOL *aCPUHotPlugEnabled);
    HRESULT setCPUHotPlugEnabled(BOOL aCPUHotPlugEnabled);
    HRESULT getCPUExecutionCap(ULONG *aCPUExecutionCap);
    HRESULT setCPUExecutionCap(ULONG aCPUExecutionCap);
    HRESULT getCPUIDPortabilityLevel(ULONG *aCPUIDPortabilityLevel);
    HRESULT setCPUIDPortabilityLevel(ULONG aCPUIDPortabilityLevel);
    HRESULT getCPUProfile(com::Utf8Str &aCPUProfile);
    HRESULT setCPUProfile(const com::Utf8Str &aCPUProfile);
    HRESULT getMemorySize(ULONG *aMemorySize);
    HRESULT setMemorySize(ULONG aMemorySize);
    HRESULT getMemoryBalloonSize(ULONG *aMemoryBalloonSize);
    HRESULT setMemoryBalloonSize(ULONG aMemoryBalloonSize);
    HRESULT getPageFusionEnabled(BOOL *aPageFusionEnabled);
    HRESULT setPageFusionEnabled(BOOL aPageFusionEnabled);
    HRESULT getGraphicsControllerType(GraphicsControllerType_T *aGraphicsControllerType);
    HRESULT setGraphicsControllerType(GraphicsControllerType_T aGraphicsControllerType);
    HRESULT getVRAMSize(ULONG *aVRAMSize);
    HRESULT setVRAMSize(ULONG aVRAMSize);
    HRESULT getAccelerate3DEnabled(BOOL *aAccelerate3DEnabled);
    HRESULT setAccelerate3DEnabled(BOOL aAccelerate3DEnabled);
    HRESULT getAccelerate2DVideoEnabled(BOOL *aAccelerate2DVideoEnabled);
    HRESULT setAccelerate2DVideoEnabled(BOOL aAccelerate2DVideoEnabled);
    HRESULT getMonitorCount(ULONG *aMonitorCount);
    HRESULT setMonitorCount(ULONG aMonitorCount);
    HRESULT getBIOSSettings(ComPtr<IBIOSSettings> &aBIOSSettings);
    HRESULT getRecordingSettings(ComPtr<IRecordingSettings> &aRecordingSettings);
    HRESULT getFirmwareType(FirmwareType_T *aFirmwareType);
    HRESULT setFirmwareType(FirmwareType_T aFirmwareType);
    HRESULT getPointingHIDType(PointingHIDType_T *aPointingHIDType);
    HRESULT setPointingHIDType(PointingHIDType_T aPointingHIDType);
    HRESULT getKeyboardHIDType(KeyboardHIDType_T *aKeyboardHIDType);
    HRESULT setKeyboardHIDType(KeyboardHIDType_T aKeyboardHIDType);
    HRESULT getHPETEnabled(BOOL *aHPETEnabled);
    HRESULT setHPETEnabled(BOOL aHPETEnabled);
    HRESULT getChipsetType(ChipsetType_T *aChipsetType);
    HRESULT setChipsetType(ChipsetType_T aChipsetType);
    HRESULT getSnapshotFolder(com::Utf8Str &aSnapshotFolder);
    HRESULT setSnapshotFolder(const com::Utf8Str &aSnapshotFolder);
    HRESULT getVRDEServer(ComPtr<IVRDEServer> &aVRDEServer);
    HRESULT getEmulatedUSBCardReaderEnabled(BOOL *aEmulatedUSBCardReaderEnabled);
    HRESULT setEmulatedUSBCardReaderEnabled(BOOL aEmulatedUSBCardReaderEnabled);
    HRESULT getMediumAttachments(std::vector<ComPtr<IMediumAttachment> > &aMediumAttachments);
    HRESULT getUSBControllers(std::vector<ComPtr<IUSBController> > &aUSBControllers);
    HRESULT getUSBDeviceFilters(ComPtr<IUSBDeviceFilters> &aUSBDeviceFilters);
    HRESULT getAudioAdapter(ComPtr<IAudioAdapter> &aAudioAdapter);
    HRESULT getStorageControllers(std::vector<ComPtr<IStorageController> > &aStorageControllers);
    HRESULT getSettingsFilePath(com::Utf8Str &aSettingsFilePath);
    HRESULT getSettingsAuxFilePath(com::Utf8Str &aSettingsAuxFilePath);
    HRESULT getSettingsModified(BOOL *aSettingsModified);
    HRESULT getSessionState(SessionState_T *aSessionState);
    HRESULT getSessionType(SessionType_T *aSessionType);
    HRESULT getSessionName(com::Utf8Str &aSessionType);
    HRESULT getSessionPID(ULONG *aSessionPID);
    HRESULT getState(MachineState_T *aState);
    HRESULT getLastStateChange(LONG64 *aLastStateChange);
    HRESULT getStateFilePath(com::Utf8Str &aStateFilePath);
    HRESULT getLogFolder(com::Utf8Str &aLogFolder);
    HRESULT getCurrentSnapshot(ComPtr<ISnapshot> &aCurrentSnapshot);
    HRESULT getSnapshotCount(ULONG *aSnapshotCount);
    HRESULT getCurrentStateModified(BOOL *aCurrentStateModified);
    HRESULT getSharedFolders(std::vector<ComPtr<ISharedFolder> > &aSharedFolders);
    HRESULT getClipboardMode(ClipboardMode_T *aClipboardMode);
    HRESULT setClipboardMode(ClipboardMode_T aClipboardMode);
    HRESULT getDnDMode(DnDMode_T *aDnDMode);
    HRESULT setDnDMode(DnDMode_T aDnDMode);
    HRESULT getTeleporterEnabled(BOOL *aTeleporterEnabled);
    HRESULT setTeleporterEnabled(BOOL aTeleporterEnabled);
    HRESULT getTeleporterPort(ULONG *aTeleporterPort);
    HRESULT setTeleporterPort(ULONG aTeleporterPort);
    HRESULT getTeleporterAddress(com::Utf8Str &aTeleporterAddress);
    HRESULT setTeleporterAddress(const com::Utf8Str &aTeleporterAddress);
    HRESULT getTeleporterPassword(com::Utf8Str &aTeleporterPassword);
    HRESULT setTeleporterPassword(const com::Utf8Str &aTeleporterPassword);
    HRESULT getParavirtProvider(ParavirtProvider_T *aParavirtProvider);
    HRESULT setParavirtProvider(ParavirtProvider_T aParavirtProvider);
    HRESULT getParavirtDebug(com::Utf8Str &aParavirtDebug);
    HRESULT setParavirtDebug(const com::Utf8Str &aParavirtDebug);
    HRESULT getFaultToleranceState(FaultToleranceState_T *aFaultToleranceState);
    HRESULT setFaultToleranceState(FaultToleranceState_T aFaultToleranceState);
    HRESULT getFaultTolerancePort(ULONG *aFaultTolerancePort);
    HRESULT setFaultTolerancePort(ULONG aFaultTolerancePort);
    HRESULT getFaultToleranceAddress(com::Utf8Str &aFaultToleranceAddress);
    HRESULT setFaultToleranceAddress(const com::Utf8Str &aFaultToleranceAddress);
    HRESULT getFaultTolerancePassword(com::Utf8Str &aFaultTolerancePassword);
    HRESULT setFaultTolerancePassword(const com::Utf8Str &aFaultTolerancePassword);
    HRESULT getFaultToleranceSyncInterval(ULONG *aFaultToleranceSyncInterval);
    HRESULT setFaultToleranceSyncInterval(ULONG aFaultToleranceSyncInterval);
    HRESULT getRTCUseUTC(BOOL *aRTCUseUTC);
    HRESULT setRTCUseUTC(BOOL aRTCUseUTC);
    HRESULT getIOCacheEnabled(BOOL *aIOCacheEnabled);
    HRESULT setIOCacheEnabled(BOOL aIOCacheEnabled);
    HRESULT getIOCacheSize(ULONG *aIOCacheSize);
    HRESULT setIOCacheSize(ULONG aIOCacheSize);
    HRESULT getPCIDeviceAssignments(std::vector<ComPtr<IPCIDeviceAttachment> > &aPCIDeviceAssignments);
    HRESULT getBandwidthControl(ComPtr<IBandwidthControl> &aBandwidthControl);
    HRESULT getTracingEnabled(BOOL *aTracingEnabled);
    HRESULT setTracingEnabled(BOOL aTracingEnabled);
    HRESULT getTracingConfig(com::Utf8Str &aTracingConfig);
    HRESULT setTracingConfig(const com::Utf8Str &aTracingConfig);
    HRESULT getAllowTracingToAccessVM(BOOL *aAllowTracingToAccessVM);
    HRESULT setAllowTracingToAccessVM(BOOL aAllowTracingToAccessVM);
    HRESULT getAutostartEnabled(BOOL *aAutostartEnabled);
    HRESULT setAutostartEnabled(BOOL aAutostartEnabled);
    HRESULT getAutostartDelay(ULONG *aAutostartDelay);
    HRESULT setAutostartDelay(ULONG aAutostartDelay);
    HRESULT getAutostopType(AutostopType_T *aAutostopType);
    HRESULT setAutostopType(AutostopType_T aAutostopType);
    HRESULT getDefaultFrontend(com::Utf8Str &aDefaultFrontend);
    HRESULT setDefaultFrontend(const com::Utf8Str &aDefaultFrontend);
    HRESULT getUSBProxyAvailable(BOOL *aUSBProxyAvailable);
    HRESULT getVMProcessPriority(com::Utf8Str &aVMProcessPriority);
    HRESULT setVMProcessPriority(const com::Utf8Str &aVMProcessPriority);

    // wrapped IMachine methods
    HRESULT lockMachine(const ComPtr<ISession> &aSession,
                        LockType_T aLockType);
    HRESULT launchVMProcess(const ComPtr<ISession> &aSession,
                            const com::Utf8Str &aType,
                            const com::Utf8Str &aEnvironment,
                            ComPtr<IProgress> &aProgress);
    HRESULT setBootOrder(ULONG aPosition,
                         DeviceType_T aDevice);
    HRESULT getBootOrder(ULONG aPosition,
                         DeviceType_T *aDevice);
    HRESULT attachDevice(const com::Utf8Str &aName,
                         LONG aControllerPort,
                         LONG aDevice,
                         DeviceType_T aType,
                         const ComPtr<IMedium> &aMedium);
    HRESULT attachDeviceWithoutMedium(const com::Utf8Str &aName,
                                      LONG aControllerPort,
                                      LONG aDevice,
                                      DeviceType_T aType);
    HRESULT detachDevice(const com::Utf8Str &aName,
                         LONG aControllerPort,
                         LONG aDevice);
    HRESULT passthroughDevice(const com::Utf8Str &aName,
                              LONG aControllerPort,
                              LONG aDevice,
                              BOOL aPassthrough);
    HRESULT temporaryEjectDevice(const com::Utf8Str &aName,
                                 LONG aControllerPort,
                                 LONG aDevice,
                                 BOOL aTemporaryEject);
    HRESULT nonRotationalDevice(const com::Utf8Str &aName,
                                LONG aControllerPort,
                                LONG aDevice,
                                BOOL aNonRotational);
    HRESULT setAutoDiscardForDevice(const com::Utf8Str &aName,
                                    LONG aControllerPort,
                                    LONG aDevice,
                                    BOOL aDiscard);
    HRESULT setHotPluggableForDevice(const com::Utf8Str &aName,
                                     LONG aControllerPort,
                                     LONG aDevice,
                                     BOOL aHotPluggable);
    HRESULT setBandwidthGroupForDevice(const com::Utf8Str &aName,
                                       LONG aControllerPort,
                                       LONG aDevice,
                                       const ComPtr<IBandwidthGroup> &aBandwidthGroup);
    HRESULT setNoBandwidthGroupForDevice(const com::Utf8Str &aName,
                                         LONG aControllerPort,
                                         LONG aDevice);
    HRESULT unmountMedium(const com::Utf8Str &aName,
                          LONG aControllerPort,
                          LONG aDevice,
                          BOOL aForce);
    HRESULT mountMedium(const com::Utf8Str &aName,
                        LONG aControllerPort,
                        LONG aDevice,
                        const ComPtr<IMedium> &aMedium,
                        BOOL aForce);
    HRESULT getMedium(const com::Utf8Str &aName,
                      LONG aControllerPort,
                      LONG aDevice,
                      ComPtr<IMedium> &aMedium);
    HRESULT getMediumAttachmentsOfController(const com::Utf8Str &aName,
                                             std::vector<ComPtr<IMediumAttachment> > &aMediumAttachments);
    HRESULT getMediumAttachment(const com::Utf8Str &aName,
                                LONG aControllerPort,
                                LONG aDevice,
                                ComPtr<IMediumAttachment> &aAttachment);
    HRESULT attachHostPCIDevice(LONG aHostAddress,
                                LONG aDesiredGuestAddress,
                                BOOL aTryToUnbind);
    HRESULT detachHostPCIDevice(LONG aHostAddress);
    HRESULT getNetworkAdapter(ULONG aSlot,
                              ComPtr<INetworkAdapter> &aAdapter);
    HRESULT addStorageController(const com::Utf8Str &aName,
                                 StorageBus_T aConnectionType,
                                 ComPtr<IStorageController> &aController);
    HRESULT getStorageControllerByName(const com::Utf8Str &aName,
                                       ComPtr<IStorageController> &aStorageController);
    HRESULT getStorageControllerByInstance(StorageBus_T aConnectionType,
                                           ULONG aInstance,
                                           ComPtr<IStorageController> &aStorageController);
    HRESULT removeStorageController(const com::Utf8Str &aName);
    HRESULT setStorageControllerBootable(const com::Utf8Str &aName,
                                         BOOL aBootable);
    HRESULT addUSBController(const com::Utf8Str &aName,
                             USBControllerType_T aType,
                             ComPtr<IUSBController> &aController);
    HRESULT removeUSBController(const com::Utf8Str &aName);
    HRESULT getUSBControllerByName(const com::Utf8Str &aName,
                                   ComPtr<IUSBController> &aController);
    HRESULT getUSBControllerCountByType(USBControllerType_T aType,
                                        ULONG *aControllers);
    HRESULT getSerialPort(ULONG aSlot,
                          ComPtr<ISerialPort> &aPort);
    HRESULT getParallelPort(ULONG aSlot,
                            ComPtr<IParallelPort> &aPort);
    HRESULT getExtraDataKeys(std::vector<com::Utf8Str> &aKeys);
    HRESULT getExtraData(const com::Utf8Str &aKey,
                         com::Utf8Str &aValue);
    HRESULT setExtraData(const com::Utf8Str &aKey,
                         const com::Utf8Str &aValue);
    HRESULT getCPUProperty(CPUPropertyType_T aProperty,
                           BOOL *aValue);
    HRESULT setCPUProperty(CPUPropertyType_T aProperty,
                           BOOL aValue);
    HRESULT getCPUIDLeafByOrdinal(ULONG aOrdinal,
                                  ULONG *aIdx,
                                  ULONG *aSubIdx,
                                  ULONG *aValEax,
                                  ULONG *aValEbx,
                                  ULONG *aValEcx,
                                  ULONG *aValEdx);
    HRESULT getCPUIDLeaf(ULONG aIdx, ULONG aSubIdx,
                         ULONG *aValEax,
                         ULONG *aValEbx,
                         ULONG *aValEcx,
                         ULONG *aValEdx);
    HRESULT setCPUIDLeaf(ULONG aIdx, ULONG aSubIdx,
                         ULONG aValEax,
                         ULONG aValEbx,
                         ULONG aValEcx,
                         ULONG aValEdx);
    HRESULT removeCPUIDLeaf(ULONG aIdx, ULONG aSubIdx);
    HRESULT removeAllCPUIDLeaves();
    HRESULT getHWVirtExProperty(HWVirtExPropertyType_T aProperty,
                                BOOL *aValue);
    HRESULT setHWVirtExProperty(HWVirtExPropertyType_T aProperty,
                                BOOL aValue);
    HRESULT setSettingsFilePath(const com::Utf8Str &aSettingsFilePath,
                                ComPtr<IProgress> &aProgress);
    HRESULT saveSettings();
    HRESULT discardSettings();
    HRESULT unregister(AutoCaller &aAutoCaller,
                       CleanupMode_T aCleanupMode,
                       std::vector<ComPtr<IMedium> > &aMedia);
    HRESULT deleteConfig(const std::vector<ComPtr<IMedium> > &aMedia,
                         ComPtr<IProgress> &aProgress);
    HRESULT exportTo(const ComPtr<IAppliance> &aAppliance,
                     const com::Utf8Str &aLocation,
                     ComPtr<IVirtualSystemDescription> &aDescription);
    HRESULT findSnapshot(const com::Utf8Str &aNameOrId,
                         ComPtr<ISnapshot> &aSnapshot);
    HRESULT createSharedFolder(const com::Utf8Str &aName,
                               const com::Utf8Str &aHostPath,
                               BOOL aWritable,
                               BOOL aAutomount,
                               const com::Utf8Str &aAutoMountPoint);
    HRESULT removeSharedFolder(const com::Utf8Str &aName);
    HRESULT canShowConsoleWindow(BOOL *aCanShow);
    HRESULT showConsoleWindow(LONG64 *aWinId);
    HRESULT getGuestProperty(const com::Utf8Str &aName,
                             com::Utf8Str &aValue,
                             LONG64 *aTimestamp,
                             com::Utf8Str &aFlags);
    HRESULT getGuestPropertyValue(const com::Utf8Str &aProperty,
                                  com::Utf8Str &aValue);
    HRESULT getGuestPropertyTimestamp(const com::Utf8Str &aProperty,
                                      LONG64 *aValue);
    HRESULT setGuestProperty(const com::Utf8Str &aProperty,
                             const com::Utf8Str &aValue,
                             const com::Utf8Str &aFlags);
    HRESULT setGuestPropertyValue(const com::Utf8Str &aProperty,
                                  const com::Utf8Str &aValue);
    HRESULT deleteGuestProperty(const com::Utf8Str &aName);
    HRESULT enumerateGuestProperties(const com::Utf8Str &aPatterns,
                                     std::vector<com::Utf8Str> &aNames,
                                     std::vector<com::Utf8Str> &aValues,
                                     std::vector<LONG64> &aTimestamps,
                                     std::vector<com::Utf8Str> &aFlags);
    HRESULT querySavedGuestScreenInfo(ULONG aScreenId,
                                      ULONG *aOriginX,
                                      ULONG *aOriginY,
                                      ULONG *aWidth,
                                      ULONG *aHeight,
                                      BOOL *aEnabled);
    HRESULT readSavedThumbnailToArray(ULONG aScreenId,
                                      BitmapFormat_T aBitmapFormat,
                                      ULONG *aWidth,
                                      ULONG *aHeight,
                                      std::vector<BYTE> &aData);
    HRESULT querySavedScreenshotInfo(ULONG aScreenId,
                                     ULONG *aWidth,
                                     ULONG *aHeight,
                                     std::vector<BitmapFormat_T> &aBitmapFormats);
    HRESULT readSavedScreenshotToArray(ULONG aScreenId,
                                       BitmapFormat_T aBitmapFormat,
                                       ULONG *aWidth,
                                       ULONG *aHeight,
                                       std::vector<BYTE> &aData);

    HRESULT hotPlugCPU(ULONG aCpu);
    HRESULT hotUnplugCPU(ULONG aCpu);
    HRESULT getCPUStatus(ULONG aCpu,
                         BOOL *aAttached);
    HRESULT getEffectiveParavirtProvider(ParavirtProvider_T *aParavirtProvider);
    HRESULT queryLogFilename(ULONG aIdx,
                             com::Utf8Str &aFilename);
    HRESULT readLog(ULONG aIdx,
                    LONG64 aOffset,
                    LONG64 aSize,
                    std::vector<BYTE> &aData);
    HRESULT cloneTo(const ComPtr<IMachine> &aTarget,
                    CloneMode_T aMode,
                    const std::vector<CloneOptions_T> &aOptions,
                    ComPtr<IProgress> &aProgress);
    HRESULT moveTo(const com::Utf8Str &aTargetPath,
                   const com::Utf8Str &aType,
                   ComPtr<IProgress> &aProgress);
    HRESULT saveState(ComPtr<IProgress> &aProgress);
    HRESULT adoptSavedState(const com::Utf8Str &aSavedStateFile);
    HRESULT discardSavedState(BOOL aFRemoveFile);
    HRESULT takeSnapshot(const com::Utf8Str &aName,
                         const com::Utf8Str &aDescription,
                         BOOL aPause,
                         com::Guid &aId,
                         ComPtr<IProgress> &aProgress);
    HRESULT deleteSnapshot(const com::Guid &aId,
                           ComPtr<IProgress> &aProgress);
    HRESULT deleteSnapshotAndAllChildren(const com::Guid &aId,
                                         ComPtr<IProgress> &aProgress);
    HRESULT deleteSnapshotRange(const com::Guid &aStartId,
                                const com::Guid &aEndId,
                                ComPtr<IProgress> &aProgress);
    HRESULT restoreSnapshot(const ComPtr<ISnapshot> &aSnapshot,
                            ComPtr<IProgress> &aProgress);
    HRESULT applyDefaults(const com::Utf8Str &aFlags);

    // wrapped IInternalMachineControl properties

    // wrapped IInternalMachineControl methods
    HRESULT updateState(MachineState_T aState);
    HRESULT beginPowerUp(const ComPtr<IProgress> &aProgress);
    HRESULT endPowerUp(LONG aResult);
    HRESULT beginPoweringDown(ComPtr<IProgress> &aProgress);
    HRESULT endPoweringDown(LONG aResult,
                            const com::Utf8Str &aErrMsg);
    HRESULT runUSBDeviceFilters(const ComPtr<IUSBDevice> &aDevice,
                                BOOL *aMatched,
                                ULONG *aMaskedInterfaces);
    HRESULT captureUSBDevice(const com::Guid &aId,
                             const com::Utf8Str &aCaptureFilename);
    HRESULT detachUSBDevice(const com::Guid &aId,
                            BOOL aDone);
    HRESULT autoCaptureUSBDevices();
    HRESULT detachAllUSBDevices(BOOL aDone);
    HRESULT onSessionEnd(const ComPtr<ISession> &aSession,
                         ComPtr<IProgress> &aProgress);
    HRESULT finishOnlineMergeMedium();
    HRESULT pullGuestProperties(std::vector<com::Utf8Str> &aNames,
                                std::vector<com::Utf8Str> &aValues,
                                std::vector<LONG64> &aTimestamps,
                                std::vector<com::Utf8Str> &aFlags);
    HRESULT pushGuestProperty(const com::Utf8Str &aName,
                              const com::Utf8Str &aValue,
                              LONG64 aTimestamp,
                              const com::Utf8Str &aFlags);
    HRESULT lockMedia();
    HRESULT unlockMedia();
    HRESULT ejectMedium(const ComPtr<IMediumAttachment> &aAttachment,
                        ComPtr<IMediumAttachment> &aNewAttachment);
    HRESULT reportVmStatistics(ULONG aValidStats,
                               ULONG aCpuUser,
                               ULONG aCpuKernel,
                               ULONG aCpuIdle,
                               ULONG aMemTotal,
                               ULONG aMemFree,
                               ULONG aMemBalloon,
                               ULONG aMemShared,
                               ULONG aMemCache,
                               ULONG aPagedTotal,
                               ULONG aMemAllocTotal,
                               ULONG aMemFreeTotal,
                               ULONG aMemBalloonTotal,
                               ULONG aMemSharedTotal,
                               ULONG aVmNetRx,
                               ULONG aVmNetTx);
    HRESULT authenticateExternal(const std::vector<com::Utf8Str> &aAuthParams,
                                 com::Utf8Str &aResult);
};

// SessionMachine class
////////////////////////////////////////////////////////////////////////////////

/**
 *  @note Notes on locking objects of this class:
 *  SessionMachine shares some data with the primary Machine instance (pointed
 *  to by the |mPeer| member). In order to provide data consistency it also
 *  shares its lock handle. This means that whenever you lock a SessionMachine
 *  instance using Auto[Reader]Lock or AutoMultiLock, the corresponding Machine
 *  instance is also locked in the same lock mode. Keep it in mind.
 */
class ATL_NO_VTABLE SessionMachine :
    public Machine
{
public:
    VIRTUALBOXBASE_ADD_ERRORINFO_SUPPORT(SessionMachine, IMachine)

    DECLARE_NOT_AGGREGATABLE(SessionMachine)

    DECLARE_PROTECT_FINAL_CONSTRUCT()

    BEGIN_COM_MAP(SessionMachine)
        COM_INTERFACE_ENTRY(ISupportErrorInfo)
        COM_INTERFACE_ENTRY(IMachine)
        COM_INTERFACE_ENTRY2(IDispatch, IMachine)
        COM_INTERFACE_ENTRY(IInternalMachineControl)
        VBOX_TWEAK_INTERFACE_ENTRY(IMachine)
    END_COM_MAP()

    DECLARE_EMPTY_CTOR_DTOR(SessionMachine)

    HRESULT FinalConstruct();
    void FinalRelease();

    struct Uninit
    {
        enum Reason { Unexpected, Abnormal, Normal };
    };

    // public initializer/uninitializer for internal purposes only
    HRESULT init(Machine *aMachine);
    void uninit() { uninit(Uninit::Unexpected); }
    void uninit(Uninit::Reason aReason);


    // util::Lockable interface
    RWLockHandle *lockHandle() const;

    // public methods only for internal purposes

    virtual bool i_isSessionMachine() const
    {
        return true;
    }

#ifndef VBOX_WITH_GENERIC_SESSION_WATCHER
    bool i_checkForDeath();

    void i_getTokenId(Utf8Str &strTokenId);
#else /* VBOX_WITH_GENERIC_SESSION_WATCHER */
    IToken *i_getToken();
#endif /* VBOX_WITH_GENERIC_SESSION_WATCHER */
    // getClientToken must be only used by callers who can guarantee that
    // the object cannot be deleted in the mean time, i.e. have a caller/lock.
    ClientToken *i_getClientToken();

    HRESULT i_onNetworkAdapterChange(INetworkAdapter *networkAdapter, BOOL changeAdapter);
    HRESULT i_onNATRedirectRuleChange(ULONG ulSlot, BOOL aNatRuleRemove, IN_BSTR aRuleName,
                                      NATProtocol_T aProto, IN_BSTR aHostIp, LONG aHostPort,
                                      IN_BSTR aGuestIp, LONG aGuestPort);
    HRESULT i_onStorageControllerChange();
    HRESULT i_onMediumChange(IMediumAttachment *aMediumAttachment, BOOL aForce);
    HRESULT i_onAudioAdapterChange(IAudioAdapter *audioAdapter);
    HRESULT i_onSerialPortChange(ISerialPort *serialPort);
    HRESULT i_onParallelPortChange(IParallelPort *parallelPort);
    HRESULT i_onCPUChange(ULONG aCPU, BOOL aRemove);
    HRESULT i_onVRDEServerChange(BOOL aRestart);
    HRESULT i_onRecordingChange(BOOL aEnable);
    HRESULT i_onUSBControllerChange();
    HRESULT i_onUSBDeviceAttach(IUSBDevice *aDevice,
                                IVirtualBoxErrorInfo *aError,
                                ULONG aMaskedIfs,
                                const com::Utf8Str &aCaptureFilename);
    HRESULT i_onUSBDeviceDetach(IN_BSTR aId,
                                IVirtualBoxErrorInfo *aError);
    HRESULT i_onSharedFolderChange();
    HRESULT i_onClipboardModeChange(ClipboardMode_T aClipboardMode);
    HRESULT i_onDnDModeChange(DnDMode_T aDnDMode);
    HRESULT i_onBandwidthGroupChange(IBandwidthGroup *aBandwidthGroup);
    HRESULT i_onStorageDeviceChange(IMediumAttachment *aMediumAttachment, BOOL aRemove, BOOL aSilent);
    HRESULT i_onCPUExecutionCapChange(ULONG aCpuExecutionCap);

    bool i_hasMatchingUSBFilter(const ComObjPtr<HostUSBDevice> &aDevice, ULONG *aMaskedIfs);

    HRESULT i_lockMedia();
    HRESULT i_unlockMedia();

    HRESULT i_saveStateWithReason(Reason_T aReason, ComPtr<IProgress> &aProgress);

private:

    // wrapped IInternalMachineControl properties

    // wrapped IInternalMachineControl methods
    HRESULT setRemoveSavedStateFile(BOOL aRemove);
    HRESULT updateState(MachineState_T aState);
    HRESULT beginPowerUp(const ComPtr<IProgress> &aProgress);
    HRESULT endPowerUp(LONG aResult);
    HRESULT beginPoweringDown(ComPtr<IProgress> &aProgress);
    HRESULT endPoweringDown(LONG aResult,
                            const com::Utf8Str &aErrMsg);
    HRESULT runUSBDeviceFilters(const ComPtr<IUSBDevice> &aDevice,
                                BOOL *aMatched,
                                ULONG *aMaskedInterfaces);
    HRESULT captureUSBDevice(const com::Guid &aId, const com::Utf8Str &aCaptureFilename);
    HRESULT detachUSBDevice(const com::Guid &aId,
                            BOOL aDone);
    HRESULT autoCaptureUSBDevices();
    HRESULT detachAllUSBDevices(BOOL aDone);
    HRESULT onSessionEnd(const ComPtr<ISession> &aSession,
                         ComPtr<IProgress> &aProgress);
    HRESULT finishOnlineMergeMedium();
    HRESULT pullGuestProperties(std::vector<com::Utf8Str> &aNames,
                                std::vector<com::Utf8Str> &aValues,
                                std::vector<LONG64> &aTimestamps,
                                std::vector<com::Utf8Str> &aFlags);
    HRESULT pushGuestProperty(const com::Utf8Str &aName,
                              const com::Utf8Str &aValue,
                              LONG64 aTimestamp,
                              const com::Utf8Str &aFlags);
    HRESULT lockMedia();
    HRESULT unlockMedia();
    HRESULT ejectMedium(const ComPtr<IMediumAttachment> &aAttachment,
                        ComPtr<IMediumAttachment> &aNewAttachment);
    HRESULT reportVmStatistics(ULONG aValidStats,
                               ULONG aCpuUser,
                               ULONG aCpuKernel,
                               ULONG aCpuIdle,
                               ULONG aMemTotal,
                               ULONG aMemFree,
                               ULONG aMemBalloon,
                               ULONG aMemShared,
                               ULONG aMemCache,
                               ULONG aPagedTotal,
                               ULONG aMemAllocTotal,
                               ULONG aMemFreeTotal,
                               ULONG aMemBalloonTotal,
                               ULONG aMemSharedTotal,
                               ULONG aVmNetRx,
                               ULONG aVmNetTx);
    HRESULT authenticateExternal(const std::vector<com::Utf8Str> &aAuthParams,
                                 com::Utf8Str &aResult);


    struct ConsoleTaskData
    {
        ConsoleTaskData()
            : mLastState(MachineState_Null),
              mDeleteSnapshotInfo(NULL)
        { }

        MachineState_T mLastState;
        ComObjPtr<Progress> mProgress;

        // used when deleting online snaphshot
        void *mDeleteSnapshotInfo;
    };

    class SaveStateTask;
    class SnapshotTask;
    class TakeSnapshotTask;
    class DeleteSnapshotTask;
    class RestoreSnapshotTask;

    void i_saveStateHandler(SaveStateTask &aTask);

    // Override some functionality for SessionMachine, this is where the
    // real action happens (the Machine methods are just dummies).
    HRESULT saveState(ComPtr<IProgress> &aProgress);
    HRESULT adoptSavedState(const com::Utf8Str &aSavedStateFile);
    HRESULT discardSavedState(BOOL aFRemoveFile);
    HRESULT takeSnapshot(const com::Utf8Str &aName,
                         const com::Utf8Str &aDescription,
                         BOOL aPause,
                         com::Guid &aId,
                         ComPtr<IProgress> &aProgress);
    HRESULT deleteSnapshot(const com::Guid &aId,
                           ComPtr<IProgress> &aProgress);
    HRESULT deleteSnapshotAndAllChildren(const com::Guid &aId,
                                         ComPtr<IProgress> &aProgress);
    HRESULT deleteSnapshotRange(const com::Guid &aStartId,
                                const com::Guid &aEndId,
                                ComPtr<IProgress> &aProgress);
    HRESULT restoreSnapshot(const ComPtr<ISnapshot> &aSnapshot,
                            ComPtr<IProgress> &aProgress);

    void i_releaseSavedStateFile(const Utf8Str &strSavedStateFile, Snapshot *pSnapshotToIgnore);

    void i_takeSnapshotHandler(TakeSnapshotTask &aTask);
    static void i_takeSnapshotProgressCancelCallback(void *pvUser);
    HRESULT i_finishTakingSnapshot(TakeSnapshotTask &aTask, AutoWriteLock &alock, bool aSuccess);
    HRESULT i_deleteSnapshot(const com::Guid &aStartId,
                             const com::Guid &aEndId,
                             BOOL aDeleteAllChildren,
                             ComPtr<IProgress> &aProgress);
    void i_deleteSnapshotHandler(DeleteSnapshotTask &aTask);
    void i_restoreSnapshotHandler(RestoreSnapshotTask &aTask);

    HRESULT i_prepareDeleteSnapshotMedium(const ComObjPtr<Medium> &aHD,
                                          const Guid &machineId,
                                          const Guid &snapshotId,
                                          bool fOnlineMergePossible,
                                          MediumLockList *aVMMALockList,
                                          ComObjPtr<Medium> &aSource,
                                          ComObjPtr<Medium> &aTarget,
                                          bool &fMergeForward,
                                          ComObjPtr<Medium> &pParentForTarget,
                                          MediumLockList * &aChildrenToReparent,
                                          bool &fNeedOnlineMerge,
                                          MediumLockList * &aMediumLockList,
                                          ComPtr<IToken> &aHDLockToken);
    void i_cancelDeleteSnapshotMedium(const ComObjPtr<Medium> &aHD,
                                      const ComObjPtr<Medium> &aSource,
                                      MediumLockList *aChildrenToReparent,
                                      bool fNeedsOnlineMerge,
                                      MediumLockList *aMediumLockList,
                                      const ComPtr<IToken> &aHDLockToken,
                                      const Guid &aMediumId,
                                      const Guid &aSnapshotId);
    HRESULT i_onlineMergeMedium(const ComObjPtr<MediumAttachment> &aMediumAttachment,
                                const ComObjPtr<Medium> &aSource,
                                const ComObjPtr<Medium> &aTarget,
                                bool fMergeForward,
                                const ComObjPtr<Medium> &pParentForTarget,
                                MediumLockList *aChildrenToReparent,
                                MediumLockList *aMediumLockList,
                                ComObjPtr<Progress> &aProgress,
                                bool *pfNeedsMachineSaveSettings);

    HRESULT i_setMachineState(MachineState_T aMachineState);
    HRESULT i_updateMachineStateOnClient();

    bool mRemoveSavedState;

    ConsoleTaskData mConsoleTaskData;

    /** client token for this machine */
    ClientToken *mClientToken;

    int miNATNetworksStarted;

    AUTHLIBRARYCONTEXT mAuthLibCtx;
};

// SnapshotMachine class
////////////////////////////////////////////////////////////////////////////////

/**
 *  @note Notes on locking objects of this class:
 *  SnapshotMachine shares some data with the primary Machine instance (pointed
 *  to by the |mPeer| member). In order to provide data consistency it also
 *  shares its lock handle. This means that whenever you lock a SessionMachine
 *  instance using Auto[Reader]Lock or AutoMultiLock, the corresponding Machine
 *  instance is also locked in the same lock mode. Keep it in mind.
 */
class ATL_NO_VTABLE SnapshotMachine :
    public Machine
{
public:
    VIRTUALBOXBASE_ADD_ERRORINFO_SUPPORT(SnapshotMachine, IMachine)

    DECLARE_NOT_AGGREGATABLE(SnapshotMachine)

    DECLARE_PROTECT_FINAL_CONSTRUCT()

    BEGIN_COM_MAP(SnapshotMachine)
        COM_INTERFACE_ENTRY(ISupportErrorInfo)
        COM_INTERFACE_ENTRY(IMachine)
        COM_INTERFACE_ENTRY2(IDispatch, IMachine)
        VBOX_TWEAK_INTERFACE_ENTRY(IMachine)
    END_COM_MAP()

    DECLARE_EMPTY_CTOR_DTOR(SnapshotMachine)

    HRESULT FinalConstruct();
    void FinalRelease();

    // public initializer/uninitializer for internal purposes only
    HRESULT init(SessionMachine *aSessionMachine,
                 IN_GUID aSnapshotId,
                 const Utf8Str &aStateFilePath);
    HRESULT initFromSettings(Machine *aMachine,
                             const settings::Hardware &hardware,
                             const settings::Debugging *pDbg,
                             const settings::Autostart *pAutostart,
                             IN_GUID aSnapshotId,
                             const Utf8Str &aStateFilePath);
    void uninit();

    // util::Lockable interface
    RWLockHandle *lockHandle() const;

    // public methods only for internal purposes

    virtual bool i_isSnapshotMachine() const
    {
        return true;
    }

    HRESULT i_onSnapshotChange(Snapshot *aSnapshot);

    // unsafe inline public methods for internal purposes only (ensure there is
    // a caller and a read lock before calling them!)

    const Guid& i_getSnapshotId() const { return mSnapshotId; }

private:

    Guid mSnapshotId;
    /** This field replaces mPeer for SessionMachine instances, as having
     * a peer reference is plain meaningless and causes many subtle problems
     * with saving settings and the like. */
    Machine * const mMachine;

    friend class Snapshot;
};

// third party methods that depend on SnapshotMachine definition

inline const Guid &Machine::i_getSnapshotId() const
{
    return (i_isSnapshotMachine())
                ? static_cast<const SnapshotMachine*>(this)->i_getSnapshotId()
                : Guid::Empty;
}


#endif /* !MAIN_INCLUDED_MachineImpl_h */
/* vi: set tabstop=4 shiftwidth=4 expandtab: */
