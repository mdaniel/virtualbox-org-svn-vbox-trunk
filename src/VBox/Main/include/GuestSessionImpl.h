/* $Id$ */
/** @file
 * VirtualBox Main - Guest session handling.
 */

/*
 * Copyright (C) 2012-2018 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ____H_GUESTSESSIONIMPL
#define ____H_GUESTSESSIONIMPL

#include "GuestSessionWrap.h"
#include "EventImpl.h"

#include "GuestCtrlImplPrivate.h"
#include "GuestProcessImpl.h"
#include "GuestDirectoryImpl.h"
#include "GuestFileImpl.h"
#include "GuestFsObjInfoImpl.h"
#include "ThreadTask.h"

#include <iprt/isofs.h> /* For UpdateAdditions. */

#include <deque>

class Guest;
class GuestSessionTaskInternalOpen;

/**
 * Abstract base class for a lenghtly per-session operation which
 * runs in a Main worker thread.
 */
class GuestSessionTask : public ThreadTask
{
public:

    GuestSessionTask(GuestSession *pSession);

    virtual ~GuestSessionTask(void);

public:

    virtual int Run(void) = 0;
    void handler()
    {
        int vrc = Run();
        NOREF(vrc);
        /** @todo
         *
         * r=bird: what was your idea WRT to Run status code and async tasks?
         *
         */
    }

    int RunAsync(const Utf8Str &strDesc, ComObjPtr<Progress> &pProgress);

    HRESULT Init(const Utf8Str &strTaskDesc)
    {
        HRESULT hr = S_OK;
        setTaskDesc(strTaskDesc);
        hr = createAndSetProgressObject();
        return hr;
    }

    const ComObjPtr<Progress>& GetProgressObject() const { return mProgress; }

protected:

    /** @name Directory handling primitives.
     * @{ */
    int directoryCreate(const com::Utf8Str &strPath, DirectoryCreateFlag_T enmDirecotryCreateFlags, uint32_t uMode,
                        bool fFollowSymlinks);
    /** @}  */

    /** @name File handling primitives.
     * @{ */
    int fileCopyFromEx(const Utf8Str &strSource, const Utf8Str &strDest, FileCopyFlag_T enmFileCopyFlags,
                       PRTFILE pFile, uint64_t cbOffset, uint64_t cbSize);
    int fileCopyFrom(const Utf8Str &strSource, const Utf8Str &strDest, FileCopyFlag_T enmFileCopyFlags);
    int fileCopyToEx(const Utf8Str &strSource, const Utf8Str &strDest, FileCopyFlag_T enmFileCopyFlags, PRTFILE pFile,
                     uint64_t cbOffset, uint64_t cbSize); /**< r=bird: 'cbOffset' makes no sense what so ever. It should be 'off', or do you mean sizeof(uint64_t)? */
    int fileCopyTo(const Utf8Str &strSource, const Utf8Str &strDest, FileCopyFlag_T enmFileCopyFlags);
    /** @}  */

    /** @name Guest property handling primitives.
     * @{ */
    int getGuestProperty(const ComObjPtr<Guest> &pGuest, const Utf8Str &strPath, Utf8Str &strValue);
    /** @}  */

    /** @name Path handling primitives.
     * @{ */
    int pathConstructOnGuest(const Utf8Str &strSourceRoot, const Utf8Str &strSource, const Utf8Str &strDest, Utf8Str &strOut);
    /** @}  */

    int setProgress(ULONG uPercent);
    int setProgressSuccess(void);
    HRESULT setProgressErrorMsg(HRESULT hr, const Utf8Str &strMsg);

    inline void setTaskDesc(const Utf8Str &strTaskDesc) throw()
    {
        mDesc = strTaskDesc;
    }

    HRESULT createAndSetProgressObject();

protected:

    Utf8Str                 mDesc;
    /** The guest session object this task is working on. */
    ComObjPtr<GuestSession> mSession;
    /** Progress object for getting updated when running
     *  asynchronously. Optional. */
    ComObjPtr<Progress>     mProgress;
};

/**
 * Task for opening a guest session.
 */
class SessionTaskOpen : public GuestSessionTask
{
public:

