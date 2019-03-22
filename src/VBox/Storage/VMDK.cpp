/* $Id$ */
/** @file
 * VMDK disk image, core code.
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


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#define LOG_GROUP LOG_GROUP_VD_VMDK
#include <VBox/vd-plugin.h>
#include <VBox/err.h>

#include <VBox/log.h>
#include <iprt/assert.h>
#include <iprt/alloc.h>
#include <iprt/uuid.h>
#include <iprt/path.h>
#include <iprt/string.h>
#include <iprt/rand.h>
#include <iprt/zip.h>
#include <iprt/asm.h>

#include "VDBackends.h"


/*********************************************************************************************************************************
*   Constants And Macros, Structures and Typedefs                                                                                *
*********************************************************************************************************************************/

/** Maximum encoded string size (including NUL) we allow for VMDK images.
 * Deliberately not set high to avoid running out of descriptor space. */
#define VMDK_ENCODED_COMMENT_MAX 1024

/** VMDK descriptor DDB entry for PCHS cylinders. */
#define VMDK_DDB_GEO_PCHS_CYLINDERS "ddb.geometry.cylinders"

/** VMDK descriptor DDB entry for PCHS heads. */
#define VMDK_DDB_GEO_PCHS_HEADS "ddb.geometry.heads"

/** VMDK descriptor DDB entry for PCHS sectors. */
#define VMDK_DDB_GEO_PCHS_SECTORS "ddb.geometry.sectors"

/** VMDK descriptor DDB entry for LCHS cylinders. */
#define VMDK_DDB_GEO_LCHS_CYLINDERS "ddb.geometry.biosCylinders"

/** VMDK descriptor DDB entry for LCHS heads. */
#define VMDK_DDB_GEO_LCHS_HEADS "ddb.geometry.biosHeads"

/** VMDK descriptor DDB entry for LCHS sectors. */
#define VMDK_DDB_GEO_LCHS_SECTORS "ddb.geometry.biosSectors"

/** VMDK descriptor DDB entry for image UUID. */
#define VMDK_DDB_IMAGE_UUID "ddb.uuid.image"

/** VMDK descriptor DDB entry for image modification UUID. */
#define VMDK_DDB_MODIFICATION_UUID "ddb.uuid.modification"

/** VMDK descriptor DDB entry for parent image UUID. */
#define VMDK_DDB_PARENT_UUID "ddb.uuid.parent"

/** VMDK descriptor DDB entry for parent image modification UUID. */
#define VMDK_DDB_PARENT_MODIFICATION_UUID "ddb.uuid.parentmodification"

/** No compression for streamOptimized files. */
#define VMDK_COMPRESSION_NONE 0

/** Deflate compression for streamOptimized files. */
#define VMDK_COMPRESSION_DEFLATE 1

/** Marker that the actual GD value is stored in the footer. */
#define VMDK_GD_AT_END 0xffffffffffffffffULL

/** Marker for end-of-stream in streamOptimized images. */
#define VMDK_MARKER_EOS 0

/** Marker for grain table block in streamOptimized images. */
#define VMDK_MARKER_GT 1

/** Marker for grain directory block in streamOptimized images. */
#define VMDK_MARKER_GD 2

/** Marker for footer in streamOptimized images. */
#define VMDK_MARKER_FOOTER 3

/** Marker for unknown purpose in streamOptimized images.
 * Shows up in very recent images created by vSphere, but only sporadically.
 * They "forgot" to document that one in the VMDK specification. */
#define VMDK_MARKER_UNSPECIFIED 4

/** Dummy marker for "don't check the marker value". */
#define VMDK_MARKER_IGNORE 0xffffffffU

/**
 * Magic number for hosted images created by VMware Workstation 4, VMware
 * Workstation 5, VMware Server or VMware Player. Not necessarily sparse.
 */
#define VMDK_SPARSE_MAGICNUMBER 0x564d444b /* 'V' 'M' 'D' 'K' */

/**
 * VMDK hosted binary extent header. The "Sparse" is a total misnomer, as
 * this header is also used for monolithic flat images.
 */
#pragma pack(1)
typedef struct SparseExtentHeader
{
    uint32_t    magicNumber;
    uint32_t    version;
    uint32_t    flags;
    uint64_t    capacity;
    uint64_t    grainSize;
    uint64_t    descriptorOffset;
    uint64_t    descriptorSize;
    uint32_t    numGTEsPerGT;
    uint64_t    rgdOffset;
    uint64_t    gdOffset;
    uint64_t    overHead;
    bool        uncleanShutdown;
    char        singleEndLineChar;
    char        nonEndLineChar;
    char        doubleEndLineChar1;
    char        doubleEndLineChar2;
    uint16_t    compressAlgorithm;
    uint8_t     pad[433];
} SparseExtentHeader;
#pragma pack()

/** The maximum allowed descriptor size in the extent header in sectors. */
#define VMDK_SPARSE_DESCRIPTOR_SIZE_MAX UINT64_C(20480) /* 10MB */

/** VMDK capacity for a single chunk when 2G splitting is turned on. Should be
 * divisible by the default grain size (64K) */
#define VMDK_2G_SPLIT_SIZE (2047 * 1024 * 1024)

/** VMDK streamOptimized file format marker. The type field may or may not
 * be actually valid, but there's always data to read there. */
#pragma pack(1)
typedef struct VMDKMARKER
{
    uint64_t uSector;
    uint32_t cbSize;
    uint32_t uType;
} VMDKMARKER, *PVMDKMARKER;
#pragma pack()


/** Convert sector number/size to byte offset/size. */
#define VMDK_SECTOR2BYTE(u) ((uint64_t)(u) << 9)

/** Convert byte offset/size to sector number/size. */
#define VMDK_BYTE2SECTOR(u) ((u) >> 9)

/**
 * VMDK extent type.
 */
typedef enum VMDKETYPE
{
    /** Hosted sparse extent. */
    VMDKETYPE_HOSTED_SPARSE = 1,
    /** Flat extent. */
    VMDKETYPE_FLAT,
    /** Zero extent. */
    VMDKETYPE_ZERO,
    /** VMFS extent, used by ESX. */
    VMDKETYPE_VMFS
} VMDKETYPE, *PVMDKETYPE;

/**
 * VMDK access type for a extent.
 */
typedef enum VMDKACCESS
{
    /** No access allowed. */
    VMDKACCESS_NOACCESS = 0,
    /** Read-only access. */
    VMDKACCESS_READONLY,
    /** Read-write access. */
    VMDKACCESS_READWRITE
} VMDKACCESS, *PVMDKACCESS;

/** Forward declaration for PVMDKIMAGE. */
typedef struct VMDKIMAGE *PVMDKIMAGE;

/**
 * Extents files entry. Used for opening a particular file only once.
 */
typedef struct VMDKFILE
{
    /** Pointer to filename. Local copy. */
    const char      *pszFilename;
    /** File open flags for consistency checking. */
    unsigned         fOpen;
    /** Handle for sync/async file abstraction.*/
    PVDIOSTORAGE     pStorage;
    /** Reference counter. */
    unsigned         uReferences;
    /** Flag whether the file should be deleted on last close. */
    bool             fDelete;
    /** Pointer to the image we belong to (for debugging purposes). */
    PVMDKIMAGE       pImage;
    /** Pointer to next file descriptor. */
    struct VMDKFILE *pNext;
    /** Pointer to the previous file descriptor. */
    struct VMDKFILE *pPrev;
} VMDKFILE, *PVMDKFILE;

/**
 * VMDK extent data structure.
 */
typedef struct VMDKEXTENT
{
    /** File handle. */
    PVMDKFILE    pFile;
    /** Base name of the image extent. */
    const char  *pszBasename;
    /** Full name of the image extent. */
    const char  *pszFullname;
    /** Number of sectors in this extent. */
    uint64_t    cSectors;
    /** Number of sectors per block (grain in VMDK speak). */
    uint64_t    cSectorsPerGrain;
    /** Starting sector number of descriptor. */
    uint64_t    uDescriptorSector;
    /** Size of descriptor in sectors. */
    uint64_t    cDescriptorSectors;
    /** Starting sector number of grain directory. */
    uint64_t    uSectorGD;
    /** Starting sector number of redundant grain directory. */
    uint64_t    uSectorRGD;
    /** Total number of metadata sectors. */
    uint64_t    cOverheadSectors;
    /** Nominal size (i.e. as described by the descriptor) of this extent. */
    uint64_t    cNominalSectors;
    /** Sector offset (i.e. as described by the descriptor) of this extent. */
    uint64_t    uSectorOffset;
    /** Number of entries in a grain table. */
    uint32_t    cGTEntries;
    /** Number of sectors reachable via a grain directory entry. */
    uint32_t    cSectorsPerGDE;
    /** Number of entries in the grain directory. */
    uint32_t    cGDEntries;
    /** Pointer to the next free sector. Legacy information. Do not use. */
    uint32_t    uFreeSector;
    /** Number of this extent in the list of images. */
    uint32_t    uExtent;
    /** Pointer to the descriptor (NULL if no descriptor in this extent). */
    char        *pDescData;
    /** Pointer to the grain directory. */
    uint32_t    *pGD;
    /** Pointer to the redundant grain directory. */
    uint32_t    *pRGD;
    /** VMDK version of this extent. 1=1.0/1.1 */
    uint32_t    uVersion;
    /** Type of this extent. */
    VMDKETYPE   enmType;
    /** Access to this extent. */
    VMDKACCESS  enmAccess;
    /** Flag whether this extent is marked as unclean. */
    bool        fUncleanShutdown;
    /** Flag whether the metadata in the extent header needs to be updated. */
    bool        fMetaDirty;
    /** Flag whether there is a footer in this extent. */
    bool        fFooter;
    /** Compression type for this extent. */
    uint16_t    uCompression;
    /** Append position for writing new grain. Only for sparse extents. */
    uint64_t    uAppendPosition;
    /** Last grain which was accessed. Only for streamOptimized extents. */
    uint32_t    uLastGrainAccess;
    /** Starting sector corresponding to the grain buffer. */
    uint32_t    uGrainSectorAbs;
    /** Grain number corresponding to the grain buffer. */
    uint32_t    uGrain;
    /** Actual size of the compressed data, only valid for reading. */
    uint32_t    cbGrainStreamRead;
    /** Size of compressed grain buffer for streamOptimized extents. */
    size_t      cbCompGrain;
    /** Compressed grain buffer for streamOptimized extents, with marker. */
    void        *pvCompGrain;
    /** Decompressed grain buffer for streamOptimized extents. */
    void        *pvGrain;
    /** Reference to the image in which this extent is used. Do not use this
     * on a regular basis to avoid passing pImage references to functions
     * explicitly. */
    struct VMDKIMAGE *pImage;
} VMDKEXTENT, *PVMDKEXTENT;

/**
 * Grain table cache size. Allocated per image.
 */
#define VMDK_GT_CACHE_SIZE 256

/**
 * Grain table block size. Smaller than an actual grain table block to allow
 * more grain table blocks to be cached without having to allocate excessive
 * amounts of memory for the cache.
 */
#define VMDK_GT_CACHELINE_SIZE 128


/**
 * Maximum number of lines in a descriptor file. Not worth the effort of
 * making it variable. Descriptor files are generally very short (~20 lines),
 * with the exception of sparse files split in 2G chunks, which need for the
 * maximum size (almost 2T) exactly 1025 lines for the disk database.
 */
#define VMDK_DESCRIPTOR_LINES_MAX   1100U

/**
 * Parsed descriptor information. Allows easy access and update of the
 * descriptor (whether separate file or not). Free form text files suck.
 */
typedef struct VMDKDESCRIPTOR
{
    /** Line number of first entry of the disk descriptor. */
    unsigned    uFirstDesc;
    /** Line number of first entry in the extent description. */
    unsigned    uFirstExtent;
    /** Line number of first disk database entry. */
    unsigned    uFirstDDB;
    /** Total number of lines. */
    unsigned    cLines;
    /** Total amount of memory available for the descriptor. */
    size_t      cbDescAlloc;
    /** Set if descriptor has been changed and not yet written to disk. */
    bool        fDirty;
    /** Array of pointers to the data in the descriptor. */
    char        *aLines[VMDK_DESCRIPTOR_LINES_MAX];
    /** Array of line indices pointing to the next non-comment line. */
    unsigned    aNextLines[VMDK_DESCRIPTOR_LINES_MAX];
} VMDKDESCRIPTOR, *PVMDKDESCRIPTOR;


/**
 * Cache entry for translating extent/sector to a sector number in that
 * extent.
 */
typedef struct VMDKGTCACHEENTRY
{
    /** Extent number for which this entry is valid. */
    uint32_t    uExtent;
    /** GT data block number. */
    uint64_t    uGTBlock;
    /** Data part of the cache entry. */
    uint32_t    aGTData[VMDK_GT_CACHELINE_SIZE];
} VMDKGTCACHEENTRY, *PVMDKGTCACHEENTRY;

/**
 * Cache data structure for blocks of grain table entries. For now this is a
 * fixed size direct mapping cache, but this should be adapted to the size of
 * the sparse image and maybe converted to a set-associative cache. The
 * implementation below implements a write-through cache with write allocate.
 */
typedef struct VMDKGTCACHE
{
    /** Cache entries. */
    VMDKGTCACHEENTRY    aGTCache[VMDK_GT_CACHE_SIZE];
    /** Number of cache entries (currently unused). */
    unsigned            cEntries;
} VMDKGTCACHE, *PVMDKGTCACHE;

/**
 * Complete VMDK image data structure. Mainly a collection of extents and a few
 * extra global data fields.
 */
typedef struct VMDKIMAGE
{
    /** Image name. */
    const char        *pszFilename;
    /** Descriptor file if applicable. */
    PVMDKFILE         pFile;

    /** Pointer to the per-disk VD interface list. */
    PVDINTERFACE      pVDIfsDisk;
    /** Pointer to the per-image VD interface list. */
    PVDINTERFACE      pVDIfsImage;

    /** Error interface. */
    PVDINTERFACEERROR pIfError;
    /** I/O interface. */
    PVDINTERFACEIOINT pIfIo;


    /** Pointer to the image extents. */
    PVMDKEXTENT     pExtents;
    /** Number of image extents. */
    unsigned        cExtents;
    /** Pointer to the files list, for opening a file referenced multiple
     * times only once (happens mainly with raw partition access). */
    PVMDKFILE       pFiles;

    /**
     * Pointer to an array of segment entries for async I/O.
     * This is an optimization because the task number to submit is not known
     * and allocating/freeing an array in the read/write functions every time
     * is too expensive.
     */
    PPDMDATASEG     paSegments;
    /** Entries available in the segments array. */
    unsigned        cSegments;

    /** Open flags passed by VBoxHD layer. */
    unsigned        uOpenFlags;
    /** Image flags defined during creation or determined during open. */
    unsigned        uImageFlags;
    /** Total size of the image. */
    uint64_t        cbSize;
    /** Physical geometry of this image. */
    VDGEOMETRY      PCHSGeometry;
    /** Logical geometry of this image. */
    VDGEOMETRY      LCHSGeometry;
    /** Image UUID. */
    RTUUID          ImageUuid;
    /** Image modification UUID. */
    RTUUID          ModificationUuid;
    /** Parent image UUID. */
    RTUUID          ParentUuid;
    /** Parent image modification UUID. */
    RTUUID          ParentModificationUuid;

    /** Pointer to grain table cache, if this image contains sparse extents. */
    PVMDKGTCACHE    pGTCache;
    /** Pointer to the descriptor (NULL if no separate descriptor file). */
    char            *pDescData;
    /** Allocation size of the descriptor file. */
    size_t          cbDescAlloc;
    /** Parsed descriptor file content. */
    VMDKDESCRIPTOR  Descriptor;
    /** The static region list. */
    VDREGIONLIST    RegionList;
} VMDKIMAGE;


/** State for the input/output callout of the inflate reader/deflate writer. */
typedef struct VMDKCOMPRESSIO
{
    /* Image this operation relates to. */
    PVMDKIMAGE pImage;
    /* Current read position. */
    ssize_t iOffset;
    /* Size of the compressed grain buffer (available data). */
    size_t cbCompGrain;
    /* Pointer to the compressed grain buffer. */
    void *pvCompGrain;
} VMDKCOMPRESSIO;


/** Tracks async grain allocation. */
typedef struct VMDKGRAINALLOCASYNC
{
    /** Flag whether the allocation failed. */
    bool        fIoErr;
    /** Current number of transfers pending.
     * If reached 0 and there is an error the old state is restored. */
    unsigned    cIoXfersPending;
    /** Sector number */
    uint64_t    uSector;
    /** Flag whether the grain table needs to be updated. */
    bool        fGTUpdateNeeded;
    /** Extent the allocation happens. */
    PVMDKEXTENT pExtent;
    /** Position of the new grain, required for the grain table update. */
    uint64_t    uGrainOffset;
    /** Grain table sector. */
    uint64_t    uGTSector;
    /** Backup grain table sector. */
    uint64_t    uRGTSector;
} VMDKGRAINALLOCASYNC, *PVMDKGRAINALLOCASYNC;

/**
 * State information for vmdkRename() and helpers.
 */
typedef struct VMDKRENAMESTATE
{
    /** Array of old filenames. */
    char           **apszOldName;
    /** Array of new filenames. */
    char           **apszNewName;
    /** Array of new lines in the extent descriptor. */
    char           **apszNewLines;
    /** Name of the old descriptor file if not a sparse image. */
    char           *pszOldDescName;
    /** Flag whether we called vmdkFreeImage(). */
    bool           fImageFreed;
    /** Flag whther the descriptor is embedded in the image (sparse) or
     * in a separate file. */
    bool           fEmbeddedDesc;
    /** Number of extents in the image. */
    unsigned       cExtents;
    /** New base filename. */
    char           *pszNewBaseName;
    /** The old base filename. */
    char           *pszOldBaseName;
    /** New full filename. */
    char           *pszNewFullName;
    /** Old full filename. */
    char           *pszOldFullName;
    /** The old image name. */
    const char     *pszOldImageName;
    /** Copy of the original VMDK descriptor. */
    VMDKDESCRIPTOR DescriptorCopy;
    /** Copy of the extent state for sparse images. */
    VMDKEXTENT     ExtentCopy;
} VMDKRENAMESTATE;
/** Pointer to a VMDK rename state. */
typedef VMDKRENAMESTATE *PVMDKRENAMESTATE;


/*********************************************************************************************************************************
*   Static Variables                                                                                                             *
*********************************************************************************************************************************/

/** NULL-terminated array of supported file extensions. */
static const VDFILEEXTENSION s_aVmdkFileExtensions[] =
{
    {"vmdk", VDTYPE_HDD},
    {NULL, VDTYPE_INVALID}
};


/*********************************************************************************************************************************
*   Internal Functions                                                                                                           *
*********************************************************************************************************************************/

static void vmdkFreeStreamBuffers(PVMDKEXTENT pExtent);
static int vmdkFreeExtentData(PVMDKIMAGE pImage, PVMDKEXTENT pExtent,
                              bool fDelete);

static int vmdkCreateExtents(PVMDKIMAGE pImage, unsigned cExtents);
static int vmdkFlushImage(PVMDKIMAGE pImage, PVDIOCTX pIoCtx);
static int vmdkSetImageComment(PVMDKIMAGE pImage, const char *pszComment);
static int vmdkFreeImage(PVMDKIMAGE pImage, bool fDelete, bool fFlush);

static DECLCALLBACK(int) vmdkAllocGrainComplete(void *pBackendData, PVDIOCTX pIoCtx,
                                                void *pvUser, int rcReq);

/**
 * Internal: open a file (using a file descriptor cache to ensure each file
 * is only opened once - anything else can cause locking problems).
 */
static int vmdkFileOpen(PVMDKIMAGE pImage, PVMDKFILE *ppVmdkFile,
                        const char *pszFilename, uint32_t fOpen)
{
    int rc = VINF_SUCCESS;
    PVMDKFILE pVmdkFile;

    for (pVmdkFile = pImage->pFiles;
         pVmdkFile != NULL;
         pVmdkFile = pVmdkFile->pNext)
    {
        if (!strcmp(pszFilename, pVmdkFile->pszFilename))
        {
            Assert(fOpen == pVmdkFile->fOpen);
            pVmdkFile->uReferences++;

            *ppVmdkFile = pVmdkFile;

            return rc;
        }
    }

    /* If we get here, there's no matching entry in the cache. */
    pVmdkFile = (PVMDKFILE)RTMemAllocZ(sizeof(VMDKFILE));
    if (!pVmdkFile)
    {
        *ppVmdkFile = NULL;
        return VERR_NO_MEMORY;
    }

    pVmdkFile->pszFilename = RTStrDup(pszFilename);
    if (!pVmdkFile->pszFilename)
    {
        RTMemFree(pVmdkFile);
        *ppVmdkFile = NULL;
        return VERR_NO_MEMORY;
    }
    pVmdkFile->fOpen = fOpen;

    rc = vdIfIoIntFileOpen(pImage->pIfIo, pszFilename, fOpen,
                           &pVmdkFile->pStorage);
    if (RT_SUCCESS(rc))
    {
        pVmdkFile->uReferences = 1;
        pVmdkFile->pImage = pImage;
        pVmdkFile->pNext = pImage->pFiles;
        if (pImage->pFiles)
            pImage->pFiles->pPrev = pVmdkFile;
        pImage->pFiles = pVmdkFile;
        *ppVmdkFile = pVmdkFile;
    }
    else
    {
        RTStrFree((char *)(void *)pVmdkFile->pszFilename);
        RTMemFree(pVmdkFile);
        *ppVmdkFile = NULL;
    }

    return rc;
}

/**
 * Internal: close a file, updating the file descriptor cache.
 */
static int vmdkFileClose(PVMDKIMAGE pImage, PVMDKFILE *ppVmdkFile, bool fDelete)
{
    int rc = VINF_SUCCESS;
    PVMDKFILE pVmdkFile = *ppVmdkFile;

    AssertPtr(pVmdkFile);

    pVmdkFile->fDelete |= fDelete;
    Assert(pVmdkFile->uReferences);
    pVmdkFile->uReferences--;
    if (pVmdkFile->uReferences == 0)
    {
        PVMDKFILE pPrev;
        PVMDKFILE pNext;

        /* Unchain the element from the list. */
        pPrev = pVmdkFile->pPrev;
        pNext = pVmdkFile->pNext;

        if (pNext)
            pNext->pPrev = pPrev;
        if (pPrev)
            pPrev->pNext = pNext;
        else
            pImage->pFiles = pNext;

        rc = vdIfIoIntFileClose(pImage->pIfIo, pVmdkFile->pStorage);
        if (pVmdkFile->fDelete)
        {
            int rc2 = vdIfIoIntFileDelete(pImage->pIfIo, pVmdkFile->pszFilename);
            if (RT_SUCCESS(rc))
                rc = rc2;
        }
        RTStrFree((char *)(void *)pVmdkFile->pszFilename);
        RTMemFree(pVmdkFile);
    }

    *ppVmdkFile = NULL;
    return rc;
}

/*#define VMDK_USE_BLOCK_DECOMP_API - test and enable */
#ifndef VMDK_USE_BLOCK_DECOMP_API
static DECLCALLBACK(int) vmdkFileInflateHelper(void *pvUser, void *pvBuf, size_t cbBuf, size_t *pcbBuf)
{
    VMDKCOMPRESSIO *pInflateState = (VMDKCOMPRESSIO *)pvUser;
    size_t cbInjected = 0;

    Assert(cbBuf);
    if (pInflateState->iOffset < 0)
    {
        *(uint8_t *)pvBuf = RTZIPTYPE_ZLIB;
        pvBuf = (uint8_t *)pvBuf + 1;
        cbBuf--;
        cbInjected = 1;
        pInflateState->iOffset = RT_UOFFSETOF(VMDKMARKER, uType);
    }
    if (!cbBuf)
    {
        if (pcbBuf)
            *pcbBuf = cbInjected;
        return VINF_SUCCESS;
    }
    cbBuf = RT_MIN(cbBuf, pInflateState->cbCompGrain - pInflateState->iOffset);
    memcpy(pvBuf,
           (uint8_t *)pInflateState->pvCompGrain + pInflateState->iOffset,
           cbBuf);
    pInflateState->iOffset += cbBuf;
    Assert(pcbBuf);
    *pcbBuf = cbBuf + cbInjected;
    return VINF_SUCCESS;
}
#endif

/**
 * Internal: read from a file and inflate the compressed data,
 * distinguishing between async and normal operation
 */
DECLINLINE(int) vmdkFileInflateSync(PVMDKIMAGE pImage, PVMDKEXTENT pExtent,
                                    uint64_t uOffset, void *pvBuf,
                                    size_t cbToRead, const void *pcvMarker,
                                    uint64_t *puLBA, uint32_t *pcbMarkerData)
{
    int rc;
#ifndef VMDK_USE_BLOCK_DECOMP_API
    PRTZIPDECOMP pZip = NULL;
#endif
    VMDKMARKER *pMarker = (VMDKMARKER *)pExtent->pvCompGrain;
    size_t cbCompSize, cbActuallyRead;

    if (!pcvMarker)
    {
        rc = vdIfIoIntFileReadSync(pImage->pIfIo, pExtent->pFile->pStorage,
                                   uOffset, pMarker, RT_UOFFSETOF(VMDKMARKER, uType));
        if (RT_FAILURE(rc))
            return rc;
    }
    else
    {
        memcpy(pMarker, pcvMarker, RT_UOFFSETOF(VMDKMARKER, uType));
        /* pcvMarker endianness has already been partially transformed, fix it */
        pMarker->uSector = RT_H2LE_U64(pMarker->uSector);
        pMarker->cbSize = RT_H2LE_U32(pMarker->cbSize);
    }

    cbCompSize = RT_LE2H_U32(pMarker->cbSize);
    if (cbCompSize == 0)
    {
        AssertMsgFailed(("VMDK: corrupted marker\n"));
        return VERR_VD_VMDK_INVALID_FORMAT;
    }

    /* Sanity check - the expansion ratio should be much less than 2. */
    Assert(cbCompSize < 2 * cbToRead);
    if (cbCompSize >= 2 * cbToRead)
        return VERR_VD_VMDK_INVALID_FORMAT;

    /* Compressed grain marker. Data follows immediately. */
    rc = vdIfIoIntFileReadSync(pImage->pIfIo, pExtent->pFile->pStorage,
                               uOffset + RT_UOFFSETOF(VMDKMARKER, uType),
                                (uint8_t *)pExtent->pvCompGrain
                              + RT_UOFFSETOF(VMDKMARKER, uType),
                               RT_ALIGN_Z(  cbCompSize
                                          + RT_UOFFSETOF(VMDKMARKER, uType),
                                          512)
                               - RT_UOFFSETOF(VMDKMARKER, uType));

    if (puLBA)
        *puLBA = RT_LE2H_U64(pMarker->uSector);
    if (pcbMarkerData)
        *pcbMarkerData = RT_ALIGN(  cbCompSize
                                  + RT_UOFFSETOF(VMDKMARKER, uType),
                                  512);

#ifdef VMDK_USE_BLOCK_DECOMP_API
    rc = RTZipBlockDecompress(RTZIPTYPE_ZLIB, 0 /*fFlags*/,
                              pExtent->pvCompGrain, cbCompSize + RT_UOFFSETOF(VMDKMARKER, uType), NULL,
                              pvBuf, cbToRead, &cbActuallyRead);
#else
    VMDKCOMPRESSIO InflateState;
    InflateState.pImage = pImage;
    InflateState.iOffset = -1;
    InflateState.cbCompGrain = cbCompSize + RT_UOFFSETOF(VMDKMARKER, uType);
    InflateState.pvCompGrain = pExtent->pvCompGrain;

    rc = RTZipDecompCreate(&pZip, &InflateState, vmdkFileInflateHelper);
    if (RT_FAILURE(rc))
        return rc;
    rc = RTZipDecompress(pZip, pvBuf, cbToRead, &cbActuallyRead);
    RTZipDecompDestroy(pZip);
#endif /* !VMDK_USE_BLOCK_DECOMP_API */
    if (RT_FAILURE(rc))
    {
        if (rc == VERR_ZIP_CORRUPTED)
            rc = vdIfError(pImage->pIfError, rc, RT_SRC_POS, N_("VMDK: Compressed image is corrupted '%s'"), pExtent->pszFullname);
        return rc;
    }
    if (cbActuallyRead != cbToRead)
        rc = VERR_VD_VMDK_INVALID_FORMAT;
    return rc;
}

static DECLCALLBACK(int) vmdkFileDeflateHelper(void *pvUser, const void *pvBuf, size_t cbBuf)
{
    VMDKCOMPRESSIO *pDeflateState = (VMDKCOMPRESSIO *)pvUser;

    Assert(cbBuf);
    if (pDeflateState->iOffset < 0)
    {
        pvBuf = (const uint8_t *)pvBuf + 1;
        cbBuf--;
        pDeflateState->iOffset = RT_UOFFSETOF(VMDKMARKER, uType);
    }
    if (!cbBuf)
        return VINF_SUCCESS;
    if (pDeflateState->iOffset + cbBuf > pDeflateState->cbCompGrain)
        return VERR_BUFFER_OVERFLOW;
    memcpy((uint8_t *)pDeflateState->pvCompGrain + pDeflateState->iOffset,
           pvBuf, cbBuf);
    pDeflateState->iOffset += cbBuf;
    return VINF_SUCCESS;
}

/**
 * Internal: deflate the uncompressed data and write to a file,
 * distinguishing between async and normal operation
 */
DECLINLINE(int) vmdkFileDeflateSync(PVMDKIMAGE pImage, PVMDKEXTENT pExtent,
                                    uint64_t uOffset, const void *pvBuf,
                                    size_t cbToWrite, uint64_t uLBA,
                                    uint32_t *pcbMarkerData)
{
    int rc;
    PRTZIPCOMP pZip = NULL;
    VMDKCOMPRESSIO DeflateState;

    DeflateState.pImage = pImage;
    DeflateState.iOffset = -1;
    DeflateState.cbCompGrain = pExtent->cbCompGrain;
    DeflateState.pvCompGrain = pExtent->pvCompGrain;

    rc = RTZipCompCreate(&pZip, &DeflateState, vmdkFileDeflateHelper,
                         RTZIPTYPE_ZLIB, RTZIPLEVEL_DEFAULT);
    if (RT_FAILURE(rc))
        return rc;
    rc = RTZipCompress(pZip, pvBuf, cbToWrite);
    if (RT_SUCCESS(rc))
        rc = RTZipCompFinish(pZip);
    RTZipCompDestroy(pZip);
    if (RT_SUCCESS(rc))
    {
        Assert(   DeflateState.iOffset > 0
               && (size_t)DeflateState.iOffset <= DeflateState.cbCompGrain);

        /* pad with zeroes to get to a full sector size */
        uint32_t uSize = DeflateState.iOffset;
        if (uSize % 512)
        {
            uint32_t uSizeAlign = RT_ALIGN(uSize, 512);
            memset((uint8_t *)pExtent->pvCompGrain + uSize, '\0',
                   uSizeAlign - uSize);
            uSize = uSizeAlign;
        }

        if (pcbMarkerData)
            *pcbMarkerData = uSize;

        /* Compressed grain marker. Data follows immediately. */
        VMDKMARKER *pMarker = (VMDKMARKER *)pExtent->pvCompGrain;
        pMarker->uSector = RT_H2LE_U64(uLBA);
        pMarker->cbSize = RT_H2LE_U32(  DeflateState.iOffset
                                      - RT_UOFFSETOF(VMDKMARKER, uType));
        rc = vdIfIoIntFileWriteSync(pImage->pIfIo, pExtent->pFile->pStorage,
                                    uOffset, pMarker, uSize);
        if (RT_FAILURE(rc))
            return rc;
    }
    return rc;
}


/**
 * Internal: check if all files are closed, prevent leaking resources.
 */
static int vmdkFileCheckAllClose(PVMDKIMAGE pImage)
{
    int rc = VINF_SUCCESS, rc2;
    PVMDKFILE pVmdkFile;

    Assert(pImage->pFiles == NULL);
    for (pVmdkFile = pImage->pFiles;
         pVmdkFile != NULL;
         pVmdkFile = pVmdkFile->pNext)
    {
        LogRel(("VMDK: leaking reference to file \"%s\"\n",
                pVmdkFile->pszFilename));
        pImage->pFiles = pVmdkFile->pNext;

        rc2 = vmdkFileClose(pImage, &pVmdkFile, pVmdkFile->fDelete);

        if (RT_SUCCESS(rc))
            rc = rc2;
    }
    return rc;
}

/**
 * Internal: truncate a string (at a UTF8 code point boundary) and encode the
 * critical non-ASCII characters.
 */
static char *vmdkEncodeString(const char *psz)
{
    char szEnc[VMDK_ENCODED_COMMENT_MAX + 3];
    char *pszDst = szEnc;

    AssertPtr(psz);

    for (; *psz; psz = RTStrNextCp(psz))
    {
        char *pszDstPrev = pszDst;
        RTUNICP Cp = RTStrGetCp(psz);
        if (Cp == '\\')
        {
            pszDst = RTStrPutCp(pszDst, Cp);
            pszDst = RTStrPutCp(pszDst, Cp);
        }
        else if (Cp == '\n')
        {
            pszDst = RTStrPutCp(pszDst, '\\');
            pszDst = RTStrPutCp(pszDst, 'n');
        }
        else if (Cp == '\r')
        {
            pszDst = RTStrPutCp(pszDst, '\\');
            pszDst = RTStrPutCp(pszDst, 'r');
        }
        else
            pszDst = RTStrPutCp(pszDst, Cp);
        if (pszDst - szEnc >= VMDK_ENCODED_COMMENT_MAX - 1)
        {
            pszDst = pszDstPrev;
            break;
        }
    }
    *pszDst = '\0';
    return RTStrDup(szEnc);
}

/**
 * Internal: decode a string and store it into the specified string.
 */
static int vmdkDecodeString(const char *pszEncoded, char *psz, size_t cb)
{
    int rc = VINF_SUCCESS;
    char szBuf[4];

    if (!cb)
        return VERR_BUFFER_OVERFLOW;

    AssertPtr(psz);

    for (; *pszEncoded; pszEncoded = RTStrNextCp(pszEncoded))
    {
        char *pszDst = szBuf;
        RTUNICP Cp = RTStrGetCp(pszEncoded);
        if (Cp == '\\')
        {
            pszEncoded = RTStrNextCp(pszEncoded);
            RTUNICP CpQ = RTStrGetCp(pszEncoded);
            if (CpQ == 'n')
                RTStrPutCp(pszDst, '\n');
            else if (CpQ == 'r')
                RTStrPutCp(pszDst, '\r');
            else if (CpQ == '\0')
            {
                rc = VERR_VD_VMDK_INVALID_HEADER;
                break;
            }
            else
                RTStrPutCp(pszDst, CpQ);
        }
        else
            pszDst = RTStrPutCp(pszDst, Cp);

        /* Need to leave space for terminating NUL. */
        if ((size_t)(pszDst - szBuf) + 1 >= cb)
        {
            rc = VERR_BUFFER_OVERFLOW;
            break;
        }
        memcpy(psz, szBuf, pszDst - szBuf);
        psz += pszDst - szBuf;
    }
    *psz = '\0';
    return rc;
}

/**
 * Internal: free all buffers associated with grain directories.
 */
static void vmdkFreeGrainDirectory(PVMDKEXTENT pExtent)
{
    if (pExtent->pGD)
    {
        RTMemFree(pExtent->pGD);
        pExtent->pGD = NULL;
    }
    if (pExtent->pRGD)
    {
        RTMemFree(pExtent->pRGD);
        pExtent->pRGD = NULL;
    }
}

/**
 * Internal: allocate the compressed/uncompressed buffers for streamOptimized
 * images.
 */
