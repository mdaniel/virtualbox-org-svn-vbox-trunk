/* $Id$ */
/** @file
 * IPRT - ISO Image Maker.
 */

/*
 * Copyright (C) 2017 Oracle Corporation
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


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#define LOG_GROUP RTLOGGROUP_FS
#include "internal/iprt.h"
#include <iprt/fsisomaker.h>

#include <iprt/asm.h>
#include <iprt/assert.h>
#include <iprt/buildconfig.h>
#include <iprt/err.h>
#include <iprt/ctype.h>
#include <iprt/file.h>
#include <iprt/list.h>
#include <iprt/log.h>
#include <iprt/mem.h>
#include <iprt/path.h>
#include <iprt/string.h>
#include <iprt/vfs.h>
#include <iprt/vfslowlevel.h>
#include <iprt/formats/iso9660.h>

#include <internal/magics.h>


/*********************************************************************************************************************************
*   Defined Constants And Macros                                                                                                 *
*********************************************************************************************************************************/
/** Asserts valid handle, returns @a a_rcRet if not. */
#define RTFSISOMAKER_ASSERT_VALID_HANDLE_RET_EX(a_pThis, a_rcRet) \
    do { AssertPtrReturn(a_pThis, a_rcRet); \
         AssertReturn((a_pThis)->uMagic == RTFSISOMAKERINT_MAGIC, a_rcRet); \
    } while (0)

/** Asserts valid handle, returns VERR_INVALID_HANDLE if not. */
#define RTFSISOMAKER_ASSERT_VALID_HANDLE_RET(a_pThis) RTFSISOMAKER_ASSERT_VALID_HANDLE_RET_EX(a_pThis, VERR_INVALID_HANDLE)

/** The sector size. */
#define RTFSISOMAKER_SECTOR_SIZE            _2K
/** The sector offset mask. */
#define RTFSISOMAKER_SECTOR_OFFSET_MASK     (_2K - 1)
/** Maximum number of objects. */
#define RTFSISOMAKER_MAX_OBJECTS            _16M
/** Maximum number of objects per directory. */
#define RTFSISOMAKER_MAX_OBJECTS_PER_DIR    _256K /**< @todo check limit */

/** UTF-8 name buffer.  */
#define RTFSISOMAKER_MAX_NAME_BUF           768

/** TRANS.TBL left padding length.
 * We keep the amount of padding low to avoid wasing memory when generating
 * these long obsolete files. */
#define RTFSISOMAKER_TRANS_TBL_LEFT_PAD     12

/** Tests if @a a_ch is in the set of d-characters. */
#define RTFSISOMAKER_IS_IN_D_CHARS(a_ch)        (RT_C_IS_UPPER(a_ch) || RT_C_IS_DIGIT(a_ch) || (a_ch) == '_')

/** Tests if @a a_ch is in the set of d-characters when uppercased. */
#define RTFSISOMAKER_IS_UPPER_IN_D_CHARS(a_ch)  (RT_C_IS_ALNUM(a_ch) || (a_ch) == '_')


/** Calculates the path table record size given the name length.
 * @note  The root directory length is 1 (name byte is 0x00), we make sure this
 *        is the case in rtFsIsoMakerNormalizeNameForNamespace. */
#define RTFSISOMAKER_CALC_PATHREC_SIZE(a_cbNameInDirRec) \
    ( RT_UOFFSETOF(ISO9660PATHREC, achDirId[(a_cbNameInDirRec) + ((a_cbNameInDirRec) & 1)]) )



/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/
/** Pointer to an ISO maker object name space node. */
typedef struct RTFSISOMAKERNAME *PRTFSISOMAKERNAME;
/** Pointer to a const ISO maker object name space node. */
typedef struct RTFSISOMAKERNAME const *PCRTFSISOMAKERNAME;
/** Pointer to an ISO maker object name space node pointer. */
typedef PRTFSISOMAKERNAME *PPRTFSISOMAKERNAME;

/** Pointer to a common ISO image maker file system object. */
typedef struct RTFSISOMAKEROBJ *PRTFSISOMAKEROBJ;
/** Pointer to a const common ISO image maker file system object. */
typedef struct RTFSISOMAKEROBJ const *PCRTFSISOMAKEROBJ;

/** Pointer to a ISO maker file object. */
typedef struct RTFSISOMAKERFILE *PRTFSISOMAKERFILE;
/** Pointer to a const ISO maker file object. */
typedef struct RTFSISOMAKERFILE const *PCRTFSISOMAKERFILE;

/**
 * Filesystem object type.
 */
typedef enum RTFSISOMAKEROBJTYPE
{
    RTFSISOMAKEROBJTYPE_INVALID = 0,
    RTFSISOMAKEROBJTYPE_DIR,
    RTFSISOMAKEROBJTYPE_FILE,
    //RTFSISOMAKEROBJTYPE_SYMLINK,
    RTFSISOMAKEROBJTYPE_END
} RTFSISOMAKEROBJTYPE;

/**
 * Extra name space information required for directories.
 */
typedef struct RTFSISOMAKERNAMEDIR
{
    /** The location of the directory data. */
    uint64_t                offDir;
    /** The size of the directory. */
    uint32_t                cbDir;
    /** Number of children. */
    uint32_t                cChildren;
    /** Sorted array of children. */
    PPRTFSISOMAKERNAME      papChildren;
    /** The translate table file. */
    PRTFSISOMAKERFILE       pTransTblFile;

    /** The offset in the path table (ISO-9660).
     * This is set when finalizing the image.  */
    uint32_t                offPathTable;
    /** The path table identifier of this directory (ISO-9660).
    * This is set when finalizing the image.  */
    uint16_t                idPathTable;
    /** The size of the first directory record (0x00 - '.'). */
    uint8_t                 cbDirRec00;
    /** The size of the second directory record (0x01 - '..'). */
    uint8_t                 cbDirRec01;
    /** Pointer to back to the namespace node this belongs to (for the finalized
     *  entry list). */
    PRTFSISOMAKERNAME       pName;
    /** Entry in the list of finalized directories. */
    RTLISTNODE              FinalizedEntry;
} RTFSISOMAKERNAMEDIR;
/** Pointer to directory specfic namespace node info. */
typedef RTFSISOMAKERNAMEDIR *PRTFSISOMAKERNAMEDIR;
/** Pointer to const directory specfic namespace node info. */
typedef const RTFSISOMAKERNAMEDIR *PCRTFSISOMAKERNAMEDIR;


/**
 * ISO maker object namespace node.
 */
typedef struct RTFSISOMAKERNAME
{
    /** Pointer to the file system object. */
    PRTFSISOMAKEROBJ        pObj;
    /** Pointer to the partent directory, NULL if root dir. */
    PRTFSISOMAKERNAME       pParent;

    /** Pointer to the directory information if this is a directory, NULL if not a
     * directory. This is allocated together with this structure, so it doesn't need
     * freeing. */
    PRTFSISOMAKERNAMEDIR    pDir;

    /** The name specified when creating this namespace node.  Helps navigating
     * the namespace when we mangle or otherwise change the names.
     * Allocated together with of this structure, no spearate free necessary. */
    const char             *pszSpecNm;

    /** Alternative rock ridge name. */
    char                   *pszRockRidgeNm;
    /** Alternative TRANS.TBL name. */
    char                   *pszTransNm;
    /** Length of pszSpecNm. */
    uint16_t                cchSpecNm;
    /** Length of pszRockRidgeNm. */
    uint16_t                cchRockRidgeNm;
    /** Length of pszTransNm. */
    uint16_t                cchTransNm;

    /** The depth in the namespace tree of this name. */
    uint8_t                 uDepth;
    /** Set if pszTransNm is allocated separately.  Normally same as pszSpecNm. */
    bool                    fRockRidgeNmAlloced : 1;
    /** Set if pszTransNm is allocated separately.  Normally same as pszSpecNm. */
    bool                    fTransNmAlloced : 1;

    /** The mode mask.
     * Starts out as a copy of RTFSISOMAKEROBJ::fMode. */
    RTFMODE                 fMode;
    /** The owner ID.
     * Starts out as a copy of RTFSISOMAKEROBJ::uid. */
    RTUID                   uid;
    /** The group ID.
     * Starts out as a copy of RTFSISOMAKEROBJ::gid. */
    RTGID                   gid;
    /** The device number if a character or block device.
     * This is for Rock Ridge.  */
    RTDEV                   Device;
    /** The inode/serial number.
     * This is for Rock Ridge.  */
    uint64_t                INode;
    /** The number of hardlinks to report in the file stats.
     * This is for Rock Ridge.  */
    uint32_t                cHardlinks;

    /** The offset of the directory entry in the parent directory. */
    uint32_t                offDirRec;
    /** Size of the directory record (ISO-9660).
     * This is set when the image is being finalized. */
    uint16_t                cbDirRec;
    /** Same as cbDirRec but with end of sector zero padding added. */
    uint16_t                cbDirRecWithZeroPad;

    /** The number of bytes the name requires in the directory record. */
    uint16_t                cbNameInDirRec;
    /** The name length. */
    uint16_t                cchName;
    /** The name. */
    char                    szName[RT_FLEXIBLE_ARRAY];
} RTFSISOMAKERNAME;

/**
 * A ISO maker namespace.
 */
typedef struct RTFSISOMAKERNAMESPACE
{
    /** The namespace root. */
    PRTFSISOMAKERNAME       pRoot;
    /** Total number of name nodes in the namespace. */
    uint32_t                cNames;
    /** Total number of directories in the namespace. */
    uint32_t                cDirs;
    /** The namespace selector (RTFSISOMAKER_NAMESPACE_XXX). */
    uint32_t                fNamespace;
    /** Offset into RTFSISOMAKERNAMESPACE of the name member. */
    uint32_t                offName;
    /** The configuration level for this name space.
     *     - For UDF and HFS namespaces this is either @c true or @c false.
     *     - For the primary ISO-9660 namespace this is 1, 2, or 3.
     *     - For the joliet namespace this 0 (joliet disabled), 1, 2, or 3. */
    uint8_t                 uLevel;
    /** The rock ridge level: 1 - enabled; 2 - with ER tag.
     * Linux behaves a little different when seeing the ER tag. */
    uint8_t                 uRockRidgeLevel;
    /** The TRANS.TBL filename if enabled, NULL if disabled.
     * When not NULL, this may be pointing to heap or g_szTransTbl. */
    char                   *pszTransTbl;
    /** The system ID (ISO9660PRIMARYVOLDESC::achSystemId). Empty if NULL.
     * When not NULL, this may be pointing to heap of g_szSystemId. */
    char                   *pszSystemId;
    /** The volume ID / label (ISO9660PRIMARYVOLDESC::achVolumeId).
     * A string representation of RTFSISOMAKERINT::ImageCreationTime if NULL. */
    char                   *pszVolumeId;
    /** The volume set ID (ISO9660PRIMARYVOLDESC::achVolumeSetId). Empty if NULL. */
    char                   *pszVolumeSetId;
    /** The publisher ID or (root) file reference (ISO9660PRIMARYVOLDESC::achPublisherId).  Empty if NULL.  */
    char                   *pszPublisherId;
    /** The data preperer ID or (root) file reference (ISO9660PRIMARYVOLDESC::achDataPreparerId).
     * Defaults to g_szPreparerIdPrimaryIso or g_szPreparerIdJoliet. */
    char                   *pszDataPreparerId;
    /** The application ID or (root) file reference (ISO9660PRIMARYVOLDESC::achApplicationId). None if NULL. */
    char                   *pszApplicationId;
    /** The copyright (root) file identifier (ISO9660PRIMARYVOLDESC::achCopyrightFileId).  None if NULL. */
    char                   *pszCopyrightFileId;
    /** The abstract (root) file identifier (ISO9660PRIMARYVOLDESC::achAbstractFileId). None if NULL. */
    char                   *pszAbstractFileId;
    /** The bibliographic (root) file identifier (ISO9660PRIMARYVOLDESC::achBibliographicFileId).  None if NULL. */
    char                   *pszBibliographicFileId;
} RTFSISOMAKERNAMESPACE;
/** Pointer to a namespace. */
typedef RTFSISOMAKERNAMESPACE *PRTFSISOMAKERNAMESPACE;
/** Pointer to a const namespace. */
typedef RTFSISOMAKERNAMESPACE const *PCRTFSISOMAKERNAMESPACE;


/**
 * Common base structure for the file system objects.
 *
 * The times are shared across all namespaces, while the uid, gid and mode are
 * duplicates in each namespace.
 */
typedef struct RTFSISOMAKEROBJ
{
    /** The linear list entry of the image content. */
    RTLISTNODE              Entry;
    /** The object index. */
    uint32_t                idxObj;
    /** The type of this object. */
    RTFSISOMAKEROBJTYPE     enmType;

    /** The primary ISO-9660 name space name. */
    PRTFSISOMAKERNAME       pPrimaryName;
    /** The joliet name space name. */
    PRTFSISOMAKERNAME       pJolietName;
    /** The UDF name space name. */
    PRTFSISOMAKERNAME       pUdfName;
    /** The HFS name space name. */
    PRTFSISOMAKERNAME       pHfsName;

    /** Birth (creation) time. */
    RTTIMESPEC              BirthTime;
    /** Attribute change time. */
    RTTIMESPEC              ChangeTime;
    /** Modification time. */
    RTTIMESPEC              ModificationTime;
    /** Accessed time. */
    RTTIMESPEC              AccessedTime;

    /** Owner ID. */
    RTUID                   uid;
    /** Group ID. */
    RTGID                   gid;
    /** Attributes (unix permissions bits mainly). */
    RTFMODE                 fMode;

    /** Used to make sure things like the boot catalog stays in the image even if
     * it's not mapped into any of the namespaces. */
    uint32_t                cNotOrphan;
} RTFSISOMAKEROBJ;


/**
 * File source type.
 */
typedef enum RTFSISOMAKERSRCTYPE
{
    RTFSISOMAKERSRCTYPE_INVALID = 0,
    RTFSISOMAKERSRCTYPE_PATH,
    RTFSISOMAKERSRCTYPE_VFS_FILE,
    RTFSISOMAKERSRCTYPE_COMMON,
    RTFSISOMAKERSRCTYPE_TRANS_TBL,
    RTFSISOMAKERSRCTYPE_END
} RTFSISOMAKERSRCTYPE;

/**
 * ISO maker file object.
 */
typedef struct RTFSISOMAKERFILE
{
    /** The common bit. */
    RTFSISOMAKEROBJ         Core;
    /** The file data size. */
    uint64_t                cbData;
    /** Byte offset of the data in the image.
     * UINT64_MAX until the location is finalized. */
    uint64_t                offData;

    /** The type of source object. */
    RTFSISOMAKERSRCTYPE     enmSrcType;
    /** The source data. */
    union
    {
        /** Path to the source file.
         * Allocated together with this structure.  */
        const char         *pszSrcPath;
        /** Source VFS file. */
        RTVFSFILE           hVfsFile;
        /** Source is a part of a common VFS file. */
        struct
        {
            /** The offset into the file */
            uint64_t        offData;
            /** The index of the common file. */
            uint32_t        idxSrc;
        } Common;
        /** The directory the translation table belongs to. */
        PRTFSISOMAKERNAME   pTransTblDir;
    } u;

    /** Boot info table to patch into the file.
     * This is calculated during file finalization as it needs the file location. */
    PISO9660SYSLINUXINFOTABLE pBootInfoTable;

    /** Entry in the list of finalized directories. */
    RTLISTNODE              FinalizedEntry;
} RTFSISOMAKERFILE;


/**
 * ISO maker directory object.
 *
 * Unlike files, the allocation info is name space specific and lives in the
 * corresponding RTFSISOMAKERNAMEDIR structures.
 */
typedef struct RTFSISOMAKERDIR
{
    /** The common bit. */
    RTFSISOMAKEROBJ         Core;
} RTFSISOMAKERDIR;
/** Pointer to an ISO maker directory object.  */
typedef RTFSISOMAKERDIR *PRTFSISOMAKERDIR;



/**
 * Instance data for a ISO image maker.
 */
typedef struct RTFSISOMAKERINT
{
    /** Magic value (RTFSISOMAKERINT_MAGIC). */
    uint32_t                uMagic;
    /** Reference counter. */
    uint32_t volatile       cRefs;

    /** Set after we've been fed the first bit of content.
     * This means that the namespace configuration has been finalized and can no
     * longer be changed because it's simply too much work to do adjustments
     * after having started to add files. */
    bool                    fSeenContent;
    /** Set once we've finalized the image structures.
     * After this no more changes are allowed.  */
    bool                    fFinalized;

    /** The primary ISO-9660 namespace. */
    RTFSISOMAKERNAMESPACE   PrimaryIso;
    /** The joliet namespace. */
    RTFSISOMAKERNAMESPACE   Joliet;
    /** The UDF namespace. */
    RTFSISOMAKERNAMESPACE   Udf;
    /** The hybrid HFS+ namespace. */
    RTFSISOMAKERNAMESPACE   Hfs;

    /** The list of objects (RTFSISOMAKEROBJ). */
    RTLISTANCHOR            ObjectHead;
    /** Number of objects in the image (ObjectHead).
     * This is used to number them, i.e. create RTFSISOMAKEROBJ::idxObj.  */
    uint32_t                cObjects;

    /** Amount of file data. */
    uint64_t                cbData;
    /** Number of volume descriptors. */
    uint32_t                cVolumeDescriptors;

    /** The 'now' timestamp we use for the whole image.
     * This way we'll save lots of RTTimeNow calls and have similar timestamps
     * over the whole image. */
    RTTIMESPEC              ImageCreationTime;
    /** The default owner ID. */
    RTUID                   uidDefault;
    /** The default group ID. */
    RTGID                   gidDefault;
    /** The default file mode mask. */
    RTFMODE                 fDefaultFileMode;
    /** The default file mode mask. */
    RTFMODE                 fDefaultDirMode;

    /** Number of common source files. */
    uint32_t                cCommonSources;
    /** Array of common source file handles. */
    PRTVFSFILE              paCommonSources;

    /** @name Boot related stuff
     * @{ */
    /** The boot catalog file. */
    PRTFSISOMAKERFILE       pBootCatFile;
    /** Per boot catalog entry data needed for updating offsets when finalizing. */
    struct
    {
        /** The type (ISO9660_ELTORITO_HEADER_ID_VALIDATION_ENTRY,
         * ISO9660_ELTORITO_HEADER_ID_SECTION_HEADER,
         * ISO9660_ELTORITO_HEADER_ID_FINAL_SECTION_HEADER,
         * ISO9660_ELTORITO_BOOT_INDICATOR_BOOTABLE or
         * ISO9660_ELTORITO_BOOT_INDICATOR_NOT_BOOTABLE). */
        uint8_t             bType;
        /** Number of entries related to this one.  This is zero for unused entries,
         *  2 for the validation entry, 2+ for section headers, and 1 for images. */
        uint8_t             cEntries;
        /** The boot file. */
        PRTFSISOMAKERFILE   pBootFile;
    }                       aBootCatEntries[64];
    /** @} */

    /** @name Finalized image stuff
     * @{ */
    /** The finalized image size. */
    uint64_t                cbFinalizedImage;
    /** System area content (sectors 0 thur 15).  This is NULL if the system area
     * are all zeros, which is often the case.   Hybrid ISOs have an MBR followed by
     * a GUID partition table here, helping making the image bootable when
     * transfered to a USB stick. */
    uint8_t                *pbSysArea;
    /** Number of non-zero system area bytes pointed to by pbSysArea.  */
    size_t                  cbSysArea;

    /** Pointer to the buffer holding the volume descriptors. */
    uint8_t                *pbVolDescs;
    /** Pointer to the primary volume descriptor. */
    PISO9660PRIMARYVOLDESC  pPrimaryVolDesc;
    /** El Torito volume descriptor. */
    PISO9660BOOTRECORDELTORITO pElToritoDesc;
    /** Pointer to the primary volume descriptor. */
    PISO9660SUPVOLDESC      pJolietVolDesc;
    /** Terminating ISO-9660 volume descriptor. */
    PISO9660VOLDESCHDR      pTerminatorVolDesc;

    /** Finalized ISO-9660 directory structures. */
    struct RTFSISOMAKERFINALIZEDDIRS
    {
        /** The image byte offset of the first directory. */
        uint64_t            offDirs;
        /** The image byte offset of the little endian path table.
         * This always follows offDirs.  */
        uint64_t            offPathTableL;
        /** The image byte offset of the big endian path table.
         * This always follows offPathTableL.  */
        uint64_t            offPathTableM;
        /** The size of the path table.   */
        uint32_t            cbPathTable;
        /** List of finalized directories for this namespace.
         * The list is in path table order so it can be generated on the fly.  The
         * directories will be ordered in the same way. */
        RTLISTANCHOR        FinalizedDirs;
        /** Rock ridge spill file. */
        PRTFSISOMAKERFILE   pRRSpillFile;
    }
    /** The finalized directory data for the primary ISO-9660 namespace. */
                            PrimaryIsoDirs,
    /** The finalized directory data for the joliet namespace. */
                            JolietDirs;

    /** The image byte offset of the first file. */
    uint64_t                offFirstFile;
    /** Finalized file head (RTFSISOMAKERFILE).
     * The list is ordered by disk location.  Files are following the
     * directories and path tables. */
    RTLISTANCHOR            FinalizedFiles;
    /** @} */

} RTFSISOMAKERINT;
/** Pointer to an ISO maker instance. */
typedef RTFSISOMAKERINT *PRTFSISOMAKERINT;

/** Pointer to the data for finalized ISO-9660 (primary / joliet) dirs. */
typedef struct RTFSISOMAKERINT::RTFSISOMAKERFINALIZEDDIRS *PRTFSISOMAKERFINALIZEDDIRS;


/**
 * Instance data of an ISO maker output file.
 */
typedef struct RTFSISOMAKEROUTPUTFILE
{
    /** The ISO maker (owns a reference). */
    PRTFSISOMAKERINT        pIsoMaker;
    /** The current file position. */
    uint64_t                offCurPos;
    /** Current file hint. */
    PRTFSISOMAKERFILE       pFileHint;
    /** Source file corresponding to pFileHint.
     * This is used when dealing with a RTFSISOMAKERSRCTYPE_VFS_FILE or
     * RTFSISOMAKERSRCTYPE_TRANS_TBL file. */
    RTVFSFILE               hVfsSrcFile;
    /** Current directory hint for the primary ISO namespace. */
    PRTFSISOMAKERNAMEDIR    pDirHintPrimaryIso;
    /** Current directory hint for the joliet namespace. */
    PRTFSISOMAKERNAMEDIR    pDirHintJoliet;
} RTFSISOMAKEROUTPUTFILE;
/** Pointer to the instance data of an ISO maker output file. */
typedef RTFSISOMAKEROUTPUTFILE *PRTFSISOMAKEROUTPUTFILE;



/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/
/**
 * Help for iterating over namespaces.
 */
static const struct
{
    /** The RTFSISOMAKER_NAMESPACE_XXX indicator.  */
    uint32_t        fNamespace;
    /** Offset into RTFSISOMAKERINT of the namespace member. */
    uintptr_t       offNamespace;
    /** Offset into RTFSISOMAKERNAMESPACE of the name member. */
    uintptr_t       offName;
    /** Namespace name for debugging purposes. */
    const char     *pszName;
} g_aRTFsIsoNamespaces[] =
{
    {   RTFSISOMAKER_NAMESPACE_ISO_9660, RT_OFFSETOF(RTFSISOMAKERINT, PrimaryIso), RT_OFFSETOF(RTFSISOMAKEROBJ, pPrimaryName), "iso-9660" },
    {   RTFSISOMAKER_NAMESPACE_JOLIET,   RT_OFFSETOF(RTFSISOMAKERINT, Joliet),     RT_OFFSETOF(RTFSISOMAKEROBJ, pJolietName),  "joliet" },
    {   RTFSISOMAKER_NAMESPACE_UDF,      RT_OFFSETOF(RTFSISOMAKERINT, Udf),        RT_OFFSETOF(RTFSISOMAKEROBJ, pUdfName),     "udf" },
    {   RTFSISOMAKER_NAMESPACE_HFS,      RT_OFFSETOF(RTFSISOMAKERINT, Hfs),        RT_OFFSETOF(RTFSISOMAKEROBJ, pHfsName),     "hfs" },
};

/**
 * Translates a single namespace flag (RTFSISOMAKER_NAMESPACE_XXX) to an
 * index into g_aRTFsIsoNamespaces.
 */
static const uint8_t g_aidxRTFsIsoNamespaceFlagToIdx[] =
{
    /*[0]                               = */ RT_ELEMENTS(g_aRTFsIsoNamespaces),
    /*[RTFSISOMAKER_NAMESPACE_ISO_9660] = */ 0,
    /*[RTFSISOMAKER_NAMESPACE_JOLIET]   = */ 1,
    /*[3]                               = */ RT_ELEMENTS(g_aRTFsIsoNamespaces),
    /*[RTFSISOMAKER_NAMESPACE_UDF]      = */ 2,
    /*[5]                               = */ RT_ELEMENTS(g_aRTFsIsoNamespaces),
    /*[6]                               = */ RT_ELEMENTS(g_aRTFsIsoNamespaces),
    /*[7]                               = */ RT_ELEMENTS(g_aRTFsIsoNamespaces),
    /*[RTFSISOMAKER_NAMESPACE_HFS]      = */ 3,
    /*[9]                               = */ RT_ELEMENTS(g_aRTFsIsoNamespaces),
    /*[10]                              = */ RT_ELEMENTS(g_aRTFsIsoNamespaces),
    /*[11]                              = */ RT_ELEMENTS(g_aRTFsIsoNamespaces),
    /*[12]                              = */ RT_ELEMENTS(g_aRTFsIsoNamespaces),
    /*[13]                              = */ RT_ELEMENTS(g_aRTFsIsoNamespaces),
    /*[14]                              = */ RT_ELEMENTS(g_aRTFsIsoNamespaces),
    /*[15]                              = */ RT_ELEMENTS(g_aRTFsIsoNamespaces),
};

/** The default translation table filename. */
static const char   g_szTransTbl[] = "TRANS.TBL";
/** The default data preparer ID for the primary ISO-9660 volume descriptor. */
static char         g_szPreparerIdPrimaryIso[64] = "";
/** The default data preparer ID for the joliet volume descriptor. */
static char         g_szPreparerIdJoliet[64]     = "";
/** The default system ID the primary ISO-9660 volume descriptor. */
static char         g_szSystemId[64] = "";



/*********************************************************************************************************************************
*   Internal Functions                                                                                                           *
*********************************************************************************************************************************/
static int rtFsIsoMakerObjSetName(PRTFSISOMAKERINT pThis, PRTFSISOMAKERNAMESPACE pNamespace, PRTFSISOMAKEROBJ pObj,
                                  PRTFSISOMAKERNAME pParent, const char *pchSpec, size_t cchSpec, PPRTFSISOMAKERNAME ppNewName);
static int rtFsIsoMakerObjUnsetName(PRTFSISOMAKERINT pThis, PRTFSISOMAKERNAMESPACE pNamespace, PRTFSISOMAKEROBJ pObj);
static int rtFsIsoMakerAddUnnamedDirWorker(PRTFSISOMAKERINT pThis, PRTFSISOMAKERDIR *ppDir);
static int rtFsIsoMakerAddUnnamedFileWorker(PRTFSISOMAKERINT pThis, PCRTFSOBJINFO pObjInfo, size_t cbExtra,
                                            PRTFSISOMAKERFILE *ppFile);
static int rtFsIsoMakerObjRemoveWorker(PRTFSISOMAKERINT pThis, PRTFSISOMAKEROBJ pObj);

static DECLCALLBACK(int) rtFsIsoMakerOutFile_Seek(void *pvThis, RTFOFF offSeek, unsigned uMethod, PRTFOFF poffActual);



/**
 * Creates an ISO maker instance.
 *
 * @returns IPRT status code.
 * @param   phIsoMaker          Where to return the handle to the new ISO maker.
 */