    SessionTaskOpen(GuestSession *pSession,
                    uint32_t uFlags,
                    uint32_t uTimeoutMS);
    virtual ~SessionTaskOpen(void);
    int Run(void);

protected:

    /** Session creation flags. */
    uint32_t mFlags;
    /** Session creation timeout (in ms). */
    uint32_t mTimeoutMS;
};

/**
 * Task for copying directories from guest to the host.
 */
class SessionTaskCopyDirFrom : public GuestSessionTask
{
public:

    SessionTaskCopyDirFrom(GuestSession *pSession, const Utf8Str &strSource, const Utf8Str &strDest, const Utf8Str &strFilter,
                           DirectoryCopyFlag_T enmDirCopyFlags);
    virtual ~SessionTaskCopyDirFrom(void);
    int Run(void);

protected:

    int directoryCopyToHost(const Utf8Str &strSource, const Utf8Str &strFilter, const Utf8Str &strDest, bool fRecursive,
                            bool fFollowSymlinks, const Utf8Str &strSubDir /* For recursion. */);
protected:

    Utf8Str             mSource;
    Utf8Str             mDest;
    Utf8Str             mFilter;
    DirectoryCopyFlag_T mDirCopyFlags;
};

/**
 * Task for copying directories from host to the guest.
 */
class SessionTaskCopyDirTo : public GuestSessionTask
{
public:

    SessionTaskCopyDirTo(GuestSession *pSession, const Utf8Str &strSource, const Utf8Str &strDest, const Utf8Str &strFilter,
                         DirectoryCopyFlag_T enmDirCopyFlags);
    virtual ~SessionTaskCopyDirTo(void);
    int Run(void);

protected:

    int directoryCopyToGuest(const Utf8Str &strSource, const Utf8Str &strFilter, const Utf8Str &strDest, bool fRecursive,
                             bool fFollowSymlinks, const Utf8Str &strSubDir /* For recursion. */);
protected:

    Utf8Str             mSource;
    Utf8Str             mDest;
    Utf8Str             mFilter;
    DirectoryCopyFlag_T mDirCopyFlags;
};

/**
 * Task for copying files from host to the guest.
 */
class SessionTaskCopyFileTo : public GuestSessionTask
{
public:

    SessionTaskCopyFileTo(GuestSession *pSession,
                          const Utf8Str &strSource, const Utf8Str &strDest, FileCopyFlag_T enmFileCopyFlags);
    SessionTaskCopyFileTo(GuestSession *pSession,
                          PRTFILE pSourceFile, size_t cbSourceOffset, uint64_t cbSourceSize,
                          const Utf8Str &strDest, FileCopyFlag_T enmFileCopyFlags);
    virtual ~SessionTaskCopyFileTo(void);
    int Run(void);

protected:

    Utf8Str        mSource;
    PRTFILE        mSourceFile;
    size_t         mSourceOffset;
    uint64_t       mSourceSize;
    Utf8Str        mDest;
    FileCopyFlag_T mFileCopyFlags;
};

/**
 * Task for copying files from guest to the host.
 */
class SessionTaskCopyFileFrom : public GuestSessionTask
{
public:

    SessionTaskCopyFileFrom(GuestSession *pSession,
                        const Utf8Str &strSource, const Utf8Str &strDest, FileCopyFlag_T enmFileCopyFlags);
    virtual ~SessionTaskCopyFileFrom(void);
    int Run(void);

protected:

    Utf8Str        mSource;
    Utf8Str        mDest;
    FileCopyFlag_T mFileCopyFlags;
};

/**
 * Task for automatically updating the Guest Additions on the guest.
 */
class SessionTaskUpdateAdditions : public GuestSessionTask
{
public:

    SessionTaskUpdateAdditions(GuestSession *pSession,
                               const Utf8Str &strSource, const ProcessArguments &aArguments,
                               uint32_t uFlags);
    virtual ~SessionTaskUpdateAdditions(void);
    int Run(void);

protected:

    /**
     * Suported OS types for automatic updating.
     */
    enum eOSType
    {
        eOSType_Unknown = 0,
        eOSType_Windows = 1,
        eOSType_Linux   = 2,
        eOSType_Solaris = 3
    };