static int vmdkAllocStreamBuffers(PVMDKIMAGE pImage, PVMDKEXTENT pExtent)
{
    int rc = VINF_SUCCESS;

    if (pImage->uImageFlags & VD_VMDK_IMAGE_FLAGS_STREAM_OPTIMIZED)
    {
        /* streamOptimized extents need a compressed grain buffer, which must
         * be big enough to hold uncompressible data (which needs ~8 bytes
         * more than the uncompressed data), the marker and padding. */
        pExtent->cbCompGrain = RT_ALIGN_Z(  VMDK_SECTOR2BYTE(pExtent->cSectorsPerGrain)
                                          + 8 + sizeof(VMDKMARKER), 512);
        pExtent->pvCompGrain = RTMemAlloc(pExtent->cbCompGrain);
        if (RT_LIKELY(pExtent->pvCompGrain))
        {
            /* streamOptimized extents need a decompressed grain buffer. */
            pExtent->pvGrain = RTMemAlloc(VMDK_SECTOR2BYTE(pExtent->cSectorsPerGrain));
            if (!pExtent->pvGrain)
                rc = VERR_NO_MEMORY;
        }
        else
            rc = VERR_NO_MEMORY;
    }

    if (RT_FAILURE(rc))
        vmdkFreeStreamBuffers(pExtent);
    return rc;
}

/**
 * Internal: allocate all buffers associated with grain directories.
 */
static int vmdkAllocGrainDirectory(PVMDKIMAGE pImage, PVMDKEXTENT pExtent)
{
    RT_NOREF1(pImage);
    int rc = VINF_SUCCESS;
    size_t cbGD = pExtent->cGDEntries * sizeof(uint32_t);

    pExtent->pGD = (uint32_t *)RTMemAllocZ(cbGD);
    if (RT_LIKELY(pExtent->pGD))
    {
        if (pExtent->uSectorRGD)
        {
            pExtent->pRGD = (uint32_t *)RTMemAllocZ(cbGD);
            if (RT_UNLIKELY(!pExtent->pRGD))
                rc = VERR_NO_MEMORY;
        }
    }
    else
        rc = VERR_NO_MEMORY;

    if (RT_FAILURE(rc))
        vmdkFreeGrainDirectory(pExtent);
    return rc;
}

/**
 * Converts the grain directory from little to host endianess.
 *
 * @returns nothing.
 * @param   pGD             The grain directory.
 * @param   cGDEntries      Number of entries in the grain directory to convert.
 */
DECLINLINE(void) vmdkGrainDirectoryConvToHost(uint32_t *pGD, uint32_t cGDEntries)
{
    uint32_t *pGDTmp = pGD;

    for (uint32_t i = 0; i < cGDEntries; i++, pGDTmp++)
        *pGDTmp = RT_LE2H_U32(*pGDTmp);
}

/**
 * Read the grain directory and allocated grain tables verifying them against
 * their back up copies if available.
 *
 * @returns VBox status code.
 * @param   pImage          Image instance data.
 * @param   pExtent         The VMDK extent.
 */
static int vmdkReadGrainDirectory(PVMDKIMAGE pImage, PVMDKEXTENT pExtent)
{
    int rc = VINF_SUCCESS;
    size_t cbGD = pExtent->cGDEntries * sizeof(uint32_t);

    AssertReturn((   pExtent->enmType == VMDKETYPE_HOSTED_SPARSE
                  && pExtent->uSectorGD != VMDK_GD_AT_END
                  && pExtent->uSectorRGD != VMDK_GD_AT_END), VERR_INTERNAL_ERROR);

    rc = vmdkAllocGrainDirectory(pImage, pExtent);
    if (RT_SUCCESS(rc))
    {
        /* The VMDK 1.1 spec seems to talk about compressed grain directories,
         * but in reality they are not compressed. */
        rc = vdIfIoIntFileReadSync(pImage->pIfIo, pExtent->pFile->pStorage,
                                   VMDK_SECTOR2BYTE(pExtent->uSectorGD),
                                   pExtent->pGD, cbGD);
        if (RT_SUCCESS(rc))
        {
            vmdkGrainDirectoryConvToHost(pExtent->pGD, pExtent->cGDEntries);

            if (   pExtent->uSectorRGD
                && !(pImage->uOpenFlags & VD_OPEN_FLAGS_SKIP_CONSISTENCY_CHECKS))
            {
                /* The VMDK 1.1 spec seems to talk about compressed grain directories,
                 * but in reality they are not compressed. */
                rc = vdIfIoIntFileReadSync(pImage->pIfIo, pExtent->pFile->pStorage,
                                           VMDK_SECTOR2BYTE(pExtent->uSectorRGD),
                                           pExtent->pRGD, cbGD);
                if (RT_SUCCESS(rc))
                {
                    vmdkGrainDirectoryConvToHost(pExtent->pRGD, pExtent->cGDEntries);

                    /* Check grain table and redundant grain table for consistency. */
                    size_t cbGT = pExtent->cGTEntries * sizeof(uint32_t);
                    size_t cbGTBuffers = cbGT; /* Start with space for one GT. */
                    size_t cbGTBuffersMax = _1M;

                    uint32_t *pTmpGT1 = (uint32_t *)RTMemAlloc(cbGTBuffers);
                    uint32_t *pTmpGT2 = (uint32_t *)RTMemAlloc(cbGTBuffers);

                    if (   !pTmpGT1
                        || !pTmpGT2)
                        rc = VERR_NO_MEMORY;

                    size_t i = 0;
                    uint32_t *pGDTmp = pExtent->pGD;
                    uint32_t *pRGDTmp = pExtent->pRGD;

                    /* Loop through all entries. */
                    while (i < pExtent->cGDEntries)
                    {
                        uint32_t uGTStart = *pGDTmp;
                        uint32_t uRGTStart = *pRGDTmp;
                        size_t   cbGTRead = cbGT;

                        /* If no grain table is allocated skip the entry. */
                        if (*pGDTmp == 0 && *pRGDTmp == 0)
                        {
                            i++;
                            continue;
                        }

                        if (*pGDTmp == 0 || *pRGDTmp == 0 || *pGDTmp == *pRGDTmp)
                        {
                            /* Just one grain directory entry refers to a not yet allocated
                             * grain table or both grain directory copies refer to the same
                             * grain table. Not allowed. */
                            rc = vdIfError(pImage->pIfError, VERR_VD_VMDK_INVALID_HEADER, RT_SRC_POS,
                                           N_("VMDK: inconsistent references to grain directory in '%s'"), pExtent->pszFullname);
                            break;
                        }

                        i++;
                        pGDTmp++;
                        pRGDTmp++;

                        /*
                         * Read a few tables at once if adjacent to decrease the number
                         * of I/O requests. Read at maximum 1MB at once.
                         */
                        while (   i < pExtent->cGDEntries
                               && cbGTRead < cbGTBuffersMax)
                        {
                            /* If no grain table is allocated skip the entry. */
                            if (*pGDTmp == 0 && *pRGDTmp == 0)
                            {
                                i++;
                                continue;
                            }

                            if (*pGDTmp == 0 || *pRGDTmp == 0 || *pGDTmp == *pRGDTmp)
                            {
                                /* Just one grain directory entry refers to a not yet allocated
                                 * grain table or both grain directory copies refer to the same
                                 * grain table. Not allowed. */
                                rc = vdIfError(pImage->pIfError, VERR_VD_VMDK_INVALID_HEADER, RT_SRC_POS,
                                               N_("VMDK: inconsistent references to grain directory in '%s'"), pExtent->pszFullname);
                                break;
                            }

                            /* Check that the start offsets are adjacent.*/
                            if (   VMDK_SECTOR2BYTE(uGTStart) + cbGTRead != VMDK_SECTOR2BYTE(*pGDTmp)
                                || VMDK_SECTOR2BYTE(uRGTStart) + cbGTRead != VMDK_SECTOR2BYTE(*pRGDTmp))
                                break;

                            i++;
                            pGDTmp++;
                            pRGDTmp++;
                            cbGTRead += cbGT;
                        }

                        /* Increase buffers if required. */
                        if (   RT_SUCCESS(rc)
                            && cbGTBuffers < cbGTRead)
                        {
                            uint32_t *pTmp;
                            pTmp = (uint32_t *)RTMemRealloc(pTmpGT1, cbGTRead);
                            if (pTmp)
                            {
                                pTmpGT1 = pTmp;
                                pTmp = (uint32_t *)RTMemRealloc(pTmpGT2, cbGTRead);
                                if (pTmp)
                                    pTmpGT2 = pTmp;
                                else
                                    rc = VERR_NO_MEMORY;
                            }
                            else
                                rc = VERR_NO_MEMORY;

                            if (rc == VERR_NO_MEMORY)
                            {
                                /* Reset to the old values. */
                                rc = VINF_SUCCESS;
                                i -= cbGTRead / cbGT;
                                cbGTRead = cbGT;

                                /* Don't try to increase the buffer again in the next run. */
                                cbGTBuffersMax = cbGTBuffers;
                            }
                        }

                        if (RT_SUCCESS(rc))
                        {
                           /* The VMDK 1.1 spec seems to talk about compressed grain tables,
                             * but in reality they are not compressed. */
                            rc = vdIfIoIntFileReadSync(pImage->pIfIo, pExtent->pFile->pStorage,
                                                       VMDK_SECTOR2BYTE(uGTStart),
                                                       pTmpGT1, cbGTRead);
                            if (RT_FAILURE(rc))
                            {
                                rc = vdIfError(pImage->pIfError, rc, RT_SRC_POS,
                                               N_("VMDK: error reading grain table in '%s'"), pExtent->pszFullname);
                                break;
                            }
                            /* The VMDK 1.1 spec seems to talk about compressed grain tables,
                             * but in reality they are not compressed. */
                            rc = vdIfIoIntFileReadSync(pImage->pIfIo, pExtent->pFile->pStorage,
                                                       VMDK_SECTOR2BYTE(uRGTStart),
                                                       pTmpGT2, cbGTRead);
                            if (RT_FAILURE(rc))
                            {
                                rc = vdIfError(pImage->pIfError, rc, RT_SRC_POS,
                                               N_("VMDK: error reading backup grain table in '%s'"), pExtent->pszFullname);
                                break;
                            }
                            if (memcmp(pTmpGT1, pTmpGT2, cbGTRead))
                            {
                                rc = vdIfError(pImage->pIfError, VERR_VD_VMDK_INVALID_HEADER, RT_SRC_POS,
                                               N_("VMDK: inconsistency between grain table and backup grain table in '%s'"), pExtent->pszFullname);
                                break;
                            }
                        }
                    } /* while (i < pExtent->cGDEntries) */

                    /** @todo figure out what to do for unclean VMDKs. */
                    if (pTmpGT1)
                        RTMemFree(pTmpGT1);
                    if (pTmpGT2)
                        RTMemFree(pTmpGT2);
                }
                else
                    rc = vdIfError(pImage->pIfError, rc, RT_SRC_POS,
                                   N_("VMDK: could not read redundant grain directory in '%s'"), pExtent->pszFullname);
            }
        }
        else
            rc = vdIfError(pImage->pIfError, rc, RT_SRC_POS,
                           N_("VMDK: could not read grain directory in '%s': %Rrc"), pExtent->pszFullname, rc);
    }

    if (RT_FAILURE(rc))
        vmdkFreeGrainDirectory(pExtent);
    return rc;
}

/**
 * Creates a new grain directory for the given extent at the given start sector.
 *
 * @returns VBox status code.
 * @param   pImage          Image instance data.
 * @param   pExtent         The VMDK extent.
 * @param   uStartSector    Where the grain directory should be stored in the image.
 * @param   fPreAlloc       Flag whether to pre allocate the grain tables at this point.
 */
static int vmdkCreateGrainDirectory(PVMDKIMAGE pImage, PVMDKEXTENT pExtent,
                                    uint64_t uStartSector, bool fPreAlloc)
{
    int rc = VINF_SUCCESS;
    unsigned i;
    size_t cbGD = pExtent->cGDEntries * sizeof(uint32_t);
    size_t cbGDRounded = RT_ALIGN_64(cbGD, 512);
    size_t cbGTRounded;
    uint64_t cbOverhead;

    if (fPreAlloc)
    {
        cbGTRounded = RT_ALIGN_64(pExtent->cGDEntries * pExtent->cGTEntries * sizeof(uint32_t), 512);
        cbOverhead  = VMDK_SECTOR2BYTE(uStartSector) + cbGDRounded + cbGTRounded;
    }
    else
    {
        /* Use a dummy start sector for layout computation. */
        if (uStartSector == VMDK_GD_AT_END)
            uStartSector = 1;
        cbGTRounded = 0;
        cbOverhead = VMDK_SECTOR2BYTE(uStartSector) + cbGDRounded;
    }

    /* For streamOptimized extents there is only one grain directory,
     * and for all others take redundant grain directory into account. */
    if (pImage->uImageFlags & VD_VMDK_IMAGE_FLAGS_STREAM_OPTIMIZED)
    {
        cbOverhead = RT_ALIGN_64(cbOverhead,
                                 VMDK_SECTOR2BYTE(pExtent->cSectorsPerGrain));
    }
    else
    {
        cbOverhead += cbGDRounded + cbGTRounded;
        cbOverhead = RT_ALIGN_64(cbOverhead,
                                 VMDK_SECTOR2BYTE(pExtent->cSectorsPerGrain));
        rc = vdIfIoIntFileSetSize(pImage->pIfIo, pExtent->pFile->pStorage, cbOverhead);
    }

    if (RT_SUCCESS(rc))
    {
        pExtent->uAppendPosition = cbOverhead;
        pExtent->cOverheadSectors = VMDK_BYTE2SECTOR(cbOverhead);

        if (pImage->uImageFlags & VD_VMDK_IMAGE_FLAGS_STREAM_OPTIMIZED)
        {
            pExtent->uSectorRGD = 0;
            pExtent->uSectorGD = uStartSector;
        }
        else
        {
            pExtent->uSectorRGD = uStartSector;
            pExtent->uSectorGD = uStartSector + VMDK_BYTE2SECTOR(cbGDRounded + cbGTRounded);
        }

        rc = vmdkAllocStreamBuffers(pImage, pExtent);
        if (RT_SUCCESS(rc))
        {
            rc = vmdkAllocGrainDirectory(pImage, pExtent);
            if (   RT_SUCCESS(rc)
                && fPreAlloc)
            {
                uint32_t uGTSectorLE;
                uint64_t uOffsetSectors;

                if (pExtent->pRGD)
                {
                    uOffsetSectors = pExtent->uSectorRGD + VMDK_BYTE2SECTOR(cbGDRounded);
                    for (i = 0; i < pExtent->cGDEntries; i++)
                    {
                        pExtent->pRGD[i] = uOffsetSectors;
                        uGTSectorLE = RT_H2LE_U64(uOffsetSectors);
                        /* Write the redundant grain directory entry to disk. */
                        rc = vdIfIoIntFileWriteSync(pImage->pIfIo, pExtent->pFile->pStorage,
                                                    VMDK_SECTOR2BYTE(pExtent->uSectorRGD) + i * sizeof(uGTSectorLE),
                                                    &uGTSectorLE, sizeof(uGTSectorLE));
                        if (RT_FAILURE(rc))
                        {
                            rc = vdIfError(pImage->pIfError, rc, RT_SRC_POS, N_("VMDK: cannot write new redundant grain directory entry in '%s'"), pExtent->pszFullname);
                            break;
                        }
                        uOffsetSectors += VMDK_BYTE2SECTOR(pExtent->cGTEntries * sizeof(uint32_t));
                    }
                }

                if (RT_SUCCESS(rc))
                {
                    uOffsetSectors = pExtent->uSectorGD + VMDK_BYTE2SECTOR(cbGDRounded);
                    for (i = 0; i < pExtent->cGDEntries; i++)
                    {
                        pExtent->pGD[i] = uOffsetSectors;
                        uGTSectorLE = RT_H2LE_U64(uOffsetSectors);
                        /* Write the grain directory entry to disk. */
                        rc = vdIfIoIntFileWriteSync(pImage->pIfIo, pExtent->pFile->pStorage,
                                                    VMDK_SECTOR2BYTE(pExtent->uSectorGD) + i * sizeof(uGTSectorLE),
                                                    &uGTSectorLE, sizeof(uGTSectorLE));
                        if (RT_FAILURE(rc))
                        {
                            rc = vdIfError(pImage->pIfError, rc, RT_SRC_POS, N_("VMDK: cannot write new grain directory entry in '%s'"), pExtent->pszFullname);
                            break;
                        }
                        uOffsetSectors += VMDK_BYTE2SECTOR(pExtent->cGTEntries * sizeof(uint32_t));
                    }
                }
            }
        }
    }

    if (RT_FAILURE(rc))
        vmdkFreeGrainDirectory(pExtent);
    return rc;
}

/**
 * Unquotes the given string returning the result in a separate buffer.
 *
 * @returns VBox status code.
 * @param   pImage          The VMDK image state.
 * @param   pszStr          The string to unquote.
 * @param   ppszUnquoted    Where to store the return value, use RTMemTmpFree to
 *                          free.
 * @param   ppszNext        Where to store the pointer to any character following
 *                          the quoted value, optional.
 */
static int vmdkStringUnquote(PVMDKIMAGE pImage, const char *pszStr,
                             char **ppszUnquoted, char **ppszNext)
{
    const char *pszStart = pszStr;
    char *pszQ;
    char *pszUnquoted;

    /* Skip over whitespace. */
    while (*pszStr == ' ' || *pszStr == '\t')
        pszStr++;

    if (*pszStr != '"')
    {
        pszQ = (char *)pszStr;
        while (*pszQ && *pszQ != ' ' && *pszQ != '\t')
            pszQ++;
    }
    else
    {
        pszStr++;
        pszQ = (char *)strchr(pszStr, '"');
        if (pszQ == NULL)
            return vdIfError(pImage->pIfError, VERR_VD_VMDK_INVALID_HEADER, RT_SRC_POS, N_("VMDK: incorrectly quoted value in descriptor in '%s' (raw value %s)"),
                             pImage->pszFilename, pszStart);
    }

    pszUnquoted = (char *)RTMemTmpAlloc(pszQ - pszStr + 1);
    if (!pszUnquoted)
        return VERR_NO_MEMORY;
    memcpy(pszUnquoted, pszStr, pszQ - pszStr);
    pszUnquoted[pszQ - pszStr] = '\0';
    *ppszUnquoted = pszUnquoted;
    if (ppszNext)
        *ppszNext = pszQ + 1;
    return VINF_SUCCESS;
}

static int vmdkDescInitStr(PVMDKIMAGE pImage, PVMDKDESCRIPTOR pDescriptor,
                           const char *pszLine)
{
    char *pEnd = pDescriptor->aLines[pDescriptor->cLines];
    ssize_t cbDiff = strlen(pszLine) + 1;

    if (    pDescriptor->cLines >= VMDK_DESCRIPTOR_LINES_MAX - 1
        &&  pEnd - pDescriptor->aLines[0] > (ptrdiff_t)pDescriptor->cbDescAlloc - cbDiff)
        return vdIfError(pImage->pIfError, VERR_BUFFER_OVERFLOW, RT_SRC_POS, N_("VMDK: descriptor too big in '%s'"), pImage->pszFilename);

    memcpy(pEnd, pszLine, cbDiff);
    pDescriptor->cLines++;
    pDescriptor->aLines[pDescriptor->cLines] = pEnd + cbDiff;
    pDescriptor->fDirty = true;

    return VINF_SUCCESS;
}

static bool vmdkDescGetStr(PVMDKDESCRIPTOR pDescriptor, unsigned uStart,
                           const char *pszKey, const char **ppszValue)
{
    size_t cbKey = strlen(pszKey);
    const char *pszValue;

    while (uStart != 0)
    {
        if (!strncmp(pDescriptor->aLines[uStart], pszKey, cbKey))
        {
            /* Key matches, check for a '=' (preceded by whitespace). */
            pszValue = pDescriptor->aLines[uStart] + cbKey;
            while (*pszValue == ' ' || *pszValue == '\t')
                pszValue++;
            if (*pszValue == '=')
            {
                *ppszValue = pszValue + 1;
                break;
            }
        }
        uStart = pDescriptor->aNextLines[uStart];
    }
    return !!uStart;
}

static int vmdkDescSetStr(PVMDKIMAGE pImage, PVMDKDESCRIPTOR pDescriptor,
                          unsigned uStart,
                          const char *pszKey, const char *pszValue)
{
    char *pszTmp = NULL; /* (MSC naturally cannot figure this isn't used uninitialized) */
    size_t cbKey = strlen(pszKey);
    unsigned uLast = 0;

    while (uStart != 0)
    {
        if (!strncmp(pDescriptor->aLines[uStart], pszKey, cbKey))
        {
            /* Key matches, check for a '=' (preceded by whitespace). */
            pszTmp = pDescriptor->aLines[uStart] + cbKey;
            while (*pszTmp == ' ' || *pszTmp == '\t')
                pszTmp++;
            if (*pszTmp == '=')
            {
                pszTmp++;
                /** @todo r=bird: Doesn't skipping trailing blanks here just cause unecessary
                 *        bloat and potentially out of space error? */
                while (*pszTmp == ' ' || *pszTmp == '\t')
                    pszTmp++;
                break;
            }
        }
        if (!pDescriptor->aNextLines[uStart])
            uLast = uStart;
        uStart = pDescriptor->aNextLines[uStart];
    }
    if (uStart)
    {
        if (pszValue)
        {
            /* Key already exists, replace existing value. */
            size_t cbOldVal = strlen(pszTmp);
            size_t cbNewVal = strlen(pszValue);
            ssize_t cbDiff = cbNewVal - cbOldVal;
            /* Check for buffer overflow. */
            if (  pDescriptor->aLines[pDescriptor->cLines] - pDescriptor->aLines[0]
                > (ptrdiff_t)pDescriptor->cbDescAlloc - cbDiff)
                return vdIfError(pImage->pIfError, VERR_BUFFER_OVERFLOW, RT_SRC_POS, N_("VMDK: descriptor too big in '%s'"), pImage->pszFilename);

            memmove(pszTmp + cbNewVal, pszTmp + cbOldVal,
                    pDescriptor->aLines[pDescriptor->cLines] - pszTmp - cbOldVal);
            memcpy(pszTmp, pszValue, cbNewVal + 1);
            for (unsigned i = uStart + 1; i <= pDescriptor->cLines; i++)
                pDescriptor->aLines[i] += cbDiff;
        }
        else
        {
            memmove(pDescriptor->aLines[uStart], pDescriptor->aLines[uStart+1],
                    pDescriptor->aLines[pDescriptor->cLines] - pDescriptor->aLines[uStart+1] + 1);
            for (unsigned i = uStart + 1; i <= pDescriptor->cLines; i++)
            {
                pDescriptor->aLines[i-1] = pDescriptor->aLines[i];
                if (pDescriptor->aNextLines[i])
                    pDescriptor->aNextLines[i-1] = pDescriptor->aNextLines[i] - 1;
                else
                    pDescriptor->aNextLines[i-1] = 0;
            }
            pDescriptor->cLines--;
            /* Adjust starting line numbers of following descriptor sections. */
            if (uStart < pDescriptor->uFirstExtent)
                pDescriptor->uFirstExtent--;
            if (uStart < pDescriptor->uFirstDDB)
                pDescriptor->uFirstDDB--;
        }
    }
    else
    {
        /* Key doesn't exist, append after the last entry in this category. */
        if (!pszValue)
        {
            /* Key doesn't exist, and it should be removed. Simply a no-op. */
            return VINF_SUCCESS;
        }
        cbKey = strlen(pszKey);
        size_t cbValue = strlen(pszValue);
        ssize_t cbDiff = cbKey + 1 + cbValue + 1;
        /* Check for buffer overflow. */
        if (   (pDescriptor->cLines >= VMDK_DESCRIPTOR_LINES_MAX - 1)
            || (  pDescriptor->aLines[pDescriptor->cLines]
                - pDescriptor->aLines[0] > (ptrdiff_t)pDescriptor->cbDescAlloc - cbDiff))
            return vdIfError(pImage->pIfError, VERR_BUFFER_OVERFLOW, RT_SRC_POS, N_("VMDK: descriptor too big in '%s'"), pImage->pszFilename);
        for (unsigned i = pDescriptor->cLines + 1; i > uLast + 1; i--)
        {
            pDescriptor->aLines[i] = pDescriptor->aLines[i - 1];
            if (pDescriptor->aNextLines[i - 1])
                pDescriptor->aNextLines[i] = pDescriptor->aNextLines[i - 1] + 1;
            else
                pDescriptor->aNextLines[i] = 0;
        }
        uStart = uLast + 1;
        pDescriptor->aNextLines[uLast] = uStart;
        pDescriptor->aNextLines[uStart] = 0;
        pDescriptor->cLines++;
        pszTmp = pDescriptor->aLines[uStart];
        memmove(pszTmp + cbDiff, pszTmp,
                pDescriptor->aLines[pDescriptor->cLines] - pszTmp);
        memcpy(pDescriptor->aLines[uStart], pszKey, cbKey);
        pDescriptor->aLines[uStart][cbKey] = '=';
        memcpy(pDescriptor->aLines[uStart] + cbKey + 1, pszValue, cbValue + 1);
        for (unsigned i = uStart + 1; i <= pDescriptor->cLines; i++)
            pDescriptor->aLines[i] += cbDiff;

        /* Adjust starting line numbers of following descriptor sections. */
        if (uStart <= pDescriptor->uFirstExtent)
            pDescriptor->uFirstExtent++;
        if (uStart <= pDescriptor->uFirstDDB)
            pDescriptor->uFirstDDB++;
    }
    pDescriptor->fDirty = true;
    return VINF_SUCCESS;
}

static int vmdkDescBaseGetU32(PVMDKDESCRIPTOR pDescriptor, const char *pszKey,
                              uint32_t *puValue)
{
    const char *pszValue;

    if (!vmdkDescGetStr(pDescriptor, pDescriptor->uFirstDesc, pszKey,
                        &pszValue))
        return VERR_VD_VMDK_VALUE_NOT_FOUND;
    return RTStrToUInt32Ex(pszValue, NULL, 10, puValue);
}

/**
 * Returns the value of the given key as a string allocating the necessary memory.
 *
 * @returns VBox status code.
 * @retval  VERR_VD_VMDK_VALUE_NOT_FOUND if the value could not be found.
 * @param   pImage          The VMDK image state.
 * @param   pDescriptor     The descriptor to fetch the value from.
 * @param   pszKey          The key to get the value from.
 * @param   ppszValue       Where to store the return value, use RTMemTmpFree to
 *                          free.
 */
static int vmdkDescBaseGetStr(PVMDKIMAGE pImage, PVMDKDESCRIPTOR pDescriptor,
                              const char *pszKey, char **ppszValue)
{
    const char *pszValue;
    char *pszValueUnquoted;

    if (!vmdkDescGetStr(pDescriptor, pDescriptor->uFirstDesc, pszKey,
                        &pszValue))
        return VERR_VD_VMDK_VALUE_NOT_FOUND;
    int rc = vmdkStringUnquote(pImage, pszValue, &pszValueUnquoted, NULL);
    if (RT_FAILURE(rc))
        return rc;
    *ppszValue = pszValueUnquoted;
    return rc;
}

static int vmdkDescBaseSetStr(PVMDKIMAGE pImage, PVMDKDESCRIPTOR pDescriptor,
                              const char *pszKey, const char *pszValue)
{
    char *pszValueQuoted;

    RTStrAPrintf(&pszValueQuoted, "\"%s\"", pszValue);
    if (!pszValueQuoted)
        return VERR_NO_STR_MEMORY;
    int rc = vmdkDescSetStr(pImage, pDescriptor, pDescriptor->uFirstDesc, pszKey,
                            pszValueQuoted);
    RTStrFree(pszValueQuoted);
    return rc;
}

static void vmdkDescExtRemoveDummy(PVMDKIMAGE pImage,
                                   PVMDKDESCRIPTOR pDescriptor)
{
    RT_NOREF1(pImage);
    unsigned uEntry = pDescriptor->uFirstExtent;
    ssize_t cbDiff;

    if (!uEntry)
        return;

    cbDiff = strlen(pDescriptor->aLines[uEntry]) + 1;
    /* Move everything including \0 in the entry marking the end of buffer. */
    memmove(pDescriptor->aLines[uEntry], pDescriptor->aLines[uEntry + 1],
            pDescriptor->aLines[pDescriptor->cLines] - pDescriptor->aLines[uEntry + 1] + 1);
    for (unsigned i = uEntry + 1; i <= pDescriptor->cLines; i++)
    {
        pDescriptor->aLines[i - 1] = pDescriptor->aLines[i] - cbDiff;
        if (pDescriptor->aNextLines[i])
            pDescriptor->aNextLines[i - 1] = pDescriptor->aNextLines[i] - 1;
        else
            pDescriptor->aNextLines[i - 1] = 0;
    }
    pDescriptor->cLines--;
    if (pDescriptor->uFirstDDB)
        pDescriptor->uFirstDDB--;

    return;
}

static int vmdkDescExtInsert(PVMDKIMAGE pImage, PVMDKDESCRIPTOR pDescriptor,
                             VMDKACCESS enmAccess, uint64_t cNominalSectors,
                             VMDKETYPE enmType, const char *pszBasename,
                             uint64_t uSectorOffset)
{
    static const char *apszAccess[] = { "NOACCESS", "RDONLY", "RW" };
    static const char *apszType[] = { "", "SPARSE", "FLAT", "ZERO", "VMFS" };
    char *pszTmp;
    unsigned uStart = pDescriptor->uFirstExtent, uLast = 0;
    char szExt[1024];
    ssize_t cbDiff;

    Assert((unsigned)enmAccess < RT_ELEMENTS(apszAccess));
    Assert((unsigned)enmType < RT_ELEMENTS(apszType));

    /* Find last entry in extent description. */
    while (uStart)
    {
        if (!pDescriptor->aNextLines[uStart])
            uLast = uStart;
        uStart = pDescriptor->aNextLines[uStart];
    }

    if (enmType == VMDKETYPE_ZERO)
    {
        RTStrPrintf(szExt, sizeof(szExt), "%s %llu %s ", apszAccess[enmAccess],
                    cNominalSectors, apszType[enmType]);
    }
    else if (enmType == VMDKETYPE_FLAT)
    {
        RTStrPrintf(szExt, sizeof(szExt), "%s %llu %s \"%s\" %llu",
                    apszAccess[enmAccess], cNominalSectors,
                    apszType[enmType], pszBasename, uSectorOffset);
    }
    else
    {
        RTStrPrintf(szExt, sizeof(szExt), "%s %llu %s \"%s\"",
                    apszAccess[enmAccess], cNominalSectors,
                    apszType[enmType], pszBasename);
    }
    cbDiff = strlen(szExt) + 1;

    /* Check for buffer overflow. */
    if (   (pDescriptor->cLines >= VMDK_DESCRIPTOR_LINES_MAX - 1)
        || (  pDescriptor->aLines[pDescriptor->cLines]
            - pDescriptor->aLines[0] > (ptrdiff_t)pDescriptor->cbDescAlloc - cbDiff))
        return vdIfError(pImage->pIfError, VERR_BUFFER_OVERFLOW, RT_SRC_POS, N_("VMDK: descriptor too big in '%s'"), pImage->pszFilename);

    for (unsigned i = pDescriptor->cLines + 1; i > uLast + 1; i--)
    {
        pDescriptor->aLines[i] = pDescriptor->aLines[i - 1];
        if (pDescriptor->aNextLines[i - 1])
            pDescriptor->aNextLines[i] = pDescriptor->aNextLines[i - 1] + 1;
        else
            pDescriptor->aNextLines[i] = 0;
    }
    uStart = uLast + 1;
    pDescriptor->aNextLines[uLast] = uStart;
    pDescriptor->aNextLines[uStart] = 0;
    pDescriptor->cLines++;
    pszTmp = pDescriptor->aLines[uStart];
    memmove(pszTmp + cbDiff, pszTmp,
            pDescriptor->aLines[pDescriptor->cLines] - pszTmp);
    memcpy(pDescriptor->aLines[uStart], szExt, cbDiff);
    for (unsigned i = uStart + 1; i <= pDescriptor->cLines; i++)
        pDescriptor->aLines[i] += cbDiff;

    /* Adjust starting line numbers of following descriptor sections. */
    if (uStart <= pDescriptor->uFirstDDB)
        pDescriptor->uFirstDDB++;

    pDescriptor->fDirty = true;
    return VINF_SUCCESS;
}

/**
 * Returns the value of the given key from the DDB as a string allocating
 * the necessary memory.
 *
 * @returns VBox status code.
 * @retval  VERR_VD_VMDK_VALUE_NOT_FOUND if the value could not be found.
 * @param   pImage          The VMDK image state.
 * @param   pDescriptor     The descriptor to fetch the value from.
 * @param   pszKey          The key to get the value from.
 * @param   ppszValue       Where to store the return value, use RTMemTmpFree to
 *                          free.
 */
static int vmdkDescDDBGetStr(PVMDKIMAGE pImage, PVMDKDESCRIPTOR pDescriptor,
                             const char *pszKey, char **ppszValue)
{
    const char *pszValue;
    char *pszValueUnquoted;

    if (!vmdkDescGetStr(pDescriptor, pDescriptor->uFirstDDB, pszKey,
                        &pszValue))
        return VERR_VD_VMDK_VALUE_NOT_FOUND;
    int rc = vmdkStringUnquote(pImage, pszValue, &pszValueUnquoted, NULL);
    if (RT_FAILURE(rc))
        return rc;
    *ppszValue = pszValueUnquoted;
    return rc;
}

static int vmdkDescDDBGetU32(PVMDKIMAGE pImage, PVMDKDESCRIPTOR pDescriptor,
                             const char *pszKey, uint32_t *puValue)
{
    const char *pszValue;
    char *pszValueUnquoted;

    if (!vmdkDescGetStr(pDescriptor, pDescriptor->uFirstDDB, pszKey,
                        &pszValue))
        return VERR_VD_VMDK_VALUE_NOT_FOUND;
    int rc = vmdkStringUnquote(pImage, pszValue, &pszValueUnquoted, NULL);
    if (RT_FAILURE(rc))
        return rc;
    rc = RTStrToUInt32Ex(pszValueUnquoted, NULL, 10, puValue);
    RTMemTmpFree(pszValueUnquoted);
    return rc;
}

static int vmdkDescDDBGetUuid(PVMDKIMAGE pImage, PVMDKDESCRIPTOR pDescriptor,
                              const char *pszKey, PRTUUID pUuid)
{
    const char *pszValue;
    char *pszValueUnquoted;

    if (!vmdkDescGetStr(pDescriptor, pDescriptor->uFirstDDB, pszKey,
                        &pszValue))
        return VERR_VD_VMDK_VALUE_NOT_FOUND;
    int rc = vmdkStringUnquote(pImage, pszValue, &pszValueUnquoted, NULL);
    if (RT_FAILURE(rc))
        return rc;
    rc = RTUuidFromStr(pUuid, pszValueUnquoted);
    RTMemTmpFree(pszValueUnquoted);
    return rc;
}

static int vmdkDescDDBSetStr(PVMDKIMAGE pImage, PVMDKDESCRIPTOR pDescriptor,
                             const char *pszKey, const char *pszVal)
{
    int rc;
    char *pszValQuoted;

    if (pszVal)
    {
        RTStrAPrintf(&pszValQuoted, "\"%s\"", pszVal);
        if (!pszValQuoted)
            return VERR_NO_STR_MEMORY;
    }
    else
        pszValQuoted = NULL;
    rc = vmdkDescSetStr(pImage, pDescriptor, pDescriptor->uFirstDDB, pszKey,
                        pszValQuoted);
    if (pszValQuoted)
        RTStrFree(pszValQuoted);
    return rc;
}

static int vmdkDescDDBSetUuid(PVMDKIMAGE pImage, PVMDKDESCRIPTOR pDescriptor,
                              const char *pszKey, PCRTUUID pUuid)
{
    char *pszUuid;

    RTStrAPrintf(&pszUuid, "\"%RTuuid\"", pUuid);
    if (!pszUuid)
        return VERR_NO_STR_MEMORY;
    int rc = vmdkDescSetStr(pImage, pDescriptor, pDescriptor->uFirstDDB, pszKey,
                            pszUuid);
    RTStrFree(pszUuid);
    return rc;
}