RTDECL(int) RTFsIsoMakerCreate(PRTFSISOMAKER phIsoMaker)
{
    /*
     * Do some integrity checks first.
     */
    AssertReturn(g_aRTFsIsoNamespaces[g_aidxRTFsIsoNamespaceFlagToIdx[RTFSISOMAKER_NAMESPACE_ISO_9660]].fNamespace == RTFSISOMAKER_NAMESPACE_ISO_9660,
                 VERR_INTERNAL_ERROR_5);
    AssertReturn(g_aRTFsIsoNamespaces[g_aidxRTFsIsoNamespaceFlagToIdx[RTFSISOMAKER_NAMESPACE_JOLIET]].fNamespace   == RTFSISOMAKER_NAMESPACE_JOLIET,
                 VERR_INTERNAL_ERROR_5);
    AssertReturn(g_aRTFsIsoNamespaces[g_aidxRTFsIsoNamespaceFlagToIdx[RTFSISOMAKER_NAMESPACE_UDF]].fNamespace      == RTFSISOMAKER_NAMESPACE_UDF,
                 VERR_INTERNAL_ERROR_5);
    AssertReturn(g_aRTFsIsoNamespaces[g_aidxRTFsIsoNamespaceFlagToIdx[RTFSISOMAKER_NAMESPACE_HFS]].fNamespace      == RTFSISOMAKER_NAMESPACE_HFS,
                 VERR_INTERNAL_ERROR_5);

    if (g_szPreparerIdPrimaryIso[0] == '\0')
        RTStrPrintf(g_szPreparerIdPrimaryIso, sizeof(g_szPreparerIdPrimaryIso), "IPRT ISO MAKER V%u.%u.%u R%s",
                    RTBldCfgVersionMajor(), RTBldCfgVersionMinor(), RTBldCfgVersionBuild(), RTBldCfgRevisionStr());
    if (g_szPreparerIdJoliet[0] == '\0')
        RTStrPrintf(g_szPreparerIdJoliet, sizeof(g_szPreparerIdJoliet),
                    "IPRT ISO Maker v%s r%s", RTBldCfgVersion(), RTBldCfgRevisionStr());
    if (g_szSystemId[0] == '\0')
    {
        RTStrCopy(g_szSystemId, sizeof(g_szSystemId), RTBldCfgTargetDotArch());
        RTStrToUpper(g_szSystemId);
    }

    /*
     * Create the instance with defaults.
     */
    int              rc;
    PRTFSISOMAKERINT pThis = (PRTFSISOMAKERINT)RTMemAllocZ(sizeof(*pThis));
    if (pThis)
    {
        pThis->uMagic                       = RTFSISOMAKERINT_MAGIC;
        pThis->cRefs                        = 1;
        //pThis->fSeenContent               = false;
        //pThis->fFinalized                 = false;

        pThis->PrimaryIso.fNamespace        = RTFSISOMAKER_NAMESPACE_ISO_9660;
        pThis->PrimaryIso.offName           = RT_OFFSETOF(RTFSISOMAKEROBJ, pPrimaryName);
        pThis->PrimaryIso.uLevel            = 3; /* 30 char names, large files */
        pThis->PrimaryIso.uRockRidgeLevel   = 1;
        pThis->PrimaryIso.pszTransTbl       = (char *)g_szTransTbl;
        pThis->PrimaryIso.pszSystemId       = g_szSystemId;
        //pThis->PrimaryIso.pszVolumeId       = NULL;
        //pThis->PrimaryIso.pszSetVolumeId    = NULL;
        //pThis->PrimaryIso.pszPublisherId    = NULL;
        pThis->PrimaryIso.pszDataPreparerId = g_szPreparerIdPrimaryIso;
        //pThis->PrimaryIso.pszApplicationId  = NULL;
        //pThis->PrimaryIso.pszCopyrightFileId= NULL;
        //pThis->PrimaryIso.pszAbstractFileId = NULL;
        //pThis->PrimaryIso.pszBibliographicFileId = NULL;

        pThis->Joliet.fNamespace            = RTFSISOMAKER_NAMESPACE_JOLIET;
        pThis->Joliet.offName               = RT_OFFSETOF(RTFSISOMAKEROBJ, pJolietName);
        pThis->Joliet.uLevel                = 3;
        //pThis->Joliet.uRockRidgeLevel     = 0;
        //pThis->Joliet.pszTransTbl         = NULL;
        //pThis->Joliet.pszSystemId         = NULL;
        //pThis->Joliet.pszVolumeId         = NULL;
        //pThis->Joliet.pszSetVolumeId      = NULL;
        //pThis->Joliet.pszPublisherId      = NULL;
        pThis->Joliet.pszDataPreparerId     = g_szPreparerIdJoliet;
        //pThis->Joliet.pszApplicationId    = NULL;
        //pThis->Joliet.pszCopyrightFileId  = NULL;
        //pThis->Joliet.pszAbstractFileId   = NULL;
        //pThis->Joliet.pszBibliographicFileId = NULL;

        pThis->Udf.fNamespace               = RTFSISOMAKER_NAMESPACE_UDF;
        pThis->Udf.offName                  = RT_OFFSETOF(RTFSISOMAKEROBJ, pUdfName);
        //pThis->Udf.uLevel                 = 0;
        //pThis->Udf.uRockRidgeLevel        = 0;
        //pThis->Udf.pszTransTbl            = NULL;
        //pThis->Udf.uRockRidgeLevel        = 0;
        //pThis->Udf.pszTransTbl            = NULL;
        //pThis->Udf.pszSystemId            = NULL;
        //pThis->Udf.pszVolumeId            = NULL;
        //pThis->Udf.pszSetVolumeId         = NULL;
        //pThis->Udf.pszPublisherId         = NULL;
        //pThis->Udf.pszDataPreparerId      = NULL;
        //pThis->Udf.pszApplicationId       = NULL;
        //pThis->Udf.pszCopyrightFileId     = NULL;
        //pThis->Udf.pszAbstractFileId      = NULL;
        //pThis->Udf.pszBibliographicFileId = NULL;

        pThis->Hfs.fNamespace               = RTFSISOMAKER_NAMESPACE_HFS;
        pThis->Hfs.offName                  = RT_OFFSETOF(RTFSISOMAKEROBJ, pHfsName);
        //pThis->Hfs.uLevel                 = 0;
        //pThis->Hfs.uRockRidgeLevel        = 0;
        //pThis->Hfs.pszTransTbl            = NULL;
        //pThis->Hfs.pszSystemId            = NULL;
        //pThis->Hfs.pszVolumeId            = NULL;
        //pThis->Hfs.pszSetVolumeId         = NULL;
        //pThis->Hfs.pszPublisherId         = NULL;
        //pThis->Hfs.pszDataPreparerId      = NULL;
        //pThis->Hfs.pszApplicationId       = NULL;
        //pThis->Hfs.pszCopyrightFileId     = NULL;
        //pThis->Hfs.pszAbstractFileId      = NULL;
        //pThis->Hfs.pszBibliographicFileId = NULL;

        RTListInit(&pThis->ObjectHead);
        //pThis->cObjects                   = 0;
        //pThis->cbData                     = 0;

        pThis->cVolumeDescriptors           = 3; /* primary, secondary joliet, terminator. */

        //pThis->uidDefault                 = 0;
        //pThis->gidDefault                 = 0;
        pThis->fDefaultFileMode             = 0444 | RTFS_TYPE_FILE      | RTFS_DOS_ARCHIVED  | RTFS_DOS_READONLY;
        pThis->fDefaultDirMode              = 0555 | RTFS_TYPE_DIRECTORY | RTFS_DOS_DIRECTORY | RTFS_DOS_READONLY;

        //pThis->cCommonSources             = 0;
        //pThis->paCommonSources            = NULL;

        //pThis->pBootCatFile               = NULL;

        pThis->cbFinalizedImage             = UINT64_MAX;
        //pThis->pbSysArea                  = NULL;
        //pThis->cbSysArea                  = 0;
        //pThis->pbVolDescs                 = NULL;
        //pThis->pPrimaryVolDesc            = NULL;
        //pThis->pElToritoDesc              = NULL;
        //pThis->pJolietVolDesc             = NULL;

        pThis->PrimaryIsoDirs.offDirs       = UINT64_MAX;
        pThis->PrimaryIsoDirs.offPathTableL = UINT64_MAX;
        pThis->PrimaryIsoDirs.offPathTableM = UINT64_MAX;
        pThis->PrimaryIsoDirs.cbPathTable   = 0;
        RTListInit(&pThis->PrimaryIsoDirs.FinalizedDirs);
        //pThis->PrimaryIsoDirs.pRRSpillFile = NULL;

        pThis->JolietDirs.offDirs           = UINT64_MAX;
        pThis->JolietDirs.offPathTableL     = UINT64_MAX;
        pThis->JolietDirs.offPathTableM     = UINT64_MAX;
        pThis->JolietDirs.cbPathTable       = 0;
        RTListInit(&pThis->JolietDirs.FinalizedDirs);
        //pThis->JolietDirs.pRRSpillFile    = NULL;

        pThis->offFirstFile                 = UINT64_MAX;
        RTListInit(&pThis->FinalizedFiles);

        RTTimeNow(&pThis->ImageCreationTime);

        /*
         * Add the root directory node with idObj == 0.
         */
        PRTFSISOMAKERDIR pDirRoot;
        rc = rtFsIsoMakerAddUnnamedDirWorker(pThis, &pDirRoot);
        if (RT_SUCCESS(rc))
        {
            *phIsoMaker = pThis;
            return VINF_SUCCESS;
        }

        RTMemFree(pThis);
    }
    else
        rc = VERR_NO_MEMORY;
    return rc;
}


/**
 * Frees an object.
 *
 * This is a worker for rtFsIsoMakerDestroy and RTFsIsoMakerObjRemove.
 *
 * @param   pObj                The object to free.
 */
DECLINLINE(void) rtFsIsoMakerObjDestroy(PRTFSISOMAKEROBJ pObj)
{
    if (pObj->enmType == RTFSISOMAKEROBJTYPE_FILE)
    {
        PRTFSISOMAKERFILE pFile = (PRTFSISOMAKERFILE)pObj;
        switch (pFile->enmSrcType)
        {
            case RTFSISOMAKERSRCTYPE_PATH:
                pFile->u.pszSrcPath = NULL;
                break;

            case RTFSISOMAKERSRCTYPE_TRANS_TBL:
                pFile->u.pTransTblDir = NULL;
                break;

            case RTFSISOMAKERSRCTYPE_VFS_FILE:
                RTVfsFileRelease(pFile->u.hVfsFile);
                pFile->u.hVfsFile = NIL_RTVFSFILE;
                break;

            case RTFSISOMAKERSRCTYPE_COMMON:
                break;

            case RTFSISOMAKERSRCTYPE_INVALID:
            case RTFSISOMAKERSRCTYPE_END:
                AssertFailed();
                break;

            /* no default, want warnings */
        }
        if (pFile->pBootInfoTable)
        {
            RTMemFree(pFile->pBootInfoTable);
            pFile->pBootInfoTable = NULL;
        }
    }

    RTMemFree(pObj);
}


/**
 * Frees a namespace node.
 *
 * This is a worker for rtFsIsoMakerDestroyTree and rtFsIsoMakerObjUnsetName.
 *
 * @param   pName               The node to free.
 */
DECLINLINE(void) rtFsIsoMakerDestroyName(PRTFSISOMAKERNAME pName)
{
    if (pName->fRockRidgeNmAlloced)
    {
        RTMemFree(pName->pszRockRidgeNm);
        pName->pszRockRidgeNm = NULL;
    }
    if (pName->fTransNmAlloced)
    {
        RTMemFree(pName->pszTransNm);
        pName->pszTransNm = NULL;
    }
    RTMemFree(pName);
}


/**
 * Destroys a namespace.
 *
 * @param   pNamespace          The namespace to destroy.
 */
static void rtFsIsoMakerDestroyTree(PRTFSISOMAKERNAMESPACE pNamespace)
{
    /*
     * Recursively destroy the tree first.
     */
    PRTFSISOMAKERNAME pCur = pNamespace->pRoot;
    if (pCur)
    {
        Assert(!pCur->pParent);
        for (;;)
        {
            if (   pCur->pDir
                && pCur->pDir->cChildren)
                pCur = pCur->pDir->papChildren[pCur->pDir->cChildren - 1];
            else
            {
                PRTFSISOMAKERNAME pNext = pCur->pParent;
                rtFsIsoMakerDestroyName(pCur);

                /* Unlink from parent, we're the last entry. */
                if (pNext)
                {
                    Assert(pNext->pDir->cChildren > 0);
                    pNext->pDir->cChildren--;
                    Assert(pNext->pDir->papChildren[pNext->pDir->cChildren] == pCur);
                    pNext->pDir->papChildren[pNext->pDir->cChildren] = NULL;
                    pCur = pNext;
                }
                else
                {
                    Assert(pNamespace->pRoot == pCur);
                    break;
                }
            }
        }
        pNamespace->pRoot = NULL;
    }

    /*
     * Free the translation table filename if allocated.
     */
    if (pNamespace->pszTransTbl)
    {
        if (pNamespace->pszTransTbl != g_szTransTbl)
            RTMemFree(pNamespace->pszTransTbl);
        pNamespace->pszTransTbl = NULL;
    }

    /*
     * Free string IDs.
     */
    if (pNamespace->pszSystemId)
    {
        if (pNamespace->pszSystemId != g_szSystemId)
            RTMemFree(pNamespace->pszSystemId);
        pNamespace->pszSystemId = NULL;
    }

    if (pNamespace->pszVolumeId)
    {
        RTMemFree(pNamespace->pszVolumeId);
        pNamespace->pszVolumeId = NULL;
    }

    if (pNamespace->pszVolumeSetId)
    {
        RTMemFree(pNamespace->pszVolumeSetId);
        pNamespace->pszVolumeSetId = NULL;
    }

    if (pNamespace->pszPublisherId)
    {
        RTMemFree(pNamespace->pszPublisherId);
        pNamespace->pszPublisherId = NULL;
    }

    if (pNamespace->pszDataPreparerId)
    {
        if (   pNamespace->pszDataPreparerId != g_szPreparerIdPrimaryIso
            && pNamespace->pszDataPreparerId != g_szPreparerIdJoliet)
            RTMemFree(pNamespace->pszDataPreparerId);
        pNamespace->pszDataPreparerId = NULL;
    }

    if (pNamespace->pszApplicationId)
    {
        RTMemFree(pNamespace->pszApplicationId);
        pNamespace->pszApplicationId = NULL;
    }

    if (pNamespace->pszCopyrightFileId)
    {
        RTMemFree(pNamespace->pszCopyrightFileId);
        pNamespace->pszCopyrightFileId = NULL;
    }

    if (pNamespace->pszAbstractFileId)
    {
        RTMemFree(pNamespace->pszAbstractFileId);
        pNamespace->pszAbstractFileId = NULL;
    }

    if (pNamespace->pszBibliographicFileId)
    {
        RTMemFree(pNamespace->pszBibliographicFileId);
        pNamespace->pszBibliographicFileId = NULL;
    }
}


/**
 * Destroys an ISO maker instance.
 *
 * @param   pThis               The ISO maker instance to destroy.
 */
static void rtFsIsoMakerDestroy(PRTFSISOMAKERINT pThis)
{
    rtFsIsoMakerDestroyTree(&pThis->PrimaryIso);
    rtFsIsoMakerDestroyTree(&pThis->Joliet);
    rtFsIsoMakerDestroyTree(&pThis->Udf);
    rtFsIsoMakerDestroyTree(&pThis->Hfs);

    PRTFSISOMAKEROBJ pCur;
    PRTFSISOMAKEROBJ pNext;
    RTListForEachSafe(&pThis->ObjectHead, pCur, pNext, RTFSISOMAKEROBJ, Entry)
    {
        RTListNodeRemove(&pCur->Entry);
        rtFsIsoMakerObjDestroy(pCur);
    }

    if (pThis->paCommonSources)
    {
        RTMemFree(pThis->paCommonSources);
        pThis->paCommonSources = NULL;
    }

    pThis->uMagic = ~RTFSISOMAKERINT_MAGIC;
    RTMemFree(pThis);
}


/**
 * Retains a references to an ISO maker instance.
 *
 * @returns New reference count on success, UINT32_MAX if invalid handle.
 * @param   hIsoMaker           The ISO maker handle.
 */
RTDECL(uint32_t) RTFsIsoMakerRetain(RTFSISOMAKER hIsoMaker)
{
    PRTFSISOMAKERINT pThis = hIsoMaker;
    AssertPtrReturn(pThis, UINT32_MAX);
    AssertReturn(pThis->uMagic == RTFSISOMAKERINT_MAGIC, UINT32_MAX);
    uint32_t cRefs = ASMAtomicIncU32(&pThis->cRefs);
    Assert(cRefs > 1);
    Assert(cRefs < _64K);
    return cRefs;
}


/**
 * Releases a references to an ISO maker instance.
 *
 * @returns New reference count on success, UINT32_MAX if invalid handle.
 * @param   hIsoMaker           The ISO maker handle.  NIL is ignored.
 */
RTDECL(uint32_t) RTFsIsoMakerRelease(RTFSISOMAKER hIsoMaker)
{
    PRTFSISOMAKERINT pThis = hIsoMaker;
    uint32_t         cRefs;
    if (pThis == NIL_RTFSISOMAKER)
        cRefs = 0;
    else
    {
        AssertPtrReturn(pThis, UINT32_MAX);
        AssertReturn(pThis->uMagic == RTFSISOMAKERINT_MAGIC, UINT32_MAX);
        cRefs = ASMAtomicDecU32(&pThis->cRefs);
        Assert(cRefs < _64K);
        if (!cRefs)
            rtFsIsoMakerDestroy(pThis);
    }
    return cRefs;
}


/**
 * Sets the ISO-9660 level.
 *
 * @returns IPRT status code
 * @param   hIsoMaker           The ISO maker handle.
 * @param   uIsoLevel           The level, 1-3.
 */
RTDECL(int) RTFsIsoMakerSetIso9660Level(RTFSISOMAKER hIsoMaker, uint8_t uIsoLevel)
{
    PRTFSISOMAKERINT pThis = hIsoMaker;
    RTFSISOMAKER_ASSERT_VALID_HANDLE_RET(pThis);
    AssertReturn(uIsoLevel <= 3, VERR_INVALID_PARAMETER);
    AssertReturn(uIsoLevel > 0, VERR_INVALID_PARAMETER); /* currently not possible to disable this */
    AssertReturn(!pThis->fSeenContent, VERR_WRONG_ORDER);

    pThis->PrimaryIso.uLevel = uIsoLevel;
    return VINF_SUCCESS;
}


/**
 * Sets the joliet level.
 *
 * @returns IPRT status code
 * @param   hIsoMaker           The ISO maker handle.
 * @param   uJolietLevel        The joliet UCS-2 level 1-3, or 0 to disable
 *                              joliet.
 */
RTDECL(int) RTFsIsoMakerSetJolietUcs2Level(RTFSISOMAKER hIsoMaker, uint8_t uJolietLevel)
{
    PRTFSISOMAKERINT pThis = hIsoMaker;
    RTFSISOMAKER_ASSERT_VALID_HANDLE_RET(pThis);
    AssertReturn(uJolietLevel <= 3, VERR_INVALID_PARAMETER);
    AssertReturn(!pThis->fSeenContent, VERR_WRONG_ORDER);

    if (pThis->Joliet.uLevel != uJolietLevel)
    {
        if (uJolietLevel == 0)
            pThis->cVolumeDescriptors--;
        else if (pThis->Joliet.uLevel == 0)
            pThis->cVolumeDescriptors++;
        pThis->Joliet.uLevel = uJolietLevel;
    }
    return VINF_SUCCESS;
}


/**
 * Sets the rock ridge support level (on the primary ISO-9660 namespace).
 *
 * @returns IPRT status code
 * @param   hIsoMaker           The ISO maker handle.
 * @param   uLevel              0 if disabled, 1 to just enable, 2 to enable and
 *                              write the ER tag.
 */
RTDECL(int) RTFsIsoMakerSetRockRidgeLevel(RTFSISOMAKER hIsoMaker, uint8_t uLevel)
{
    PRTFSISOMAKERINT pThis = hIsoMaker;
    RTFSISOMAKER_ASSERT_VALID_HANDLE_RET(pThis);
    AssertReturn(!pThis->fSeenContent, VERR_WRONG_ORDER);
    AssertReturn(uLevel <= 2, VERR_INVALID_PARAMETER);

    pThis->PrimaryIso.uRockRidgeLevel = uLevel;
    return VINF_SUCCESS;
}


/**
 * Sets the rock ridge support level on the joliet namespace (experimental).
 *
 * @returns IPRT status code
 * @param   hIsoMaker           The ISO maker handle.
 * @param   uLevel              0 if disabled, 1 to just enable, 2 to enable and
 *                              write the ER tag.
 */
RTDECL(int) RTFsIsoMakerSetJolietRockRidgeLevel(RTFSISOMAKER hIsoMaker, uint8_t uLevel)
{
    PRTFSISOMAKERINT pThis = hIsoMaker;
    RTFSISOMAKER_ASSERT_VALID_HANDLE_RET(pThis);
    AssertReturn(!pThis->fSeenContent, VERR_WRONG_ORDER);
    AssertReturn(uLevel <= 2, VERR_INVALID_PARAMETER);

    pThis->Joliet.uRockRidgeLevel = uLevel;
    return VINF_SUCCESS;
}


/**
 * Sets the content of the system area, i.e. the first 32KB of the image.
 *
 * This can be used to put generic boot related stuff.
 *
 * @note    Other settings may overwrite parts of the content (yet to be
 *          determined which).
 *
 * @returns IPRT status code
 * @param   hIsoMaker           The ISO maker handle.
 * @param   pvContent           The content to put in the system area.
 * @param   cbContent           The size of the content.
 * @param   off                 The offset into the system area.
 */
RTDECL(int) RTFsIsoMakerSetSysAreaContent(RTFSISOMAKER hIsoMaker, void const *pvContent, size_t cbContent, uint32_t off)
{
    /*
     * Validate input.
     */
    PRTFSISOMAKERINT pThis = hIsoMaker;
    RTFSISOMAKER_ASSERT_VALID_HANDLE_RET(pThis);
    AssertReturn(!pThis->fFinalized, VERR_WRONG_ORDER);
    AssertReturn(cbContent > 0, VERR_OUT_OF_RANGE);
    AssertReturn(cbContent <= _32K, VERR_OUT_OF_RANGE);
    AssertReturn(off < _32K, VERR_OUT_OF_RANGE);
    size_t cbSysArea = off + cbContent;
    AssertReturn(cbSysArea <= _32K, VERR_OUT_OF_RANGE);

    /*
     * Adjust the allocation and copy over the new/additional content.
     */
    if (pThis->cbSysArea < cbSysArea)
    {
        void *pvNew = RTMemRealloc(pThis->pbSysArea, cbSysArea);
        AssertReturn(pvNew, VERR_NO_MEMORY);
        pThis->pbSysArea = (uint8_t *)pvNew;
        memset(&pThis->pbSysArea[pThis->cbSysArea], 0, cbSysArea - pThis->cbSysArea);
    }

    memcpy(&pThis->pbSysArea[off], pvContent, cbContent);

    return VINF_SUCCESS;
}


/*
 *
 * Name space related internals.
 * Name space related internals.
 * Name space related internals.
 *
 */


/**
 * Gets the pointer to the name member for the given namespace.
 *
 * @returns Pointer to name member.
 * @param   pObj                The object to find a name member in.
 * @param   pNamespace          The namespace which name to calculate.
 */
DECLINLINE(PPRTFSISOMAKERNAME) rtFsIsoMakerObjGetNameForNamespace(PRTFSISOMAKEROBJ pObj, PCRTFSISOMAKERNAMESPACE pNamespace)
{
    return (PPRTFSISOMAKERNAME)((uintptr_t)pObj + pNamespace->offName);
}


/**
 * Locates a child object by its namespace name.
 *
 * @returns Pointer to the child if found, NULL if not.
 * @param   pDirObj             The directory object to search.
 * @param   pszEntry            The (namespace) entry name.
 * @param   cchEntry            The length of the name.
 */
static PRTFSISOMAKERNAME rtFsIsoMakerFindObjInDir(PRTFSISOMAKERNAME pDirObj, const char *pszEntry, size_t cchEntry)
{
    if (pDirObj)
    {
        PRTFSISOMAKERNAMEDIR pDir = pDirObj->pDir;
        AssertReturn(pDir, NULL);

        uint32_t i = pDir->cChildren;
        while (i-- > 0)
        {
            PRTFSISOMAKERNAME pChild = pDir->papChildren[i];
            if (   pChild->cchName == cchEntry
                && RTStrNICmp(pChild->szName, pszEntry, cchEntry) == 0)
                return pChild;
        }
    }
    return NULL;
}


/**
 * Compares the two names according to ISO-9660 directory sorting rules.
 *
 * As long as we don't want to do case insensitive joliet sorting, this works
 * for joliet names to, I think.
 *
 * @returns 0 if equal, -1 if pszName1 comes first, 1 if pszName2 comes first.
 * @param   pszName1            The first name.
 * @param   pszName2            The second name.
 */
DECLINLINE(int) rtFsIsoMakerCompareIso9660Names(const char *pszName1, const char *pszName2)
{
    for (;;)
    {
        char const ch1 = *pszName1++;
        char const ch2 = *pszName2++;
        if (ch1 == ch2)
        {
            if (ch1)
            { /* likely */ }
            else
                return 0;
        }
        else if (ch1 == ';' || ch2 == ';')
            return ch1 == ';' ? -1 : 1;
        else if (ch1 == '.' || ch2 == '.')
            return ch1 == '.' ? -1 : 1;
        else
            return (unsigned char)ch1 < (unsigned char)ch2 ? -1 : 1;
    }
}


/**
 * Finds the index into papChildren where the given name should be inserted.
 *
 * @returns Index of the given name.
 * @param   pNamespace          The namspace.
 * @param   pParent             The parent namespace node.
 * @param   pszName             The name.
 */
static uint32_t rtFsIsoMakerFindInsertIndex(PRTFSISOMAKERNAMESPACE pNamespace, PRTFSISOMAKERNAME pParent, const char *pszName)
{
    uint32_t idxRet = pParent->pDir->cChildren;
    if (idxRet > 0)
    {
        /*
         * The idea is to do binary search using a namespace specific compare
         * function.  However, it looks like we can get away with using the
         * same compare function for all namespaces.
         */
        uint32_t            idxStart = 0;
        uint32_t            idxEnd   = idxRet;
        PPRTFSISOMAKERNAME  papChildren = pParent->pDir->papChildren;
        switch (pNamespace->fNamespace)
        {
            case RTFSISOMAKER_NAMESPACE_ISO_9660:
            case RTFSISOMAKER_NAMESPACE_JOLIET:
            case RTFSISOMAKER_NAMESPACE_UDF:
            case RTFSISOMAKER_NAMESPACE_HFS:
                for (;;)
                {
                    idxRet = idxStart + (idxEnd - idxStart) / 2;
                    PRTFSISOMAKERNAME pCur = papChildren[idxRet];
                    int iDiff = rtFsIsoMakerCompareIso9660Names(pszName, pCur->szName);
                    if (iDiff < 0)
                    {
                        if (idxRet > idxStart)
                            idxEnd = idxRet;
                        else
                            break;
                    }
                    else
                    {
                        idxRet++;
                        if (   iDiff != 0
                            && idxRet < idxEnd)
                            idxStart = idxRet;
                        else
                            break;
                    }
                }
                break;

            default:
                AssertFailed();
                break;
        }
    }
    return idxRet;
}



/**
 * Locates a child entry by its specified name.
 *
 * @returns Pointer to the child if found, NULL if not.
 * @param   pDirName            The directory name to search.
 * @param   pszEntry            The (specified) entry name.
 * @param   cchEntry            The length of the name.
 */
static PRTFSISOMAKERNAME rtFsIsoMakerFindEntryInDirBySpec(PRTFSISOMAKERNAME pDirName, const char *pszEntry, size_t cchEntry)
{
    if (pDirName)
    {
        PRTFSISOMAKERNAMEDIR pDir = pDirName->pDir;
        AssertReturn(pDir, NULL);

        uint32_t i = pDir->cChildren;
        while (i-- > 0)
        {
            PRTFSISOMAKERNAME pChild = pDir->papChildren[i];
            if (   pChild->cchSpecNm == cchEntry
                && RTStrNICmp(pChild->pszSpecNm, pszEntry, cchEntry) == 0)
                return pChild;
        }
    }
    return NULL;
}


/**
 * Locates a subdir object in any namespace by its specified name.
 *
 * This is used to avoid having one instance of RTFSISOMAKERDIR in each
 * namespace for the same directory.
 *
 * @returns Pointer to the subdir object if found, NULL if not.
 * @param   pDirObj             The directory object to search.
 * @param   pszEntry            The (specified) entry name.
 * @param   cchEntry            The length of the name.
 * @param   fSkipNamespaces     Namespaces to skip.
 * @sa      rtFsIsoMakerFindEntryInDirBySpec
 */
static PRTFSISOMAKERDIR rtFsIsoMakerFindSubdirBySpec(PRTFSISOMAKERDIR pDirObj, const char *pszEntry, size_t cchEntry,
                                                     uint32_t fSkipNamespaces)
{
    AssertReturn(pDirObj, NULL);
    AssertReturn(pDirObj->Core.enmType == RTFSISOMAKEROBJTYPE_DIR, NULL);
    for (uint32_t i = 0; i < RT_ELEMENTS(g_aRTFsIsoNamespaces); i++)
        if (!(fSkipNamespaces & g_aRTFsIsoNamespaces[i].fNamespace))
        {
            PRTFSISOMAKERNAME pDirName = *(PPRTFSISOMAKERNAME)((uintptr_t)pDirObj + g_aRTFsIsoNamespaces[i].offName);
            if (pDirName)
            {
                PRTFSISOMAKERNAMEDIR pDir = pDirName->pDir;
                AssertStmt(pDir, continue);

                uint32_t iChild = pDir->cChildren;
                while (iChild-- > 0)
                {
                    PRTFSISOMAKERNAME pChild = pDir->papChildren[iChild];
                    if (   pChild->cchSpecNm == cchEntry
                        && pChild->pDir      != NULL
                        && RTStrNICmp(pChild->pszSpecNm, pszEntry, cchEntry) == 0)
                        return (PRTFSISOMAKERDIR)pChild->pObj;
                }
            }
        }
    return NULL;
}


/**
 * Walks the given path by specified object names in a namespace.
 *
 * @returns IPRT status code.
 * @param   pNamespace  The namespace to walk the path in.
 * @param   pszPath     The path to walk.
 * @param   ppName      Where to return the name node that the path ends with.
 */
static int rtFsIsoMakerWalkPathBySpec(PRTFSISOMAKERNAMESPACE pNamespace, const char *pszPath, PPRTFSISOMAKERNAME ppName)
{
    *ppName = NULL;
    AssertReturn(RTPATH_IS_SLASH(*pszPath), VERR_INVALID_NAME);

    /*
     * Deal with the special case of the root.
     */
    while (RTPATH_IS_SLASH(*pszPath))
        pszPath++;
    AssertReturn(*pszPath, VERR_INTERNAL_ERROR_4);

    PRTFSISOMAKERNAME pCur = pNamespace->pRoot;
    if (!pCur)
        return *pszPath ? VERR_PATH_NOT_FOUND : VERR_FILE_NOT_FOUND;
    if (!*pszPath)
    {
        *ppName = pCur;
        return VINF_SUCCESS;
    }

    /*
     * Now, do the rest of the path.
     */
    for (;;)
    {
        /*
         * Find the end of the component.
         */
        char ch;
        size_t cchComponent = 0;
        while ((ch = pszPath[cchComponent]) != '\0' && !RTPATH_IS_SLASH(ch))
            cchComponent++;
        if (!cchComponent)
        {
            *ppName = pCur;
            return VINF_SUCCESS;
        }

        size_t offNext = cchComponent;
        while (RTPATH_IS_SLASH(ch))
            ch = pszPath[offNext++];

        /*
         * Deal with dot and dot-dot.
         */
        if (cchComponent == 1 && pszPath[0] == '.')
        { /* nothing to do */ }
        else if (cchComponent == 2 && pszPath[0] == '.' && pszPath[1] == '.')
        {
            if (pCur->pParent)
                pCur = pCur->pParent;
        }
        /*
         * Look up the name.
         */
        else
        {
            PRTFSISOMAKERNAME pChild = rtFsIsoMakerFindEntryInDirBySpec(pCur, pszPath, cchComponent);
            if (!pChild)
                return pszPath[offNext] ? VERR_PATH_NOT_FOUND : VERR_FILE_NOT_FOUND;
            if (   (offNext > cchComponent)
                && !pChild->pDir)
                return VERR_NOT_A_DIRECTORY;
            pCur = pChild;
        }

        /*
         * Skip ahead in the path.
         */
        pszPath += offNext;
    }
}


/**
 * Copy and convert a name to valid ISO-9660 (d-characters only).
 *
 * Worker for rtFsIsoMakerNormalizeNameForNamespace.  ASSUMES it deals with
 * dots.
 *
 * @returns Length of the resulting string.
 * @param   pszDst      The output buffer.
 * @param   cchDstMax   The maximum number of (d-chars) to put in the output
 *                      buffer.
 * @param   pchSrc      The UTF-8 source string (not neccessarily terminated).
 * @param   cchSrc      The maximum number of chars to copy from the source
 *                      string.
 */
static size_t rtFsIsoMakerCopyIso9660Name(char *pszDst, size_t cchDstMax, const char *pchSrc, size_t cchSrc)
{
    const char *pchSrcIn = pchSrc;
    size_t      offDst = 0;
    while ((size_t)(pchSrc - pchSrcIn) < cchSrc)
    {
        RTUNICP uc;
        int rc = RTStrGetCpEx(&pchSrc, &uc);
        if (RT_SUCCESS(rc))
        {
            if (   uc < 128
                && RTFSISOMAKER_IS_UPPER_IN_D_CHARS((char)uc))
            {
                pszDst[offDst++] = RT_C_TO_UPPER((char)uc);
                if (offDst >= cchDstMax)
                    break;
            }
        }
    }
    pszDst[offDst] = '\0';
    return offDst;
}


