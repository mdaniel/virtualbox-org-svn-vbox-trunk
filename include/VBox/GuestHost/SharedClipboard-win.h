/** @file
 * Shared Clipboard - Common Guest and Host Code, for Windows OSes.
 */

/*
 * Copyright (C) 2006-2023 Oracle and/or its affiliates.
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
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL), a copy of it is provided in the "COPYING.CDDL" file included
 * in the VirtualBox distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 *
 * SPDX-License-Identifier: GPL-3.0-only OR CDDL-1.0
 */

#ifndef VBOX_INCLUDED_GuestHost_SharedClipboard_win_h
#define VBOX_INCLUDED_GuestHost_SharedClipboard_win_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <iprt/critsect.h>
#include <iprt/types.h>
#include <iprt/req.h>
#include <iprt/win/windows.h>

#include <VBox/GuestHost/SharedClipboard.h>

# ifdef VBOX_WITH_SHARED_CLIPBOARD_TRANSFERS
#  include <vector>

#  include <iprt/cpp/ministring.h> /* For RTCString. */
#  include <iprt/win/shlobj.h>     /* For DROPFILES and friends. */
#  include <VBox/com/string.h>     /* For Utf8Str. */
#  include <oleidl.h>

# include <VBox/GuestHost/SharedClipboard-transfers.h>

using namespace com;
# endif /* VBOX_WITH_SHARED_CLIPBOARD_TRANSFERS */

#ifndef WM_CLIPBOARDUPDATE
# define WM_CLIPBOARDUPDATE 0x031D
#endif

#define SHCL_WIN_WNDCLASS_NAME        "VBoxSharedClipboardClass"

/** See: https://docs.microsoft.com/en-us/windows/desktop/dataxchg/html-clipboard-format
 *       Do *not* change the name, as this will break compatbility with other (legacy) applications! */
#define SHCL_WIN_REGFMT_HTML          "HTML Format"

/** Default timeout (in ms) for passing down messages down the clipboard chain. */
#define SHCL_WIN_CBCHAIN_TIMEOUT_MS   5000

/** Reports clipboard formats. */
#define SHCL_WIN_WM_REPORT_FORMATS          WM_USER
/** Reads data from the clipboard and sends it to the destination. */
#define SHCL_WIN_WM_READ_DATA               WM_USER + 1
#ifdef VBOX_WITH_SHARED_CLIPBOARD_TRANSFERS
/** Starts a transfer on the guest.
 *  This creates the necessary IDataObject in the matching window thread. */
# define SHCL_WIN_WM_TRANSFER_START         WM_USER + 2
#endif

/* Dynamically load clipboard functions from User32.dll. */
typedef BOOL WINAPI FNADDCLIPBOARDFORMATLISTENER(HWND);
typedef FNADDCLIPBOARDFORMATLISTENER *PFNADDCLIPBOARDFORMATLISTENER;

typedef BOOL WINAPI FNREMOVECLIPBOARDFORMATLISTENER(HWND);
typedef FNREMOVECLIPBOARDFORMATLISTENER *PFNREMOVECLIPBOARDFORMATLISTENER;

/**
 * Structure for keeping function pointers for the new clipboard API.
 * If the new API is not available, those function pointer are NULL.
 */
typedef struct _SHCLWINAPINEW
{
    PFNADDCLIPBOARDFORMATLISTENER    pfnAddClipboardFormatListener;
    PFNREMOVECLIPBOARDFORMATLISTENER pfnRemoveClipboardFormatListener;
} SHCLWINAPINEW, *PSHCLWINAPINEW;

/**
 * Structure for keeping variables which are needed to drive the old clipboard API.
 */
typedef struct _SHCLWINAPIOLD
{
    /** Timer ID for the refresh timer. */
    UINT                   timerRefresh;
    /** Whether "pinging" the clipboard chain currently is in progress or not. */
    bool                   fCBChainPingInProcess;
} SHCLWINAPIOLD, *PSHCLWINAPIOLD;

#ifdef VBOX_WITH_SHARED_CLIPBOARD_TRANSFERS
/** Forward declaration for the Windows data object. */
class SharedClipboardWinDataObject;
#endif

/**
 * Structure for maintaining a Shared Clipboard context on Windows platforms.
 */