static int vmdkDescDDBSetU32(PVMDKIMAGE pImage, PVMDKDESCRIPTOR pDescriptor,
                             const char *pszKey, uint32_t uValue)
{
    char *pszValue;

    RTStrAPrintf(&pszValue, "\"%d\"", uValue);
    if (!pszValue)
        return VERR_NO_STR_MEMORY;
    int rc = vmdkDescSetStr(pImage, pDescriptor, pDescriptor->uFirstDDB, pszKey,
                            pszValue);
    RTStrFree(pszValue);
    return rc;
}

/**
 * Splits the descriptor data into individual lines checking for correct line
 * endings and descriptor size.
 *
 * @returns VBox status code.
 * @param   pImage          The image instance.
 * @param   pDesc           The descriptor.
 * @param   pszTmp          The raw descriptor data from the image.
 */
static int vmdkDescSplitLines(PVMDKIMAGE pImage, PVMDKDESCRIPTOR pDesc, char *pszTmp)
{
    unsigned cLine = 0;
    int rc = VINF_SUCCESS;

    while (   RT_SUCCESS(rc)
           && *pszTmp != '\0')
    {
        pDesc->aLines[cLine++] = pszTmp;
        if (cLine >= VMDK_DESCRIPTOR_LINES_MAX)
        {
            vdIfError(pImage->pIfError, VERR_VD_VMDK_INVALID_HEADER, RT_SRC_POS, N_("VMDK: descriptor too big in '%s'"), pImage->pszFilename);
            rc = VERR_VD_VMDK_INVALID_HEADER;
            break;
        }

        while (*pszTmp != '\0' && *pszTmp != '\n')
        {
            if (*pszTmp == '\r')
            {
                if (*(pszTmp + 1) != '\n')
                {
                    rc = vdIfError(pImage->pIfError, VERR_VD_VMDK_INVALID_HEADER, RT_SRC_POS, N_("VMDK: unsupported end of line in descriptor in '%s'"), pImage->pszFilename);
                    break;
                }
                else
                {
                    /* Get rid of CR character. */
                    *pszTmp = '\0';
                }
            }
            pszTmp++;
        }

        if (RT_FAILURE(rc))
            break;

        /* Get rid of LF character. */
        if (*pszTmp == '\n')
        {
            *pszTmp = '\0';
            pszTmp++;
        }
    }

    if (RT_SUCCESS(rc))
    {
        pDesc->cLines = cLine;
        /* Pointer right after the end of the used part of the buffer. */
        pDesc->aLines[cLine] = pszTmp;
    }

    return rc;
}

static int vmdkPreprocessDescriptor(PVMDKIMAGE pImage, char *pDescData,
                                    size_t cbDescData, PVMDKDESCRIPTOR pDescriptor)
{
    pDescriptor->cbDescAlloc = cbDescData;
    int rc = vmdkDescSplitLines(pImage, pDescriptor, pDescData);
    if (RT_SUCCESS(rc))
    {
        if (    strcmp(pDescriptor->aLines[0], "# Disk DescriptorFile")
            &&  strcmp(pDescriptor->aLines[0], "# Disk Descriptor File")
            &&  strcmp(pDescriptor->aLines[0], "#Disk Descriptor File")
            &&  strcmp(pDescriptor->aLines[0], "#Disk DescriptorFile"))
            rc = vdIfError(pImage->pIfError, VERR_VD_VMDK_INVALID_HEADER, RT_SRC_POS,
                           N_("VMDK: descriptor does not start as expected in '%s'"), pImage->pszFilename);
        else
        {
            unsigned uLastNonEmptyLine = 0;

            /* Initialize those, because we need to be able to reopen an image. */
            pDescriptor->uFirstDesc = 0;
            pDescriptor->uFirstExtent = 0;
            pDescriptor->uFirstDDB = 0;
            for (unsigned i = 0; i < pDescriptor->cLines; i++)
            {
                if (*pDescriptor->aLines[i] != '#' && *pDescriptor->aLines[i] != '\0')
                {
                    if (    !strncmp(pDescriptor->aLines[i], "RW", 2)
                        ||  !strncmp(pDescriptor->aLines[i], "RDONLY", 6)
                        ||  !strncmp(pDescriptor->aLines[i], "NOACCESS", 8) )
                    {
                        /* An extent descriptor. */
                        if (!pDescriptor->uFirstDesc || pDescriptor->uFirstDDB)
                        {
                            /* Incorrect ordering of entries. */
                            rc = vdIfError(pImage->pIfError, VERR_VD_VMDK_INVALID_HEADER, RT_SRC_POS,
                                           N_("VMDK: incorrect ordering of entries in descriptor in '%s'"), pImage->pszFilename);
                            break;
                        }
                        if (!pDescriptor->uFirstExtent)
                        {
                            pDescriptor->uFirstExtent = i;
                            uLastNonEmptyLine = 0;
                        }
                    }
                    else if (!strncmp(pDescriptor->aLines[i], "ddb.", 4))
                    {
                        /* A disk database entry. */
                        if (!pDescriptor->uFirstDesc || !pDescriptor->uFirstExtent)
                        {
                            /* Incorrect ordering of entries. */
                            rc = vdIfError(pImage->pIfError, VERR_VD_VMDK_INVALID_HEADER, RT_SRC_POS,
                                           N_("VMDK: incorrect ordering of entries in descriptor in '%s'"), pImage->pszFilename);
                            break;
                        }
                        if (!pDescriptor->uFirstDDB)
                        {
                            pDescriptor->uFirstDDB = i;
                            uLastNonEmptyLine = 0;
                        }
                    }
                    else
                    {
                        /* A normal entry. */
                        if (pDescriptor->uFirstExtent || pDescriptor->uFirstDDB)
                        {
                            /* Incorrect ordering of entries. */
                            rc = vdIfError(pImage->pIfError, VERR_VD_VMDK_INVALID_HEADER, RT_SRC_POS,
                                           N_("VMDK: incorrect ordering of entries in descriptor in '%s'"), pImage->pszFilename);
                            break;
                        }
                        if (!pDescriptor->uFirstDesc)
                        {
                            pDescriptor->uFirstDesc = i;
                            uLastNonEmptyLine = 0;
                        }
                    }
                    if (uLastNonEmptyLine)
                        pDescriptor->aNextLines[uLastNonEmptyLine] = i;
                    uLastNonEmptyLine = i;
                }
            }
        }
    }

    return rc;
}

static int vmdkDescSetPCHSGeometry(PVMDKIMAGE pImage,
                                   PCVDGEOMETRY pPCHSGeometry)
{
    int rc = vmdkDescDDBSetU32(pImage, &pImage->Descriptor,
                           VMDK_DDB_GEO_PCHS_CYLINDERS,
                           pPCHSGeometry->cCylinders);
    if (RT_FAILURE(rc))
        return rc;
    rc = vmdkDescDDBSetU32(pImage, &pImage->Descriptor,
                           VMDK_DDB_GEO_PCHS_HEADS,
                           pPCHSGeometry->cHeads);
    if (RT_FAILURE(rc))
        return rc;
    rc = vmdkDescDDBSetU32(pImage, &pImage->Descriptor,
                           VMDK_DDB_GEO_PCHS_SECTORS,
                           pPCHSGeometry->cSectors);
    return rc;
}

static int vmdkDescSetLCHSGeometry(PVMDKIMAGE pImage,
                                   PCVDGEOMETRY pLCHSGeometry)
{
    int rc = vmdkDescDDBSetU32(pImage, &pImage->Descriptor,
                           VMDK_DDB_GEO_LCHS_CYLINDERS,
                           pLCHSGeometry->cCylinders);
    if (RT_FAILURE(rc))
        return rc;
    rc = vmdkDescDDBSetU32(pImage, &pImage->Descriptor,
                           VMDK_DDB_GEO_LCHS_HEADS,

                           pLCHSGeometry->cHeads);
    if (RT_FAILURE(rc))
        return rc;
    rc = vmdkDescDDBSetU32(pImage, &pImage->Descriptor,
                           VMDK_DDB_GEO_LCHS_SECTORS,
                           pLCHSGeometry->cSectors);
    return rc;
}

static int vmdkCreateDescriptor(PVMDKIMAGE pImage, char *pDescData,
                                size_t cbDescData, PVMDKDESCRIPTOR pDescriptor)
{
    pDescriptor->uFirstDesc = 0;
    pDescriptor->uFirstExtent = 0;
    pDescriptor->uFirstDDB = 0;
    pDescriptor->cLines = 0;
    pDescriptor->cbDescAlloc = cbDescData;
    pDescriptor->fDirty = false;
    pDescriptor->aLines[pDescriptor->cLines] = pDescData;
    memset(pDescriptor->aNextLines, '\0', sizeof(pDescriptor->aNextLines));

    int rc = vmdkDescInitStr(pImage, pDescriptor, "# Disk DescriptorFile");
    if (RT_SUCCESS(rc))
        rc = vmdkDescInitStr(pImage, pDescriptor, "version=1");
    if (RT_SUCCESS(rc))
    {
        pDescriptor->uFirstDesc = pDescriptor->cLines - 1;
        rc = vmdkDescInitStr(pImage, pDescriptor, "");
    }
    if (RT_SUCCESS(rc))
        rc = vmdkDescInitStr(pImage, pDescriptor, "# Extent description");
    if (RT_SUCCESS(rc))
        rc = vmdkDescInitStr(pImage, pDescriptor, "NOACCESS 0 ZERO ");
    if (RT_SUCCESS(rc))
    {
        pDescriptor->uFirstExtent = pDescriptor->cLines - 1;
        rc = vmdkDescInitStr(pImage, pDescriptor, "");
    }
    if (RT_SUCCESS(rc))
    {
        /* The trailing space is created by VMware, too. */
        rc = vmdkDescInitStr(pImage, pDescriptor, "# The disk Data Base ");
    }
    if (RT_SUCCESS(rc))
        rc = vmdkDescInitStr(pImage, pDescriptor, "#DDB");
    if (RT_SUCCESS(rc))
        rc = vmdkDescInitStr(pImage, pDescriptor, "");
    if (RT_SUCCESS(rc))
        rc = vmdkDescInitStr(pImage, pDescriptor, "ddb.virtualHWVersion = \"4\"");
    if (RT_SUCCESS(rc))
    {
        pDescriptor->uFirstDDB = pDescriptor->cLines - 1;

        /* Now that the framework is in place, use the normal functions to insert
         * the remaining keys. */
        char szBuf[9];
        RTStrPrintf(szBuf, sizeof(szBuf), "%08x", RTRandU32());
        rc = vmdkDescSetStr(pImage, pDescriptor, pDescriptor->uFirstDesc,
                            "CID", szBuf);
    }
    if (RT_SUCCESS(rc))
        rc = vmdkDescSetStr(pImage, pDescriptor, pDescriptor->uFirstDesc,
                            "parentCID", "ffffffff");
    if (RT_SUCCESS(rc))
        rc = vmdkDescDDBSetStr(pImage, pDescriptor, "ddb.adapterType", "ide");

    return rc;
}

static int vmdkParseDescriptor(PVMDKIMAGE pImage, char *pDescData, size_t cbDescData)
{
    int rc;
    unsigned cExtents;
    unsigned uLine;
    unsigned i;

    rc = vmdkPreprocessDescriptor(pImage, pDescData, cbDescData,
                                  &pImage->Descriptor);
    if (RT_FAILURE(rc))
        return rc;

    /* Check version, must be 1. */
    uint32_t uVersion;
    rc = vmdkDescBaseGetU32(&pImage->Descriptor, "version", &uVersion);
    if (RT_FAILURE(rc))
        return vdIfError(pImage->pIfError, rc, RT_SRC_POS, N_("VMDK: error finding key 'version' in descriptor in '%s'"), pImage->pszFilename);
    if (uVersion != 1)
        return vdIfError(pImage->pIfError, VERR_VD_VMDK_UNSUPPORTED_VERSION, RT_SRC_POS, N_("VMDK: unsupported format version in descriptor in '%s'"), pImage->pszFilename);

    /* Get image creation type and determine image flags. */
    char *pszCreateType = NULL;   /* initialized to make gcc shut up */
    rc = vmdkDescBaseGetStr(pImage, &pImage->Descriptor, "createType",
                            &pszCreateType);
    if (RT_FAILURE(rc))
        return vdIfError(pImage->pIfError, rc, RT_SRC_POS, N_("VMDK: cannot get image type from descriptor in '%s'"), pImage->pszFilename);
    if (    !strcmp(pszCreateType, "twoGbMaxExtentSparse")
        ||  !strcmp(pszCreateType, "twoGbMaxExtentFlat"))
        pImage->uImageFlags |= VD_VMDK_IMAGE_FLAGS_SPLIT_2G;
    else if (   !strcmp(pszCreateType, "partitionedDevice")
             || !strcmp(pszCreateType, "fullDevice"))
        pImage->uImageFlags |= VD_VMDK_IMAGE_FLAGS_RAWDISK;
    else if (!strcmp(pszCreateType, "streamOptimized"))
        pImage->uImageFlags |= VD_VMDK_IMAGE_FLAGS_STREAM_OPTIMIZED;
    else if (!strcmp(pszCreateType, "vmfs"))
        pImage->uImageFlags |= VD_IMAGE_FLAGS_FIXED | VD_VMDK_IMAGE_FLAGS_ESX;
    RTMemTmpFree(pszCreateType);

    /* Count the number of extent config entries. */
    for (uLine = pImage->Descriptor.uFirstExtent, cExtents = 0;
         uLine != 0;
         uLine = pImage->Descriptor.aNextLines[uLine], cExtents++)
        /* nothing */;

    if (!pImage->pDescData && cExtents != 1)
    {
        /* Monolithic image, must have only one extent (already opened). */
        return vdIfError(pImage->pIfError, VERR_VD_VMDK_INVALID_HEADER, RT_SRC_POS, N_("VMDK: monolithic image may only have one extent in '%s'"), pImage->pszFilename);
    }

    if (pImage->pDescData)
    {
        /* Non-monolithic image, extents need to be allocated. */
        rc = vmdkCreateExtents(pImage, cExtents);
        if (RT_FAILURE(rc))
            return rc;
    }

    for (i = 0, uLine = pImage->Descriptor.uFirstExtent;
         i < cExtents; i++, uLine = pImage->Descriptor.aNextLines[uLine])
    {
        char *pszLine = pImage->Descriptor.aLines[uLine];

        /* Access type of the extent. */
        if (!strncmp(pszLine, "RW", 2))
        {
            pImage->pExtents[i].enmAccess = VMDKACCESS_READWRITE;
            pszLine += 2;
        }
        else if (!strncmp(pszLine, "RDONLY", 6))
        {
            pImage->pExtents[i].enmAccess = VMDKACCESS_READONLY;
            pszLine += 6;
        }
        else if (!strncmp(pszLine, "NOACCESS", 8))
        {
            pImage->pExtents[i].enmAccess = VMDKACCESS_NOACCESS;
            pszLine += 8;
        }
        else
            return vdIfError(pImage->pIfError, VERR_VD_VMDK_INVALID_HEADER, RT_SRC_POS, N_("VMDK: parse error in extent description in '%s'"), pImage->pszFilename);
        if (*pszLine++ != ' ')
            return vdIfError(pImage->pIfError, VERR_VD_VMDK_INVALID_HEADER, RT_SRC_POS, N_("VMDK: parse error in extent description in '%s'"), pImage->pszFilename);

        /* Nominal size of the extent. */
        rc = RTStrToUInt64Ex(pszLine, &pszLine, 10,
                             &pImage->pExtents[i].cNominalSectors);
        if (RT_FAILURE(rc))
            return vdIfError(pImage->pIfError, VERR_VD_VMDK_INVALID_HEADER, RT_SRC_POS, N_("VMDK: parse error in extent description in '%s'"), pImage->pszFilename);
        if (*pszLine++ != ' ')
            return vdIfError(pImage->pIfError, VERR_VD_VMDK_INVALID_HEADER, RT_SRC_POS, N_("VMDK: parse error in extent description in '%s'"), pImage->pszFilename);

        /* Type of the extent. */
        if (!strncmp(pszLine, "SPARSE", 6))
        {
            pImage->pExtents[i].enmType = VMDKETYPE_HOSTED_SPARSE;
            pszLine += 6;
        }
        else if (!strncmp(pszLine, "FLAT", 4))
        {
            pImage->pExtents[i].enmType = VMDKETYPE_FLAT;
            pszLine += 4;
        }
        else if (!strncmp(pszLine, "ZERO", 4))
        {
            pImage->pExtents[i].enmType = VMDKETYPE_ZERO;
            pszLine += 4;
        }
        else if (!strncmp(pszLine, "VMFS", 4))
        {
            pImage->pExtents[i].enmType = VMDKETYPE_VMFS;
            pszLine += 4;
        }
        else
            return vdIfError(pImage->pIfError, VERR_VD_VMDK_INVALID_HEADER, RT_SRC_POS, N_("VMDK: parse error in extent description in '%s'"), pImage->pszFilename);

        if (pImage->pExtents[i].enmType == VMDKETYPE_ZERO)
        {
            /* This one has no basename or offset. */
            if (*pszLine == ' ')
                pszLine++;
            if (*pszLine != '\0')
                return vdIfError(pImage->pIfError, VERR_VD_VMDK_INVALID_HEADER, RT_SRC_POS, N_("VMDK: parse error in extent description in '%s'"), pImage->pszFilename);
            pImage->pExtents[i].pszBasename = NULL;
        }
        else
        {
            /* All other extent types have basename and optional offset. */
            if (*pszLine++ != ' ')
                return vdIfError(pImage->pIfError, VERR_VD_VMDK_INVALID_HEADER, RT_SRC_POS, N_("VMDK: parse error in extent description in '%s'"), pImage->pszFilename);

            /* Basename of the image. Surrounded by quotes. */
            char *pszBasename;
            rc = vmdkStringUnquote(pImage, pszLine, &pszBasename, &pszLine);
            if (RT_FAILURE(rc))
                return rc;
            pImage->pExtents[i].pszBasename = pszBasename;
            if (*pszLine == ' ')
            {
                pszLine++;
                if (*pszLine != '\0')
                {
                    /* Optional offset in extent specified. */
                    rc = RTStrToUInt64Ex(pszLine, &pszLine, 10,
                                         &pImage->pExtents[i].uSectorOffset);
                    if (RT_FAILURE(rc))
                        return vdIfError(pImage->pIfError, VERR_VD_VMDK_INVALID_HEADER, RT_SRC_POS, N_("VMDK: parse error in extent description in '%s'"), pImage->pszFilename);
                }
            }

            if (*pszLine != '\0')
                return vdIfError(pImage->pIfError, VERR_VD_VMDK_INVALID_HEADER, RT_SRC_POS, N_("VMDK: parse error in extent description in '%s'"), pImage->pszFilename);
        }
    }

    /* Determine PCHS geometry (autogenerate if necessary). */
    rc = vmdkDescDDBGetU32(pImage, &pImage->Descriptor,
                           VMDK_DDB_GEO_PCHS_CYLINDERS,
                           &pImage->PCHSGeometry.cCylinders);
    if (rc == VERR_VD_VMDK_VALUE_NOT_FOUND)
        pImage->PCHSGeometry.cCylinders = 0;
    else if (RT_FAILURE(rc))
        return vdIfError(pImage->pIfError, rc, RT_SRC_POS, N_("VMDK: error getting PCHS geometry from extent description in '%s'"), pImage->pszFilename);
    rc = vmdkDescDDBGetU32(pImage, &pImage->Descriptor,
                           VMDK_DDB_GEO_PCHS_HEADS,
                           &pImage->PCHSGeometry.cHeads);
    if (rc == VERR_VD_VMDK_VALUE_NOT_FOUND)
        pImage->PCHSGeometry.cHeads = 0;
    else if (RT_FAILURE(rc))
        return vdIfError(pImage->pIfError, rc, RT_SRC_POS, N_("VMDK: error getting PCHS geometry from extent description in '%s'"), pImage->pszFilename);
    rc = vmdkDescDDBGetU32(pImage, &pImage->Descriptor,
                           VMDK_DDB_GEO_PCHS_SECTORS,
                           &pImage->PCHSGeometry.cSectors);
    if (rc == VERR_VD_VMDK_VALUE_NOT_FOUND)
        pImage->PCHSGeometry.cSectors = 0;
    else if (RT_FAILURE(rc))
        return vdIfError(pImage->pIfError, rc, RT_SRC_POS, N_("VMDK: error getting PCHS geometry from extent description in '%s'"), pImage->pszFilename);
    if (    pImage->PCHSGeometry.cCylinders == 0
        ||  pImage->PCHSGeometry.cHeads == 0
        ||  pImage->PCHSGeometry.cHeads > 16
        ||  pImage->PCHSGeometry.cSectors == 0
        ||  pImage->PCHSGeometry.cSectors > 63)
    {
        /* Mark PCHS geometry as not yet valid (can't do the calculation here
         * as the total image size isn't known yet). */
        pImage->PCHSGeometry.cCylinders = 0;
        pImage->PCHSGeometry.cHeads = 16;
        pImage->PCHSGeometry.cSectors = 63;
    }

    /* Determine LCHS geometry (set to 0 if not specified). */
    rc = vmdkDescDDBGetU32(pImage, &pImage->Descriptor,
                           VMDK_DDB_GEO_LCHS_CYLINDERS,
                           &pImage->LCHSGeometry.cCylinders);
    if (rc == VERR_VD_VMDK_VALUE_NOT_FOUND)
        pImage->LCHSGeometry.cCylinders = 0;
    else if (RT_FAILURE(rc))
        return vdIfError(pImage->pIfError, rc, RT_SRC_POS, N_("VMDK: error getting LCHS geometry from extent description in '%s'"), pImage->pszFilename);
    rc = vmdkDescDDBGetU32(pImage, &pImage->Descriptor,
                           VMDK_DDB_GEO_LCHS_HEADS,
                           &pImage->LCHSGeometry.cHeads);
    if (rc == VERR_VD_VMDK_VALUE_NOT_FOUND)
        pImage->LCHSGeometry.cHeads = 0;
    else if (RT_FAILURE(rc))
        return vdIfError(pImage->pIfError, rc, RT_SRC_POS, N_("VMDK: error getting LCHS geometry from extent description in '%s'"), pImage->pszFilename);
    rc = vmdkDescDDBGetU32(pImage, &pImage->Descriptor,
                           VMDK_DDB_GEO_LCHS_SECTORS,
                           &pImage->LCHSGeometry.cSectors);
    if (rc == VERR_VD_VMDK_VALUE_NOT_FOUND)
        pImage->LCHSGeometry.cSectors = 0;
    else if (RT_FAILURE(rc))
        return vdIfError(pImage->pIfError, rc, RT_SRC_POS, N_("VMDK: error getting LCHS geometry from extent description in '%s'"), pImage->pszFilename);
    if (    pImage->LCHSGeometry.cCylinders == 0
        ||  pImage->LCHSGeometry.cHeads == 0
        ||  pImage->LCHSGeometry.cSectors == 0)
    {
        pImage->LCHSGeometry.cCylinders = 0;
        pImage->LCHSGeometry.cHeads = 0;
        pImage->LCHSGeometry.cSectors = 0;
    }

    /* Get image UUID. */
    rc = vmdkDescDDBGetUuid(pImage, &pImage->Descriptor, VMDK_DDB_IMAGE_UUID,
                            &pImage->ImageUuid);
    if (rc == VERR_VD_VMDK_VALUE_NOT_FOUND)
    {
        /* Image without UUID. Probably created by VMware and not yet used
         * by VirtualBox. Can only be added for images opened in read/write
         * mode, so don't bother producing a sensible UUID otherwise. */
        if (pImage->uOpenFlags & VD_OPEN_FLAGS_READONLY)
            RTUuidClear(&pImage->ImageUuid);
        else
        {
            rc = RTUuidCreate(&pImage->ImageUuid);
            if (RT_FAILURE(rc))
                return rc;
            rc = vmdkDescDDBSetUuid(pImage, &pImage->Descriptor,
                                    VMDK_DDB_IMAGE_UUID, &pImage->ImageUuid);
            if (RT_FAILURE(rc))
                return vdIfError(pImage->pIfError, rc, RT_SRC_POS, N_("VMDK: error storing image UUID in descriptor in '%s'"), pImage->pszFilename);
        }
    }
    else if (RT_FAILURE(rc))
        return rc;

    /* Get image modification UUID. */
    rc = vmdkDescDDBGetUuid(pImage, &pImage->Descriptor,
                            VMDK_DDB_MODIFICATION_UUID,
                            &pImage->ModificationUuid);
    if (rc == VERR_VD_VMDK_VALUE_NOT_FOUND)
    {
        /* Image without UUID. Probably created by VMware and not yet used
         * by VirtualBox. Can only be added for images opened in read/write
         * mode, so don't bother producing a sensible UUID otherwise. */
        if (pImage->uOpenFlags & VD_OPEN_FLAGS_READONLY)
            RTUuidClear(&pImage->ModificationUuid);
        else
        {
            rc = RTUuidCreate(&pImage->ModificationUuid);
            if (RT_FAILURE(rc))
                return rc;
            rc = vmdkDescDDBSetUuid(pImage, &pImage->Descriptor,
                                    VMDK_DDB_MODIFICATION_UUID,
                                    &pImage->ModificationUuid);
            if (RT_FAILURE(rc))
                return vdIfError(pImage->pIfError, rc, RT_SRC_POS, N_("VMDK: error storing image modification UUID in descriptor in '%s'"), pImage->pszFilename);
        }
    }
    else if (RT_FAILURE(rc))
        return rc;

    /* Get UUID of parent image. */
    rc = vmdkDescDDBGetUuid(pImage, &pImage->Descriptor, VMDK_DDB_PARENT_UUID,
                            &pImage->ParentUuid);
    if (rc == VERR_VD_VMDK_VALUE_NOT_FOUND)
    {
        /* Image without UUID. Probably created by VMware and not yet used
         * by VirtualBox. Can only be added for images opened in read/write
         * mode, so don't bother producing a sensible UUID otherwise. */
        if (pImage->uOpenFlags & VD_OPEN_FLAGS_READONLY)
            RTUuidClear(&pImage->ParentUuid);
        else
        {
            rc = RTUuidClear(&pImage->ParentUuid);
            if (RT_FAILURE(rc))
                return rc;
            rc = vmdkDescDDBSetUuid(pImage, &pImage->Descriptor,
                                    VMDK_DDB_PARENT_UUID, &pImage->ParentUuid);
            if (RT_FAILURE(rc))
                return vdIfError(pImage->pIfError, rc, RT_SRC_POS, N_("VMDK: error storing parent UUID in descriptor in '%s'"), pImage->pszFilename);
        }
    }
    else if (RT_FAILURE(rc))
        return rc;

    /* Get parent image modification UUID. */
    rc = vmdkDescDDBGetUuid(pImage, &pImage->Descriptor,
                            VMDK_DDB_PARENT_MODIFICATION_UUID,
                            &pImage->ParentModificationUuid);
    if (rc == VERR_VD_VMDK_VALUE_NOT_FOUND)
    {
        /* Image without UUID. Probably created by VMware and not yet used
         * by VirtualBox. Can only be added for images opened in read/write
         * mode, so don't bother producing a sensible UUID otherwise. */
        if (pImage->uOpenFlags & VD_OPEN_FLAGS_READONLY)
            RTUuidClear(&pImage->ParentModificationUuid);
        else
        {
            RTUuidClear(&pImage->ParentModificationUuid);
            rc = vmdkDescDDBSetUuid(pImage, &pImage->Descriptor,
                                    VMDK_DDB_PARENT_MODIFICATION_UUID,
                                    &pImage->ParentModificationUuid);
            if (RT_FAILURE(rc))
                return vdIfError(pImage->pIfError, rc, RT_SRC_POS, N_("VMDK: error storing parent modification UUID in descriptor in '%s'"), pImage->pszFilename);
        }
    }
    else if (RT_FAILURE(rc))
        return rc;

    return VINF_SUCCESS;
}

/**
 * Internal : Prepares the descriptor to write to the image.
 */
static int vmdkDescriptorPrepare(PVMDKIMAGE pImage, uint64_t cbLimit,
                                 void **ppvData, size_t *pcbData)
{
    int rc = VINF_SUCCESS;

    /*
     * Allocate temporary descriptor buffer.
     * In case there is no limit allocate a default
     * and increase if required.
     */
    size_t cbDescriptor = cbLimit ? cbLimit : 4 * _1K;
    char *pszDescriptor = (char *)RTMemAllocZ(cbDescriptor);
    size_t offDescriptor = 0;

    if (!pszDescriptor)
        return VERR_NO_MEMORY;

    for (unsigned i = 0; i < pImage->Descriptor.cLines; i++)
    {
        const char *psz = pImage->Descriptor.aLines[i];
        size_t cb = strlen(psz);

        /*
         * Increase the descriptor if there is no limit and
         * there is not enough room left for this line.
         */
        if (offDescriptor + cb + 1 > cbDescriptor)
        {
            if (cbLimit)
            {
                rc = vdIfError(pImage->pIfError, VERR_BUFFER_OVERFLOW, RT_SRC_POS, N_("VMDK: descriptor too long in '%s'"), pImage->pszFilename);
                break;
            }
            else
            {
                char *pszDescriptorNew = NULL;
                LogFlow(("Increasing descriptor cache\n"));

                pszDescriptorNew = (char *)RTMemRealloc(pszDescriptor, cbDescriptor + cb + 4 * _1K);
                if (!pszDescriptorNew)
                {
                    rc = VERR_NO_MEMORY;
                    break;
                }
                pszDescriptor = pszDescriptorNew;
                cbDescriptor += cb + 4 * _1K;
            }
        }

        if (cb > 0)
        {
            memcpy(pszDescriptor + offDescriptor, psz, cb);
            offDescriptor += cb;
        }

        memcpy(pszDescriptor + offDescriptor, "\n", 1);
        offDescriptor++;
    }

    if (RT_SUCCESS(rc))
    {
        *ppvData = pszDescriptor;
        *pcbData = offDescriptor;
    }
    else if (pszDescriptor)
        RTMemFree(pszDescriptor);

    return rc;
}

/**
 * Internal: write/update the descriptor part of the image.
 */
static int vmdkWriteDescriptor(PVMDKIMAGE pImage, PVDIOCTX pIoCtx)
{
    int rc = VINF_SUCCESS;
    uint64_t cbLimit;
    uint64_t uOffset;
    PVMDKFILE pDescFile;
    void *pvDescriptor = NULL;
    size_t cbDescriptor;

    if (pImage->pDescData)
    {
        /* Separate descriptor file. */
        uOffset = 0;
        cbLimit = 0;
        pDescFile = pImage->pFile;
    }
    else
    {
        /* Embedded descriptor file. */
        uOffset = VMDK_SECTOR2BYTE(pImage->pExtents[0].uDescriptorSector);
        cbLimit = VMDK_SECTOR2BYTE(pImage->pExtents[0].cDescriptorSectors);
        pDescFile = pImage->pExtents[0].pFile;
    }
    /* Bail out if there is no file to write to. */
    if (pDescFile == NULL)
        return VERR_INVALID_PARAMETER;

    rc = vmdkDescriptorPrepare(pImage, cbLimit, &pvDescriptor, &cbDescriptor);
    if (RT_SUCCESS(rc))
    {
        rc = vdIfIoIntFileWriteMeta(pImage->pIfIo, pDescFile->pStorage,
                                    uOffset, pvDescriptor,
                                    cbLimit ? cbLimit : cbDescriptor,
                                    pIoCtx, NULL, NULL);
        if (   RT_FAILURE(rc)
            && rc != VERR_VD_ASYNC_IO_IN_PROGRESS)
            rc = vdIfError(pImage->pIfError, rc, RT_SRC_POS, N_("VMDK: error writing descriptor in '%s'"), pImage->pszFilename);
    }

    if (RT_SUCCESS(rc) && !cbLimit)
    {
        rc = vdIfIoIntFileSetSize(pImage->pIfIo, pDescFile->pStorage, cbDescriptor);
        if (RT_FAILURE(rc))
            rc = vdIfError(pImage->pIfError, rc, RT_SRC_POS, N_("VMDK: error truncating descriptor in '%s'"), pImage->pszFilename);
    }

    if (RT_SUCCESS(rc))
        pImage->Descriptor.fDirty = false;

    if (pvDescriptor)
        RTMemFree(pvDescriptor);
    return rc;

}

/**
 * Internal: validate the consistency check values in a binary header.
 */
static int vmdkValidateHeader(PVMDKIMAGE pImage, PVMDKEXTENT pExtent, const SparseExtentHeader *pHeader)
{
    int rc = VINF_SUCCESS;
    if (RT_LE2H_U32(pHeader->magicNumber) != VMDK_SPARSE_MAGICNUMBER)
    {
        rc = vdIfError(pImage->pIfError, VERR_VD_VMDK_INVALID_HEADER, RT_SRC_POS, N_("VMDK: incorrect magic in sparse extent header in '%s'"), pExtent->pszFullname);
        return rc;
    }
    if (RT_LE2H_U32(pHeader->version) != 1 && RT_LE2H_U32(pHeader->version) != 3)
    {
        rc = vdIfError(pImage->pIfError, VERR_VD_VMDK_UNSUPPORTED_VERSION, RT_SRC_POS, N_("VMDK: incorrect version in sparse extent header in '%s', not a VMDK 1.0/1.1 conforming file"), pExtent->pszFullname);
        return rc;
    }
    if (    (RT_LE2H_U32(pHeader->flags) & 1)
        &&  (   pHeader->singleEndLineChar != '\n'
             || pHeader->nonEndLineChar != ' '
             || pHeader->doubleEndLineChar1 != '\r'
             || pHeader->doubleEndLineChar2 != '\n') )
    {
        rc = vdIfError(pImage->pIfError, VERR_VD_VMDK_INVALID_HEADER, RT_SRC_POS, N_("VMDK: corrupted by CR/LF translation in '%s'"), pExtent->pszFullname);
        return rc;
    }
    if (RT_LE2H_U64(pHeader->descriptorSize) > VMDK_SPARSE_DESCRIPTOR_SIZE_MAX)
    {
        rc = vdIfError(pImage->pIfError, VERR_VD_VMDK_INVALID_HEADER, RT_SRC_POS, N_("VMDK: descriptor size out of bounds (%llu vs %llu) '%s'"),
                       pExtent->pszFullname, RT_LE2H_U64(pHeader->descriptorSize), VMDK_SPARSE_DESCRIPTOR_SIZE_MAX);
        return rc;
    }
    return rc;
}

/**
 * Internal: read metadata belonging to an extent with binary header, i.e.
 * as found in monolithic files.
 */