/**
 * Normalizes a name for the primary ISO-9660 namespace.
 *
 * @returns IPRT status code.
 * @param   pThis       The ISO maker instance.
 * @param   pParent     The parent directory.  NULL if root.
 * @param   pchSrc      The specified name to normalize (not necessarily zero
 *                      terminated).
 * @param   cchSrc      The length of the specified name.
 * @param   fIsDir      Indicates whether it's a directory or file (like).
 * @param   pszDst      The output buffer.  Must be at least 32 bytes.
 * @param   cbDst       The size of the output buffer.
 * @param   pcchDst     Where to return the length of the returned string (i.e.
 *                      not counting the terminator).
 * @param   pcbInDirRec Where to return the name size in the directory record.
 */
static int rtFsIsoMakerNormalizeNameForPrimaryIso9660(PRTFSISOMAKERINT pThis, PRTFSISOMAKERNAME pParent,
                                                      const char *pchSrc, size_t cchSrc, bool fIsDir,
                                                      char *pszDst, size_t cbDst, size_t *pcchDst, size_t *pcbInDirRec)
{
    AssertReturn(cbDst > ISO9660_MAX_NAME_LEN + 2, VERR_INTERNAL_ERROR_3);

    /* Skip leading dots. */
    while (cchSrc > 0 && *pchSrc == '.')
        pchSrc++, cchSrc--;
    if (!cchSrc)
    {
        pchSrc = "DOTS";
        cchSrc = 4;
    }

    /*
     * Produce a first name.
     */
    uint8_t const uIsoLevel = pThis->PrimaryIso.uLevel;
    size_t cchDst;
    size_t offDstDot;
    if (fIsDir)
        offDstDot = cchDst = rtFsIsoMakerCopyIso9660Name(pszDst, uIsoLevel >= 2 ? ISO9660_MAX_NAME_LEN : 8,
                                                         pchSrc, cchSrc);
    else
    {
        /* Look for the last dot and try preserve the extension when doing the conversion. */
        size_t offLastDot = cchSrc;
        for (size_t off = 0; off < cchSrc; off++)
            if (pchSrc[off] == '.')
                offLastDot = off;

        if (offLastDot == cchSrc)
            offDstDot = cchDst = rtFsIsoMakerCopyIso9660Name(pszDst, uIsoLevel >= 2 ? ISO9660_MAX_NAME_LEN : 8,
                                                             pchSrc, cchSrc);
        else
        {
            const char * const pchSrcExt = &pchSrc[offLastDot + 1];
            size_t       const cchSrcExt = cchSrc - offLastDot - 1;
            if (uIsoLevel < 2)
            {
                cchDst = rtFsIsoMakerCopyIso9660Name(pszDst, 8, pchSrc, cchSrc);
                offDstDot = cchDst;
                pszDst[cchDst++] = '.';
                cchDst += rtFsIsoMakerCopyIso9660Name(&pszDst[cchDst], 3, pchSrcExt, cchSrcExt);
            }
            else
            {
                size_t cchDstExt = rtFsIsoMakerCopyIso9660Name(pszDst, ISO9660_MAX_NAME_LEN - 2, pchSrcExt, cchSrcExt);
                if (cchDstExt > 0)
                {
                    size_t cchBasename = rtFsIsoMakerCopyIso9660Name(pszDst, ISO9660_MAX_NAME_LEN - 2,
                                                                     pchSrc, offLastDot);
                    if (cchBasename + 1 + cchDstExt <= ISO9660_MAX_NAME_LEN)
                        cchDst = cchBasename;
                    else
                        cchDst = ISO9660_MAX_NAME_LEN - 1 - RT_MIN(cchDstExt, 4);
                    offDstDot = cchDst;
                    pszDst[cchDst++] = '.';
                    cchDst += rtFsIsoMakerCopyIso9660Name(&pszDst[cchDst], ISO9660_MAX_NAME_LEN - 1 - cchDst,
                                                          pchSrcExt, cchSrcExt);
                }
                else
                    offDstDot = cchDst = rtFsIsoMakerCopyIso9660Name(pszDst, ISO9660_MAX_NAME_LEN, pchSrc, cchSrc);
            }
        }
    }

    /* Append version if not directory */
    if (!fIsDir)
    {
        pszDst[cchDst++] = ';';
        pszDst[cchDst++] = '1';
        pszDst[cchDst]   = '\0';
    }

    /*
     * Unique name?
     */
    if (!rtFsIsoMakerFindObjInDir(pParent, pszDst, cchDst))
    {
        *pcchDst     = cchDst;
        *pcbInDirRec = cchDst;
        return VINF_SUCCESS;
    }

    /*
     * Mangle the name till we've got a unique one.
     */
    size_t const cchMaxBasename = (uIsoLevel >= 2 ? ISO9660_MAX_NAME_LEN : 8) - (cchDst - offDstDot);
    size_t       cchInserted = 0;
    for (uint32_t i = 0; i < _32K; i++)
    {
        /* Add a numberic infix. */
        char szOrd[64];
        size_t cchOrd = RTStrFormatU32(szOrd, sizeof(szOrd), i + 1, 10, -1, -1, 0 /*fFlags*/);
        Assert((ssize_t)cchOrd > 0);

        /* Do we need to shuffle the suffix? */
        if (cchOrd > cchInserted)
        {
            if (offDstDot < cchMaxBasename)
            {
                memmove(&pszDst[offDstDot + 1], &pszDst[offDstDot], cchDst + 1 - offDstDot);
                cchDst++;
                offDstDot++;
            }
            cchInserted = cchOrd;
        }

        /* Insert the new infix and try again. */
        memcpy(&pszDst[offDstDot - cchOrd], szOrd, cchOrd);
        if (!rtFsIsoMakerFindObjInDir(pParent, pszDst, cchDst))
        {
            *pcchDst     = cchDst;
            *pcbInDirRec = cchDst;
            return VINF_SUCCESS;
        }
    }
    AssertFailed();
    return VERR_DUPLICATE;
}


/**
 * Normalizes a name for the specified name space.
 *
 * @returns IPRT status code.
 * @param   pThis       The ISO maker instance.
 * @param   pNamespace  The namespace which rules to normalize it according to.
 * @param   pParent     The parent directory.  NULL if root.
 * @param   pchSrc      The specified name to normalize (not necessarily zero
 *                      terminated).
 * @param   cchSrc      The length of the specified name.
 * @param   fIsDir      Indicates whether it's a directory or file (like).
 * @param   pszDst      The output buffer.  Must be at least 32 bytes.
 * @param   cbDst       The size of the output buffer.
 * @param   pcchDst     Where to return the length of the returned string (i.e.
 *                      not counting the terminator).
 * @param   pcbInDirRec Where to return the name size in the directory record.
 */
static int rtFsIsoMakerNormalizeNameForNamespace(PRTFSISOMAKERINT pThis, PRTFSISOMAKERNAMESPACE pNamespace,
                                                 PRTFSISOMAKERNAME pParent, const char *pchSrc, size_t cchSrc, bool fIsDir,
                                                 char *pszDst, size_t cbDst, size_t *pcchDst, size_t *pcbInDirRec)
{
    if (cchSrc > 0)
    {
        /*
         * Check that the object doesn't already exist.
         */
        AssertReturn(!rtFsIsoMakerFindEntryInDirBySpec(pParent, pchSrc, cchSrc), VERR_ALREADY_EXISTS);
        switch (pNamespace->fNamespace)
        {
            /*
             * This one is a lot of work, so separate function.
             */
            case RTFSISOMAKER_NAMESPACE_ISO_9660:
                return rtFsIsoMakerNormalizeNameForPrimaryIso9660(pThis, pParent, pchSrc, cchSrc, fIsDir,
                                                                  pszDst, cbDst, pcchDst, pcbInDirRec);

            /*
             * At the moment we don't give darn about UCS-2 limitations here...
             */
            case RTFSISOMAKER_NAMESPACE_JOLIET:
            {
/** @todo Joliet name limit and check for duplicates.   */
                AssertReturn(cbDst > cchSrc, VERR_BUFFER_OVERFLOW);
                memcpy(pszDst, pchSrc, cchSrc);
                pszDst[cchSrc] = '\0';
                *pcchDst     = cchSrc;
                *pcbInDirRec = RTStrCalcUtf16Len(pszDst) * sizeof(RTUTF16);
                return VINF_SUCCESS;
            }

            case RTFSISOMAKER_NAMESPACE_UDF:
            case RTFSISOMAKER_NAMESPACE_HFS:
                AssertFailedReturn(VERR_NOT_IMPLEMENTED);

            default:
                AssertFailedReturn(VERR_INTERNAL_ERROR_2);
        }
    }
    else
    {
        /*
         * Root special case.
         *
         * For ISO-9660 and joliet, we enter it with a length of 1 byte.  The
         * value byte value is zero.  The path tables we generate won't be
         * accepted by windows unless we do this.
         */
        *pszDst      = '\0';
        *pcchDst     = 0;
        *pcbInDirRec = pNamespace->fNamespace & (RTFSISOMAKER_NAMESPACE_ISO_9660 | RTFSISOMAKER_NAMESPACE_JOLIET) ? 1 : 0;
        AssertReturn(!pParent, VERR_INTERNAL_ERROR_3);
        return VINF_SUCCESS;
    }
}


/**
 * Creates a TRANS.TBL file object for a newly named directory.
 *
 * The file is associated with the namespace node for the directory.  The file
 * will be generated on the fly from the directory object.
 *
 * @returns IPRT status code.
 * @param   pThis       The ISO maker instance.
 * @param   pNamespace  The namespace.
 * @param   pDirName    The new name space node for the directory.
 */
static int rtFsIsoMakerAddTransTblFileToNewDir(PRTFSISOMAKERINT pThis, PRTFSISOMAKERNAMESPACE pNamespace,
                                               PRTFSISOMAKERNAME pDirName)
{
    /*
     * Create a file object for it.
     */
    PRTFSISOMAKERFILE pFile;
    int rc = rtFsIsoMakerAddUnnamedFileWorker(pThis, NULL, 0, &pFile);
    if (RT_SUCCESS(rc))
    {
        pFile->enmSrcType     = RTFSISOMAKERSRCTYPE_TRANS_TBL;
        pFile->u.pTransTblDir = pDirName;
        pFile->pBootInfoTable = NULL;
        pDirName->pDir->pTransTblFile = pFile;

        /*
         * Add it to the directory.
         */
        PRTFSISOMAKERNAME pTransTblNm;
        rc = rtFsIsoMakerObjSetName(pThis, pNamespace, &pFile->Core, pDirName,
                                    pNamespace->pszTransTbl, strlen(pNamespace->pszTransTbl), &pTransTblNm);
        if (RT_SUCCESS(rc))
        {
            pTransTblNm->cchTransNm = 0;
            return VINF_SUCCESS;
        }

        /*
         * Bail.
         */
        pDirName->pDir->pTransTblFile = NULL;
        rtFsIsoMakerObjRemoveWorker(pThis, &pFile->Core);
    }
    return rc;
}


/**
 * Sets the name of an object in a namespace.
 *
 * If the object is already named in the name space, it will first be removed
 * from that namespace.  Should we run out of memory or into normalization
 * issues after removing it, its original state will _not_ be restored.
 *
 * @returns IPRT status code.
 * @param   pThis       The ISO maker instance.
 * @param   pNamespace  The namespace.
 * @param   pObj        The object to name.
 * @param   pParent     The parent namespace entry
 * @param   pchSpec     The specified name (not necessarily terminated).
 * @param   cchSpec     The specified name length.
 * @param   ppNewName   Where to return the name entry.  Optional.
 */
static int rtFsIsoMakerObjSetName(PRTFSISOMAKERINT pThis, PRTFSISOMAKERNAMESPACE pNamespace, PRTFSISOMAKEROBJ pObj,
                                  PRTFSISOMAKERNAME pParent, const char *pchSpec, size_t cchSpec, PPRTFSISOMAKERNAME ppNewName)
{
    Assert(cchSpec < _32K);

    /*
     * If the object is already named, unset that name before continuing.
     */
    if (*rtFsIsoMakerObjGetNameForNamespace(pObj, pNamespace))
    {
        int rc = rtFsIsoMakerObjUnsetName(pThis, pNamespace, pObj);
        if (RT_FAILURE(rc))
            return rc;
    }

    /*
     * To avoid need to revert anything, make sure papChildren in the parent is
     * large enough.  If root object, make sure we haven't got a root already.
     */
    if (pParent)
    {
        AssertReturn(pParent->pDir, VERR_INTERNAL_ERROR_5);
        uint32_t cChildren = pParent->pDir->cChildren;
        if (cChildren & 31)
        { /* likely */ }
        else
        {
            AssertReturn(cChildren < RTFSISOMAKER_MAX_OBJECTS_PER_DIR, VERR_TOO_MUCH_DATA);
            void *pvNew = RTMemRealloc(pParent->pDir->papChildren, (cChildren + 32) * sizeof(pParent->pDir->papChildren[0]));
            AssertReturn(pvNew, VERR_NO_MEMORY);
            pParent->pDir->papChildren = (PPRTFSISOMAKERNAME)pvNew;
        }
    }
    else
        AssertReturn(pNamespace->pRoot == NULL, VERR_INTERNAL_ERROR_5);

    /*
     * Normalize the name for this namespace.
     */
    size_t cchName        = 0;
    size_t cbNameInDirRec = 0;
    char   szName[RTFSISOMAKER_MAX_NAME_BUF];
    int rc = rtFsIsoMakerNormalizeNameForNamespace(pThis, pNamespace, pParent, pchSpec, cchSpec,
                                                   pObj->enmType == RTFSISOMAKEROBJTYPE_DIR,
                                                   szName, sizeof(szName), &cchName, &cbNameInDirRec);
    if (RT_SUCCESS(rc))
    {
        Assert(cbNameInDirRec > 0);

        size_t cbName = sizeof(RTFSISOMAKERNAME)
                      + cchName + 1
                      + cchSpec + 1;
        if (pObj->enmType == RTFSISOMAKEROBJTYPE_DIR)
            cbName = RT_ALIGN_Z(cbName, 8) + sizeof(RTFSISOMAKERNAMEDIR);
        PRTFSISOMAKERNAME pName = (PRTFSISOMAKERNAME)RTMemAllocZ(cbName);
        if (pName)
        {
            pName->pObj                 = pObj;
            pName->pParent              = pParent;
            pName->cbNameInDirRec       = (uint16_t)cbNameInDirRec;
            pName->cchName              = (uint16_t)cchName;

            char *pszDst = &pName->szName[cchName + 1];
            memcpy(pszDst, pchSpec, cchSpec);
            pszDst[cchSpec] = '\0';
            pName->pszSpecNm            = pszDst;
            pName->pszRockRidgeNm       = pszDst;
            pName->pszTransNm           = pszDst;
            pName->cchSpecNm            = (uint16_t)cchSpec;
            pName->cchRockRidgeNm       = (uint16_t)cchSpec;
            pName->cchTransNm           = (uint16_t)cchSpec;
            pName->uDepth               = pParent ? pParent->uDepth + 1 : 0;
            pName->fRockRidgeNmAlloced  = false;
            pName->fTransNmAlloced      = false;

            pName->fMode                = pObj->fMode;
            pName->uid                  = pObj->uid;
            pName->gid                  = pObj->gid;
            pName->Device               = 0;
            pName->INode                = pObj->idxObj;
            pName->cHardlinks           = 1;
            pName->offDirRec            = UINT32_MAX;
            pName->cbDirRec             = 0;
            pName->cbDirRecWithZeroPad  = 0;

            memcpy(pName->szName, szName, cchName);
            pName->szName[cchName] = '\0';

            if (pObj->enmType != RTFSISOMAKEROBJTYPE_DIR)
                pName->pDir             = NULL;
            else
            {
                size_t offDir = RT_UOFFSETOF(RTFSISOMAKERNAME, szName) + cchName + 1 + cchSpec + 1;
                offDir = RT_ALIGN_Z(offDir, 8);
                PRTFSISOMAKERNAMEDIR pDir = (PRTFSISOMAKERNAMEDIR)((uintptr_t)pName + offDir);
                pDir->offDir        = UINT64_MAX;
                pDir->cbDir         = 0;
                pDir->cChildren     = 0;
                pDir->papChildren   = NULL;
                pDir->pTransTblFile = NULL;
                pDir->pName         = pName;
                pDir->offPathTable  = UINT32_MAX;
                pDir->idPathTable   = UINT16_MAX;
                pDir->cbDirRec00    = 0;
                pDir->cbDirRec01    = 0;
                RTListInit(&pDir->FinalizedEntry);
                pName->pDir = pDir;

                /* Create the TRANS.TBL file object and enter it into this directory as the first entry. */
                if (pNamespace->pszTransTbl)
                {
                    rc = rtFsIsoMakerAddTransTblFileToNewDir(pThis, pNamespace, pName);
                    if (RT_FAILURE(rc))
                    {
                        RTMemFree(pName);
                        return rc;
                    }
                }
            }

            /*
             * Do the linking and stats.  We practice insertion sorting.
             */
            if (pParent)
            {
                uint32_t idxName   = rtFsIsoMakerFindInsertIndex(pNamespace, pParent, pName->szName);
                uint32_t cChildren = pParent->pDir->cChildren;
                if (idxName < cChildren)
                    memmove(&pParent->pDir->papChildren[idxName + 1], &pParent->pDir->papChildren[idxName],
                            (cChildren - idxName) * sizeof(pParent->pDir->papChildren[0]));
                pParent->pDir->papChildren[idxName] = pName;
                pParent->pDir->cChildren++;
            }
            else
                pNamespace->pRoot = pName;
            *rtFsIsoMakerObjGetNameForNamespace(pObj, pNamespace) = pName;
            pNamespace->cNames++;

            /*
             * Done.
             */
            if (ppNewName)
                *ppNewName = pName;
            return VINF_SUCCESS;
        }
    }
    return rc;
}


/**
 * Walks the path up to the parent, creating missing directories as needed.
 *
 * As usual, we walk the specified names rather than the mangled ones.
 *
 * @returns IPRT status code.
 * @param   pThis       The ISO maker instance.
 * @param   pNamespace  The namespace to walk.
 * @param   pszPath     The path to walk.
 * @param   ppParent    Where to return the pointer to the parent
 *                      namespace node.
 * @param   ppszEntry   Where to return the pointer to the final name component.
 * @param   pcchEntry   Where to return the length of the final name component.
 */
static int rtFsIsoMakerCreatePathToParent(PRTFSISOMAKERINT pThis, PRTFSISOMAKERNAMESPACE pNamespace, const char *pszPath,
                                          PPRTFSISOMAKERNAME ppParent, const char **ppszEntry, size_t *pcchEntry)
{
    *ppParent  = NULL; /* shut up gcc */
    *ppszEntry = NULL; /* shut up gcc */
    *pcchEntry = 0;    /* shut up gcc */

    int rc;
    AssertReturn(RTPATH_IS_SLASH(*pszPath), VERR_INTERNAL_ERROR_4);

    /*
     * Deal with the special case of the root.
     */
    while (RTPATH_IS_SLASH(*pszPath))
        pszPath++;
    AssertReturn(*pszPath, VERR_INTERNAL_ERROR_4); /* We should not be called on a root path. */

    PRTFSISOMAKERNAME pParent = pNamespace->pRoot;
    if (!pParent)
    {
        PRTFSISOMAKERDIR pDir = RTListGetFirst(&pThis->ObjectHead, RTFSISOMAKERDIR, Core.Entry);
#ifdef RT_STRICT
        Assert(pDir);
        Assert(pDir->Core.idxObj == 0);
        Assert(pDir->Core.enmType == RTFSISOMAKEROBJTYPE_DIR);
        Assert(*rtFsIsoMakerObjGetNameForNamespace(&pDir->Core, pNamespace) == NULL);
#endif

        rc = rtFsIsoMakerObjSetName(pThis, pNamespace, &pDir->Core, NULL /*pParent*/, "", 0, &pParent);
        AssertRCReturn(rc, rc);
        pParent = pNamespace->pRoot;
        AssertReturn(pParent, VERR_INTERNAL_ERROR_4);
    }

    /*
     * Now, do the rest of the path.
     */
    for (;;)
    {
        /*
         * Find the end of the component and see if its the final one or not.
         */
        char ch;
        size_t cchComponent = 0;
        while ((ch = pszPath[cchComponent]) != '\0' && !RTPATH_IS_SLASH(ch))
            cchComponent++;
        AssertReturn(cchComponent > 0, VERR_INTERNAL_ERROR_4);

        size_t offNext = cchComponent;
        while (RTPATH_IS_SLASH(ch))
            ch = pszPath[++offNext];

        if (ch == '\0')
        {
            /*
             * Final component.  Make sure it is not dot or dot-dot before returning.
             */
            AssertReturn(   pszPath[0] != '.'
                         || cchComponent > 2
                         || (   cchComponent == 2
                             && pszPath[1] != '.'),
                         VERR_INVALID_NAME);

            *ppParent  = pParent;
            *ppszEntry = pszPath;
            *pcchEntry = cchComponent;
            return VINF_SUCCESS;
        }

        /*
         * Deal with dot and dot-dot.
         */
        if (cchComponent == 1 && pszPath[0] == '.')
        { /* nothing to do */ }
        else if (cchComponent == 2 && pszPath[0] == '.' && pszPath[1] == '.')
        {
            if (pParent->pParent)
                pParent = pParent->pParent;
        }
        /*
         * Look it up.
         */
        else
        {
            PRTFSISOMAKERNAME pChild = rtFsIsoMakerFindEntryInDirBySpec(pParent, pszPath, cchComponent);
            if (pChild)
            {
                if (pChild->pDir)
                    pParent = pChild;
                else
                    return VERR_NOT_A_DIRECTORY;
            }
            else
            {
                /* Try see if we've got a directory with the same spec name in a different namespace.
                   (We don't want to waste heap by creating a directory instance per namespace.) */
                PRTFSISOMAKERDIR pChildObj = rtFsIsoMakerFindSubdirBySpec((PRTFSISOMAKERDIR)pParent->pObj,
                                                                           pszPath, cchComponent, pNamespace->fNamespace);
                if (pChildObj)
                {
                    PPRTFSISOMAKERNAME ppChildName = rtFsIsoMakerObjGetNameForNamespace(&pChildObj->Core, pNamespace);
                    if (!*ppChildName)
                    {
                        rc = rtFsIsoMakerObjSetName(pThis, pNamespace, &pChildObj->Core, pParent, pszPath, cchComponent, &pChild);
                        if (RT_FAILURE(rc))
                            return rc;
                        AssertReturn(pChild != NULL, VERR_INTERNAL_ERROR_4);
                    }
                }
                /* If we didn't have luck in other namespaces, create a new directory. */
                if (!pChild)
                {
                    rc = rtFsIsoMakerAddUnnamedDirWorker(pThis, &pChildObj);
                    if (RT_SUCCESS(rc))
                        rc = rtFsIsoMakerObjSetName(pThis, pNamespace, &pChildObj->Core, pParent, pszPath, cchComponent, &pChild);
                    if (RT_FAILURE(rc))
                        return rc;
                    AssertReturn(pChild != NULL, VERR_INTERNAL_ERROR_4);
                }
                pParent = pChild;
            }
        }

        /*
         * Skip ahead in the path.
         */
        pszPath += offNext;
    }
}


/**
 * Worker for RTFsIsoMakerObjSetPath that operates on a single namespace.
 *
 * @returns IPRT status code.
 * @param   pThis           The ISO maker instance.
 * @param   pNamespace      The namespace to name it in.
 * @param   pObj            The filesystem object to name.
 * @param   pszPath         The path to the entry in the namespace.
 */
static int rtFsIsoMakerObjSetPathInOne(PRTFSISOMAKERINT pThis, PRTFSISOMAKERNAMESPACE pNamespace,
                                       PRTFSISOMAKEROBJ pObj, const char *pszPath)
{
    AssertReturn(*rtFsIsoMakerObjGetNameForNamespace(pObj, pNamespace) == NULL, VERR_WRONG_ORDER);
    AssertReturn(RTPATH_IS_SLASH(*pszPath), VERR_INTERNAL_ERROR_5);

    /*
     * Figure out where the parent is.
     * This will create missing parent name space entries and directory nodes.
     */
    PRTFSISOMAKERNAME   pParent;
    const char         *pszEntry;
    size_t              cchEntry;
    int                 rc;
    if (pszPath[1] != '\0')
        rc = rtFsIsoMakerCreatePathToParent(pThis, pNamespace, pszPath, &pParent, &pszEntry, &cchEntry);
    else
    {
        /*
         * Special case for the root directory.
         */
        Assert(pObj->enmType == RTFSISOMAKEROBJTYPE_DIR);
        AssertReturn(pNamespace->pRoot == NULL, VERR_WRONG_ORDER);
        pszEntry = "/";
        cchEntry = 0;
        pParent  = NULL;
        rc       = VINF_SUCCESS;
    }

    /*
     * Do the job on the final path component.
     */
    if (RT_SUCCESS(rc))
    {
        AssertReturn(!RTPATH_IS_SLASH(pszEntry[cchEntry]) || pObj->enmType == RTFSISOMAKEROBJTYPE_DIR,
                     VERR_NOT_A_DIRECTORY);
        rc = rtFsIsoMakerObjSetName(pThis, pNamespace, pObj, pParent, pszEntry, cchEntry, NULL);
    }
    return rc;
}


/**
 * Removes an object from the given namespace.
 *
 * @returns IPRT status code.
 * @param   pThis       The ISO maker instance.
 * @param   pNamespace  The namespace.
 * @param   pObj        The object to name.
 */
static int rtFsIsoMakerObjUnsetName(PRTFSISOMAKERINT pThis, PRTFSISOMAKERNAMESPACE pNamespace, PRTFSISOMAKEROBJ pObj)
{
    LogFlow(("rtFsIsoMakerObjUnsetName: idxObj=#%#x\n", pObj->idxObj));

    /*
     * First check if there is anything to do here at all.
     */
    PPRTFSISOMAKERNAME ppName = rtFsIsoMakerObjGetNameForNamespace(pObj, pNamespace);
    PRTFSISOMAKERNAME  pName = *ppName;
    if (!pName)
        return VINF_SUCCESS;

    /*
     * We don't support this on the root.
     */
    AssertReturn(pName->pParent, VERR_ACCESS_DENIED);

    /*
     * If this is a directory, we're in for some real fun here as we need to
     * unset the names of all the children too.
     */
    PRTFSISOMAKERNAMEDIR pDir = pName->pDir;
    if (pDir)
    {
        uint32_t iChild = pDir->cChildren;
        while (iChild-- > 0)
        {
            int rc = rtFsIsoMakerObjUnsetName(pThis, pNamespace, pDir->papChildren[iChild]->pObj);
            if (RT_FAILURE(rc))
                return rc;
        }
        AssertReturn(pDir->cChildren == 0, VERR_DIR_NOT_EMPTY);
    }

    /*
     * Unlink the pName from the parent.
     */
    pDir = pName->pParent->pDir;
    uint32_t iChild = pDir->cChildren;
    while (iChild-- > 0)
        if (pDir->papChildren[iChild] == pName)
        {
            uint32_t cToMove = pDir->cChildren - iChild - 1;
            if (cToMove > 0)
                memmove(&pDir->papChildren[iChild], &pDir->papChildren[iChild + 1], cToMove * sizeof(pDir->papChildren[0]));
            pDir->cChildren--;
            pNamespace->cNames--;

            /*
             * NULL the name member in the object and free the structure.
             */
            *ppName = NULL;
            RTMemFree(pName);

            return VINF_SUCCESS;
        }

    /* Not found. This can't happen. */
    AssertFailed();
    return VERR_INTERNAL_ERROR_2;
}






/*
 *
 * Object level config
 * Object level config
 * Object level config
 *
 */


/**
 * Translates an object index number to an object pointer, slow path.
 *
 * @returns Pointer to object, NULL if not found.
 * @param   pThis               The ISO maker instance.
 * @param   idxObj              The object index too resolve.
 */
DECL_NO_INLINE(static, PRTFSISOMAKEROBJ) rtFsIsoMakerIndexToObjSlow(PRTFSISOMAKERINT pThis, uint32_t idxObj)
{
    PRTFSISOMAKEROBJ pObj;
    RTListForEachReverse(&pThis->ObjectHead, pObj, RTFSISOMAKEROBJ, Entry)
    {
        if (pObj->idxObj == idxObj)
            return pObj;
    }
    return NULL;
}


/**
 * Translates an object index number to an object pointer.
 *
 * @returns Pointer to object, NULL if not found.
 * @param   pThis               The ISO maker instance.
 * @param   idxObj              The object index too resolve.
 */
DECLINLINE(PRTFSISOMAKEROBJ) rtFsIsoMakerIndexToObj(PRTFSISOMAKERINT pThis, uint32_t idxObj)
{
    PRTFSISOMAKEROBJ pObj = RTListGetLast(&pThis->ObjectHead, RTFSISOMAKEROBJ, Entry);
    if (!pObj || RT_LIKELY(pObj->idxObj == idxObj))
        return pObj;
    return rtFsIsoMakerIndexToObjSlow(pThis, idxObj);
}


/**
 * Resolves a path into a object ID.
 *
 * This will be doing the looking up using the specified object names rather
 * than the version adjusted and mangled according to the namespace setup.
 *
 * @returns The object ID corresponding to @a pszPath, or UINT32_MAX if not
 *          found or invalid parameters.
 * @param   hIsoMaker           The ISO maker instance.
 * @param   fNamespaces         The namespace to resolve @a pszPath in.  It's
 *                              possible to specify multiple namespaces here, of
 *                              course, but that's inefficient.
 * @param   pszPath             The path to the object.
 */
RTDECL(uint32_t) RTFsIsoMakerGetObjIdxForPath(RTFSISOMAKER hIsoMaker, uint32_t fNamespaces, const char *pszPath)
{
    /*
     * Validate input.
     */
    PRTFSISOMAKERINT pThis = hIsoMaker;
    RTFSISOMAKER_ASSERT_VALID_HANDLE_RET_EX(pThis, UINT32_MAX);

    /*
     * Do the searching.
     */
    for (uint32_t i = 0; i < RT_ELEMENTS(g_aRTFsIsoNamespaces); i++)
        if (fNamespaces & g_aRTFsIsoNamespaces[i].fNamespace)
        {
            PRTFSISOMAKERNAMESPACE pNamespace = (PRTFSISOMAKERNAMESPACE)((uintptr_t)pThis + g_aRTFsIsoNamespaces[i].offNamespace);
            if (pNamespace->pRoot)
            {
                PRTFSISOMAKERNAME pName;
                int rc = rtFsIsoMakerWalkPathBySpec(pNamespace, pszPath, &pName);
                if (RT_SUCCESS(rc))
                    return pName->pObj->idxObj;
            }
        }

    return UINT32_MAX;
}