typedef struct _SHCLWINCTX
{
    /** Critical section to serialize access. */
    RTCRITSECT         CritSect;
    /** Window handle of our (invisible) clipbaord window. */
    HWND               hWnd;
    /** Window handle which is next to us in the clipboard chain. */
    HWND               hWndNextInChain;
    /** Window handle of the clipboard owner *if* we are the owner.
     * @todo r=bird: Ignore the misleading statement above.  This is only set to
     * NULL by the initialization code and then it's set to the clipboard owner
     * after we announce data to the clipboard.  So, essentially this will be our
     * windows handle or NULL.  End of story. */
    HWND               hWndClipboardOwnerUs;
    /** Structure for maintaining the new clipboard API. */
    SHCLWINAPINEW      newAPI;
    /** Structure for maintaining the old clipboard API. */
    SHCLWINAPIOLD      oldAPI;
#ifdef VBOX_WITH_SHARED_CLIPBOARD_TRANSFERS
    /** The "in-flight" data object for file transfers.
     *  This is the current data object which has been created and sent to the Windows clipboard.
     *  That way Windows knows that a potential file transfer is available, but the actual transfer
     *  hasn't been started yet.
     *  Can be NULL if currently not being used / no current "in-flight" transfer present. */
    SharedClipboardWinDataObject
                      *pDataObjInFlight;
#endif
    /** Request queue.
     *  Needed for processing HGCM requests within the HGCM (main) thread from the Windows event thread. */
    RTREQQUEUE         hReqQ;
} SHCLWINCTX, *PSHCLWINCTX;

int SharedClipboardWinOpen(HWND hWnd);
int SharedClipboardWinClose(void);
int SharedClipboardWinClear(void);

int SharedClipboardWinCtxInit(PSHCLWINCTX pWinCtx);
void SharedClipboardWinCtxDestroy(PSHCLWINCTX pWinCtx);

int SharedClipboardWinCheckAndInitNewAPI(PSHCLWINAPINEW pAPI);
bool SharedClipboardWinIsNewAPI(PSHCLWINAPINEW pAPI);

int SharedClipboardWinDataWrite(UINT cfFormat, void *pvData, uint32_t cbData);

int SharedClipboardWinChainAdd(PSHCLWINCTX pCtx);
int SharedClipboardWinChainRemove(PSHCLWINCTX pCtx);
VOID CALLBACK SharedClipboardWinChainPingProc(HWND hWnd, UINT uMsg, ULONG_PTR dwData, LRESULT lResult) RT_NOTHROW_DEF;
LRESULT SharedClipboardWinChainPassToNext(PSHCLWINCTX pWinCtx, UINT msg, WPARAM wParam, LPARAM lParam);

SHCLFORMAT SharedClipboardWinClipboardFormatToVBox(UINT uFormat);
int SharedClipboardWinGetFormats(PSHCLWINCTX pCtx, PSHCLFORMATS pfFormats);

int SharedClipboardWinGetCFHTMLHeaderValue(const char *pszSrc, const char *pszOption, uint32_t *puValue);
bool SharedClipboardWinIsCFHTML(const char *pszSource);
int SharedClipboardWinConvertCFHTMLToMIME(const char *pszSource, const uint32_t cch, char **ppszOutput, uint32_t *pcbOutput);
int SharedClipboardWinConvertMIMEToCFHTML(const char *pszSource, size_t cb, char **ppszOutput, uint32_t *pcbOutput);