static int vmdkReadBinaryMetaExtent(PVMDKIMAGE pImage, PVMDKEXTENT pExtent,
                                    bool fMagicAlreadyRead)
{
    SparseExtentHeader Header;
    int rc;

    if (!fMagicAlreadyRead)
        rc = vdIfIoIntFileReadSync(pImage->pIfIo, pExtent->pFile->pStorage, 0,
                                   &Header, sizeof(Header));
    else
    {
        Header.magicNumber = RT_H2LE_U32(VMDK_SPARSE_MAGICNUMBER);
        rc = vdIfIoIntFileReadSync(pImage->pIfIo, pExtent->pFile->pStorage,
                                   RT_UOFFSETOF(SparseExtentHeader, version),
                                   &Header.version,
                                     sizeof(Header)
                                   - RT_UOFFSETOF(SparseExtentHeader, version));
    }

    if (RT_SUCCESS(rc))
    {
        rc = vmdkValidateHeader(pImage, pExtent, &Header);
        if (RT_SUCCESS(rc))
        {
            uint64_t cbFile = 0;

            if (    (RT_LE2H_U32(Header.flags) & RT_BIT(17))
                &&  RT_LE2H_U64(Header.gdOffset) == VMDK_GD_AT_END)
                pExtent->fFooter = true;

            if (   !(pImage->uOpenFlags & VD_OPEN_FLAGS_READONLY)
                || (   pExtent->fFooter
                    && !(pImage->uOpenFlags & VD_OPEN_FLAGS_SEQUENTIAL)))
            {
                rc = vdIfIoIntFileGetSize(pImage->pIfIo, pExtent->pFile->pStorage, &cbFile);
                if (RT_FAILURE(rc))
                    rc = vdIfError(pImage->pIfError, rc, RT_SRC_POS, N_("VMDK: cannot get size of '%s'"), pExtent->pszFullname);
            }

            if (RT_SUCCESS(rc))
            {
                if (!(pImage->uOpenFlags & VD_OPEN_FLAGS_READONLY))
                    pExtent->uAppendPosition = RT_ALIGN_64(cbFile, 512);

                if (   pExtent->fFooter
                    && (   !(pImage->uOpenFlags & VD_OPEN_FLAGS_READONLY)
                        || !(pImage->uOpenFlags & VD_OPEN_FLAGS_SEQUENTIAL)))
                {
                    /* Read the footer, which comes before the end-of-stream marker. */
                    rc = vdIfIoIntFileReadSync(pImage->pIfIo, pExtent->pFile->pStorage,
                                               cbFile - 2*512, &Header,
                                               sizeof(Header));
                    if (RT_FAILURE(rc))
                    {
                        vdIfError(pImage->pIfError, rc, RT_SRC_POS, N_("VMDK: error reading extent footer in '%s'"), pExtent->pszFullname);
                        rc = VERR_VD_VMDK_INVALID_HEADER;
                    }

                    if (RT_SUCCESS(rc))
                        rc = vmdkValidateHeader(pImage, pExtent, &Header);
                    /* Prohibit any writes to this extent. */
                    pExtent->uAppendPosition = 0;
                }

                if (RT_SUCCESS(rc))
                {
                    pExtent->uVersion           = RT_LE2H_U32(Header.version);
                    pExtent->enmType            = VMDKETYPE_HOSTED_SPARSE; /* Just dummy value, changed later. */
                    pExtent->cSectors           = RT_LE2H_U64(Header.capacity);
                    pExtent->cSectorsPerGrain   = RT_LE2H_U64(Header.grainSize);
                    pExtent->uDescriptorSector  = RT_LE2H_U64(Header.descriptorOffset);
                    pExtent->cDescriptorSectors = RT_LE2H_U64(Header.descriptorSize);
                    pExtent->cGTEntries         = RT_LE2H_U32(Header.numGTEsPerGT);
                    pExtent->cOverheadSectors   = RT_LE2H_U64(Header.overHead);
                    pExtent->fUncleanShutdown   = !!Header.uncleanShutdown;
                    pExtent->uCompression       = RT_LE2H_U16(Header.compressAlgorithm);
                    if (RT_LE2H_U32(Header.flags) & RT_BIT(1))
                    {
                        pExtent->uSectorRGD     = RT_LE2H_U64(Header.rgdOffset);
                        pExtent->uSectorGD      = RT_LE2H_U64(Header.gdOffset);
                    }
                    else
                    {
                        pExtent->uSectorGD      = RT_LE2H_U64(Header.gdOffset);
                        pExtent->uSectorRGD     = 0;
                    }

                    if (pExtent->uDescriptorSector && !pExtent->cDescriptorSectors)
                        rc = vdIfError(pImage->pIfError, VERR_VD_VMDK_INVALID_HEADER, RT_SRC_POS,
                                       N_("VMDK: inconsistent embedded descriptor config in '%s'"), pExtent->pszFullname);

                    if (   RT_SUCCESS(rc)
                        && (   pExtent->uSectorGD == VMDK_GD_AT_END
                            || pExtent->uSectorRGD == VMDK_GD_AT_END)
                        && (   !(pImage->uOpenFlags & VD_OPEN_FLAGS_READONLY)
                            || !(pImage->uOpenFlags & VD_OPEN_FLAGS_SEQUENTIAL)))
                        rc = vdIfError(pImage->pIfError, VERR_VD_VMDK_INVALID_HEADER, RT_SRC_POS,
                                       N_("VMDK: cannot resolve grain directory offset in '%s'"), pExtent->pszFullname);

                    if (RT_SUCCESS(rc))
                    {
                        uint64_t cSectorsPerGDE = pExtent->cGTEntries * pExtent->cSectorsPerGrain;
                        if (!cSectorsPerGDE || cSectorsPerGDE > UINT32_MAX)
                            rc = vdIfError(pImage->pIfError, VERR_VD_VMDK_INVALID_HEADER, RT_SRC_POS,
                                           N_("VMDK: incorrect grain directory size in '%s'"), pExtent->pszFullname);
                        else
                        {
                            pExtent->cSectorsPerGDE = cSectorsPerGDE;
                            pExtent->cGDEntries = (pExtent->cSectors + cSectorsPerGDE - 1) / cSectorsPerGDE;

                            /* Fix up the number of descriptor sectors, as some flat images have
                             * really just one, and this causes failures when inserting the UUID
                             * values and other extra information. */
                            if (pExtent->cDescriptorSectors != 0 && pExtent->cDescriptorSectors < 4)
                            {
                                /* Do it the easy way - just fix it for flat images which have no
                                 * other complicated metadata which needs space too. */
                                if (    pExtent->uDescriptorSector + 4 < pExtent->cOverheadSectors
                                    &&  pExtent->cGTEntries * pExtent->cGDEntries == 0)
                                    pExtent->cDescriptorSectors = 4;
                            }
                        }
                    }
                }
            }
        }
    }
    else
    {
        vdIfError(pImage->pIfError, rc, RT_SRC_POS, N_("VMDK: error reading extent header in '%s'"), pExtent->pszFullname);
        rc = VERR_VD_VMDK_INVALID_HEADER;
    }

    if (RT_FAILURE(rc))
        vmdkFreeExtentData(pImage, pExtent, false);

    return rc;
}

/**
 * Internal: read additional metadata belonging to an extent. For those
 * extents which have no additional metadata just verify the information.
 */
static int vmdkReadMetaExtent(PVMDKIMAGE pImage, PVMDKEXTENT pExtent)
{
    int rc = VINF_SUCCESS;

/* disabled the check as there are too many truncated vmdk images out there */
#ifdef VBOX_WITH_VMDK_STRICT_SIZE_CHECK
    uint64_t cbExtentSize;
    /* The image must be a multiple of a sector in size and contain the data
     * area (flat images only). If not, it means the image is at least
     * truncated, or even seriously garbled. */
    rc = vdIfIoIntFileGetSize(pImage->pIfIo, pExtent->pFile->pStorage, &cbExtentSize);
    if (RT_FAILURE(rc))
        rc = vdIfError(pImage->pIfError, rc, RT_SRC_POS, N_("VMDK: error getting size in '%s'"), pExtent->pszFullname);
    else if (    cbExtentSize != RT_ALIGN_64(cbExtentSize, 512)
        &&  (pExtent->enmType != VMDKETYPE_FLAT || pExtent->cNominalSectors + pExtent->uSectorOffset > VMDK_BYTE2SECTOR(cbExtentSize)))
        rc = vdIfError(pImage->pIfError, VERR_VD_VMDK_INVALID_HEADER, RT_SRC_POS,
                       N_("VMDK: file size is not a multiple of 512 in '%s', file is truncated or otherwise garbled"), pExtent->pszFullname);
#endif /* VBOX_WITH_VMDK_STRICT_SIZE_CHECK */
    if (   RT_SUCCESS(rc)
        && pExtent->enmType == VMDKETYPE_HOSTED_SPARSE)
    {
        /* The spec says that this must be a power of two and greater than 8,
         * but probably they meant not less than 8. */
        if (    (pExtent->cSectorsPerGrain & (pExtent->cSectorsPerGrain - 1))
            ||  pExtent->cSectorsPerGrain < 8)
            rc = vdIfError(pImage->pIfError, VERR_VD_VMDK_INVALID_HEADER, RT_SRC_POS,
                           N_("VMDK: invalid extent grain size %u in '%s'"), pExtent->cSectorsPerGrain, pExtent->pszFullname);
        else
        {
            /* This code requires that a grain table must hold a power of two multiple
             * of the number of entries per GT cache entry. */
            if (    (pExtent->cGTEntries & (pExtent->cGTEntries - 1))
                ||  pExtent->cGTEntries < VMDK_GT_CACHELINE_SIZE)
                rc = vdIfError(pImage->pIfError, VERR_VD_VMDK_INVALID_HEADER, RT_SRC_POS,
                               N_("VMDK: grain table cache size problem in '%s'"), pExtent->pszFullname);
            else
            {
                rc = vmdkAllocStreamBuffers(pImage, pExtent);
                if (RT_SUCCESS(rc))
                {
                    /* Prohibit any writes to this streamOptimized extent. */
                    if (pImage->uImageFlags & VD_VMDK_IMAGE_FLAGS_STREAM_OPTIMIZED)
                        pExtent->uAppendPosition = 0;

                    if (   !(pImage->uImageFlags & VD_VMDK_IMAGE_FLAGS_STREAM_OPTIMIZED)
                        || !(pImage->uOpenFlags & VD_OPEN_FLAGS_READONLY)
                        || !(pImage->uOpenFlags & VD_OPEN_FLAGS_SEQUENTIAL))
                        rc = vmdkReadGrainDirectory(pImage, pExtent);
                    else
                    {
                        pExtent->uGrainSectorAbs = pExtent->cOverheadSectors;
                        pExtent->cbGrainStreamRead = 0;
                    }
                }
            }
        }
    }

    if (RT_FAILURE(rc))
        vmdkFreeExtentData(pImage, pExtent, false);

    return rc;
}

/**
 * Internal: write/update the metadata for a sparse extent.
 */
static int vmdkWriteMetaSparseExtent(PVMDKIMAGE pImage, PVMDKEXTENT pExtent,
                                     uint64_t uOffset, PVDIOCTX pIoCtx)
{
    SparseExtentHeader Header;

    memset(&Header, '\0', sizeof(Header));
    Header.magicNumber = RT_H2LE_U32(VMDK_SPARSE_MAGICNUMBER);
    Header.version = RT_H2LE_U32(pExtent->uVersion);
    Header.flags = RT_H2LE_U32(RT_BIT(0));
    if (pExtent->pRGD)
        Header.flags |= RT_H2LE_U32(RT_BIT(1));
    if (pImage->uImageFlags & VD_VMDK_IMAGE_FLAGS_STREAM_OPTIMIZED)
        Header.flags |= RT_H2LE_U32(RT_BIT(16) | RT_BIT(17));
    Header.capacity = RT_H2LE_U64(pExtent->cSectors);
    Header.grainSize = RT_H2LE_U64(pExtent->cSectorsPerGrain);
    Header.descriptorOffset = RT_H2LE_U64(pExtent->uDescriptorSector);
    Header.descriptorSize = RT_H2LE_U64(pExtent->cDescriptorSectors);
    Header.numGTEsPerGT = RT_H2LE_U32(pExtent->cGTEntries);
    if (pExtent->fFooter && uOffset == 0)
    {
        if (pExtent->pRGD)
        {
            Assert(pExtent->uSectorRGD);
            Header.rgdOffset = RT_H2LE_U64(VMDK_GD_AT_END);
            Header.gdOffset = RT_H2LE_U64(VMDK_GD_AT_END);
        }
        else
            Header.gdOffset = RT_H2LE_U64(VMDK_GD_AT_END);
    }
    else
    {
        if (pExtent->pRGD)
        {
            Assert(pExtent->uSectorRGD);
            Header.rgdOffset = RT_H2LE_U64(pExtent->uSectorRGD);
            Header.gdOffset = RT_H2LE_U64(pExtent->uSectorGD);
        }
        else
            Header.gdOffset = RT_H2LE_U64(pExtent->uSectorGD);
    }
    Header.overHead = RT_H2LE_U64(pExtent->cOverheadSectors);
    Header.uncleanShutdown = pExtent->fUncleanShutdown;
    Header.singleEndLineChar = '\n';
    Header.nonEndLineChar = ' ';
    Header.doubleEndLineChar1 = '\r';
    Header.doubleEndLineChar2 = '\n';
    Header.compressAlgorithm = RT_H2LE_U16(pExtent->uCompression);

    int rc = vdIfIoIntFileWriteMeta(pImage->pIfIo, pExtent->pFile->pStorage,
                                    uOffset, &Header, sizeof(Header),
                                    pIoCtx, NULL, NULL);
    if (RT_FAILURE(rc) && (rc != VERR_VD_ASYNC_IO_IN_PROGRESS))
        rc = vdIfError(pImage->pIfError, rc, RT_SRC_POS, N_("VMDK: error writing extent header in '%s'"), pExtent->pszFullname);
    return rc;
}

/**
 * Internal: free the buffers used for streamOptimized images.
 */
static void vmdkFreeStreamBuffers(PVMDKEXTENT pExtent)
{
    if (pExtent->pvCompGrain)
    {
        RTMemFree(pExtent->pvCompGrain);
        pExtent->pvCompGrain = NULL;
    }
    if (pExtent->pvGrain)
    {
        RTMemFree(pExtent->pvGrain);
        pExtent->pvGrain = NULL;
    }
}

/**
 * Internal: free the memory used by the extent data structure, optionally
 * deleting the referenced files.
 *
 * @returns VBox status code.
 * @param   pImage    Pointer to the image instance data.
 * @param   pExtent   The extent to free.
 * @param   fDelete   Flag whether to delete the backing storage.
 */
static int vmdkFreeExtentData(PVMDKIMAGE pImage, PVMDKEXTENT pExtent,
                              bool fDelete)
{
    int rc = VINF_SUCCESS;

    vmdkFreeGrainDirectory(pExtent);
    if (pExtent->pDescData)
    {
        RTMemFree(pExtent->pDescData);
        pExtent->pDescData = NULL;
    }
    if (pExtent->pFile != NULL)
    {
        /* Do not delete raw extents, these have full and base names equal. */
        rc = vmdkFileClose(pImage, &pExtent->pFile,
                              fDelete
                           && pExtent->pszFullname
                           && pExtent->pszBasename
                           && strcmp(pExtent->pszFullname, pExtent->pszBasename));
    }
    if (pExtent->pszBasename)
    {
        RTMemTmpFree((void *)pExtent->pszBasename);
        pExtent->pszBasename = NULL;
    }
    if (pExtent->pszFullname)
    {
        RTStrFree((char *)(void *)pExtent->pszFullname);
        pExtent->pszFullname = NULL;
    }
    vmdkFreeStreamBuffers(pExtent);

    return rc;
}

/**
 * Internal: allocate grain table cache if necessary for this image.
 */
static int vmdkAllocateGrainTableCache(PVMDKIMAGE pImage)
{
    PVMDKEXTENT pExtent;

    /* Allocate grain table cache if any sparse extent is present. */
    for (unsigned i = 0; i < pImage->cExtents; i++)
    {
        pExtent = &pImage->pExtents[i];
        if (pExtent->enmType == VMDKETYPE_HOSTED_SPARSE)
        {
            /* Allocate grain table cache. */
            pImage->pGTCache = (PVMDKGTCACHE)RTMemAllocZ(sizeof(VMDKGTCACHE));
            if (!pImage->pGTCache)
                return VERR_NO_MEMORY;
            for (unsigned j = 0; j < VMDK_GT_CACHE_SIZE; j++)
            {
                PVMDKGTCACHEENTRY pGCE = &pImage->pGTCache->aGTCache[j];
                pGCE->uExtent = UINT32_MAX;
            }
            pImage->pGTCache->cEntries = VMDK_GT_CACHE_SIZE;
            break;
        }
    }

    return VINF_SUCCESS;
}

/**
 * Internal: allocate the given number of extents.
 */
static int vmdkCreateExtents(PVMDKIMAGE pImage, unsigned cExtents)
{
    int rc = VINF_SUCCESS;
    PVMDKEXTENT pExtents = (PVMDKEXTENT)RTMemAllocZ(cExtents * sizeof(VMDKEXTENT));
    if (pExtents)
    {
        for (unsigned i = 0; i < cExtents; i++)
        {
            pExtents[i].pFile = NULL;
            pExtents[i].pszBasename = NULL;
            pExtents[i].pszFullname = NULL;
            pExtents[i].pGD = NULL;
            pExtents[i].pRGD = NULL;
            pExtents[i].pDescData = NULL;
            pExtents[i].uVersion = 1;
            pExtents[i].uCompression = VMDK_COMPRESSION_NONE;
            pExtents[i].uExtent = i;
            pExtents[i].pImage = pImage;
        }
        pImage->pExtents = pExtents;
        pImage->cExtents = cExtents;
    }
    else
        rc = VERR_NO_MEMORY;

    return rc;
}

/**
 * Reads and processes the descriptor embedded in sparse images.
 *
 * @returns VBox status code.
 * @param   pImage         VMDK image instance.
 * @param   pFile          The sparse file handle.
 */
static int vmdkDescriptorReadSparse(PVMDKIMAGE pImage, PVMDKFILE pFile)
{
    /* It's a hosted single-extent image. */
    int rc = vmdkCreateExtents(pImage, 1);
    if (RT_SUCCESS(rc))
    {
        /* The opened file is passed to the extent. No separate descriptor
         * file, so no need to keep anything open for the image. */
        PVMDKEXTENT pExtent = &pImage->pExtents[0];
        pExtent->pFile = pFile;
        pImage->pFile = NULL;
        pExtent->pszFullname = RTPathAbsDup(pImage->pszFilename);
        if (RT_LIKELY(pExtent->pszFullname))
        {
            /* As we're dealing with a monolithic image here, there must
             * be a descriptor embedded in the image file. */
            rc = vmdkReadBinaryMetaExtent(pImage, pExtent, true /* fMagicAlreadyRead */);
            if (   RT_SUCCESS(rc)
                && pExtent->uDescriptorSector
                && pExtent->cDescriptorSectors)
            {
                /* HACK: extend the descriptor if it is unusually small and it fits in
                 * the unused space after the image header. Allows opening VMDK files
                 * with extremely small descriptor in read/write mode.
                 *
                 * The previous version introduced a possible regression for VMDK stream
                 * optimized images from VMware which tend to have only a single sector sized
                 * descriptor. Increasing the descriptor size resulted in adding the various uuid
                 * entries required to make it work with VBox but for stream optimized images
                 * the updated binary header wasn't written to the disk creating a mismatch
                 * between advertised and real descriptor size.
                 *
                 * The descriptor size will be increased even if opened readonly now if there
                 * enough room but the new value will not be written back to the image.
                 */
                if (    pExtent->cDescriptorSectors < 3
                    &&  (int64_t)pExtent->uSectorGD - pExtent->uDescriptorSector >= 4
                    &&  (!pExtent->uSectorRGD || (int64_t)pExtent->uSectorRGD - pExtent->uDescriptorSector >= 4))
                {
                    uint64_t cDescriptorSectorsOld = pExtent->cDescriptorSectors;

                    pExtent->cDescriptorSectors = 4;
                    if (!(pImage->uOpenFlags & VD_OPEN_FLAGS_READONLY))
                    {
                        /*
                         * Update the on disk number now to make sure we don't introduce inconsistencies
                         * in case of stream optimized images from VMware where the descriptor is just
                         * one sector big (the binary header is not written to disk for complete
                         * stream optimized images in vmdkFlushImage()).
                         */
                        uint64_t u64DescSizeNew = RT_H2LE_U64(pExtent->cDescriptorSectors);
                        rc = vdIfIoIntFileWriteSync(pImage->pIfIo, pFile->pStorage,
                                                    RT_UOFFSETOF(SparseExtentHeader, descriptorSize),
                                                    &u64DescSizeNew, sizeof(u64DescSizeNew));
                        if (RT_FAILURE(rc))
                        {
                            LogFlowFunc(("Increasing the descriptor size failed with %Rrc\n", rc));
                            /* Restore the old size and carry on. */
                            pExtent->cDescriptorSectors = cDescriptorSectorsOld;
                        }
                    }
                }
                /* Read the descriptor from the extent. */
                pExtent->pDescData = (char *)RTMemAllocZ(VMDK_SECTOR2BYTE(pExtent->cDescriptorSectors));
                if (RT_LIKELY(pExtent->pDescData))
                {
                    rc = vdIfIoIntFileReadSync(pImage->pIfIo, pExtent->pFile->pStorage,
                                               VMDK_SECTOR2BYTE(pExtent->uDescriptorSector),
                                               pExtent->pDescData,
                                               VMDK_SECTOR2BYTE(pExtent->cDescriptorSectors));
                    if (RT_SUCCESS(rc))
                    {
                        rc = vmdkParseDescriptor(pImage, pExtent->pDescData,
                                                 VMDK_SECTOR2BYTE(pExtent->cDescriptorSectors));
                        if (   RT_SUCCESS(rc)
                            && (   !(pImage->uImageFlags & VD_VMDK_IMAGE_FLAGS_STREAM_OPTIMIZED)
                                || !(pImage->uOpenFlags & VD_OPEN_FLAGS_ASYNC_IO)))
                        {
                            rc = vmdkReadMetaExtent(pImage, pExtent);
                            if (RT_SUCCESS(rc))
                            {
                                /* Mark the extent as unclean if opened in read-write mode. */
                                if (   !(pImage->uOpenFlags & VD_OPEN_FLAGS_READONLY)
                                    && !(pImage->uImageFlags & VD_VMDK_IMAGE_FLAGS_STREAM_OPTIMIZED))
                                {
                                    pExtent->fUncleanShutdown = true;
                                    pExtent->fMetaDirty = true;
                                }
                            }
                        }
                        else if (RT_SUCCESS(rc))
                            rc = VERR_NOT_SUPPORTED;
                    }
                    else
                        rc = vdIfError(pImage->pIfError, rc, RT_SRC_POS, N_("VMDK: read error for descriptor in '%s'"), pExtent->pszFullname);
                }
                else
                    rc = VERR_NO_MEMORY;
            }
            else if (RT_SUCCESS(rc))
                rc = vdIfError(pImage->pIfError, VERR_VD_VMDK_INVALID_HEADER, RT_SRC_POS, N_("VMDK: monolithic image without descriptor in '%s'"), pImage->pszFilename);
        }
        else
            rc = VERR_NO_MEMORY;
    }

    return rc;
}

/**
 * Reads the descriptor from a pure text file.
 *
 * @returns VBox status code.
 * @param   pImage          VMDK image instance.
 * @param   pFile           The descriptor file handle.
 */
static int vmdkDescriptorReadAscii(PVMDKIMAGE pImage, PVMDKFILE pFile)
{
    /* Allocate at least 10K, and make sure that there is 5K free space
     * in case new entries need to be added to the descriptor. Never
     * allocate more than 128K, because that's no valid descriptor file
     * and will result in the correct "truncated read" error handling. */
    uint64_t cbFileSize;
    int rc = vdIfIoIntFileGetSize(pImage->pIfIo, pFile->pStorage, &cbFileSize);
    if (   RT_SUCCESS(rc)
        && cbFileSize >= 50)
    {
        uint64_t cbSize = cbFileSize;
        if (cbSize % VMDK_SECTOR2BYTE(10))
            cbSize += VMDK_SECTOR2BYTE(20) - cbSize % VMDK_SECTOR2BYTE(10);
        else
            cbSize += VMDK_SECTOR2BYTE(10);
        cbSize = RT_MIN(cbSize, _128K);
        pImage->cbDescAlloc = RT_MAX(VMDK_SECTOR2BYTE(20), cbSize);
        pImage->pDescData = (char *)RTMemAllocZ(pImage->cbDescAlloc);
        if (RT_LIKELY(pImage->pDescData))
        {
            rc = vdIfIoIntFileReadSync(pImage->pIfIo, pFile->pStorage, 0, pImage->pDescData,
                                       RT_MIN(pImage->cbDescAlloc, cbFileSize));
            if (RT_SUCCESS(rc))
            {
#if 0 /** @todo Revisit */
                cbRead += sizeof(u32Magic);
                if (cbRead == pImage->cbDescAlloc)
                {
                    /* Likely the read is truncated. Better fail a bit too early
                     * (normally the descriptor is much smaller than our buffer). */
                    rc = vdIfError(pImage->pIfError, VERR_VD_VMDK_INVALID_HEADER, RT_SRC_POS, N_("VMDK: cannot read descriptor in '%s'"), pImage->pszFilename);
                    goto out;
                }
#endif
                rc = vmdkParseDescriptor(pImage, pImage->pDescData,
                                         pImage->cbDescAlloc);
                if (RT_SUCCESS(rc))
                {
                    for (unsigned i = 0; i < pImage->cExtents && RT_SUCCESS(rc); i++)
                    {
                        PVMDKEXTENT pExtent = &pImage->pExtents[i];
                        if (pExtent->pszBasename)
                        {
                            /* Hack to figure out whether the specified name in the
                             * extent descriptor is absolute. Doesn't always work, but
                             * should be good enough for now. */
                            char *pszFullname;
                            /** @todo implement proper path absolute check. */
                            if (pExtent->pszBasename[0] == RTPATH_SLASH)
                            {
                                pszFullname = RTStrDup(pExtent->pszBasename);
                                if (!pszFullname)
                                {
                                    rc = VERR_NO_MEMORY;
                                    break;
                                }
                            }
                            else
                            {
                                char *pszDirname = RTStrDup(pImage->pszFilename);
                                if (!pszDirname)
                                {
                                    rc = VERR_NO_MEMORY;
                                    break;
                                }
                                RTPathStripFilename(pszDirname);
                                pszFullname = RTPathJoinA(pszDirname, pExtent->pszBasename);
                                RTStrFree(pszDirname);
                                if (!pszFullname)
                                {
                                    rc = VERR_NO_STR_MEMORY;
                                    break;
                                }
                            }
                            pExtent->pszFullname = pszFullname;
                        }
                        else
                            pExtent->pszFullname = NULL;

                        unsigned uOpenFlags = pImage->uOpenFlags | ((pExtent->enmAccess == VMDKACCESS_READONLY) ? VD_OPEN_FLAGS_READONLY : 0);
                        switch (pExtent->enmType)
                        {
                            case VMDKETYPE_HOSTED_SPARSE:
                                rc = vmdkFileOpen(pImage, &pExtent->pFile, pExtent->pszFullname,
                                                  VDOpenFlagsToFileOpenFlags(uOpenFlags, false /* fCreate */));
                                if (RT_FAILURE(rc))
                                {
                                    /* Do NOT signal an appropriate error here, as the VD
                                     * layer has the choice of retrying the open if it
                                     * failed. */
                                    break;
                                }
                                rc = vmdkReadBinaryMetaExtent(pImage, pExtent,
                                                              false /* fMagicAlreadyRead */);
                                if (RT_FAILURE(rc))
                                    break;
                                rc = vmdkReadMetaExtent(pImage, pExtent);
                                if (RT_FAILURE(rc))
                                    break;

                                /* Mark extent as unclean if opened in read-write mode. */
                                if (!(pImage->uOpenFlags & VD_OPEN_FLAGS_READONLY))
                                {
                                    pExtent->fUncleanShutdown = true;
                                    pExtent->fMetaDirty = true;
                                }
                                break;
                            case VMDKETYPE_VMFS:
                            case VMDKETYPE_FLAT:
                                rc = vmdkFileOpen(pImage, &pExtent->pFile, pExtent->pszFullname,
                                                  VDOpenFlagsToFileOpenFlags(uOpenFlags, false /* fCreate */));
                                if (RT_FAILURE(rc))
                                {
                                    /* Do NOT signal an appropriate error here, as the VD
                                     * layer has the choice of retrying the open if it
                                     * failed. */
                                    break;
                                }
                                break;
                            case VMDKETYPE_ZERO:
                                /* Nothing to do. */
                                break;
                            default:
                                AssertMsgFailed(("unknown vmdk extent type %d\n", pExtent->enmType));
                        }
                    }
                }
            }
            else
                rc = vdIfError(pImage->pIfError, rc, RT_SRC_POS, N_("VMDK: read error for descriptor in '%s'"), pImage->pszFilename);
        }
        else
            rc = VERR_NO_MEMORY;
    }
    else if (RT_SUCCESS(rc))
        rc = vdIfError(pImage->pIfError, VERR_VD_VMDK_INVALID_HEADER, RT_SRC_POS, N_("VMDK: descriptor in '%s' is too short"), pImage->pszFilename);

    return rc;
}

/**
 * Read and process the descriptor based on the image type.
 *
 * @returns VBox status code.
 * @param   pImage    VMDK image instance.
 * @param   pFile     VMDK file handle.
 */
static int vmdkDescriptorRead(PVMDKIMAGE pImage, PVMDKFILE pFile)
{
    uint32_t u32Magic;

    /* Read magic (if present). */
    int rc = vdIfIoIntFileReadSync(pImage->pIfIo, pFile->pStorage, 0,
                                   &u32Magic, sizeof(u32Magic));
    if (RT_SUCCESS(rc))
    {
        /* Handle the file according to its magic number. */
        if (RT_LE2H_U32(u32Magic) == VMDK_SPARSE_MAGICNUMBER)
            rc = vmdkDescriptorReadSparse(pImage, pFile);
        else
            rc = vmdkDescriptorReadAscii(pImage, pFile);
    }
    else
    {
        vdIfError(pImage->pIfError, rc, RT_SRC_POS, N_("VMDK: error reading the magic number in '%s'"), pImage->pszFilename);
        rc = VERR_VD_VMDK_INVALID_HEADER;
    }

    return rc;
}

/**
 * Internal: Open an image, constructing all necessary data structures.
 */
static int vmdkOpenImage(PVMDKIMAGE pImage, unsigned uOpenFlags)
{
    pImage->uOpenFlags = uOpenFlags;
    pImage->pIfError   = VDIfErrorGet(pImage->pVDIfsDisk);
    pImage->pIfIo      = VDIfIoIntGet(pImage->pVDIfsImage);
    AssertPtrReturn(pImage->pIfIo, VERR_INVALID_PARAMETER);

    /*
     * Open the image.
     * We don't have to check for asynchronous access because
     * we only support raw access and the opened file is a description
     * file were no data is stored.
     */
    PVMDKFILE pFile;
    int rc = vmdkFileOpen(pImage, &pFile, pImage->pszFilename,
                          VDOpenFlagsToFileOpenFlags(uOpenFlags, false /* fCreate */));
    if (RT_SUCCESS(rc))
    {
        pImage->pFile = pFile;

        rc = vmdkDescriptorRead(pImage, pFile);
        if (RT_SUCCESS(rc))
        {
            /* Determine PCHS geometry if not set. */
            if (pImage->PCHSGeometry.cCylinders == 0)
            {
                uint64_t cCylinders =   VMDK_BYTE2SECTOR(pImage->cbSize)
                                      / pImage->PCHSGeometry.cHeads
                                      / pImage->PCHSGeometry.cSectors;
                pImage->PCHSGeometry.cCylinders = (unsigned)RT_MIN(cCylinders, 16383);
                if (   !(pImage->uOpenFlags & VD_OPEN_FLAGS_READONLY)
                    && !(pImage->uImageFlags & VD_VMDK_IMAGE_FLAGS_STREAM_OPTIMIZED))
                {
                    rc = vmdkDescSetPCHSGeometry(pImage, &pImage->PCHSGeometry);
                    AssertRC(rc);
                }
            }

            /* Update the image metadata now in case has changed. */
            rc = vmdkFlushImage(pImage, NULL);
            if (RT_SUCCESS(rc))
            {
                /* Figure out a few per-image constants from the extents. */
                pImage->cbSize = 0;
                for (unsigned i = 0; i < pImage->cExtents; i++)
                {
                    PVMDKEXTENT pExtent = &pImage->pExtents[i];
                    if (pExtent->enmType == VMDKETYPE_HOSTED_SPARSE)
                    {
                        /* Here used to be a check whether the nominal size of an extent
                         * is a multiple of the grain size. The spec says that this is
                         * always the case, but unfortunately some files out there in the
                         * wild violate the spec (e.g. ReactOS 0.3.1). */
                    }
                    else if (    pExtent->enmType == VMDKETYPE_FLAT
                             ||  pExtent->enmType == VMDKETYPE_ZERO)
                        pImage->uImageFlags |= VD_IMAGE_FLAGS_FIXED;

                    pImage->cbSize += VMDK_SECTOR2BYTE(pExtent->cNominalSectors);
                }

                if (   !(pImage->uImageFlags & VD_VMDK_IMAGE_FLAGS_STREAM_OPTIMIZED)
                    || !(pImage->uOpenFlags & VD_OPEN_FLAGS_READONLY)
                    || !(pImage->uOpenFlags & VD_OPEN_FLAGS_SEQUENTIAL))
                    rc = vmdkAllocateGrainTableCache(pImage);
            }
        }
    }
    /* else: Do NOT signal an appropriate error here, as the VD layer has the
     *       choice of retrying the open if it failed. */

    if (RT_SUCCESS(rc))
    {
        PVDREGIONDESC pRegion = &pImage->RegionList.aRegions[0];
        pImage->RegionList.fFlags   = 0;
        pImage->RegionList.cRegions = 1;

        pRegion->offRegion            = 0; /* Disk start. */
        pRegion->cbBlock              = 512;
        pRegion->enmDataForm          = VDREGIONDATAFORM_RAW;
        pRegion->enmMetadataForm      = VDREGIONMETADATAFORM_NONE;
        pRegion->cbData               = 512;
        pRegion->cbMetadata           = 0;
        pRegion->cRegionBlocksOrBytes = pImage->cbSize;
    }
    else
        vmdkFreeImage(pImage, false, false /*fFlush*/); /* Don't try to flush anything if opening failed. */
    return rc;
}

/**
 * Internal: create VMDK images for raw disk/partition access.
 */