/**
 * Removes the specified object from the image.
 *
 * This is a worker for RTFsIsoMakerObjRemove and
 * rtFsIsoMakerFinalizeRemoveOrphans.
 *
 * @returns IPRT status code.
 * @param   hIsoMaker           The ISO maker instance.
 * @param   pObj                The object to remove from the image.
 */
static int rtFsIsoMakerObjRemoveWorker(PRTFSISOMAKERINT pThis, PRTFSISOMAKEROBJ pObj)
{
    /*
     * Don't allow removing trans.tbl files and the boot catalog.
     */
    if (pObj->enmType == RTFSISOMAKEROBJTYPE_FILE)
    {
        PRTFSISOMAKERFILE pFile = (PRTFSISOMAKERFILE)pObj;
        AssertReturn(pFile->enmSrcType != RTFSISOMAKERSRCTYPE_TRANS_TBL, VERR_ACCESS_DENIED);
        AssertReturn(pFile != pThis->pBootCatFile, VERR_ACCESS_DENIED);
    }

    /*
     * Remove the object from all name spaces.
     */
    int rc = VINF_SUCCESS;
    for (uint32_t i = 0; i < RT_ELEMENTS(g_aRTFsIsoNamespaces); i++)
    {
        PRTFSISOMAKERNAMESPACE pNamespace = (PRTFSISOMAKERNAMESPACE)((uintptr_t)pThis + g_aRTFsIsoNamespaces[i].offNamespace);
        int rc2 = rtFsIsoMakerObjUnsetName(pThis, pNamespace, pObj);
        if (RT_SUCCESS(rc2) || RT_FAILURE(rc))
            continue;
        rc = rc2;
    }

    /*
     * If that succeeded, remove the object itself.
     */
    if (RT_SUCCESS(rc))
    {
        RTListNodeRemove(&pObj->Entry);
        if (pObj->enmType == RTFSISOMAKEROBJTYPE_FILE)
        {
            uint64_t cbData = ((PRTFSISOMAKERFILE)pObj)->cbData;
            pThis->cbData -= RT_ALIGN_64(cbData, RTFSISOMAKER_SECTOR_SIZE);
        }
        pThis->cObjects--;
        rtFsIsoMakerObjDestroy(pObj);
    }
    return rc;
}


/**
 * Removes the specified object from the image.
 *
 * @returns IPRT status code.
 * @param   hIsoMaker           The ISO maker instance.
 * @param   idxObj              The index of the object to remove.
 */
RTDECL(int) RTFsIsoMakerObjRemove(RTFSISOMAKER hIsoMaker, uint32_t idxObj)
{
    /*
     * Validate and translate input.
     */
    PRTFSISOMAKERINT pThis = hIsoMaker;
    RTFSISOMAKER_ASSERT_VALID_HANDLE_RET(pThis);
    PRTFSISOMAKEROBJ pObj = rtFsIsoMakerIndexToObj(pThis, idxObj);
    AssertReturn(pObj, VERR_OUT_OF_RANGE);
    AssertReturn(!pThis->fFinalized, VERR_WRONG_ORDER);

    /*
     * Call worker.
     */
    return rtFsIsoMakerObjRemoveWorker(pThis, pObj);
}


/**
 * Sets the path (name) of an object in the selected namespaces.
 *
 * The name will be transformed as necessary.
 *
 * The initial implementation does not allow this function to be called more
 * than once on an object.
 *
 * @returns IPRT status code.
 * @param   hIsoMaker           The ISO maker handle.
 * @param   idxObj              The configuration index of to name.
 * @param   fNamespaces         The namespaces to apply the path to
 *                              (RTFSISOMAKER_NAMESPACE_XXX).
 * @param   pszPath             The path.
 */
RTDECL(int) RTFsIsoMakerObjSetPath(RTFSISOMAKER hIsoMaker, uint32_t idxObj, uint32_t fNamespaces, const char *pszPath)
{
    /*
     * Validate and translate input.
     */
    PRTFSISOMAKERINT pThis = hIsoMaker;
    RTFSISOMAKER_ASSERT_VALID_HANDLE_RET(pThis);
    AssertReturn(!(fNamespaces & ~RTFSISOMAKER_NAMESPACE_VALID_MASK), VERR_INVALID_FLAGS);
    AssertPtrReturn(pszPath, VERR_INVALID_POINTER);
    AssertReturn(RTPATH_IS_SLASH(*pszPath), VERR_INVALID_NAME);
    PRTFSISOMAKEROBJ pObj = rtFsIsoMakerIndexToObj(pThis, idxObj);
    AssertReturn(pObj, VERR_OUT_OF_RANGE);
    AssertReturn(!pThis->fFinalized, VERR_WRONG_ORDER);

    /*
     * Execute requested actions.
     */
    int rc = VINF_SUCCESS;
    for (uint32_t i = 0; i < RT_ELEMENTS(g_aRTFsIsoNamespaces); i++)
        if (fNamespaces & g_aRTFsIsoNamespaces[i].fNamespace)
        {
            PRTFSISOMAKERNAMESPACE pNamespace = (PRTFSISOMAKERNAMESPACE)((uintptr_t)pThis + g_aRTFsIsoNamespaces[i].offNamespace);
            if (pNamespace->uLevel > 0)
            {
                int rc2 = rtFsIsoMakerObjSetPathInOne(pThis, pNamespace, pObj, pszPath);
                if (RT_SUCCESS(rc2) || RT_FAILURE(rc))
                    continue;
                rc = rc2;
            }
        }
    return rc;
}


/**
 * Sets the name of an object in the selected namespaces, placing it under the
 * given directory.
 *
 * The name will be transformed as necessary.
 *
 * @returns IPRT status code.
 * @param   hIsoMaker           The ISO maker handle.
 * @param   idxObj              The configuration index of to name.
 * @param   idxParentObj        The parent directory object.
 * @param   fNamespaces         The namespaces to apply the path to
 *                              (RTFSISOMAKER_NAMESPACE_XXX).
 * @param   pszName             The name.
 */
RTDECL(int) RTFsIsoMakerObjSetNameAndParent(RTFSISOMAKER hIsoMaker, uint32_t idxObj, uint32_t idxParentObj,
                                            uint32_t fNamespaces, const char *pszName)
{
    /*
     * Validate and translate input.
     */
    PRTFSISOMAKERINT pThis = hIsoMaker;
    RTFSISOMAKER_ASSERT_VALID_HANDLE_RET(pThis);
    AssertReturn(!(fNamespaces & ~RTFSISOMAKER_NAMESPACE_VALID_MASK), VERR_INVALID_FLAGS);
    AssertPtrReturn(pszName, VERR_INVALID_POINTER);
    size_t cchName = strlen(pszName);
    AssertReturn(cchName > 0, VERR_INVALID_NAME);
    AssertReturn(memchr(pszName, '/', cchName) == NULL, VERR_INVALID_NAME);
    PRTFSISOMAKEROBJ pObj = rtFsIsoMakerIndexToObj(pThis, idxObj);
    AssertReturn(pObj, VERR_OUT_OF_RANGE);
    PRTFSISOMAKEROBJ pParentObj = rtFsIsoMakerIndexToObj(pThis, idxParentObj);
    AssertReturn(pParentObj, VERR_OUT_OF_RANGE);
    AssertReturn(!pThis->fFinalized, VERR_WRONG_ORDER);

    /*
     * Execute requested actions.
     */
    int rc = VINF_SUCCESS;
    for (uint32_t i = 0; i < RT_ELEMENTS(g_aRTFsIsoNamespaces); i++)
        if (fNamespaces & g_aRTFsIsoNamespaces[i].fNamespace)
        {
            PRTFSISOMAKERNAMESPACE pNamespace = (PRTFSISOMAKERNAMESPACE)((uintptr_t)pThis + g_aRTFsIsoNamespaces[i].offNamespace);
            if (pNamespace->uLevel > 0)
            {
                PRTFSISOMAKERNAME pParentName = *rtFsIsoMakerObjGetNameForNamespace(pParentObj, pNamespace);
                if (pParentName)
                {
                    int rc2 = rtFsIsoMakerObjSetName(pThis, pNamespace, pObj, pParentName, pszName, cchName, NULL /*ppNewName*/);
                    if (RT_SUCCESS(rc2) || RT_FAILURE(rc))
                        continue;
                    rc = rc2;
                }
            }
        }
    return rc;
}


/**
 * Enables or disable syslinux boot info table patching of a file.
 *
 * @returns IPRT status code.
 * @param   hIsoMaker           The ISO maker handle.
 * @param   idxObj              The configuration index.
 * @param   fEnable             Whether to enable or disable patching.
 */
RTDECL(int) RTFsIsoMakerObjEnableBootInfoTablePatching(RTFSISOMAKER hIsoMaker, uint32_t idxObj, bool fEnable)
{
    /*
     * Validate and translate input.
     */
    PRTFSISOMAKERINT pThis = hIsoMaker;
    RTFSISOMAKER_ASSERT_VALID_HANDLE_RET(pThis);
    AssertReturn(!pThis->fFinalized, VERR_WRONG_ORDER);
    PRTFSISOMAKEROBJ pObj = rtFsIsoMakerIndexToObj(pThis, idxObj);
    AssertReturn(pObj, VERR_OUT_OF_RANGE);
    AssertReturn(pObj->enmType == RTFSISOMAKEROBJTYPE_FILE, VERR_WRONG_TYPE);
    PRTFSISOMAKERFILE pFile = (PRTFSISOMAKERFILE)pObj;
    AssertReturn(   pFile->enmSrcType == RTFSISOMAKERSRCTYPE_PATH
                 || pFile->enmSrcType == RTFSISOMAKERSRCTYPE_VFS_FILE
                 || pFile->enmSrcType == RTFSISOMAKERSRCTYPE_COMMON,
                 VERR_WRONG_TYPE);

    /*
     * Do the job.
     */
    if (fEnable)
    {
        if (!pFile->pBootInfoTable)
        {
            pFile->pBootInfoTable = (PISO9660SYSLINUXINFOTABLE)RTMemAllocZ(sizeof(*pFile->pBootInfoTable));
            AssertReturn(pFile->pBootInfoTable, VERR_NO_MEMORY);
        }
    }
    else if (pFile->pBootInfoTable)
    {
        RTMemFree(pFile->pBootInfoTable);
        pFile->pBootInfoTable = NULL;
    }
    return VINF_SUCCESS;
}


/**
 * Gets the data size of an object.
 *
 * Currently only supported on file objects.
 *
 * @returns IPRT status code.
 * @param   hIsoMaker           The ISO maker handle.
 * @param   idxObj              The configuration index.
 * @param   pcbData             Where to return the size.
 */
RTDECL(int) RTFsIsoMakerObjQueryDataSize(RTFSISOMAKER hIsoMaker, uint32_t idxObj, uint64_t *pcbData)
{
    /*
     * Validate and translate input.
     */
    AssertPtrReturn(pcbData, VERR_INVALID_POINTER);
    *pcbData = UINT64_MAX;
    PRTFSISOMAKERINT pThis = hIsoMaker;
    RTFSISOMAKER_ASSERT_VALID_HANDLE_RET(pThis);
    PRTFSISOMAKEROBJ pObj = rtFsIsoMakerIndexToObj(pThis, idxObj);
    AssertReturn(pObj, VERR_OUT_OF_RANGE);

    /*
     * Do the job.
     */
    if (pObj->enmType == RTFSISOMAKEROBJTYPE_FILE)
    {
        PRTFSISOMAKERFILE pFile = (PRTFSISOMAKERFILE)pObj;
        if (pFile->enmSrcType != RTFSISOMAKERSRCTYPE_TRANS_TBL)
        {
            *pcbData = ((PRTFSISOMAKERFILE)pObj)->cbData;
            return VINF_SUCCESS;
        }
    }
    return VERR_WRONG_TYPE;
}


/**
 * Initalizes the common part of a file system object and links it into global
 * chain.
 *
 * @returns IPRT status code
 * @param   pThis               The ISO maker instance.
 * @param   pObj                The common object.
 * @param   enmType             The object type.
 * @param   pObjInfo            The object information (typically source).
 *                              Optional.
 */
static int rtFsIsoMakerInitCommonObj(PRTFSISOMAKERINT pThis, PRTFSISOMAKEROBJ pObj,
                                     RTFSISOMAKEROBJTYPE enmType, PCRTFSOBJINFO pObjInfo)
{
    Assert(!pThis->fFinalized);
    AssertReturn(pThis->cObjects < RTFSISOMAKER_MAX_OBJECTS, VERR_OUT_OF_RANGE);

    pObj->enmType       = enmType;
    pObj->pPrimaryName  = NULL;
    pObj->pJolietName   = NULL;
    pObj->pUdfName      = NULL;
    pObj->pHfsName      = NULL;
    pObj->idxObj        = pThis->cObjects++;
    pObj->cNotOrphan    = 0;
    if (pObjInfo)
    {
        pObj->BirthTime         = pObjInfo->BirthTime;
        pObj->ChangeTime        = pObjInfo->ChangeTime;
        pObj->ModificationTime  = pObjInfo->ModificationTime;
        pObj->AccessedTime      = pObjInfo->AccessTime;
        pObj->fMode             = pObjInfo->Attr.fMode;
        pObj->uid               = pObjInfo->Attr.u.Unix.uid != NIL_RTUID ? pObjInfo->Attr.u.Unix.uid : pThis->uidDefault;
        pObj->gid               = pObjInfo->Attr.u.Unix.gid != NIL_RTGID ? pObjInfo->Attr.u.Unix.gid : pThis->gidDefault;
    }
    else
    {
        pObj->BirthTime         = pThis->ImageCreationTime;
        pObj->ChangeTime        = pThis->ImageCreationTime;
        pObj->ModificationTime  = pThis->ImageCreationTime;
        pObj->AccessedTime      = pThis->ImageCreationTime;
        pObj->fMode             = enmType == RTFSISOMAKEROBJTYPE_DIR ? pThis->fDefaultDirMode : pThis->fDefaultFileMode;
        pObj->uid               = pThis->uidDefault;
        pObj->gid               = pThis->gidDefault;
    }

    RTListAppend(&pThis->ObjectHead, &pObj->Entry);
    return VINF_SUCCESS;
}


/**
 * Internal function for adding an unnamed directory.
 *
 * @returns IPRT status code.
 * @param   pThis               The ISO make instance.
 * @param   ppDir               Where to return the directory.
 */
static int rtFsIsoMakerAddUnnamedDirWorker(PRTFSISOMAKERINT pThis, PRTFSISOMAKERDIR *ppDir)
{
    PRTFSISOMAKERDIR pDir = (PRTFSISOMAKERDIR)RTMemAllocZ(sizeof(*pDir));
    AssertReturn(pDir, VERR_NO_MEMORY);
    int rc = rtFsIsoMakerInitCommonObj(pThis, &pDir->Core, RTFSISOMAKEROBJTYPE_DIR, NULL);
    if (RT_SUCCESS(rc))
    {
        *ppDir = pDir;
        return VINF_SUCCESS;
    }
    RTMemFree(pDir);
    return rc;

}


/**
 * Adds an unnamed directory to the image.
 *
 * The directory must explictly be entered into the desired namespaces.
 *
 * @returns IPRT status code
 * @param   hIsoMaker           The ISO maker handle.
 * @param   pidxObj             Where to return the configuration index of the
 *                              directory.
 * @sa      RTFsIsoMakerAddDir, RTFsIsoMakerObjSetPath
 */
RTDECL(int) RTFsIsoMakerAddUnnamedDir(RTFSISOMAKER hIsoMaker, uint32_t *pidxObj)
{
    PRTFSISOMAKERINT pThis = hIsoMaker;
    RTFSISOMAKER_ASSERT_VALID_HANDLE_RET(pThis);
    AssertPtrReturn(pidxObj, VERR_INVALID_POINTER);
    AssertReturn(!pThis->fFinalized, VERR_WRONG_ORDER);

    PRTFSISOMAKERDIR pDir;
    int rc = rtFsIsoMakerAddUnnamedDirWorker(pThis, &pDir);
    *pidxObj = RT_SUCCESS(rc) ? pDir->Core.idxObj : UINT32_MAX;
    return rc;
}


/**
 * Adds a directory to the image in all namespaces and default attributes.
 *
 * @returns IPRT status code
 * @param   hIsoMaker           The ISO maker handle.
 * @param   pszDir              The path (UTF-8) to the directory in the ISO.
 *
 * @param   pidxObj             Where to return the configuration index of the
 *                              directory.  Optional.
 * @sa      RTFsIsoMakerAddUnnamedDir, RTFsIsoMakerObjSetPath
 */
RTDECL(int) RTFsIsoMakerAddDir(RTFSISOMAKER hIsoMaker, const char *pszDir, uint32_t *pidxObj)
{
    PRTFSISOMAKERINT pThis = hIsoMaker;
    RTFSISOMAKER_ASSERT_VALID_HANDLE_RET(pThis);
    AssertPtrReturn(pszDir, VERR_INVALID_POINTER);
    AssertReturn(RTPATH_IS_SLASH(*pszDir), VERR_INVALID_NAME);

    uint32_t idxObj;
    int rc = RTFsIsoMakerAddUnnamedDir(hIsoMaker, &idxObj);
    if (RT_SUCCESS(rc))
    {
        rc = RTFsIsoMakerObjSetPath(hIsoMaker, idxObj, RTFSISOMAKER_NAMESPACE_ALL, pszDir);
        if (RT_SUCCESS(rc))
        {
            if (pidxObj)
                *pidxObj = idxObj;
        }
        else
            RTFsIsoMakerObjRemove(hIsoMaker, idxObj);
    }
    return rc;
}


/**
 * Internal function for adding an unnamed file.
 *
 * @returns IPRT status code.
 * @param   pThis               The ISO make instance.
 * @param   pObjInfo            Object information.  Optional.
 * @param   cbExtra             Extra space for additional data (e.g. source
 *                              path string copy).
 * @param   ppFile              Where to return the file.
 */
static int rtFsIsoMakerAddUnnamedFileWorker(PRTFSISOMAKERINT pThis, PCRTFSOBJINFO pObjInfo, size_t cbExtra,
                                            PRTFSISOMAKERFILE *ppFile)
{
    PRTFSISOMAKERFILE pFile = (PRTFSISOMAKERFILE)RTMemAllocZ(sizeof(*pFile) + cbExtra);
    AssertReturn(pFile, VERR_NO_MEMORY);
    int rc = rtFsIsoMakerInitCommonObj(pThis, &pFile->Core, RTFSISOMAKEROBJTYPE_FILE, pObjInfo);
    if (RT_SUCCESS(rc))
    {
        pFile->cbData       = pObjInfo ? pObjInfo->cbObject : 0;
        pThis->cbData += RT_ALIGN_64(pFile->cbData, RTFSISOMAKER_SECTOR_SIZE);
        pFile->offData      = UINT64_MAX;
        pFile->enmSrcType   = RTFSISOMAKERSRCTYPE_INVALID;
        pFile->u.pszSrcPath = NULL;
        pFile->pBootInfoTable = NULL;
        RTListInit(&pFile->FinalizedEntry);

        *ppFile = pFile;
        return VINF_SUCCESS;
    }
    RTMemFree(pFile);
    return rc;

}


/**
 * Adds an unnamed file to the image that's backed by a host file.
 *
 * The file must explictly be entered into the desired namespaces.
 *
 * @returns IPRT status code
 * @param   hIsoMaker           The ISO maker handle.
 * @param   pszSrcFile          The source file path.  VFS chain spec allowed.
 * @param   pidxObj             Where to return the configuration index of the
 *                              directory.
 * @sa      RTFsIsoMakerAddFile, RTFsIsoMakerObjSetPath
 */
RTDECL(int) RTFsIsoMakerAddUnnamedFileWithSrcPath(RTFSISOMAKER hIsoMaker, const char *pszSrcFile, uint32_t *pidxObj)
{
    PRTFSISOMAKERINT pThis = hIsoMaker;
    RTFSISOMAKER_ASSERT_VALID_HANDLE_RET(pThis);
    AssertPtrReturn(pidxObj, VERR_INVALID_POINTER);
    *pidxObj = UINT32_MAX;
    AssertReturn(!pThis->fFinalized, VERR_WRONG_ORDER);

    /*
     * Check that the source file exists and is a file.
     */
    uint32_t    offError = 0;
    RTFSOBJINFO ObjInfo;
    int rc = RTVfsChainQueryInfo(pszSrcFile, &ObjInfo, RTFSOBJATTRADD_UNIX, RTPATH_F_FOLLOW_LINK, &offError, NULL);
    AssertMsgRCReturn(rc, ("%s -> %Rrc offError=%u\n", pszSrcFile, rc, offError), rc);
    AssertMsgReturn(RTFS_IS_FILE(ObjInfo.Attr.fMode), ("%#x - %s\n", ObjInfo.Attr.fMode, pszSrcFile), VERR_NOT_A_FILE);

    /*
     * Create a file object for it.
     */
    size_t const      cbSrcFile = strlen(pszSrcFile) + 1;
    PRTFSISOMAKERFILE pFile;
    rc = rtFsIsoMakerAddUnnamedFileWorker(pThis, &ObjInfo, cbSrcFile, &pFile);
    if (RT_SUCCESS(rc))
    {
        pFile->enmSrcType   = RTFSISOMAKERSRCTYPE_PATH;
        pFile->u.pszSrcPath = (char *)memcpy(pFile + 1, pszSrcFile, cbSrcFile);

        *pidxObj = pFile->Core.idxObj;
    }
    return rc;
}


/**
 * Adds an unnamed file to the image that's backed by a VFS file.
 *
 * The file must explictly be entered into the desired namespaces.
 *
 * @returns IPRT status code
 * @param   hIsoMaker           The ISO maker handle.
 * @param   hVfsFileSrc         The source file handle.
 * @param   pidxObj             Where to return the configuration index of the
 *                              directory.
 * @sa      RTFsIsoMakerAddUnnamedFileWithSrcPath, RTFsIsoMakerObjSetPath
 */
RTDECL(int) RTFsIsoMakerAddUnnamedFileWithVfsFile(RTFSISOMAKER hIsoMaker, RTVFSFILE hVfsFileSrc, uint32_t *pidxObj)
{
    PRTFSISOMAKERINT pThis = hIsoMaker;
    RTFSISOMAKER_ASSERT_VALID_HANDLE_RET(pThis);
    AssertPtrReturn(pidxObj, VERR_INVALID_POINTER);
    *pidxObj = UINT32_MAX;
    AssertReturn(!pThis->fFinalized, VERR_WRONG_ORDER);

    /*
     * Get the VFS file info.  This implicitly validates the handle.
     */
    RTFSOBJINFO ObjInfo;
    int rc = RTVfsFileQueryInfo(hVfsFileSrc, &ObjInfo, RTFSOBJATTRADD_UNIX);
    AssertMsgRCReturn(rc, ("RTVfsFileQueryInfo(%p) -> %Rrc\n", hVfsFileSrc, rc), rc);

    /*
     * Retain a reference to the file.
     */
    uint32_t cRefs = RTVfsFileRetain(hVfsFileSrc);
    AssertReturn(cRefs != UINT32_MAX, VERR_INVALID_HANDLE);

    /*
     * Create a file object for it.
     */
    PRTFSISOMAKERFILE pFile;
    rc = rtFsIsoMakerAddUnnamedFileWorker(pThis, &ObjInfo, 0, &pFile);
    if (RT_SUCCESS(rc))
    {
        pFile->enmSrcType   = RTFSISOMAKERSRCTYPE_VFS_FILE;
        pFile->u.hVfsFile   = hVfsFileSrc;

        *pidxObj = pFile->Core.idxObj;
    }
    else
        RTVfsFileRelease(hVfsFileSrc);
    return rc;
}


/**
 * Adds an unnamed file to the image that's backed by a portion of a common
 * source file.
 *
 * The file must explictly be entered into the desired namespaces.
 *
 * @returns IPRT status code
 * @param   hIsoMaker           The ISO maker handle.
 * @param   idxCommonSrc        The common source file index.
 * @param   offData             The offset of the data in the source file.
 * @param   cbData              The file size.
 * @param   pObjInfo            Pointer to file info.  Optional.
 * @param   pidxObj             Where to return the configuration index of the
 *                              directory.
 * @sa      RTFsIsoMakerAddUnnamedFileWithSrcPath, RTFsIsoMakerObjSetPath
 */
RTDECL(int) RTFsIsoMakerAddUnnamedFileWithCommonSrc(RTFSISOMAKER hIsoMaker, uint32_t idxCommonSrc,
                                                    uint64_t offData, uint64_t cbData, PCRTFSOBJINFO pObjInfo, uint32_t *pidxObj)
{
    /*
     * Validate and fake input.
     */
    PRTFSISOMAKERINT pThis = hIsoMaker;
    RTFSISOMAKER_ASSERT_VALID_HANDLE_RET(pThis);
    AssertPtrReturn(pidxObj, VERR_INVALID_POINTER);
    *pidxObj = UINT32_MAX;
    AssertReturn(!pThis->fFinalized, VERR_WRONG_ORDER);
    AssertReturn(idxCommonSrc < pThis->cCommonSources, VERR_INVALID_PARAMETER);
    AssertReturn(offData < (uint64_t)RTFOFF_MAX, VERR_OUT_OF_RANGE);
    AssertReturn(cbData < (uint64_t)RTFOFF_MAX, VERR_OUT_OF_RANGE);
    AssertReturn(offData + cbData < (uint64_t)RTFOFF_MAX, VERR_OUT_OF_RANGE);
    RTFSOBJINFO ObjInfo;
    if (!pObjInfo)
    {
        ObjInfo.cbObject            = cbData;
        ObjInfo.cbAllocated         = cbData;
        ObjInfo.BirthTime           = pThis->ImageCreationTime;
        ObjInfo.ChangeTime          = pThis->ImageCreationTime;
        ObjInfo.ModificationTime    = pThis->ImageCreationTime;
        ObjInfo.AccessTime          = pThis->ImageCreationTime;
        ObjInfo.Attr.fMode          = pThis->fDefaultFileMode;
        ObjInfo.Attr.enmAdditional  = RTFSOBJATTRADD_UNIX;
        ObjInfo.Attr.u.Unix.uid             = NIL_RTUID;
        ObjInfo.Attr.u.Unix.gid             = NIL_RTGID;
        ObjInfo.Attr.u.Unix.cHardlinks      = 1;
        ObjInfo.Attr.u.Unix.INodeIdDevice   = 0;
        ObjInfo.Attr.u.Unix.INodeId         = 0;
        ObjInfo.Attr.u.Unix.fFlags          = 0;
        ObjInfo.Attr.u.Unix.GenerationId    = 0;
        ObjInfo.Attr.u.Unix.Device          = 0;
        pObjInfo = &ObjInfo;
    }
    else
    {
        AssertPtrReturn(pObjInfo, VERR_INVALID_POINTER);
        AssertReturn(pObjInfo->Attr.enmAdditional == RTFSOBJATTRADD_UNIX, VERR_WRONG_TYPE);
        AssertReturn((uint64_t)pObjInfo->cbObject == cbData, VERR_INVALID_PARAMETER);
    }

    /*
     * Create a file object for it.
     */
    PRTFSISOMAKERFILE pFile;
    int rc = rtFsIsoMakerAddUnnamedFileWorker(pThis, pObjInfo, 0, &pFile);
    if (RT_SUCCESS(rc))
    {
        pFile->enmSrcType       = RTFSISOMAKERSRCTYPE_COMMON;
        pFile->u.Common.idxSrc  = idxCommonSrc;
        pFile->u.Common.offData = offData;

        *pidxObj = pFile->Core.idxObj;
    }
    return rc;
}


/**
 * Adds a common source file.
 *
 * Using RTFsIsoMakerAddUnnamedFileWithCommonSrc a sections common source file
 * can be referenced to make up other files.  The typical use case is when
 * importing data from an existing ISO.
 *
 * @returns IPRT status code
 * @param   hIsoMaker           The ISO maker handle.
 * @param   hVfsFile            VFS handle of the common source.  (A reference
 *                              is added, none consumed.)
 * @param   pidxCommonSrc       Where to return the assigned common source
 *                              index.  This is used to reference the file.
 * @sa      RTFsIsoMakerAddUnnamedFileWithCommonSrc
 */
RTDECL(int) RTFsIsoMakerAddCommonSourceFile(RTFSISOMAKER hIsoMaker, RTVFSFILE hVfsFile, uint32_t *pidxCommonSrc)
{
    /*
     * Validate input.
     */
    PRTFSISOMAKERINT pThis = hIsoMaker;
    RTFSISOMAKER_ASSERT_VALID_HANDLE_RET(pThis);
    AssertPtrReturn(pidxCommonSrc, VERR_INVALID_POINTER);
    *pidxCommonSrc = UINT32_MAX;
    AssertReturn(!pThis->fFinalized, VERR_WRONG_ORDER);

    /*
     * Resize the common source array if necessary.
     */
    if ((pThis->cCommonSources & 15) == 0)
    {
        void *pvNew = RTMemRealloc(pThis->paCommonSources, (pThis->cCommonSources + 16) * sizeof(pThis->paCommonSources[0]));
        AssertReturn(pvNew, VERR_NO_MEMORY);
        pThis->paCommonSources = (PRTVFSFILE)pvNew;
    }

    /*
     * Retain a reference to the source file, thereby validating the handle.
     * Then add it to the array.
     */
    uint32_t cRefs = RTVfsFileRetain(hVfsFile);
    AssertReturn(cRefs != UINT32_MAX, VERR_INVALID_HANDLE);

    uint32_t idx = pThis->cCommonSources++;
    pThis->paCommonSources[idx] = hVfsFile;

    *pidxCommonSrc = idx;
    return VINF_SUCCESS;
}


/**
 * Adds a file that's backed by a host file to the image in all namespaces and
 * with attributes taken from the source file.
 *
 * @returns IPRT status code
 * @param   hIsoMaker       The ISO maker handle.
 * @param   pszFile         The path to the file in the image.
 * @param   pszSrcFile      The source file path.  VFS chain spec allowed.
 * @param   pidxObj         Where to return the configuration index of the file.
 *                          Optional
 * @sa      RTFsIsoMakerAddFileWithVfsFile,
 *          RTFsIsoMakerAddUnnamedFileWithSrcPath
 */