LRESULT SharedClipboardWinHandleWMChangeCBChain(PSHCLWINCTX pWinCtx, HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
int SharedClipboardWinHandleWMDestroy(PSHCLWINCTX pWinCtx);
int SharedClipboardWinHandleWMRenderAllFormats(PSHCLWINCTX pWinCtx, HWND hWnd);
int SharedClipboardWinHandleWMTimer(PSHCLWINCTX pWinCtx);

int SharedClipboardWinClearAndAnnounceFormats(PSHCLWINCTX pWinCtx, SHCLFORMATS fFormats, HWND hWnd);

#ifdef VBOX_WITH_SHARED_CLIPBOARD_TRANSFERS
class SharedClipboardTransferList;
#  ifndef FILEGROUPDESCRIPTOR
class FILEGROUPDESCRIPTOR;
#  endif

/**
 * Generic Windows class implementing IDataObject for Shared Clipboard data transfers.
 */
class SharedClipboardWinDataObject : public IDataObject //, public IDataObjectAsyncCapability
{
public:

    /**
     * Structure for keeping a data object callback context.
     */
    struct CALLBACKCTX
    {
        /** Pointer to the data object of this callback. */
        SharedClipboardWinDataObject *pThis;
        /** User-supplied pointer to more context data. */
        void                         *pvUser;
    };
    /** Pointer to a Shared Clipboard Windows data object callback table. */
    typedef CALLBACKCTX *PCALLBACKCTX;

    /**
     * @name Shared Clipboard Windows data object callback table.
     */
    struct CALLBACKS
    {
        /**
         * Called by the data object if a transfer needs to be started.
         *
         * @returns VBox status code.
         * @param   pCbCtx          Pointer to callback context.
         */
        DECLCALLBACKMEMBER(int, pfnTransferBegin, (PCALLBACKCTX pCbCtx));
        /**
         * Called by the data object if a transfer has been ended (succeeded or failed).
         *
         * @returns VBox status code.
         * @param   pCbCtx          Pointer to callback context.
         * @param   pTransfer       Pointer to transfer being completed.
         * @param   rcTransfer      Result (IPRT-style) code.
         */
        DECLCALLBACKMEMBER(int, pfnTransferEnd, (PCALLBACKCTX pCbCtx, PSHCLTRANSFER pTransfer, int rcTransfer));
    };
    /** Pointer to a Shared Clipboard Windows data object callback table. */
    typedef CALLBACKS *PCALLBACKS;

    enum Status
    {
        /** The object is uninitialized (not ready). */
        Uninitialized = 0,
        /** The object is initialized and ready to use.
         *  A transfer is *not* running yet! */
        Initialized,
        /** Transfer is running. */
        Running,
        /** The operation has been successfully completed. */
        Completed,
        /** The operation has been canceled. */
        Canceled,
        /** An (unrecoverable) error occurred. */
        Error
    };

public:

    SharedClipboardWinDataObject(void);
    virtual ~SharedClipboardWinDataObject(void);

public:

    int Init(PSHCLCONTEXT pCtx, SharedClipboardWinDataObject::PCALLBACKS pCallbacks, LPFORMATETC pFormatEtc = NULL, LPSTGMEDIUM pStgMed = NULL, ULONG cFormats = 0);
    void Uninit(void);
    void Destroy(void);

public: /* IUnknown methods. */

    STDMETHOD(QueryInterface)(REFIID iid, void ** ppvObject);
    STDMETHOD_(ULONG, AddRef)(void);
    STDMETHOD_(ULONG, Release)(void);

public: /* IDataObject methods. */

    STDMETHOD(GetData)(LPFORMATETC pFormatEtc, LPSTGMEDIUM pMedium);
    STDMETHOD(GetDataHere)(LPFORMATETC pFormatEtc, LPSTGMEDIUM pMedium);
    STDMETHOD(QueryGetData)(LPFORMATETC pFormatEtc);
    STDMETHOD(GetCanonicalFormatEtc)(LPFORMATETC pFormatEct,  LPFORMATETC pFormatEtcOut);
    STDMETHOD(SetData)(LPFORMATETC pFormatEtc, LPSTGMEDIUM pMedium, BOOL fRelease);
    STDMETHOD(EnumFormatEtc)(DWORD dwDirection, IEnumFORMATETC **ppEnumFormatEtc);
    STDMETHOD(DAdvise)(LPFORMATETC pFormatEtc, DWORD advf, IAdviseSink *pAdvSink, DWORD *pdwConnection);
    STDMETHOD(DUnadvise)(DWORD dwConnection);
    STDMETHOD(EnumDAdvise)(IEnumSTATDATA **ppEnumAdvise);

#ifdef VBOX_WITH_SHARED_CLIPBOARD_WIN_ASYNC
public: /* IDataObjectAsyncCapability methods. */

    STDMETHOD(EndOperation)(HRESULT hResult, IBindCtx* pbcReserved, DWORD dwEffects);
    STDMETHOD(GetAsyncMode)(BOOL* pfIsOpAsync);
    STDMETHOD(InOperation)(BOOL* pfInAsyncOp);
    STDMETHOD(SetAsyncMode)(BOOL fDoOpAsync);
    STDMETHOD(StartOperation)(IBindCtx* pbcReserved);
#endif /* VBOX_WITH_SHARED_CLIPBOARD_WIN_ASYNC */

public:

    int SetTransfer(PSHCLTRANSFER pTransfer);
    int SetStatus(Status enmStatus, int rcSts = VINF_SUCCESS);

public:

    static DECLCALLBACK(int) readThread(PSHCLTRANSFER pTransfer, void *pvUser);

    static void logFormat(CLIPFORMAT fmt);

protected:

    void uninitInternal(void);

    static int Thread(RTTHREAD hThread, void *pvUser);

    inline int lock(void);
    inline int unlock(void);

    int readDir(PSHCLTRANSFER pTransfer, const Utf8Str &strPath);

    int copyToHGlobal(const void *pvData, size_t cbData, UINT fFlags, HGLOBAL *phGlobal);
    int createFileGroupDescriptorFromTransfer(PSHCLTRANSFER pTransfer,
                                              bool fUnicode, HGLOBAL *phGlobal);

    bool lookupFormatEtc(LPFORMATETC pFormatEtc, ULONG *puIndex);
    void registerFormat(LPFORMATETC pFormatEtc, CLIPFORMAT clipFormat, TYMED tyMed = TYMED_HGLOBAL,
                        LONG lindex = -1, DWORD dwAspect = DVASPECT_CONTENT, DVTARGETDEVICE *pTargetDevice = NULL);
    int setTransferLocked(PSHCLTRANSFER pTransfer);
    int setStatusLocked(Status enmStatus, int rc = VINF_SUCCESS);

protected:

    /**
     * Structure for keeping a single file system object entry.
     */
    struct FSOBJENTRY
    {
        /** Relative path of the object. */
        char         *pszPath;
        /** Related (cached) object information. */
        SHCLFSOBJINFO objInfo;
    };

    /** Vector containing file system objects with its (cached) objection information. */
    typedef std::vector<FSOBJENTRY> FsObjEntryList;

    /** Shared Clipboard context to use. */
    PSHCLCONTEXT                m_pCtx;
    /** The object's current status. */
    Status                      m_enmStatus;
    /** Last (IPRT-style) error set in conjunction with the status. */
    int                         m_rcStatus;
    /** Data object callback table to use. */
    CALLBACKS                   m_Callbacks;
    /** Data object callback table context to use. */
    CALLBACKCTX                 m_CallbackCtx;
    /** The object's current reference count. */
    ULONG                       m_lRefCount;
    /** How many formats have been registered. */
    ULONG                       m_cFormats;
    LPFORMATETC                 m_pFormatEtc;
    LPSTGMEDIUM                 m_pStgMedium;
    /** Pointer to the associated transfer object being handled. */
    PSHCLTRANSFER               m_pTransfer;
    /** Current stream object being used. */
    IStream                    *m_pStream;
    /** Current object index being handled by the data object.
     *  This is needed to create the next IStream object for e.g. the next upcoming file/dir/++ in the transfer. */
    ULONG                       m_uObjIdx;
    /** List of (cached) file system objects. */
    FsObjEntryList              m_lstEntries;
    /** Critical section to serialize access. */
    RTCRITSECT                  m_CritSect;
    /** Event being triggered when reading the transfer list been completed. */
    RTSEMEVENT                  m_EventListComplete;
    /** Event being triggered when the object status has been changed. */
    RTSEMEVENT                  m_EventStatusChanged;
    /** Registered format for CFSTR_FILEDESCRIPTORA. */
    UINT                        m_cfFileDescriptorA;
    /** Registered format for CFSTR_FILEDESCRIPTORW. */
    UINT                        m_cfFileDescriptorW;
    /** Registered format for CFSTR_FILECONTENTS. */
    UINT                        m_cfFileContents;
    /** Registered format for CFSTR_PERFORMEDDROPEFFECT. */
    UINT                        m_cfPerformedDropEffect;
};

/**
 * Generic Windows class implementing IEnumFORMATETC for Shared Clipboard data transfers.
 */
class SharedClipboardWinEnumFormatEtc : public IEnumFORMATETC
{
public:

    SharedClipboardWinEnumFormatEtc(void);
    virtual ~SharedClipboardWinEnumFormatEtc(void);

public:

    int Init(LPFORMATETC pFormatEtc, ULONG cFormats);
    void Destroy(void);

public: /* IUnknown methods. */

    STDMETHOD(QueryInterface)(REFIID iid, void ** ppvObject);
    STDMETHOD_(ULONG, AddRef)(void);
    STDMETHOD_(ULONG, Release)(void);

public: /* IEnumFORMATETC methods. */

    STDMETHOD(Next)(ULONG cFormats, LPFORMATETC pFormatEtc, ULONG *pcFetched);
    STDMETHOD(Skip)(ULONG cFormats);
    STDMETHOD(Reset)(void);
    STDMETHOD(Clone)(IEnumFORMATETC **ppEnumFormatEtc);

public:

    static void CopyFormat(LPFORMATETC pFormatDest, LPFORMATETC pFormatSource);
    static HRESULT CreateEnumFormatEtc(UINT cFormats, LPFORMATETC pFormatEtc, IEnumFORMATETC **ppEnumFormatEtc);

private:

    LONG        m_lRefCount;
    ULONG       m_nIndex;
    ULONG       m_nNumFormats;
    LPFORMATETC m_pFormatEtc;
};

/**
 * Generic Windows class implementing IStream for Shared Clipboard data transfers.
 */
class SharedClipboardWinStreamImpl : public IStream
{
public:

    SharedClipboardWinStreamImpl(SharedClipboardWinDataObject *pParent, PSHCLTRANSFER pTransfer,
                                 const Utf8Str &strPath, PSHCLFSOBJINFO pObjInfo);
    virtual ~SharedClipboardWinStreamImpl(void);

public: /* IUnknown methods. */

    STDMETHOD(QueryInterface)(REFIID iid, void ** ppvObject);
    STDMETHOD_(ULONG, AddRef)(void);
    STDMETHOD_(ULONG, Release)(void);

public: /* IStream methods. */

    STDMETHOD(Clone)(IStream** ppStream);
    STDMETHOD(Commit)(DWORD dwFrags);
    STDMETHOD(CopyTo)(IStream* pDestStream, ULARGE_INTEGER nBytesToCopy, ULARGE_INTEGER* nBytesRead, ULARGE_INTEGER* nBytesWritten);
    STDMETHOD(LockRegion)(ULARGE_INTEGER nStart, ULARGE_INTEGER nBytes,DWORD dwFlags);
    STDMETHOD(Read)(void* pvBuffer, ULONG nBytesToRead, ULONG* nBytesRead);
    STDMETHOD(Revert)(void);
    STDMETHOD(Seek)(LARGE_INTEGER nMove, DWORD dwOrigin, ULARGE_INTEGER* nNewPos);
    STDMETHOD(SetSize)(ULARGE_INTEGER nNewSize);
    STDMETHOD(Stat)(STATSTG* statstg, DWORD dwFlags);
    STDMETHOD(UnlockRegion)(ULARGE_INTEGER nStart, ULARGE_INTEGER nBytes, DWORD dwFlags);
    STDMETHOD(Write)(const void* pvBuffer, ULONG nBytesToRead, ULONG* nBytesRead);

public: /* Own methods. */

    static HRESULT Create(SharedClipboardWinDataObject *pParent, PSHCLTRANSFER pTransfer, const Utf8Str &strPath,
                          PSHCLFSOBJINFO pObjInfo, IStream **ppStream);
private:

    /** Pointer to the parent data object. */
    SharedClipboardWinDataObject  *m_pParent;
    /** The stream object's current reference count. */
    LONG                           m_lRefCount;
    /** Pointer to the associated Shared Clipboard transfer. */
    PSHCLTRANSFER                  m_pTransfer;
    /** The object handle to use. */
    SHCLOBJHANDLE                  m_hObj;
    /** Object path. */
    Utf8Str                        m_strPath;
    /** (Cached) object information. */
    SHCLFSOBJINFO                  m_objInfo;
    /** Number of bytes already processed. */
    uint64_t                       m_cbProcessed;
    /** Whether this object already is in completed state or not. */
    bool                           m_fIsComplete;
};

/**
 * Class for Windows-specifics for maintaining a single Shared Clipboard transfer.
 * Set as pvUser / cbUser for SHCLTRANSFER on Windows hosts / guests.
 */
class SharedClipboardWinTransferCtx
{
public:
    SharedClipboardWinTransferCtx()
        : pDataObj(NULL) { }

    virtual ~SharedClipboardWinTransferCtx() { }

    /** Pointer to data object to use for this transfer. Not owned.
     *  Can be NULL if not being used. */
    SharedClipboardWinDataObject *pDataObj;
};

int SharedClipboardWinTransferGetRoots(PSHCLWINCTX pWinCtx, PSHCLTRANSFER pTransfer);
int SharedClipboardWinTransferDropFilesToStringList(DROPFILES *pDropFiles, char **papszList, uint32_t *pcbList);
int SharedClipboardWinTransferGetRootsFromClipboard(PSHCLWINCTX pWinCtx, PSHCLTRANSFER pTransfer);

int SharedClipboardWinTransferCreate(PSHCLWINCTX pWinCtx, PSHCLTRANSFER pTransfer);
void SharedClipboardWinTransferDestroy(PSHCLWINCTX pWinCtx, PSHCLTRANSFER pTransfer);

int SharedClipboardWinTransferHandOffToDataObject(PSHCLWINCTX pWinCtx, PSHCLTRANSFER pTransfer);
int SharedClipboardWinTransferCreateAndSetDataObject(PSHCLWINCTX pWinCtx, PSHCLCONTEXT pCtx, SharedClipboardWinDataObject::PCALLBACKS pCallbacks);
# endif /* VBOX_WITH_SHARED_CLIPBOARD_TRANSFERS */
#endif /* !VBOX_INCLUDED_GuestHost_SharedClipboard_win_h */