static int vmdkCreateRawImage(PVMDKIMAGE pImage, const PVDISKRAW pRaw,
                              uint64_t cbSize)
{
    int rc = VINF_SUCCESS;
    PVMDKEXTENT pExtent;

    if (pRaw->uFlags & VDISKRAW_DISK)
    {
        /* Full raw disk access. This requires setting up a descriptor
         * file and open the (flat) raw disk. */
        rc = vmdkCreateExtents(pImage, 1);
        if (RT_FAILURE(rc))
            return vdIfError(pImage->pIfError, rc, RT_SRC_POS, N_("VMDK: could not create new extent list in '%s'"), pImage->pszFilename);
        pExtent = &pImage->pExtents[0];
        /* Create raw disk descriptor file. */
        rc = vmdkFileOpen(pImage, &pImage->pFile, pImage->pszFilename,
                          VDOpenFlagsToFileOpenFlags(pImage->uOpenFlags,
                                                     true /* fCreate */));
        if (RT_FAILURE(rc))
            return vdIfError(pImage->pIfError, rc, RT_SRC_POS, N_("VMDK: could not create new file '%s'"), pImage->pszFilename);

        /* Set up basename for extent description. Cannot use StrDup. */
        size_t cbBasename = strlen(pRaw->pszRawDisk) + 1;
        char *pszBasename = (char *)RTMemTmpAlloc(cbBasename);
        if (!pszBasename)
            return VERR_NO_MEMORY;
        memcpy(pszBasename, pRaw->pszRawDisk, cbBasename);
        pExtent->pszBasename = pszBasename;
        /* For raw disks the full name is identical to the base name. */
        pExtent->pszFullname = RTStrDup(pszBasename);
        if (!pExtent->pszFullname)
            return VERR_NO_MEMORY;
        pExtent->enmType = VMDKETYPE_FLAT;
        pExtent->cNominalSectors = VMDK_BYTE2SECTOR(cbSize);
        pExtent->uSectorOffset = 0;
        pExtent->enmAccess = (pRaw->uFlags & VDISKRAW_READONLY) ? VMDKACCESS_READONLY : VMDKACCESS_READWRITE;
        pExtent->fMetaDirty = false;

        /* Open flat image, the raw disk. */
        rc = vmdkFileOpen(pImage, &pExtent->pFile, pExtent->pszFullname,
                          VDOpenFlagsToFileOpenFlags(pImage->uOpenFlags | ((pExtent->enmAccess == VMDKACCESS_READONLY) ? VD_OPEN_FLAGS_READONLY : 0),
                                                     false /* fCreate */));
        if (RT_FAILURE(rc))
            return vdIfError(pImage->pIfError, rc, RT_SRC_POS, N_("VMDK: could not open raw disk file '%s'"), pExtent->pszFullname);
    }
    else
    {
        /* Raw partition access. This requires setting up a descriptor
         * file, write the partition information to a flat extent and
         * open all the (flat) raw disk partitions. */

        /* First pass over the partition data areas to determine how many
         * extents we need. One data area can require up to 2 extents, as
         * it might be necessary to skip over unpartitioned space. */
        unsigned cExtents = 0;
        uint64_t uStart = 0;
        for (unsigned i = 0; i < pRaw->cPartDescs; i++)
        {
            PVDISKRAWPARTDESC pPart = &pRaw->pPartDescs[i];
            if (uStart > pPart->uStart)
                return vdIfError(pImage->pIfError, VERR_INVALID_PARAMETER, RT_SRC_POS,
                                 N_("VMDK: incorrect partition data area ordering set up by the caller in '%s'"), pImage->pszFilename);

            if (uStart < pPart->uStart)
                cExtents++;
            uStart = pPart->uStart + pPart->cbData;
            cExtents++;
        }
        /* Another extent for filling up the rest of the image. */
        if (uStart != cbSize)
            cExtents++;

        rc = vmdkCreateExtents(pImage, cExtents);
        if (RT_FAILURE(rc))
            return vdIfError(pImage->pIfError, rc, RT_SRC_POS, N_("VMDK: could not create new extent list in '%s'"), pImage->pszFilename);

        /* Create raw partition descriptor file. */
        rc = vmdkFileOpen(pImage, &pImage->pFile, pImage->pszFilename,
                          VDOpenFlagsToFileOpenFlags(pImage->uOpenFlags,
                                                     true /* fCreate */));
        if (RT_FAILURE(rc))
            return vdIfError(pImage->pIfError, rc, RT_SRC_POS, N_("VMDK: could not create new file '%s'"), pImage->pszFilename);

        /* Create base filename for the partition table extent. */
        /** @todo remove fixed buffer without creating memory leaks. */
        char pszPartition[1024];
        const char *pszBase = RTPathFilename(pImage->pszFilename);
        const char *pszSuff = RTPathSuffix(pszBase);
        if (pszSuff == NULL)
            return vdIfError(pImage->pIfError, rc, RT_SRC_POS, N_("VMDK: invalid filename '%s'"), pImage->pszFilename);
        char *pszBaseBase = RTStrDup(pszBase);
        if (!pszBaseBase)
            return VERR_NO_MEMORY;
        RTPathStripSuffix(pszBaseBase);
        RTStrPrintf(pszPartition, sizeof(pszPartition), "%s-pt%s",
                    pszBaseBase, pszSuff);
        RTStrFree(pszBaseBase);

        /* Second pass over the partitions, now define all extents. */
        uint64_t uPartOffset = 0;
        cExtents = 0;
        uStart = 0;
        for (unsigned i = 0; i < pRaw->cPartDescs; i++)
        {
            PVDISKRAWPARTDESC pPart = &pRaw->pPartDescs[i];
            pExtent = &pImage->pExtents[cExtents++];

            if (uStart < pPart->uStart)
            {
                pExtent->pszBasename = NULL;
                pExtent->pszFullname = NULL;
                pExtent->enmType = VMDKETYPE_ZERO;
                pExtent->cNominalSectors = VMDK_BYTE2SECTOR(pPart->uStart - uStart);
                pExtent->uSectorOffset = 0;
                pExtent->enmAccess = VMDKACCESS_READWRITE;
                pExtent->fMetaDirty = false;
                /* go to next extent */
                pExtent = &pImage->pExtents[cExtents++];
            }
            uStart = pPart->uStart + pPart->cbData;

            if (pPart->pvPartitionData)
            {
                /* Set up basename for extent description. Can't use StrDup. */
                size_t cbBasename = strlen(pszPartition) + 1;
                char *pszBasename = (char *)RTMemTmpAlloc(cbBasename);
                if (!pszBasename)
                    return VERR_NO_MEMORY;
                memcpy(pszBasename, pszPartition, cbBasename);
                pExtent->pszBasename = pszBasename;

                /* Set up full name for partition extent. */
                char *pszDirname = RTStrDup(pImage->pszFilename);
                if (!pszDirname)
                    return VERR_NO_STR_MEMORY;
                RTPathStripFilename(pszDirname);
                char *pszFullname = RTPathJoinA(pszDirname, pExtent->pszBasename);
                RTStrFree(pszDirname);
                if (!pszFullname)
                    return VERR_NO_STR_MEMORY;
                pExtent->pszFullname = pszFullname;
                pExtent->enmType = VMDKETYPE_FLAT;
                pExtent->cNominalSectors = VMDK_BYTE2SECTOR(pPart->cbData);
                pExtent->uSectorOffset = uPartOffset;
                pExtent->enmAccess = VMDKACCESS_READWRITE;
                pExtent->fMetaDirty = false;

                /* Create partition table flat image. */
                rc = vmdkFileOpen(pImage, &pExtent->pFile, pExtent->pszFullname,
                                  VDOpenFlagsToFileOpenFlags(pImage->uOpenFlags | ((pExtent->enmAccess == VMDKACCESS_READONLY) ? VD_OPEN_FLAGS_READONLY : 0),
                                                             true /* fCreate */));
                if (RT_FAILURE(rc))
                    return vdIfError(pImage->pIfError, rc, RT_SRC_POS, N_("VMDK: could not create new partition data file '%s'"), pExtent->pszFullname);
                rc = vdIfIoIntFileWriteSync(pImage->pIfIo, pExtent->pFile->pStorage,
                                            VMDK_SECTOR2BYTE(uPartOffset),
                                            pPart->pvPartitionData,
                                            pPart->cbData);
                if (RT_FAILURE(rc))
                    return vdIfError(pImage->pIfError, rc, RT_SRC_POS, N_("VMDK: could not write partition data to '%s'"), pExtent->pszFullname);
                uPartOffset += VMDK_BYTE2SECTOR(pPart->cbData);
            }
            else
            {
                if (pPart->pszRawDevice)
                {
                    /* Set up basename for extent descr. Can't use StrDup. */
                    size_t cbBasename = strlen(pPart->pszRawDevice) + 1;
                    char *pszBasename = (char *)RTMemTmpAlloc(cbBasename);
                    if (!pszBasename)
                        return VERR_NO_MEMORY;
                    memcpy(pszBasename, pPart->pszRawDevice, cbBasename);
                    pExtent->pszBasename = pszBasename;
                    /* For raw disks full name is identical to base name. */
                    pExtent->pszFullname = RTStrDup(pszBasename);
                    if (!pExtent->pszFullname)
                        return VERR_NO_MEMORY;
                    pExtent->enmType = VMDKETYPE_FLAT;
                    pExtent->cNominalSectors = VMDK_BYTE2SECTOR(pPart->cbData);
                    pExtent->uSectorOffset = VMDK_BYTE2SECTOR(pPart->uStartOffset);
                    pExtent->enmAccess = (pPart->uFlags & VDISKRAW_READONLY) ? VMDKACCESS_READONLY : VMDKACCESS_READWRITE;
                    pExtent->fMetaDirty = false;

                    /* Open flat image, the raw partition. */
                    rc = vmdkFileOpen(pImage, &pExtent->pFile, pExtent->pszFullname,
                                      VDOpenFlagsToFileOpenFlags(pImage->uOpenFlags | ((pExtent->enmAccess == VMDKACCESS_READONLY) ? VD_OPEN_FLAGS_READONLY : 0),
                                                                 false /* fCreate */));
                    if (RT_FAILURE(rc))
                        return vdIfError(pImage->pIfError, rc, RT_SRC_POS, N_("VMDK: could not open raw partition file '%s'"), pExtent->pszFullname);
                }
                else
                {
                    pExtent->pszBasename = NULL;
                    pExtent->pszFullname = NULL;
                    pExtent->enmType = VMDKETYPE_ZERO;
                    pExtent->cNominalSectors = VMDK_BYTE2SECTOR(pPart->cbData);
                    pExtent->uSectorOffset = 0;
                    pExtent->enmAccess = VMDKACCESS_READWRITE;
                    pExtent->fMetaDirty = false;
                }
            }
        }
        /* Another extent for filling up the rest of the image. */
        if (uStart != cbSize)
        {
            pExtent = &pImage->pExtents[cExtents++];
            pExtent->pszBasename = NULL;
            pExtent->pszFullname = NULL;
            pExtent->enmType = VMDKETYPE_ZERO;
            pExtent->cNominalSectors = VMDK_BYTE2SECTOR(cbSize - uStart);
            pExtent->uSectorOffset = 0;
            pExtent->enmAccess = VMDKACCESS_READWRITE;
            pExtent->fMetaDirty = false;
        }
    }

    rc = vmdkDescBaseSetStr(pImage, &pImage->Descriptor, "createType",
                            (pRaw->uFlags & VDISKRAW_DISK) ?
                            "fullDevice" : "partitionedDevice");
    if (RT_FAILURE(rc))
        return vdIfError(pImage->pIfError, rc, RT_SRC_POS, N_("VMDK: could not set the image type in '%s'"), pImage->pszFilename);
    return rc;
}

/**
 * Internal: create a regular (i.e. file-backed) VMDK image.
 */
static int vmdkCreateRegularImage(PVMDKIMAGE pImage, uint64_t cbSize,
                                  unsigned uImageFlags, PVDINTERFACEPROGRESS pIfProgress,
                                  unsigned uPercentStart, unsigned uPercentSpan)
{
    int rc = VINF_SUCCESS;
    unsigned cExtents = 1;
    uint64_t cbOffset = 0;
    uint64_t cbRemaining = cbSize;

    if (uImageFlags & VD_VMDK_IMAGE_FLAGS_SPLIT_2G)
    {
        cExtents = cbSize / VMDK_2G_SPLIT_SIZE;
        /* Do proper extent computation: need one smaller extent if the total
         * size isn't evenly divisible by the split size. */
        if (cbSize % VMDK_2G_SPLIT_SIZE)
            cExtents++;
    }
    rc = vmdkCreateExtents(pImage, cExtents);
    if (RT_FAILURE(rc))
        return vdIfError(pImage->pIfError, rc, RT_SRC_POS, N_("VMDK: could not create new extent list in '%s'"), pImage->pszFilename);

    /* Basename strings needed for constructing the extent names. */
    char *pszBasenameSubstr = RTPathFilename(pImage->pszFilename);
    AssertPtr(pszBasenameSubstr);
    size_t cbBasenameSubstr = strlen(pszBasenameSubstr) + 1;

    /* Create separate descriptor file if necessary. */
    if (cExtents != 1 || (uImageFlags & VD_IMAGE_FLAGS_FIXED))
    {
        rc = vmdkFileOpen(pImage, &pImage->pFile, pImage->pszFilename,
                          VDOpenFlagsToFileOpenFlags(pImage->uOpenFlags,
                                                     true /* fCreate */));
        if (RT_FAILURE(rc))
            return vdIfError(pImage->pIfError, rc, RT_SRC_POS, N_("VMDK: could not create new sparse descriptor file '%s'"), pImage->pszFilename);
    }
    else
        pImage->pFile = NULL;

    /* Set up all extents. */
    for (unsigned i = 0; i < cExtents; i++)
    {
        PVMDKEXTENT pExtent = &pImage->pExtents[i];
        uint64_t cbExtent = cbRemaining;

        /* Set up fullname/basename for extent description. Cannot use StrDup
         * for basename, as it is not guaranteed that the memory can be freed
         * with RTMemTmpFree, which must be used as in other code paths
         * StrDup is not usable. */
        if (cExtents == 1 && !(uImageFlags & VD_IMAGE_FLAGS_FIXED))
        {
            char *pszBasename = (char *)RTMemTmpAlloc(cbBasenameSubstr);
            if (!pszBasename)
                return VERR_NO_MEMORY;
            memcpy(pszBasename, pszBasenameSubstr, cbBasenameSubstr);
            pExtent->pszBasename = pszBasename;
        }
        else
        {
            char *pszBasenameSuff = RTPathSuffix(pszBasenameSubstr);
            char *pszBasenameBase = RTStrDup(pszBasenameSubstr);
            RTPathStripSuffix(pszBasenameBase);
            char *pszTmp;
            size_t cbTmp;
            if (uImageFlags & VD_IMAGE_FLAGS_FIXED)
            {
                if (cExtents == 1)
                    RTStrAPrintf(&pszTmp, "%s-flat%s", pszBasenameBase,
                                 pszBasenameSuff);
                else
                    RTStrAPrintf(&pszTmp, "%s-f%03d%s", pszBasenameBase,
                                 i+1, pszBasenameSuff);
            }
            else
                RTStrAPrintf(&pszTmp, "%s-s%03d%s", pszBasenameBase, i+1,
                             pszBasenameSuff);
            RTStrFree(pszBasenameBase);
            if (!pszTmp)
                return VERR_NO_STR_MEMORY;
            cbTmp = strlen(pszTmp) + 1;
            char *pszBasename = (char *)RTMemTmpAlloc(cbTmp);
            if (!pszBasename)
            {
                RTStrFree(pszTmp);
                return VERR_NO_MEMORY;
            }
            memcpy(pszBasename, pszTmp, cbTmp);
            RTStrFree(pszTmp);
            pExtent->pszBasename = pszBasename;
            if (uImageFlags & VD_VMDK_IMAGE_FLAGS_SPLIT_2G)
                cbExtent = RT_MIN(cbRemaining, VMDK_2G_SPLIT_SIZE);
        }
        char *pszBasedirectory = RTStrDup(pImage->pszFilename);
        if (!pszBasedirectory)
            return VERR_NO_STR_MEMORY;
        RTPathStripFilename(pszBasedirectory);
        char *pszFullname = RTPathJoinA(pszBasedirectory, pExtent->pszBasename);
        RTStrFree(pszBasedirectory);
        if (!pszFullname)
            return VERR_NO_STR_MEMORY;
        pExtent->pszFullname = pszFullname;

        /* Create file for extent. */
        rc = vmdkFileOpen(pImage, &pExtent->pFile, pExtent->pszFullname,
                          VDOpenFlagsToFileOpenFlags(pImage->uOpenFlags,
                                                     true /* fCreate */));
        if (RT_FAILURE(rc))
            return vdIfError(pImage->pIfError, rc, RT_SRC_POS, N_("VMDK: could not create new file '%s'"), pExtent->pszFullname);
        if (uImageFlags & VD_IMAGE_FLAGS_FIXED)
        {
            rc = vdIfIoIntFileSetAllocationSize(pImage->pIfIo, pExtent->pFile->pStorage, cbExtent,
                                                0 /* fFlags */, pIfProgress,
                                                uPercentStart + cbOffset * uPercentSpan / cbSize,
                                                cbExtent * uPercentSpan / cbSize);
            if (RT_FAILURE(rc))
                return vdIfError(pImage->pIfError, rc, RT_SRC_POS, N_("VMDK: could not set size of new file '%s'"), pExtent->pszFullname);
        }

        /* Place descriptor file information (where integrated). */
        if (cExtents == 1 && !(uImageFlags & VD_IMAGE_FLAGS_FIXED))
        {
            pExtent->uDescriptorSector = 1;
            pExtent->cDescriptorSectors = VMDK_BYTE2SECTOR(pImage->cbDescAlloc);
            /* The descriptor is part of the (only) extent. */
            pExtent->pDescData = pImage->pDescData;
            pImage->pDescData = NULL;
        }

        if (!(uImageFlags & VD_IMAGE_FLAGS_FIXED))
        {
            uint64_t cSectorsPerGDE, cSectorsPerGD;
            pExtent->enmType = VMDKETYPE_HOSTED_SPARSE;
            pExtent->cSectors = VMDK_BYTE2SECTOR(RT_ALIGN_64(cbExtent, _64K));
            pExtent->cSectorsPerGrain = VMDK_BYTE2SECTOR(_64K);
            pExtent->cGTEntries = 512;
            cSectorsPerGDE = pExtent->cGTEntries * pExtent->cSectorsPerGrain;
            pExtent->cSectorsPerGDE = cSectorsPerGDE;
            pExtent->cGDEntries = (pExtent->cSectors + cSectorsPerGDE - 1) / cSectorsPerGDE;
            cSectorsPerGD = (pExtent->cGDEntries + (512 / sizeof(uint32_t) - 1)) / (512 / sizeof(uint32_t));
            if (pImage->uImageFlags & VD_VMDK_IMAGE_FLAGS_STREAM_OPTIMIZED)
            {
                /* The spec says version is 1 for all VMDKs, but the vast
                 * majority of streamOptimized VMDKs actually contain
                 * version 3 - so go with the majority. Both are accepted. */
                pExtent->uVersion = 3;
                pExtent->uCompression = VMDK_COMPRESSION_DEFLATE;
            }
        }
        else
        {
            if (uImageFlags & VD_VMDK_IMAGE_FLAGS_ESX)
                pExtent->enmType = VMDKETYPE_VMFS;
            else
                pExtent->enmType = VMDKETYPE_FLAT;
        }

        pExtent->enmAccess = VMDKACCESS_READWRITE;
        pExtent->fUncleanShutdown = true;
        pExtent->cNominalSectors = VMDK_BYTE2SECTOR(cbExtent);
        pExtent->uSectorOffset = 0;
        pExtent->fMetaDirty = true;

        if (!(uImageFlags & VD_IMAGE_FLAGS_FIXED))
        {
            /* fPreAlloc should never be false because VMware can't use such images. */
            rc = vmdkCreateGrainDirectory(pImage, pExtent,
                                          RT_MAX(  pExtent->uDescriptorSector
                                                 + pExtent->cDescriptorSectors,
                                                 1),
                                          true /* fPreAlloc */);
            if (RT_FAILURE(rc))
                return vdIfError(pImage->pIfError, rc, RT_SRC_POS, N_("VMDK: could not create new grain directory in '%s'"), pExtent->pszFullname);
        }

        cbOffset += cbExtent;

        if (RT_SUCCESS(rc))
            vdIfProgress(pIfProgress, uPercentStart + cbOffset * uPercentSpan / cbSize);

        cbRemaining -= cbExtent;
    }

    if (pImage->uImageFlags & VD_VMDK_IMAGE_FLAGS_ESX)
    {
        /* VirtualBox doesn't care, but VMWare ESX freaks out if the wrong
         * controller type is set in an image. */
        rc = vmdkDescDDBSetStr(pImage, &pImage->Descriptor, "ddb.adapterType", "lsilogic");
        if (RT_FAILURE(rc))
            return vdIfError(pImage->pIfError, rc, RT_SRC_POS, N_("VMDK: could not set controller type to lsilogic in '%s'"), pImage->pszFilename);
    }

    const char *pszDescType = NULL;
    if (uImageFlags & VD_IMAGE_FLAGS_FIXED)
    {
        if (pImage->uImageFlags & VD_VMDK_IMAGE_FLAGS_ESX)
            pszDescType = "vmfs";
        else
            pszDescType =   (cExtents == 1)
                          ? "monolithicFlat" : "twoGbMaxExtentFlat";
    }
    else
    {
        if (pImage->uImageFlags & VD_VMDK_IMAGE_FLAGS_STREAM_OPTIMIZED)
            pszDescType = "streamOptimized";
        else
        {
            pszDescType =   (cExtents == 1)
                          ? "monolithicSparse" : "twoGbMaxExtentSparse";
        }
    }
    rc = vmdkDescBaseSetStr(pImage, &pImage->Descriptor, "createType",
                            pszDescType);
    if (RT_FAILURE(rc))
        return vdIfError(pImage->pIfError, rc, RT_SRC_POS, N_("VMDK: could not set the image type in '%s'"), pImage->pszFilename);
    return rc;
}

/**
 * Internal: Create a real stream optimized VMDK using only linear writes.
 */
static int vmdkCreateStreamImage(PVMDKIMAGE pImage, uint64_t cbSize)
{
    int rc = vmdkCreateExtents(pImage, 1);
    if (RT_FAILURE(rc))
        return vdIfError(pImage->pIfError, rc, RT_SRC_POS, N_("VMDK: could not create new extent list in '%s'"), pImage->pszFilename);

    /* Basename strings needed for constructing the extent names. */
    const char *pszBasenameSubstr = RTPathFilename(pImage->pszFilename);
    AssertPtr(pszBasenameSubstr);
    size_t cbBasenameSubstr = strlen(pszBasenameSubstr) + 1;

    /* No separate descriptor file. */
    pImage->pFile = NULL;

    /* Set up all extents. */
    PVMDKEXTENT pExtent = &pImage->pExtents[0];

    /* Set up fullname/basename for extent description. Cannot use StrDup
     * for basename, as it is not guaranteed that the memory can be freed
     * with RTMemTmpFree, which must be used as in other code paths
     * StrDup is not usable. */
    char *pszBasename = (char *)RTMemTmpAlloc(cbBasenameSubstr);
    if (!pszBasename)
        return VERR_NO_MEMORY;
    memcpy(pszBasename, pszBasenameSubstr, cbBasenameSubstr);
    pExtent->pszBasename = pszBasename;

    char *pszBasedirectory = RTStrDup(pImage->pszFilename);
    RTPathStripFilename(pszBasedirectory);
    char *pszFullname = RTPathJoinA(pszBasedirectory, pExtent->pszBasename);
    RTStrFree(pszBasedirectory);
    if (!pszFullname)
        return VERR_NO_STR_MEMORY;
    pExtent->pszFullname = pszFullname;

    /* Create file for extent. Make it write only, no reading allowed. */
    rc = vmdkFileOpen(pImage, &pExtent->pFile, pExtent->pszFullname,
                        VDOpenFlagsToFileOpenFlags(pImage->uOpenFlags,
                                                   true /* fCreate */)
                      & ~RTFILE_O_READ);
    if (RT_FAILURE(rc))
        return vdIfError(pImage->pIfError, rc, RT_SRC_POS, N_("VMDK: could not create new file '%s'"), pExtent->pszFullname);

    /* Place descriptor file information. */
    pExtent->uDescriptorSector = 1;
    pExtent->cDescriptorSectors = VMDK_BYTE2SECTOR(pImage->cbDescAlloc);
    /* The descriptor is part of the (only) extent. */
    pExtent->pDescData = pImage->pDescData;
    pImage->pDescData = NULL;

    uint64_t cSectorsPerGDE, cSectorsPerGD;
    pExtent->enmType = VMDKETYPE_HOSTED_SPARSE;
    pExtent->cSectors = VMDK_BYTE2SECTOR(RT_ALIGN_64(cbSize, _64K));
    pExtent->cSectorsPerGrain = VMDK_BYTE2SECTOR(_64K);
    pExtent->cGTEntries = 512;
    cSectorsPerGDE = pExtent->cGTEntries * pExtent->cSectorsPerGrain;
    pExtent->cSectorsPerGDE = cSectorsPerGDE;
    pExtent->cGDEntries = (pExtent->cSectors + cSectorsPerGDE - 1) / cSectorsPerGDE;
    cSectorsPerGD = (pExtent->cGDEntries + (512 / sizeof(uint32_t) - 1)) / (512 / sizeof(uint32_t));

    /* The spec says version is 1 for all VMDKs, but the vast
     * majority of streamOptimized VMDKs actually contain
     * version 3 - so go with the majority. Both are accepted. */
    pExtent->uVersion = 3;
    pExtent->uCompression = VMDK_COMPRESSION_DEFLATE;
    pExtent->fFooter = true;

    pExtent->enmAccess = VMDKACCESS_READONLY;
    pExtent->fUncleanShutdown = false;
    pExtent->cNominalSectors = VMDK_BYTE2SECTOR(cbSize);
    pExtent->uSectorOffset = 0;
    pExtent->fMetaDirty = true;

    /* Create grain directory, without preallocating it straight away. It will
     * be constructed on the fly when writing out the data and written when
     * closing the image. The end effect is that the full grain directory is
     * allocated, which is a requirement of the VMDK specs. */
    rc = vmdkCreateGrainDirectory(pImage, pExtent, VMDK_GD_AT_END,
                                  false /* fPreAlloc */);
    if (RT_FAILURE(rc))
        return vdIfError(pImage->pIfError, rc, RT_SRC_POS, N_("VMDK: could not create new grain directory in '%s'"), pExtent->pszFullname);

    rc = vmdkDescBaseSetStr(pImage, &pImage->Descriptor, "createType",
                            "streamOptimized");
    if (RT_FAILURE(rc))
        rc = vdIfError(pImage->pIfError, rc, RT_SRC_POS, N_("VMDK: could not set the image type in '%s'"), pImage->pszFilename);

    return rc;
}

/**
 * Initializes the UUID fields in the DDB.
 *
 * @returns VBox status code.
 * @param   pImage          The VMDK image instance.
 */
static int vmdkCreateImageDdbUuidsInit(PVMDKIMAGE pImage)
{
    int rc = vmdkDescDDBSetUuid(pImage, &pImage->Descriptor, VMDK_DDB_IMAGE_UUID, &pImage->ImageUuid);
    if (RT_SUCCESS(rc))
    {
        rc = vmdkDescDDBSetUuid(pImage, &pImage->Descriptor, VMDK_DDB_PARENT_UUID, &pImage->ParentUuid);
        if (RT_SUCCESS(rc))
        {
            rc = vmdkDescDDBSetUuid(pImage, &pImage->Descriptor, VMDK_DDB_MODIFICATION_UUID,
                                    &pImage->ModificationUuid);
            if (RT_SUCCESS(rc))
            {
                rc = vmdkDescDDBSetUuid(pImage, &pImage->Descriptor, VMDK_DDB_PARENT_MODIFICATION_UUID,
                                        &pImage->ParentModificationUuid);
                if (RT_FAILURE(rc))
                    rc = vdIfError(pImage->pIfError, rc, RT_SRC_POS,
                                   N_("VMDK: error storing parent modification UUID in new descriptor in '%s'"), pImage->pszFilename);
            }
            else
                rc = vdIfError(pImage->pIfError, rc, RT_SRC_POS,
                               N_("VMDK: error storing modification UUID in new descriptor in '%s'"), pImage->pszFilename);
        }
        else
            rc = vdIfError(pImage->pIfError, rc, RT_SRC_POS,
                           N_("VMDK: error storing parent image UUID in new descriptor in '%s'"), pImage->pszFilename);
    }
    else
        rc = vdIfError(pImage->pIfError, rc, RT_SRC_POS,
                       N_("VMDK: error storing image UUID in new descriptor in '%s'"), pImage->pszFilename);

    return rc;
}

/**
 * Internal: The actual code for creating any VMDK variant currently in
 * existence on hosted environments.
 */
static int vmdkCreateImage(PVMDKIMAGE pImage, uint64_t cbSize,
                           unsigned uImageFlags, const char *pszComment,
                           PCVDGEOMETRY pPCHSGeometry,
                           PCVDGEOMETRY pLCHSGeometry, PCRTUUID pUuid,
                           PVDINTERFACEPROGRESS pIfProgress,
                           unsigned uPercentStart, unsigned uPercentSpan)
{
    pImage->uImageFlags = uImageFlags;

    pImage->pIfError = VDIfErrorGet(pImage->pVDIfsDisk);
    pImage->pIfIo = VDIfIoIntGet(pImage->pVDIfsImage);
    AssertPtrReturn(pImage->pIfIo, VERR_INVALID_PARAMETER);

    int rc = vmdkCreateDescriptor(pImage, pImage->pDescData, pImage->cbDescAlloc,
                                  &pImage->Descriptor);
    if (RT_SUCCESS(rc))
    {
        if (    (uImageFlags & VD_IMAGE_FLAGS_FIXED)
            &&  (uImageFlags & VD_VMDK_IMAGE_FLAGS_RAWDISK))
        {
            /* Raw disk image (includes raw partition). */
            const PVDISKRAW pRaw = (const PVDISKRAW)pszComment;
            /* As the comment is misused, zap it so that no garbage comment
             * is set below. */
            pszComment = NULL;
            rc = vmdkCreateRawImage(pImage, pRaw, cbSize);
        }
        else if (uImageFlags & VD_VMDK_IMAGE_FLAGS_STREAM_OPTIMIZED)
        {
            /* Stream optimized sparse image (monolithic). */
            rc = vmdkCreateStreamImage(pImage, cbSize);
        }
        else
        {
            /* Regular fixed or sparse image (monolithic or split). */
            rc = vmdkCreateRegularImage(pImage, cbSize, uImageFlags,
                                        pIfProgress, uPercentStart,
                                        uPercentSpan * 95 / 100);
        }

        if (RT_SUCCESS(rc))
        {
            vdIfProgress(pIfProgress, uPercentStart + uPercentSpan * 98 / 100);

            pImage->cbSize = cbSize;

            for (unsigned i = 0; i < pImage->cExtents; i++)
            {
                PVMDKEXTENT pExtent = &pImage->pExtents[i];

                rc = vmdkDescExtInsert(pImage, &pImage->Descriptor, pExtent->enmAccess,
                                       pExtent->cNominalSectors, pExtent->enmType,
                                       pExtent->pszBasename, pExtent->uSectorOffset);
                if (RT_FAILURE(rc))
                {
                    rc = vdIfError(pImage->pIfError, rc, RT_SRC_POS, N_("VMDK: could not insert the extent list into descriptor in '%s'"), pImage->pszFilename);
                    break;
                }
            }

            if (RT_SUCCESS(rc))
                vmdkDescExtRemoveDummy(pImage, &pImage->Descriptor);

            if (   RT_SUCCESS(rc)
                && pPCHSGeometry->cCylinders != 0
                && pPCHSGeometry->cHeads != 0
                && pPCHSGeometry->cSectors != 0)
                rc = vmdkDescSetPCHSGeometry(pImage, pPCHSGeometry);

            if (   RT_SUCCESS(rc)
                && pLCHSGeometry->cCylinders != 0
                && pLCHSGeometry->cHeads != 0
                && pLCHSGeometry->cSectors != 0)
                rc = vmdkDescSetLCHSGeometry(pImage, pLCHSGeometry);

            pImage->LCHSGeometry = *pLCHSGeometry;
            pImage->PCHSGeometry = *pPCHSGeometry;

            pImage->ImageUuid = *pUuid;
            RTUuidClear(&pImage->ParentUuid);
            RTUuidClear(&pImage->ModificationUuid);
            RTUuidClear(&pImage->ParentModificationUuid);

            if (RT_SUCCESS(rc))
                rc = vmdkCreateImageDdbUuidsInit(pImage);

            if (RT_SUCCESS(rc))
                rc = vmdkAllocateGrainTableCache(pImage);

            if (RT_SUCCESS(rc))
            {
                rc = vmdkSetImageComment(pImage, pszComment);
                if (RT_FAILURE(rc))
                    rc = vdIfError(pImage->pIfError, rc, RT_SRC_POS, N_("VMDK: cannot set image comment in '%s'"), pImage->pszFilename);
            }

            if (RT_SUCCESS(rc))
            {
                vdIfProgress(pIfProgress, uPercentStart + uPercentSpan * 99 / 100);

                if (pImage->uImageFlags & VD_VMDK_IMAGE_FLAGS_STREAM_OPTIMIZED)
                {
                    /* streamOptimized is a bit special, we cannot trigger the flush
                     * until all data has been written. So we write the necessary
                     * information explicitly. */
                    pImage->pExtents[0].cDescriptorSectors = VMDK_BYTE2SECTOR(RT_ALIGN_64(  pImage->Descriptor.aLines[pImage->Descriptor.cLines]
                                                                                          - pImage->Descriptor.aLines[0], 512));
                    rc = vmdkWriteMetaSparseExtent(pImage, &pImage->pExtents[0], 0, NULL);
                    if (RT_SUCCESS(rc))
                    {
                        rc = vmdkWriteDescriptor(pImage, NULL);
                        if (RT_FAILURE(rc))
                            rc = vdIfError(pImage->pIfError, rc, RT_SRC_POS, N_("VMDK: cannot write VMDK descriptor in '%s'"), pImage->pszFilename);
                    }
                    else
                        rc = vdIfError(pImage->pIfError, rc, RT_SRC_POS, N_("VMDK: cannot write VMDK header in '%s'"), pImage->pszFilename);
                }
                else
                    rc = vmdkFlushImage(pImage, NULL);
            }
        }
    }
    else
        rc = vdIfError(pImage->pIfError, rc, RT_SRC_POS, N_("VMDK: could not create new descriptor in '%s'"), pImage->pszFilename);


    if (RT_SUCCESS(rc))
    {
        PVDREGIONDESC pRegion = &pImage->RegionList.aRegions[0];
        pImage->RegionList.fFlags   = 0;
        pImage->RegionList.cRegions = 1;

        pRegion->offRegion            = 0; /* Disk start. */
        pRegion->cbBlock              = 512;
        pRegion->enmDataForm          = VDREGIONDATAFORM_RAW;
        pRegion->enmMetadataForm      = VDREGIONMETADATAFORM_NONE;
        pRegion->cbData               = 512;
        pRegion->cbMetadata           = 0;
        pRegion->cRegionBlocksOrBytes = pImage->cbSize;

        vdIfProgress(pIfProgress, uPercentStart + uPercentSpan);
    }
    else
        vmdkFreeImage(pImage, rc != VERR_ALREADY_EXISTS, false /*fFlush*/);
    return rc;
}

/**
 * Internal: Update image comment.
 */
static int vmdkSetImageComment(PVMDKIMAGE pImage, const char *pszComment)
{
    char *pszCommentEncoded = NULL;
    if (pszComment)
    {
        pszCommentEncoded = vmdkEncodeString(pszComment);
        if (!pszCommentEncoded)
            return VERR_NO_MEMORY;
    }

    int rc = vmdkDescDDBSetStr(pImage, &pImage->Descriptor,
                               "ddb.comment", pszCommentEncoded);
    if (pszCommentEncoded)
        RTStrFree(pszCommentEncoded);
    if (RT_FAILURE(rc))
        return vdIfError(pImage->pIfError, rc, RT_SRC_POS, N_("VMDK: error storing image comment in descriptor in '%s'"), pImage->pszFilename);
    return VINF_SUCCESS;
}

/**
 * Internal. Clear the grain table buffer for real stream optimized writing.
 */
static void vmdkStreamClearGT(PVMDKIMAGE pImage, PVMDKEXTENT pExtent)
{
    uint32_t cCacheLines = RT_ALIGN(pExtent->cGTEntries, VMDK_GT_CACHELINE_SIZE) / VMDK_GT_CACHELINE_SIZE;
    for (uint32_t i = 0; i < cCacheLines; i++)
        memset(&pImage->pGTCache->aGTCache[i].aGTData[0], '\0',
               VMDK_GT_CACHELINE_SIZE * sizeof(uint32_t));
}

/**
 * Internal. Flush the grain table buffer for real stream optimized writing.
 */