    /**
     * Structure representing a file to
     * get off the .ISO, copied to the guest.
     */
    struct InstallerFile
    {
        InstallerFile(const Utf8Str &aSource,
                      const Utf8Str &aDest,
                      uint32_t       aFlags = 0)
            : strSource(aSource),
              strDest(aDest),
              fFlags(aFlags) { }

        InstallerFile(const Utf8Str          &aSource,
                      const Utf8Str          &aDest,
                      uint32_t                aFlags,
                      const GuestProcessStartupInfo &aStartupInfo)
            : strSource(aSource),
              strDest(aDest),
              fFlags(aFlags),
              mProcInfo(aStartupInfo)
        {
            mProcInfo.mExecutable = strDest;
            if (mProcInfo.mName.isEmpty())
                mProcInfo.mName = strDest;
        }

        /** Source file on .ISO. */
        Utf8Str                 strSource;
        /** Destination file on the guest. */
        Utf8Str                 strDest;
        /** File flags. */
        uint32_t                fFlags;
        /** Optional arguments if this file needs to be
         *  executed. */
        GuestProcessStartupInfo mProcInfo;
    };

    int addProcessArguments(ProcessArguments &aArgumentsDest, const ProcessArguments &aArgumentsSource);
    int copyFileToGuest(GuestSession *pSession, PRTISOFSFILE pISO, Utf8Str const &strFileSource, const Utf8Str &strFileDest, bool fOptional, uint32_t *pcbSize);
    int runFileOnGuest(GuestSession *pSession, GuestProcessStartupInfo &procInfo);

    /** Files to handle. */
    std::vector<InstallerFile> mFiles;
    /** The (optionally) specified Guest Additions .ISO on the host
     *  which will be used for the updating process. */
    Utf8Str                    mSource;
    /** (Optional) installer command line arguments. */
    ProcessArguments           mArguments;
    /** Update flags. */
    uint32_t                   mFlags;
};

/**
 * Guest session implementation.
 */