RTDECL(int) RTFsIsoMakerAddFileWithSrcPath(RTFSISOMAKER hIsoMaker, const char *pszFile, const char *pszSrcFile, uint32_t *pidxObj)
{
    PRTFSISOMAKERINT pThis = hIsoMaker;
    RTFSISOMAKER_ASSERT_VALID_HANDLE_RET(pThis);
    AssertPtrReturn(pszFile, VERR_INVALID_POINTER);
    AssertReturn(RTPATH_IS_SLASH(*pszFile), VERR_INVALID_NAME);

    uint32_t idxObj;
    int rc = RTFsIsoMakerAddUnnamedFileWithSrcPath(hIsoMaker, pszSrcFile, &idxObj);
    if (RT_SUCCESS(rc))
    {
        rc = RTFsIsoMakerObjSetPath(hIsoMaker, idxObj, RTFSISOMAKER_NAMESPACE_ALL, pszFile);
        if (RT_SUCCESS(rc))
        {
            if (pidxObj)
                *pidxObj = idxObj;
        }
        else
            RTFsIsoMakerObjRemove(hIsoMaker, idxObj);
    }
    return rc;
}


/**
 * Adds a file that's backed by a VFS file to the image in all namespaces and
 * with attributes taken from the source file.
 *
 * @returns IPRT status code
 * @param   hIsoMaker       The ISO maker handle.
 * @param   pszFile         The path to the file in the image.
 * @param   hVfsFileSrc     The source file handle.
 * @param   pidxObj         Where to return the configuration index of the file.
 *                          Optional.
 * @sa      RTFsIsoMakerAddUnnamedFileWithVfsFile,
 *          RTFsIsoMakerAddFileWithSrcPath
 */
RTDECL(int) RTFsIsoMakerAddFileWithVfsFile(RTFSISOMAKER hIsoMaker, const char *pszFile, RTVFSFILE hVfsFileSrc, uint32_t *pidxObj)
{
    PRTFSISOMAKERINT pThis = hIsoMaker;
    RTFSISOMAKER_ASSERT_VALID_HANDLE_RET(pThis);
    AssertPtrReturn(pszFile, VERR_INVALID_POINTER);
    AssertReturn(RTPATH_IS_SLASH(*pszFile), VERR_INVALID_NAME);

    uint32_t idxObj;
    int rc = RTFsIsoMakerAddUnnamedFileWithVfsFile(hIsoMaker, hVfsFileSrc, &idxObj);
    if (RT_SUCCESS(rc))
    {
        rc = RTFsIsoMakerObjSetPath(hIsoMaker, idxObj, RTFSISOMAKER_NAMESPACE_ALL, pszFile);
        if (RT_SUCCESS(rc))
        {
            if (pidxObj)
                *pidxObj = idxObj;
        }
        else
            RTFsIsoMakerObjRemove(hIsoMaker, idxObj);
    }
    return rc;
}




/*
 *
 * El Torito Booting.
 * El Torito Booting.
 * El Torito Booting.
 * El Torito Booting.
 *
 */

/**
 * Ensures that we've got a boot catalog file.
 *
 * @returns IPRT status code.
 * @param   pThis               The ISO maker instance.
 */
static int rtFsIsoMakerEnsureBootCatFile(PRTFSISOMAKERINT pThis)
{
    if (pThis->pBootCatFile)
        return VINF_SUCCESS;

    AssertReturn(!pThis->fFinalized, VERR_WRONG_ORDER);

    /* Create a VFS memory file for backing up the file. */
    RTVFSFILE hVfsFile;
    int rc = RTVfsMemFileCreate(NIL_RTVFSIOSTREAM, RTFSISOMAKER_SECTOR_SIZE, &hVfsFile);
    if (RT_SUCCESS(rc))
    {
        /* Create an unnamed VFS backed file and mark it as non-orphaned. */
        PRTFSISOMAKERFILE pFile;
        rc = rtFsIsoMakerAddUnnamedFileWorker(pThis, NULL, 0, &pFile);
        if (RT_SUCCESS(rc))
        {
            pFile->enmSrcType       = RTFSISOMAKERSRCTYPE_VFS_FILE;
            pFile->u.hVfsFile       = hVfsFile;
            pFile->Core.cNotOrphan  = 1;

            /* Save file pointer and allocate a volume descriptor. */
            pThis->pBootCatFile = pFile;
            pThis->cVolumeDescriptors++;

            return VINF_SUCCESS;
        }
        RTVfsFileRelease(hVfsFile);
    }
    return rc;
}


/**
 * Queries the configuration index of the boot catalog file object.
 *
 * The boot catalog file is created as necessary, thus this have to be a query
 * rather than a getter since object creation may fail.
 *
 * @returns IPRT status code.
 * @param   hIsoMaker           The ISO maker handle.
 * @param   pidxObj             Where to return the configuration index.
 */
RTDECL(int) RTFsIsoMakerQueryObjIdxForBootCatalog(RTFSISOMAKER hIsoMaker, uint32_t *pidxObj)
{
    /*
     * Validate input.
     */
    AssertPtrReturn(pidxObj, VERR_INVALID_POINTER);
    *pidxObj = UINT32_MAX;
    PRTFSISOMAKERINT pThis = hIsoMaker;
    RTFSISOMAKER_ASSERT_VALID_HANDLE_RET(pThis);

    /*
     * Do the job.
     */
    int rc = rtFsIsoMakerEnsureBootCatFile(pThis);
    if (RT_SUCCESS(rc))
        *pidxObj = pThis->pBootCatFile->Core.idxObj;
    return rc;
}


/**
 * Sets the boot catalog backing file.
 *
 * The content of the given file will be discarded and replaced with the boot
 * catalog, the naming and file attributes (other than size) will be retained.
 *
 * This API exists mainly to assist when importing ISOs.
 *
 * @returns IPRT status code.
 * @param   hIsoMaker           The ISO maker handle.
 * @param   idxObj              The configuration index of the file.
 */
RTDECL(int) RTFsIsoMakerBootCatSetFile(RTFSISOMAKER hIsoMaker, uint32_t idxObj)
{
    /*
     * Validate and translate input.
     */
    PRTFSISOMAKERINT pThis = hIsoMaker;
    RTFSISOMAKER_ASSERT_VALID_HANDLE_RET(pThis);

    PRTFSISOMAKEROBJ pObj = rtFsIsoMakerIndexToObj(pThis, idxObj);
    AssertReturn(pObj, VERR_OUT_OF_RANGE);
    AssertReturn(pObj->enmType == RTFSISOMAKEROBJTYPE_FILE, VERR_WRONG_TYPE);
    PRTFSISOMAKERFILE pFile = (PRTFSISOMAKERFILE)pObj;
    AssertReturn(   pFile->enmSrcType == RTFSISOMAKERSRCTYPE_PATH
                 || pFile->enmSrcType == RTFSISOMAKERSRCTYPE_COMMON
                 || pFile->enmSrcType == RTFSISOMAKERSRCTYPE_VFS_FILE,
                 VERR_WRONG_TYPE);

    /*
     * To reduce the possible combinations here, make sure there is a boot cat
     * file that we're "replacing".
     */
    int rc = rtFsIsoMakerEnsureBootCatFile(pThis);
    if (RT_SUCCESS(rc))
    {
        /*
         * Grab a reference to the boot cat memory VFS so we can destroy it
         * later using regular destructors.
         */
        PRTFSISOMAKERFILE pOldFile = pThis->pBootCatFile;
        RTVFSFILE         hVfsFile = pOldFile->u.hVfsFile;
        uint32_t          cRefs    = RTVfsFileRetain(hVfsFile);
        if (cRefs != UINT32_MAX)
        {
            /*
             * Try remove the existing boot file.
             */
            pOldFile->Core.cNotOrphan--;
            pThis->pBootCatFile = NULL;
            rc = rtFsIsoMakerObjRemoveWorker(pThis, &pOldFile->Core);
            if (RT_SUCCESS(rc))
            {
                /*
                 * Just morph pFile into a boot catalog file.
                 */
                if (pFile->enmSrcType == RTFSISOMAKERSRCTYPE_VFS_FILE)
                {
                    RTVfsFileRelease(pFile->u.hVfsFile);
                    pFile->u.hVfsFile = NIL_RTVFSFILE;
                }

                pThis->cbData -= RT_ALIGN_64(pFile->cbData, RTFSISOMAKER_SECTOR_SIZE);
                pFile->cbData     = 0;
                pFile->Core.cNotOrphan++;
                pFile->enmSrcType = RTFSISOMAKERSRCTYPE_VFS_FILE;
                pFile->u.hVfsFile = hVfsFile;

                pThis->pBootCatFile = pFile;

                return VINF_SUCCESS;
            }

            pThis->pBootCatFile = pOldFile;
            pOldFile->Core.cNotOrphan++;
            RTVfsFileRelease(hVfsFile);
        }
        else
            rc = VERR_INTERNAL_ERROR_2;
    }
    return rc;
}


/**
 * Set the validation entry of the boot catalog (this is the first entry).
 *
 * @returns IPRT status code.
 * @param   hIsoMaker           The ISO maker handle.
 * @param   idPlatform          The platform ID
 *                              (ISO9660_ELTORITO_PLATFORM_ID_XXX).
 * @param   pszString           CD/DVD-ROM identifier.  Optional.
 */
RTDECL(int) RTFsIsoMakerBootCatSetValidationEntry(RTFSISOMAKER hIsoMaker, uint8_t idPlatform, const char *pszString)
{
    /*
     * Validate input.
     */
    PRTFSISOMAKERINT pThis = hIsoMaker;
    RTFSISOMAKER_ASSERT_VALID_HANDLE_RET(pThis);
    size_t cchString = 0;
    if (pszString)
    {
        cchString = RTStrCalcLatin1Len(pszString);
        AssertReturn(cchString < RT_SIZEOFMEMB(ISO9660ELTORITOVALIDATIONENTRY, achId), VERR_OUT_OF_RANGE);
    }

    /*
     * Make sure we've got a boot file.
     */
    int rc = rtFsIsoMakerEnsureBootCatFile(pThis);
    if (RT_SUCCESS(rc))
    {
        /*
         * Construct the entry data.
         */
        ISO9660ELTORITOVALIDATIONENTRY Entry;
        Entry.bHeaderId = ISO9660_ELTORITO_HEADER_ID_VALIDATION_ENTRY;
        Entry.bPlatformId = idPlatform;
        Entry.u16Reserved = 0;
        RT_ZERO(Entry.achId);
        if (cchString)
        {
            char *pszTmp = Entry.achId;
            rc = RTStrToLatin1Ex(pszString, RTSTR_MAX, &pszTmp, sizeof(Entry.achId), NULL);
            AssertRC(rc);
        }
        Entry.u16Checksum = 0;
        Entry.bKey1 = ISO9660_ELTORITO_KEY_BYTE_1;
        Entry.bKey2 = ISO9660_ELTORITO_KEY_BYTE_2;

        /* Calc checksum. */
        uint16_t uSum = 0;
        uint16_t const *pu16Src = (uint16_t const *)&Entry;
        uint16_t        cLeft   = sizeof(Entry) / sizeof(uint16_t);
        while (cLeft-- > 0)
        {
            uSum += RT_LE2H_U16(*pu16Src);
            pu16Src++;
        }
        Entry.u16Checksum = RT_H2LE_U16((uint16_t)0 - uSum);

        /*
         * Write the entry and update our internal tracker.
         */
        rc = RTVfsFileWriteAt(pThis->pBootCatFile->u.hVfsFile, 0, &Entry, sizeof(Entry), NULL);
        if (RT_SUCCESS(rc))
        {
            pThis->aBootCatEntries[0].bType    = ISO9660_ELTORITO_HEADER_ID_VALIDATION_ENTRY;
            pThis->aBootCatEntries[0].cEntries = 2;
        }
    }
    return rc;
}


/**
 * Set the validation entry of the boot catalog (this is the first entry).
 *
 * @returns IPRT status code.
 * @param   hIsoMaker           The ISO maker handle.
 * @param   idxBootCat          The boot catalog entry.  Zero and two are
 *                              invalid.  Must be less than 63.
 * @param   idxImageObj         The configuration index of the boot image.
 * @param   bBootMediaType      The media type and flag (not for entry 1)
 *                              (ISO9660_ELTORITO_BOOT_MEDIA_TYPE_XXX,
 *                              ISO9660_ELTORITO_BOOT_MEDIA_F_XXX).
 * @param   bSystemType         The partitiona table system ID.
 * @param   fBootable           Whether it's a bootable entry or if we just want
 *                              the BIOS to setup the emulation without booting
 *                              it.
 * @param   uLoadSeg            The load address divided by 0x10 (i.e. the real
 *                              mode segment number).
 * @param   cSectorsToLoad      Number of emulated sectors to load.
 * @param   bSelCritType        The selection criteria type, if none pass
 *                              ISO9660_ELTORITO_SEL_CRIT_TYPE_NONE.
 * @param   pvSelCritData       Pointer to the selection criteria data.
 * @param   cbSelCritData       Size of the selection criteria data.
 */
RTDECL(int) RTFsIsoMakerBootCatSetSectionEntry(RTFSISOMAKER hIsoMaker, uint32_t idxBootCat, uint32_t idxImageObj,
                                               uint8_t bBootMediaType, uint8_t bSystemType, bool fBootable,
                                               uint16_t uLoadSeg, uint16_t cSectorsToLoad,
                                               uint8_t bSelCritType, void const *pvSelCritData, size_t cbSelCritData)
{
    /*
     * Validate input.
     */
    PRTFSISOMAKERINT pThis = hIsoMaker;
    RTFSISOMAKER_ASSERT_VALID_HANDLE_RET(pThis);
    PRTFSISOMAKERFILE pFile = (PRTFSISOMAKERFILE)rtFsIsoMakerIndexToObj(pThis, idxImageObj);
    AssertReturn(pFile, VERR_OUT_OF_RANGE);
    AssertReturn((bBootMediaType & ISO9660_ELTORITO_BOOT_MEDIA_TYPE_MASK) <= ISO9660_ELTORITO_BOOT_MEDIA_TYPE_HARD_DISK,
                 VERR_INVALID_PARAMETER);
    AssertReturn(!(bBootMediaType & ISO9660_ELTORITO_BOOT_MEDIA_F_MASK) || idxBootCat != 1,
                 VERR_INVALID_PARAMETER);

    AssertReturn(idxBootCat != 0 && idxBootCat != 2 && idxBootCat < RT_ELEMENTS(pThis->aBootCatEntries) - 1U, VERR_OUT_OF_RANGE);

    size_t cExtEntries = 0;
    if (bSelCritType == ISO9660_ELTORITO_SEL_CRIT_TYPE_NONE)
        AssertReturn(cbSelCritData == 0, VERR_INVALID_PARAMETER);
    else
    {
        AssertReturn(idxBootCat > 2, VERR_INVALID_PARAMETER);
        if (cbSelCritData > 0)
        {
            AssertPtrReturn(pvSelCritData, VERR_INVALID_POINTER);

            if (cbSelCritData <= RT_SIZEOFMEMB(ISO9660ELTORITOSECTIONENTRY, abSelectionCriteria))
                cExtEntries = 0;
            else
            {
                cExtEntries = (cbSelCritData - RT_SIZEOFMEMB(ISO9660ELTORITOSECTIONENTRY, abSelectionCriteria)
                               + RT_SIZEOFMEMB(ISO9660ELTORITOSECTIONENTRYEXT, abSelectionCriteria) - 1)
                            / RT_SIZEOFMEMB(ISO9660ELTORITOSECTIONENTRYEXT, abSelectionCriteria);
                AssertReturn(cExtEntries + 1 < RT_ELEMENTS(pThis->aBootCatEntries) - 1, VERR_TOO_MUCH_DATA);
            }
        }
    }

    /*
     * Make sure we've got a boot file.
     */
    int rc = rtFsIsoMakerEnsureBootCatFile(pThis);
    if (RT_SUCCESS(rc))
    {
        /*
         * Construct the entry.
         */
        union
        {
            ISO9660ELTORITOSECTIONENTRY     Entry;
            ISO9660ELTORITOSECTIONENTRYEXT  ExtEntry;
        } u;
        u.Entry.bBootIndicator            = fBootable ? ISO9660_ELTORITO_BOOT_INDICATOR_BOOTABLE
                                          :             ISO9660_ELTORITO_BOOT_INDICATOR_NOT_BOOTABLE;
        u.Entry.bBootMediaType            = bBootMediaType;
        u.Entry.uLoadSeg                  = RT_H2LE_U16(uLoadSeg);
        u.Entry.bSystemType               = cExtEntries == 0
                                          ? bSystemType & ~ISO9660_ELTORITO_BOOT_MEDIA_F_CONTINUATION
                                          : bSystemType | ISO9660_ELTORITO_BOOT_MEDIA_F_CONTINUATION;
        u.Entry.bUnused                   = 0;
        u.Entry.cEmulatedSectorsToLoad    = RT_H2LE_U16(cSectorsToLoad);
        u.Entry.offBootImage              = 0;
        u.Entry.bSelectionCriteriaType    = bSelCritType;
        RT_ZERO(u.Entry.abSelectionCriteria);
        if (cbSelCritData > 0)
            memcpy(u.Entry.abSelectionCriteria, pvSelCritData, RT_MIN(cbSelCritData, sizeof(u.Entry.abSelectionCriteria)));

        /*
         * Write it and update our internal tracker.
         */
        rc = RTVfsFileWriteAt(pThis->pBootCatFile->u.hVfsFile, ISO9660_ELTORITO_ENTRY_SIZE * idxBootCat,
                              &u.Entry, sizeof(u.Entry), NULL);
        if (RT_SUCCESS(rc))
        {
            if (pThis->aBootCatEntries[idxBootCat].pBootFile != pFile)
            {
                if (pThis->aBootCatEntries[idxBootCat].pBootFile)
                    pThis->aBootCatEntries[idxBootCat].pBootFile->Core.cNotOrphan--;
                pFile->Core.cNotOrphan++;
                pThis->aBootCatEntries[idxBootCat].pBootFile = pFile;
            }

            pThis->aBootCatEntries[idxBootCat].bType    = u.Entry.bBootIndicator;
            pThis->aBootCatEntries[idxBootCat].cEntries = 1;
        }

        /*
         * Do add further extension entries with selection criteria.
         */
        if (cExtEntries)
        {
            uint8_t const *pbSrc = (uint8_t const *)pvSelCritData;
            size_t         cbSrc = cbSelCritData;
            pbSrc += sizeof(u.Entry.abSelectionCriteria);
            cbSrc -= sizeof(u.Entry.abSelectionCriteria);

            while (cbSrc > 0)
            {
                u.ExtEntry.bExtensionId = ISO9660_ELTORITO_SECTION_ENTRY_EXT_ID;
                if (cbSrc > sizeof(u.ExtEntry.abSelectionCriteria))
                {
                    u.ExtEntry.fFlags = ISO9660_ELTORITO_SECTION_ENTRY_EXT_F_MORE;
                    memcpy(u.ExtEntry.abSelectionCriteria, pbSrc, sizeof(u.ExtEntry.abSelectionCriteria));
                    pbSrc += sizeof(u.ExtEntry.abSelectionCriteria);
                    cbSrc -= sizeof(u.ExtEntry.abSelectionCriteria);
                }
                else
                {
                    u.ExtEntry.fFlags = 0;
                    RT_ZERO(u.ExtEntry.abSelectionCriteria);
                    memcpy(u.ExtEntry.abSelectionCriteria, pbSrc, cbSrc);
                    cbSrc = 0;
                }

                idxBootCat++;
                rc = RTVfsFileWriteAt(pThis->pBootCatFile->u.hVfsFile, ISO9660_ELTORITO_ENTRY_SIZE * idxBootCat,
                                      &u.Entry, sizeof(u.Entry), NULL);
                if (RT_FAILURE(rc))
                    break;

                /* update the internal tracker. */
                if (pThis->aBootCatEntries[idxBootCat].pBootFile)
                {
                    pThis->aBootCatEntries[idxBootCat].pBootFile->Core.cNotOrphan--;
                    pThis->aBootCatEntries[idxBootCat].pBootFile = NULL;
                }

                pThis->aBootCatEntries[idxBootCat].bType    = ISO9660_ELTORITO_SECTION_ENTRY_EXT_ID;
                pThis->aBootCatEntries[idxBootCat].cEntries = 1;
            }
        }
    }
    return rc;
}


/**
 * Set the validation entry of the boot catalog (this is the first entry).
 *
 * @returns IPRT status code.
 * @param   hIsoMaker           The ISO maker handle.
 * @param   idxBootCat          The boot catalog entry.
 * @param   cEntries            Number of entries in the section.
 * @param   idPlatform          The platform ID
 *                              (ISO9660_ELTORITO_PLATFORM_ID_XXX).
 * @param   pszString           Section identifier or something.  Optional.
 */
RTDECL(int) RTFsIsoMakerBootCatSetSectionHeaderEntry(RTFSISOMAKER hIsoMaker, uint32_t idxBootCat, uint32_t cEntries,
                                                     uint8_t idPlatform, const char *pszString)
{
    /*
     * Validate input.
     */
    PRTFSISOMAKERINT pThis = hIsoMaker;
    RTFSISOMAKER_ASSERT_VALID_HANDLE_RET(pThis);

    AssertReturn(idxBootCat >= 2 && idxBootCat < RT_ELEMENTS(pThis->aBootCatEntries) - 1U, VERR_OUT_OF_RANGE);
    AssertReturn(cEntries < RT_ELEMENTS(pThis->aBootCatEntries) - 2U - 1U, VERR_OUT_OF_RANGE);
    AssertReturn(idxBootCat + cEntries + 1 < RT_ELEMENTS(pThis->aBootCatEntries), VERR_OUT_OF_RANGE);

    size_t cchString = 0;
    if (pszString)
    {
        cchString = RTStrCalcLatin1Len(pszString);
        AssertReturn(cchString < RT_SIZEOFMEMB(ISO9660ELTORITOVALIDATIONENTRY, achId), VERR_OUT_OF_RANGE);
    }

    /*
     * Make sure we've got a boot file.
     */
    int rc = rtFsIsoMakerEnsureBootCatFile(pThis);
    if (RT_SUCCESS(rc))
    {
        /*
         * Construct the entry data.
         */
        ISO9660ELTORITOSECTIONHEADER Entry;
        Entry.bHeaderId   = ISO9660_ELTORITO_HEADER_ID_SECTION_HEADER;
        Entry.bPlatformId = idPlatform;
        Entry.cEntries    = RT_H2LE_U16(cEntries);
        RT_ZERO(Entry.achSectionId);
        if (cchString)
        {
            char *pszTmp = Entry.achSectionId;
            rc = RTStrToLatin1Ex(pszString, RTSTR_MAX, &pszTmp, sizeof(Entry.achSectionId), NULL);
            AssertRC(rc);
        }

        /*
         * Write the entry and update our internal tracker.
         */
        rc = RTVfsFileWriteAt(pThis->pBootCatFile->u.hVfsFile, ISO9660_ELTORITO_ENTRY_SIZE * idxBootCat,
                              &Entry, sizeof(Entry), NULL);
        if (RT_SUCCESS(rc))
        {
            if (pThis->aBootCatEntries[idxBootCat].pBootFile != NULL)
            {
                pThis->aBootCatEntries[idxBootCat].pBootFile->Core.cNotOrphan--;
                pThis->aBootCatEntries[idxBootCat].pBootFile = NULL;
            }

            pThis->aBootCatEntries[idxBootCat].bType    = ISO9660_ELTORITO_HEADER_ID_SECTION_HEADER;
            pThis->aBootCatEntries[idxBootCat].cEntries = cEntries + 1;
        }
    }
    return rc;
}





/*
 *
 * Image finalization.
 * Image finalization.
 * Image finalization.
 *
 */


/**
 * Remove any orphaned object from the disk.
 *
 * @returns IPRT status code.
 * @param   pThis               The ISO maker instance.
 */
static int rtFsIsoMakerFinalizeRemoveOrphans(PRTFSISOMAKERINT pThis)
{
    for (;;)
    {
        uint32_t         cRemoved = 0;
        PRTFSISOMAKEROBJ pCur;
        PRTFSISOMAKEROBJ pNext;
        RTListForEachSafe(&pThis->ObjectHead, pCur, pNext, RTFSISOMAKEROBJ, Entry)
        {
            if (   pCur->pPrimaryName
                || pCur->pJolietName
                || pCur->pUdfName
                || pCur->pHfsName
                || pCur->cNotOrphan > 0)
            { /* likely */ }
            else
            {
                Log4(("rtFsIsoMakerFinalizeRemoveOrphans: %#x cbData=%#RX64\n", pCur->idxObj,
                      pCur->enmType == RTFSISOMAKEROBJTYPE_FILE ? ((PRTFSISOMAKERFILE)(pCur))->cbData : 0));
                int rc = rtFsIsoMakerObjRemoveWorker(pThis, pCur);
                if (RT_SUCCESS(rc))
                    cRemoved++;
                else
                    return rc;
            }
        }
        if (!cRemoved)
            return VINF_SUCCESS;
    }
}


/**
 * Finalizes the El Torito boot stuff, part 1.
 *
 * This includes generating the boot catalog data and fixing the location of all
 * related image files.
 *
 * @returns IPRT status code.
 * @param   pThis               The ISO maker instance.
 */
static int rtFsIsoMakerFinalizeBootStuffPart1(PRTFSISOMAKERINT pThis)
{
    /*
     * Anything?
     */
    if (!pThis->pBootCatFile)
        return VINF_SUCCESS;

    /*
     * Validate the boot catalog file.
     */
    AssertReturn(pThis->aBootCatEntries[0].bType == ISO9660_ELTORITO_HEADER_ID_VALIDATION_ENTRY,
                 VERR_ISOMK_BOOT_CAT_NO_VALIDATION_ENTRY);
    AssertReturn(pThis->aBootCatEntries[1].pBootFile != NULL, VERR_ISOMK_BOOT_CAT_NO_DEFAULT_ENTRY);

    /* Check any sections following the default one. */
    uint32_t cEntries = 2;
    while (   cEntries < RT_ELEMENTS(pThis->aBootCatEntries) - 1U
           && pThis->aBootCatEntries[cEntries].cEntries > 0)
    {
        AssertReturn(pThis->aBootCatEntries[cEntries].bType == ISO9660_ELTORITO_HEADER_ID_SECTION_HEADER,
                     VERR_ISOMK_BOOT_CAT_EXPECTED_SECTION_HEADER);
        for (uint32_t i = 1; i < pThis->aBootCatEntries[cEntries].cEntries; i++)
            AssertReturn(pThis->aBootCatEntries[cEntries].pBootFile != NULL,
                         pThis->aBootCatEntries[cEntries].cEntries == 0
                         ? VERR_ISOMK_BOOT_CAT_EMPTY_ENTRY : VERR_ISOMK_BOOT_CAT_INVALID_SECTION_SIZE);
        cEntries += pThis->aBootCatEntries[cEntries].cEntries;
    }

    /* Save for size setting. */
    uint32_t const cEntriesInFile = cEntries + 1;

    /* Check that the remaining entries are empty. */
    while (cEntries < RT_ELEMENTS(pThis->aBootCatEntries))
    {
        AssertReturn(pThis->aBootCatEntries[cEntries].cEntries == 0, VERR_ISOMK_BOOT_CAT_ERRATIC_ENTRY);
        cEntries++;
    }

    /*
     * Fixate the size of the boot catalog file.
     */
    pThis->pBootCatFile->cbData = cEntriesInFile * ISO9660_ELTORITO_ENTRY_SIZE;
    pThis->cbData  += RT_ALIGN_32(cEntriesInFile * ISO9660_ELTORITO_ENTRY_SIZE, RTFSISOMAKER_SECTOR_SIZE);

    /*
     * Move up the boot images and boot catalog to the start of the image.
     */
    for (uint32_t i = RT_ELEMENTS(pThis->aBootCatEntries) - 2; i > 0; i--)
        if (pThis->aBootCatEntries[i].pBootFile)
        {
            RTListNodeRemove(&pThis->aBootCatEntries[i].pBootFile->Core.Entry);
            RTListPrepend(&pThis->ObjectHead, &pThis->aBootCatEntries[i].pBootFile->Core.Entry);
        }

    /* The boot catalog comes first. */
    RTListNodeRemove(&pThis->pBootCatFile->Core.Entry);
    RTListPrepend(&pThis->ObjectHead, &pThis->pBootCatFile->Core.Entry);

    return VINF_SUCCESS;
}


/**
 * Finalizes the El Torito boot stuff, part 1.
 *
 * This includes generating the boot catalog data and fixing the location of all
 * related image files.
 *
 * @returns IPRT status code.
 * @param   pThis               The ISO maker instance.
 */
static int rtFsIsoMakerFinalizeBootStuffPart2(PRTFSISOMAKERINT pThis)
{
    /*
     * Anything?
     */
    if (!pThis->pBootCatFile)
        return VINF_SUCCESS;

    /*
     * Fill in the descriptor.
     */
    PISO9660BOOTRECORDELTORITO pDesc = pThis->pElToritoDesc;
    pDesc->Hdr.bDescType    = ISO9660VOLDESC_TYPE_BOOT_RECORD;
    pDesc->Hdr.bDescVersion = ISO9660PRIMARYVOLDESC_VERSION;
    memcpy(pDesc->Hdr.achStdId, ISO9660VOLDESC_STD_ID, sizeof(pDesc->Hdr.achStdId));
    memcpy(pDesc->achBootSystemId, RT_STR_TUPLE(ISO9660BOOTRECORDELTORITO_BOOT_SYSTEM_ID));
    pDesc->offBootCatalog   = RT_H2LE_U32((uint32_t)(pThis->pBootCatFile->offData / RTFSISOMAKER_SECTOR_SIZE));

    /*
     * Update the image file locations.
     */
    uint32_t cEntries = 2;
    for (uint32_t i = 1; i < RT_ELEMENTS(pThis->aBootCatEntries) - 1; i++)
        if (pThis->aBootCatEntries[i].pBootFile)
        {
            uint32_t off = pThis->aBootCatEntries[i].pBootFile->offData / RTFSISOMAKER_SECTOR_SIZE;
            off = RT_H2LE_U32(off);
            int rc = RTVfsFileWriteAt(pThis->pBootCatFile->u.hVfsFile,
                                      i * ISO9660_ELTORITO_ENTRY_SIZE + RT_UOFFSETOF(ISO9660ELTORITOSECTIONENTRY, offBootImage),
                                      &off, sizeof(off), NULL /*pcbWritten*/);
            AssertRCReturn(rc, rc);
            if (i == cEntries)
                cEntries = i + 1;
        }

    /*
     * Write end section.
     */
    ISO9660ELTORITOSECTIONHEADER Entry;
    Entry.bHeaderId   = ISO9660_ELTORITO_HEADER_ID_FINAL_SECTION_HEADER;
    Entry.bPlatformId = ISO9660_ELTORITO_PLATFORM_ID_X86;
    Entry.cEntries    = 0;
    RT_ZERO(Entry.achSectionId);
    int rc = RTVfsFileWriteAt(pThis->pBootCatFile->u.hVfsFile, cEntries * ISO9660_ELTORITO_ENTRY_SIZE,
                              &Entry, sizeof(Entry), NULL /*pcbWritten*/);
    AssertRCReturn(rc, rc);

    return VINF_SUCCESS;
}


/**
 * Gathers the dirs for an ISO-9660 namespace (e.g. primary or joliet).
 *
 * @param   pNamespace          The namespace.
 * @param   pFinalizedDirs      The finalized directory structure.  The
 *                              FinalizedDirs will be worked here.
 */