static int vmdkStreamFlushGT(PVMDKIMAGE pImage, PVMDKEXTENT pExtent,
                             uint32_t uGDEntry)
{
    int rc = VINF_SUCCESS;
    uint32_t cCacheLines = RT_ALIGN(pExtent->cGTEntries, VMDK_GT_CACHELINE_SIZE) / VMDK_GT_CACHELINE_SIZE;

    /* VMware does not write out completely empty grain tables in the case
     * of streamOptimized images, which according to my interpretation of
     * the VMDK 1.1 spec is bending the rules. Since they do it and we can
     * handle it without problems do it the same way and save some bytes. */
    bool fAllZero = true;
    for (uint32_t i = 0; i < cCacheLines; i++)
    {
        /* Convert the grain table to little endian in place, as it will not
         * be used at all after this function has been called. */
        uint32_t *pGTTmp = &pImage->pGTCache->aGTCache[i].aGTData[0];
        for (uint32_t j = 0; j < VMDK_GT_CACHELINE_SIZE; j++, pGTTmp++)
            if (*pGTTmp)
            {
                fAllZero = false;
                break;
            }
        if (!fAllZero)
            break;
    }
    if (fAllZero)
        return VINF_SUCCESS;

    uint64_t uFileOffset = pExtent->uAppendPosition;
    if (!uFileOffset)
        return VERR_INTERNAL_ERROR;
    /* Align to sector, as the previous write could have been any size. */
    uFileOffset = RT_ALIGN_64(uFileOffset, 512);

    /* Grain table marker. */
    uint8_t aMarker[512];
    PVMDKMARKER pMarker = (PVMDKMARKER)&aMarker[0];
    memset(pMarker, '\0', sizeof(aMarker));
    pMarker->uSector = RT_H2LE_U64(VMDK_BYTE2SECTOR((uint64_t)pExtent->cGTEntries * sizeof(uint32_t)));
    pMarker->uType = RT_H2LE_U32(VMDK_MARKER_GT);
    rc = vdIfIoIntFileWriteSync(pImage->pIfIo, pExtent->pFile->pStorage, uFileOffset,
                                aMarker, sizeof(aMarker));
    AssertRC(rc);
    uFileOffset += 512;

    if (!pExtent->pGD || pExtent->pGD[uGDEntry])
        return VERR_INTERNAL_ERROR;

    pExtent->pGD[uGDEntry] = VMDK_BYTE2SECTOR(uFileOffset);

    for (uint32_t i = 0; i < cCacheLines; i++)
    {
        /* Convert the grain table to little endian in place, as it will not
         * be used at all after this function has been called. */
        uint32_t *pGTTmp = &pImage->pGTCache->aGTCache[i].aGTData[0];
        for (uint32_t j = 0; j < VMDK_GT_CACHELINE_SIZE; j++, pGTTmp++)
            *pGTTmp = RT_H2LE_U32(*pGTTmp);

        rc = vdIfIoIntFileWriteSync(pImage->pIfIo, pExtent->pFile->pStorage, uFileOffset,
                                    &pImage->pGTCache->aGTCache[i].aGTData[0],
                                    VMDK_GT_CACHELINE_SIZE * sizeof(uint32_t));
        uFileOffset += VMDK_GT_CACHELINE_SIZE * sizeof(uint32_t);
        if (RT_FAILURE(rc))
            break;
    }
    Assert(!(uFileOffset % 512));
    pExtent->uAppendPosition = RT_ALIGN_64(uFileOffset, 512);
    return rc;
}

/**
 * Internal. Free all allocated space for representing an image, and optionally
 * delete the image from disk.
 */
static int vmdkFreeImage(PVMDKIMAGE pImage, bool fDelete, bool fFlush)
{
    int rc = VINF_SUCCESS;

    /* Freeing a never allocated image (e.g. because the open failed) is
     * not signalled as an error. After all nothing bad happens. */
    if (pImage)
    {
        if (!(pImage->uOpenFlags & VD_OPEN_FLAGS_READONLY))
        {
            if (pImage->uImageFlags & VD_VMDK_IMAGE_FLAGS_STREAM_OPTIMIZED)
            {
                /* Check if all extents are clean. */
                for (unsigned i = 0; i < pImage->cExtents; i++)
                {
                    Assert(!pImage->pExtents[i].fUncleanShutdown);
                }
            }
            else
            {
                /* Mark all extents as clean. */
                for (unsigned i = 0; i < pImage->cExtents; i++)
                {
                    if (   pImage->pExtents[i].enmType == VMDKETYPE_HOSTED_SPARSE
                        && pImage->pExtents[i].fUncleanShutdown)
                    {
                        pImage->pExtents[i].fUncleanShutdown = false;
                        pImage->pExtents[i].fMetaDirty = true;
                    }

                    /* From now on it's not safe to append any more data. */
                    pImage->pExtents[i].uAppendPosition = 0;
                }
            }
        }

        if (pImage->uImageFlags & VD_VMDK_IMAGE_FLAGS_STREAM_OPTIMIZED)
        {
            /* No need to write any pending data if the file will be deleted
             * or if the new file wasn't successfully created. */
            if (   !fDelete && pImage->pExtents
                && pImage->pExtents[0].cGTEntries
                && pImage->pExtents[0].uAppendPosition)
            {
                PVMDKEXTENT pExtent = &pImage->pExtents[0];
                uint32_t uLastGDEntry = pExtent->uLastGrainAccess / pExtent->cGTEntries;
                rc = vmdkStreamFlushGT(pImage, pExtent, uLastGDEntry);
                AssertRC(rc);
                vmdkStreamClearGT(pImage, pExtent);
                for (uint32_t i = uLastGDEntry + 1; i < pExtent->cGDEntries; i++)
                {
                    rc = vmdkStreamFlushGT(pImage, pExtent, i);
                    AssertRC(rc);
                }

                uint64_t uFileOffset = pExtent->uAppendPosition;
                if (!uFileOffset)
                    return VERR_INTERNAL_ERROR;
                uFileOffset = RT_ALIGN_64(uFileOffset, 512);

                /* From now on it's not safe to append any more data. */
                pExtent->uAppendPosition = 0;

                /* Grain directory marker. */
                uint8_t aMarker[512];
                PVMDKMARKER pMarker = (PVMDKMARKER)&aMarker[0];
                memset(pMarker, '\0', sizeof(aMarker));
                pMarker->uSector = VMDK_BYTE2SECTOR(RT_ALIGN_64(RT_H2LE_U64((uint64_t)pExtent->cGDEntries * sizeof(uint32_t)), 512));
                pMarker->uType = RT_H2LE_U32(VMDK_MARKER_GD);
                rc = vdIfIoIntFileWriteSync(pImage->pIfIo, pExtent->pFile->pStorage, uFileOffset,
                                            aMarker, sizeof(aMarker));
                AssertRC(rc);
                uFileOffset += 512;

                /* Write grain directory in little endian style. The array will
                 * not be used after this, so convert in place. */
                uint32_t *pGDTmp = pExtent->pGD;
                for (uint32_t i = 0; i < pExtent->cGDEntries; i++, pGDTmp++)
                    *pGDTmp = RT_H2LE_U32(*pGDTmp);
                rc = vdIfIoIntFileWriteSync(pImage->pIfIo, pExtent->pFile->pStorage,
                                            uFileOffset, pExtent->pGD,
                                            pExtent->cGDEntries * sizeof(uint32_t));
                AssertRC(rc);

                pExtent->uSectorGD = VMDK_BYTE2SECTOR(uFileOffset);
                pExtent->uSectorRGD = VMDK_BYTE2SECTOR(uFileOffset);
                uFileOffset = RT_ALIGN_64(  uFileOffset
                                          + pExtent->cGDEntries * sizeof(uint32_t),
                                          512);

                /* Footer marker. */
                memset(pMarker, '\0', sizeof(aMarker));
                pMarker->uSector = VMDK_BYTE2SECTOR(512);
                pMarker->uType = RT_H2LE_U32(VMDK_MARKER_FOOTER);
                rc = vdIfIoIntFileWriteSync(pImage->pIfIo, pExtent->pFile->pStorage,
                                            uFileOffset, aMarker, sizeof(aMarker));
                AssertRC(rc);

                uFileOffset += 512;
                rc = vmdkWriteMetaSparseExtent(pImage, pExtent, uFileOffset, NULL);
                AssertRC(rc);

                uFileOffset += 512;
                /* End-of-stream marker. */
                memset(pMarker, '\0', sizeof(aMarker));
                rc = vdIfIoIntFileWriteSync(pImage->pIfIo, pExtent->pFile->pStorage,
                                            uFileOffset, aMarker, sizeof(aMarker));
                AssertRC(rc);
            }
        }
        else if (!fDelete && fFlush)
            vmdkFlushImage(pImage, NULL);

        if (pImage->pExtents != NULL)
        {
            for (unsigned i = 0 ; i < pImage->cExtents; i++)
            {
                int rc2 = vmdkFreeExtentData(pImage, &pImage->pExtents[i], fDelete);
                if (RT_SUCCESS(rc))
                    rc = rc2; /* Propogate any error when closing the file. */
            }
            RTMemFree(pImage->pExtents);
            pImage->pExtents = NULL;
        }
        pImage->cExtents = 0;
        if (pImage->pFile != NULL)
        {
            int rc2 = vmdkFileClose(pImage, &pImage->pFile, fDelete);
            if (RT_SUCCESS(rc))
                rc = rc2; /* Propogate any error when closing the file. */
        }
        int rc2 = vmdkFileCheckAllClose(pImage);
        if (RT_SUCCESS(rc))
            rc = rc2; /* Propogate any error when closing the file. */

        if (pImage->pGTCache)
        {
            RTMemFree(pImage->pGTCache);
            pImage->pGTCache = NULL;
        }
        if (pImage->pDescData)
        {
            RTMemFree(pImage->pDescData);
            pImage->pDescData = NULL;
        }
    }

    LogFlowFunc(("returns %Rrc\n", rc));
    return rc;
}

/**
 * Internal. Flush image data (and metadata) to disk.
 */
static int vmdkFlushImage(PVMDKIMAGE pImage, PVDIOCTX pIoCtx)
{
    PVMDKEXTENT pExtent;
    int rc = VINF_SUCCESS;

    /* Update descriptor if changed. */
    if (pImage->Descriptor.fDirty)
        rc = vmdkWriteDescriptor(pImage, pIoCtx);

    if (RT_SUCCESS(rc))
    {
        for (unsigned i = 0; i < pImage->cExtents; i++)
        {
            pExtent = &pImage->pExtents[i];
            if (pExtent->pFile != NULL && pExtent->fMetaDirty)
            {
                switch (pExtent->enmType)
                {
                    case VMDKETYPE_HOSTED_SPARSE:
                        if (!pExtent->fFooter)
                            rc = vmdkWriteMetaSparseExtent(pImage, pExtent, 0, pIoCtx);
                        else
                        {
                            uint64_t uFileOffset = pExtent->uAppendPosition;
                            /* Simply skip writing anything if the streamOptimized
                             * image hasn't been just created. */
                            if (!uFileOffset)
                                break;
                            uFileOffset = RT_ALIGN_64(uFileOffset, 512);
                            rc = vmdkWriteMetaSparseExtent(pImage, pExtent,
                                                           uFileOffset, pIoCtx);
                        }
                        break;
                    case VMDKETYPE_VMFS:
                    case VMDKETYPE_FLAT:
                        /* Nothing to do. */
                        break;
                    case VMDKETYPE_ZERO:
                    default:
                        AssertMsgFailed(("extent with type %d marked as dirty\n",
                                         pExtent->enmType));
                        break;
                }
            }

            if (RT_FAILURE(rc))
                break;

            switch (pExtent->enmType)
            {
                case VMDKETYPE_HOSTED_SPARSE:
                case VMDKETYPE_VMFS:
                case VMDKETYPE_FLAT:
                    /** @todo implement proper path absolute check. */
                    if (   pExtent->pFile != NULL
                        && !(pImage->uOpenFlags & VD_OPEN_FLAGS_READONLY)
                        && !(pExtent->pszBasename[0] == RTPATH_SLASH))
                        rc = vdIfIoIntFileFlush(pImage->pIfIo, pExtent->pFile->pStorage, pIoCtx,
                                                NULL, NULL);
                    break;
                case VMDKETYPE_ZERO:
                    /* No need to do anything for this extent. */
                    break;
                default:
                    AssertMsgFailed(("unknown extent type %d\n", pExtent->enmType));
                    break;
            }
        }
    }

    return rc;
}

/**
 * Internal. Find extent corresponding to the sector number in the disk.
 */
static int vmdkFindExtent(PVMDKIMAGE pImage, uint64_t offSector,
                          PVMDKEXTENT *ppExtent, uint64_t *puSectorInExtent)
{
    PVMDKEXTENT pExtent = NULL;
    int rc = VINF_SUCCESS;

    for (unsigned i = 0; i < pImage->cExtents; i++)
    {
        if (offSector < pImage->pExtents[i].cNominalSectors)
        {
            pExtent = &pImage->pExtents[i];
            *puSectorInExtent = offSector + pImage->pExtents[i].uSectorOffset;
            break;
        }
        offSector -= pImage->pExtents[i].cNominalSectors;
    }

    if (pExtent)
        *ppExtent = pExtent;
    else
        rc = VERR_IO_SECTOR_NOT_FOUND;

    return rc;
}

/**
 * Internal. Hash function for placing the grain table hash entries.
 */
static uint32_t vmdkGTCacheHash(PVMDKGTCACHE pCache, uint64_t uSector,
                                unsigned uExtent)
{
    /** @todo this hash function is quite simple, maybe use a better one which
     * scrambles the bits better. */
    return (uSector + uExtent) % pCache->cEntries;
}

/**
 * Internal. Get sector number in the extent file from the relative sector
 * number in the extent.
 */
static int vmdkGetSector(PVMDKIMAGE pImage, PVDIOCTX pIoCtx,
                         PVMDKEXTENT pExtent, uint64_t uSector,
                         uint64_t *puExtentSector)
{
    PVMDKGTCACHE pCache = pImage->pGTCache;
    uint64_t uGDIndex, uGTSector, uGTBlock;
    uint32_t uGTHash, uGTBlockIndex;
    PVMDKGTCACHEENTRY pGTCacheEntry;
    uint32_t aGTDataTmp[VMDK_GT_CACHELINE_SIZE];
    int rc;

    /* For newly created and readonly/sequentially opened streamOptimized
     * images this must be a no-op, as the grain directory is not there. */
    if (   (   pImage->uImageFlags & VD_VMDK_IMAGE_FLAGS_STREAM_OPTIMIZED
            && pExtent->uAppendPosition)
        || (   pImage->uImageFlags & VD_VMDK_IMAGE_FLAGS_STREAM_OPTIMIZED
            && pImage->uOpenFlags & VD_OPEN_FLAGS_READONLY
            && pImage->uOpenFlags & VD_OPEN_FLAGS_SEQUENTIAL))
    {
        *puExtentSector = 0;
        return VINF_SUCCESS;
    }

    uGDIndex = uSector / pExtent->cSectorsPerGDE;
    if (uGDIndex >= pExtent->cGDEntries)
        return VERR_OUT_OF_RANGE;
    uGTSector = pExtent->pGD[uGDIndex];
    if (!uGTSector)
    {
        /* There is no grain table referenced by this grain directory
         * entry. So there is absolutely no data in this area. */
        *puExtentSector = 0;
        return VINF_SUCCESS;
    }

    uGTBlock = uSector / (pExtent->cSectorsPerGrain * VMDK_GT_CACHELINE_SIZE);
    uGTHash = vmdkGTCacheHash(pCache, uGTBlock, pExtent->uExtent);
    pGTCacheEntry = &pCache->aGTCache[uGTHash];
    if (    pGTCacheEntry->uExtent != pExtent->uExtent
        ||  pGTCacheEntry->uGTBlock != uGTBlock)
    {
        /* Cache miss, fetch data from disk. */
        PVDMETAXFER pMetaXfer;
        rc = vdIfIoIntFileReadMeta(pImage->pIfIo, pExtent->pFile->pStorage,
                                   VMDK_SECTOR2BYTE(uGTSector) + (uGTBlock % (pExtent->cGTEntries / VMDK_GT_CACHELINE_SIZE)) * sizeof(aGTDataTmp),
                                   aGTDataTmp, sizeof(aGTDataTmp), pIoCtx, &pMetaXfer, NULL, NULL);
        if (RT_FAILURE(rc))
            return rc;
        /* We can release the metadata transfer immediately. */
        vdIfIoIntMetaXferRelease(pImage->pIfIo, pMetaXfer);
        pGTCacheEntry->uExtent = pExtent->uExtent;
        pGTCacheEntry->uGTBlock = uGTBlock;
        for (unsigned i = 0; i < VMDK_GT_CACHELINE_SIZE; i++)
            pGTCacheEntry->aGTData[i] = RT_LE2H_U32(aGTDataTmp[i]);
    }
    uGTBlockIndex = (uSector / pExtent->cSectorsPerGrain) % VMDK_GT_CACHELINE_SIZE;
    uint32_t uGrainSector = pGTCacheEntry->aGTData[uGTBlockIndex];
    if (uGrainSector)
        *puExtentSector = uGrainSector + uSector % pExtent->cSectorsPerGrain;
    else
        *puExtentSector = 0;
    return VINF_SUCCESS;
}

/**
 * Internal. Writes the grain and also if necessary the grain tables.
 * Uses the grain table cache as a true grain table.
 */
static int vmdkStreamAllocGrain(PVMDKIMAGE pImage, PVMDKEXTENT pExtent,
                                uint64_t uSector, PVDIOCTX pIoCtx,
                                uint64_t cbWrite)
{
    uint32_t uGrain;
    uint32_t uGDEntry, uLastGDEntry;
    uint32_t cbGrain = 0;
    uint32_t uCacheLine, uCacheEntry;
    const void *pData;
    int rc;

    /* Very strict requirements: always write at least one full grain, with
     * proper alignment. Everything else would require reading of already
     * written data, which we don't support for obvious reasons. The only
     * exception is the last grain, and only if the image size specifies
     * that only some portion holds data. In any case the write must be
     * within the image limits, no "overshoot" allowed. */
    if (   cbWrite == 0
        || (   cbWrite < VMDK_SECTOR2BYTE(pExtent->cSectorsPerGrain)
            && pExtent->cNominalSectors - uSector >= pExtent->cSectorsPerGrain)
        || uSector % pExtent->cSectorsPerGrain
        || uSector + VMDK_BYTE2SECTOR(cbWrite) > pExtent->cNominalSectors)
        return VERR_INVALID_PARAMETER;

    /* Clip write range to at most the rest of the grain. */
    cbWrite = RT_MIN(cbWrite, VMDK_SECTOR2BYTE(pExtent->cSectorsPerGrain - uSector % pExtent->cSectorsPerGrain));

    /* Do not allow to go back. */
    uGrain = uSector / pExtent->cSectorsPerGrain;
    uCacheLine = uGrain % pExtent->cGTEntries / VMDK_GT_CACHELINE_SIZE;
    uCacheEntry = uGrain % VMDK_GT_CACHELINE_SIZE;
    uGDEntry = uGrain / pExtent->cGTEntries;
    uLastGDEntry = pExtent->uLastGrainAccess / pExtent->cGTEntries;
    if (uGrain < pExtent->uLastGrainAccess)
        return VERR_VD_VMDK_INVALID_WRITE;

    /* Zero byte write optimization. Since we don't tell VBoxHDD that we need
     * to allocate something, we also need to detect the situation ourself. */
    if (   !(pImage->uOpenFlags & VD_OPEN_FLAGS_HONOR_ZEROES)
        && vdIfIoIntIoCtxIsZero(pImage->pIfIo, pIoCtx, cbWrite, true /* fAdvance */))
        return VINF_SUCCESS;

    if (uGDEntry != uLastGDEntry)
    {
        rc = vmdkStreamFlushGT(pImage, pExtent, uLastGDEntry);
        if (RT_FAILURE(rc))
            return rc;
        vmdkStreamClearGT(pImage, pExtent);
        for (uint32_t i = uLastGDEntry + 1; i < uGDEntry; i++)
        {
            rc = vmdkStreamFlushGT(pImage, pExtent, i);
            if (RT_FAILURE(rc))
                return rc;
        }
    }

    uint64_t uFileOffset;
    uFileOffset = pExtent->uAppendPosition;
    if (!uFileOffset)
        return VERR_INTERNAL_ERROR;
    /* Align to sector, as the previous write could have been any size. */
    uFileOffset = RT_ALIGN_64(uFileOffset, 512);

    /* Paranoia check: extent type, grain table buffer presence and
     * grain table buffer space. Also grain table entry must be clear. */
    if (   pExtent->enmType != VMDKETYPE_HOSTED_SPARSE
        || !pImage->pGTCache
        || pExtent->cGTEntries > VMDK_GT_CACHE_SIZE * VMDK_GT_CACHELINE_SIZE
        || pImage->pGTCache->aGTCache[uCacheLine].aGTData[uCacheEntry])
        return VERR_INTERNAL_ERROR;

    /* Update grain table entry. */
    pImage->pGTCache->aGTCache[uCacheLine].aGTData[uCacheEntry] = VMDK_BYTE2SECTOR(uFileOffset);

    if (cbWrite != VMDK_SECTOR2BYTE(pExtent->cSectorsPerGrain))
    {
        vdIfIoIntIoCtxCopyFrom(pImage->pIfIo, pIoCtx, pExtent->pvGrain, cbWrite);
        memset((char *)pExtent->pvGrain + cbWrite, '\0',
               VMDK_SECTOR2BYTE(pExtent->cSectorsPerGrain) - cbWrite);
        pData = pExtent->pvGrain;
    }
    else
    {
        RTSGSEG Segment;
        unsigned cSegments = 1;
        size_t cbSeg = 0;

        cbSeg = vdIfIoIntIoCtxSegArrayCreate(pImage->pIfIo, pIoCtx, &Segment,
                                             &cSegments, VMDK_SECTOR2BYTE(pExtent->cSectorsPerGrain));
        Assert(cbSeg == VMDK_SECTOR2BYTE(pExtent->cSectorsPerGrain));
        pData = Segment.pvSeg;
    }
    rc = vmdkFileDeflateSync(pImage, pExtent, uFileOffset, pData,
                             VMDK_SECTOR2BYTE(pExtent->cSectorsPerGrain),
                             uSector, &cbGrain);
    if (RT_FAILURE(rc))
    {
        pExtent->uGrainSectorAbs = 0;
        AssertRC(rc);
        return vdIfError(pImage->pIfError, rc, RT_SRC_POS, N_("VMDK: cannot write compressed data block in '%s'"), pExtent->pszFullname);
    }
    pExtent->uLastGrainAccess = uGrain;
    pExtent->uAppendPosition += cbGrain;

    return rc;
}

/**
 * Internal: Updates the grain table during grain allocation.
 */
static int vmdkAllocGrainGTUpdate(PVMDKIMAGE pImage, PVMDKEXTENT pExtent, PVDIOCTX pIoCtx,
                                  PVMDKGRAINALLOCASYNC pGrainAlloc)
{
    int rc = VINF_SUCCESS;
    PVMDKGTCACHE pCache = pImage->pGTCache;
    uint32_t aGTDataTmp[VMDK_GT_CACHELINE_SIZE];
    uint32_t uGTHash, uGTBlockIndex;
    uint64_t uGTSector, uRGTSector, uGTBlock;
    uint64_t uSector = pGrainAlloc->uSector;
    PVMDKGTCACHEENTRY pGTCacheEntry;

    LogFlowFunc(("pImage=%#p pExtent=%#p pCache=%#p pIoCtx=%#p pGrainAlloc=%#p\n",
                 pImage, pExtent, pCache, pIoCtx, pGrainAlloc));

    uGTSector = pGrainAlloc->uGTSector;
    uRGTSector = pGrainAlloc->uRGTSector;
    LogFlow(("uGTSector=%llu uRGTSector=%llu\n", uGTSector, uRGTSector));

    /* Update the grain table (and the cache). */
    uGTBlock = uSector / (pExtent->cSectorsPerGrain * VMDK_GT_CACHELINE_SIZE);
    uGTHash = vmdkGTCacheHash(pCache, uGTBlock, pExtent->uExtent);
    pGTCacheEntry = &pCache->aGTCache[uGTHash];
    if (    pGTCacheEntry->uExtent != pExtent->uExtent
        ||  pGTCacheEntry->uGTBlock != uGTBlock)
    {
        /* Cache miss, fetch data from disk. */
        LogFlow(("Cache miss, fetch data from disk\n"));
        PVDMETAXFER pMetaXfer = NULL;
        rc = vdIfIoIntFileReadMeta(pImage->pIfIo, pExtent->pFile->pStorage,
                                   VMDK_SECTOR2BYTE(uGTSector) + (uGTBlock % (pExtent->cGTEntries / VMDK_GT_CACHELINE_SIZE)) * sizeof(aGTDataTmp),
                                   aGTDataTmp, sizeof(aGTDataTmp), pIoCtx,
                                   &pMetaXfer, vmdkAllocGrainComplete, pGrainAlloc);
        if (rc == VERR_VD_ASYNC_IO_IN_PROGRESS)
        {
            pGrainAlloc->cIoXfersPending++;
            pGrainAlloc->fGTUpdateNeeded = true;
            /* Leave early, we will be called  again after the read completed. */
            LogFlowFunc(("Metadata read in progress, leaving\n"));
            return rc;
        }
        else if (RT_FAILURE(rc))
            return vdIfError(pImage->pIfError, rc, RT_SRC_POS, N_("VMDK: cannot read allocated grain table entry in '%s'"), pExtent->pszFullname);
        vdIfIoIntMetaXferRelease(pImage->pIfIo, pMetaXfer);
        pGTCacheEntry->uExtent = pExtent->uExtent;
        pGTCacheEntry->uGTBlock = uGTBlock;
        for (unsigned i = 0; i < VMDK_GT_CACHELINE_SIZE; i++)
            pGTCacheEntry->aGTData[i] = RT_LE2H_U32(aGTDataTmp[i]);
    }
    else
    {
        /* Cache hit. Convert grain table block back to disk format, otherwise
         * the code below will write garbage for all but the updated entry. */
        for (unsigned i = 0; i < VMDK_GT_CACHELINE_SIZE; i++)
            aGTDataTmp[i] = RT_H2LE_U32(pGTCacheEntry->aGTData[i]);
    }
    pGrainAlloc->fGTUpdateNeeded = false;
    uGTBlockIndex = (uSector / pExtent->cSectorsPerGrain) % VMDK_GT_CACHELINE_SIZE;
    aGTDataTmp[uGTBlockIndex] = RT_H2LE_U32(VMDK_BYTE2SECTOR(pGrainAlloc->uGrainOffset));
    pGTCacheEntry->aGTData[uGTBlockIndex] = VMDK_BYTE2SECTOR(pGrainAlloc->uGrainOffset);
    /* Update grain table on disk. */
    rc = vdIfIoIntFileWriteMeta(pImage->pIfIo, pExtent->pFile->pStorage,
                                VMDK_SECTOR2BYTE(uGTSector) + (uGTBlock % (pExtent->cGTEntries / VMDK_GT_CACHELINE_SIZE)) * sizeof(aGTDataTmp),
                                aGTDataTmp, sizeof(aGTDataTmp), pIoCtx,
                                vmdkAllocGrainComplete, pGrainAlloc);
    if (rc == VERR_VD_ASYNC_IO_IN_PROGRESS)
        pGrainAlloc->cIoXfersPending++;
    else if (RT_FAILURE(rc))
        return vdIfError(pImage->pIfError, rc, RT_SRC_POS, N_("VMDK: cannot write updated grain table in '%s'"), pExtent->pszFullname);
    if (pExtent->pRGD)
    {
        /* Update backup grain table on disk. */
        rc = vdIfIoIntFileWriteMeta(pImage->pIfIo, pExtent->pFile->pStorage,
                                    VMDK_SECTOR2BYTE(uRGTSector) + (uGTBlock % (pExtent->cGTEntries / VMDK_GT_CACHELINE_SIZE)) * sizeof(aGTDataTmp),
                                    aGTDataTmp, sizeof(aGTDataTmp), pIoCtx,
                                    vmdkAllocGrainComplete, pGrainAlloc);
        if (rc == VERR_VD_ASYNC_IO_IN_PROGRESS)
            pGrainAlloc->cIoXfersPending++;
        else if (RT_FAILURE(rc))
            return vdIfError(pImage->pIfError, rc, RT_SRC_POS, N_("VMDK: cannot write updated backup grain table in '%s'"), pExtent->pszFullname);
    }

    LogFlowFunc(("leaving rc=%Rrc\n", rc));
    return rc;
}

/**
 * Internal - complete the grain allocation by updating disk grain table if required.
 */
static int vmdkAllocGrainComplete(void *pBackendData, PVDIOCTX pIoCtx, void *pvUser, int rcReq)
{
    RT_NOREF1(rcReq);
    int rc = VINF_SUCCESS;
    PVMDKIMAGE pImage = (PVMDKIMAGE)pBackendData;
    PVMDKGRAINALLOCASYNC pGrainAlloc = (PVMDKGRAINALLOCASYNC)pvUser;

    LogFlowFunc(("pBackendData=%#p pIoCtx=%#p pvUser=%#p rcReq=%Rrc\n",
                 pBackendData, pIoCtx, pvUser, rcReq));

    pGrainAlloc->cIoXfersPending--;
    if (!pGrainAlloc->cIoXfersPending && pGrainAlloc->fGTUpdateNeeded)
        rc = vmdkAllocGrainGTUpdate(pImage, pGrainAlloc->pExtent, pIoCtx, pGrainAlloc);

    if (!pGrainAlloc->cIoXfersPending)
    {
        /* Grain allocation completed. */
        RTMemFree(pGrainAlloc);
    }

    LogFlowFunc(("Leaving rc=%Rrc\n", rc));
    return rc;
}

/**
 * Internal. Allocates a new grain table (if necessary).
 */
static int vmdkAllocGrain(PVMDKIMAGE pImage, PVMDKEXTENT pExtent, PVDIOCTX pIoCtx,
                          uint64_t uSector, uint64_t cbWrite)
{
    PVMDKGTCACHE pCache = pImage->pGTCache; NOREF(pCache);
    uint64_t uGDIndex, uGTSector, uRGTSector;
    uint64_t uFileOffset;
    PVMDKGRAINALLOCASYNC pGrainAlloc = NULL;
    int rc;

    LogFlowFunc(("pCache=%#p pExtent=%#p pIoCtx=%#p uSector=%llu cbWrite=%llu\n",
                 pCache, pExtent, pIoCtx, uSector, cbWrite));

    pGrainAlloc = (PVMDKGRAINALLOCASYNC)RTMemAllocZ(sizeof(VMDKGRAINALLOCASYNC));
    if (!pGrainAlloc)
        return VERR_NO_MEMORY;

    pGrainAlloc->pExtent = pExtent;
    pGrainAlloc->uSector = uSector;

    uGDIndex = uSector / pExtent->cSectorsPerGDE;
    if (uGDIndex >= pExtent->cGDEntries)
    {
        RTMemFree(pGrainAlloc);
        return VERR_OUT_OF_RANGE;
    }
    uGTSector = pExtent->pGD[uGDIndex];
    if (pExtent->pRGD)
        uRGTSector = pExtent->pRGD[uGDIndex];
    else
        uRGTSector = 0; /**< avoid compiler warning */
    if (!uGTSector)
    {
        LogFlow(("Allocating new grain table\n"));

        /* There is no grain table referenced by this grain directory
         * entry. So there is absolutely no data in this area. Allocate
         * a new grain table and put the reference to it in the GDs. */
        uFileOffset = pExtent->uAppendPosition;
        if (!uFileOffset)
        {
            RTMemFree(pGrainAlloc);
            return VERR_INTERNAL_ERROR;
        }
        Assert(!(uFileOffset % 512));

        uFileOffset = RT_ALIGN_64(uFileOffset, 512);
        uGTSector = VMDK_BYTE2SECTOR(uFileOffset);

        /* Normally the grain table is preallocated for hosted sparse extents
         * that support more than 32 bit sector numbers. So this shouldn't
         * ever happen on a valid extent. */
        if (uGTSector > UINT32_MAX)
        {
            RTMemFree(pGrainAlloc);
            return VERR_VD_VMDK_INVALID_HEADER;
        }

        /* Write grain table by writing the required number of grain table
         * cache chunks. Allocate memory dynamically here or we flood the
         * metadata cache with very small entries. */
        size_t cbGTDataTmp = pExtent->cGTEntries * sizeof(uint32_t);
        uint32_t *paGTDataTmp = (uint32_t *)RTMemTmpAllocZ(cbGTDataTmp);

        if (!paGTDataTmp)
        {
            RTMemFree(pGrainAlloc);
            return VERR_NO_MEMORY;
        }

        memset(paGTDataTmp, '\0', cbGTDataTmp);
        rc = vdIfIoIntFileWriteMeta(pImage->pIfIo, pExtent->pFile->pStorage,
                                    VMDK_SECTOR2BYTE(uGTSector),
                                    paGTDataTmp, cbGTDataTmp, pIoCtx,
                                    vmdkAllocGrainComplete, pGrainAlloc);
        if (rc == VERR_VD_ASYNC_IO_IN_PROGRESS)
            pGrainAlloc->cIoXfersPending++;
        else if (RT_FAILURE(rc))
        {
            RTMemTmpFree(paGTDataTmp);
            RTMemFree(pGrainAlloc);
            return vdIfError(pImage->pIfError, rc, RT_SRC_POS, N_("VMDK: cannot write grain table allocation in '%s'"), pExtent->pszFullname);
        }
        pExtent->uAppendPosition = RT_ALIGN_64(  pExtent->uAppendPosition
                                               + cbGTDataTmp, 512);

        if (pExtent->pRGD)
        {
            AssertReturn(!uRGTSector, VERR_VD_VMDK_INVALID_HEADER);
            uFileOffset = pExtent->uAppendPosition;
            if (!uFileOffset)
                return VERR_INTERNAL_ERROR;
            Assert(!(uFileOffset % 512));
            uRGTSector = VMDK_BYTE2SECTOR(uFileOffset);

            /* Normally the redundant grain table is preallocated for hosted
             * sparse extents that support more than 32 bit sector numbers. So
             * this shouldn't ever happen on a valid extent. */
            if (uRGTSector > UINT32_MAX)
            {
                RTMemTmpFree(paGTDataTmp);
                return VERR_VD_VMDK_INVALID_HEADER;
            }

            /* Write grain table by writing the required number of grain table
             * cache chunks. Allocate memory dynamically here or we flood the
             * metadata cache with very small entries. */
            rc = vdIfIoIntFileWriteMeta(pImage->pIfIo, pExtent->pFile->pStorage,
                                        VMDK_SECTOR2BYTE(uRGTSector),
                                        paGTDataTmp, cbGTDataTmp, pIoCtx,
                                        vmdkAllocGrainComplete, pGrainAlloc);
            if (rc == VERR_VD_ASYNC_IO_IN_PROGRESS)
                pGrainAlloc->cIoXfersPending++;
            else if (RT_FAILURE(rc))
            {
                RTMemTmpFree(paGTDataTmp);
                return vdIfError(pImage->pIfError, rc, RT_SRC_POS, N_("VMDK: cannot write backup grain table allocation in '%s'"), pExtent->pszFullname);
            }

            pExtent->uAppendPosition = pExtent->uAppendPosition + cbGTDataTmp;
        }

        RTMemTmpFree(paGTDataTmp);

        /* Update the grain directory on disk (doing it before writing the
         * grain table will result in a garbled extent if the operation is
         * aborted for some reason. Otherwise the worst that can happen is
         * some unused sectors in the extent. */
        uint32_t uGTSectorLE = RT_H2LE_U64(uGTSector);
        rc = vdIfIoIntFileWriteMeta(pImage->pIfIo, pExtent->pFile->pStorage,
                                    VMDK_SECTOR2BYTE(pExtent->uSectorGD) + uGDIndex * sizeof(uGTSectorLE),
                                    &uGTSectorLE, sizeof(uGTSectorLE), pIoCtx,
                                    vmdkAllocGrainComplete, pGrainAlloc);
        if (rc == VERR_VD_ASYNC_IO_IN_PROGRESS)
            pGrainAlloc->cIoXfersPending++;
        else if (RT_FAILURE(rc))
            return vdIfError(pImage->pIfError, rc, RT_SRC_POS, N_("VMDK: cannot write grain directory entry in '%s'"), pExtent->pszFullname);
        if (pExtent->pRGD)
        {
            uint32_t uRGTSectorLE = RT_H2LE_U64(uRGTSector);
            rc = vdIfIoIntFileWriteMeta(pImage->pIfIo, pExtent->pFile->pStorage,
                                        VMDK_SECTOR2BYTE(pExtent->uSectorRGD) + uGDIndex * sizeof(uGTSectorLE),
                                        &uRGTSectorLE, sizeof(uRGTSectorLE), pIoCtx,
                                        vmdkAllocGrainComplete, pGrainAlloc);
            if (rc == VERR_VD_ASYNC_IO_IN_PROGRESS)
                pGrainAlloc->cIoXfersPending++;
            else if (RT_FAILURE(rc))
                return vdIfError(pImage->pIfError, rc, RT_SRC_POS, N_("VMDK: cannot write backup grain directory entry in '%s'"), pExtent->pszFullname);
        }

        /* As the final step update the in-memory copy of the GDs. */
        pExtent->pGD[uGDIndex] = uGTSector;
        if (pExtent->pRGD)
            pExtent->pRGD[uGDIndex] = uRGTSector;
    }

    LogFlow(("uGTSector=%llu uRGTSector=%llu\n", uGTSector, uRGTSector));
    pGrainAlloc->uGTSector = uGTSector;
    pGrainAlloc->uRGTSector = uRGTSector;

    uFileOffset = pExtent->uAppendPosition;
    if (!uFileOffset)
        return VERR_INTERNAL_ERROR;
    Assert(!(uFileOffset % 512));

    pGrainAlloc->uGrainOffset = uFileOffset;

    if (pImage->uImageFlags & VD_VMDK_IMAGE_FLAGS_STREAM_OPTIMIZED)
    {
        AssertMsgReturn(vdIfIoIntIoCtxIsSynchronous(pImage->pIfIo, pIoCtx),
                        ("Accesses to stream optimized images must be synchronous\n"),
                        VERR_INVALID_STATE);

        if (cbWrite != VMDK_SECTOR2BYTE(pExtent->cSectorsPerGrain))
            return vdIfError(pImage->pIfError, VERR_INTERNAL_ERROR, RT_SRC_POS, N_("VMDK: not enough data for a compressed data block in '%s'"), pExtent->pszFullname);

        /* Invalidate cache, just in case some code incorrectly allows mixing
         * of reads and writes. Normally shouldn't be needed. */
        pExtent->uGrainSectorAbs = 0;

        /* Write compressed data block and the markers. */
        uint32_t cbGrain = 0;
        size_t cbSeg = 0;
        RTSGSEG Segment;
        unsigned cSegments = 1;

        cbSeg = vdIfIoIntIoCtxSegArrayCreate(pImage->pIfIo, pIoCtx, &Segment,
                                             &cSegments, cbWrite);
        Assert(cbSeg == cbWrite);

        rc = vmdkFileDeflateSync(pImage, pExtent, uFileOffset,
                                 Segment.pvSeg, cbWrite, uSector, &cbGrain);
        if (RT_FAILURE(rc))
        {
            AssertRC(rc);
            return vdIfError(pImage->pIfError, rc, RT_SRC_POS, N_("VMDK: cannot write allocated compressed data block in '%s'"), pExtent->pszFullname);
        }
        pExtent->uLastGrainAccess = uSector / pExtent->cSectorsPerGrain;
        pExtent->uAppendPosition += cbGrain;
    }
    else
    {
        /* Write the data. Always a full grain, or we're in big trouble. */
        rc = vdIfIoIntFileWriteUser(pImage->pIfIo, pExtent->pFile->pStorage,
                                    uFileOffset, pIoCtx, cbWrite,
                                    vmdkAllocGrainComplete, pGrainAlloc);
        if (rc == VERR_VD_ASYNC_IO_IN_PROGRESS)
            pGrainAlloc->cIoXfersPending++;
        else if (RT_FAILURE(rc))
            return vdIfError(pImage->pIfError, rc, RT_SRC_POS, N_("VMDK: cannot write allocated data block in '%s'"), pExtent->pszFullname);

        pExtent->uAppendPosition += cbWrite;
    }

    rc = vmdkAllocGrainGTUpdate(pImage, pExtent, pIoCtx, pGrainAlloc);

    if (!pGrainAlloc->cIoXfersPending)
    {
        /* Grain allocation completed. */
        RTMemFree(pGrainAlloc);
    }

    LogFlowFunc(("leaving rc=%Rrc\n", rc));

    return rc;
}

