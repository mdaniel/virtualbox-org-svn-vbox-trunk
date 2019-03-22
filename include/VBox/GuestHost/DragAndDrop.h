/* $Id$ */
/** @file
 * DnD - Shared functions between host and guest.
 */

/*
 * Copyright (C) 2014-2018 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL) only, as it comes in the "COPYING.CDDL" file of the
 * VirtualBox OSE distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 */

#ifndef ___VBox_GuestHost_DragAndDrop_h
#define ___VBox_GuestHost_DragAndDrop_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <iprt/assert.h>
#include <iprt/fs.h>

#include <iprt/cpp/list.h>
#include <iprt/cpp/ministring.h>

/** DnDURIDroppedFiles flags. */
typedef uint32_t DNDURIDROPPEDFILEFLAGS;

/** No flags specified. */
#define DNDURIDROPPEDFILE_FLAGS_NONE                   0

/**
 * Class for maintaining a "dropped files" directory
 * on the host or guest. This will contain all received files & directories
 * for a single drag and drop operation.
 *
 * In case of a failed drag and drop operation this class can also
 * perform a gentle rollback if required.
 */
class DnDDroppedFiles
{

public:

    DnDDroppedFiles(void);
    DnDDroppedFiles(const char *pszPath, DNDURIDROPPEDFILEFLAGS fFlags = DNDURIDROPPEDFILE_FLAGS_NONE);
    virtual ~DnDDroppedFiles(void);

public:

    int AddFile(const char *pszFile);
    int AddDir(const char *pszDir);
    int Close(void);
    bool IsOpen(void) const;
    int OpenEx(const char *pszPath, DNDURIDROPPEDFILEFLAGS fFlags = DNDURIDROPPEDFILE_FLAGS_NONE);
    int OpenTemp(DNDURIDROPPEDFILEFLAGS fFlags = DNDURIDROPPEDFILE_FLAGS_NONE);
    const char *GetDirAbs(void) const;
    int Reopen(void);
    int Reset(bool fDeleteContent);
    int Rollback(void);

protected:

    int closeInternal(void);

protected:

    /** Open flags. */
    uint32_t                     m_fOpen;
    /** Directory handle for drop directory. */
    RTDIR                        m_hDir;
    /** Absolute path to drop directory. */
    RTCString                    m_strPathAbs;
    /** List for holding created directories in the case of a rollback. */
    RTCList<RTCString>           m_lstDirs;
    /** List for holding created files in the case of a rollback. */
    RTCList<RTCString>           m_lstFiles;
};

bool DnDMIMEHasFileURLs(const char *pcszFormat, size_t cchFormatMax);
bool DnDMIMENeedsDropDir(const char *pcszFormat, size_t cchFormatMax);

int DnDPathSanitizeFilename(char *pszPath, size_t cbPath);
int DnDPathSanitize(char *pszPath, size_t cbPath);

/** DnDURIObject flags. */
typedef uint32_t DNDURIOBJECTFLAGS;

/** No flags specified. */
#define DNDURIOBJECT_FLAGS_NONE                   0

/** Mask of all valid DnD URI object flags. */
#define DNDURIOBJECT_FLAGS_VALID_MASK             UINT32_C(0x0)

/**
 * Class for handling DnD URI objects.
 * This class abstracts the access and handling objects when performing DnD actions.
 */
class DnDURIObject
{
public:

    /**
     * Enumeration for specifying an URI object type.
     */
    enum Type
    {
        /** Unknown type, do not use. */
        Type_Unknown = 0,
        /** Object is a file. */
        Type_File,
        /** Object is a directory. */
        Type_Directory,
        /** The usual 32-bit hack. */
        Type_32Bit_Hack = 0x7fffffff
    };

    /**
     * Enumeration for specifying an URI object view
     * for representing its data accordingly.
     */
    enum View
    {
        /** Unknown view, do not use. */
        View_Unknown = 0,
        /** Handle data from the source point of view. */
        View_Source,
        /** Handle data from the destination point of view. */
        View_Target,
        /** The usual 32-bit hack. */
        View_Dest_32Bit_Hack = 0x7fffffff
    };

    DnDURIObject(void);
    DnDURIObject(Type type,
                 const RTCString &strSrcPathAbs = "",
                 const RTCString &strDstPathAbs = "");
    virtual ~DnDURIObject(void);

public:

    /**
     * Returns the given absolute source path of the object.
     *
     * @return  Absolute source path of the object.
     */
    const RTCString &GetSourcePathAbs(void) const { return m_strSrcPathAbs; }

    /**
     * Returns the given, absolute destination path of the object.
     *
     * @return  Absolute destination path of the object.
     */
    const RTCString &GetDestPathAbs(void) const { return m_strTgtPathAbs; }

    RTFMODE GetMode(void) const;

    uint64_t GetProcessed(void) const;

    uint64_t GetSize(void) const;

    /**
     * Returns the object's type.
     *
     * @return  The object's type.
     */
    Type GetType(void) const { return m_enmType; }

    /**
     * Returns the object's view.
     *
     * @return  The object's view.
     */
    View GetView(void) const { return m_enmView; }

public:

    int SetSize(uint64_t cbSize);

public:

    void Close(void);
    bool IsComplete(void) const;
    bool IsOpen(void) const;
    int Open(View enmView, uint64_t fOpen, RTFMODE fMode = 0);
    int OpenEx(const RTCString &strPath, View enmView, uint64_t fOpen = 0, RTFMODE fMode = 0, DNDURIOBJECTFLAGS = DNDURIOBJECT_FLAGS_NONE);
    int QueryInfo(View enmView);
    int Read(void *pvBuf, size_t cbBuf, uint32_t *pcbRead);
    void Reset(void);
    int Write(const void *pvBuf, size_t cbBuf, uint32_t *pcbWritten);

public:

    static int RebaseURIPath(RTCString &strPath, const RTCString &strBaseOld = "", const RTCString &strBaseNew = "");

protected:

    void closeInternal(void);
    int queryInfoInternal(View enmView);

protected:

    /** The object's type. */
    Type      m_enmType;
    /** The object's view. */
    View      m_enmView;
    /** Absolute path (base) for the source. */
    RTCString m_strSrcPathAbs;
    /** Absolute path (base) for the target. */
    RTCString m_strTgtPathAbs;
    /** Whether the object is in "opened" state. */
    bool      m_fIsOpen;

    /** Union containing data depending on the object's type. */
    union
    {
        /** Structure containing members for objects that
         *  are files. */
        struct
        {
            /** File handle. */
            RTFILE      hFile;
            /** File system object information of this file. */
            RTFSOBJINFO objInfo;
            /** Bytes to proces for reading/writing. */
            uint64_t    cbToProcess;
            /** Bytes processed reading/writing. */
            uint64_t    cbProcessed;
        } File;
        struct
        {
            /** Directory handle. */
            RTDIR       hDir;
            /** File system object information of this directory. */
            RTFSOBJINFO objInfo;
        } Dir;
    } u;
};

/** DnDURIList flags. */
typedef uint32_t DNDURILISTFLAGS;

/** No flags specified. */
#define DNDURILIST_FLAGS_NONE                   0
/** Keep the original paths, don't convert paths to relative ones. */
#define DNDURILIST_FLAGS_ABSOLUTE_PATHS         RT_BIT(0)
/** Resolve all symlinks. */
#define DNDURILIST_FLAGS_RESOLVE_SYMLINKS       RT_BIT(1)
/** Keep the files + directory entries open while
 *  being in this list. */
#define DNDURILIST_FLAGS_KEEP_OPEN              RT_BIT(2)
/** Lazy loading: Only enumerate sub directories when needed.
 ** @todo Implement lazy loading.  */
#define DNDURILIST_FLAGS_LAZY                   RT_BIT(3)

/** Mask of all valid DnD URI list flags. */
#define DNDURILIST_FLAGS_VALID_MASK             UINT32_C(0xF)

class DnDURIList
{
public:

    DnDURIList(void);
    virtual ~DnDURIList(void);

public:

    int AppendNativePath(const char *pszPath, DNDURILISTFLAGS fFlags);
    int AppendNativePathsFromList(const char *pszNativePaths, size_t cbNativePaths, DNDURILISTFLAGS fFlags);
    int AppendNativePathsFromList(const RTCList<RTCString> &lstNativePaths, DNDURILISTFLAGS fFlags);
    int AppendURIPath(const char *pszURI, DNDURILISTFLAGS fFlags);
    int AppendURIPathsFromList(const char *pszURIPaths, size_t cbURIPaths, DNDURILISTFLAGS fFlags);
    int AppendURIPathsFromList(const RTCList<RTCString> &lstURI, DNDURILISTFLAGS fFlags);

    void Clear(void);
    DnDURIObject *First(void) { return m_lstTree.first(); }
    bool IsEmpty(void) const { return m_lstTree.isEmpty(); }
    void RemoveFirst(void);
    int SetFromURIData(const void *pvData, size_t cbData, DNDURILISTFLAGS fFlags);

    RTCString GetRootEntries(const RTCString &strPathBase = "", const RTCString &strSeparator = "\r\n") const;
    uint64_t GetRootCount(void) const { return m_lstRoot.size(); }
    uint64_t GetTotalCount(void) const { return m_cTotal; }
    uint64_t GetTotalBytes(void) const { return m_cbTotal; }

protected:

    int addEntry(const char *pcszSource, const char *pcszTarget, DNDURILISTFLAGS fFlags);
    int appendPathRecursive(const char *pcszSrcPath, const char *pcszDstPath, const char *pcszDstBase, size_t cchDstBase, DNDURILISTFLAGS fFlags);

protected:

    /** List of all top-level file/directory entries.
     *  Note: All paths are kept internally as UNIX paths for
     *        easier conversion/handling!  */
    RTCList<RTCString>      m_lstRoot;
    /** List of all URI objects added. The list's content
     *  might vary depending on how the objects are being
     *  added (lazy or not). */
    RTCList<DnDURIObject *> m_lstTree;
    /** Total number of all URI objects. */
    uint64_t                m_cTotal;
    /** Total size of all URI objects, that is, the file
     *  size of all objects (in bytes).
     *  Note: Do *not* size_t here, as we also want to support large files
     *        on 32-bit guests. */
    uint64_t                m_cbTotal;
};

#endif /* !___VBox_GuestHost_DragAndDrop_h */