static void rtFsIsoMakerFinalizeGatherDirs(PRTFSISOMAKERNAMESPACE pNamespace, PRTFSISOMAKERFINALIZEDDIRS pFinalizedDirs)
{
    RTListInit(&pFinalizedDirs->FinalizedDirs);

    /*
     * Enter the root directory (if we got one).
     */
    if (!pNamespace->pRoot)
        return;
    PRTFSISOMAKERNAMEDIR pCurDir = pNamespace->pRoot->pDir;
    RTListAppend(&pFinalizedDirs->FinalizedDirs, &pCurDir->FinalizedEntry);
    do
    {
        /*
         * Scan pCurDir and add directories.  We don't need to sort anything
         * here because the directory is already in path table compatible order.
         */
        uint32_t            cLeft    = pCurDir->cChildren;
        PPRTFSISOMAKERNAME  ppChild  = pCurDir->papChildren;
        while (cLeft-- > 0)
        {
            PRTFSISOMAKERNAME pChild = *ppChild++;
            if (pChild->pDir)
                RTListAppend(&pFinalizedDirs->FinalizedDirs, &pChild->pDir->FinalizedEntry);
        }

        /*
         * Advance to the next directory.
         */
        pCurDir = RTListGetNext(&pFinalizedDirs->FinalizedDirs, pCurDir, RTFSISOMAKERNAMEDIR, FinalizedEntry);
    } while (pCurDir);
}


/**
 * Finalizes a directory entry (i.e. namespace node).
 *
 * This calculates the directory record size.
 *
 * @returns IPRT status code.
 * @param   pFinalizedDirs  .
 * @param   pName           The directory entry to finalize.
 * @param   offInDir        The offset in the directory of this record.
 * @param   uRootRockRidge  This is the rock ridge level when
 *                          root, otherwise it's zero.
 */
static int rtFsIsoMakerFinalizeIsoDirectoryEntry(PRTFSISOMAKERFINALIZEDDIRS pFinalizedDirs, PRTFSISOMAKERNAME pName,
                                                 uint32_t offInDir, uint8_t uRootRockRidge)
{
    /* Set directory and translation table offsets.  (These are for
       helping generating data blocks later.) */
    pName->offDirRec   = offInDir;

    /* Calculate the minimal directory record size. */
    size_t cbDirRec  = RT_UOFFSETOF(ISO9660DIRREC, achFileId) + pName->cbNameInDirRec + !(pName->cbNameInDirRec & 1);
    AssertReturn(cbDirRec <= UINT8_MAX, VERR_FILENAME_TOO_LONG);
    pName->cbDirRecWithZeroPad = pName->cbDirRec = (uint8_t)cbDirRec;

    /*
     * Calculate additional rock ridge stuff, if it doesn't all fit write it
     * to the spill file.
     */
    if (pFinalizedDirs->pRRSpillFile)
    {
        /** @todo rock ridge    */
        RT_NOREF(uRootRockRidge);
    }

    return VINF_SUCCESS;
}


/**
 * Finalizes either a primary and secondary ISO namespace.
 *
 * @returns IPRT status code
 * @param   pThis           The ISO maker instance.
 * @param   pNamespace      The namespace.
 * @param   pFinalizedDirs  The finalized directories structure for the
 *                          namespace.
 * @param   poffData        The data offset.  We will allocate blocks for the
 *                          directories and the path tables.
 */
static int rtFsIsoMakerFinalizeDirectoriesInIsoNamespace(PRTFSISOMAKERINT pThis, PRTFSISOMAKERNAMESPACE pNamespace,
                                                         PRTFSISOMAKERFINALIZEDDIRS pFinalizedDirs, uint64_t *poffData)
{
    /* The directory data comes first, so take down it's offset. */
    pFinalizedDirs->offDirs = *poffData;

    /*
     * Reset the rock ridge spill file (in case we allow finalizing more than once)
     * and create a new spill file if rock ridge is enabled.  The directory entry
     * finalize function uses this as a clue that rock ridge is enabled.
     */
    if (pFinalizedDirs->pRRSpillFile)
    {
        rtFsIsoMakerObjRemoveWorker(pThis, &pFinalizedDirs->pRRSpillFile->Core);
        pFinalizedDirs->pRRSpillFile = NULL;
    }
    if (pNamespace->uRockRidgeLevel > 0)
    {
        /** @todo create rock ridge spill file to indicate rock ridge */
    }

    int      rc;
    uint16_t idPathTable = 1;
    uint32_t cbPathTable = 0;
    if (pNamespace->pRoot)
    {
        /*
         * Precalc the directory record size for the root directory.
         */
        rc = rtFsIsoMakerFinalizeIsoDirectoryEntry(pFinalizedDirs, pNamespace->pRoot, 0 /*offInDir*/, pNamespace->uRockRidgeLevel);
        AssertRCReturn(rc, rc);

        /*
         * Work thru the directories.
         */
        PRTFSISOMAKERNAMEDIR pCurDir;
        RTListForEach(&pFinalizedDirs->FinalizedDirs, pCurDir, RTFSISOMAKERNAMEDIR, FinalizedEntry)
        {
            PRTFSISOMAKERNAME pCurName    = pCurDir->pName;
            PRTFSISOMAKERNAME pParentName = pCurName->pParent ? pCurName->pParent : pCurName;

            /* We don't do anything special for the special '.' and '..' directory
               entries, instead we use the directory entry in the parent directory
               with a 1 byte name (00 or 01). */
            Assert(pCurName->cbDirRec != 0);
            Assert(pParentName->cbDirRec != 0);
            pCurDir->cbDirRec00 = pCurName->cbDirRec    - pCurName->cbNameInDirRec    - !(pCurName->cbNameInDirRec    & 1) + 1;
            pCurDir->cbDirRec01 = pParentName->cbDirRec - pParentName->cbNameInDirRec - !(pParentName->cbNameInDirRec & 1) + 1;

            uint32_t offInDir   = (uint32_t)pCurDir->cbDirRec00 + pCurDir->cbDirRec01;

            /* Finalize the directory entries. */
            uint32_t            cbTransTbl  = 0;
            uint32_t            cLeft       = pCurDir->cChildren;
            PPRTFSISOMAKERNAME  ppChild     = pCurDir->papChildren;
            while (cLeft-- > 0)
            {
                PRTFSISOMAKERNAME pChild = *ppChild++;
                rc = rtFsIsoMakerFinalizeIsoDirectoryEntry(pFinalizedDirs, pChild, offInDir, 0 /*uRootRockRidge*/);
                AssertRCReturn(rc, rc);

                if ((RTFSISOMAKER_SECTOR_SIZE - (offInDir & RTFSISOMAKER_SECTOR_OFFSET_MASK)) < pChild->cbDirRec)
                {
                    Assert(ppChild[-1] == pChild && &ppChild[-1] != pCurDir->papChildren);
                    ppChild[-2]->cbDirRecWithZeroPad += RTFSISOMAKER_SECTOR_SIZE - (offInDir & RTFSISOMAKER_SECTOR_OFFSET_MASK);
                    offInDir = (offInDir | RTFSISOMAKER_SECTOR_OFFSET_MASK) + 1; /* doesn't fit, skip to next sector. */
                    Log4(("rtFsIsoMakerFinalizeDirectoriesInIsoNamespace: zero padding dir rec @%#x: %#x -> %#x; offset %#x -> %#x\n",
                          ppChild[-2]->offDirRec, ppChild[-2]->cbDirRec, ppChild[-2]->cbDirRecWithZeroPad, pChild->offDirRec, offInDir));
                    pChild->offDirRec = offInDir;
                }

                offInDir += pChild->cbDirRec;
                if (pChild->cchTransNm)
                    cbTransTbl += 2 /* type & space*/
                               +  RT_MAX(pChild->cchName, RTFSISOMAKER_TRANS_TBL_LEFT_PAD)
                               +  1 /* tab */
                               +  pChild->cchTransNm
                               +  1 /* newline */;
            }

            /* Set the directory size and location, advancing the data offset. */
            pCurDir->cbDir  = offInDir;
            pCurDir->offDir = *poffData;
            *poffData += RT_ALIGN_32(offInDir, RTFSISOMAKER_SECTOR_SIZE);

            /* Set the translation table file size. */
            if (pCurDir->pTransTblFile)
            {
                pCurDir->pTransTblFile->cbData = cbTransTbl;
                pThis->cbData += RT_ALIGN_32(cbTransTbl, RTFSISOMAKER_SECTOR_SIZE);
            }

            /* Add to the path table size calculation. */
            pCurDir->offPathTable = cbPathTable;
            pCurDir->idPathTable  = idPathTable++;
            cbPathTable += RTFSISOMAKER_CALC_PATHREC_SIZE(pCurName->cbNameInDirRec);

            Log4(("rtFsIsoMakerFinalizeDirectoriesInIsoNamespace: idxObj=#%#x cbDir=%#08x cChildren=%#05x %s\n",
                  pCurDir->pName->pObj->idxObj, pCurDir->cbDir, pCurDir->cChildren, pCurDir->pName->szName));
        }
    }

    /*
     * Update the rock ridge spill file size.
     */
    if (pFinalizedDirs->pRRSpillFile)
    {
        rc = RTVfsFileGetSize(pFinalizedDirs->pRRSpillFile->u.hVfsFile, &pFinalizedDirs->pRRSpillFile->cbData);
        AssertRCReturn(rc, rc);
        pThis->cbData += pFinalizedDirs->pRRSpillFile->cbData;
    }

    /*
     * Calculate the path table offsets and move past them.
     */
    pFinalizedDirs->cbPathTable   = cbPathTable;
    pFinalizedDirs->offPathTableL = *poffData;
    *poffData += RT_ALIGN_64(cbPathTable, RTFSISOMAKER_SECTOR_SIZE);

    pFinalizedDirs->offPathTableM = *poffData;
    *poffData += RT_ALIGN_64(cbPathTable, RTFSISOMAKER_SECTOR_SIZE);

    return VINF_SUCCESS;
}



/**
 * Finalizes directories and related stuff.
 *
 * This will not generate actual directory data, but calculate the size of it
 * once it's generated.  Ditto for the path tables.  The exception is the rock
 * ridge spill file, which will be generated in memory.
 *
 * @returns IPRT status code.
 * @param   pThis               The ISO maker instance.
 * @param   poffData            The data offset (in/out).
 */
static int rtFsIsoMakerFinalizeDirectories(PRTFSISOMAKERINT pThis, uint64_t *poffData)
{
    /*
     * Locate the directories, width first, inserting them in the finalized lists so
     * we can process them efficiently.
     */
    rtFsIsoMakerFinalizeGatherDirs(&pThis->PrimaryIso, &pThis->PrimaryIsoDirs);
    rtFsIsoMakerFinalizeGatherDirs(&pThis->Joliet, &pThis->JolietDirs);

    /*
     * Process the primary ISO and joliet namespaces.
     */
    int rc = rtFsIsoMakerFinalizeDirectoriesInIsoNamespace(pThis, &pThis->PrimaryIso, &pThis->PrimaryIsoDirs, poffData);
    if (RT_SUCCESS(rc))
        rc = rtFsIsoMakerFinalizeDirectoriesInIsoNamespace(pThis, &pThis->Joliet, &pThis->JolietDirs, poffData);
    if (RT_SUCCESS(rc))
    {
        /*
         * Later: UDF, HFS.
         */
    }
    return rc;
}


/**
 * Finalizes data allocations.
 *
 * This will set the RTFSISOMAKERFILE::offData members.
 *
 * @returns IPRT status code.
 * @param   pThis               The ISO maker instance.
 * @param   poffData            The data offset (in/out).
 */
static int rtFsIsoMakerFinalizeData(PRTFSISOMAKERINT pThis, uint64_t *poffData)
{
    pThis->offFirstFile = *poffData;

    /*
     * We currently does not have any ordering prioritizing implemented, so we
     * just store files in the order they were added.
     */
    PRTFSISOMAKEROBJ pCur;
    RTListForEach(&pThis->ObjectHead, pCur, RTFSISOMAKEROBJ, Entry)
    {
        if (pCur->enmType == RTFSISOMAKEROBJTYPE_FILE)
        {
            PRTFSISOMAKERFILE pCurFile = (PRTFSISOMAKERFILE)pCur;
            if (pCurFile->offData == UINT64_MAX)
            {
                pCurFile->offData = *poffData;
                *poffData += RT_ALIGN_64(pCurFile->cbData, RTFSISOMAKER_SECTOR_SIZE);
                RTListAppend(&pThis->FinalizedFiles, &pCurFile->FinalizedEntry);
                Log4(("rtFsIsoMakerFinalizeData: %#x @%#RX64 cbData=%#RX64\n", pCurFile->Core.idxObj, pCurFile->offData, pCurFile->cbData));
            }

            /*
             * Create the boot info table.
             */
            if (pCurFile->pBootInfoTable)
            {
                /*
                 * Checksum the file.
                 */
                int rc;
                RTVFSFILE hVfsFile;
                uint64_t  offBase;
                switch (pCurFile->enmSrcType)
                {
                    case RTFSISOMAKERSRCTYPE_PATH:
                        rc = RTVfsChainOpenFile(pCurFile->u.pszSrcPath, RTFILE_O_READ | RTFILE_O_DENY_NONE | RTFILE_O_OPEN,
                                                &hVfsFile, NULL, NULL);
                        AssertMsgRCReturn(rc, ("%s -> %Rrc\n", pCurFile->u.pszSrcPath, rc), rc);
                        offBase = 0;
                        break;
                    case RTFSISOMAKERSRCTYPE_VFS_FILE:
                        hVfsFile = pCurFile->u.hVfsFile;
                        offBase = 0;
                        rc = VINF_SUCCESS;
                        break;
                    case RTFSISOMAKERSRCTYPE_COMMON:
                        hVfsFile = pThis->paCommonSources[pCurFile->u.Common.idxSrc];
                        offBase  = pCurFile->u.Common.offData;
                        rc = VINF_SUCCESS;
                        break;
                    default:
                        AssertMsgFailedReturn(("enmSrcType=%d\n", pCurFile->enmSrcType), VERR_INTERNAL_ERROR_3);
                }

                uint32_t uChecksum = 0;
                uint32_t off       = 64;
                uint32_t cbLeft    = RT_MAX(64, (uint32_t)pCurFile->cbData) - 64;
                while (cbLeft > 0)
                {
                    union
                    {
                        uint8_t     ab[_16K];
                        uint32_t    au32[_16K / sizeof(uint32_t)];
                    }        uBuf;
                    uint32_t cbRead = RT_MIN(sizeof(uBuf), cbLeft);
                    if (cbRead & 3)
                        RT_ZERO(uBuf);
                    rc = RTVfsFileReadAt(hVfsFile, offBase + off, &uBuf, cbRead, NULL);
                    if (RT_FAILURE(rc))
                        break;

                    size_t i = RT_ALIGN_Z(cbRead, sizeof(uint32_t)) / sizeof(uint32_t);
                    while (i-- > 0)
                        uChecksum += RT_LE2H_U32(uBuf.au32[i]);

                    off    += cbRead;
                    cbLeft -= cbRead;
                }

                if (pCurFile->enmSrcType == RTFSISOMAKERSRCTYPE_PATH)
                    RTVfsFileRelease(hVfsFile);
                if (RT_FAILURE(rc))
                    return rc;

                /*
                 * Populate the structure.
                 */
                pCurFile->pBootInfoTable->offPrimaryVolDesc = RT_H2LE_U32(16 * RTFSISOMAKER_SECTOR_SIZE);
                pCurFile->pBootInfoTable->offBootFile       = RT_H2LE_U32((uint32_t)(pCurFile->offData / RTFSISOMAKER_SECTOR_SIZE));
                pCurFile->pBootInfoTable->cbBootFile        = RT_H2LE_U32((uint32_t)pCurFile->cbData);
                pCurFile->pBootInfoTable->uChecksum         = RT_H2LE_U32(uChecksum);
                RT_ZERO(pCurFile->pBootInfoTable->auReserved);
            }
        }
    }

    return VINF_SUCCESS;
}


/**
 * Copies the given string as UTF-16 and pad unused space in the destination
 * with spaces.
 *
 * @param   pachDst     The destination field.  C type is char, but real life
 *                      type is UTF-16 / UCS-2.
 * @param   cchDst      The size of the destination field.
 * @param   pszSrc      The source string. NULL is treated like empty string.
 */
static void rtFsIsoMakerFinalizeCopyAsUtf16BigAndSpacePad(char *pachDst, size_t cchDst, const char *pszSrc)
{
    size_t cwcSrc = 0;
    if (pszSrc)
    {
        RTUTF16  wszSrc[256];
        PRTUTF16 pwszSrc = wszSrc;
        int rc = RTStrToUtf16BigEx(pszSrc, RTSTR_MAX, &pwszSrc, RT_ELEMENTS(wszSrc), &cwcSrc);
        AssertRCStmt(rc, cwcSrc = 0);

        if (cwcSrc > cchDst / sizeof(RTUTF16))
            cwcSrc = cchDst / sizeof(RTUTF16);
        memcpy(pachDst, wszSrc, cwcSrc * sizeof(RTUTF16));
    }

    /* Space padding.  Note! cchDst can be an odd number. */
    size_t cchWritten = cwcSrc * sizeof(RTUTF16);
    if (cchWritten < cchDst)
    {
        while (cchWritten + 2 <= cchDst)
        {
            pachDst[cchWritten++] = '\0';
            pachDst[cchWritten++] = ' ';
        }
        if (cchWritten < cchDst)
            pachDst[cchWritten] = '\0';
    }
}


/**
 * Copies the given string and pad unused space in the destination with spaces.
 *
 * @param   pachDst     The destination field.
 * @param   cchDst      The size of the destination field.
 * @param   pszSrc      The source string. NULL is treated like empty string.
 */
static void rtFsIsoMakerFinalizeCopyAndSpacePad(char *pachDst, size_t cchDst, const char *pszSrc)
{
    size_t cchSrc;
    if (!pszSrc)
        cchSrc = 0;
    else
    {
        cchSrc = strlen(pszSrc);
        if (cchSrc > cchDst)
            cchSrc = cchDst;
        memcpy(pachDst, pszSrc, cchSrc);
    }
    if (cchSrc < cchDst)
        memset(&pachDst[cchSrc], ' ', cchDst - cchSrc);
}


/**
 * Formats a timespec as an ISO-9660 ascii timestamp.
 *
 * @param   pTime       The timespec to format.
 * @param   pIsoTs      The ISO-9660 timestamp destination buffer.
 */
static void rtFsIsoMakerTimespecToIso9660Timestamp(PCRTTIMESPEC pTime, PISO9660TIMESTAMP pIsoTs)
{
    RTTIME Exploded;
    RTTimeExplode(&Exploded, pTime);

    char szTmp[64];
#define FORMAT_FIELD(a_achDst, a_uSrc) \
        do { \
            RTStrFormatU32(szTmp, sizeof(szTmp), a_uSrc, 10, sizeof(a_achDst), sizeof(a_achDst), \
                           RTSTR_F_ZEROPAD | RTSTR_F_WIDTH | RTSTR_F_PRECISION); \
            memcpy(a_achDst, szTmp, sizeof(a_achDst)); \
        } while (0)
    FORMAT_FIELD(pIsoTs->achYear,   Exploded.i32Year);
    FORMAT_FIELD(pIsoTs->achMonth,  Exploded.u8Month);
    FORMAT_FIELD(pIsoTs->achDay,    Exploded.u8MonthDay);
    FORMAT_FIELD(pIsoTs->achHour,   Exploded.u8Hour);
    FORMAT_FIELD(pIsoTs->achMinute, Exploded.u8Minute);
    FORMAT_FIELD(pIsoTs->achSecond, Exploded.u8Second);
    FORMAT_FIELD(pIsoTs->achCentisecond, Exploded.u32Nanosecond / RT_NS_10MS);
#undef FORMAT_FIELD
    pIsoTs->offUtc = 0;
}

/**
 * Formats zero ISO-9660 ascii timestamp (treated as not specified).
 *
 * @param   pIsoTs      The ISO-9660 timestamp destination buffer.
 */
static void rtFsIsoMakerZero9660Timestamp(PISO9660TIMESTAMP pIsoTs)
{
    memset(pIsoTs, '0', RT_OFFSETOF(ISO9660TIMESTAMP, offUtc));
    pIsoTs->offUtc = 0;
}


/**
 * Formats a timespec as an ISO-9660 record timestamp.
 *
 * @param   pTime       The timespec to format.
 * @param   pIsoTs      The ISO-9660 timestamp destination buffer.
 */
static void rtFsIsoMakerTimespecToIso9660RecTimestamp(PCRTTIMESPEC pTime, PISO9660RECTIMESTAMP pIsoRecTs)
{
    RTTIME Exploded;
    RTTimeExplode(&Exploded, pTime);

    pIsoRecTs->bYear    = Exploded.i32Year >= 1900 ? Exploded.i32Year - 1900 : 0;
    pIsoRecTs->bMonth   = Exploded.u8Month;
    pIsoRecTs->bDay     = Exploded.u8MonthDay;
    pIsoRecTs->bHour    = Exploded.u8Hour;
    pIsoRecTs->bMinute  = Exploded.u8Minute;
    pIsoRecTs->bSecond  = Exploded.u8Second;
    pIsoRecTs->offUtc   = 0;
}


/**
 * Allocate and prepare the volume descriptors.
 *
 * What's not done here gets done later by rtFsIsoMakerFinalizeBootStuffPart2,
 * or at teh very end of the finalization by
 * rtFsIsoMakerFinalizeVolumeDescriptors.
 *
 * @returns IPRT status code
 * @param   pThis               The ISO maker instance.
 */
static int rtFsIsoMakerFinalizePrepVolumeDescriptors(PRTFSISOMAKERINT pThis)
{
    /*
     * Allocate and calc pointers.
     */
    RTMemFree(pThis->pbVolDescs);
    pThis->pbVolDescs = (uint8_t *)RTMemAllocZ(pThis->cVolumeDescriptors * RTFSISOMAKER_SECTOR_SIZE);
    AssertReturn(pThis->pbVolDescs, VERR_NO_MEMORY);

    uint32_t offVolDescs = 0;

    pThis->pPrimaryVolDesc = (PISO9660PRIMARYVOLDESC)&pThis->pbVolDescs[offVolDescs];
    offVolDescs += RTFSISOMAKER_SECTOR_SIZE;

    if (!pThis->pBootCatFile)
        pThis->pElToritoDesc = NULL;
    else
    {
        pThis->pElToritoDesc = (PISO9660BOOTRECORDELTORITO)&pThis->pbVolDescs[offVolDescs];
        offVolDescs += RTFSISOMAKER_SECTOR_SIZE;
    }

    if (!pThis->Joliet.uLevel)
        pThis->pJolietVolDesc = NULL;
    else
    {
        pThis->pJolietVolDesc = (PISO9660SUPVOLDESC)&pThis->pbVolDescs[offVolDescs];
        offVolDescs += RTFSISOMAKER_SECTOR_SIZE;
    }

    pThis->pTerminatorVolDesc = (PISO9660VOLDESCHDR)&pThis->pbVolDescs[offVolDescs];
    offVolDescs += RTFSISOMAKER_SECTOR_SIZE;

    if (pThis->Udf.uLevel > 0)
    {
        /** @todo UDF descriptors. */
    }
    AssertReturn(offVolDescs == pThis->cVolumeDescriptors * RTFSISOMAKER_SECTOR_SIZE, VERR_INTERNAL_ERROR_2);

    /*
     * This may be needed later.
     */
    char szImageCreationTime[42];
    RTTimeSpecToString(&pThis->ImageCreationTime, szImageCreationTime, sizeof(szImageCreationTime));

    /*
     * Initialize the primary descriptor.
     */
    PISO9660PRIMARYVOLDESC pPrimary = pThis->pPrimaryVolDesc;

    pPrimary->Hdr.bDescType             = ISO9660VOLDESC_TYPE_PRIMARY;
    pPrimary->Hdr.bDescVersion          = ISO9660PRIMARYVOLDESC_VERSION;
    memcpy(pPrimary->Hdr.achStdId, ISO9660VOLDESC_STD_ID, sizeof(pPrimary->Hdr.achStdId));
    //pPrimary->bPadding8               = 0;
    rtFsIsoMakerFinalizeCopyAndSpacePad(pPrimary->achSystemId, sizeof(pPrimary->achSystemId), pThis->PrimaryIso.pszSystemId);
    rtFsIsoMakerFinalizeCopyAndSpacePad(pPrimary->achVolumeId, sizeof(pPrimary->achVolumeId),
                                        pThis->PrimaryIso.pszVolumeId ? pThis->PrimaryIso.pszVolumeId : szImageCreationTime);
    //pPrimary->Unused73                = {0}
    //pPrimary->VolumeSpaceSize         = later
    //pPrimary->abUnused89              = {0}
    pPrimary->cVolumesInSet.be          = RT_H2BE_U16_C(1);
    pPrimary->cVolumesInSet.le          = RT_H2LE_U16_C(1);
    pPrimary->VolumeSeqNo.be            = RT_H2BE_U16_C(1);
    pPrimary->VolumeSeqNo.le            = RT_H2LE_U16_C(1);
    pPrimary->cbLogicalBlock.be         = RT_H2BE_U16_C(RTFSISOMAKER_SECTOR_SIZE);
    pPrimary->cbLogicalBlock.le         = RT_H2LE_U16_C(RTFSISOMAKER_SECTOR_SIZE);
    //pPrimary->cbPathTable             = later
    //pPrimary->offTypeLPathTable       = later
    //pPrimary->offOptionalTypeLPathTable = {0}
    //pPrimary->offTypeMPathTable       = later
    //pPrimary->offOptionalTypeMPathTable = {0}
    //pPrimary->RootDir                 = later
    rtFsIsoMakerFinalizeCopyAndSpacePad(pPrimary->achVolumeSetId, sizeof(pPrimary->achVolumeSetId),
                                        pThis->PrimaryIso.pszVolumeSetId);
    rtFsIsoMakerFinalizeCopyAndSpacePad(pPrimary->achPublisherId, sizeof(pPrimary->achPublisherId),
                                        pThis->PrimaryIso.pszPublisherId);
    rtFsIsoMakerFinalizeCopyAndSpacePad(pPrimary->achDataPreparerId, sizeof(pPrimary->achDataPreparerId),
                                        pThis->PrimaryIso.pszDataPreparerId);
    rtFsIsoMakerFinalizeCopyAndSpacePad(pPrimary->achApplicationId, sizeof(pPrimary->achApplicationId),
                                        pThis->PrimaryIso.pszApplicationId);
    rtFsIsoMakerFinalizeCopyAndSpacePad(pPrimary->achCopyrightFileId, sizeof(pPrimary->achCopyrightFileId),
                                        pThis->PrimaryIso.pszCopyrightFileId);
    rtFsIsoMakerFinalizeCopyAndSpacePad(pPrimary->achAbstractFileId, sizeof(pPrimary->achAbstractFileId),
                                        pThis->PrimaryIso.pszAbstractFileId);
    rtFsIsoMakerFinalizeCopyAndSpacePad(pPrimary->achBibliographicFileId, sizeof(pPrimary->achBibliographicFileId),
                                        pThis->PrimaryIso.pszBibliographicFileId);
    rtFsIsoMakerTimespecToIso9660Timestamp(&pThis->ImageCreationTime, &pPrimary->BirthTime);
    rtFsIsoMakerTimespecToIso9660Timestamp(&pThis->ImageCreationTime, &pPrimary->ModifyTime);
    rtFsIsoMakerZero9660Timestamp(&pPrimary->ExpireTime);
    rtFsIsoMakerZero9660Timestamp(&pPrimary->EffectiveTime);
    pPrimary->bFileStructureVersion     = ISO9660_FILE_STRUCTURE_VERSION;
    //pPrimary->bReserved883            = 0;
    //RT_ZERO(pPrimary->abAppUse);
    //RT_ZERO(pPrimary->abReserved1396);

    /*
     * Initialize the joliet descriptor if included.
     */
    PISO9660SUPVOLDESC pJoliet = pThis->pJolietVolDesc;
    if (pJoliet)
    {
        pJoliet->Hdr.bDescType              = ISO9660VOLDESC_TYPE_SUPPLEMENTARY;
        pJoliet->Hdr.bDescVersion           = ISO9660SUPVOLDESC_VERSION;
        memcpy(pJoliet->Hdr.achStdId, ISO9660VOLDESC_STD_ID, sizeof(pJoliet->Hdr.achStdId));
        pJoliet->fVolumeFlags               = ISO9660SUPVOLDESC_VOL_F_ESC_ONLY_REG;
        rtFsIsoMakerFinalizeCopyAsUtf16BigAndSpacePad(pJoliet->achSystemId, sizeof(pJoliet->achSystemId), pThis->Joliet.pszSystemId);
        rtFsIsoMakerFinalizeCopyAsUtf16BigAndSpacePad(pJoliet->achVolumeId, sizeof(pJoliet->achVolumeId),
                                                      pThis->Joliet.pszVolumeId ? pThis->Joliet.pszVolumeId : szImageCreationTime);
        //pJoliet->Unused73                 = {0}
        //pJoliet->VolumeSpaceSize          = later
        memset(pJoliet->abEscapeSequences, ' ', sizeof(pJoliet->abEscapeSequences));
        pJoliet->abEscapeSequences[0]       = ISO9660_JOLIET_ESC_SEQ_0;
        pJoliet->abEscapeSequences[1]       = ISO9660_JOLIET_ESC_SEQ_1;
        pJoliet->abEscapeSequences[2]       = pThis->Joliet.uLevel == 1 ? ISO9660_JOLIET_ESC_SEQ_2_LEVEL_1
                                            : pThis->Joliet.uLevel == 2 ? ISO9660_JOLIET_ESC_SEQ_2_LEVEL_2
                                            :                             ISO9660_JOLIET_ESC_SEQ_2_LEVEL_3;
        pJoliet->cVolumesInSet.be           = RT_H2BE_U16_C(1);
        pJoliet->cVolumesInSet.le           = RT_H2LE_U16_C(1);
        pJoliet->VolumeSeqNo.be             = RT_H2BE_U16_C(1);
        pJoliet->VolumeSeqNo.le             = RT_H2LE_U16_C(1);
        pJoliet->cbLogicalBlock.be          = RT_H2BE_U16_C(RTFSISOMAKER_SECTOR_SIZE);
        pJoliet->cbLogicalBlock.le          = RT_H2LE_U16_C(RTFSISOMAKER_SECTOR_SIZE);
        //pJoliet->cbPathTable              = later
        //pJoliet->offTypeLPathTable        = later
        //pJoliet->offOptionalTypeLPathTable = {0}
        //pJoliet->offTypeMPathTable        = later
        //pJoliet->offOptionalTypeMPathTable = {0}
        //pJoliet->RootDir                  = later
        rtFsIsoMakerFinalizeCopyAsUtf16BigAndSpacePad(pJoliet->achVolumeSetId, sizeof(pJoliet->achVolumeSetId),
                                                      pThis->Joliet.pszVolumeSetId);
        rtFsIsoMakerFinalizeCopyAsUtf16BigAndSpacePad(pJoliet->achPublisherId, sizeof(pJoliet->achPublisherId),
                                                      pThis->Joliet.pszPublisherId);
        rtFsIsoMakerFinalizeCopyAsUtf16BigAndSpacePad(pJoliet->achDataPreparerId, sizeof(pJoliet->achDataPreparerId),
                                                      pThis->Joliet.pszDataPreparerId);
        rtFsIsoMakerFinalizeCopyAsUtf16BigAndSpacePad(pJoliet->achApplicationId, sizeof(pJoliet->achApplicationId),
                                                      pThis->Joliet.pszApplicationId);
        rtFsIsoMakerFinalizeCopyAsUtf16BigAndSpacePad(pJoliet->achCopyrightFileId, sizeof(pJoliet->achCopyrightFileId),
                                                      pThis->Joliet.pszCopyrightFileId);
        rtFsIsoMakerFinalizeCopyAsUtf16BigAndSpacePad(pJoliet->achAbstractFileId, sizeof(pJoliet->achAbstractFileId),
                                                      pThis->Joliet.pszAbstractFileId);
        rtFsIsoMakerFinalizeCopyAsUtf16BigAndSpacePad(pJoliet->achBibliographicFileId, sizeof(pJoliet->achBibliographicFileId),
                                                      pThis->Joliet.pszBibliographicFileId);
        rtFsIsoMakerTimespecToIso9660Timestamp(&pThis->ImageCreationTime, &pJoliet->BirthTime);
        rtFsIsoMakerTimespecToIso9660Timestamp(&pThis->ImageCreationTime, &pJoliet->ModifyTime);
        rtFsIsoMakerZero9660Timestamp(&pJoliet->ExpireTime);
        rtFsIsoMakerZero9660Timestamp(&pJoliet->EffectiveTime);
        pJoliet->bFileStructureVersion      = ISO9660_FILE_STRUCTURE_VERSION;
        //pJoliet->bReserved883             = 0;
        //RT_ZERO(pJoliet->abAppUse);
        //RT_ZERO(pJoliet->abReserved1396);
    }

    /*
     * The ISO-9660 terminator descriptor.
     */
    pThis->pTerminatorVolDesc->bDescType    = ISO9660VOLDESC_TYPE_TERMINATOR;
    pThis->pTerminatorVolDesc->bDescVersion = 1;
    memcpy(pThis->pTerminatorVolDesc->achStdId, ISO9660VOLDESC_STD_ID, sizeof(pThis->pTerminatorVolDesc->achStdId));

    return VINF_SUCCESS;
}