/**
 * Internal. Reads the contents by sequentially going over the compressed
 * grains (hoping that they are in sequence).
 */
static int vmdkStreamReadSequential(PVMDKIMAGE pImage, PVMDKEXTENT pExtent,
                                    uint64_t uSector, PVDIOCTX pIoCtx,
                                    uint64_t cbRead)
{
    int rc;

    LogFlowFunc(("pImage=%#p pExtent=%#p uSector=%llu pIoCtx=%#p cbRead=%llu\n",
                 pImage, pExtent, uSector, pIoCtx, cbRead));

    AssertMsgReturn(vdIfIoIntIoCtxIsSynchronous(pImage->pIfIo, pIoCtx),
                    ("Async I/O not supported for sequential stream optimized images\n"),
                    VERR_INVALID_STATE);

    /* Do not allow to go back. */
    uint32_t uGrain = uSector / pExtent->cSectorsPerGrain;
    if (uGrain < pExtent->uLastGrainAccess)
        return VERR_VD_VMDK_INVALID_STATE;
    pExtent->uLastGrainAccess = uGrain;

    /* After a previous error do not attempt to recover, as it would need
     * seeking (in the general case backwards which is forbidden). */
    if (!pExtent->uGrainSectorAbs)
        return VERR_VD_VMDK_INVALID_STATE;

    /* Check if we need to read something from the image or if what we have
     * in the buffer is good to fulfill the request. */
    if (!pExtent->cbGrainStreamRead || uGrain > pExtent->uGrain)
    {
        uint32_t uGrainSectorAbs =   pExtent->uGrainSectorAbs
                                   + VMDK_BYTE2SECTOR(pExtent->cbGrainStreamRead);

        /* Get the marker from the next data block - and skip everything which
         * is not a compressed grain. If it's a compressed grain which is for
         * the requested sector (or after), read it. */
        VMDKMARKER Marker;
        do
        {
            RT_ZERO(Marker);
            rc = vdIfIoIntFileReadSync(pImage->pIfIo, pExtent->pFile->pStorage,
                                       VMDK_SECTOR2BYTE(uGrainSectorAbs),
                                       &Marker, RT_UOFFSETOF(VMDKMARKER, uType));
            if (RT_FAILURE(rc))
                return rc;
            Marker.uSector = RT_LE2H_U64(Marker.uSector);
            Marker.cbSize = RT_LE2H_U32(Marker.cbSize);

            if (Marker.cbSize == 0)
            {
                /* A marker for something else than a compressed grain. */
                rc = vdIfIoIntFileReadSync(pImage->pIfIo, pExtent->pFile->pStorage,
                                             VMDK_SECTOR2BYTE(uGrainSectorAbs)
                                           + RT_UOFFSETOF(VMDKMARKER, uType),
                                           &Marker.uType, sizeof(Marker.uType));
                if (RT_FAILURE(rc))
                    return rc;
                Marker.uType = RT_LE2H_U32(Marker.uType);
                switch (Marker.uType)
                {
                    case VMDK_MARKER_EOS:
                        uGrainSectorAbs++;
                        /* Read (or mostly skip) to the end of file. Uses the
                         * Marker (LBA sector) as it is unused anyway. This
                         * makes sure that really everything is read in the
                         * success case. If this read fails it means the image
                         * is truncated, but this is harmless so ignore. */
                        vdIfIoIntFileReadSync(pImage->pIfIo, pExtent->pFile->pStorage,
                                                VMDK_SECTOR2BYTE(uGrainSectorAbs)
                                              + 511,
                                              &Marker.uSector, 1);
                        break;
                    case VMDK_MARKER_GT:
                        uGrainSectorAbs += 1 + VMDK_BYTE2SECTOR(pExtent->cGTEntries * sizeof(uint32_t));
                        break;
                    case VMDK_MARKER_GD:
                        uGrainSectorAbs += 1 + VMDK_BYTE2SECTOR(RT_ALIGN(pExtent->cGDEntries * sizeof(uint32_t), 512));
                        break;
                    case VMDK_MARKER_FOOTER:
                        uGrainSectorAbs += 2;
                        break;
                    case VMDK_MARKER_UNSPECIFIED:
                        /* Skip over the contents of the unspecified marker
                         * type 4 which exists in some vSphere created files. */
                        /** @todo figure out what the payload means. */
                        uGrainSectorAbs += 1;
                        break;
                    default:
                        AssertMsgFailed(("VMDK: corrupted marker, type=%#x\n", Marker.uType));
                        pExtent->uGrainSectorAbs = 0;
                        return VERR_VD_VMDK_INVALID_STATE;
                }
                pExtent->cbGrainStreamRead = 0;
            }
            else
            {
                /* A compressed grain marker. If it is at/after what we're
                 * interested in read and decompress data. */
                if (uSector > Marker.uSector + pExtent->cSectorsPerGrain)
                {
                    uGrainSectorAbs += VMDK_BYTE2SECTOR(RT_ALIGN(Marker.cbSize + RT_UOFFSETOF(VMDKMARKER, uType), 512));
                    continue;
                }
                uint64_t uLBA = 0;
                uint32_t cbGrainStreamRead = 0;
                rc = vmdkFileInflateSync(pImage, pExtent,
                                         VMDK_SECTOR2BYTE(uGrainSectorAbs),
                                         pExtent->pvGrain,
                                         VMDK_SECTOR2BYTE(pExtent->cSectorsPerGrain),
                                         &Marker, &uLBA, &cbGrainStreamRead);
                if (RT_FAILURE(rc))
                {
                    pExtent->uGrainSectorAbs = 0;
                    return rc;
                }
                if (   pExtent->uGrain
                    && uLBA / pExtent->cSectorsPerGrain <= pExtent->uGrain)
                {
                    pExtent->uGrainSectorAbs = 0;
                    return VERR_VD_VMDK_INVALID_STATE;
                }
                pExtent->uGrain = uLBA / pExtent->cSectorsPerGrain;
                pExtent->cbGrainStreamRead = cbGrainStreamRead;
                break;
            }
        } while (Marker.uType != VMDK_MARKER_EOS);

        pExtent->uGrainSectorAbs = uGrainSectorAbs;

        if (!pExtent->cbGrainStreamRead && Marker.uType == VMDK_MARKER_EOS)
        {
            pExtent->uGrain = UINT32_MAX;
            /* Must set a non-zero value for pExtent->cbGrainStreamRead or
             * the next read would try to get more data, and we're at EOF. */
            pExtent->cbGrainStreamRead = 1;
        }
    }

    if (pExtent->uGrain > uSector / pExtent->cSectorsPerGrain)
    {
        /* The next data block we have is not for this area, so just return
         * that there is no data. */
        LogFlowFunc(("returns VERR_VD_BLOCK_FREE\n"));
        return VERR_VD_BLOCK_FREE;
    }

    uint32_t uSectorInGrain = uSector % pExtent->cSectorsPerGrain;
    vdIfIoIntIoCtxCopyTo(pImage->pIfIo, pIoCtx,
                         (uint8_t *)pExtent->pvGrain + VMDK_SECTOR2BYTE(uSectorInGrain),
                         cbRead);
    LogFlowFunc(("returns VINF_SUCCESS\n"));
    return VINF_SUCCESS;
}

/**
 * Replaces a fragment of a string with the specified string.
 *
 * @returns Pointer to the allocated UTF-8 string.
 * @param   pszWhere        UTF-8 string to search in.
 * @param   pszWhat         UTF-8 string to search for.
 * @param   pszByWhat       UTF-8 string to replace the found string with.
 */
static char *vmdkStrReplace(const char *pszWhere, const char *pszWhat,
                            const char *pszByWhat)
{
    AssertPtr(pszWhere);
    AssertPtr(pszWhat);
    AssertPtr(pszByWhat);
    const char *pszFoundStr = strstr(pszWhere, pszWhat);
    if (!pszFoundStr)
        return NULL;
    size_t cFinal = strlen(pszWhere) + 1 + strlen(pszByWhat) - strlen(pszWhat);
    char *pszNewStr = (char *)RTMemAlloc(cFinal);
    if (pszNewStr)
    {
        char *pszTmp = pszNewStr;
        memcpy(pszTmp, pszWhere, pszFoundStr - pszWhere);
        pszTmp += pszFoundStr - pszWhere;
        memcpy(pszTmp, pszByWhat, strlen(pszByWhat));
        pszTmp += strlen(pszByWhat);
        strcpy(pszTmp, pszFoundStr + strlen(pszWhat));
    }
    return pszNewStr;
}


/** @copydoc VDIMAGEBACKEND::pfnProbe */
static DECLCALLBACK(int) vmdkProbe(const char *pszFilename, PVDINTERFACE pVDIfsDisk,
                                   PVDINTERFACE pVDIfsImage, VDTYPE *penmType)
{
    LogFlowFunc(("pszFilename=\"%s\" pVDIfsDisk=%#p pVDIfsImage=%#p penmType=%#p\n",
                 pszFilename, pVDIfsDisk, pVDIfsImage, penmType));

    AssertReturn((VALID_PTR(pszFilename) && *pszFilename), VERR_INVALID_PARAMETER);

    int rc = VINF_SUCCESS;
    PVMDKIMAGE pImage = (PVMDKIMAGE)RTMemAllocZ(RT_UOFFSETOF(VMDKIMAGE, RegionList.aRegions[1]));
    if (RT_LIKELY(pImage))
    {
        pImage->pszFilename = pszFilename;
        pImage->pFile = NULL;
        pImage->pExtents = NULL;
        pImage->pFiles = NULL;
        pImage->pGTCache = NULL;
        pImage->pDescData = NULL;
        pImage->pVDIfsDisk = pVDIfsDisk;
        pImage->pVDIfsImage = pVDIfsImage;
        /** @todo speed up this test open (VD_OPEN_FLAGS_INFO) by skipping as
         * much as possible in vmdkOpenImage. */
        rc = vmdkOpenImage(pImage, VD_OPEN_FLAGS_INFO | VD_OPEN_FLAGS_READONLY);
        vmdkFreeImage(pImage, false, false /*fFlush*/);
        RTMemFree(pImage);

        if (RT_SUCCESS(rc))
            *penmType = VDTYPE_HDD;
    }
    else
        rc = VERR_NO_MEMORY;

    LogFlowFunc(("returns %Rrc\n", rc));
    return rc;
}

/** @copydoc VDIMAGEBACKEND::pfnOpen */
static DECLCALLBACK(int) vmdkOpen(const char *pszFilename, unsigned uOpenFlags,
                                  PVDINTERFACE pVDIfsDisk, PVDINTERFACE pVDIfsImage,
                                  VDTYPE enmType, void **ppBackendData)
{
    RT_NOREF1(enmType); /**< @todo r=klaus make use of the type info. */

    LogFlowFunc(("pszFilename=\"%s\" uOpenFlags=%#x pVDIfsDisk=%#p pVDIfsImage=%#p enmType=%u ppBackendData=%#p\n",
                 pszFilename, uOpenFlags, pVDIfsDisk, pVDIfsImage, enmType, ppBackendData));
    int rc;

    /* Check open flags. All valid flags are supported. */
    AssertReturn(!(uOpenFlags & ~VD_OPEN_FLAGS_MASK), VERR_INVALID_PARAMETER);
    AssertReturn((VALID_PTR(pszFilename) && *pszFilename), VERR_INVALID_PARAMETER);

    PVMDKIMAGE pImage = (PVMDKIMAGE)RTMemAllocZ(RT_UOFFSETOF(VMDKIMAGE, RegionList.aRegions[1]));
    if (RT_LIKELY(pImage))
    {
        pImage->pszFilename = pszFilename;
        pImage->pFile = NULL;
        pImage->pExtents = NULL;
        pImage->pFiles = NULL;
        pImage->pGTCache = NULL;
        pImage->pDescData = NULL;
        pImage->pVDIfsDisk = pVDIfsDisk;
        pImage->pVDIfsImage = pVDIfsImage;

        rc = vmdkOpenImage(pImage, uOpenFlags);
        if (RT_SUCCESS(rc))
            *ppBackendData = pImage;
        else
            RTMemFree(pImage);
    }
    else
        rc = VERR_NO_MEMORY;

    LogFlowFunc(("returns %Rrc (pBackendData=%#p)\n", rc, *ppBackendData));
    return rc;
}

/** @copydoc VDIMAGEBACKEND::pfnCreate */
static DECLCALLBACK(int) vmdkCreate(const char *pszFilename, uint64_t cbSize,
                                    unsigned uImageFlags, const char *pszComment,
                                    PCVDGEOMETRY pPCHSGeometry, PCVDGEOMETRY pLCHSGeometry,
                                    PCRTUUID pUuid, unsigned uOpenFlags,
                                    unsigned uPercentStart, unsigned uPercentSpan,
                                    PVDINTERFACE pVDIfsDisk, PVDINTERFACE pVDIfsImage,
                                    PVDINTERFACE pVDIfsOperation, VDTYPE enmType,
                                    void **ppBackendData)
{
    LogFlowFunc(("pszFilename=\"%s\" cbSize=%llu uImageFlags=%#x pszComment=\"%s\" pPCHSGeometry=%#p pLCHSGeometry=%#p Uuid=%RTuuid uOpenFlags=%#x uPercentStart=%u uPercentSpan=%u pVDIfsDisk=%#p pVDIfsImage=%#p pVDIfsOperation=%#p enmType=%u ppBackendData=%#p\n",
                 pszFilename, cbSize, uImageFlags, pszComment, pPCHSGeometry, pLCHSGeometry, pUuid, uOpenFlags, uPercentStart, uPercentSpan, pVDIfsDisk, pVDIfsImage, pVDIfsOperation, enmType, ppBackendData));
    int rc;

    /* Check the VD container type and image flags. */
    if (   enmType != VDTYPE_HDD
        || (uImageFlags & ~VD_VMDK_IMAGE_FLAGS_MASK) != 0)
        return VERR_VD_INVALID_TYPE;

    /* Check size. Maximum 256TB-64K for sparse images, otherwise unlimited. */
    if (    !cbSize
        ||  (!(uImageFlags & VD_IMAGE_FLAGS_FIXED) && cbSize >= _1T * 256 - _64K))
        return VERR_VD_INVALID_SIZE;

    /* Check image flags for invalid combinations. */
    if (   (uImageFlags & VD_VMDK_IMAGE_FLAGS_STREAM_OPTIMIZED)
        && (uImageFlags & ~(VD_VMDK_IMAGE_FLAGS_STREAM_OPTIMIZED | VD_IMAGE_FLAGS_DIFF)))
        return VERR_INVALID_PARAMETER;

    /* Check open flags. All valid flags are supported. */
    AssertReturn(!(uOpenFlags & ~VD_OPEN_FLAGS_MASK), VERR_INVALID_PARAMETER);
    AssertReturn(   VALID_PTR(pszFilename)
                 && *pszFilename
                 && VALID_PTR(pPCHSGeometry)
                 && VALID_PTR(pLCHSGeometry)
                 && !(   uImageFlags & VD_VMDK_IMAGE_FLAGS_ESX
                      && !(uImageFlags & VD_IMAGE_FLAGS_FIXED)),
                 VERR_INVALID_PARAMETER);

    PVMDKIMAGE pImage = (PVMDKIMAGE)RTMemAllocZ(RT_UOFFSETOF(VMDKIMAGE, RegionList.aRegions[1]));
    if (RT_LIKELY(pImage))
    {
        PVDINTERFACEPROGRESS pIfProgress = VDIfProgressGet(pVDIfsOperation);

        pImage->pszFilename = pszFilename;
        pImage->pFile = NULL;
        pImage->pExtents = NULL;
        pImage->pFiles = NULL;
        pImage->pGTCache = NULL;
        pImage->pDescData = NULL;
        pImage->pVDIfsDisk = pVDIfsDisk;
        pImage->pVDIfsImage = pVDIfsImage;
        /* Descriptors for split images can be pretty large, especially if the
         * filename is long. So prepare for the worst, and allocate quite some
         * memory for the descriptor in this case. */
        if (uImageFlags & VD_VMDK_IMAGE_FLAGS_SPLIT_2G)
            pImage->cbDescAlloc = VMDK_SECTOR2BYTE(200);
        else
            pImage->cbDescAlloc = VMDK_SECTOR2BYTE(20);
        pImage->pDescData = (char *)RTMemAllocZ(pImage->cbDescAlloc);
        if (RT_LIKELY(pImage->pDescData))
        {
            rc = vmdkCreateImage(pImage, cbSize, uImageFlags, pszComment,
                                 pPCHSGeometry, pLCHSGeometry, pUuid,
                                 pIfProgress, uPercentStart, uPercentSpan);
            if (RT_SUCCESS(rc))
            {
                /* So far the image is opened in read/write mode. Make sure the
                 * image is opened in read-only mode if the caller requested that. */
                if (uOpenFlags & VD_OPEN_FLAGS_READONLY)
                {
                    vmdkFreeImage(pImage, false, true /*fFlush*/);
                    rc = vmdkOpenImage(pImage, uOpenFlags);
                }

                if (RT_SUCCESS(rc))
                    *ppBackendData = pImage;
            }

            if (RT_FAILURE(rc))
                RTMemFree(pImage->pDescData);
        }
        else
            rc = VERR_NO_MEMORY;

        if (RT_FAILURE(rc))
            RTMemFree(pImage);
    }
    else
        rc = VERR_NO_MEMORY;

    LogFlowFunc(("returns %Rrc (pBackendData=%#p)\n", rc, *ppBackendData));
    return rc;
}

/**
 * Prepares the state for renaming a VMDK image, setting up the state and allocating
 * memory.
 *
 * @returns VBox status code.
 * @param   pImage          VMDK image instance.
 * @param   pRenameState    The state to initialize.
 * @param   pszFilename     The new filename.
 */
static int vmdkRenameStatePrepare(PVMDKIMAGE pImage, PVMDKRENAMESTATE pRenameState, const char *pszFilename)
{
    int rc = VINF_SUCCESS;

    memset(&pRenameState->DescriptorCopy, 0, sizeof(pRenameState->DescriptorCopy));

    /*
     * Allocate an array to store both old and new names of renamed files
     * in case we have to roll back the changes. Arrays are initialized
     * with zeros. We actually save stuff when and if we change it.
     */
    pRenameState->cExtents     = pImage->cExtents;
    pRenameState->apszOldName  = (char **)RTMemTmpAllocZ((pRenameState->cExtents + 1) * sizeof(char*));
    pRenameState->apszNewName  = (char **)RTMemTmpAllocZ((pRenameState->cExtents + 1) * sizeof(char*));
    pRenameState->apszNewLines = (char **)RTMemTmpAllocZ(pRenameState->cExtents * sizeof(char*));
    if (   pRenameState->apszOldName
        && pRenameState->apszNewName
        && pRenameState->apszNewLines)
    {
        /* Save the descriptor size and position. */
        if (pImage->pDescData)
        {
            /* Separate descriptor file. */
            pRenameState->fEmbeddedDesc = false;
        }
        else
        {
            /* Embedded descriptor file. */
            pRenameState->ExtentCopy = pImage->pExtents[0];
            pRenameState->fEmbeddedDesc = true;
        }

        /* Save the descriptor content. */
        pRenameState->DescriptorCopy.cLines = pImage->Descriptor.cLines;
        for (unsigned i = 0; i < pRenameState->DescriptorCopy.cLines; i++)
        {
            pRenameState->DescriptorCopy.aLines[i] = RTStrDup(pImage->Descriptor.aLines[i]);
            if (!pRenameState->DescriptorCopy.aLines[i])
            {
                rc = VERR_NO_MEMORY;
                break;
            }
        }

        if (RT_SUCCESS(rc))
        {
            /* Prepare both old and new base names used for string replacement. */
            pRenameState->pszNewBaseName = RTStrDup(RTPathFilename(pszFilename));
            RTPathStripSuffix(pRenameState->pszNewBaseName);
            pRenameState->pszOldBaseName = RTStrDup(RTPathFilename(pImage->pszFilename));
            RTPathStripSuffix(pRenameState->pszOldBaseName);
            /* Prepare both old and new full names used for string replacement. */
            pRenameState->pszNewFullName = RTStrDup(pszFilename);
            RTPathStripSuffix(pRenameState->pszNewFullName);
            pRenameState->pszOldFullName = RTStrDup(pImage->pszFilename);
            RTPathStripSuffix(pRenameState->pszOldFullName);

            /* Save the old name for easy access to the old descriptor file. */
            pRenameState->pszOldDescName = RTStrDup(pImage->pszFilename);
            /* Save old image name. */
            pRenameState->pszOldImageName = pImage->pszFilename;
        }
    }
    else
        rc = VERR_NO_MEMORY;

    return rc;
}

/**
 * Destroys the given rename state, freeing all allocated memory.
 *
 * @returns nothing.
 * @param   pRenameState    The rename state to destroy.
 */
static void vmdkRenameStateDestroy(PVMDKRENAMESTATE pRenameState)
{
    for (unsigned i = 0; i < pRenameState->DescriptorCopy.cLines; i++)
        if (pRenameState->DescriptorCopy.aLines[i])
            RTStrFree(pRenameState->DescriptorCopy.aLines[i]);
    if (pRenameState->apszOldName)
    {
        for (unsigned i = 0; i <= pRenameState->cExtents; i++)
            if (pRenameState->apszOldName[i])
                RTStrFree(pRenameState->apszOldName[i]);
        RTMemTmpFree(pRenameState->apszOldName);
    }
    if (pRenameState->apszNewName)
    {
        for (unsigned i = 0; i <= pRenameState->cExtents; i++)
            if (pRenameState->apszNewName[i])
                RTStrFree(pRenameState->apszNewName[i]);
        RTMemTmpFree(pRenameState->apszNewName);
    }
    if (pRenameState->apszNewLines)
    {
        for (unsigned i = 0; i < pRenameState->cExtents; i++)
            if (pRenameState->apszNewLines[i])
                RTStrFree(pRenameState->apszNewLines[i]);
        RTMemTmpFree(pRenameState->apszNewLines);
    }
    if (pRenameState->pszOldDescName)
        RTStrFree(pRenameState->pszOldDescName);
    if (pRenameState->pszOldBaseName)
        RTStrFree(pRenameState->pszOldBaseName);
    if (pRenameState->pszNewBaseName)
        RTStrFree(pRenameState->pszNewBaseName);
    if (pRenameState->pszOldFullName)
        RTStrFree(pRenameState->pszOldFullName);
    if (pRenameState->pszNewFullName)
        RTStrFree(pRenameState->pszNewFullName);
}

/**
 * Rolls back the rename operation to the original state.
 *
 * @returns VBox status code.
 * @param   pImage          VMDK image instance.
 * @param   pRenameState    The rename state.
 */
static int vmdkRenameRollback(PVMDKIMAGE pImage, PVMDKRENAMESTATE pRenameState)
{
    int rc = VINF_SUCCESS;

    if (!pRenameState->fImageFreed)
    {
        /*
         * Some extents may have been closed, close the rest. We will
         * re-open the whole thing later.
         */
        vmdkFreeImage(pImage, false, true /*fFlush*/);
    }

    /* Rename files back. */
    for (unsigned i = 0; i <= pRenameState->cExtents; i++)
    {
        if (pRenameState->apszOldName[i])
        {
            rc = vdIfIoIntFileMove(pImage->pIfIo, pRenameState->apszNewName[i], pRenameState->apszOldName[i], 0);
            AssertRC(rc);
        }
    }
    /* Restore the old descriptor. */
    PVMDKFILE pFile;
    rc = vmdkFileOpen(pImage, &pFile, pRenameState->pszOldDescName,
                      VDOpenFlagsToFileOpenFlags(VD_OPEN_FLAGS_NORMAL,
                                                 false /* fCreate */));
    AssertRC(rc);
    if (pRenameState->fEmbeddedDesc)
    {
        pRenameState->ExtentCopy.pFile = pFile;
        pImage->pExtents = &pRenameState->ExtentCopy;
    }
    else
    {
        /* Shouldn't be null for separate descriptor.
         * There will be no access to the actual content.
         */
        pImage->pDescData = pRenameState->pszOldDescName;
        pImage->pFile = pFile;
    }
    pImage->Descriptor = pRenameState->DescriptorCopy;
    vmdkWriteDescriptor(pImage, NULL);
    vmdkFileClose(pImage, &pFile, false);
    /* Get rid of the stuff we implanted. */
    pImage->pExtents = NULL;
    pImage->pFile = NULL;
    pImage->pDescData = NULL;
    /* Re-open the image back. */
    pImage->pszFilename = pRenameState->pszOldImageName;
    rc = vmdkOpenImage(pImage, pImage->uOpenFlags);

    return rc;
}

/**
 * Rename worker doing the real work.
 *
 * @returns VBox status code.
 * @param   pImage          VMDK image instance.
 * @param   pRenameState    The rename state.
 * @param   pszFilename     The new filename.
 */
static int vmdkRenameWorker(PVMDKIMAGE pImage, PVMDKRENAMESTATE pRenameState, const char *pszFilename)
{
    int rc = VINF_SUCCESS;
    unsigned i, line;

    /* Update the descriptor with modified extent names. */
    for (i = 0, line = pImage->Descriptor.uFirstExtent;
        i < pRenameState->cExtents;
        i++, line = pImage->Descriptor.aNextLines[line])
    {
        /* Update the descriptor. */
        pRenameState->apszNewLines[i] = vmdkStrReplace(pImage->Descriptor.aLines[line],
                                                       pRenameState->pszOldBaseName,
                                                       pRenameState->pszNewBaseName);
        if (!pRenameState->apszNewLines[i])
        {
            rc = VERR_NO_MEMORY;
            break;
        }
        pImage->Descriptor.aLines[line] = pRenameState->apszNewLines[i];
    }

    if (RT_SUCCESS(rc))
    {
        /* Make sure the descriptor gets written back. */
        pImage->Descriptor.fDirty = true;
        /* Flush the descriptor now, in case it is embedded. */
        vmdkFlushImage(pImage, NULL);

        /* Close and rename/move extents. */
        for (i = 0; i < pRenameState->cExtents; i++)
        {
            PVMDKEXTENT pExtent = &pImage->pExtents[i];
            /* Compose new name for the extent. */
            pRenameState->apszNewName[i] = vmdkStrReplace(pExtent->pszFullname,
                                                          pRenameState->pszOldFullName,
                                                          pRenameState->pszNewFullName);
            if (!pRenameState->apszNewName[i])
            {
                rc = VERR_NO_MEMORY;
                break;
            }
            /* Close the extent file. */
            rc = vmdkFileClose(pImage, &pExtent->pFile, false);
            if (RT_FAILURE(rc))
                break;;

            /* Rename the extent file. */
            rc = vdIfIoIntFileMove(pImage->pIfIo, pExtent->pszFullname, pRenameState->apszNewName[i], 0);
            if (RT_FAILURE(rc))
                break;
            /* Remember the old name. */
            pRenameState->apszOldName[i] = RTStrDup(pExtent->pszFullname);
        }

        if (RT_SUCCESS(rc))
        {
            /* Release all old stuff. */
            rc = vmdkFreeImage(pImage, false, true /*fFlush*/);
            if (RT_SUCCESS(rc))
            {
                pRenameState->fImageFreed = true;

                /* Last elements of new/old name arrays are intended for
                 * storing descriptor's names.
                 */
                pRenameState->apszNewName[pRenameState->cExtents] = RTStrDup(pszFilename);
                /* Rename the descriptor file if it's separate. */
                if (!pRenameState->fEmbeddedDesc)
                {
                    rc = vdIfIoIntFileMove(pImage->pIfIo, pImage->pszFilename, pRenameState->apszNewName[pRenameState->cExtents], 0);
                    if (RT_SUCCESS(rc))
                    {
                        /* Save old name only if we may need to change it back. */
                        pRenameState->apszOldName[pRenameState->cExtents] = RTStrDup(pszFilename);
                    }
                }

                /* Update pImage with the new information. */
                pImage->pszFilename = pszFilename;

                /* Open the new image. */
                rc = vmdkOpenImage(pImage, pImage->uOpenFlags);
            }
        }
    }

    return rc;
}

/** @copydoc VDIMAGEBACKEND::pfnRename */
static DECLCALLBACK(int) vmdkRename(void *pBackendData, const char *pszFilename)
{
    LogFlowFunc(("pBackendData=%#p pszFilename=%#p\n", pBackendData, pszFilename));

    PVMDKIMAGE  pImage  = (PVMDKIMAGE)pBackendData;
    VMDKRENAMESTATE RenameState;

    memset(&RenameState, 0, sizeof(RenameState));

    /* Check arguments. */
    AssertReturn((   pImage
                  && VALID_PTR(pszFilename)
                  && *pszFilename
                  && !(pImage->uImageFlags & VD_VMDK_IMAGE_FLAGS_RAWDISK)), VERR_INVALID_PARAMETER);

    int rc = vmdkRenameStatePrepare(pImage, &RenameState, pszFilename);
    if (RT_SUCCESS(rc))
    {
        /* --- Up to this point we have not done any damage yet. --- */

        rc = vmdkRenameWorker(pImage, &RenameState, pszFilename);
        /* Roll back all changes in case of failure. */
        if (RT_FAILURE(rc))
        {
            int rrc = vmdkRenameRollback(pImage, &RenameState);
            AssertRC(rrc);
        }
    }

    vmdkRenameStateDestroy(&RenameState);
    LogFlowFunc(("returns %Rrc\n", rc));
    return rc;
}

/** @copydoc VDIMAGEBACKEND::pfnClose */
static DECLCALLBACK(int) vmdkClose(void *pBackendData, bool fDelete)
{
    LogFlowFunc(("pBackendData=%#p fDelete=%d\n", pBackendData, fDelete));
    PVMDKIMAGE pImage = (PVMDKIMAGE)pBackendData;

    int rc = vmdkFreeImage(pImage, fDelete, true /*fFlush*/);
    RTMemFree(pImage);

    LogFlowFunc(("returns %Rrc\n", rc));
    return rc;
}