class ATL_NO_VTABLE GuestSession :
    public GuestSessionWrap,
    public GuestBase
{
public:
    /** @name COM and internal init/term/mapping cruft.
     * @{ */
    DECLARE_EMPTY_CTOR_DTOR(GuestSession)

    int     init(Guest *pGuest, const GuestSessionStartupInfo &ssInfo, const GuestCredentials &guestCreds);
    void    uninit(void);
    HRESULT FinalConstruct(void);
    void    FinalRelease(void);
    /** @}  */

private:

    /** Wrapped @name IGuestSession properties.
     * @{ */
    HRESULT getUser(com::Utf8Str &aUser);
    HRESULT getDomain(com::Utf8Str &aDomain);
    HRESULT getName(com::Utf8Str &aName);
    HRESULT getId(ULONG *aId);
    HRESULT getTimeout(ULONG *aTimeout);
    HRESULT setTimeout(ULONG aTimeout);
    HRESULT getProtocolVersion(ULONG *aProtocolVersion);
    HRESULT getStatus(GuestSessionStatus_T *aStatus);
    HRESULT getEnvironmentChanges(std::vector<com::Utf8Str> &aEnvironmentChanges);
    HRESULT setEnvironmentChanges(const std::vector<com::Utf8Str> &aEnvironmentChanges);
    HRESULT getEnvironmentBase(std::vector<com::Utf8Str> &aEnvironmentBase);
    HRESULT getProcesses(std::vector<ComPtr<IGuestProcess> > &aProcesses);
    HRESULT getPathStyle(PathStyle_T *aPathStyle);
    HRESULT getCurrentDirectory(com::Utf8Str &aCurrentDirectory);
    HRESULT setCurrentDirectory(const com::Utf8Str &aCurrentDirectory);
    HRESULT getUserDocuments(com::Utf8Str &aUserDocuments);
    HRESULT getUserHome(com::Utf8Str &aUserHome);
    HRESULT getDirectories(std::vector<ComPtr<IGuestDirectory> > &aDirectories);
    HRESULT getFiles(std::vector<ComPtr<IGuestFile> > &aFiles);
    HRESULT getEventSource(ComPtr<IEventSource> &aEventSource);
    /** @}  */

    /** Wrapped @name IGuestSession methods.
     * @{ */
    HRESULT close();

    HRESULT directoryCopy(const com::Utf8Str &aSource,
                          const com::Utf8Str &aDestination,
                          const std::vector<DirectoryCopyFlag_T> &aFlags,
                          ComPtr<IProgress> &aProgress);
    HRESULT directoryCopyFromGuest(const com::Utf8Str &aSource,
                                   const com::Utf8Str &aDestination,
                                   const std::vector<DirectoryCopyFlag_T> &aFlags,
                                   ComPtr<IProgress> &aProgress);
    HRESULT directoryCopyToGuest(const com::Utf8Str &aSource,
                                 const com::Utf8Str &aDestination,
                                 const std::vector<DirectoryCopyFlag_T> &aFlags,
                                 ComPtr<IProgress> &aProgress);
    HRESULT directoryCreate(const com::Utf8Str &aPath,
                            ULONG aMode,
                            const std::vector<DirectoryCreateFlag_T> &aFlags);
    HRESULT directoryCreateTemp(const com::Utf8Str &aTemplateName,
                                ULONG aMode,
                                const com::Utf8Str &aPath,
                                BOOL aSecure,
                                com::Utf8Str &aDirectory);
    HRESULT directoryExists(const com::Utf8Str &aPath,
                            BOOL aFollowSymlinks,
                            BOOL *aExists);
    HRESULT directoryOpen(const com::Utf8Str &aPath,
                          const com::Utf8Str &aFilter,
                          const std::vector<DirectoryOpenFlag_T> &aFlags,
                          ComPtr<IGuestDirectory> &aDirectory);
    HRESULT directoryRemove(const com::Utf8Str &aPath);
    HRESULT directoryRemoveRecursive(const com::Utf8Str &aPath,
                                     const std::vector<DirectoryRemoveRecFlag_T> &aFlags,
                                     ComPtr<IProgress> &aProgress);
    HRESULT environmentScheduleSet(const com::Utf8Str &aName,
                                   const com::Utf8Str &aValue);
    HRESULT environmentScheduleUnset(const com::Utf8Str &aName);
    HRESULT environmentGetBaseVariable(const com::Utf8Str &aName,
                                       com::Utf8Str &aValue);
    HRESULT environmentDoesBaseVariableExist(const com::Utf8Str &aName,
                                             BOOL *aExists);

    HRESULT fileCopy(const com::Utf8Str &aSource,
                     const com::Utf8Str &aDestination,
                     const std::vector<FileCopyFlag_T> &aFlags,
                     ComPtr<IProgress> &aProgress);
    HRESULT fileCopyToGuest(const com::Utf8Str &aSource,
                            const com::Utf8Str &aDestination,
                            const std::vector<FileCopyFlag_T> &aFlags,
                            ComPtr<IProgress> &aProgress);
    HRESULT fileCopyFromGuest(const com::Utf8Str &aSource,
                              const com::Utf8Str &aDestination,
                              const std::vector<FileCopyFlag_T> &aFlags,
                              ComPtr<IProgress> &aProgress);
    HRESULT fileCreateTemp(const com::Utf8Str &aTemplateName,
                           ULONG aMode,
                           const com::Utf8Str &aPath,
                           BOOL aSecure,
                           ComPtr<IGuestFile> &aFile);
    HRESULT fileExists(const com::Utf8Str &aPath,
                       BOOL aFollowSymlinks,
                       BOOL *aExists);
    HRESULT fileOpen(const com::Utf8Str &aPath,
                     FileAccessMode_T aAccessMode,
                     FileOpenAction_T aOpenAction,
                     ULONG aCreationMode,
                     ComPtr<IGuestFile> &aFile);
    HRESULT fileOpenEx(const com::Utf8Str &aPath,
                       FileAccessMode_T aAccessMode,
                       FileOpenAction_T aOpenAction,
                       FileSharingMode_T aSharingMode,
                       ULONG aCreationMode,
                       const std::vector<FileOpenExFlag_T> &aFlags,
                       ComPtr<IGuestFile> &aFile);
    HRESULT fileQuerySize(const com::Utf8Str &aPath,
                          BOOL aFollowSymlinks,
                          LONG64 *aSize);
    HRESULT fsObjExists(const com::Utf8Str &aPath,
                        BOOL aFollowSymlinks,
                        BOOL *pfExists);
    HRESULT fsObjQueryInfo(const com::Utf8Str &aPath,
                           BOOL aFollowSymlinks,
                           ComPtr<IGuestFsObjInfo> &aInfo);
    HRESULT fsObjRemove(const com::Utf8Str &aPath);
    HRESULT fsObjRename(const com::Utf8Str &aOldPath,
                        const com::Utf8Str &aNewPath,
                        const std::vector<FsObjRenameFlag_T> &aFlags);
    HRESULT fsObjMove(const com::Utf8Str &aSource,
                      const com::Utf8Str &aDestination,
                      const std::vector<FsObjMoveFlag_T> &aFlags,
                      ComPtr<IProgress> &aProgress);
    HRESULT fsObjSetACL(const com::Utf8Str &aPath,
                        BOOL aFollowSymlinks,
                        const com::Utf8Str &aAcl,
                        ULONG aMode);
    HRESULT processCreate(const com::Utf8Str &aCommand,
                          const std::vector<com::Utf8Str> &aArguments,
                          const std::vector<com::Utf8Str> &aEnvironment,
                          const std::vector<ProcessCreateFlag_T> &aFlags,
                          ULONG aTimeoutMS,
                          ComPtr<IGuestProcess> &aGuestProcess);
    HRESULT processCreateEx(const com::Utf8Str &aCommand,
                            const std::vector<com::Utf8Str> &aArguments,
                            const std::vector<com::Utf8Str> &aEnvironment,
                            const std::vector<ProcessCreateFlag_T> &aFlags,
                            ULONG aTimeoutMS,
                            ProcessPriority_T aPriority,
                            const std::vector<LONG> &aAffinity,
                            ComPtr<IGuestProcess> &aGuestProcess);
    HRESULT processGet(ULONG aPid,
                       ComPtr<IGuestProcess> &aGuestProcess);
    HRESULT symlinkCreate(const com::Utf8Str &aSource,
                          const com::Utf8Str &aTarget,
                          SymlinkType_T aType);
    HRESULT symlinkExists(const com::Utf8Str &aSymlink,
                          BOOL *aExists);
    HRESULT symlinkRead(const com::Utf8Str &aSymlink,
                        const std::vector<SymlinkReadFlag_T> &aFlags,
                        com::Utf8Str &aTarget);
    HRESULT waitFor(ULONG aWaitFor,
                    ULONG aTimeoutMS,
                    GuestSessionWaitResult_T *aReason);
    HRESULT waitForArray(const std::vector<GuestSessionWaitForFlag_T> &aWaitFor,
                         ULONG aTimeoutMS,
                         GuestSessionWaitResult_T *aReason);
    /** @}  */

    /** Map of guest directories. The key specifies the internal directory ID. */
    typedef std::map <uint32_t, ComObjPtr<GuestDirectory> > SessionDirectories;
    /** Map of guest files. The key specifies the internal file ID. */
    typedef std::map <uint32_t, ComObjPtr<GuestFile> > SessionFiles;
    /** Map of guest processes. The key specifies the internal process number.
     *  To retrieve the process' guest PID use the Id() method of the IProcess interface. */
    typedef std::map <uint32_t, ComObjPtr<GuestProcess> > SessionProcesses;

    /** Guest session object type enumeration. */
    enum SESSIONOBJECTTYPE
    {
        /** Anonymous object. */
        SESSIONOBJECTTYPE_ANONYMOUS  = 0,
        /** Session object. */
        SESSIONOBJECTTYPE_SESSION    = 1,
        /** Directory object. */
        SESSIONOBJECTTYPE_DIRECTORY  = 2,
        /** File object. */
        SESSIONOBJECTTYPE_FILE       = 3,
        /** Process object. */
        SESSIONOBJECTTYPE_PROCESS    = 4,
        /** The usual 32-bit hack. */
        SESSIONOBJECTTYPE_32BIT_HACK = 0x7fffffff
    };

    struct SessionObject
    {
        /** Creation timestamp (in ms). */
        uint64_t          tsCreatedMs;
        /** The object type. */
        SESSIONOBJECTTYPE enmType;
    };

    /** Map containing all objects bound to a guest session.
     *  The key specifies the (global) context ID. */
    typedef std::map <uint32_t, SessionObject> SessionObjects;
    /** Queue containing context IDs which are no longer in use.
     *  Useful for quickly retrieving a new, unused context ID. */
    typedef std::deque <uint32_t>              SessionObjectsFree;

public:
    /** @name Public internal methods.
     * @todo r=bird: Most of these are public for no real reason...
     * @{ */
    int                     i_closeSession(uint32_t uFlags, uint32_t uTimeoutMS, int *pGuestRc);
    inline bool             i_directoryExists(uint32_t uDirID, ComObjPtr<GuestDirectory> *pDir);
    int                     i_directoryUnregister(GuestDirectory *pDirectory);
    int                     i_directoryRemove(const Utf8Str &strPath, uint32_t uFlags, int *pGuestRc);
    int                     i_directoryCreate(const Utf8Str &strPath, uint32_t uMode, uint32_t uFlags, int *pGuestRc);
    int                     i_directoryOpen(const GuestDirectoryOpenInfo &openInfo,
                                            ComObjPtr<GuestDirectory> &pDirectory, int *pGuestRc);
    int                     i_directoryQueryInfo(const Utf8Str &strPath, bool fFollowSymlinks, GuestFsObjData &objData, int *pGuestRc);
    int                     i_dispatchToObject(PVBOXGUESTCTRLHOSTCBCTX pCtxCb, PVBOXGUESTCTRLHOSTCALLBACK pSvcCb);
    int                     i_dispatchToThis(PVBOXGUESTCTRLHOSTCBCTX pCtxCb, PVBOXGUESTCTRLHOSTCALLBACK pSvcCb);
    inline bool             i_fileExists(uint32_t uFileID, ComObjPtr<GuestFile> *pFile);
    int                     i_fileUnregister(GuestFile *pFile);
    int                     i_fileRemove(const Utf8Str &strPath, int *pGuestRc);
    int                     i_fileOpen(const GuestFileOpenInfo &openInfo, ComObjPtr<GuestFile> &pFile, int *pGuestRc);
    int                     i_fileQueryInfo(const Utf8Str &strPath, bool fFollowSymlinks, GuestFsObjData &objData, int *pGuestRc);
    int                     i_fileQuerySize(const Utf8Str &strPath, bool fFollowSymlinks, int64_t *pllSize, int *pGuestRc);
    int                     i_fsCreateTemp(const Utf8Str &strTemplate, const Utf8Str &strPath, bool fDirectory,
                                           Utf8Str &strName, int *pGuestRc);
    int                     i_fsQueryInfo(const Utf8Str &strPath, bool fFollowSymlinks, GuestFsObjData &objData, int *pGuestRc);
    const GuestCredentials &i_getCredentials(void);
    EventSource            *i_getEventSource(void) { return mEventSource; }
    Utf8Str                 i_getName(void);
    ULONG                   i_getId(void) { return mData.mSession.mID; }
    static Utf8Str          i_guestErrorToString(int guestRc);
    HRESULT                 i_isReadyExternal(void);
    int                     i_onRemove(void);
    int                     i_onSessionStatusChange(PVBOXGUESTCTRLHOSTCBCTX pCbCtx, PVBOXGUESTCTRLHOSTCALLBACK pSvcCbData);
    int                     i_startSession(int *pGuestRc);
    int                     i_startSessionAsync(void);
    static void             i_startSessionThreadTask(GuestSessionTaskInternalOpen *pTask);
    Guest                  *i_getParent(void) { return mParent; }
    uint32_t                i_getProtocolVersion(void) { return mData.mProtocolVersion; }
    int                     i_objectRegister(SESSIONOBJECTTYPE enmType, uint32_t *puObjectID);
    int                     i_objectRegisterEx(SESSIONOBJECTTYPE enmType, uint32_t fFlags, uint32_t *puObjectID);
    int                     i_objectUnregister(uint32_t uObjectID);
    int                     i_pathRename(const Utf8Str &strSource, const Utf8Str &strDest, uint32_t uFlags, int *pGuestRc);
    int                     i_pathUserDocuments(Utf8Str &strPath, int *prcGuest);
    int                     i_pathUserHome(Utf8Str &strPath, int *prcGuest);
    int                     i_processUnregister(GuestProcess *pProcess);
    int                     i_processCreateEx(GuestProcessStartupInfo &procInfo, ComObjPtr<GuestProcess> &pProgress);
    inline bool             i_processExists(uint32_t uProcessID, ComObjPtr<GuestProcess> *pProcess);
    inline int              i_processGetByPID(ULONG uPID, ComObjPtr<GuestProcess> *pProcess);
    int                     i_sendCommand(uint32_t uFunction, uint32_t uParms, PVBOXHGCMSVCPARM paParms);
    static HRESULT          i_setErrorExternal(VirtualBoxBase *pInterface, int guestRc);
    int                     i_setSessionStatus(GuestSessionStatus_T sessionStatus, int sessionRc);
    int                     i_signalWaiters(GuestSessionWaitResult_T enmWaitResult, int rc /*= VINF_SUCCESS */);
    int                     i_determineProtocolVersion(void);
    int                     i_waitFor(uint32_t fWaitFlags, ULONG uTimeoutMS, GuestSessionWaitResult_T &waitResult, int *pGuestRc);
    int                     i_waitForStatusChange(GuestWaitEvent *pEvent, uint32_t fWaitFlags, uint32_t uTimeoutMS,
                                                  GuestSessionStatus_T *pSessionStatus, int *pGuestRc);
    /** @}  */

private:

    /** Pointer to the parent (Guest). */
    Guest                          *mParent;
    /**
     * The session's event source. This source is used for
     * serving the internal listener as well as all other
     * external listeners that may register to it.
     *
     * Note: This can safely be used without holding any locks.
     * An AutoCaller suffices to prevent it being destroy while in use and
     * internally there is a lock providing the necessary serialization.
     */
    const ComObjPtr<EventSource>    mEventSource;

    struct Data
    {
        /** The session credentials. */
        GuestCredentials            mCredentials;
        /** The session's startup info. */
        GuestSessionStartupInfo     mSession;
        /** The session's object ID.
         *  Needed for registering wait events which are bound directly to this session. */
        uint32_t                    mObjectID;
        /** The session's current status. */
        GuestSessionStatus_T        mStatus;
        /** The set of environment changes for the session for use when
         *  creating new guest processes. */
        GuestEnvironmentChanges     mEnvironmentChanges;
        /** Pointer to the immutable base environment for the session.
         * @note This is not allocated until the guest reports it to the host. It is
         *       also shared with child processes. */
        GuestEnvironment const     *mpBaseEnvironment;
        /** Directory objects bound to this session. */
        SessionDirectories          mDirectories;
        /** File objects bound to this session. */
        SessionFiles                mFiles;
        /** Process objects bound to this session. */
        SessionProcesses            mProcesses;
        /** Map of registered session objects (files, directories, ...). */
        SessionObjects              mObjects;
        /** Queue of object IDs which are not used anymore (free list).
         *  Acts as a "free list" for the mObjects map. */
        SessionObjectsFree          mObjectsFree;
        /** Guest control protocol version to be used.
         *  Guest Additions < VBox 4.3 have version 1,
         *  any newer version will have version 2. */
        uint32_t                    mProtocolVersion;
        /** Session timeout (in ms). */
        uint32_t                    mTimeout;
        /** The last returned session status
         *  returned from the guest side. */
        int                         mRC;

        Data(void)
            : mpBaseEnvironment(NULL)
        { }
        Data(const Data &rThat)
            : mCredentials(rThat.mCredentials)
            , mSession(rThat.mSession)
            , mStatus(rThat.mStatus)
            , mEnvironmentChanges(rThat.mEnvironmentChanges)
            , mpBaseEnvironment(NULL)
            , mDirectories(rThat.mDirectories)
            , mFiles(rThat.mFiles)
            , mProcesses(rThat.mProcesses)
            , mObjects(rThat.mObjects)
            , mObjectsFree(rThat.mObjectsFree)
            , mProtocolVersion(rThat.mProtocolVersion)
            , mTimeout(rThat.mTimeout)
            , mRC(rThat.mRC)
        { }
        ~Data(void)
        {
            if (mpBaseEnvironment)
            {
                mpBaseEnvironment->releaseConst();
                mpBaseEnvironment = NULL;
            }
        }
    } mData;
};

#endif /* !____H_GUESTSESSIONIMPL */