/**
 * Finalizes the volume descriptors.
 *
 * This will set the RTFSISOMAKERFILE::offData members.
 *
 * @returns IPRT status code.
 * @param   pThis               The ISO maker instance.
 */
static int rtFsIsoMakerFinalizeVolumeDescriptors(PRTFSISOMAKERINT pThis)
{
    AssertReturn(pThis->pbVolDescs && pThis->pPrimaryVolDesc && pThis->pTerminatorVolDesc, VERR_INTERNAL_ERROR_3);

    /*
     * Primary descriptor.
     */
    PISO9660PRIMARYVOLDESC pPrimary = pThis->pPrimaryVolDesc;

    pPrimary->VolumeSpaceSize.be        = RT_H2BE_U32(pThis->cbFinalizedImage / RTFSISOMAKER_SECTOR_SIZE);
    pPrimary->VolumeSpaceSize.le        = RT_H2LE_U32(pThis->cbFinalizedImage / RTFSISOMAKER_SECTOR_SIZE);
    pPrimary->cbPathTable.be            = RT_H2BE_U32(pThis->PrimaryIsoDirs.cbPathTable);
    pPrimary->cbPathTable.le            = RT_H2LE_U32(pThis->PrimaryIsoDirs.cbPathTable);
    pPrimary->offTypeLPathTable         = RT_H2LE_U32(pThis->PrimaryIsoDirs.offPathTableL / RTFSISOMAKER_SECTOR_SIZE);
    pPrimary->offTypeMPathTable         = RT_H2BE_U32(pThis->PrimaryIsoDirs.offPathTableM / RTFSISOMAKER_SECTOR_SIZE);
    pPrimary->RootDir.DirRec.cbDirRec           = sizeof(pPrimary->RootDir);
    pPrimary->RootDir.DirRec.cExtAttrBlocks     = 0;
    pPrimary->RootDir.DirRec.offExtent.be       = RT_H2BE_U32(pThis->PrimaryIso.pRoot->pDir->offDir / RTFSISOMAKER_SECTOR_SIZE);
    pPrimary->RootDir.DirRec.offExtent.le       = RT_H2LE_U32(pThis->PrimaryIso.pRoot->pDir->offDir / RTFSISOMAKER_SECTOR_SIZE);
    pPrimary->RootDir.DirRec.cbData.be          = RT_H2BE_U32(pThis->PrimaryIso.pRoot->pDir->cbDir);
    pPrimary->RootDir.DirRec.cbData.le          = RT_H2LE_U32(pThis->PrimaryIso.pRoot->pDir->cbDir);
    rtFsIsoMakerTimespecToIso9660RecTimestamp(&pThis->PrimaryIso.pRoot->pObj->BirthTime, &pPrimary->RootDir.DirRec.RecTime);
    pPrimary->RootDir.DirRec.fFileFlags         = ISO9660_FILE_FLAGS_DIRECTORY;
    pPrimary->RootDir.DirRec.bFileUnitSize      = 0;
    pPrimary->RootDir.DirRec.bInterleaveGapSize = 0;
    pPrimary->RootDir.DirRec.VolumeSeqNo.be     = RT_H2BE_U16_C(1);
    pPrimary->RootDir.DirRec.VolumeSeqNo.le     = RT_H2LE_U16_C(1);
    pPrimary->RootDir.DirRec.bFileIdLength      = 1;
    pPrimary->RootDir.DirRec.achFileId[0]       = 0x00;

    /*
     * Initialize the joliet descriptor if included.
     */
    PISO9660SUPVOLDESC pJoliet = pThis->pJolietVolDesc;
    if (pJoliet)
    {
        pJoliet->VolumeSpaceSize            = pPrimary->VolumeSpaceSize;
        pJoliet->cbPathTable.be             = RT_H2BE_U32(pThis->JolietDirs.cbPathTable);
        pJoliet->cbPathTable.le             = RT_H2LE_U32(pThis->JolietDirs.cbPathTable);
        pJoliet->offTypeLPathTable          = RT_H2LE_U32(pThis->JolietDirs.offPathTableL / RTFSISOMAKER_SECTOR_SIZE);
        pJoliet->offTypeMPathTable          = RT_H2BE_U32(pThis->JolietDirs.offPathTableM / RTFSISOMAKER_SECTOR_SIZE);
        pJoliet->RootDir.DirRec.cbDirRec           = sizeof(pJoliet->RootDir);
        pJoliet->RootDir.DirRec.cExtAttrBlocks     = 0;
        pJoliet->RootDir.DirRec.offExtent.be       = RT_H2BE_U32(pThis->Joliet.pRoot->pDir->offDir / RTFSISOMAKER_SECTOR_SIZE);
        pJoliet->RootDir.DirRec.offExtent.le       = RT_H2LE_U32(pThis->Joliet.pRoot->pDir->offDir / RTFSISOMAKER_SECTOR_SIZE);
        pJoliet->RootDir.DirRec.cbData.be          = RT_H2BE_U32(pThis->Joliet.pRoot->pDir->cbDir);
        pJoliet->RootDir.DirRec.cbData.le          = RT_H2LE_U32(pThis->Joliet.pRoot->pDir->cbDir);
        rtFsIsoMakerTimespecToIso9660RecTimestamp(&pThis->Joliet.pRoot->pObj->BirthTime, &pJoliet->RootDir.DirRec.RecTime);
        pJoliet->RootDir.DirRec.fFileFlags         = ISO9660_FILE_FLAGS_DIRECTORY;
        pJoliet->RootDir.DirRec.bFileUnitSize      = 0;
        pJoliet->RootDir.DirRec.bInterleaveGapSize = 0;
        pJoliet->RootDir.DirRec.VolumeSeqNo.be     = RT_H2BE_U16_C(1);
        pJoliet->RootDir.DirRec.VolumeSeqNo.le     = RT_H2LE_U16_C(1);
        pJoliet->RootDir.DirRec.bFileIdLength      = 1;
        pJoliet->RootDir.DirRec.achFileId[0]       = 0x00;
    }

    return VINF_SUCCESS;
}


/**
 * Finalizes the image.
 *
 * @returns IPRT status code.
 * @param   hIsoMaker       The ISO maker handle.
 */
RTDECL(int) RTFsIsoMakerFinalize(RTFSISOMAKER hIsoMaker)
{
    PRTFSISOMAKERINT pThis = hIsoMaker;
    RTFSISOMAKER_ASSERT_VALID_HANDLE_RET(pThis);
    AssertReturn(!pThis->fFinalized, VERR_WRONG_ORDER);

    /*
     * Remove orphaned objects and allocate volume descriptors.
     */
    int rc = rtFsIsoMakerFinalizeRemoveOrphans(pThis);
    if (RT_FAILURE(rc))
        return rc;
    AssertReturn(pThis->cObjects > 0, VERR_NO_DATA);
    AssertReturn(pThis->PrimaryIso.pRoot || pThis->PrimaryIso.uLevel == 0, VERR_NO_DATA);
    AssertReturn(pThis->Joliet.pRoot     || pThis->Joliet.uLevel     == 0, VERR_NO_DATA);

    rc = rtFsIsoMakerFinalizePrepVolumeDescriptors(pThis);
    if (RT_FAILURE(rc))
        return rc;

    /*
     * If there is any boot related stuff to be included, it ends up right after
     * the descriptors.
     */
    uint64_t offData = _32K + pThis->cVolumeDescriptors * RTFSISOMAKER_SECTOR_SIZE;
    rc = rtFsIsoMakerFinalizeBootStuffPart1(pThis);
    if (RT_SUCCESS(rc))
    {
        /*
         * Directories and path tables comes next.
         */
        rc = rtFsIsoMakerFinalizeDirectories(pThis, &offData);
        if (RT_SUCCESS(rc))
        {
            /*
             * Then we store the file data.
             */
            rc = rtFsIsoMakerFinalizeData(pThis, &offData);
            if (RT_SUCCESS(rc))
            {
                pThis->cbFinalizedImage = offData;

                /*
                 * Do a 2nd pass over the boot stuff to finalize locations.
                 */
                rc = rtFsIsoMakerFinalizeBootStuffPart2(pThis);
                if (RT_SUCCESS(rc))
                {
                    /*
                     * Finally, finalize the volume descriptors as they depend on some of the
                     * block allocations done in the previous steps.
                     */
                    rc = rtFsIsoMakerFinalizeVolumeDescriptors(pThis);
                    if (RT_SUCCESS(rc))
                    {
                        pThis->fFinalized = true;
                        return VINF_SUCCESS;
                    }
                }
            }
        }
    }
    return rc;
}





/*
 *
 * Image I/O.
 * Image I/O.
 * Image I/O.
 *
 */

/**
 * @interface_method_impl{RTVFSOBJOPS,pfnClose}
 */