/** @copydoc VDIMAGEBACKEND::pfnRead */
static DECLCALLBACK(int) vmdkRead(void *pBackendData, uint64_t uOffset, size_t cbToRead,
                                  PVDIOCTX pIoCtx, size_t *pcbActuallyRead)
{
    LogFlowFunc(("pBackendData=%#p uOffset=%llu pIoCtx=%#p cbToRead=%zu pcbActuallyRead=%#p\n",
                 pBackendData, uOffset, pIoCtx, cbToRead, pcbActuallyRead));
    PVMDKIMAGE pImage = (PVMDKIMAGE)pBackendData;

    AssertPtr(pImage);
    Assert(uOffset % 512 == 0);
    Assert(cbToRead % 512 == 0);
    AssertReturn((VALID_PTR(pIoCtx) && cbToRead), VERR_INVALID_PARAMETER);
    AssertReturn(uOffset + cbToRead <= pImage->cbSize, VERR_INVALID_PARAMETER);

    /* Find the extent and check access permissions as defined in the extent descriptor. */
    PVMDKEXTENT pExtent;
    uint64_t uSectorExtentRel;
    int rc = vmdkFindExtent(pImage, VMDK_BYTE2SECTOR(uOffset),
                            &pExtent, &uSectorExtentRel);
    if (   RT_SUCCESS(rc)
        && pExtent->enmAccess != VMDKACCESS_NOACCESS)
    {
        /* Clip read range to remain in this extent. */
        cbToRead = RT_MIN(cbToRead, VMDK_SECTOR2BYTE(pExtent->uSectorOffset + pExtent->cNominalSectors - uSectorExtentRel));

        /* Handle the read according to the current extent type. */
        switch (pExtent->enmType)
        {
            case VMDKETYPE_HOSTED_SPARSE:
            {
                uint64_t uSectorExtentAbs;

                rc = vmdkGetSector(pImage, pIoCtx, pExtent, uSectorExtentRel, &uSectorExtentAbs);
                if (RT_FAILURE(rc))
                    break;
                /* Clip read range to at most the rest of the grain. */
                cbToRead = RT_MIN(cbToRead, VMDK_SECTOR2BYTE(pExtent->cSectorsPerGrain - uSectorExtentRel % pExtent->cSectorsPerGrain));
                Assert(!(cbToRead % 512));
                if (uSectorExtentAbs == 0)
                {
                    if (   !(pImage->uImageFlags & VD_VMDK_IMAGE_FLAGS_STREAM_OPTIMIZED)
                        || !(pImage->uOpenFlags & VD_OPEN_FLAGS_READONLY)
                        || !(pImage->uOpenFlags & VD_OPEN_FLAGS_SEQUENTIAL))
                        rc = VERR_VD_BLOCK_FREE;
                    else
                        rc = vmdkStreamReadSequential(pImage, pExtent,
                                                      uSectorExtentRel,
                                                      pIoCtx, cbToRead);
                }
                else
                {
                    if (pImage->uImageFlags & VD_VMDK_IMAGE_FLAGS_STREAM_OPTIMIZED)
                    {
                        AssertMsg(vdIfIoIntIoCtxIsSynchronous(pImage->pIfIo, pIoCtx),
                                  ("Async I/O is not supported for stream optimized VMDK's\n"));

                        uint32_t uSectorInGrain = uSectorExtentRel % pExtent->cSectorsPerGrain;
                        uSectorExtentAbs -= uSectorInGrain;
                        if (pExtent->uGrainSectorAbs != uSectorExtentAbs)
                        {
                            uint64_t uLBA = 0; /* gcc maybe uninitialized */
                            rc = vmdkFileInflateSync(pImage, pExtent,
                                                     VMDK_SECTOR2BYTE(uSectorExtentAbs),
                                                     pExtent->pvGrain,
                                                     VMDK_SECTOR2BYTE(pExtent->cSectorsPerGrain),
                                                     NULL, &uLBA, NULL);
                            if (RT_FAILURE(rc))
                            {
                                pExtent->uGrainSectorAbs = 0;
                                break;
                            }
                            pExtent->uGrainSectorAbs = uSectorExtentAbs;
                            pExtent->uGrain = uSectorExtentRel / pExtent->cSectorsPerGrain;
                            Assert(uLBA == uSectorExtentRel);
                        }
                        vdIfIoIntIoCtxCopyTo(pImage->pIfIo, pIoCtx,
                                               (uint8_t *)pExtent->pvGrain
                                             + VMDK_SECTOR2BYTE(uSectorInGrain),
                                             cbToRead);
                    }
                    else
                        rc = vdIfIoIntFileReadUser(pImage->pIfIo, pExtent->pFile->pStorage,
                                                   VMDK_SECTOR2BYTE(uSectorExtentAbs),
                                                   pIoCtx, cbToRead);
                }
                break;
            }
            case VMDKETYPE_VMFS:
            case VMDKETYPE_FLAT:
                rc = vdIfIoIntFileReadUser(pImage->pIfIo, pExtent->pFile->pStorage,
                                           VMDK_SECTOR2BYTE(uSectorExtentRel),
                                           pIoCtx, cbToRead);
                break;
            case VMDKETYPE_ZERO:
            {
                size_t cbSet;

                cbSet = vdIfIoIntIoCtxSet(pImage->pIfIo, pIoCtx, 0, cbToRead);
                Assert(cbSet == cbToRead);
                break;
            }
        }
        if (pcbActuallyRead)
            *pcbActuallyRead = cbToRead;
    }
    else if (RT_SUCCESS(rc))
        rc = VERR_VD_VMDK_INVALID_STATE;

    LogFlowFunc(("returns %Rrc\n", rc));
    return rc;
}

/** @copydoc VDIMAGEBACKEND::pfnWrite */
static DECLCALLBACK(int) vmdkWrite(void *pBackendData, uint64_t uOffset, size_t cbToWrite,
                                   PVDIOCTX pIoCtx, size_t *pcbWriteProcess, size_t *pcbPreRead,
                                   size_t *pcbPostRead, unsigned fWrite)
{
    LogFlowFunc(("pBackendData=%#p uOffset=%llu pIoCtx=%#p cbToWrite=%zu pcbWriteProcess=%#p pcbPreRead=%#p pcbPostRead=%#p\n",
                 pBackendData, uOffset, pIoCtx, cbToWrite, pcbWriteProcess, pcbPreRead, pcbPostRead));
    PVMDKIMAGE pImage = (PVMDKIMAGE)pBackendData;
    int rc;

    AssertPtr(pImage);
    Assert(uOffset % 512 == 0);
    Assert(cbToWrite % 512 == 0);
    AssertReturn((VALID_PTR(pIoCtx) && cbToWrite), VERR_INVALID_PARAMETER);

    if (!(pImage->uOpenFlags & VD_OPEN_FLAGS_READONLY))
    {
        PVMDKEXTENT pExtent;
        uint64_t uSectorExtentRel;
        uint64_t uSectorExtentAbs;

        /* No size check here, will do that later when the extent is located.
         * There are sparse images out there which according to the spec are
         * invalid, because the total size is not a multiple of the grain size.
         * Also for sparse images which are stitched together in odd ways (not at
         * grain boundaries, and with the nominal size not being a multiple of the
         * grain size), this would prevent writing to the last grain. */

        rc = vmdkFindExtent(pImage, VMDK_BYTE2SECTOR(uOffset),
                            &pExtent, &uSectorExtentRel);
        if (RT_SUCCESS(rc))
        {
            if (   pExtent->enmAccess != VMDKACCESS_READWRITE
                && (   !(pImage->uImageFlags & VD_VMDK_IMAGE_FLAGS_STREAM_OPTIMIZED)
                    && !pImage->pExtents[0].uAppendPosition
                    && pExtent->enmAccess != VMDKACCESS_READONLY))
                rc = VERR_VD_VMDK_INVALID_STATE;
            else
            {
                /* Handle the write according to the current extent type. */
                switch (pExtent->enmType)
                {
                    case VMDKETYPE_HOSTED_SPARSE:
                        rc = vmdkGetSector(pImage, pIoCtx, pExtent, uSectorExtentRel, &uSectorExtentAbs);
                        if (RT_SUCCESS(rc))
                        {
                            if (    pImage->uImageFlags & VD_VMDK_IMAGE_FLAGS_STREAM_OPTIMIZED
                                &&  uSectorExtentRel < (uint64_t)pExtent->uLastGrainAccess * pExtent->cSectorsPerGrain)
                                rc = VERR_VD_VMDK_INVALID_WRITE;
                            else
                            {
                                /* Clip write range to at most the rest of the grain. */
                                cbToWrite = RT_MIN(cbToWrite,
                                                   VMDK_SECTOR2BYTE(  pExtent->cSectorsPerGrain
                                                                    - uSectorExtentRel % pExtent->cSectorsPerGrain));
                                if (uSectorExtentAbs == 0)
                                {
                                    if (!(pImage->uImageFlags & VD_VMDK_IMAGE_FLAGS_STREAM_OPTIMIZED))
                                    {
                                        if (cbToWrite == VMDK_SECTOR2BYTE(pExtent->cSectorsPerGrain))
                                        {
                                            /* Full block write to a previously unallocated block.
                                             * Check if the caller wants to avoid the automatic alloc. */
                                            if (!(fWrite & VD_WRITE_NO_ALLOC))
                                            {
                                                /* Allocate GT and find out where to store the grain. */
                                                rc = vmdkAllocGrain(pImage, pExtent, pIoCtx,
                                                                    uSectorExtentRel, cbToWrite);
                                            }
                                            else
                                                rc = VERR_VD_BLOCK_FREE;
                                            *pcbPreRead = 0;
                                            *pcbPostRead = 0;
                                        }
                                        else
                                        {
                                            /* Clip write range to remain in this extent. */
                                            cbToWrite = RT_MIN(cbToWrite,
                                                               VMDK_SECTOR2BYTE(  pExtent->uSectorOffset
                                                                                + pExtent->cNominalSectors - uSectorExtentRel));
                                            *pcbPreRead = VMDK_SECTOR2BYTE(uSectorExtentRel % pExtent->cSectorsPerGrain);
                                            *pcbPostRead = VMDK_SECTOR2BYTE(pExtent->cSectorsPerGrain) - cbToWrite - *pcbPreRead;
                                            rc = VERR_VD_BLOCK_FREE;
                                        }
                                    }
                                    else
                                        rc = vmdkStreamAllocGrain(pImage, pExtent, uSectorExtentRel,
                                                                  pIoCtx, cbToWrite);
                                }
                                else
                                {
                                    if (pImage->uImageFlags & VD_VMDK_IMAGE_FLAGS_STREAM_OPTIMIZED)
                                    {
                                        /* A partial write to a streamOptimized image is simply
                                         * invalid. It requires rewriting already compressed data
                                         * which is somewhere between expensive and impossible. */
                                        rc = VERR_VD_VMDK_INVALID_STATE;
                                        pExtent->uGrainSectorAbs = 0;
                                        AssertRC(rc);
                                    }
                                    else
                                    {
                                        Assert(!(pImage->uImageFlags & VD_VMDK_IMAGE_FLAGS_STREAM_OPTIMIZED));
                                        rc = vdIfIoIntFileWriteUser(pImage->pIfIo, pExtent->pFile->pStorage,
                                                                    VMDK_SECTOR2BYTE(uSectorExtentAbs),
                                                                    pIoCtx, cbToWrite, NULL, NULL);
                                    }
                                }
                            }
                        }
                        break;
                    case VMDKETYPE_VMFS:
                    case VMDKETYPE_FLAT:
                        /* Clip write range to remain in this extent. */
                        cbToWrite = RT_MIN(cbToWrite, VMDK_SECTOR2BYTE(pExtent->uSectorOffset + pExtent->cNominalSectors - uSectorExtentRel));
                        rc = vdIfIoIntFileWriteUser(pImage->pIfIo, pExtent->pFile->pStorage,
                                                    VMDK_SECTOR2BYTE(uSectorExtentRel),
                                                    pIoCtx, cbToWrite, NULL, NULL);
                        break;
                    case VMDKETYPE_ZERO:
                        /* Clip write range to remain in this extent. */
                        cbToWrite = RT_MIN(cbToWrite, VMDK_SECTOR2BYTE(pExtent->uSectorOffset + pExtent->cNominalSectors - uSectorExtentRel));
                        break;
                }
            }

            if (pcbWriteProcess)
                *pcbWriteProcess = cbToWrite;
        }
    }
    else
        rc = VERR_VD_IMAGE_READ_ONLY;

    LogFlowFunc(("returns %Rrc\n", rc));
    return rc;
}

/** @copydoc VDIMAGEBACKEND::pfnFlush */
static DECLCALLBACK(int) vmdkFlush(void *pBackendData, PVDIOCTX pIoCtx)
{
    PVMDKIMAGE pImage = (PVMDKIMAGE)pBackendData;

    return vmdkFlushImage(pImage, pIoCtx);
}

/** @copydoc VDIMAGEBACKEND::pfnGetVersion */
static DECLCALLBACK(unsigned) vmdkGetVersion(void *pBackendData)
{
    LogFlowFunc(("pBackendData=%#p\n", pBackendData));
    PVMDKIMAGE pImage = (PVMDKIMAGE)pBackendData;

    AssertPtrReturn(pImage, 0);

    return VMDK_IMAGE_VERSION;
}

/** @copydoc VDIMAGEBACKEND::pfnGetFileSize */
static DECLCALLBACK(uint64_t) vmdkGetFileSize(void *pBackendData)
{
    LogFlowFunc(("pBackendData=%#p\n", pBackendData));
    PVMDKIMAGE pImage = (PVMDKIMAGE)pBackendData;
    uint64_t cb = 0;

    AssertPtrReturn(pImage, 0);

    if (pImage->pFile != NULL)
    {
        uint64_t cbFile;
        int rc = vdIfIoIntFileGetSize(pImage->pIfIo, pImage->pFile->pStorage, &cbFile);
        if (RT_SUCCESS(rc))
            cb += cbFile;
    }
    for (unsigned i = 0; i < pImage->cExtents; i++)
    {
        if (pImage->pExtents[i].pFile != NULL)
        {
            uint64_t cbFile;
            int rc = vdIfIoIntFileGetSize(pImage->pIfIo, pImage->pExtents[i].pFile->pStorage, &cbFile);
            if (RT_SUCCESS(rc))
                cb += cbFile;
        }
    }

    LogFlowFunc(("returns %lld\n", cb));
    return cb;
}

/** @copydoc VDIMAGEBACKEND::pfnGetPCHSGeometry */
static DECLCALLBACK(int) vmdkGetPCHSGeometry(void *pBackendData, PVDGEOMETRY pPCHSGeometry)
{
    LogFlowFunc(("pBackendData=%#p pPCHSGeometry=%#p\n", pBackendData, pPCHSGeometry));
    PVMDKIMAGE pImage = (PVMDKIMAGE)pBackendData;
    int rc = VINF_SUCCESS;

    AssertPtrReturn(pImage, VERR_VD_NOT_OPENED);

    if (pImage->PCHSGeometry.cCylinders)
        *pPCHSGeometry = pImage->PCHSGeometry;
    else
        rc = VERR_VD_GEOMETRY_NOT_SET;

    LogFlowFunc(("returns %Rrc (PCHS=%u/%u/%u)\n", rc, pPCHSGeometry->cCylinders, pPCHSGeometry->cHeads, pPCHSGeometry->cSectors));
    return rc;
}

/** @copydoc VDIMAGEBACKEND::pfnSetPCHSGeometry */
static DECLCALLBACK(int) vmdkSetPCHSGeometry(void *pBackendData, PCVDGEOMETRY pPCHSGeometry)
{
    LogFlowFunc(("pBackendData=%#p pPCHSGeometry=%#p PCHS=%u/%u/%u\n",
                 pBackendData, pPCHSGeometry, pPCHSGeometry->cCylinders, pPCHSGeometry->cHeads, pPCHSGeometry->cSectors));
    PVMDKIMAGE pImage = (PVMDKIMAGE)pBackendData;
    int rc = VINF_SUCCESS;

    AssertPtrReturn(pImage, VERR_VD_NOT_OPENED);

    if (!(pImage->uOpenFlags & VD_OPEN_FLAGS_READONLY))
    {
        if (!(pImage->uOpenFlags & VD_VMDK_IMAGE_FLAGS_STREAM_OPTIMIZED))
        {
            rc = vmdkDescSetPCHSGeometry(pImage, pPCHSGeometry);
            if (RT_SUCCESS(rc))
                pImage->PCHSGeometry = *pPCHSGeometry;
        }
        else
            rc = VERR_NOT_SUPPORTED;
    }
    else
        rc = VERR_VD_IMAGE_READ_ONLY;

    LogFlowFunc(("returns %Rrc\n", rc));
    return rc;
}

/** @copydoc VDIMAGEBACKEND::pfnGetLCHSGeometry */
static DECLCALLBACK(int) vmdkGetLCHSGeometry(void *pBackendData, PVDGEOMETRY pLCHSGeometry)
{
    LogFlowFunc(("pBackendData=%#p pLCHSGeometry=%#p\n", pBackendData, pLCHSGeometry));
    PVMDKIMAGE pImage = (PVMDKIMAGE)pBackendData;
    int rc = VINF_SUCCESS;

    AssertPtrReturn(pImage, VERR_VD_NOT_OPENED);

    if (pImage->LCHSGeometry.cCylinders)
        *pLCHSGeometry = pImage->LCHSGeometry;
    else
        rc = VERR_VD_GEOMETRY_NOT_SET;

    LogFlowFunc(("returns %Rrc (LCHS=%u/%u/%u)\n", rc, pLCHSGeometry->cCylinders, pLCHSGeometry->cHeads, pLCHSGeometry->cSectors));
    return rc;
}

/** @copydoc VDIMAGEBACKEND::pfnSetLCHSGeometry */
static DECLCALLBACK(int) vmdkSetLCHSGeometry(void *pBackendData, PCVDGEOMETRY pLCHSGeometry)
{
    LogFlowFunc(("pBackendData=%#p pLCHSGeometry=%#p LCHS=%u/%u/%u\n",
                 pBackendData, pLCHSGeometry, pLCHSGeometry->cCylinders, pLCHSGeometry->cHeads, pLCHSGeometry->cSectors));
    PVMDKIMAGE pImage = (PVMDKIMAGE)pBackendData;
    int rc = VINF_SUCCESS;

    AssertPtrReturn(pImage, VERR_VD_NOT_OPENED);

    if (!(pImage->uOpenFlags & VD_OPEN_FLAGS_READONLY))
    {
        if (!(pImage->uOpenFlags & VD_VMDK_IMAGE_FLAGS_STREAM_OPTIMIZED))
        {
            rc = vmdkDescSetLCHSGeometry(pImage, pLCHSGeometry);
            if (RT_SUCCESS(rc))
                pImage->LCHSGeometry = *pLCHSGeometry;
        }
        else
            rc = VERR_NOT_SUPPORTED;
    }
    else
        rc = VERR_VD_IMAGE_READ_ONLY;

    LogFlowFunc(("returns %Rrc\n", rc));
    return rc;
}

/** @copydoc VDIMAGEBACKEND::pfnQueryRegions */
static DECLCALLBACK(int) vmdkQueryRegions(void *pBackendData, PCVDREGIONLIST *ppRegionList)
{
    LogFlowFunc(("pBackendData=%#p ppRegionList=%#p\n", pBackendData, ppRegionList));
    PVMDKIMAGE pThis = (PVMDKIMAGE)pBackendData;

    AssertPtrReturn(pThis, VERR_VD_NOT_OPENED);

    *ppRegionList = &pThis->RegionList;
    LogFlowFunc(("returns %Rrc\n", VINF_SUCCESS));
    return VINF_SUCCESS;
}

/** @copydoc VDIMAGEBACKEND::pfnRegionListRelease */
static DECLCALLBACK(void) vmdkRegionListRelease(void *pBackendData, PCVDREGIONLIST pRegionList)
{
    RT_NOREF1(pRegionList);
    LogFlowFunc(("pBackendData=%#p pRegionList=%#p\n", pBackendData, pRegionList));
    PVMDKIMAGE pThis = (PVMDKIMAGE)pBackendData;
    AssertPtr(pThis); RT_NOREF(pThis);

    /* Nothing to do here. */
}

/** @copydoc VDIMAGEBACKEND::pfnGetImageFlags */
static DECLCALLBACK(unsigned) vmdkGetImageFlags(void *pBackendData)
{
    LogFlowFunc(("pBackendData=%#p\n", pBackendData));
    PVMDKIMAGE pImage = (PVMDKIMAGE)pBackendData;

    AssertPtrReturn(pImage, 0);

    LogFlowFunc(("returns %#x\n", pImage->uImageFlags));
    return pImage->uImageFlags;
}

/** @copydoc VDIMAGEBACKEND::pfnGetOpenFlags */
static DECLCALLBACK(unsigned) vmdkGetOpenFlags(void *pBackendData)
{
    LogFlowFunc(("pBackendData=%#p\n", pBackendData));
    PVMDKIMAGE pImage = (PVMDKIMAGE)pBackendData;

    AssertPtrReturn(pImage, 0);

    LogFlowFunc(("returns %#x\n", pImage->uOpenFlags));
    return pImage->uOpenFlags;
}

/** @copydoc VDIMAGEBACKEND::pfnSetOpenFlags */
static DECLCALLBACK(int) vmdkSetOpenFlags(void *pBackendData, unsigned uOpenFlags)
{
    LogFlowFunc(("pBackendData=%#p uOpenFlags=%#x\n", pBackendData, uOpenFlags));
    PVMDKIMAGE pImage = (PVMDKIMAGE)pBackendData;
    int rc;

    /* Image must be opened and the new flags must be valid. */
    if (!pImage || (uOpenFlags & ~(  VD_OPEN_FLAGS_READONLY | VD_OPEN_FLAGS_INFO
                                   | VD_OPEN_FLAGS_ASYNC_IO | VD_OPEN_FLAGS_SHAREABLE
                                   | VD_OPEN_FLAGS_SEQUENTIAL | VD_OPEN_FLAGS_SKIP_CONSISTENCY_CHECKS)))
        rc = VERR_INVALID_PARAMETER;
    else
    {
        /* StreamOptimized images need special treatment: reopen is prohibited. */
        if (pImage->uImageFlags & VD_VMDK_IMAGE_FLAGS_STREAM_OPTIMIZED)
        {
            if (pImage->uOpenFlags == uOpenFlags)
                rc = VINF_SUCCESS;
            else
                rc = VERR_INVALID_PARAMETER;
        }
        else
        {
            /* Implement this operation via reopening the image. */
            vmdkFreeImage(pImage, false, true /*fFlush*/);
            rc = vmdkOpenImage(pImage, uOpenFlags);
        }
    }

    LogFlowFunc(("returns %Rrc\n", rc));
    return rc;
}

/** @copydoc VDIMAGEBACKEND::pfnGetComment */
static DECLCALLBACK(int) vmdkGetComment(void *pBackendData, char *pszComment, size_t cbComment)
{
    LogFlowFunc(("pBackendData=%#p pszComment=%#p cbComment=%zu\n", pBackendData, pszComment, cbComment));
    PVMDKIMAGE pImage = (PVMDKIMAGE)pBackendData;

    AssertPtrReturn(pImage, VERR_VD_NOT_OPENED);

    char *pszCommentEncoded = NULL;
    int rc = vmdkDescDDBGetStr(pImage, &pImage->Descriptor,
                               "ddb.comment", &pszCommentEncoded);
    if (rc == VERR_VD_VMDK_VALUE_NOT_FOUND)
    {
        pszCommentEncoded = NULL;
        rc = VINF_SUCCESS;
    }

    if (RT_SUCCESS(rc))
    {
        if (pszComment && pszCommentEncoded)
            rc = vmdkDecodeString(pszCommentEncoded, pszComment, cbComment);
        else if (pszComment)
                *pszComment = '\0';

        if (pszCommentEncoded)
            RTMemTmpFree(pszCommentEncoded);
    }

    LogFlowFunc(("returns %Rrc comment='%s'\n", rc, pszComment));
    return rc;
}

/** @copydoc VDIMAGEBACKEND::pfnSetComment */
static DECLCALLBACK(int) vmdkSetComment(void *pBackendData, const char *pszComment)
{
    LogFlowFunc(("pBackendData=%#p pszComment=\"%s\"\n", pBackendData, pszComment));
    PVMDKIMAGE pImage = (PVMDKIMAGE)pBackendData;
    int rc;

    AssertPtrReturn(pImage, VERR_VD_NOT_OPENED);

    if (!(pImage->uOpenFlags & VD_OPEN_FLAGS_READONLY))
    {
        if (!(pImage->uOpenFlags & VD_VMDK_IMAGE_FLAGS_STREAM_OPTIMIZED))
            rc = vmdkSetImageComment(pImage, pszComment);
        else
            rc = VERR_NOT_SUPPORTED;
    }
    else
        rc = VERR_VD_IMAGE_READ_ONLY;

    LogFlowFunc(("returns %Rrc\n", rc));
    return rc;
}

/** @copydoc VDIMAGEBACKEND::pfnGetUuid */
static DECLCALLBACK(int) vmdkGetUuid(void *pBackendData, PRTUUID pUuid)
{
    LogFlowFunc(("pBackendData=%#p pUuid=%#p\n", pBackendData, pUuid));
    PVMDKIMAGE pImage = (PVMDKIMAGE)pBackendData;

    AssertPtrReturn(pImage, VERR_VD_NOT_OPENED);

    *pUuid = pImage->ImageUuid;

    LogFlowFunc(("returns %Rrc (%RTuuid)\n", VINF_SUCCESS, pUuid));
    return VINF_SUCCESS;
}

/** @copydoc VDIMAGEBACKEND::pfnSetUuid */
static DECLCALLBACK(int) vmdkSetUuid(void *pBackendData, PCRTUUID pUuid)
{
    LogFlowFunc(("pBackendData=%#p Uuid=%RTuuid\n", pBackendData, pUuid));
    PVMDKIMAGE pImage = (PVMDKIMAGE)pBackendData;
    int rc = VINF_SUCCESS;

    AssertPtrReturn(pImage, VERR_VD_NOT_OPENED);

    if (!(pImage->uOpenFlags & VD_OPEN_FLAGS_READONLY))
    {
        if (!(pImage->uOpenFlags & VD_VMDK_IMAGE_FLAGS_STREAM_OPTIMIZED))
        {
            pImage->ImageUuid = *pUuid;
            rc = vmdkDescDDBSetUuid(pImage, &pImage->Descriptor,
                                    VMDK_DDB_IMAGE_UUID, pUuid);
            if (RT_FAILURE(rc))
                rc = vdIfError(pImage->pIfError, rc, RT_SRC_POS,
                               N_("VMDK: error storing image UUID in descriptor in '%s'"), pImage->pszFilename);
        }
        else
            rc = VERR_NOT_SUPPORTED;
    }
    else
        rc = VERR_VD_IMAGE_READ_ONLY;

    LogFlowFunc(("returns %Rrc\n", rc));
    return rc;
}

/** @copydoc VDIMAGEBACKEND::pfnGetModificationUuid */
static DECLCALLBACK(int) vmdkGetModificationUuid(void *pBackendData, PRTUUID pUuid)
{
    LogFlowFunc(("pBackendData=%#p pUuid=%#p\n", pBackendData, pUuid));
    PVMDKIMAGE pImage = (PVMDKIMAGE)pBackendData;

    AssertPtrReturn(pImage, VERR_VD_NOT_OPENED);

    *pUuid = pImage->ModificationUuid;

    LogFlowFunc(("returns %Rrc (%RTuuid)\n", VINF_SUCCESS, pUuid));
    return VINF_SUCCESS;
}

/** @copydoc VDIMAGEBACKEND::pfnSetModificationUuid */
static DECLCALLBACK(int) vmdkSetModificationUuid(void *pBackendData, PCRTUUID pUuid)
{
    LogFlowFunc(("pBackendData=%#p Uuid=%RTuuid\n", pBackendData, pUuid));
    PVMDKIMAGE pImage = (PVMDKIMAGE)pBackendData;
    int rc = VINF_SUCCESS;

    AssertPtrReturn(pImage, VERR_VD_NOT_OPENED);

    if (!(pImage->uOpenFlags & VD_OPEN_FLAGS_READONLY))
    {
        if (!(pImage->uOpenFlags & VD_VMDK_IMAGE_FLAGS_STREAM_OPTIMIZED))
        {
            /* Only touch the modification uuid if it changed. */
            if (RTUuidCompare(&pImage->ModificationUuid, pUuid))
            {
                pImage->ModificationUuid = *pUuid;
                rc = vmdkDescDDBSetUuid(pImage, &pImage->Descriptor,
                                        VMDK_DDB_MODIFICATION_UUID, pUuid);
                if (RT_FAILURE(rc))
                    rc = vdIfError(pImage->pIfError, rc, RT_SRC_POS, N_("VMDK: error storing modification UUID in descriptor in '%s'"), pImage->pszFilename);
            }
        }
        else
            rc = VERR_NOT_SUPPORTED;
    }
    else
        rc = VERR_VD_IMAGE_READ_ONLY;

    LogFlowFunc(("returns %Rrc\n", rc));
    return rc;
}

/** @copydoc VDIMAGEBACKEND::pfnGetParentUuid */
static DECLCALLBACK(int) vmdkGetParentUuid(void *pBackendData, PRTUUID pUuid)
{
    LogFlowFunc(("pBackendData=%#p pUuid=%#p\n", pBackendData, pUuid));
    PVMDKIMAGE pImage = (PVMDKIMAGE)pBackendData;

    AssertPtrReturn(pImage, VERR_VD_NOT_OPENED);

    *pUuid = pImage->ParentUuid;

    LogFlowFunc(("returns %Rrc (%RTuuid)\n", VINF_SUCCESS, pUuid));
    return VINF_SUCCESS;
}

/** @copydoc VDIMAGEBACKEND::pfnSetParentUuid */
static DECLCALLBACK(int) vmdkSetParentUuid(void *pBackendData, PCRTUUID pUuid)
{
    LogFlowFunc(("pBackendData=%#p Uuid=%RTuuid\n", pBackendData, pUuid));
    PVMDKIMAGE pImage = (PVMDKIMAGE)pBackendData;
    int rc = VINF_SUCCESS;

    AssertPtrReturn(pImage, VERR_VD_NOT_OPENED);

    if (!(pImage->uOpenFlags & VD_OPEN_FLAGS_READONLY))
    {
        if (!(pImage->uOpenFlags & VD_VMDK_IMAGE_FLAGS_STREAM_OPTIMIZED))
        {
            pImage->ParentUuid = *pUuid;
            rc = vmdkDescDDBSetUuid(pImage, &pImage->Descriptor,
                                    VMDK_DDB_PARENT_UUID, pUuid);
            if (RT_FAILURE(rc))
                rc = vdIfError(pImage->pIfError, rc, RT_SRC_POS,
                               N_("VMDK: error storing parent image UUID in descriptor in '%s'"), pImage->pszFilename);
        }
        else
            rc = VERR_NOT_SUPPORTED;
    }
    else
        rc = VERR_VD_IMAGE_READ_ONLY;

    LogFlowFunc(("returns %Rrc\n", rc));
    return rc;
}

/** @copydoc VDIMAGEBACKEND::pfnGetParentModificationUuid */
static DECLCALLBACK(int) vmdkGetParentModificationUuid(void *pBackendData, PRTUUID pUuid)
{
    LogFlowFunc(("pBackendData=%#p pUuid=%#p\n", pBackendData, pUuid));
    PVMDKIMAGE pImage = (PVMDKIMAGE)pBackendData;

    AssertPtrReturn(pImage, VERR_VD_NOT_OPENED);

    *pUuid = pImage->ParentModificationUuid;

    LogFlowFunc(("returns %Rrc (%RTuuid)\n", VINF_SUCCESS, pUuid));
    return VINF_SUCCESS;
}

/** @copydoc VDIMAGEBACKEND::pfnSetParentModificationUuid */
static DECLCALLBACK(int) vmdkSetParentModificationUuid(void *pBackendData, PCRTUUID pUuid)
{
    LogFlowFunc(("pBackendData=%#p Uuid=%RTuuid\n", pBackendData, pUuid));
    PVMDKIMAGE pImage = (PVMDKIMAGE)pBackendData;
    int rc = VINF_SUCCESS;

    AssertPtrReturn(pImage, VERR_VD_NOT_OPENED);

    if (!(pImage->uOpenFlags & VD_OPEN_FLAGS_READONLY))
    {
        if (!(pImage->uOpenFlags & VD_VMDK_IMAGE_FLAGS_STREAM_OPTIMIZED))
        {
            pImage->ParentModificationUuid = *pUuid;
            rc = vmdkDescDDBSetUuid(pImage, &pImage->Descriptor,
                                    VMDK_DDB_PARENT_MODIFICATION_UUID, pUuid);
            if (RT_FAILURE(rc))
                rc = vdIfError(pImage->pIfError, rc, RT_SRC_POS, N_("VMDK: error storing parent image UUID in descriptor in '%s'"), pImage->pszFilename);
        }
        else
            rc = VERR_NOT_SUPPORTED;
    }
    else
        rc = VERR_VD_IMAGE_READ_ONLY;

    LogFlowFunc(("returns %Rrc\n", rc));
    return rc;
}

/** @copydoc VDIMAGEBACKEND::pfnDump */
static DECLCALLBACK(void) vmdkDump(void *pBackendData)
{
    PVMDKIMAGE pImage = (PVMDKIMAGE)pBackendData;

    AssertPtrReturnVoid(pImage);
    vdIfErrorMessage(pImage->pIfError, "Header: Geometry PCHS=%u/%u/%u LCHS=%u/%u/%u cbSector=%llu\n",
                     pImage->PCHSGeometry.cCylinders, pImage->PCHSGeometry.cHeads, pImage->PCHSGeometry.cSectors,
                     pImage->LCHSGeometry.cCylinders, pImage->LCHSGeometry.cHeads, pImage->LCHSGeometry.cSectors,
                     VMDK_BYTE2SECTOR(pImage->cbSize));
    vdIfErrorMessage(pImage->pIfError, "Header: uuidCreation={%RTuuid}\n", &pImage->ImageUuid);
    vdIfErrorMessage(pImage->pIfError, "Header: uuidModification={%RTuuid}\n", &pImage->ModificationUuid);
    vdIfErrorMessage(pImage->pIfError, "Header: uuidParent={%RTuuid}\n", &pImage->ParentUuid);
    vdIfErrorMessage(pImage->pIfError, "Header: uuidParentModification={%RTuuid}\n", &pImage->ParentModificationUuid);
}



const VDIMAGEBACKEND g_VmdkBackend =
{
    /* u32Version */
    VD_IMGBACKEND_VERSION,
    /* pszBackendName */
    "VMDK",
    /* uBackendCaps */
      VD_CAP_UUID | VD_CAP_CREATE_FIXED | VD_CAP_CREATE_DYNAMIC
    | VD_CAP_CREATE_SPLIT_2G | VD_CAP_DIFF | VD_CAP_FILE | VD_CAP_ASYNC
    | VD_CAP_VFS | VD_CAP_PREFERRED,
    /* paFileExtensions */
    s_aVmdkFileExtensions,
    /* paConfigInfo */
    NULL,
    /* pfnProbe */
    vmdkProbe,
    /* pfnOpen */
    vmdkOpen,
    /* pfnCreate */
    vmdkCreate,
    /* pfnRename */
    vmdkRename,
    /* pfnClose */
    vmdkClose,
    /* pfnRead */
    vmdkRead,
    /* pfnWrite */
    vmdkWrite,
    /* pfnFlush */
    vmdkFlush,
    /* pfnDiscard */
    NULL,
    /* pfnGetVersion */
    vmdkGetVersion,
    /* pfnGetFileSize */
    vmdkGetFileSize,
    /* pfnGetPCHSGeometry */
    vmdkGetPCHSGeometry,
    /* pfnSetPCHSGeometry */
    vmdkSetPCHSGeometry,
    /* pfnGetLCHSGeometry */
    vmdkGetLCHSGeometry,
    /* pfnSetLCHSGeometry */
    vmdkSetLCHSGeometry,
    /* pfnQueryRegions */
    vmdkQueryRegions,
    /* pfnRegionListRelease */
    vmdkRegionListRelease,
    /* pfnGetImageFlags */
    vmdkGetImageFlags,
    /* pfnGetOpenFlags */
    vmdkGetOpenFlags,
    /* pfnSetOpenFlags */
    vmdkSetOpenFlags,
    /* pfnGetComment */
    vmdkGetComment,
    /* pfnSetComment */
    vmdkSetComment,
    /* pfnGetUuid */
    vmdkGetUuid,
    /* pfnSetUuid */
    vmdkSetUuid,
    /* pfnGetModificationUuid */
    vmdkGetModificationUuid,
    /* pfnSetModificationUuid */
    vmdkSetModificationUuid,
    /* pfnGetParentUuid */
    vmdkGetParentUuid,
    /* pfnSetParentUuid */
    vmdkSetParentUuid,
    /* pfnGetParentModificationUuid */
    vmdkGetParentModificationUuid,
    /* pfnSetParentModificationUuid */
    vmdkSetParentModificationUuid,
    /* pfnDump */
    vmdkDump,
    /* pfnGetTimestamp */
    NULL,
    /* pfnGetParentTimestamp */
    NULL,
    /* pfnSetParentTimestamp */
    NULL,
    /* pfnGetParentFilename */
    NULL,
    /* pfnSetParentFilename */
    NULL,
    /* pfnComposeLocation */
    genericFileComposeLocation,
    /* pfnComposeName */
    genericFileComposeName,
    /* pfnCompact */
    NULL,
    /* pfnResize */
    NULL,
    /* pfnRepair */
    NULL,
    /* pfnTraverseMetadata */
    NULL,
    /* u32VersionEnd */
    VD_IMGBACKEND_VERSION
};