static DECLCALLBACK(int) rtFsIsoMakerOutFile_Close(void *pvThis)
{
    PRTFSISOMAKEROUTPUTFILE pThis = (PRTFSISOMAKEROUTPUTFILE)pvThis;

    RTFsIsoMakerRelease(pThis->pIsoMaker);
    pThis->pIsoMaker = NULL;

    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{RTVFSOBJOPS,pfnQueryInfo}
 */
static DECLCALLBACK(int) rtFsIsoMakerOutFile_QueryInfo(void *pvThis, PRTFSOBJINFO pObjInfo, RTFSOBJATTRADD enmAddAttr)
{
    PRTFSISOMAKEROUTPUTFILE pThis     = (PRTFSISOMAKEROUTPUTFILE)pvThis;
    PRTFSISOMAKERINT        pIsoMaker = pThis->pIsoMaker;


    pObjInfo->cbObject         = pIsoMaker->cbFinalizedImage;
    pObjInfo->cbAllocated      = pIsoMaker->cbFinalizedImage;
    pObjInfo->AccessTime       = pIsoMaker->ImageCreationTime;
    pObjInfo->ModificationTime = pIsoMaker->ImageCreationTime;
    pObjInfo->ChangeTime       = pIsoMaker->ImageCreationTime;
    pObjInfo->BirthTime        = pIsoMaker->ImageCreationTime;
    pObjInfo->Attr.fMode       = 0444 | RTFS_TYPE_FILE | RTFS_DOS_READONLY;

    switch (enmAddAttr)
    {
        case RTFSOBJATTRADD_NOTHING:
            enmAddAttr = RTFSOBJATTRADD_UNIX;
            /* fall thru */
        case RTFSOBJATTRADD_UNIX:
            pObjInfo->Attr.u.Unix.uid           = NIL_RTUID;
            pObjInfo->Attr.u.Unix.gid           = NIL_RTGID;
            pObjInfo->Attr.u.Unix.cHardlinks    = 1;
            pObjInfo->Attr.u.Unix.INodeIdDevice = 0;
            pObjInfo->Attr.u.Unix.INodeId       = 0;
            pObjInfo->Attr.u.Unix.fFlags        = 0;
            pObjInfo->Attr.u.Unix.GenerationId  = 0;
            pObjInfo->Attr.u.Unix.Device        = 0;
            break;

        case RTFSOBJATTRADD_UNIX_OWNER:
            pObjInfo->Attr.u.UnixOwner.uid = NIL_RTUID;
            pObjInfo->Attr.u.UnixOwner.szName[0] = '\0';
            break;

        case RTFSOBJATTRADD_UNIX_GROUP:
            pObjInfo->Attr.u.UnixGroup.gid = NIL_RTGID;
            pObjInfo->Attr.u.UnixGroup.szName[0] = '\0';
            break;

        case RTFSOBJATTRADD_EASIZE:
            pObjInfo->Attr.u.EASize.cb = 0;
            break;

        default:
            AssertFailedReturn(VERR_INVALID_PARAMETER);
    }
    pObjInfo->Attr.enmAdditional = enmAddAttr;

    return VINF_SUCCESS;
}


/**
 * Produces the content of a TRANS.TBL file as a memory file.
 *
 * @returns IPRT status code.
 * @param   pThis       The ISO maker output file instance.  The file is
 *                      returned as pThis->hVfsSrcFile.
 * @param   pFile       The TRANS.TBL file.
 */
static int rtFsIsoMakerOutFile_ProduceTransTbl(PRTFSISOMAKEROUTPUTFILE pThis, PRTFSISOMAKERFILE pFile)
{
    /*
     * Create memory file instance.
     */
    RTVFSFILE hVfsFile;
    int rc = RTVfsMemFileCreate(NIL_RTVFSIOSTREAM, pFile->cbData, &hVfsFile);
    AssertRCReturn(rc, rc);

    /*
     * Produce the file content.
     */
    PRTFSISOMAKERNAME *ppChild = pFile->u.pTransTblDir->pDir->papChildren;
    uint32_t           cLeft   = pFile->u.pTransTblDir->pDir->cChildren;
    while (cLeft-- > 0)
    {
        PRTFSISOMAKERNAME pChild = *ppChild++;
        if (pChild->cchTransNm)
        {
            /** @todo TRANS.TBL codeset, currently using UTF-8 which is probably not it.
             *        However, nobody uses this stuff any more, so who cares. */
            char   szEntry[RTFSISOMAKER_MAX_NAME_BUF * 2 + 128];
            size_t cchEntry = RTStrPrintf(szEntry, sizeof(szEntry), "%c %-*s\t%s\n", pChild->pDir ? 'D' : 'F',
                                          RTFSISOMAKER_TRANS_TBL_LEFT_PAD, pChild->szName, pChild->pszTransNm);
            rc = RTVfsFileWrite(hVfsFile, szEntry, cchEntry, NULL);
            if (RT_FAILURE(rc))
            {
                RTVfsFileRelease(hVfsFile);
                return rc;
            }
        }
    }

    /*
     * Check that the size matches our estimate.
     */
    uint64_t cbResult = 0;
    rc = RTVfsFileGetSize(hVfsFile, &cbResult);
    if (RT_SUCCESS(rc) && cbResult == pFile->cbData)
    {
        pThis->hVfsSrcFile = hVfsFile;
        return VINF_SUCCESS;
    }

    AssertMsgFailed(("rc=%Rrc, cbResult=%#RX64 cbData=%#RX64\n", rc, cbResult, pFile->cbData));
    RTVfsFileRelease(hVfsFile);
    return VERR_INTERNAL_ERROR_4;
}



/**
 * Reads file data.
 *
 * @returns IPRT status code
 * @param   pThis           The instance data for the VFS file.  We use this to
 *                          keep hints about where we are and we which source
 *                          file we've opened/created.
 * @param   pIsoMaker       The ISO maker instance.
 * @param   offUnsigned     The ISO image byte offset of the requested data.
 * @param   pbBuf           The output buffer.
 * @param   cbBuf           How much to read.
 * @param   pcbDone         Where to return how much was read.
 */
static int rtFsIsoMakerOutFile_ReadFileData(PRTFSISOMAKEROUTPUTFILE pThis, PRTFSISOMAKERINT pIsoMaker, uint64_t offUnsigned,
                                            uint8_t *pbBuf, size_t cbBuf, size_t *pcbDone)
{
    *pcbDone = 0;

    /*
     * Figure out which file.  We keep a hint in the instance.
     */
    uint64_t          offInFile;
    PRTFSISOMAKERFILE pFile = pThis->pFileHint;
    if (!pFile)
    {
        pFile = RTListGetFirst(&pIsoMaker->FinalizedFiles, RTFSISOMAKERFILE, FinalizedEntry);
        AssertReturn(pFile, VERR_INTERNAL_ERROR_2);
    }
    if ((offInFile = offUnsigned - pFile->offData) < RT_ALIGN_64(pFile->cbData, RTFSISOMAKER_SECTOR_SIZE))
    { /* hit */ }
    else if (offUnsigned >= pFile->offData)
    {
        /* Seek forwards. */
        do
        {
            pFile = RTListGetNext(&pIsoMaker->FinalizedFiles, pFile, RTFSISOMAKERFILE, FinalizedEntry);
            AssertReturn(pFile, VERR_INTERNAL_ERROR_3);
        } while ((offInFile = offUnsigned - pFile->offData) >= RT_ALIGN_64(pFile->cbData, RTFSISOMAKER_SECTOR_SIZE));
    }
    else
    {
        /* Seek backwards. */
        do
        {
            pFile = RTListGetPrev(&pIsoMaker->FinalizedFiles, pFile, RTFSISOMAKERFILE, FinalizedEntry);
            AssertReturn(pFile, VERR_INTERNAL_ERROR_3);
        } while ((offInFile = offUnsigned - pFile->offData) >= RT_ALIGN_64(pFile->cbData, RTFSISOMAKER_SECTOR_SIZE));
    }

    /*
     * Update the hint/current file.
     */
    if (pThis->pFileHint != pFile)
    {
        pThis->pFileHint = pFile;
        if (pThis->hVfsSrcFile != NIL_RTVFSFILE)
        {
            RTVfsFileRelease(pThis->hVfsSrcFile);
            pThis->hVfsSrcFile = NIL_RTVFSFILE;
        }
    }

    /*
     * Produce data bits according to the source type.
     */
    if (offInFile < pFile->cbData)
    {
        int    rc;
        size_t cbToRead = RT_MIN(cbBuf, pFile->cbData - offInFile);

        switch (pFile->enmSrcType)
        {
            case RTFSISOMAKERSRCTYPE_PATH:
                if (pThis->hVfsSrcFile == NIL_RTVFSFILE)
                {
                    rc = RTVfsChainOpenFile(pFile->u.pszSrcPath, RTFILE_O_READ | RTFILE_O_DENY_NONE | RTFILE_O_OPEN,
                                            &pThis->hVfsSrcFile, NULL, NULL);
                    AssertMsgRCReturn(rc, ("%s -> %Rrc\n", pFile->u.pszSrcPath, rc), rc);
                }
                rc = RTVfsFileReadAt(pThis->hVfsSrcFile, offInFile, pbBuf, cbToRead, NULL);
                AssertRC(rc);
                break;

            case RTFSISOMAKERSRCTYPE_VFS_FILE:
                rc = RTVfsFileReadAt(pFile->u.hVfsFile, offInFile, pbBuf, cbToRead, NULL);
                AssertRC(rc);
                break;

            case RTFSISOMAKERSRCTYPE_COMMON:
                rc = RTVfsFileReadAt(pIsoMaker->paCommonSources[pFile->u.Common.idxSrc],
                                     pFile->u.Common.offData + offInFile, pbBuf, cbToRead, NULL);
                AssertRC(rc);
                break;

            case RTFSISOMAKERSRCTYPE_TRANS_TBL:
                if (pThis->hVfsSrcFile == NIL_RTVFSFILE)
                {
                    rc = rtFsIsoMakerOutFile_ProduceTransTbl(pThis, pFile);
                    AssertRCReturn(rc, rc);
                }
                rc = RTVfsFileReadAt(pThis->hVfsSrcFile, offInFile, pbBuf, cbToRead, NULL);
                AssertRC(rc);
                break;

            default:
                AssertFailedReturn(VERR_INTERNAL_ERROR_5);
        }
        if (RT_FAILURE(rc))
            return rc;
        *pcbDone = cbToRead;

        /*
         * Do boot info table patching.
         */
        if (   pFile->pBootInfoTable
            && offInFile            < 64
            && offInFile + cbToRead > 8)
        {
            size_t offInBuf = offInFile <  8 ? 8 - (size_t)offInFile : 0;
            size_t offInTab = offInFile <= 8 ? 0 : (size_t)offInFile - 8;
            size_t cbToCopy = RT_MIN(sizeof(*pFile->pBootInfoTable) - offInTab, cbToRead - offInBuf);
            memcpy(&pbBuf[offInBuf], (uint8_t *)pFile->pBootInfoTable + offInTab, cbToCopy);
        }

        /*
         * Check if we're into the zero padding at the end of the file now.
         */
        if (   cbToRead < cbBuf
            && (pFile->cbData & RTFSISOMAKER_SECTOR_OFFSET_MASK)
            && offInFile + cbToRead == pFile->cbData)
        {
            cbBuf -= cbToRead;
            pbBuf += cbToRead;
            size_t cbZeros = RT_MIN(cbBuf, RTFSISOMAKER_SECTOR_SIZE - (pFile->cbData & RTFSISOMAKER_SECTOR_OFFSET_MASK));
            memset(pbBuf, 0, cbZeros);
            *pcbDone += cbZeros;
        }
    }
    else
    {
        size_t cbZeros = RT_MIN(cbBuf, RT_ALIGN_64(pFile->cbData, RTFSISOMAKER_SECTOR_SIZE) - offInFile);
        memset(pbBuf, 0, cbZeros);
        *pcbDone = cbZeros;
    }
    return VINF_SUCCESS;
}


/**
 * Generates ISO-9660 path table record into the specified buffer.
 *
 * @returns Number of bytes copied into the buffer.
 * @param   pName       The directory namespace node.
 * @param   fUnicode    Set if the name should be translated to big endian
 *                      UTF-16 / UCS-2, i.e. we're in the joliet namespace.
 * @param   pbBuf       The buffer.  This is large enough to hold the path
 *                      record (use RTFSISOMAKER_CALC_PATHREC_SIZE) and a zero
 *                      RTUTF16 terminator if @a fUnicode is true.
 */
static uint32_t rtFsIsoMakerOutFile_GeneratePathRec(PRTFSISOMAKERNAME pName, bool fUnicode, bool fLittleEndian, uint8_t *pbBuf)
{
    PISO9660PATHREC pPathRec = (PISO9660PATHREC)pbBuf;
    pPathRec->cbDirId   = pName->cbNameInDirRec;
    pPathRec->cbExtAttr = 0;
    if (fLittleEndian)
    {
        pPathRec->offExtent   = RT_H2LE_U32(pName->pDir->offDir / RTFSISOMAKER_SECTOR_SIZE);
        pPathRec->idParentRec = RT_H2LE_U16(pName->pParent ? pName->pParent->pDir->idPathTable : 1);
    }
    else
    {
        pPathRec->offExtent   = RT_H2BE_U32(pName->pDir->offDir / RTFSISOMAKER_SECTOR_SIZE);
        pPathRec->idParentRec = RT_H2BE_U16(pName->pParent ? pName->pParent->pDir->idPathTable : 1);
    }
    if (!fUnicode)
    {
        memcpy(&pPathRec->achDirId[0], pName->szName, pName->cbNameInDirRec);
        if (pName->cbNameInDirRec & 1)
            pPathRec->achDirId[pName->cbNameInDirRec] = '\0';
    }
    else
    {
        /* Caller made sure there is space for a zero terminator character. */
        PRTUTF16 pwszTmp   = (PRTUTF16)&pPathRec->achDirId[0];
        size_t   cwcResult = 0;
        int rc = RTStrToUtf16BigEx(pName->szName, RTSTR_MAX, &pwszTmp, pName->cbNameInDirRec / sizeof(RTUTF16) + 1, &cwcResult);
        AssertRC(rc);
        Assert(   cwcResult * sizeof(RTUTF16) == pName->cbNameInDirRec
               || (!pName->pParent && cwcResult == 0 && pName->cbNameInDirRec == 1) );

    }
    return RTFSISOMAKER_CALC_PATHREC_SIZE(pName->cbNameInDirRec);
}


/**
 * Deals with situations where the destination buffer doesn't cover the whole
 * path table record.
 *
 * @returns Number of bytes copied into the buffer.
 * @param   pName       The directory namespace node.
 * @param   fUnicode    Set if the name should be translated to big endian
 *                      UTF-16 / UCS-2, i.e. we're in the joliet namespace.
 * @param   offInRec    The offset into the path table record.
 * @param   pbBuf       The buffer.
 * @param   cbBuf       The buffer size.
 */
static uint32_t rtFsIsoMakerOutFile_GeneratePathRecPartial(PRTFSISOMAKERNAME pName, bool fUnicode, bool fLittleEndian,
                                                           uint32_t offInRec, uint8_t *pbBuf, size_t cbBuf)
{
    uint8_t abTmpRec[256];
    size_t cbToCopy = rtFsIsoMakerOutFile_GeneratePathRec(pName, fUnicode, fLittleEndian, abTmpRec);
    cbToCopy = RT_MIN(cbBuf, cbToCopy - offInRec);
    memcpy(pbBuf, &abTmpRec[offInRec], cbToCopy);
    return (uint32_t)cbToCopy;
}


/**
 * Generate path table records.
 *
 * This will generate record up to the end of the table.  However, it will not
 * supply the zero padding in the last sector, the caller is expected to take
 * care of that.
 *
 * @returns Number of bytes written to the buffer.
 * @param   ppDirHint       Pointer to the directory hint for the namespace.
 * @param   pFinalizedDirs  The finalized directory data for the namespace.
 * @param   fUnicode        Set if the name should be translated to big endian
 *                          UTF-16 / UCS-2, i.e. we're in the joliet namespace.
 * @param   fLittleEndian   Set if we're generating little endian records, clear
 *                          if big endian records.
 * @param   offInTable      Offset into the path table.
 * @param   pbBuf           The output buffer.
 * @param   cbBuf           The buffer size.
 */
static size_t rtFsIsoMakerOutFile_ReadPathTable(PRTFSISOMAKERNAMEDIR *ppDirHint, PRTFSISOMAKERFINALIZEDDIRS pFinalizedDirs,
                                                bool fUnicode, bool fLittleEndian, uint32_t offInTable,
                                                uint8_t *pbBuf, size_t cbBuf)
{
    /*
     * Figure out which directory to start with.  We keep a hint in the instance.
     */
    PRTFSISOMAKERNAMEDIR pDir = *ppDirHint;
    if (!pDir)
    {
        pDir = RTListGetFirst(&pFinalizedDirs->FinalizedDirs, RTFSISOMAKERNAMEDIR, FinalizedEntry);
        AssertReturnStmt(pDir, *pbBuf = 0xff, 1);
    }
    if (offInTable - pDir->offPathTable < RTFSISOMAKER_CALC_PATHREC_SIZE(pDir->pName->cbNameInDirRec))
    { /* hit */ }
    /* Seek forwards: */
    else if (offInTable > pDir->offPathTable)
        do
        {
            pDir = RTListGetNext(&pFinalizedDirs->FinalizedDirs, pDir, RTFSISOMAKERNAMEDIR, FinalizedEntry);
            AssertReturnStmt(pDir, *pbBuf = 0xff, 1);
        } while (offInTable - pDir->offPathTable >= RTFSISOMAKER_CALC_PATHREC_SIZE(pDir->pName->cbNameInDirRec));
    /* Back to the start: */
    else if (offInTable == 0)
    {
        pDir = RTListGetFirst(&pFinalizedDirs->FinalizedDirs, RTFSISOMAKERNAMEDIR, FinalizedEntry);
        AssertReturnStmt(pDir, *pbBuf = 0xff, 1);
    }
    /* Seek backwards: */
    else
        do
        {
            pDir = RTListGetPrev(&pFinalizedDirs->FinalizedDirs, pDir, RTFSISOMAKERNAMEDIR, FinalizedEntry);
            AssertReturnStmt(pDir, *pbBuf = 0xff, 1);
        } while (offInTable - pDir->offPathTable >= RTFSISOMAKER_CALC_PATHREC_SIZE(pDir->pName->cbNameInDirRec));

    /*
     * Generate content.
     */
    size_t cbDone = 0;
    while (   cbBuf > 0
           && pDir)
    {
        PRTFSISOMAKERNAME pName = pDir->pName;
        uint8_t           cbRec = RTFSISOMAKER_CALC_PATHREC_SIZE(pName->cbNameInDirRec);
        uint32_t          cbCopied;
        if (   offInTable == pDir->offPathTable
            && cbBuf      >= cbRec + fUnicode * 2U)
            cbCopied = rtFsIsoMakerOutFile_GeneratePathRec(pName, fUnicode, fLittleEndian, pbBuf);
        else
            cbCopied = rtFsIsoMakerOutFile_GeneratePathRecPartial(pName, fUnicode, fLittleEndian,
                                                                  offInTable - pDir->offPathTable, pbBuf, cbBuf);
        cbDone     += cbCopied;
        offInTable += cbCopied;
        pbBuf      += cbCopied;
        cbBuf      -= cbCopied;
        pDir = RTListGetNext(&pFinalizedDirs->FinalizedDirs, pDir, RTFSISOMAKERNAMEDIR, FinalizedEntry);
    }

    /*
     * Update the hint.
     */
    *ppDirHint = pDir;

    return cbDone;
}


/**
 * Generates ISO-9660 directory record into the specified buffer.
 *
 * @returns Number of bytes copied into the buffer.
 * @param   pName       The namespace node.
 * @param   fUnicode    Set if the name should be translated to big endian
 *                      UTF-16BE / UCS-2BE, i.e. we're in the joliet namespace.
 * @param   pbBuf       The buffer.  This is at least pName->cbDirRec bytes big.
 */
static uint32_t rtFsIsoMakerOutFile_GenerateDirRec(PRTFSISOMAKERNAME pName, bool fUnicode, uint8_t *pbBuf)
{
    /*
     * Emit a standard ISO-9660 directory record.
     */
    PISO9660DIRREC          pDirRec = (PISO9660DIRREC)pbBuf;
    PCRTFSISOMAKEROBJ       pObj    = pName->pObj;
    PCRTFSISOMAKERNAMEDIR   pDir    = pName->pDir;
    if (pDir)
    {
        pDirRec->offExtent.be       = RT_H2BE_U32(pDir->offDir / RTFSISOMAKER_SECTOR_SIZE);
        pDirRec->offExtent.le       = RT_H2LE_U32(pDir->offDir / RTFSISOMAKER_SECTOR_SIZE);
        pDirRec->cbData.be          = RT_H2BE_U32(pDir->cbDir);
        pDirRec->cbData.le          = RT_H2LE_U32(pDir->cbDir);
        pDirRec->fFileFlags         = ISO9660_FILE_FLAGS_DIRECTORY;
    }
    else if (pObj->enmType == RTFSISOMAKEROBJTYPE_FILE)
    {
        PRTFSISOMAKERFILE pFile = (PRTFSISOMAKERFILE)pObj;
        pDirRec->offExtent.be       = RT_H2BE_U32(pFile->offData / RTFSISOMAKER_SECTOR_SIZE);
        pDirRec->offExtent.le       = RT_H2LE_U32(pFile->offData / RTFSISOMAKER_SECTOR_SIZE);
        pDirRec->cbData.be          = RT_H2BE_U32(pFile->cbData);
        pDirRec->cbData.le          = RT_H2LE_U32(pFile->cbData);
        pDirRec->fFileFlags         = 0;
    }
    else
    {
        pDirRec->offExtent.be       = 0;
        pDirRec->offExtent.le       = 0;
        pDirRec->cbData.be          = 0;
        pDirRec->cbData.le          = 0;
        pDirRec->fFileFlags         = 0;
    }
    rtFsIsoMakerTimespecToIso9660RecTimestamp(&pObj->BirthTime, &pDirRec->RecTime);

    pDirRec->cbDirRec               = pName->cbDirRec;
    pDirRec->cExtAttrBlocks         = 0;
    pDirRec->bFileUnitSize          = 0;
    pDirRec->bInterleaveGapSize     = 0;
    pDirRec->VolumeSeqNo.be         = RT_H2BE_U16_C(1);
    pDirRec->VolumeSeqNo.le         = RT_H2LE_U16_C(1);
    pDirRec->bFileIdLength          = pName->cbNameInDirRec;

    if (!fUnicode)
    {
        memcpy(&pDirRec->achFileId[0], pName->szName, pName->cbNameInDirRec);
        if (!(pName->cbNameInDirRec & 1))
            pDirRec->achFileId[pName->cbNameInDirRec] = '\0';
    }
    else
    {
        /* Convert to big endian UTF-16.  We're using a separate buffer here
           because of zero terminator (none in pDirRec) and misalignment. */
        RTUTF16  wszTmp[128];
        PRTUTF16 pwszTmp = &wszTmp[0];
        size_t   cwcResult = 0;
        int rc = RTStrToUtf16BigEx(pName->szName, RTSTR_MAX, &pwszTmp, RT_ELEMENTS(wszTmp), &cwcResult);
        AssertRC(rc);
        Assert(   cwcResult * sizeof(RTUTF16) == pName->cbNameInDirRec
               || (!pName->pParent && cwcResult == 0 && pName->cbNameInDirRec == 1) );
        memcpy(&pDirRec->achFileId[0], pwszTmp, pName->cbNameInDirRec);
        pDirRec->achFileId[pName->cbNameInDirRec] = '\0';
    }

    /*
     * Rock ridge fields if enabled.
     */
    /** @todo rock ridge. */

    /*
     * Do end-of-sector zero padding.
     */
    if (pName->cbDirRecWithZeroPad == pName->cbDirRec)
    { /* likely */ }
    else
    {
        Assert(pName->cbDirRecWithZeroPad >= pName->cbDirRec);
        memset((uint8_t *)pDirRec + pName->cbDirRec, 0, pName->cbDirRecWithZeroPad - pName->cbDirRec);
    }

    return pName->cbDirRecWithZeroPad;
}


/**
 * Deals with situations where the destination buffer doesn't cover the whole
 * directory record.
 *
 * @returns Number of bytes copied into the buffer.
 * @param   pName       The namespace node.
 * @param   fUnicode    Set if the name should be translated to big endian
 *                      UTF-16 / UCS-2, i.e. we're in the joliet namespace.
 * @param   off         The offset into the directory record.
 * @param   pbBuf       The buffer.
 * @param   cbBuf       The buffer size.
 */
static uint32_t rtFsIsoMakerOutFile_GenerateDirRecPartial(PRTFSISOMAKERNAME pName, bool fUnicode,
                                                          uint32_t off, uint8_t *pbBuf, size_t cbBuf)
{
    uint8_t abTmpBuf[256 * 2];
    Assert(off < pName->cbDirRecWithZeroPad);
    Assert(pName->cbDirRecWithZeroPad <= sizeof(abTmpBuf));

    size_t cbToCopy = rtFsIsoMakerOutFile_GenerateDirRec(pName, fUnicode, pbBuf);
    cbToCopy = RT_MIN(cbBuf, cbToCopy - off);
    memcpy(pbBuf, &abTmpBuf[off], cbToCopy);
    return (uint32_t)cbToCopy;
}


/**
 * Generate a '.' or '..' directory record.
 *
 * This is the same as rtFsIsoMakerOutFile_GenerateDirRec, but with the filename
 * reduced to 1 byte.
 *
 * @returns Number of bytes copied into the buffer.
 * @param   pName       The directory namespace node.
 * @param   fUnicode    Set if the name should be translated to big endian
 *                      UTF-16 / UCS-2, i.e. we're in the joliet namespace.
 * @param   bDirId      The directory ID (0x00 or 0x01).
 * @param   off         The offset into the directory record.
 * @param   pbBuf       The buffer.
 * @param   cbBuf       The buffer size.
 */
static uint32_t rtFsIsoMakerOutFile_GenerateSpecialDirRec(PRTFSISOMAKERNAME pName, bool fUnicode, uint8_t bDirId,
                                                          uint32_t off, uint8_t *pbBuf, size_t cbBuf)
{
    Assert(off < pName->cbDirRec);
    Assert(pName->pDir);

    /* Generate a regular directory record. */
    uint8_t abTmpBuf[256 * 2];
    Assert(off < pName->cbDirRecWithZeroPad);
    size_t cbToCopy = rtFsIsoMakerOutFile_GenerateDirRec(pName, fUnicode, abTmpBuf);
    Assert(cbToCopy == pName->cbDirRecWithZeroPad);
    cbToCopy = pName->cbDirRec;

    /* Replace the filename part. */
    PISO9660DIRREC pDirRec = (PISO9660DIRREC)abTmpBuf;
    if (pDirRec->bFileIdLength != 1)
    {
        uint8_t offSysUse = pDirRec->bFileIdLength + !(pDirRec->bFileIdLength & 1) + RT_OFFSETOF(ISO9660DIRREC, achFileId);
        uint8_t cbSysUse  = pDirRec->cbDirRec - offSysUse;
        if (cbSysUse > 0)
            memmove(&pDirRec->achFileId[1], &pbBuf[offSysUse], cbSysUse);
        pDirRec->bFileIdLength = 1;
        cbToCopy = RT_OFFSETOF(ISO9660DIRREC, achFileId) + 1 + cbSysUse;
        pDirRec->cbDirRec = (uint8_t)cbToCopy;
    }
    pDirRec->achFileId[0] = bDirId;

    /* Do the copying. */
    cbToCopy = RT_MIN(cbBuf, cbToCopy - off);
    memcpy(pbBuf, &abTmpBuf[off], cbToCopy);
    return (uint32_t)cbToCopy;
}


/**
 * Read directory records.
 *
 * This locates the directory at @a offUnsigned and generates directory records
 * for it.  Caller must repeat the call to get directory entries for the next
 * directory should there be desire for that.
 *
 * @returns Number of bytes copied into @a pbBuf.
 * @param   ppDirHint       Pointer to the directory hint for the namespace.
 * @param   pIsoMaker       The ISO maker instance.
 * @param   pFinalizedDirs  The finalized directory data for the namespace.
 * @param   fUnicode        Set if the name should be translated to big endian
 *                          UTF-16 / UCS-2, i.e. we're in the joliet namespace.
 * @param   offUnsigned     The ISO image byte offset of the requested data.
 * @param   pbBuf           The output buffer.
 * @param   cbBuf           How much to read.
 */
static size_t rtFsIsoMakerOutFile_ReadDirRecords(PRTFSISOMAKERNAMEDIR *ppDirHint, PRTFSISOMAKERFINALIZEDDIRS pFinalizedDirs,
                                                 bool fUnicode, uint64_t offUnsigned, uint8_t *pbBuf, size_t cbBuf)
{
    /*
     * Figure out which directory.  We keep a hint in the instance.
     */
    uint64_t             offInDir64;
    PRTFSISOMAKERNAMEDIR pDir = *ppDirHint;
    if (!pDir)
    {
        pDir = RTListGetFirst(&pFinalizedDirs->FinalizedDirs, RTFSISOMAKERNAMEDIR, FinalizedEntry);
        AssertReturnStmt(pDir, *pbBuf = 0xff, 1);
    }
    if ((offInDir64 = offUnsigned - pDir->offDir) < RT_ALIGN_32(pDir->cbDir, RTFSISOMAKER_SECTOR_SIZE))
    { /* hit */ }
    /* Seek forwards: */
    else if (offUnsigned > pDir->offDir)
        do
        {
            pDir = RTListGetNext(&pFinalizedDirs->FinalizedDirs, pDir, RTFSISOMAKERNAMEDIR, FinalizedEntry);
            AssertReturnStmt(pDir, *pbBuf = 0xff, 1);
        } while ((offInDir64 = offUnsigned - pDir->offDir) >= RT_ALIGN_32(pDir->cbDir, RTFSISOMAKER_SECTOR_SIZE));
    /* Back to the start: */
    else if (pFinalizedDirs->offDirs / RTFSISOMAKER_SECTOR_SIZE == offUnsigned / RTFSISOMAKER_SECTOR_SIZE)
    {
        pDir = RTListGetFirst(&pFinalizedDirs->FinalizedDirs, RTFSISOMAKERNAMEDIR, FinalizedEntry);
        AssertReturnStmt(pDir, *pbBuf = 0xff, 1);
    }
    /* Seek backwards: */
    else
        do
        {
            pDir = RTListGetPrev(&pFinalizedDirs->FinalizedDirs, pDir, RTFSISOMAKERNAMEDIR, FinalizedEntry);
            AssertReturnStmt(pDir, *pbBuf = 0xff, 1);
        } while ((offInDir64 = offUnsigned - pDir->offDir) >= RT_ALIGN_32(pDir->cbDir, RTFSISOMAKER_SECTOR_SIZE));

    /*
     * Update the hint.
     */
    *ppDirHint = pDir;

    /*
     * Generate content.
     */
    size_t cbDone = 0;
    uint32_t offInDir = (uint32_t)offInDir64;
    if (offInDir < pDir->cbDir)
    {
        PRTFSISOMAKERNAME   pDirName      = pDir->pName;
        PRTFSISOMAKERNAME   pParentName   = pDirName->pParent ? pDirName->pParent : pDirName;
        uint32_t            cbSpecialRecs = (uint32_t)pDir->cbDirRec00 + pDir->cbDirRec01;

        /*
         * Special '.' and/or '..' entries requested.
         */
        uint32_t iChild;
        if (offInDir < cbSpecialRecs)
        {
            /* do '.' */
            if (offInDir < pDir->cbDirRec00)
            {
                uint32_t cbCopied = rtFsIsoMakerOutFile_GenerateSpecialDirRec(pDirName, fUnicode, 0, offInDir, pbBuf, cbBuf);
                cbDone   += cbCopied;
                offInDir += cbCopied;
                pbBuf    += cbCopied;
                cbBuf    -= cbCopied;
            }

            /* do '..' */
            if (cbBuf > 0)
            {
                uint32_t cbCopied = rtFsIsoMakerOutFile_GenerateSpecialDirRec(pParentName, fUnicode, 1,
                                                                              offInDir - pDir->cbDirRec00, pbBuf, cbBuf);
                cbDone   += cbCopied;
                offInDir += cbCopied;
                pbBuf    += cbCopied;
                cbBuf    -= cbCopied;
            }

            iChild = 0;
        }
        /*
         * Locate the directory entry we should start with.  We can do this
         * using binary searching on offInDir.
         */
        else
        {
            /** @todo binary search   */
            iChild = 0;
            while (iChild < pDir->cChildren)
            {
                PRTFSISOMAKERNAME pChild = pDir->papChildren[iChild];
                if ((offInDir - pChild->offDirRec) < pChild->cbDirRecWithZeroPad)
                    break;
                iChild++;
            }
            AssertReturnStmt(iChild < pDir->cChildren, *pbBuf = 0xff, 1);
        }

        /*
         * Normal directory entries.
         */
        while (   cbBuf > 0
               && iChild < pDir->cChildren)
        {
            PRTFSISOMAKERNAME pChild = pDir->papChildren[iChild];
            uint32_t cbCopied;
            if (   offInDir == pChild->offDirRec
                && cbBuf    >= pChild->cbDirRecWithZeroPad)
                cbCopied = rtFsIsoMakerOutFile_GenerateDirRec(pChild, fUnicode, pbBuf);
            else
                cbCopied = rtFsIsoMakerOutFile_GenerateDirRecPartial(pChild, fUnicode, offInDir - pChild->offDirRec, pbBuf, cbBuf);

            cbDone   += cbCopied;
            offInDir += cbCopied;
            pbBuf    += cbCopied;
            cbBuf    -= cbCopied;
            iChild++;
        }

        /*
         * Check if we're into the zero padding at the end of the directory now.
         */
        if (   cbBuf > 0
            && iChild >= pDir->cChildren)
        {
            size_t cbZeros = RT_MIN(cbBuf, RTFSISOMAKER_SECTOR_SIZE - (pDir->cbDir & RTFSISOMAKER_SECTOR_OFFSET_MASK));
            memset(pbBuf, 0, cbZeros);
            cbDone += cbZeros;
        }
    }
    else
    {
        cbDone = RT_MIN(cbBuf, RT_ALIGN_32(pDir->cbDir, RTFSISOMAKER_SECTOR_SIZE) - offInDir);
        memset(pbBuf, 0, cbDone);
    }

    return cbDone;
}


/**
 * Read directory records or path table records.
 *
 * Will not necessarily fill the entire buffer.  Caller must call again to get
 * more.
 *
 * @returns Number of bytes copied into @a pbBuf.
 * @param   ppDirHint       Pointer to the directory hint for the namespace.
 * @param   pIsoMaker       The ISO maker instance.
 * @param   pNamespace      The namespace.
 * @param   pFinalizedDirs  The finalized directory data for the namespace.
 * @param   offUnsigned     The ISO image byte offset of the requested data.
 * @param   pbBuf           The output buffer.
 * @param   cbBuf           How much to read.
 */
static size_t rtFsIsoMakerOutFile_ReadDirStructures(PRTFSISOMAKERNAMEDIR *ppDirHint, PRTFSISOMAKERNAMESPACE pNamespace,
                                                    PRTFSISOMAKERFINALIZEDDIRS pFinalizedDirs,
                                                    uint64_t offUnsigned, uint8_t *pbBuf, size_t cbBuf)
{
    if (offUnsigned < pFinalizedDirs->offPathTableL)
        return rtFsIsoMakerOutFile_ReadDirRecords(ppDirHint, pFinalizedDirs,
                                                  pNamespace->fNamespace == RTFSISOMAKER_NAMESPACE_JOLIET,
                                                  offUnsigned, pbBuf, cbBuf);

    uint64_t offInTable;
    if ((offInTable = offUnsigned - pFinalizedDirs->offPathTableL) < pFinalizedDirs->cbPathTable)
        return rtFsIsoMakerOutFile_ReadPathTable(ppDirHint, pFinalizedDirs,
                                                 pNamespace->fNamespace == RTFSISOMAKER_NAMESPACE_JOLIET,
                                                 true /*fLittleEndian*/, (uint32_t)offInTable, pbBuf, cbBuf);

    if ((offInTable = offUnsigned - pFinalizedDirs->offPathTableM) < pFinalizedDirs->cbPathTable)
        return rtFsIsoMakerOutFile_ReadPathTable(ppDirHint, pFinalizedDirs,
                                                 pNamespace->fNamespace == RTFSISOMAKER_NAMESPACE_JOLIET,
                                                 false /*fLittleEndian*/, (uint32_t)offInTable, pbBuf, cbBuf);

    /* ASSUME we're in the zero padding at the end of a path table. */
    Assert(   offUnsigned - pFinalizedDirs->offPathTableL <  RT_ALIGN_32(pFinalizedDirs->cbPathTable, RTFSISOMAKER_SECTOR_SIZE)
           || offUnsigned - pFinalizedDirs->offPathTableM <  RT_ALIGN_32(pFinalizedDirs->cbPathTable, RTFSISOMAKER_SECTOR_SIZE));
    size_t cbZeros = RT_MIN(cbBuf, RTFSISOMAKER_SECTOR_SIZE - ((size_t)offUnsigned & RTFSISOMAKER_SECTOR_OFFSET_MASK));
    memset(pbBuf, 0, cbZeros);
    return cbZeros;
}



/**
 * @interface_method_impl{RTVFSIOSTREAMOPS,pfnRead}
 */
static DECLCALLBACK(int) rtFsIsoMakerOutFile_Read(void *pvThis, RTFOFF off, PCRTSGBUF pSgBuf, bool fBlocking, size_t *pcbRead)
{
    PRTFSISOMAKEROUTPUTFILE pThis     = (PRTFSISOMAKEROUTPUTFILE)pvThis;
    PRTFSISOMAKERINT        pIsoMaker = pThis->pIsoMaker;
    size_t                  cbBuf     = pSgBuf->paSegs[0].cbSeg;
    uint8_t                *pbBuf     = (uint8_t *)pSgBuf->paSegs[0].pvSeg;

    Assert(pSgBuf->cSegs == 1);
    RT_NOREF(fBlocking);

    /*
     * Process the offset, checking for end-of-file.
     */
    uint64_t offUnsigned;
    if (off < 0)
        offUnsigned = pThis->offCurPos;
    else
        offUnsigned = (uint64_t)off;
    if (offUnsigned >= pIsoMaker->cbFinalizedImage)
    {
        if (*pcbRead)
        {
            *pcbRead = 0;
            return VINF_EOF;
        }
        return VERR_EOF;
    }
    if (   !pcbRead
        && pIsoMaker->cbFinalizedImage - offUnsigned < cbBuf)
        return VERR_EOF;

    /*
     * Produce the bytes.
     */
    int    rc     = VINF_SUCCESS;
    size_t cbRead = 0;
    while (cbBuf > 0)
    {
        size_t cbDone;

        /* Betting on there being more file data than metadata, thus doing the
           offset switch in decending order. */
        if (offUnsigned >= pIsoMaker->offFirstFile)
        {
            if (offUnsigned < pIsoMaker->cbFinalizedImage)
            {
                rc = rtFsIsoMakerOutFile_ReadFileData(pThis, pIsoMaker, offUnsigned, pbBuf, cbBuf, &cbDone);
                if (RT_FAILURE(rc))
                    break;
            }
            else
            {
                rc = pcbRead ? VINF_EOF : VERR_EOF;
                break;
            }
        }
        /*
         * Joliet directory structures.
         */
        else if (   offUnsigned >= pIsoMaker->JolietDirs.offDirs
                 && pIsoMaker->JolietDirs.offDirs < pIsoMaker->JolietDirs.offPathTableL)
            cbDone = rtFsIsoMakerOutFile_ReadDirStructures(&pThis->pDirHintJoliet, &pIsoMaker->Joliet, &pIsoMaker->JolietDirs,
                                                           offUnsigned, pbBuf, cbBuf);
        /*
         * Primary ISO directory structures.
         */
        else if (offUnsigned >= pIsoMaker->PrimaryIsoDirs.offDirs)
            cbDone = rtFsIsoMakerOutFile_ReadDirStructures(&pThis->pDirHintPrimaryIso, &pIsoMaker->PrimaryIso,
                                                           &pIsoMaker->PrimaryIsoDirs, offUnsigned, pbBuf, cbBuf);
        /** @todo Insert El Torito stuff here? Probably okay to let it be in the file
         *        area, right? */
        /*
         * Volume descriptors.
         */
        else if (offUnsigned >= _32K)
        {
            size_t offVolDescs = (size_t)offUnsigned - _32K;
            cbDone = RT_MIN(cbBuf, (pIsoMaker->cVolumeDescriptors * RTFSISOMAKER_SECTOR_SIZE) - offVolDescs);
            memcpy(pbBuf, &pIsoMaker->pbVolDescs[offVolDescs], cbDone);
        }
        /*
         * Zeros in the system area.
         */
        else if (offUnsigned >= pIsoMaker->cbSysArea)
        {
            cbDone = RT_MIN(cbBuf, _32K - (size_t)offUnsigned);
            memset(pbBuf, 0, cbDone);
        }
        /*
         * Actual data in the system area.
         */
        else
        {
            cbDone = RT_MIN(cbBuf, pIsoMaker->cbSysArea - (size_t)offUnsigned);
            memcpy(pbBuf, &pIsoMaker->pbSysArea[(size_t)offUnsigned], cbDone);
        }

        /*
         * Common advance.
         */
        cbRead      += cbDone;
        offUnsigned += cbDone;
        pbBuf       += cbDone;
        cbBuf       -= cbDone;
    }

    if (pcbRead)
        *pcbRead = cbRead;
    return rc;
}


/**
 * @interface_method_impl{RTVFSIOSTREAMOPS,pfnWrite}
 */
static DECLCALLBACK(int) rtFsIsoMakerOutFile_Write(void *pvThis, RTFOFF off, PCRTSGBUF pSgBuf, bool fBlocking, size_t *pcbWritten)
{
    RT_NOREF(pvThis, off, pSgBuf, fBlocking, pcbWritten);
    return VERR_WRITE_PROTECT;
}


/**
 * @interface_method_impl{RTVFSIOSTREAMOPS,pfnFlush}
 */
static DECLCALLBACK(int) rtFsIsoMakerOutFile_Flush(void *pvThis)
{
    RT_NOREF(pvThis);
    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{RTVFSIOSTREAMOPS,pfnPollOne}
 */
static DECLCALLBACK(int) rtFsIsoMakerOutFile_PollOne(void *pvThis, uint32_t fEvents, RTMSINTERVAL cMillies, bool fIntr,
                                                     uint32_t *pfRetEvents)
{
    NOREF(pvThis);
    return RTVfsUtilDummyPollOne(fEvents, cMillies, fIntr, pfRetEvents);
}


/**
 * @interface_method_impl{RTVFSIOSTREAMOPS,pfnTell}
 */
static DECLCALLBACK(int) rtFsIsoMakerOutFile_Tell(void *pvThis, PRTFOFF poffActual)
{
    PRTFSISOMAKEROUTPUTFILE pThis = (PRTFSISOMAKEROUTPUTFILE)pvThis;
    *poffActual = pThis->offCurPos;
    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{RTVFSIOSTREAMOPS,pfnSkip}
 */
static DECLCALLBACK(int) rtFsIsoMakerOutFile_Skip(void *pvThis, RTFOFF cb)
{
    RTFOFF offIgnored;
    return rtFsIsoMakerOutFile_Seek(pvThis, cb, RTFILE_SEEK_CURRENT, &offIgnored);
}


/**
 * @interface_method_impl{RTVFSOBJSETOPS,pfnMode}
 */
static DECLCALLBACK(int) rtFsIsoMakerOutFile_SetMode(void *pvThis, RTFMODE fMode, RTFMODE fMask)
{
    RT_NOREF(pvThis, fMode, fMask);
    return VERR_WRITE_PROTECT;
}


/**
 * @interface_method_impl{RTVFSOBJSETOPS,pfnSetTimes}
 */
static DECLCALLBACK(int) rtFsIsoMakerOutFile_SetTimes(void *pvThis, PCRTTIMESPEC pAccessTime, PCRTTIMESPEC pModificationTime,
                                                      PCRTTIMESPEC pChangeTime, PCRTTIMESPEC pBirthTime)
{
    RT_NOREF(pvThis, pAccessTime, pModificationTime, pChangeTime, pBirthTime);
    return VERR_WRITE_PROTECT;
}


/**
 * @interface_method_impl{RTVFSOBJSETOPS,pfnSetOwner}
 */
static DECLCALLBACK(int) rtFsIsoMakerOutFile_SetOwner(void *pvThis, RTUID uid, RTGID gid)
{
    RT_NOREF(pvThis, uid, gid);
    return VERR_WRITE_PROTECT;
}


/**
 * @interface_method_impl{RTVFSFILEOPS,pfnSeek}
 */
static DECLCALLBACK(int) rtFsIsoMakerOutFile_Seek(void *pvThis, RTFOFF offSeek, unsigned uMethod, PRTFOFF poffActual)
{
    PRTFSISOMAKEROUTPUTFILE pThis = (PRTFSISOMAKEROUTPUTFILE)pvThis;

    /*
     * Seek relative to which position.
     */
    uint64_t offWrt;
    switch (uMethod)
    {
        case RTFILE_SEEK_BEGIN:
            offWrt = 0;
            break;

        case RTFILE_SEEK_CURRENT:
            offWrt = pThis->offCurPos;
            break;

        case RTFILE_SEEK_END:
            offWrt = pThis->pIsoMaker->cbFinalizedImage;
            break;

        default:
            return VERR_INTERNAL_ERROR_5;
    }

    /*
     * Calc new position, take care to stay within RTFOFF type bounds.
     */
    uint64_t offNew;
    if (offSeek == 0)
        offNew = offWrt;
    else if (offSeek > 0)
    {
        offNew = offWrt + offSeek;
        if (   offNew < offWrt
            || offNew > RTFOFF_MAX)
            offNew = RTFOFF_MAX;
    }
    else if ((uint64_t)-offSeek < offWrt)
        offNew = offWrt + offSeek;
    else
        offNew = 0;
    pThis->offCurPos = offNew;

    *poffActual = offNew;
    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{RTVFSFILEOPS,pfnQuerySize}
 */
static DECLCALLBACK(int) rtFsIsoMakerOutFile_QuerySize(void *pvThis, uint64_t *pcbFile)
{
    PRTFSISOMAKEROUTPUTFILE pThis = (PRTFSISOMAKEROUTPUTFILE)pvThis;
    *pcbFile = pThis->pIsoMaker->cbFinalizedImage;
    return VINF_SUCCESS;
}


/**
 * Standard file operations.
 */
DECL_HIDDEN_CONST(const RTVFSFILEOPS) g_rtFsIsoMakerOutputFileOps =
{
    { /* Stream */
        { /* Obj */
            RTVFSOBJOPS_VERSION,
            RTVFSOBJTYPE_FILE,
            "ISO Maker Output File",
            rtFsIsoMakerOutFile_Close,
            rtFsIsoMakerOutFile_QueryInfo,
            RTVFSOBJOPS_VERSION
        },
        RTVFSIOSTREAMOPS_VERSION,
        RTVFSIOSTREAMOPS_FEAT_NO_SG,
        rtFsIsoMakerOutFile_Read,
        rtFsIsoMakerOutFile_Write,
        rtFsIsoMakerOutFile_Flush,
        rtFsIsoMakerOutFile_PollOne,
        rtFsIsoMakerOutFile_Tell,
        rtFsIsoMakerOutFile_Skip,
        NULL /*ZeroFill*/,
        RTVFSIOSTREAMOPS_VERSION,
    },
    RTVFSFILEOPS_VERSION,
    0,
    { /* ObjSet */
        RTVFSOBJSETOPS_VERSION,
        RT_OFFSETOF(RTVFSFILEOPS, Stream.Obj) - RT_OFFSETOF(RTVFSFILEOPS, ObjSet),
        rtFsIsoMakerOutFile_SetMode,
        rtFsIsoMakerOutFile_SetTimes,
        rtFsIsoMakerOutFile_SetOwner,
        RTVFSOBJSETOPS_VERSION
    },
    rtFsIsoMakerOutFile_Seek,
    rtFsIsoMakerOutFile_QuerySize,
    RTVFSFILEOPS_VERSION
};



/**
 * Creates a VFS file for a finalized ISO maker instanced.
 *
 * The file can be used to access the image.  Both sequential and random access
 * are supported, so that this could in theory be hooked up to a CD/DVD-ROM
 * drive emulation and used as a virtual ISO image.
 *
 * @returns IRPT status code.
 * @param   hIsoMaker           The ISO maker handle.
 * @param   phVfsFile           Where to return the handle.
 */
RTDECL(int) RTFsIsoMakerCreateVfsOutputFile(RTFSISOMAKER hIsoMaker, PRTVFSFILE phVfsFile)
{
    PRTFSISOMAKERINT pThis = hIsoMaker;
    RTFSISOMAKER_ASSERT_VALID_HANDLE_RET(pThis);
    AssertReturn(pThis->fFinalized, VERR_WRONG_ORDER);
    AssertPtrReturn(phVfsFile, VERR_INVALID_POINTER);

    uint32_t cRefs = RTFsIsoMakerRetain(pThis);
    AssertReturn(cRefs != UINT32_MAX, VERR_INVALID_HANDLE);

    PRTFSISOMAKEROUTPUTFILE pFileData;
    RTVFSFILE               hVfsFile;
    int rc = RTVfsNewFile(&g_rtFsIsoMakerOutputFileOps, sizeof(*pFileData), RTFILE_O_READ | RTFILE_O_DENY_NONE | RTFILE_O_CREATE,
                          NIL_RTVFS, NIL_RTVFSLOCK, &hVfsFile, (void **)&pFileData);
    if (RT_SUCCESS(rc))
    {
        pFileData->pIsoMaker          = pThis;
        pFileData->offCurPos          = 0;
        pFileData->pFileHint          = NULL;
        pFileData->hVfsSrcFile        = NIL_RTVFSFILE;
        pFileData->pDirHintPrimaryIso = NULL;
        pFileData->pDirHintJoliet     = NULL;
        *phVfsFile = hVfsFile;
        return VINF_SUCCESS;
    }

    RTFsIsoMakerRelease(pThis);
    *phVfsFile = NIL_RTVFSFILE;
    return rc;
}

