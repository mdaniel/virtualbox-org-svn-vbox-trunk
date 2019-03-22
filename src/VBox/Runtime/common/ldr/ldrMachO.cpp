/* $Id$ */
/** @file
 * kLdr - The Module Interpreter for the MACH-O format.
 */

/*
 * Copyright (c) 2006-2013 Knut St. Osmundsen <bird-kStuff-spamix@anduin.net>
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#define LOG_GROUP RTLOGGROUP_LDR
#include <iprt/ldr.h>
#include "internal/iprt.h"

#include <iprt/asm.h>
#include <iprt/assert.h>
#include <iprt/err.h>
#include <iprt/log.h>
#include <iprt/mem.h>
#include <iprt/string.h>

#include <iprt/formats/mach-o.h>
#include "internal/ldr.h"


/*********************************************************************************************************************************
*   Defined Constants And Macros                                                                                                 *
*********************************************************************************************************************************/
/** @def KLDRMODMACHO_STRICT
 * Define KLDRMODMACHO_STRICT to enabled strict checks in KLDRMODMACHO. */
#define KLDRMODMACHO_STRICT 1

/** @def KLDRMODMACHO_ASSERT
 * Assert that an expression is true when KLDR_STRICT is defined.
 */
#ifdef KLDRMODMACHO_STRICT
# define KLDRMODMACHO_ASSERT(expr)  Assert(expr)
#else
# define KLDRMODMACHO_ASSERT(expr)  do {} while (0)
#endif

/** @def KLDRMODMACHO_CHECK_RETURN
 * Checks that an expression is true and return if it isn't.
 * This is a debug aid.
 */
#ifdef KLDRMODMACHO_STRICT2
# define KLDRMODMACHO_CHECK_RETURN(expr, rc)  AssertReturn(expr, rc)
#else
# define KLDRMODMACHO_CHECK_RETURN(expr, rc)  do { if (!(expr)) { return (rc); } } while (0)
#endif

/** @def KLDRMODMACHO_CHECK_RETURN
 * Checks that an expression is true and return if it isn't.
 * This is a debug aid.
 */
#ifdef KLDRMODMACHO_STRICT2
# define KLDRMODMACHO_FAILED_RETURN(rc)  AssertFailedReturn(rc)
#else
# define KLDRMODMACHO_FAILED_RETURN(rc)  return (rc)
#endif


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/
/**
 * Mach-O section details.
 */
typedef struct KLDRMODMACHOSECT
{
    /** The size of the section (in bytes). */
    RTLDRADDR               cb;
    /** The link address of this section. */
    RTLDRADDR               LinkAddress;
    /** The RVA of this section. */
    RTLDRADDR               RVA;
    /** The file offset of this section.
     * This is -1 if the section doesn't have a file backing. */
    RTFOFF                  offFile;
    /** The number of fixups. */
    uint32_t                cFixups;
    /** The array of fixups. (lazy loaded) */
    macho_relocation_info_t *paFixups;
    /** The file offset of the fixups for this section.
     * This is -1 if the section doesn't have any fixups. */
    RTFOFF                  offFixups;
    /** Mach-O section flags. */
    uint32_t                fFlags;
    /** kLdr segment index. */
    uint32_t                iSegment;
    /** Pointer to the Mach-O section structure. */
    void                   *pvMachoSection;
} KLDRMODMACHOSECT, *PKLDRMODMACHOSECT;

/**
 * Extra per-segment info.
 *
 * This is corresponds to a kLdr segment, not a Mach-O segment!
 */
typedef struct KLDRMODMACHOSEG
{
    /** Common segment info. */
    RTLDRSEG                SegInfo;

    /** The orignal segment number (in case we had to resort it). */
    uint32_t                iOrgSegNo;
    /** The number of sections in the segment. */
    uint32_t                cSections;
    /** Pointer to the sections belonging to this segment.
     * The array resides in the big memory chunk allocated for
     * the module handle, so it doesn't need freeing. */
    PKLDRMODMACHOSECT       paSections;

} KLDRMODMACHOSEG, *PKLDRMODMACHOSEG;

/**
 * Instance data for the Mach-O MH_OBJECT module interpreter.
 * @todo interpret the other MH_* formats.
 */
typedef struct KLDRMODMACHO
{
    /** Core module structure. */
    RTLDRMODINTERNAL        Core;

    /** The minium cpu this module was built for.
     * This might not be accurate, so use kLdrModCanExecuteOn() to check. */
    RTLDRCPU                enmCpu;
    /** The number of segments in the module. */
    uint32_t                cSegments;

    /** Pointer to the RDR file mapping of the raw file bits. NULL if not mapped. */
    const void             *pvBits;
    /** Pointer to the user mapping. */
    void                   *pvMapping;
    /** The module open flags. */
    uint32_t                fOpenFlags;

    /** The offset of the image. (FAT fun.) */
    RTFOFF                  offImage;
    /** The link address. */
    RTLDRADDR               LinkAddress;
    /** The size of the mapped image. */
    RTLDRADDR               cbImage;
    /** Whether we're capable of loading the image. */
    bool                    fCanLoad;
    /** Whether we're creating a global offset table segment.
     * This dependes on the cputype and image type. */
    bool                    fMakeGot;
    /** The size of a indirect GOT jump stub entry.
     * This is 0 if not needed. */
    uint32_t                cbJmpStub;
    /** Effective file type.  If the original was a MH_OBJECT file, the
     * corresponding MH_DSYM needs the segment translation of a MH_OBJECT too.
     * The MH_DSYM normally has a separate __DWARF segment, but this is
     * automatically skipped during the transation. */
    uint32_t                uEffFileType;
    /** Pointer to the load commands. (endian converted) */
    uint8_t                *pbLoadCommands;
    /** The Mach-O header. (endian converted)
     * @remark The reserved field is only valid for real 64-bit headers. */
    mach_header_64_t        Hdr;

    /** The offset of the symbol table. */
    RTFOFF                  offSymbols;
    /** The number of symbols. */
    uint32_t                cSymbols;
    /** The pointer to the loaded symbol table. */
    void                   *pvaSymbols;
    /** The offset of the string table. */
    RTFOFF                  offStrings;
    /** The size of the of the string table. */
    uint32_t                cchStrings;
    /** Pointer to the loaded string table. */
    char                   *pchStrings;

    /** The image UUID, all zeros if not found. */
    uint32_t                abImageUuid[16];

    /** The RVA of the Global Offset Table. */
    RTLDRADDR               GotRVA;
    /** The RVA of the indirect GOT jump stubs.  */
    RTLDRADDR               JmpStubsRVA;

    /** The number of sections. */
    uint32_t                cSections;
    /** Pointer to the section array running in parallel to the Mach-O one. */
    PKLDRMODMACHOSECT       paSections;

    /** Array of segments parallel to the one in KLDRMOD. */
    KLDRMODMACHOSEG         aSegments[1];
} KLDRMODMACHO, *PKLDRMODMACHO;




/*********************************************************************************************************************************
*   Internal Functions                                                                                                           *
*********************************************************************************************************************************/
#if 0
static int32_t kldrModMachONumberOfImports(PRTLDRMODINTERNAL pMod, const void *pvBits);
#endif
static DECLCALLBACK(int) rtldrMachO_RelocateBits(PRTLDRMODINTERNAL pMod, void *pvBits, RTUINTPTR NewBaseAddress,
                                                 RTUINTPTR OldBaseAddress, PFNRTLDRIMPORT pfnGetImport, void *pvUser);


static int  kldrModMachOPreParseLoadCommands(uint8_t *pbLoadCommands, const mach_header_32_t *pHdr, PRTLDRREADER pRdr, RTFOFF   offImage,
                                             uint32_t fOpenFlags, uint32_t *pcSegments, uint32_t *pcSections, uint32_t *pcbStringPool,
                                             bool *pfCanLoad, PRTLDRADDR pLinkAddress, uint8_t *puEffFileType);
static int  kldrModMachOParseLoadCommands(PKLDRMODMACHO pThis, char *pbStringPool, uint32_t cbStringPool);

static int  kldrModMachOLoadObjSymTab(PKLDRMODMACHO pThis);
static int  kldrModMachOLoadFixups(PKLDRMODMACHO pThis, RTFOFF offFixups, uint32_t cFixups, macho_relocation_info_t **ppaFixups);
static int  kldrModMachOMapVirginBits(PKLDRMODMACHO pThis);

static int  kldrModMachODoQuerySymbol32Bit(PKLDRMODMACHO pThis, const macho_nlist_32_t *paSyms, uint32_t cSyms, const char *pchStrings,
                                           uint32_t cchStrings, RTLDRADDR BaseAddress, uint32_t iSymbol, const char *pchSymbol,
                                           uint32_t cchSymbol, PRTLDRADDR puValue, uint32_t *pfKind);
static int  kldrModMachODoQuerySymbol64Bit(PKLDRMODMACHO pThis, const macho_nlist_64_t *paSyms, uint32_t cSyms, const char *pchStrings,
                                           uint32_t cchStrings, RTLDRADDR BaseAddress, uint32_t iSymbol, const char *pchSymbol,
                                           uint32_t cchSymbol, PRTLDRADDR puValue, uint32_t *pfKind);
static int  kldrModMachODoEnumSymbols32Bit(PKLDRMODMACHO pThis, const macho_nlist_32_t *paSyms, uint32_t cSyms,
                                           const char *pchStrings, uint32_t cchStrings, RTLDRADDR BaseAddress,
                                           uint32_t fFlags, PFNRTLDRENUMSYMS pfnCallback, void *pvUser);
static int  kldrModMachODoEnumSymbols64Bit(PKLDRMODMACHO pThis, const macho_nlist_64_t *paSyms, uint32_t cSyms,
                                           const char *pchStrings, uint32_t cchStrings, RTLDRADDR BaseAddress,
                                           uint32_t fFlags, PFNRTLDRENUMSYMS pfnCallback, void *pvUser);
static int  kldrModMachOObjDoImports(PKLDRMODMACHO pThis, RTLDRADDR BaseAddress, PFNRTLDRIMPORT pfnGetImport, void *pvUser);
static int  kldrModMachOObjDoFixups(PKLDRMODMACHO pThis, void *pvMapping, RTLDRADDR NewBaseAddress);
static int  kldrModMachOFixupSectionGeneric32Bit(PKLDRMODMACHO pThis, uint8_t *pbSectBits, PKLDRMODMACHOSECT pFixupSect,
                                                 macho_nlist_32_t *paSyms, uint32_t cSyms, RTLDRADDR NewBaseAddress);
static int  kldrModMachOFixupSectionAMD64(PKLDRMODMACHO pThis, uint8_t *pbSectBits, PKLDRMODMACHOSECT pFixupSect,
                                          macho_nlist_64_t *paSyms, uint32_t cSyms, RTLDRADDR NewBaseAddress);

static int  kldrModMachOMakeGOT(PKLDRMODMACHO pThis, void *pvBits, RTLDRADDR NewBaseAddress);

/*static int  kldrModMachODoFixups(PKLDRMODMACHO pThis, void *pvMapping, RTLDRADDR NewBaseAddress, RTLDRADDR OldBaseAddress);
static int  kldrModMachODoImports(PKLDRMODMACHO pThis, void *pvMapping, PFNRTLDRIMPORT pfnGetImport, void *pvUser);*/



/**
 * Separate function for reading creating the Mach-O module instance to
 * simplify cleanup on failure.
 */
static int kldrModMachODoCreate(PRTLDRREADER pRdr, RTFOFF offImage, uint32_t fOpenFlags,
                                PKLDRMODMACHO *ppModMachO, PRTERRINFO pErrInfo)
{
    *ppModMachO = NULL;

    /*
     * Read the Mach-O header.
     */
    union
    {
        mach_header_32_t    Hdr32;
        mach_header_64_t    Hdr64;
    } s;
    Assert(&s.Hdr32.magic == &s.Hdr64.magic);
    Assert(&s.Hdr32.flags == &s.Hdr64.flags);
    int rc = pRdr->pfnRead(pRdr, &s, sizeof(s), offImage);
    if (rc)
        return RTErrInfoSetF(pErrInfo, rc, "Error reading Mach-O header at %RTfoff: %Rrc", offImage, rc);
    if (    s.Hdr32.magic != IMAGE_MACHO32_SIGNATURE
        &&  s.Hdr32.magic != IMAGE_MACHO64_SIGNATURE)
    {
        if (    s.Hdr32.magic == IMAGE_MACHO32_SIGNATURE_OE
            ||  s.Hdr32.magic == IMAGE_MACHO64_SIGNATURE_OE)
            return VERR_LDRMACHO_OTHER_ENDIAN_NOT_SUPPORTED;
        return VERR_INVALID_EXE_SIGNATURE;
    }

    /* sanity checks. */
    if (    s.Hdr32.sizeofcmds > pRdr->pfnSize(pRdr) - sizeof(mach_header_32_t)
        ||  s.Hdr32.sizeofcmds < sizeof(load_command_t) * s.Hdr32.ncmds
        ||  (s.Hdr32.flags & ~MH_VALID_FLAGS))
        return VERR_LDRMACHO_BAD_HEADER;

    bool fMakeGot;
    uint8_t cbJmpStub;
    switch (s.Hdr32.cputype)
    {
        case CPU_TYPE_X86:
            fMakeGot = false;
            cbJmpStub = 0;
            break;
        case CPU_TYPE_X86_64:
            fMakeGot = s.Hdr32.filetype == MH_OBJECT;
            cbJmpStub = fMakeGot ? 8 : 0;
            break;
        default:
            return VERR_LDRMACHO_UNSUPPORTED_MACHINE;
    }

    if (   s.Hdr32.filetype != MH_OBJECT
        && s.Hdr32.filetype != MH_EXECUTE
        && s.Hdr32.filetype != MH_DYLIB
        && s.Hdr32.filetype != MH_BUNDLE
        && s.Hdr32.filetype != MH_DSYM
        && s.Hdr32.filetype != MH_KEXT_BUNDLE)
        return VERR_LDRMACHO_UNSUPPORTED_FILE_TYPE;

    /*
     * Read and pre-parse the load commands to figure out how many segments we'll be needing.
     */
    uint8_t *pbLoadCommands = (uint8_t *)RTMemAlloc(s.Hdr32.sizeofcmds);
    if (!pbLoadCommands)
        return VERR_NO_MEMORY;
    rc = pRdr->pfnRead(pRdr, pbLoadCommands, s.Hdr32.sizeofcmds,
                          s.Hdr32.magic == IMAGE_MACHO32_SIGNATURE
                       || s.Hdr32.magic == IMAGE_MACHO32_SIGNATURE_OE
                       ? sizeof(mach_header_32_t) + offImage
                       : sizeof(mach_header_64_t) + offImage);

    uint32_t    cSegments    = 0;
    uint32_t    cSections    = 0;
    uint32_t    cbStringPool = 0;
    bool        fCanLoad     = true;
    RTLDRADDR   LinkAddress  = NIL_RTLDRADDR;
    uint8_t     uEffFileType = 0;
    if (!rc)
        rc = kldrModMachOPreParseLoadCommands(pbLoadCommands, &s.Hdr32, pRdr, offImage, fOpenFlags,
                                              &cSegments, &cSections, &cbStringPool, &fCanLoad, &LinkAddress, &uEffFileType);
    if (rc)
    {
        RTMemFree(pbLoadCommands);
        return rc;
    }
    cSegments += fMakeGot;


    /*
     * Calc the instance size, allocate and initialize it.
     */
    size_t const cbModAndSegs = RT_ALIGN_Z(RT_UOFFSETOF_DYN(KLDRMODMACHO, aSegments[cSegments])
                                           + sizeof(KLDRMODMACHOSECT) * cSections, 16);
    PKLDRMODMACHO pThis = (PKLDRMODMACHO)RTMemAlloc(cbModAndSegs + cbStringPool);
    if (!pThis)
        return VERR_NO_MEMORY;
    *ppModMachO = pThis;
    pThis->pbLoadCommands = pbLoadCommands;
    pThis->offImage = offImage;

    /* Core & CPU.*/
    pThis->Core.u32Magic  = 0;          /* set by caller */
    pThis->Core.eState    = LDR_STATE_OPENED;
    pThis->Core.pOps      = NULL;       /* set by caller. */
    pThis->Core.pReader   = pRdr;
    switch (s.Hdr32.cputype)
    {
        case CPU_TYPE_X86:
            pThis->Core.enmArch   = RTLDRARCH_X86_32;
            pThis->Core.enmEndian = RTLDRENDIAN_LITTLE;
            switch (s.Hdr32.cpusubtype)
            {
                case CPU_SUBTYPE_I386_ALL: /* == CPU_SUBTYPE_386 */
                    pThis->enmCpu = RTLDRCPU_X86_32_BLEND;
                    break;
                case CPU_SUBTYPE_486:
                    pThis->enmCpu = RTLDRCPU_I486;
                    break;
                case CPU_SUBTYPE_486SX:
                    pThis->enmCpu = RTLDRCPU_I486SX;
                    break;
                case CPU_SUBTYPE_PENT: /* == CPU_SUBTYPE_586 */
                    pThis->enmCpu = RTLDRCPU_I586;
                    break;
                case CPU_SUBTYPE_PENTPRO:
                case CPU_SUBTYPE_PENTII_M3:
                case CPU_SUBTYPE_PENTII_M5:
                case CPU_SUBTYPE_CELERON:
                case CPU_SUBTYPE_CELERON_MOBILE:
                case CPU_SUBTYPE_PENTIUM_3:
                case CPU_SUBTYPE_PENTIUM_3_M:
                case CPU_SUBTYPE_PENTIUM_3_XEON:
                    pThis->enmCpu = RTLDRCPU_I686;
                    break;
                case CPU_SUBTYPE_PENTIUM_M:
                case CPU_SUBTYPE_PENTIUM_4:
                case CPU_SUBTYPE_PENTIUM_4_M:
                case CPU_SUBTYPE_XEON:
                case CPU_SUBTYPE_XEON_MP:
                    pThis->enmCpu = RTLDRCPU_P4;
                    break;

                default:
                    /* Hack for kextutil output. */
                    if (   s.Hdr32.cpusubtype == 0
                        && s.Hdr32.filetype   == MH_OBJECT)
                        break;
                    return VERR_LDRMACHO_UNSUPPORTED_MACHINE;
            }
            break;

        case CPU_TYPE_X86_64:
            pThis->Core.enmArch   = RTLDRARCH_AMD64;
            pThis->Core.enmEndian = RTLDRENDIAN_LITTLE;
            switch (s.Hdr32.cpusubtype & ~CPU_SUBTYPE_MASK)
            {
                case CPU_SUBTYPE_X86_64_ALL: pThis->enmCpu = RTLDRCPU_AMD64_BLEND; break;
                default:
                    return VERR_LDRMACHO_UNSUPPORTED_MACHINE;
            }
            break;

        default:
            return VERR_LDRMACHO_UNSUPPORTED_MACHINE;
    }

    pThis->Core.enmFormat = RTLDRFMT_MACHO;
    switch (s.Hdr32.filetype)
    {
        case MH_OBJECT:     pThis->Core.enmType = RTLDRTYPE_OBJECT; break;
        case MH_EXECUTE:    pThis->Core.enmType = RTLDRTYPE_EXECUTABLE_FIXED; break;
        case MH_DYLIB:      pThis->Core.enmType = RTLDRTYPE_SHARED_LIBRARY_RELOCATABLE; break;
        case MH_BUNDLE:     pThis->Core.enmType = RTLDRTYPE_SHARED_LIBRARY_RELOCATABLE; break;
        case MH_KEXT_BUNDLE:pThis->Core.enmType = RTLDRTYPE_SHARED_LIBRARY_RELOCATABLE; break;
        case MH_DSYM:       pThis->Core.enmType = RTLDRTYPE_DEBUG_INFO; break;
        default:
            return VERR_LDRMACHO_UNSUPPORTED_FILE_TYPE;
    }

    /* KLDRMODMACHO */
    pThis->cSegments = cSegments;
    pThis->pvBits = NULL;
    pThis->pvMapping = NULL;
    pThis->fOpenFlags = fOpenFlags;
    pThis->Hdr = s.Hdr64;
    if (    s.Hdr32.magic == IMAGE_MACHO32_SIGNATURE
        ||  s.Hdr32.magic == IMAGE_MACHO32_SIGNATURE_OE)
        pThis->Hdr.reserved = 0;
    pThis->LinkAddress = LinkAddress;
    pThis->cbImage = 0;
    pThis->fCanLoad = fCanLoad;
    pThis->fMakeGot = fMakeGot;
    pThis->cbJmpStub = cbJmpStub;
    pThis->uEffFileType = uEffFileType;
    pThis->offSymbols = 0;
    pThis->cSymbols = 0;
    pThis->pvaSymbols = NULL;
    pThis->offStrings = 0;
    pThis->cchStrings = 0;
    pThis->pchStrings = NULL;
    memset(pThis->abImageUuid, 0, sizeof(pThis->abImageUuid));
    pThis->GotRVA = NIL_RTLDRADDR;
    pThis->JmpStubsRVA = NIL_RTLDRADDR;
    pThis->cSections = cSections;
    pThis->paSections = (PKLDRMODMACHOSECT)&pThis->aSegments[pThis->cSegments];

    /*
     * Setup the KLDRMOD segment array.
     */
    rc = kldrModMachOParseLoadCommands(pThis, (char *)pThis + cbModAndSegs, cbStringPool);
    if (rc)
        return rc;

    /*
     * We're done.
     */
    return 0;
}


/**
 * Converts, validates and preparses the load commands before we carve
 * out the module instance.
 *
 * The conversion that's preformed is format endian to host endian.  The
 * preparsing has to do with segment counting, section counting and string pool
 * sizing.
 *
 * Segment are created in two different ways, depending on the file type.
 *
 * For object files there is only one segment command without a given segment
 * name. The sections inside that segment have different segment names and are
 * not sorted by their segname attribute.  We create one segment for each
 * section, with the segment name being 'segname.sectname' in order to hopefully
 * keep the names unique.  Debug sections does not get segments.
 *
 * For non-object files, one kLdr segment is created for each Mach-O segment.
 * Debug segments is not exposed by kLdr via the kLdr segment table, but via the
 * debug enumeration callback API.
 *
 * @returns 0 on success.
 * @returns VERR_LDRMACHO_* on failure.
 * @param   pbLoadCommands  The load commands to parse.
 * @param   pHdr            The header.
 * @param   pRdr            The file reader.
 * @param   offImage        The image header (FAT fun).
 * @param   pcSegments      Where to store the segment count.
 * @param   pcSegments      Where to store the section count.
 * @param   pcbStringPool   Where to store the string pool size.
 * @param   pfCanLoad       Where to store the can-load-image indicator.
 * @param   pLinkAddress    Where to store the image link address (i.e. the
 *                          lowest segment address).
 */
static int  kldrModMachOPreParseLoadCommands(uint8_t *pbLoadCommands, const mach_header_32_t *pHdr, PRTLDRREADER pRdr, RTFOFF   offImage,
                                             uint32_t fOpenFlags, uint32_t *pcSegments, uint32_t *pcSections, uint32_t *pcbStringPool,
                                             bool *pfCanLoad, PRTLDRADDR pLinkAddress, uint8_t *puEffFileType)
{
    union
    {
        uint8_t              *pb;
        load_command_t       *pLoadCmd;
        segment_command_32_t *pSeg32;
        segment_command_64_t *pSeg64;
        thread_command_t     *pThread;
        symtab_command_t     *pSymTab;
        uuid_command_t       *pUuid;
    } u;
    const uint64_t cbFile = pRdr->pfnSize(pRdr) - offImage;
    uint32_t cSegments = 0;
    uint32_t cSections = 0;
    size_t cbStringPool = 0;
    uint32_t cLeft = pHdr->ncmds;
    uint32_t cbLeft = pHdr->sizeofcmds;
    uint8_t *pb = pbLoadCommands;
    int cSegmentCommands = 0;
    int cSymbolTabs = 0;
    int fConvertEndian = pHdr->magic == IMAGE_MACHO32_SIGNATURE_OE
                      || pHdr->magic == IMAGE_MACHO64_SIGNATURE_OE;
    uint8_t uEffFileType = *puEffFileType = pHdr->filetype;

    *pcSegments = 0;
    *pcSections = 0;
    *pcbStringPool = 0;
    *pfCanLoad = true;
    *pLinkAddress = ~(RTLDRADDR)0;

    while (cLeft-- > 0)
    {
        u.pb = pb;

        /*
         * Convert and validate command header.
         */
        KLDRMODMACHO_CHECK_RETURN(cbLeft >= sizeof(load_command_t), VERR_LDRMACHO_BAD_LOAD_COMMAND);
        if (fConvertEndian)
        {
            u.pLoadCmd->cmd = RT_BSWAP_U32(u.pLoadCmd->cmd);
            u.pLoadCmd->cmdsize = RT_BSWAP_U32(u.pLoadCmd->cmdsize);
        }
        KLDRMODMACHO_CHECK_RETURN(u.pLoadCmd->cmdsize <= cbLeft, VERR_LDRMACHO_BAD_LOAD_COMMAND);
        cbLeft -= u.pLoadCmd->cmdsize;
        pb += u.pLoadCmd->cmdsize;

        /*
         * Convert endian if needed, parse and validate the command.
         */
        switch (u.pLoadCmd->cmd)
        {
            case LC_SEGMENT_32:
            {
                segment_command_32_t *pSrcSeg = (segment_command_32_t *)u.pLoadCmd;
                section_32_t   *pFirstSect    = (section_32_t *)(pSrcSeg + 1);
                section_32_t   *pSect         = pFirstSect;
                uint32_t        cSectionsLeft = pSrcSeg->nsects;
                uint64_t            offSect       = 0;

                /* Convert and verify the segment. */
                KLDRMODMACHO_CHECK_RETURN(u.pLoadCmd->cmdsize >= sizeof(segment_command_32_t), VERR_LDRMACHO_BAD_LOAD_COMMAND);
                KLDRMODMACHO_CHECK_RETURN(   pHdr->magic == IMAGE_MACHO32_SIGNATURE_OE
                                          || pHdr->magic == IMAGE_MACHO32_SIGNATURE, VERR_LDRMACHO_BIT_MIX);
                if (fConvertEndian)
                {
                    pSrcSeg->vmaddr   = RT_BSWAP_U32(pSrcSeg->vmaddr);
                    pSrcSeg->vmsize   = RT_BSWAP_U32(pSrcSeg->vmsize);
                    pSrcSeg->fileoff  = RT_BSWAP_U32(pSrcSeg->fileoff);
                    pSrcSeg->filesize = RT_BSWAP_U32(pSrcSeg->filesize);
                    pSrcSeg->maxprot  = RT_BSWAP_U32(pSrcSeg->maxprot);
                    pSrcSeg->initprot = RT_BSWAP_U32(pSrcSeg->initprot);
                    pSrcSeg->nsects   = RT_BSWAP_U32(pSrcSeg->nsects);
                    pSrcSeg->flags    = RT_BSWAP_U32(pSrcSeg->flags);
                }

                /* Validation code shared with the 64-bit variant. */
                #define VALIDATE_AND_ADD_SEGMENT(a_cBits) \
                do { \
                    bool fSkipSeg = !strcmp(pSrcSeg->segname, "__DWARF")   /* Note: Not for non-object files. */ \
                                  || (   !strcmp(pSrcSeg->segname, "__CTF") /* Their CTF tool did/does weird things, */ \
                                      && pSrcSeg->vmsize == 0)                   /* overlapping vmaddr and zero vmsize. */ \
                                  || (cSectionsLeft > 0 && (pFirstSect->flags & S_ATTR_DEBUG)); \
                    \
                    /* MH_DSYM files for MH_OBJECT files must have MH_OBJECT segment translation. */ \
                    if (   uEffFileType == MH_DSYM \
                        && cSegmentCommands == 0 \
                        && pSrcSeg->segname[0] == '\0') \
                        *puEffFileType = uEffFileType = MH_OBJECT; \
                    \
                    KLDRMODMACHO_CHECK_RETURN(   pSrcSeg->filesize == 0 \
                                              || (   pSrcSeg->fileoff <= cbFile \
                                                  && (uint64_t)pSrcSeg->fileoff + pSrcSeg->filesize <= cbFile), \
                                              VERR_LDRMACHO_BAD_LOAD_COMMAND); \
                    KLDRMODMACHO_CHECK_RETURN(   pSrcSeg->filesize <= pSrcSeg->vmsize \
                                              || (fSkipSeg && !strcmp(pSrcSeg->segname, "__CTF") /* see above */), \
                                              VERR_LDRMACHO_BAD_LOAD_COMMAND); \
                    KLDRMODMACHO_CHECK_RETURN(!(~pSrcSeg->maxprot & pSrcSeg->initprot), \
                                              VERR_LDRMACHO_BAD_LOAD_COMMAND); \
                    KLDRMODMACHO_CHECK_RETURN(!(pSrcSeg->flags & ~(SG_HIGHVM | SG_FVMLIB | SG_NORELOC | SG_PROTECTED_VERSION_1)), \
                                              VERR_LDRMACHO_BAD_LOAD_COMMAND); \
                    KLDRMODMACHO_CHECK_RETURN(   pSrcSeg->nsects * sizeof(section_##a_cBits##_t) \
                                              <= u.pLoadCmd->cmdsize - sizeof(segment_command_##a_cBits##_t), \
                                              VERR_LDRMACHO_BAD_LOAD_COMMAND); \
                    KLDRMODMACHO_CHECK_RETURN(   uEffFileType != MH_OBJECT \
                                              || cSegmentCommands == 0 \
                                              || (   cSegmentCommands == 1 \
                                                  && uEffFileType == MH_OBJECT \
                                                  && pHdr->filetype == MH_DSYM \
                                                  && fSkipSeg), \
                                              VERR_LDRMACHO_BAD_OBJECT_FILE); \
                    cSegmentCommands++; \
                    \
                    /* Add the segment, if not object file. */ \
                    if (!fSkipSeg && uEffFileType != MH_OBJECT) \
                    { \
                        cbStringPool += RTStrNLen(&pSrcSeg->segname[0], sizeof(pSrcSeg->segname)) + 1; \
                        cSegments++; \
                        if (cSegments == 1) /* The link address is set by the first segment. */  \
                            *pLinkAddress = pSrcSeg->vmaddr; \
                    } \
                } while (0)

                VALIDATE_AND_ADD_SEGMENT(32);

                /*
                 * Convert, validate and parse the sections.
                 */
                cSectionsLeft = pSrcSeg->nsects;
                pFirstSect = pSect = (section_32_t *)(pSrcSeg + 1);
                while (cSectionsLeft-- > 0)
                {
                    if (fConvertEndian)
                    {
                        pSect->addr      = RT_BSWAP_U32(pSect->addr);
                        pSect->size      = RT_BSWAP_U32(pSect->size);
                        pSect->offset    = RT_BSWAP_U32(pSect->offset);
                        pSect->align     = RT_BSWAP_U32(pSect->align);
                        pSect->reloff    = RT_BSWAP_U32(pSect->reloff);
                        pSect->nreloc    = RT_BSWAP_U32(pSect->nreloc);
                        pSect->flags     = RT_BSWAP_U32(pSect->flags);
                        pSect->reserved1 = RT_BSWAP_U32(pSect->reserved1);
                        pSect->reserved2 = RT_BSWAP_U32(pSect->reserved2);
                    }

                    /* Validation code shared with the 64-bit variant. */
                    #define VALIDATE_AND_ADD_SECTION(a_cBits) \
                    do { \
                        int fFileBits; \
                        \
                        /* validate */ \
                        if (uEffFileType != MH_OBJECT) \
                            KLDRMODMACHO_CHECK_RETURN(!strcmp(pSect->segname, pSrcSeg->segname),\
                                                      VERR_LDRMACHO_BAD_SECTION); \
                        \
                        switch (pSect->flags & SECTION_TYPE) \
                        { \
                            case S_ZEROFILL: \
                                KLDRMODMACHO_CHECK_RETURN(!pSect->reserved1, VERR_LDRMACHO_BAD_SECTION); \
                                KLDRMODMACHO_CHECK_RETURN(!pSect->reserved2, VERR_LDRMACHO_BAD_SECTION); \
                                fFileBits = 0; \
                                break; \
                            case S_REGULAR: \
                            case S_CSTRING_LITERALS: \
                            case S_COALESCED: \
                            case S_4BYTE_LITERALS: \
                            case S_8BYTE_LITERALS: \
                            case S_16BYTE_LITERALS: \
                                KLDRMODMACHO_CHECK_RETURN(!pSect->reserved1, VERR_LDRMACHO_BAD_SECTION); \
                                KLDRMODMACHO_CHECK_RETURN(!pSect->reserved2, VERR_LDRMACHO_BAD_SECTION); \
                                fFileBits = 1; \
                                break; \
                            \
                            case S_SYMBOL_STUBS: \
                                KLDRMODMACHO_CHECK_RETURN(!pSect->reserved1, VERR_LDRMACHO_BAD_SECTION); \
                                /* reserved2 == stub size. 0 has been seen (corecrypto.kext) */ \
                                KLDRMODMACHO_CHECK_RETURN(pSect->reserved2 < 64, VERR_LDRMACHO_BAD_SECTION); \
                                fFileBits = 1; \
                                break; \
                            \
                            case S_NON_LAZY_SYMBOL_POINTERS: \
                            case S_LAZY_SYMBOL_POINTERS: \
                            case S_LAZY_DYLIB_SYMBOL_POINTERS: \
                                /* (reserved 1 = is indirect symbol table index) */ \
                                KLDRMODMACHO_CHECK_RETURN(!pSect->reserved2, VERR_LDRMACHO_BAD_SECTION); \
                                *pfCanLoad = false; \
                                fFileBits = -1; /* __DATA.__got in the 64-bit mach_kernel has bits, any things without bits? */ \
                                break; \
                            \
                            case S_MOD_INIT_FUNC_POINTERS: \
                                /** @todo this requires a query API or flag... (e.g. C++ constructors) */ \
                                KLDRMODMACHO_CHECK_RETURN(fOpenFlags & RTLDR_O_FOR_DEBUG, \
                                                          VERR_LDRMACHO_UNSUPPORTED_INIT_SECTION); \
                                /* Falls through. */ \
                            case S_MOD_TERM_FUNC_POINTERS: \
                                /** @todo this requires a query API or flag... (e.g. C++ destructors) */ \
                                KLDRMODMACHO_CHECK_RETURN(fOpenFlags & RTLDR_O_FOR_DEBUG, \
                                                          VERR_LDRMACHO_UNSUPPORTED_TERM_SECTION); \
                                KLDRMODMACHO_CHECK_RETURN(!pSect->reserved1, VERR_LDRMACHO_BAD_SECTION); \
                                KLDRMODMACHO_CHECK_RETURN(!pSect->reserved2, VERR_LDRMACHO_BAD_SECTION); \
                                fFileBits = 1; \
                                break; /* ignored */ \
                            \
                            case S_LITERAL_POINTERS: \
                            case S_DTRACE_DOF: \
                                KLDRMODMACHO_CHECK_RETURN(!pSect->reserved1, VERR_LDRMACHO_BAD_SECTION); \
                                KLDRMODMACHO_CHECK_RETURN(!pSect->reserved2, VERR_LDRMACHO_BAD_SECTION); \
                                fFileBits = 1; \
                                break; \
                            \
                            case S_INTERPOSING: \
                            case S_GB_ZEROFILL: \
                                KLDRMODMACHO_FAILED_RETURN(VERR_LDRMACHO_UNSUPPORTED_SECTION); \
                            \
                            default: \
                                KLDRMODMACHO_FAILED_RETURN(VERR_LDRMACHO_UNKNOWN_SECTION); \
                        } \
                        KLDRMODMACHO_CHECK_RETURN(!(pSect->flags & ~(  S_ATTR_PURE_INSTRUCTIONS | S_ATTR_NO_TOC | S_ATTR_STRIP_STATIC_SYMS \
                                                                     | S_ATTR_NO_DEAD_STRIP | S_ATTR_LIVE_SUPPORT | S_ATTR_SELF_MODIFYING_CODE \
                                                                     | S_ATTR_DEBUG | S_ATTR_SOME_INSTRUCTIONS | S_ATTR_EXT_RELOC \
                                                                     | S_ATTR_LOC_RELOC | SECTION_TYPE)), \
                                                  VERR_LDRMACHO_BAD_SECTION); \
                        KLDRMODMACHO_CHECK_RETURN((pSect->flags & S_ATTR_DEBUG) == (pFirstSect->flags & S_ATTR_DEBUG), \
                                                  VERR_LDRMACHO_MIXED_DEBUG_SECTION_FLAGS); \
                        \
                        KLDRMODMACHO_CHECK_RETURN(pSect->addr - pSrcSeg->vmaddr <= pSrcSeg->vmsize, \
                                                  VERR_LDRMACHO_BAD_SECTION); \
                        KLDRMODMACHO_CHECK_RETURN(   pSect->addr - pSrcSeg->vmaddr + pSect->size <= pSrcSeg->vmsize \
                                                  || !strcmp(pSrcSeg->segname, "__CTF") /* see above */, \
                                                  VERR_LDRMACHO_BAD_SECTION); \
                        KLDRMODMACHO_CHECK_RETURN(pSect->align < 31, \
                                                  VERR_LDRMACHO_BAD_SECTION); \
                        /* Workaround for buggy ld64 (or as, llvm, ++) that produces a misaligned __TEXT.__unwind_info. */ \
                        /* Seen: pSect->align = 4, pSect->addr = 0x5ebe14.  Just adjust the alignment down. */ \
                        if (   ((RT_BIT_32(pSect->align) - UINT32_C(1)) & pSect->addr) \
                            && pSect->align == 4 \
                            && strcmp(pSect->sectname, "__unwind_info") == 0) \
                            pSect->align = 2; \
                        KLDRMODMACHO_CHECK_RETURN(!((RT_BIT_32(pSect->align) - UINT32_C(1)) & pSect->addr), \
                                                  VERR_LDRMACHO_BAD_SECTION); \
                        KLDRMODMACHO_CHECK_RETURN(!((RT_BIT_32(pSect->align) - UINT32_C(1)) & pSrcSeg->vmaddr), \
                                                  VERR_LDRMACHO_BAD_SECTION); \
                        \
                        /* Adjust the section offset before we check file offset. */ \
                        offSect = (offSect + RT_BIT_64(pSect->align) - UINT64_C(1)) & ~(RT_BIT_64(pSect->align) - UINT64_C(1)); \
                        if (pSect->addr) \
                        { \
                            KLDRMODMACHO_CHECK_RETURN(offSect <= pSect->addr - pSrcSeg->vmaddr, VERR_LDRMACHO_BAD_SECTION); \
                            if (offSect < pSect->addr - pSrcSeg->vmaddr) \
                                offSect = pSect->addr - pSrcSeg->vmaddr; \
                        } \
                        \
                        if (fFileBits && pSect->offset == 0 && pSrcSeg->fileoff == 0 && pHdr->filetype == MH_DSYM) \
                            fFileBits = 0; \
                        if (fFileBits) \
                        { \
                            if (uEffFileType != MH_OBJECT) \
                            { \
                                KLDRMODMACHO_CHECK_RETURN(pSect->offset == pSrcSeg->fileoff + offSect, \
                                                          VERR_LDRMACHO_NON_CONT_SEG_BITS); \
                                KLDRMODMACHO_CHECK_RETURN(pSect->offset - pSrcSeg->fileoff <= pSrcSeg->filesize, \
                                                          VERR_LDRMACHO_BAD_SECTION); \
                            } \
                            KLDRMODMACHO_CHECK_RETURN(pSect->offset <= cbFile, \
                                                      VERR_LDRMACHO_BAD_SECTION); \
                            KLDRMODMACHO_CHECK_RETURN((uint64_t)pSect->offset + pSect->size <= cbFile, \
                                                      VERR_LDRMACHO_BAD_SECTION); \
                        } \
                        else \
                            KLDRMODMACHO_CHECK_RETURN(pSect->offset == 0, VERR_LDRMACHO_BAD_SECTION); \
                        \
                        if (!pSect->nreloc) \
                            KLDRMODMACHO_CHECK_RETURN(!pSect->reloff, \
                                                      VERR_LDRMACHO_BAD_SECTION); \
                        else \
                        { \
                            KLDRMODMACHO_CHECK_RETURN(pSect->reloff <= cbFile, \
                                                      VERR_LDRMACHO_BAD_SECTION); \
                            KLDRMODMACHO_CHECK_RETURN(     (uint64_t)pSect->reloff \
                                                         + (RTFOFF)pSect->nreloc * sizeof(macho_relocation_info_t) \
                                                      <= cbFile, \
                                                      VERR_LDRMACHO_BAD_SECTION); \
                        } \
                        \
                        /* Validate against file type (pointless?) and count the section, for object files add segment. */ \
                        switch (uEffFileType) \
                        { \
                            case MH_OBJECT: \
                                if (   !(pSect->flags & S_ATTR_DEBUG) \
                                    && strcmp(pSect->segname, "__DWARF")) \
                                { \
                                    cbStringPool += RTStrNLen(&pSect->segname[0], sizeof(pSect->segname)) + 1; \
                                    cbStringPool += RTStrNLen(&pSect->sectname[0], sizeof(pSect->sectname)) + 1; \
                                    cSegments++; \
                                    if (cSegments == 1) /* The link address is set by the first segment. */  \
                                        *pLinkAddress = pSect->addr; \
                                } \
                                /* Falls through. */ \
                            case MH_EXECUTE: \
                            case MH_DYLIB: \
                            case MH_BUNDLE: \
                            case MH_DSYM: \
                            case MH_KEXT_BUNDLE: \
                                cSections++; \
                                break; \
                            default: \
                                KLDRMODMACHO_FAILED_RETURN(VERR_INVALID_PARAMETER); \
                        } \
                        \
                        /* Advance the section offset, since we're also aligning it. */ \
                        offSect += pSect->size; \
                    } while (0) /* VALIDATE_AND_ADD_SECTION */

                    VALIDATE_AND_ADD_SECTION(32);

                    /* next */
                    pSect++;
                }
                break;
            }

            case LC_SEGMENT_64:
            {
                segment_command_64_t *pSrcSeg = (segment_command_64_t *)u.pLoadCmd;
                section_64_t   *pFirstSect    = (section_64_t *)(pSrcSeg + 1);
                section_64_t   *pSect         = pFirstSect;
                uint32_t        cSectionsLeft = pSrcSeg->nsects;
                uint64_t            offSect       = 0;

                /* Convert and verify the segment. */
                KLDRMODMACHO_CHECK_RETURN(u.pLoadCmd->cmdsize >= sizeof(segment_command_64_t), VERR_LDRMACHO_BAD_LOAD_COMMAND);
                KLDRMODMACHO_CHECK_RETURN(   pHdr->magic == IMAGE_MACHO64_SIGNATURE_OE
                                          || pHdr->magic == IMAGE_MACHO64_SIGNATURE, VERR_LDRMACHO_BIT_MIX);
                if (fConvertEndian)
                {
                    pSrcSeg->vmaddr   = RT_BSWAP_U64(pSrcSeg->vmaddr);
                    pSrcSeg->vmsize   = RT_BSWAP_U64(pSrcSeg->vmsize);
                    pSrcSeg->fileoff  = RT_BSWAP_U64(pSrcSeg->fileoff);
                    pSrcSeg->filesize = RT_BSWAP_U64(pSrcSeg->filesize);
                    pSrcSeg->maxprot  = RT_BSWAP_U32(pSrcSeg->maxprot);
                    pSrcSeg->initprot = RT_BSWAP_U32(pSrcSeg->initprot);
                    pSrcSeg->nsects   = RT_BSWAP_U32(pSrcSeg->nsects);
                    pSrcSeg->flags    = RT_BSWAP_U32(pSrcSeg->flags);
                }

                VALIDATE_AND_ADD_SEGMENT(64);

                /*
                 * Convert, validate and parse the sections.
                 */
                while (cSectionsLeft-- > 0)
                {
                    if (fConvertEndian)
                    {
                        pSect->addr      = RT_BSWAP_U64(pSect->addr);
                        pSect->size      = RT_BSWAP_U64(pSect->size);
                        pSect->offset    = RT_BSWAP_U32(pSect->offset);
                        pSect->align     = RT_BSWAP_U32(pSect->align);
                        pSect->reloff    = RT_BSWAP_U32(pSect->reloff);
                        pSect->nreloc    = RT_BSWAP_U32(pSect->nreloc);
                        pSect->flags     = RT_BSWAP_U32(pSect->flags);
                        pSect->reserved1 = RT_BSWAP_U32(pSect->reserved1);
                        pSect->reserved2 = RT_BSWAP_U32(pSect->reserved2);
                    }

                    VALIDATE_AND_ADD_SECTION(64);

                    /* next */
                    pSect++;
                }
                break;
            } /* LC_SEGMENT_64 */


            case LC_SYMTAB:
            {
                size_t cbSym;
                if (fConvertEndian)
                {
                    u.pSymTab->symoff  = RT_BSWAP_U32(u.pSymTab->symoff);
                    u.pSymTab->nsyms   = RT_BSWAP_U32(u.pSymTab->nsyms);
                    u.pSymTab->stroff  = RT_BSWAP_U32(u.pSymTab->stroff);
                    u.pSymTab->strsize = RT_BSWAP_U32(u.pSymTab->strsize);
                }

                /* verify */
                cbSym = pHdr->magic == IMAGE_MACHO32_SIGNATURE
                     || pHdr->magic == IMAGE_MACHO32_SIGNATURE_OE
                      ? sizeof(macho_nlist_32_t)
                      : sizeof(macho_nlist_64_t);
                if (    u.pSymTab->symoff >= cbFile
                    ||  (uint64_t)u.pSymTab->symoff + u.pSymTab->nsyms * cbSym > cbFile)
                    KLDRMODMACHO_FAILED_RETURN(VERR_LDRMACHO_BAD_LOAD_COMMAND);
                if (    u.pSymTab->stroff >= cbFile
                    ||  (uint64_t)u.pSymTab->stroff + u.pSymTab->strsize > cbFile)
                    KLDRMODMACHO_FAILED_RETURN(VERR_LDRMACHO_BAD_LOAD_COMMAND);

                /* only one string in objects, please. */
                cSymbolTabs++;
                if (    uEffFileType == MH_OBJECT
                    &&  cSymbolTabs != 1)
                    KLDRMODMACHO_FAILED_RETURN(VERR_LDRMACHO_BAD_OBJECT_FILE);
                break;
            }

            case LC_DYSYMTAB:
                /** @todo deal with this! */
                break;

            case LC_THREAD:
            case LC_UNIXTHREAD:
            {
                uint32_t *pu32 = (uint32_t *)(u.pb + sizeof(load_command_t));
                uint32_t cItemsLeft = (u.pThread->cmdsize - sizeof(load_command_t)) / sizeof(uint32_t);
                while (cItemsLeft)
                {
                    /* convert & verify header items ([0] == flavor, [1] == uint32_t count). */
                    if (cItemsLeft < 2)
                        KLDRMODMACHO_FAILED_RETURN(VERR_LDRMACHO_BAD_LOAD_COMMAND);
                    if (fConvertEndian)
                    {
                        pu32[0] = RT_BSWAP_U32(pu32[0]);
                        pu32[1] = RT_BSWAP_U32(pu32[1]);
                    }
                    if (pu32[1] + 2 > cItemsLeft)
                        KLDRMODMACHO_FAILED_RETURN(VERR_LDRMACHO_BAD_LOAD_COMMAND);

                    /* convert & verify according to flavor. */
                    switch (pu32[0])
                    {
                        /** @todo */
                        default:
                            break;
                    }

                    /* next */
                    cItemsLeft -= pu32[1] + 2;
                    pu32 += pu32[1] + 2;
                }
                break;
            }

            case LC_UUID:
                if (u.pUuid->cmdsize != sizeof(uuid_command_t))
                    KLDRMODMACHO_FAILED_RETURN(VERR_LDRMACHO_BAD_LOAD_COMMAND);
                /** @todo Check anything here need converting? */
                break;

            case LC_CODE_SIGNATURE:
                if (u.pUuid->cmdsize != sizeof(linkedit_data_command_t))
                    KLDRMODMACHO_FAILED_RETURN(VERR_LDRMACHO_BAD_LOAD_COMMAND);
                break;

            case LC_VERSION_MIN_MACOSX:
            case LC_VERSION_MIN_IPHONEOS:
                if (u.pUuid->cmdsize != sizeof(version_min_command_t))
                    KLDRMODMACHO_FAILED_RETURN(VERR_LDRMACHO_BAD_LOAD_COMMAND);
                break;

            case LC_SOURCE_VERSION:     /* Harmless. It just gives a clue regarding the source code revision/version. */
            case LC_DATA_IN_CODE:       /* Ignore */
            case LC_DYLIB_CODE_SIGN_DRS:/* Ignore */
                /** @todo valid command size. */
                break;

            case LC_FUNCTION_STARTS:    /** @todo dylib++ */
                /* Ignore for now. */
                break;
            case LC_ID_DYLIB:           /** @todo dylib */
            case LC_LOAD_DYLIB:         /** @todo dylib */
            case LC_LOAD_DYLINKER:      /** @todo dylib */
            case LC_TWOLEVEL_HINTS:     /** @todo dylib */
            case LC_LOAD_WEAK_DYLIB:    /** @todo dylib */
            case LC_ID_DYLINKER:        /** @todo dylib */
            case LC_RPATH:              /** @todo dylib */
            case LC_SEGMENT_SPLIT_INFO: /** @todo dylib++ */
            case LC_REEXPORT_DYLIB:     /** @todo dylib */
            case LC_DYLD_INFO:          /** @todo dylib */
            case LC_DYLD_INFO_ONLY:     /** @todo dylib */
            case LC_LOAD_UPWARD_DYLIB:  /** @todo dylib */
            case LC_DYLD_ENVIRONMENT:   /** @todo dylib */
            case LC_MAIN: /** @todo parse this and find and entry point or smth. */
                /** @todo valid command size. */
                if (!(fOpenFlags & RTLDR_O_FOR_DEBUG))
                    KLDRMODMACHO_FAILED_RETURN(VERR_LDRMACHO_UNSUPPORTED_LOAD_COMMAND);
                *pfCanLoad = false;
                break;

            case LC_LOADFVMLIB:
            case LC_IDFVMLIB:
            case LC_IDENT:
            case LC_FVMFILE:
            case LC_PREPAGE:
            case LC_PREBOUND_DYLIB:
            case LC_ROUTINES:
            case LC_ROUTINES_64:
            case LC_SUB_FRAMEWORK:
            case LC_SUB_UMBRELLA:
            case LC_SUB_CLIENT:
            case LC_SUB_LIBRARY:
            case LC_PREBIND_CKSUM:
            case LC_SYMSEG:
                KLDRMODMACHO_FAILED_RETURN(VERR_LDRMACHO_UNSUPPORTED_LOAD_COMMAND);

            default:
                KLDRMODMACHO_FAILED_RETURN(VERR_LDRMACHO_UNKNOWN_LOAD_COMMAND);
        }
    }

    /* be strict. */
    if (cbLeft)
        KLDRMODMACHO_FAILED_RETURN(VERR_LDRMACHO_BAD_LOAD_COMMAND);

    switch (uEffFileType)
    {
        case MH_OBJECT:
        case MH_EXECUTE:
        case MH_DYLIB:
        case MH_BUNDLE:
        case MH_DSYM:
        case MH_KEXT_BUNDLE:
            if (!cSegments)
                KLDRMODMACHO_FAILED_RETURN(VERR_LDRMACHO_BAD_OBJECT_FILE);
            break;
    }

    *pcSegments = cSegments;
    *pcSections = cSections;
    *pcbStringPool = (uint32_t)cbStringPool;

    return 0;
}


/**
 * Parses the load commands after we've carved out the module instance.
 *
 * This fills in the segment table and perhaps some other properties.
 *
 * @returns 0 on success.
 * @returns VERR_LDRMACHO_* on failure.
 * @param   pThis       The module.
 * @param   pbStringPool    The string pool
 * @param   cbStringPool    The size of the string pool.
 */
static int  kldrModMachOParseLoadCommands(PKLDRMODMACHO pThis, char *pbStringPool, uint32_t cbStringPool)
{
    union
    {
        const uint8_t              *pb;
        const load_command_t       *pLoadCmd;
        const segment_command_32_t *pSeg32;
        const segment_command_64_t *pSeg64;
        const symtab_command_t     *pSymTab;
        const uuid_command_t       *pUuid;
    } u;
    uint32_t cLeft = pThis->Hdr.ncmds;
    uint32_t cbLeft = pThis->Hdr.sizeofcmds;
    const uint8_t *pb = pThis->pbLoadCommands;
    PKLDRMODMACHOSEG pDstSeg = &pThis->aSegments[0];
    PKLDRMODMACHOSECT pSectExtra = pThis->paSections;
    const uint32_t cSegments = pThis->cSegments;
    PKLDRMODMACHOSEG pSegItr;
    RT_NOREF(cbStringPool);

    while (cLeft-- > 0)
    {
        u.pb = pb;
        cbLeft -= u.pLoadCmd->cmdsize;
        pb += u.pLoadCmd->cmdsize;

        /*
         * Convert endian if needed, parse and validate the command.
         */
        switch (u.pLoadCmd->cmd)
        {
            case LC_SEGMENT_32:
            {
                const segment_command_32_t *pSrcSeg = (const segment_command_32_t *)u.pLoadCmd;
                section_32_t   *pFirstSect    = (section_32_t *)(pSrcSeg + 1);
                section_32_t   *pSect         = pFirstSect;
                uint32_t        cSectionsLeft = pSrcSeg->nsects;

                /* Adds a segment, used by the macro below and thus shared with the 64-bit segment variant. */
                #define NEW_SEGMENT(a_cBits, a_achName1, a_fObjFile, a_achName2, a_SegAddr, a_cbSeg, a_fFileBits, a_offFile, a_cbFile) \
                do { \
                    pDstSeg->SegInfo.pszName = pbStringPool; \
                    pDstSeg->SegInfo.cchName = (uint32_t)RTStrNLen(a_achName1, sizeof(a_achName1)); \
                    memcpy(pbStringPool, a_achName1, pDstSeg->SegInfo.cchName); \
                    pbStringPool += pDstSeg->SegInfo.cchName; \
                    if (a_fObjFile) \
                    {   /* MH_OBJECT: Add '.sectname' - sections aren't sorted by segments. */ \
                        size_t cchName2 = RTStrNLen(a_achName2, sizeof(a_achName2)); \
                        *pbStringPool++ = '.'; \
                        memcpy(pbStringPool, a_achName2, cchName2); \
                        pbStringPool += cchName2; \
                        pDstSeg->SegInfo.cchName += (uint32_t)cchName2; \
                    } \
                    *pbStringPool++ = '\0'; \
                    pDstSeg->SegInfo.SelFlat = 0; \
                    pDstSeg->SegInfo.Sel16bit = 0; \
                    pDstSeg->SegInfo.fFlags = 0; \
                    pDstSeg->SegInfo.fProt = RTMEM_PROT_READ | RTMEM_PROT_WRITE | RTMEM_PROT_EXEC; /** @todo fixme! */ \
                    pDstSeg->SegInfo.cb = (a_cbSeg); \
                    pDstSeg->SegInfo.Alignment = 1; /* updated while parsing sections. */ \
                    pDstSeg->SegInfo.LinkAddress = (a_SegAddr); \
                    if (a_fFileBits) \
                    { \
                        pDstSeg->SegInfo.offFile = (RTFOFF)((a_offFile) + pThis->offImage); \
                        pDstSeg->SegInfo.cbFile  = (RTFOFF)(a_cbFile); \
                    } \
                    else \
                    { \
                        pDstSeg->SegInfo.offFile = -1; \
                        pDstSeg->SegInfo.cbFile  = -1; \
                    } \
                    pDstSeg->SegInfo.RVA = (a_SegAddr) - pThis->LinkAddress; \
                    pDstSeg->SegInfo.cbMapped = 0; \
                    \
                    pDstSeg->iOrgSegNo = (uint32_t)(pDstSeg - &pThis->aSegments[0]); \
                    pDstSeg->cSections = 0; \
                    pDstSeg->paSections = pSectExtra; \
                } while (0)

                /* Closes the new segment - part of NEW_SEGMENT. */
                #define CLOSE_SEGMENT() \
                do { \
                    pDstSeg->cSections = (uint32_t)(pSectExtra - pDstSeg->paSections); \
                    pDstSeg++; \
                } while (0)


                /* Shared with the 64-bit variant. */
                #define ADD_SEGMENT_AND_ITS_SECTIONS(a_cBits) \
                do { \
                    bool fAddSegOuter = false; \
                    \
                    /* \
                     * Check that the segment name is unique.  We couldn't do that \
                     * in the preparsing stage. \
                     */ \
                    if (pThis->uEffFileType != MH_OBJECT) \
                        for (pSegItr = &pThis->aSegments[0]; pSegItr != pDstSeg; pSegItr++) \
                            if (!strncmp(pSegItr->SegInfo.pszName, pSrcSeg->segname, sizeof(pSrcSeg->segname))) \
                                KLDRMODMACHO_FAILED_RETURN(VERR_LDR_DUPLICATE_SEGMENT_NAME); \
                    \
                    /* \
                     * Create a new segment, unless we're supposed to skip this one. \
                     */ \
                    if (   pThis->uEffFileType != MH_OBJECT \
                        && (cSectionsLeft == 0 || !(pFirstSect->flags & S_ATTR_DEBUG)) \
                        && strcmp(pSrcSeg->segname, "__DWARF") \
                        && strcmp(pSrcSeg->segname, "__CTF") ) \
                    { \
                        NEW_SEGMENT(a_cBits, pSrcSeg->segname, false /*a_fObjFile*/, 0 /*a_achName2*/, \
                                    pSrcSeg->vmaddr, pSrcSeg->vmsize, \
                                    pSrcSeg->filesize != 0, pSrcSeg->fileoff, pSrcSeg->filesize); \
                        fAddSegOuter = true; \
                    } \
                    \
                    /* \
                     * Convert and parse the sections. \
                     */ \
                    while (cSectionsLeft-- > 0) \
                    { \
                        /* New segment if object file. */ \
                        bool fAddSegInner = false; \
                        if (   pThis->uEffFileType == MH_OBJECT \
                            && !(pSect->flags & S_ATTR_DEBUG) \
                            && strcmp(pSrcSeg->segname, "__DWARF") \
                            && strcmp(pSrcSeg->segname, "__CTF") ) \
                        { \
                            Assert(!fAddSegOuter); \
                            NEW_SEGMENT(a_cBits, pSect->segname, true /*a_fObjFile*/, pSect->sectname, \
                                        pSect->addr, pSect->size, \
                                        pSect->offset != 0, pSect->offset, pSect->size); \
                            fAddSegInner = true; \
                        } \
                        \
                        /* Section data extract. */ \
                        pSectExtra->cb = pSect->size; \
                        pSectExtra->RVA = pSect->addr - pDstSeg->SegInfo.LinkAddress; \
                        pSectExtra->LinkAddress = pSect->addr; \
                        if (pSect->offset) \
                            pSectExtra->offFile = pSect->offset + pThis->offImage; \
                        else \
                            pSectExtra->offFile = -1; \
                        pSectExtra->cFixups = pSect->nreloc; \
                        pSectExtra->paFixups = NULL; \
                        if (pSect->nreloc) \
                            pSectExtra->offFixups = pSect->reloff + pThis->offImage; \
                        else \
                            pSectExtra->offFixups = -1; \
                        pSectExtra->fFlags = pSect->flags; \
                        pSectExtra->iSegment = (uint32_t)(pDstSeg - &pThis->aSegments[0]); \
                        pSectExtra->pvMachoSection = pSect; \
                        \
                        /* Update the segment alignment, if we're not skipping it. */ \
                        if (   (fAddSegOuter || fAddSegInner) \
                            && pDstSeg->SegInfo.Alignment < ((RTLDRADDR)1 << pSect->align)) \
                            pDstSeg->SegInfo.Alignment = (RTLDRADDR)1 << pSect->align; \
                        \
                        /* Next section, and if object file next segment. */ \
                        pSectExtra++; \
                        pSect++; \
                        if (fAddSegInner) \
                            CLOSE_SEGMENT(); \
                    } \
                    \
                    /* Close the segment and advance. */ \
                    if (fAddSegOuter) \
                        CLOSE_SEGMENT(); \
                } while (0) /* ADD_SEGMENT_AND_ITS_SECTIONS */

                ADD_SEGMENT_AND_ITS_SECTIONS(32);
                break;
            }

            case LC_SEGMENT_64:
            {
                const segment_command_64_t *pSrcSeg = (const segment_command_64_t *)u.pLoadCmd;
                section_64_t   *pFirstSect    = (section_64_t *)(pSrcSeg + 1);
                section_64_t   *pSect         = pFirstSect;
                uint32_t        cSectionsLeft = pSrcSeg->nsects;

                ADD_SEGMENT_AND_ITS_SECTIONS(64);
                break;
            }

            case LC_SYMTAB:
                switch (pThis->uEffFileType)
                {
                    case MH_OBJECT:
                    case MH_EXECUTE:
                    case MH_DYLIB: /** @todo ??? */
                    case MH_BUNDLE:  /** @todo ??? */
                    case MH_DSYM:
                    case MH_KEXT_BUNDLE:
                        pThis->offSymbols = u.pSymTab->symoff + pThis->offImage;
                        pThis->cSymbols = u.pSymTab->nsyms;
                        pThis->offStrings = u.pSymTab->stroff + pThis->offImage;
                        pThis->cchStrings = u.pSymTab->strsize;
                        break;
                }
                break;

            case LC_UUID:
                memcpy(pThis->abImageUuid, u.pUuid->uuid, sizeof(pThis->abImageUuid));
                break;

            default:
                break;
        } /* command switch */
    } /* while more commands */

    Assert(pDstSeg == &pThis->aSegments[cSegments - pThis->fMakeGot]);

    /*
     * Adjust mapping addresses calculating the image size.
     */
    {
        bool                fLoadLinkEdit = false;
        PKLDRMODMACHOSECT   pSectExtraItr;
        RTLDRADDR           uNextRVA = 0;
        RTLDRADDR           cb;
        uint32_t            cSegmentsToAdjust = cSegments - pThis->fMakeGot;
        uint32_t            c;

        for (;;)
        {
            /* Check if there is __DWARF segment at the end and make sure it's left
               out of the RVA negotiations and image loading. */
            if (   cSegmentsToAdjust > 0
                && !strcmp(pThis->aSegments[cSegmentsToAdjust - 1].SegInfo.pszName, "__DWARF"))
            {
                cSegmentsToAdjust--;
                pThis->aSegments[cSegmentsToAdjust].SegInfo.RVA = NIL_RTLDRADDR;
                pThis->aSegments[cSegmentsToAdjust].SegInfo.cbMapped = 0;
                continue;
            }

            /* If we're skipping the __LINKEDIT segment, check for it and adjust
               the number of segments we'll be messing with here.  ASSUMES it's
               last (by now anyway). */
            if (   !fLoadLinkEdit
                && cSegmentsToAdjust > 0
                && !strcmp(pThis->aSegments[cSegmentsToAdjust - 1].SegInfo.pszName, "__LINKEDIT"))
            {
                cSegmentsToAdjust--;
                pThis->aSegments[cSegmentsToAdjust].SegInfo.RVA = NIL_RTLDRADDR;
                pThis->aSegments[cSegmentsToAdjust].SegInfo.cbMapped = 0;
                continue;
            }
            break;
        }

        /* Adjust RVAs. */
        c = cSegmentsToAdjust;
        for (pDstSeg = &pThis->aSegments[0]; c-- > 0; pDstSeg++)
        {
            cb = pDstSeg->SegInfo.RVA - uNextRVA;
            if (cb >= 0x00100000) /* 1MB */
            {
                pDstSeg->SegInfo.RVA = uNextRVA;
                //pThis->pMod->fFlags |= KLDRMOD_FLAGS_NON_CONTIGUOUS_LINK_ADDRS;
            }
            uNextRVA = pDstSeg->SegInfo.RVA + RTLDR_ALIGN_ADDR(pDstSeg->SegInfo.cb, pDstSeg->SegInfo.Alignment);
        }

        /* Calculate the cbMapping members. */
        c = cSegmentsToAdjust;
        for (pDstSeg = &pThis->aSegments[0]; c-- > 1; pDstSeg++)
        {

            cb = pDstSeg[1].SegInfo.RVA - pDstSeg->SegInfo.RVA;
            pDstSeg->SegInfo.cbMapped = (size_t)cb == cb ? (size_t)cb : ~(size_t)0;
        }

        cb = RTLDR_ALIGN_ADDR(pDstSeg->SegInfo.cb, pDstSeg->SegInfo.Alignment);
        pDstSeg->SegInfo.cbMapped = (size_t)cb == cb ? (size_t)cb : ~(size_t)0;

        /* Set the image size. */
        pThis->cbImage = pDstSeg->SegInfo.RVA + cb;

        /* Fixup the section RVAs (internal). */
        c        = cSegmentsToAdjust;
        uNextRVA = pThis->cbImage;
        pDstSeg  = &pThis->aSegments[0];
        for (pSectExtraItr = pThis->paSections; pSectExtraItr != pSectExtra; pSectExtraItr++)
        {
            if (pSectExtraItr->iSegment < c)
                pSectExtraItr->RVA += pDstSeg[pSectExtraItr->iSegment].SegInfo.RVA;
            else
            {
                pSectExtraItr->RVA = uNextRVA;
                uNextRVA += RTLDR_ALIGN_ADDR(pSectExtraItr->cb, 64);
            }
        }
    }

    /*
     * Make the GOT segment if necessary.
     */
    if (pThis->fMakeGot)
    {
        uint32_t cbPtr = (   pThis->Hdr.magic == IMAGE_MACHO32_SIGNATURE
                      || pThis->Hdr.magic == IMAGE_MACHO32_SIGNATURE_OE)
                   ? sizeof(uint32_t)
                   : sizeof(uint64_t);
        uint32_t cbGot = pThis->cSymbols * cbPtr;
        uint32_t cbJmpStubs;

        pThis->GotRVA = pThis->cbImage;

        if (pThis->cbJmpStub)
        {
            cbGot = RT_ALIGN_Z(cbGot, 64);
            pThis->JmpStubsRVA = pThis->GotRVA + cbGot;
            cbJmpStubs = pThis->cbJmpStub * pThis->cSymbols;
        }
        else
        {
            pThis->JmpStubsRVA = NIL_RTLDRADDR;
            cbJmpStubs = 0;
        }

        pDstSeg = &pThis->aSegments[cSegments - 1];
        pDstSeg->SegInfo.pszName = "GOT";
        pDstSeg->SegInfo.cchName = 3;
        pDstSeg->SegInfo.SelFlat = 0;
        pDstSeg->SegInfo.Sel16bit = 0;
        pDstSeg->SegInfo.fFlags = 0;
        pDstSeg->SegInfo.fProt = RTMEM_PROT_READ;
        pDstSeg->SegInfo.cb = cbGot + cbJmpStubs;
        pDstSeg->SegInfo.Alignment = 64;
        pDstSeg->SegInfo.LinkAddress = pThis->LinkAddress + pThis->GotRVA;
        pDstSeg->SegInfo.offFile = -1;
        pDstSeg->SegInfo.cbFile  = -1;
        pDstSeg->SegInfo.RVA = pThis->GotRVA;
        pDstSeg->SegInfo.cbMapped = (size_t)RTLDR_ALIGN_ADDR(cbGot + cbJmpStubs, pDstSeg->SegInfo.Alignment);

        pDstSeg->iOrgSegNo = UINT32_MAX;
        pDstSeg->cSections = 0;
        pDstSeg->paSections = NULL;

        pThis->cbImage += pDstSeg->SegInfo.cbMapped;
    }

    return 0;
}


/**
 * @interface_method_impl{RTLDROPS,pfnClose}
 */
static DECLCALLBACK(int) rtldrMachO_Close(PRTLDRMODINTERNAL pMod)
{
    PKLDRMODMACHO pThis = RT_FROM_MEMBER(pMod, KLDRMODMACHO, Core);
    int rc = 0;
    KLDRMODMACHO_ASSERT(!pThis->pvMapping);

    uint32_t i = pThis->cSegments;
    while (i-- > 0)
    {
        uint32_t j = pThis->aSegments[i].cSections;
        while (j-- > 0)
        {
            RTMemFree(pThis->aSegments[i].paSections[j].paFixups);
            pThis->aSegments[i].paSections[j].paFixups = NULL;
        }
    }

    if (pThis->Core.pReader)
    {
        rc = pThis->Core.pReader->pfnDestroy(pThis->Core.pReader);
        pThis->Core.pReader = NULL;
    }
    pThis->Core.u32Magic = 0;
    pThis->Core.pOps = NULL;
    RTMemFree(pThis->pbLoadCommands);
    pThis->pbLoadCommands = NULL;
    RTMemFree(pThis->pchStrings);
    pThis->pchStrings = NULL;
    RTMemFree(pThis->pvaSymbols);
    pThis->pvaSymbols = NULL;
    RTMemFree(pThis);
    return rc;
}


/**
 * Gets the right base address.
 *
 * @returns 0 on success.
 * @returns A non-zero status code if the BaseAddress isn't right.
 * @param   pThis       The interpreter module instance
 * @param   pBaseAddress    The base address, IN & OUT. Optional.
 */
static int kldrModMachOAdjustBaseAddress(PKLDRMODMACHO pThis, PRTLDRADDR pBaseAddress)
{
    /*
     * Adjust the base address.
     */
    if (*pBaseAddress == RTLDR_BASEADDRESS_LINK)
        *pBaseAddress = pThis->LinkAddress;
    return 0;
}


/**
 * Resolves a linker generated symbol.
 *
 * The Apple linker generates symbols indicating the start and end of sections
 * and segments.  This function checks for these and returns the right value.
 *
 * @returns 0 or VERR_SYMBOL_NOT_FOUND.
 * @param   pThis           The interpreter module instance.
 * @param   pchSymbol           The symbol.
 * @param   cchSymbol           The length of the symbol.
 * @param   BaseAddress         The base address to apply when calculating the
 *                              value.
 * @param   puValue             Where to return the symbol value.
 */
static int kldrModMachOQueryLinkerSymbol(PKLDRMODMACHO pThis, const char *pchSymbol, size_t cchSymbol,
                                         RTLDRADDR BaseAddress, PRTLDRADDR puValue)
{
    /*
     * Match possible name prefixes.
     */
    static const struct
    {
        const char *pszPrefix;
        uint32_t    cchPrefix;
        bool        fSection;
        bool        fStart;
    }   s_aPrefixes[] =
    {
        { "section$start$",  (uint8_t)sizeof("section$start$") - 1,   true,  true },
        { "section$end$",    (uint8_t)sizeof("section$end$") - 1,     true,  false},
        { "segment$start$",  (uint8_t)sizeof("segment$start$") - 1,   false, true },
        { "segment$end$",    (uint8_t)sizeof("segment$end$") - 1,     false, false},
    };
    size_t      cchSectName = 0;
    const char *pchSectName = "";
    size_t      cchSegName  = 0;
    const char *pchSegName  = NULL;
    uint32_t    iPrefix     = RT_ELEMENTS(s_aPrefixes) - 1;
    uint32_t    iSeg;
    RTLDRADDR   uValue;

    for (;;)
    {
        uint8_t const cchPrefix = s_aPrefixes[iPrefix].cchPrefix;
        if (   cchSymbol > cchPrefix
            && strncmp(pchSymbol, s_aPrefixes[iPrefix].pszPrefix, cchPrefix) == 0)
        {
            pchSegName = pchSymbol + cchPrefix;
            cchSegName = cchSymbol - cchPrefix;
            break;
        }

        /* next */
        if (!iPrefix)
            return VERR_SYMBOL_NOT_FOUND;
        iPrefix--;
    }

    /*
     * Split the remainder into segment and section name, if necessary.
     */
    if (s_aPrefixes[iPrefix].fSection)
    {
        pchSectName = (const char *)memchr(pchSegName, '$', cchSegName);
        if (!pchSectName)
            return VERR_SYMBOL_NOT_FOUND;
        cchSegName  = pchSectName - pchSegName;
        pchSectName++;
        cchSectName = cchSymbol - (pchSectName - pchSymbol);
    }

    /*
     * Locate the segment.
     */
    if (!pThis->cSegments)
        return VERR_SYMBOL_NOT_FOUND;
    for (iSeg = 0; iSeg < pThis->cSegments; iSeg++)
    {
        if (   pThis->aSegments[iSeg].SegInfo.cchName >= cchSegName
            && memcmp(pThis->aSegments[iSeg].SegInfo.pszName, pchSegName, cchSegName) == 0)
        {
            section_32_t const *pSect;
            if (   pThis->aSegments[iSeg].SegInfo.cchName == cchSegName
                && pThis->Hdr.filetype != MH_OBJECT /* Good enough for __DWARF segs in MH_DHSYM, I hope. */)
                break;

            pSect = (section_32_t *)pThis->aSegments[iSeg].paSections[0].pvMachoSection;
            if (   pThis->uEffFileType == MH_OBJECT
                && pThis->aSegments[iSeg].SegInfo.cchName > cchSegName + 1
                && pThis->aSegments[iSeg].SegInfo.pszName[cchSegName] == '.'
                && strncmp(&pThis->aSegments[iSeg].SegInfo.pszName[cchSegName + 1], pSect->sectname, sizeof(pSect->sectname)) == 0
                && pThis->aSegments[iSeg].SegInfo.cchName - cchSegName - 1 <= sizeof(pSect->sectname) )
                break;
        }
    }
    if (iSeg >= pThis->cSegments)
        return VERR_SYMBOL_NOT_FOUND;

    if (!s_aPrefixes[iPrefix].fSection)
    {
        /*
         * Calculate the segment start/end address.
         */
        uValue = pThis->aSegments[iSeg].SegInfo.RVA;
        if (!s_aPrefixes[iPrefix].fStart)
            uValue += pThis->aSegments[iSeg].SegInfo.cb;
    }
    else
    {
        /*
         * Locate the section.
         */
        uint32_t iSect = pThis->aSegments[iSeg].cSections;
        if (!iSect)
            return VERR_SYMBOL_NOT_FOUND;
        for (;;)
        {
            section_32_t *pSect = (section_32_t *)pThis->aSegments[iSeg].paSections[iSect].pvMachoSection;
            if (   cchSectName <= sizeof(pSect->sectname)
                && memcmp(pSect->sectname, pchSectName, cchSectName) == 0
                && (   cchSectName == sizeof(pSect->sectname)
                    || pSect->sectname[cchSectName] == '\0') )
                break;
            /* next */
            if (!iSect)
                return VERR_SYMBOL_NOT_FOUND;
            iSect--;
        }

        uValue = pThis->aSegments[iSeg].paSections[iSect].RVA;
        if (!s_aPrefixes[iPrefix].fStart)
            uValue += pThis->aSegments[iSeg].paSections[iSect].cb;
    }

    /*
     * Convert from RVA to load address.
     */
    uValue += BaseAddress;
    if (puValue)
        *puValue = uValue;

    return 0;
}


/**
 * @interface_method_impl{RTLDROPS,pfnGetSymbolEx}
 */
static DECLCALLBACK(int) rtldrMachO_GetSymbolEx(PRTLDRMODINTERNAL pMod, const void *pvBits, RTUINTPTR BaseAddress,
                                                uint32_t iOrdinal, const char *pszSymbol, RTUINTPTR *pValue)
{
    PKLDRMODMACHO pThis = RT_FROM_MEMBER(pMod, KLDRMODMACHO, Core);
    RT_NOREF(pvBits);
    //RT_NOREF(pszVersion);
    //RT_NOREF(pfnGetForwarder);
    //RT_NOREF(pvUser);
    uint32_t fKind = RTLDRSYMKIND_REQ_FLAT;
    uint32_t *pfKind = &fKind;
    size_t cchSymbol = pszSymbol ? strlen(pszSymbol) : 0;

    /*
     * Resolve defaults.
     */
    int rc = kldrModMachOAdjustBaseAddress(pThis, &BaseAddress);
    if (rc)
        return rc;

    /*
     * Refuse segmented requests for now.
     */
    KLDRMODMACHO_CHECK_RETURN(   !pfKind
                              || (*pfKind & RTLDRSYMKIND_REQ_TYPE_MASK) == RTLDRSYMKIND_REQ_FLAT,
                              VERR_LDRMACHO_TODO);

    /*
     * Take action according to file type.
     */
    if (   pThis->Hdr.filetype == MH_OBJECT
        || pThis->Hdr.filetype == MH_EXECUTE /** @todo dylib, execute, dsym: symbols */
        || pThis->Hdr.filetype == MH_DYLIB
        || pThis->Hdr.filetype == MH_BUNDLE
        || pThis->Hdr.filetype == MH_DSYM
        || pThis->Hdr.filetype == MH_KEXT_BUNDLE)
    {
        rc = kldrModMachOLoadObjSymTab(pThis);
        if (!rc)
        {
            if (    pThis->Hdr.magic == IMAGE_MACHO32_SIGNATURE
                ||  pThis->Hdr.magic == IMAGE_MACHO32_SIGNATURE_OE)
                rc = kldrModMachODoQuerySymbol32Bit(pThis, (macho_nlist_32_t *)pThis->pvaSymbols, pThis->cSymbols,
                                                    pThis->pchStrings, pThis->cchStrings, BaseAddress, iOrdinal, pszSymbol,
                                                    (uint32_t)cchSymbol, pValue, pfKind);
            else
                rc = kldrModMachODoQuerySymbol64Bit(pThis, (macho_nlist_64_t *)pThis->pvaSymbols, pThis->cSymbols,
                                                    pThis->pchStrings, pThis->cchStrings, BaseAddress, iOrdinal, pszSymbol,
                                                    (uint32_t)cchSymbol, pValue, pfKind);
        }

        /*
         * Check for link-editor generated symbols and supply what we can.
         *
         * As small service to clients that insists on adding a '_' prefix
         * before querying symbols, we will ignore the prefix.
         */
        if (  rc == VERR_SYMBOL_NOT_FOUND
            && cchSymbol > sizeof("section$end$") - 1
            && (    pszSymbol[0] == 's'
                || (pszSymbol[1] == 's' && pszSymbol[0] == '_') )
            && memchr(pszSymbol, '$', cchSymbol) )
        {
            if (pszSymbol[0] == '_')
                rc = kldrModMachOQueryLinkerSymbol(pThis, pszSymbol + 1, cchSymbol - 1, BaseAddress, pValue);
            else
                rc = kldrModMachOQueryLinkerSymbol(pThis, pszSymbol, cchSymbol, BaseAddress, pValue);
        }
    }
    else
        rc = VERR_LDRMACHO_TODO;

    return rc;
}


/**
 * Lookup a symbol in a 32-bit symbol table.
 *
 * @returns See kLdrModQuerySymbol.
 * @param   pThis
 * @param   paSyms      Pointer to the symbol table.
 * @param   cSyms       Number of symbols in the table.
 * @param   pchStrings  Pointer to the string table.
 * @param   cchStrings  Size of the string table.
 * @param   BaseAddress Adjusted base address, see kLdrModQuerySymbol.
 * @param   iSymbol     See kLdrModQuerySymbol.
 * @param   pchSymbol   See kLdrModQuerySymbol.
 * @param   cchSymbol   See kLdrModQuerySymbol.
 * @param   puValue     See kLdrModQuerySymbol.
 * @param   pfKind      See kLdrModQuerySymbol.
 */
static int kldrModMachODoQuerySymbol32Bit(PKLDRMODMACHO pThis, const macho_nlist_32_t *paSyms, uint32_t cSyms,
                                          const char *pchStrings, uint32_t cchStrings, RTLDRADDR BaseAddress, uint32_t iSymbol,
                                          const char *pchSymbol, uint32_t cchSymbol, PRTLDRADDR puValue, uint32_t *pfKind)
{
    /*
     * Find a valid symbol matching the search criteria.
     */
    if (iSymbol == UINT32_MAX)
    {
        /* simplify validation. */
        if (cchStrings <= cchSymbol)
            return VERR_SYMBOL_NOT_FOUND;
        cchStrings -= cchSymbol;

        /* external symbols are usually at the end, so search the other way. */
        for (iSymbol = cSyms - 1; iSymbol != UINT32_MAX; iSymbol--)
        {
            const char *psz;

            /* Skip irrellevant and non-public symbols. */
            if (paSyms[iSymbol].n_type & MACHO_N_STAB)
                continue;
            if ((paSyms[iSymbol].n_type & MACHO_N_TYPE) == MACHO_N_UNDF)
                continue;
            if (!(paSyms[iSymbol].n_type & MACHO_N_EXT)) /*??*/
                continue;
            if (paSyms[iSymbol].n_type & MACHO_N_PEXT) /*??*/
                continue;

            /* get name */
            if (!paSyms[iSymbol].n_un.n_strx)
                continue;
            if ((uint32_t)paSyms[iSymbol].n_un.n_strx >= cchStrings)
                continue;
            psz = &pchStrings[paSyms[iSymbol].n_un.n_strx];
            if (psz[cchSymbol])
                continue;
            if (memcmp(psz, pchSymbol, cchSymbol))
                continue;

            /* match! */
            break;
        }
        if (iSymbol == UINT32_MAX)
            return VERR_SYMBOL_NOT_FOUND;
    }
    else
    {
        if (iSymbol >= cSyms)
            return VERR_SYMBOL_NOT_FOUND;
        if (paSyms[iSymbol].n_type & MACHO_N_STAB)
            return VERR_SYMBOL_NOT_FOUND;
        if ((paSyms[iSymbol].n_type & MACHO_N_TYPE) == MACHO_N_UNDF)
            return VERR_SYMBOL_NOT_FOUND;
    }

    /*
     * Calc the return values.
     */
    if (pfKind)
    {
        if (    pThis->Hdr.magic == IMAGE_MACHO32_SIGNATURE
            ||  pThis->Hdr.magic == IMAGE_MACHO32_SIGNATURE_OE)
            *pfKind = RTLDRSYMKIND_32BIT | RTLDRSYMKIND_NO_TYPE;
        else
            *pfKind = RTLDRSYMKIND_64BIT | RTLDRSYMKIND_NO_TYPE;
        if (paSyms[iSymbol].n_desc & N_WEAK_DEF)
            *pfKind |= RTLDRSYMKIND_WEAK;
    }

    switch (paSyms[iSymbol].n_type & MACHO_N_TYPE)
    {
        case MACHO_N_SECT:
        {
            PKLDRMODMACHOSECT pSect;
            RTLDRADDR offSect;
            KLDRMODMACHO_CHECK_RETURN((uint32_t)(paSyms[iSymbol].n_sect - 1) < pThis->cSections, VERR_LDRMACHO_BAD_SYMBOL);
            pSect = &pThis->paSections[paSyms[iSymbol].n_sect - 1];

            offSect = paSyms[iSymbol].n_value - pSect->LinkAddress;
            KLDRMODMACHO_CHECK_RETURN(   offSect <= pSect->cb
                                      || (   paSyms[iSymbol].n_sect == 1 /* special hack for __mh_execute_header */
                                          && offSect == 0U - pSect->RVA
                                          && pThis->uEffFileType != MH_OBJECT),
                                      VERR_LDRMACHO_BAD_SYMBOL);
            if (puValue)
                *puValue = BaseAddress + pSect->RVA + offSect;

            if (    pfKind
                &&  (pSect->fFlags & (S_ATTR_PURE_INSTRUCTIONS | S_ATTR_SELF_MODIFYING_CODE)))
                *pfKind = (*pfKind & ~RTLDRSYMKIND_TYPE_MASK) | RTLDRSYMKIND_CODE;
            break;
        }

        case MACHO_N_ABS:
            if (puValue)
                *puValue = paSyms[iSymbol].n_value;
            /*if (pfKind)
                pfKind |= RTLDRSYMKIND_ABS;*/
            break;

        case MACHO_N_PBUD:
        case MACHO_N_INDR:
            /** @todo implement indirect and prebound symbols. */
        default:
            KLDRMODMACHO_FAILED_RETURN(VERR_LDRMACHO_TODO);
    }

    return 0;
}


/**
 * Lookup a symbol in a 64-bit symbol table.
 *
 * @returns See kLdrModQuerySymbol.
 * @param   pThis
 * @param   paSyms      Pointer to the symbol table.
 * @param   cSyms       Number of symbols in the table.
 * @param   pchStrings  Pointer to the string table.
 * @param   cchStrings  Size of the string table.
 * @param   BaseAddress Adjusted base address, see kLdrModQuerySymbol.
 * @param   iSymbol     See kLdrModQuerySymbol.
 * @param   pchSymbol   See kLdrModQuerySymbol.
 * @param   cchSymbol   See kLdrModQuerySymbol.
 * @param   puValue     See kLdrModQuerySymbol.
 * @param   pfKind      See kLdrModQuerySymbol.
 */
static int kldrModMachODoQuerySymbol64Bit(PKLDRMODMACHO pThis, const macho_nlist_64_t *paSyms, uint32_t cSyms,
                                          const char *pchStrings, uint32_t cchStrings, RTLDRADDR BaseAddress, uint32_t iSymbol,
                                          const char *pchSymbol, uint32_t cchSymbol, PRTLDRADDR puValue, uint32_t *pfKind)
{
    /*
     * Find a valid symbol matching the search criteria.
     */
    if (iSymbol == UINT32_MAX)
    {
        /* simplify validation. */
        if (cchStrings <= cchSymbol)
            return VERR_SYMBOL_NOT_FOUND;
        cchStrings -= cchSymbol;

        /* external symbols are usually at the end, so search the other way. */
        for (iSymbol = cSyms - 1; iSymbol != UINT32_MAX; iSymbol--)
        {
            const char *psz;

            /* Skip irrellevant and non-public symbols. */
            if (paSyms[iSymbol].n_type & MACHO_N_STAB)
                continue;
            if ((paSyms[iSymbol].n_type & MACHO_N_TYPE) == MACHO_N_UNDF)
                continue;
            if (!(paSyms[iSymbol].n_type & MACHO_N_EXT)) /*??*/
                continue;
            if (paSyms[iSymbol].n_type & MACHO_N_PEXT) /*??*/
                continue;

            /* get name */
            if (!paSyms[iSymbol].n_un.n_strx)
                continue;
            if ((uint32_t)paSyms[iSymbol].n_un.n_strx >= cchStrings)
                continue;
            psz = &pchStrings[paSyms[iSymbol].n_un.n_strx];
            if (psz[cchSymbol])
                continue;
            if (memcmp(psz, pchSymbol, cchSymbol))
                continue;

            /* match! */
            break;
        }
        if (iSymbol == UINT32_MAX)
            return VERR_SYMBOL_NOT_FOUND;
    }
    else
    {
        if (iSymbol >= cSyms)
            return VERR_SYMBOL_NOT_FOUND;
        if (paSyms[iSymbol].n_type & MACHO_N_STAB)
            return VERR_SYMBOL_NOT_FOUND;
        if ((paSyms[iSymbol].n_type & MACHO_N_TYPE) == MACHO_N_UNDF)
            return VERR_SYMBOL_NOT_FOUND;
    }

    /*
     * Calc the return values.
     */
    if (pfKind)
    {
        if (    pThis->Hdr.magic == IMAGE_MACHO32_SIGNATURE
            ||  pThis->Hdr.magic == IMAGE_MACHO32_SIGNATURE_OE)
            *pfKind = RTLDRSYMKIND_32BIT | RTLDRSYMKIND_NO_TYPE;
        else
            *pfKind = RTLDRSYMKIND_64BIT | RTLDRSYMKIND_NO_TYPE;
        if (paSyms[iSymbol].n_desc & N_WEAK_DEF)
            *pfKind |= RTLDRSYMKIND_WEAK;
    }

    switch (paSyms[iSymbol].n_type & MACHO_N_TYPE)
    {
        case MACHO_N_SECT:
        {
            PKLDRMODMACHOSECT pSect;
            RTLDRADDR offSect;
            KLDRMODMACHO_CHECK_RETURN((uint32_t)(paSyms[iSymbol].n_sect - 1) < pThis->cSections, VERR_LDRMACHO_BAD_SYMBOL);
            pSect = &pThis->paSections[paSyms[iSymbol].n_sect - 1];

            offSect = paSyms[iSymbol].n_value - pSect->LinkAddress;
            KLDRMODMACHO_CHECK_RETURN(   offSect <= pSect->cb
                                      || (   paSyms[iSymbol].n_sect == 1 /* special hack for __mh_execute_header */
                                          && offSect == 0U - pSect->RVA
                                          && pThis->uEffFileType != MH_OBJECT),
                                      VERR_LDRMACHO_BAD_SYMBOL);
            if (puValue)
                *puValue = BaseAddress + pSect->RVA + offSect;

            if (    pfKind
                &&  (pSect->fFlags & (S_ATTR_PURE_INSTRUCTIONS | S_ATTR_SELF_MODIFYING_CODE)))
                *pfKind = (*pfKind & ~RTLDRSYMKIND_TYPE_MASK) | RTLDRSYMKIND_CODE;
            break;
        }

        case MACHO_N_ABS:
            if (puValue)
                *puValue = paSyms[iSymbol].n_value;
            /*if (pfKind)
                pfKind |= RTLDRSYMKIND_ABS;*/
            break;

        case MACHO_N_PBUD:
        case MACHO_N_INDR:
            /** @todo implement indirect and prebound symbols. */
        default:
            KLDRMODMACHO_FAILED_RETURN(VERR_LDRMACHO_TODO);
    }

    return 0;
}


/**
 * @interface_method_impl{RTLDROPS,pfnEnumSymbols}
 */
static DECLCALLBACK(int) rtldrMachO_EnumSymbols(PRTLDRMODINTERNAL pMod, unsigned fFlags, const void *pvBits,
                                                RTUINTPTR BaseAddress, PFNRTLDRENUMSYMS pfnCallback, void *pvUser)
{
    PKLDRMODMACHO pThis = RT_FROM_MEMBER(pMod, KLDRMODMACHO, Core);
    int rc;
    RT_NOREF(pvBits);

    /*
     * Resolve defaults.
     */
    rc  = kldrModMachOAdjustBaseAddress(pThis, &BaseAddress);
    if (rc)
        return rc;

    /*
     * Take action according to file type.
     */
    if (   pThis->Hdr.filetype == MH_OBJECT
        || pThis->Hdr.filetype == MH_EXECUTE /** @todo dylib, execute, dsym: symbols */
        || pThis->Hdr.filetype == MH_DYLIB
        || pThis->Hdr.filetype == MH_BUNDLE
        || pThis->Hdr.filetype == MH_DSYM
        || pThis->Hdr.filetype == MH_KEXT_BUNDLE)
    {
        rc = kldrModMachOLoadObjSymTab(pThis);
        if (!rc)
        {
            if (    pThis->Hdr.magic == IMAGE_MACHO32_SIGNATURE
                ||  pThis->Hdr.magic == IMAGE_MACHO32_SIGNATURE_OE)
                rc = kldrModMachODoEnumSymbols32Bit(pThis, (macho_nlist_32_t *)pThis->pvaSymbols, pThis->cSymbols,
                                                    pThis->pchStrings, pThis->cchStrings, BaseAddress,
                                                    fFlags, pfnCallback, pvUser);
            else
                rc = kldrModMachODoEnumSymbols64Bit(pThis, (macho_nlist_64_t *)pThis->pvaSymbols, pThis->cSymbols,
                                                    pThis->pchStrings, pThis->cchStrings, BaseAddress,
                                                    fFlags, pfnCallback, pvUser);
        }
    }
    else
        KLDRMODMACHO_FAILED_RETURN(VERR_LDRMACHO_TODO);

    return rc;
}


/**
 * Enum a 32-bit symbol table.
 *
 * @returns See kLdrModQuerySymbol.
 * @param   pThis
 * @param   paSyms      Pointer to the symbol table.
 * @param   cSyms       Number of symbols in the table.
 * @param   pchStrings  Pointer to the string table.
 * @param   cchStrings  Size of the string table.
 * @param   BaseAddress Adjusted base address, see kLdrModEnumSymbols.
 * @param   fFlags      See kLdrModEnumSymbols.
 * @param   pfnCallback See kLdrModEnumSymbols.
 * @param   pvUser      See kLdrModEnumSymbols.
 */
static int kldrModMachODoEnumSymbols32Bit(PKLDRMODMACHO pThis, const macho_nlist_32_t *paSyms, uint32_t cSyms,
                                          const char *pchStrings, uint32_t cchStrings, RTLDRADDR BaseAddress,
                                          uint32_t fFlags, PFNRTLDRENUMSYMS pfnCallback, void *pvUser)
{
    const uint32_t fKindBase =    pThis->Hdr.magic == IMAGE_MACHO32_SIGNATURE
                           || pThis->Hdr.magic == IMAGE_MACHO32_SIGNATURE_OE
                         ? RTLDRSYMKIND_32BIT : RTLDRSYMKIND_64BIT;
    uint32_t iSym;
    int rc;

    /*
     * Iterate the symbol table.
     */
    for (iSym = 0; iSym < cSyms; iSym++)
    {
        uint32_t fKind;
        RTLDRADDR uValue;
        const char *psz;
        size_t cch;

        /* Skip debug symbols and undefined symbols. */
        if (paSyms[iSym].n_type & MACHO_N_STAB)
            continue;
        if ((paSyms[iSym].n_type & MACHO_N_TYPE) == MACHO_N_UNDF)
            continue;

        /* Skip non-public symbols unless they are requested explicitly. */
        if (!(fFlags & RTLDR_ENUM_SYMBOL_FLAGS_ALL))
        {
            if (!(paSyms[iSym].n_type & MACHO_N_EXT)) /*??*/
                continue;
            if (paSyms[iSym].n_type & MACHO_N_PEXT) /*??*/
                continue;
            if (!paSyms[iSym].n_un.n_strx)
                continue;
        }

        /*
         * Gather symbol info
         */

        /* name */
        KLDRMODMACHO_CHECK_RETURN((uint32_t)paSyms[iSym].n_un.n_strx < cchStrings, VERR_LDRMACHO_BAD_SYMBOL);
        psz = &pchStrings[paSyms[iSym].n_un.n_strx];
        cch = strlen(psz);
        if (!cch)
            psz = NULL;

        /* kind & value */
        fKind = fKindBase;
        if (paSyms[iSym].n_desc & N_WEAK_DEF)
            fKind |= RTLDRSYMKIND_WEAK;
        switch (paSyms[iSym].n_type & MACHO_N_TYPE)
        {
            case MACHO_N_SECT:
            {
                PKLDRMODMACHOSECT pSect;
                KLDRMODMACHO_CHECK_RETURN((uint32_t)(paSyms[iSym].n_sect - 1) < pThis->cSections, VERR_LDRMACHO_BAD_SYMBOL);
                pSect = &pThis->paSections[paSyms[iSym].n_sect - 1];

                uValue = paSyms[iSym].n_value - pSect->LinkAddress;
                KLDRMODMACHO_CHECK_RETURN(   uValue <= pSect->cb
                                          || (   paSyms[iSym].n_sect == 1 /* special hack for __mh_execute_header */
                                              && uValue == 0U - pSect->RVA
                                              && pThis->uEffFileType != MH_OBJECT),
                                          VERR_LDRMACHO_BAD_SYMBOL);
                uValue += BaseAddress + pSect->RVA;

                if (pSect->fFlags & (S_ATTR_PURE_INSTRUCTIONS | S_ATTR_SELF_MODIFYING_CODE))
                    fKind |= RTLDRSYMKIND_CODE;
                else
                    fKind |= RTLDRSYMKIND_NO_TYPE;
                break;
            }

            case MACHO_N_ABS:
                uValue = paSyms[iSym].n_value;
                fKind |= RTLDRSYMKIND_NO_TYPE /*RTLDRSYMKIND_ABS*/;
                break;

            case MACHO_N_PBUD:
            case MACHO_N_INDR:
                /** @todo implement indirect and prebound symbols. */
            default:
                KLDRMODMACHO_FAILED_RETURN(VERR_LDRMACHO_TODO);
        }

        /*
         * Do callback.
         */
        rc = pfnCallback(&pThis->Core, psz, iSym, uValue/*, fKind*/, pvUser);
        if (rc)
            return rc;
    }
    return 0;
}


/**
 * Enum a 64-bit symbol table.
 *
 * @returns See kLdrModQuerySymbol.
 * @param   pThis
 * @param   paSyms      Pointer to the symbol table.
 * @param   cSyms       Number of symbols in the table.
 * @param   pchStrings  Pointer to the string table.
 * @param   cchStrings  Size of the string table.
 * @param   BaseAddress Adjusted base address, see kLdrModEnumSymbols.
 * @param   fFlags      See kLdrModEnumSymbols.
 * @param   pfnCallback See kLdrModEnumSymbols.
 * @param   pvUser      See kLdrModEnumSymbols.
 */
static int kldrModMachODoEnumSymbols64Bit(PKLDRMODMACHO pThis, const macho_nlist_64_t *paSyms, uint32_t cSyms,
                                          const char *pchStrings, uint32_t cchStrings, RTLDRADDR BaseAddress,
                                          uint32_t fFlags, PFNRTLDRENUMSYMS pfnCallback, void *pvUser)
{
    const uint32_t fKindBase =    pThis->Hdr.magic == IMAGE_MACHO64_SIGNATURE
                           || pThis->Hdr.magic == IMAGE_MACHO64_SIGNATURE_OE
                         ? RTLDRSYMKIND_64BIT : RTLDRSYMKIND_32BIT;
    uint32_t iSym;
    int rc;

    /*
     * Iterate the symbol table.
     */
    for (iSym = 0; iSym < cSyms; iSym++)
    {
        uint32_t fKind;
        RTLDRADDR uValue;
        const char *psz;
        size_t cch;

        /* Skip debug symbols and undefined symbols. */
        if (paSyms[iSym].n_type & MACHO_N_STAB)
            continue;
        if ((paSyms[iSym].n_type & MACHO_N_TYPE) == MACHO_N_UNDF)
            continue;

        /* Skip non-public symbols unless they are requested explicitly. */
        if (!(fFlags & RTLDR_ENUM_SYMBOL_FLAGS_ALL))
        {
            if (!(paSyms[iSym].n_type & MACHO_N_EXT)) /*??*/
                continue;
            if (paSyms[iSym].n_type & MACHO_N_PEXT) /*??*/
                continue;
            if (!paSyms[iSym].n_un.n_strx)
                continue;
        }

        /*
         * Gather symbol info
         */

        /* name */
        KLDRMODMACHO_CHECK_RETURN((uint32_t)paSyms[iSym].n_un.n_strx < cchStrings, VERR_LDRMACHO_BAD_SYMBOL);
        psz = &pchStrings[paSyms[iSym].n_un.n_strx];
        cch = strlen(psz);
        if (!cch)
            psz = NULL;

        /* kind & value */
        fKind = fKindBase;
        if (paSyms[iSym].n_desc & N_WEAK_DEF)
            fKind |= RTLDRSYMKIND_WEAK;
        switch (paSyms[iSym].n_type & MACHO_N_TYPE)
        {
            case MACHO_N_SECT:
            {
                PKLDRMODMACHOSECT pSect;
                KLDRMODMACHO_CHECK_RETURN((uint32_t)(paSyms[iSym].n_sect - 1) < pThis->cSections, VERR_LDRMACHO_BAD_SYMBOL);
                pSect = &pThis->paSections[paSyms[iSym].n_sect - 1];

                uValue = paSyms[iSym].n_value - pSect->LinkAddress;
                KLDRMODMACHO_CHECK_RETURN(   uValue <= pSect->cb
                                          || (   paSyms[iSym].n_sect == 1 /* special hack for __mh_execute_header */
                                              && uValue == 0U - pSect->RVA
                                              && pThis->uEffFileType != MH_OBJECT),
                                          VERR_LDRMACHO_BAD_SYMBOL);
                uValue += BaseAddress + pSect->RVA;

                if (pSect->fFlags & (S_ATTR_PURE_INSTRUCTIONS | S_ATTR_SELF_MODIFYING_CODE))
                    fKind |= RTLDRSYMKIND_CODE;
                else
                    fKind |= RTLDRSYMKIND_NO_TYPE;
                break;
            }

            case MACHO_N_ABS:
                uValue = paSyms[iSym].n_value;
                fKind |= RTLDRSYMKIND_NO_TYPE /*RTLDRSYMKIND_ABS*/;
                break;

            case MACHO_N_PBUD:
            case MACHO_N_INDR:
                /** @todo implement indirect and prebound symbols. */
            default:
                KLDRMODMACHO_FAILED_RETURN(VERR_LDRMACHO_TODO);
        }

        /*
         * Do callback.
         */
        rc = pfnCallback(&pThis->Core, psz, iSym, uValue/*, fKind*/, pvUser);
        if (rc)
            return rc;
    }
    return 0;
}

#if 0

/** @copydoc kLdrModGetImport */
static int kldrModMachOGetImport(PRTLDRMODINTERNAL pMod, const void *pvBits, uint32_t iImport, char *pszName, size_t cchName)
{
    PKLDRMODMACHO pThis = RT_FROM_MEMBER(pMod, KLDRMODMACHO, Core);
    RT_NOREF(pvBits);
    RT_NOREF(iImport);
    RT_NOREF(pszName);
    RT_NOREF(cchName);

    if (pThis->Hdr.filetype == MH_OBJECT)
        return KLDR_ERR_IMPORT_ORDINAL_OUT_OF_BOUNDS;

    /* later */
    return KLDR_ERR_IMPORT_ORDINAL_OUT_OF_BOUNDS;
}



/** @copydoc kLdrModNumberOfImports */
static int32_t kldrModMachONumberOfImports(PRTLDRMODINTERNAL pMod, const void *pvBits)
{
    PKLDRMODMACHO pThis = RT_FROM_MEMBER(pMod, KLDRMODMACHO, Core);
    RT_NOREF(pvBits);

    if (pThis->Hdr.filetype == MH_OBJECT)
        return 0;

    /* later */
    return 0;
}


/** @copydoc kLdrModGetStackInfo */
static int kldrModMachOGetStackInfo(PRTLDRMODINTERNAL pMod, const void *pvBits, RTLDRADDR BaseAddress, PKLDRSTACKINFO pStackInfo)
{
    /*PKLDRMODMACHO pThis = RT_FROM_MEMBER(pMod, KLDRMODMACHO, Core);*/
    RT_NOREF(pMod);
    RT_NOREF(pvBits);
    RT_NOREF(BaseAddress);

    pStackInfo->Address = NIL_RTLDRADDR;
    pStackInfo->LinkAddress = NIL_RTLDRADDR;
    pStackInfo->cbStack = pStackInfo->cbStackThread = 0;
    /* later */

    return 0;
}

#endif


/** @copydoc kLdrModQueryMainEntrypoint */
static int kldrModMachOQueryMainEntrypoint(PRTLDRMODINTERNAL pMod, const void *pvBits, RTLDRADDR BaseAddress, PRTLDRADDR pMainEPAddress)
{
#if 0
    PKLDRMODMACHO pThis = RT_FROM_MEMBER(pMod, KLDRMODMACHO, Core);
    int rc;

    /*
     * Resolve base address alias if any.
     */
    rc = kldrModMachOBitsAndBaseAddress(pThis, NULL, &BaseAddress);
    if (rc)
        return rc;

    /*
     * Convert the address from the header.
     */
    *pMainEPAddress = pThis->Hdrs.OptionalHeader.AddressOfEntryPoint
        ? BaseAddress + pThis->Hdrs.OptionalHeader.AddressOfEntryPoint
        : NIL_RTLDRADDR;
#else
    *pMainEPAddress = NIL_RTLDRADDR;
    RT_NOREF(pvBits);
    RT_NOREF(BaseAddress);
    RT_NOREF(pMod);
#endif
    return 0;
}


/** @copydoc kLdrModQueryImageUuid */
static int kldrModMachOQueryImageUuid(PKLDRMODMACHO pThis, const void *pvBits, void *pvUuid, size_t cbUuid)
{
    RT_NOREF(pvBits);

    memset(pvUuid, 0, cbUuid);
    if (memcmp(pvUuid, pThis->abImageUuid, sizeof(pThis->abImageUuid)) == 0)
        return VERR_NOT_FOUND;

    memcpy(pvUuid, pThis->abImageUuid, sizeof(pThis->abImageUuid));
    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{RTLDROPS,pfnEnumDbgInfo}
 */
static DECLCALLBACK(int) rtldrMachO_EnumDbgInfo(PRTLDRMODINTERNAL pMod, const void *pvBits, PFNRTLDRENUMDBG pfnCallback, void *pvUser)
{
    PKLDRMODMACHO pThis = RT_FROM_MEMBER(pMod, KLDRMODMACHO, Core);
    int rc = VINF_SUCCESS;
    uint32_t iSect;
    RT_NOREF(pvBits);

    for (iSect = 0; iSect < pThis->cSections; iSect++)
    {
        /* (32-bit & 64-bit starts the same way) */
        section_32_t *pMachOSect = (section_32_t *)pThis->paSections[iSect].pvMachoSection;
        char          szTmp[sizeof(pMachOSect->sectname) + 1];

        if (strcmp(pMachOSect->segname, "__DWARF"))
            continue;

        memcpy(szTmp, pMachOSect->sectname, sizeof(pMachOSect->sectname));
        szTmp[sizeof(pMachOSect->sectname)] = '\0';

        RTLDRDBGINFO DbgInfo;
        DbgInfo.enmType            = RTLDRDBGINFOTYPE_DWARF;
        DbgInfo.iDbgInfo           = iSect;
        DbgInfo.LinkAddress        = pThis->paSections[iSect].LinkAddress;
        DbgInfo.cb                 = pThis->paSections[iSect].cb;
        DbgInfo.pszExtFile         = NULL;
        DbgInfo.u.Dwarf.pszSection = szTmp;
        rc = pfnCallback(&pThis->Core, &DbgInfo, pvUser);
        if (rc != VINF_SUCCESS)
            break;
    }

    return rc;
}

#if 0

/** @copydoc kLdrModHasDbgInfo */
static int kldrModMachOHasDbgInfo(PRTLDRMODINTERNAL pMod, const void *pvBits)
{
    /*PKLDRMODMACHO pThis = RT_FROM_MEMBER(pMod, KLDRMODMACHO, Core);*/

#if 0
    /*
     * Base this entirely on the presence of a debug directory.
     */
    if (    pThis->Hdrs.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_DEBUG].Size
            < sizeof(IMAGE_DEBUG_DIRECTORY) /* screw borland linkers */
        ||  !pThis->Hdrs.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_DEBUG].VirtualAddress)
        return KLDR_ERR_NO_DEBUG_INFO;
    return 0;
#else
    RT_NOREF(pMod);
    RT_NOREF(pvBits);
    return VERR_LDR_NO_DEBUG_INFO;
#endif
}


/** @copydoc kLdrModMap */
static int kldrModMachOMap(PRTLDRMODINTERNAL pMod)
{
    PKLDRMODMACHO pThis = RT_FROM_MEMBER(pMod, KLDRMODMACHO, Core);
    unsigned fFixed;
    uint32_t i;
    void *pvBase;
    int rc;

    if (!pThis->fCanLoad)
        return VERR_LDRMACHO_TODO;

    /*
     * Already mapped?
     */
    if (pThis->pvMapping)
        return KLDR_ERR_ALREADY_MAPPED;

    /*
     * Map it.
     */
    /* fixed image? */
    fFixed = pMod->enmType == KLDRTYPE_EXECUTABLE_FIXED
          || pMod->enmType == KLDRTYPE_SHARED_LIBRARY_FIXED;
    if (!fFixed)
        pvBase = NULL;
    else
    {
        pvBase = (void *)(uintptr_t)pMod->aSegments[0].LinkAddress;
        if ((uintptr_t)pvBase != pMod->aSegments[0].LinkAddress)
            return VERR_LDR_ADDRESS_OVERFLOW;
    }

    /* try do the prepare */
    rc = kRdrMap(pMod->pRdr, &pvBase, pMod->cSegments, pMod->aSegments, fFixed);
    if (rc)
        return rc;

    /*
     * Update the segments with their map addresses.
     */
    for (i = 0; i < pMod->cSegments; i++)
    {
        if (pMod->aSegments[i].RVA != NIL_RTLDRADDR)
            pMod->aSegments[i].MapAddress = (uintptr_t)pvBase + (uintptr_t)pMod->aSegments[i].RVA;
    }
    pThis->pvMapping = pvBase;

    return 0;
}


/** @copydoc kLdrModUnmap */
static int kldrModMachOUnmap(PRTLDRMODINTERNAL pMod)
{
    PKLDRMODMACHO pThis = RT_FROM_MEMBER(pMod, KLDRMODMACHO, Core);
    uint32_t i;
    int rc;

    /*
     * Mapped?
     */
    if (!pThis->pvMapping)
        return KLDR_ERR_NOT_MAPPED;

    /*
     * Try unmap the image.
     */
    rc = kRdrUnmap(pMod->pRdr, pThis->pvMapping, pMod->cSegments, pMod->aSegments);
    if (rc)
        return rc;

    /*
     * Update the segments to reflect that they aren't mapped any longer.
     */
    pThis->pvMapping = NULL;
    for (i = 0; i < pMod->cSegments; i++)
        pMod->aSegments[i].MapAddress = 0;

    return 0;
}


/** @copydoc kLdrModAllocTLS */
static int kldrModMachOAllocTLS(PRTLDRMODINTERNAL pMod, void *pvMapping)
{
    PKLDRMODMACHO pThis = RT_FROM_MEMBER(pMod, KLDRMODMACHO, Core);

    /*
     * Mapped?
     */
    if (   pvMapping == KLDRMOD_INT_MAP
        && !pThis->pvMapping )
        return KLDR_ERR_NOT_MAPPED;
    return 0;
}


/** @copydoc kLdrModFreeTLS */
static void kldrModMachOFreeTLS(PRTLDRMODINTERNAL pMod, void *pvMapping)
{
    RT_NOREF(pMod);
    RT_NOREF(pvMapping);
}



/** @copydoc kLdrModReload */
static int kldrModMachOReload(PRTLDRMODINTERNAL pMod)
{
    PKLDRMODMACHO pThis = RT_FROM_MEMBER(pMod, KLDRMODMACHO, Core);

    /*
     * Mapped?
     */
    if (!pThis->pvMapping)
        return KLDR_ERR_NOT_MAPPED;

    /* the file provider does it all */
    return kRdrRefresh(pMod->pRdr, pThis->pvMapping, pMod->cSegments, pMod->aSegments);
}


/** @copydoc kLdrModFixupMapping */
static int kldrModMachOFixupMapping(PRTLDRMODINTERNAL pMod, PFNRTLDRIMPORT pfnGetImport, void *pvUser)
{
    PKLDRMODMACHO pThis = RT_FROM_MEMBER(pMod, KLDRMODMACHO, Core);
    int rc, rc2;

    /*
     * Mapped?
     */
    if (!pThis->pvMapping)
        return KLDR_ERR_NOT_MAPPED;

    /*
     * Before doing anything we'll have to make all pages writable.
     */
    rc = kRdrProtect(pMod->pRdr, pThis->pvMapping, pMod->cSegments, pMod->aSegments, 1 /* unprotect */);
    if (rc)
        return rc;

    /*
     * Resolve imports and apply base relocations.
     */
    rc = rtldrMachO_RelocateBits(pMod, pThis->pvMapping, (uintptr_t)pThis->pvMapping, pThis->LinkAddress,
                                      pfnGetImport, pvUser);

    /*
     * Restore protection.
     */
    rc2 = kRdrProtect(pMod->pRdr, pThis->pvMapping, pMod->cSegments, pMod->aSegments, 0 /* protect */);
    if (!rc && rc2)
        rc = rc2;
    return rc;
}

#endif

/**
 * MH_OBJECT: Resolves undefined symbols (imports).
 *
 * @returns 0 on success, non-zero kLdr status code on failure.
 * @param   pThis       The Mach-O module interpreter instance.
 * @param   pfnGetImport    The callback for resolving an imported symbol.
 * @param   pvUser          User argument to the callback.
 */
static int  kldrModMachOObjDoImports(PKLDRMODMACHO pThis, RTLDRADDR BaseAddress, PFNRTLDRIMPORT pfnGetImport, void *pvUser)
{
    const uint32_t cSyms = pThis->cSymbols;
    uint32_t iSym;
    int rc;

    /*
     * Ensure that we've got the symbol table and section fixups handy.
     */
    rc = kldrModMachOLoadObjSymTab(pThis);
    if (rc)
        return rc;

    /*
     * Iterate the symbol table and resolve undefined symbols.
     * We currently ignore REFERENCE_TYPE.
     */
    if (    pThis->Hdr.magic == IMAGE_MACHO32_SIGNATURE
        ||  pThis->Hdr.magic == IMAGE_MACHO32_SIGNATURE_OE)
    {
        macho_nlist_32_t *paSyms = (macho_nlist_32_t *)pThis->pvaSymbols;
        for (iSym = 0; iSym < cSyms; iSym++)
        {
            /* skip stabs */
            if (paSyms[iSym].n_type & MACHO_N_STAB)
                continue;

            if ((paSyms[iSym].n_type & MACHO_N_TYPE) == MACHO_N_UNDF)
            {
                const char *pszSymbol;
                size_t cchSymbol;
                //uint32_t fKind = RTLDRSYMKIND_REQ_FLAT;
                RTLDRADDR Value = NIL_RTLDRADDR;

                /** @todo Implement N_REF_TO_WEAK. */
                KLDRMODMACHO_CHECK_RETURN(!(paSyms[iSym].n_desc & N_REF_TO_WEAK), VERR_LDRMACHO_TODO);

                /* Get the symbol name. */
                KLDRMODMACHO_CHECK_RETURN((uint32_t)paSyms[iSym].n_un.n_strx < pThis->cchStrings, VERR_LDRMACHO_BAD_SYMBOL);
                pszSymbol = &pThis->pchStrings[paSyms[iSym].n_un.n_strx];
                cchSymbol = strlen(pszSymbol);

                /* Check for linker defined symbols relating to sections and segments. */
                if (   cchSymbol > sizeof("section$end$") - 1
                    && *pszSymbol == 's'
                    && memchr(pszSymbol, '$', cchSymbol))
                    rc = kldrModMachOQueryLinkerSymbol(pThis, pszSymbol, cchSymbol, BaseAddress, &Value);
                else
                    rc = VERR_SYMBOL_NOT_FOUND;

                /* Ask the user for an address to the symbol. */
                if (rc)
                    rc = pfnGetImport(&pThis->Core, NULL /*pszModule*/, pszSymbol, iSym, &Value/*, &fKind*/, pvUser);
                if (rc)
                {
                    /* weak reference? */
                    if (!(paSyms[iSym].n_desc & N_WEAK_REF))
                        break;
                    Value = 0;
                }

                /* Update the symbol. */
                paSyms[iSym].n_value = (uint32_t)Value;
                if (paSyms[iSym].n_value != Value)
                {
                    rc = VERR_LDR_ADDRESS_OVERFLOW;
                    break;
                }
            }
            else if (paSyms[iSym].n_desc & N_WEAK_DEF)
            {
                /** @todo implement weak symbols. */
                /*return VERR_LDRMACHO_TODO; - ignored for now. */
            }
        }
    }
    else
    {
        /* (Identical to the 32-bit code, just different paSym type. (and n_strx is unsigned)) */
        macho_nlist_64_t *paSyms = (macho_nlist_64_t *)pThis->pvaSymbols;
        for (iSym = 0; iSym < cSyms; iSym++)
        {
            /* skip stabs */
            if (paSyms[iSym].n_type & MACHO_N_STAB)
                continue;

            if ((paSyms[iSym].n_type & MACHO_N_TYPE) == MACHO_N_UNDF)
            {
                const char *pszSymbol;
                size_t cchSymbol;
                //uint32_t fKind = RTLDRSYMKIND_REQ_FLAT;
                RTLDRADDR Value = NIL_RTLDRADDR;

                /** @todo Implement N_REF_TO_WEAK. */
                KLDRMODMACHO_CHECK_RETURN(!(paSyms[iSym].n_desc & N_REF_TO_WEAK), VERR_LDRMACHO_TODO);

                 /* Get the symbol name. */
                KLDRMODMACHO_CHECK_RETURN(paSyms[iSym].n_un.n_strx < pThis->cchStrings, VERR_LDRMACHO_BAD_SYMBOL);
                pszSymbol = &pThis->pchStrings[paSyms[iSym].n_un.n_strx];
                cchSymbol = strlen(pszSymbol);

                /* Check for linker defined symbols relating to sections and segments. */
                if (   cchSymbol > sizeof("section$end$") - 1
                    && *pszSymbol == 's'
                    && memchr(pszSymbol, '$', cchSymbol))
                    rc = kldrModMachOQueryLinkerSymbol(pThis, pszSymbol, cchSymbol, BaseAddress, &Value);
                else
                    rc = VERR_SYMBOL_NOT_FOUND;

                /* Ask the user for an address to the symbol. */
                if (rc)
                    rc = pfnGetImport(&pThis->Core, NULL, pszSymbol, iSym, &Value, /*&fKind,*/ pvUser);
                if (rc)
                {
                    /* weak reference? */
                    if (!(paSyms[iSym].n_desc & N_WEAK_REF))
                        break;
                    Value = 0;
                }

                /* Update the symbol. */
                paSyms[iSym].n_value = Value;
                if (paSyms[iSym].n_value != Value)
                {
                    rc = VERR_LDR_ADDRESS_OVERFLOW;
                    break;
                }
            }
            else if (paSyms[iSym].n_desc & N_WEAK_DEF)
            {
                /** @todo implement weak symbols. */
                /*return VERR_LDRMACHO_TODO; - ignored for now. */
            }
        }
    }

    return rc;
}


/**
 * MH_OBJECT: Applies base relocations to a (unprotected) image mapping.
 *
 * @returns 0 on success, non-zero kLdr status code on failure.
 * @param   pThis       The Mach-O module interpreter instance.
 * @param   pvMapping       The mapping to fixup.
 * @param   NewBaseAddress  The address to fixup the mapping to.
 * @param   OldBaseAddress  The address the mapping is currently fixed up to.
 */
static int  kldrModMachOObjDoFixups(PKLDRMODMACHO pThis, void *pvMapping, RTLDRADDR NewBaseAddress)
{
    uint32_t iSeg;
    int rc;


    /*
     * Ensure that we've got the symbol table and section fixups handy.
     */
    rc = kldrModMachOLoadObjSymTab(pThis);
    if (rc)
        return rc;

    /*
     * Iterate over the segments and their sections and apply fixups.
     */
    for (iSeg = rc = 0; !rc && iSeg < pThis->cSegments; iSeg++)
    {
        PKLDRMODMACHOSEG pSeg = &pThis->aSegments[iSeg];
        uint32_t iSect;

        for (iSect = 0; iSect < pSeg->cSections; iSect++)
        {
            PKLDRMODMACHOSECT pSect = &pSeg->paSections[iSect];
            uint8_t *pbSectBits;

            /* skip sections without fixups. */
            if (!pSect->cFixups)
                continue;

            /* lazy load (and endian convert) the fixups. */
            if (!pSect->paFixups)
            {
                rc = kldrModMachOLoadFixups(pThis, pSect->offFixups, pSect->cFixups, &pSect->paFixups);
                if (rc)
                    break;
            }

            /*
             * Apply the fixups.
             */
            pbSectBits = (uint8_t *)pvMapping + (uintptr_t)pSect->RVA;
            if (pThis->Hdr.magic == IMAGE_MACHO32_SIGNATURE) /** @todo this aint right. */
                rc = kldrModMachOFixupSectionGeneric32Bit(pThis, pbSectBits, pSect,
                                                          (macho_nlist_32_t *)pThis->pvaSymbols,
                                                          pThis->cSymbols, NewBaseAddress);
            else if (   pThis->Hdr.magic == IMAGE_MACHO64_SIGNATURE
                     && pThis->Hdr.cputype == CPU_TYPE_X86_64)
                rc = kldrModMachOFixupSectionAMD64(pThis, pbSectBits, pSect,
                                                   (macho_nlist_64_t *)pThis->pvaSymbols,
                                                   pThis->cSymbols, NewBaseAddress);
            else
                KLDRMODMACHO_FAILED_RETURN(VERR_LDRMACHO_TODO);
            if (rc)
                break;
        }
    }

    return rc;
}


/**
 * Applies generic fixups to a section in an image of the same endian-ness
 * as the host CPU.
 *
 * @returns 0 on success, non-zero kLdr status code on failure.
 * @param   pThis       The Mach-O module interpreter instance.
 * @param   pbSectBits      Pointer to the section bits.
 * @param   pFixupSect      The section being fixed up.
 * @param   NewBaseAddress  The new base image address.
 */
static int  kldrModMachOFixupSectionGeneric32Bit(PKLDRMODMACHO pThis, uint8_t *pbSectBits, PKLDRMODMACHOSECT pFixupSect,
                                                 macho_nlist_32_t *paSyms, uint32_t cSyms, RTLDRADDR NewBaseAddress)
{
    const macho_relocation_info_t *paFixups = pFixupSect->paFixups;
    const uint32_t cFixups = pFixupSect->cFixups;
    size_t cbSectBits = (size_t)pFixupSect->cb;
    const uint8_t *pbSectVirginBits;
    uint32_t iFixup;
    RTLDRADDR SymAddr = ~(RTLDRADDR)0;
    int rc;

    /*
     * Find the virgin bits.
     */
    if (pFixupSect->offFile != -1)
    {
        rc = kldrModMachOMapVirginBits(pThis);
        if (rc)
            return rc;
        pbSectVirginBits = (const uint8_t *)pThis->pvBits + pFixupSect->offFile;
    }
    else
        pbSectVirginBits = NULL;

    /*
     * Iterate the fixups and apply them.
     */
    for (iFixup = 0; iFixup < cFixups; iFixup++)
    {
        RTPTRUNION uFix;
        RTPTRUNION uFixVirgin;
        union
        {
            macho_relocation_info_t     r;
            scattered_relocation_info_t s;
        } Fixup;
        Fixup.r = paFixups[iFixup];

        if (!(Fixup.r.r_address & R_SCATTERED))
        {
            /* sanity */
            if ((uint32_t)Fixup.r.r_address >= cbSectBits)
                return VERR_LDR_BAD_FIXUP;

            /* calc fixup addresses. */
            uFix.pv = pbSectBits + Fixup.r.r_address;
            uFixVirgin.pv = pbSectVirginBits ? (uint8_t *)pbSectVirginBits + Fixup.r.r_address : 0;

            /*
             * Calc the symbol value.
             */
            /* Calc the linked symbol address / addend. */
            switch (Fixup.r.r_length)
            {
                /** @todo Deal with unaligned accesses on non x86 platforms. */
                case 0: SymAddr = *uFixVirgin.pi8; break;
                case 1: SymAddr = *uFixVirgin.pi16; break;
                case 2: SymAddr = *uFixVirgin.pi32; break;
                case 3: SymAddr = *uFixVirgin.pi64; break;
            }
            if (Fixup.r.r_pcrel)
                SymAddr += Fixup.r.r_address + pFixupSect->LinkAddress;

            /* Add symbol / section address. */
            if (Fixup.r.r_extern)
            {
                const macho_nlist_32_t *pSym;
                if (Fixup.r.r_symbolnum >= cSyms)
                    return VERR_LDR_BAD_FIXUP;
                pSym = &paSyms[Fixup.r.r_symbolnum];

                if (pSym->n_type & MACHO_N_STAB)
                    return VERR_LDR_BAD_FIXUP;

                switch (pSym->n_type & MACHO_N_TYPE)
                {
                    case MACHO_N_SECT:
                    {
                        PKLDRMODMACHOSECT pSymSect;
                        KLDRMODMACHO_CHECK_RETURN((uint32_t)pSym->n_sect - 1 <= pThis->cSections, VERR_LDRMACHO_BAD_SYMBOL);
                        pSymSect = &pThis->paSections[pSym->n_sect - 1];

                        SymAddr += pSym->n_value - pSymSect->LinkAddress + pSymSect->RVA + NewBaseAddress;
                        break;
                    }

                    case MACHO_N_UNDF:
                    case MACHO_N_ABS:
                        SymAddr += pSym->n_value;
                        break;

                    case MACHO_N_INDR:
                    case MACHO_N_PBUD:
                        KLDRMODMACHO_FAILED_RETURN(VERR_LDRMACHO_TODO);
                    default:
                        KLDRMODMACHO_FAILED_RETURN(VERR_LDRMACHO_BAD_SYMBOL);
                }
            }
            else if (Fixup.r.r_symbolnum != R_ABS)
            {
                PKLDRMODMACHOSECT pSymSect;
                if (Fixup.r.r_symbolnum > pThis->cSections)
                    return VERR_LDR_BAD_FIXUP;
                pSymSect = &pThis->paSections[Fixup.r.r_symbolnum - 1];

                SymAddr -= pSymSect->LinkAddress;
                SymAddr += pSymSect->RVA + NewBaseAddress;
            }

            /* adjust for PC relative */
            if (Fixup.r.r_pcrel)
                SymAddr -= Fixup.r.r_address + pFixupSect->RVA + NewBaseAddress;
        }
        else
        {
            PKLDRMODMACHOSECT pSymSect;
            uint32_t iSymSect;
            RTLDRADDR Value;

            /* sanity */
            KLDRMODMACHO_ASSERT(Fixup.s.r_scattered);
            if ((uint32_t)Fixup.s.r_address >= cbSectBits)
                return VERR_LDR_BAD_FIXUP;

            /* calc fixup addresses. */
            uFix.pv = pbSectBits + Fixup.s.r_address;
            uFixVirgin.pv = pbSectVirginBits ? (uint8_t *)pbSectVirginBits + Fixup.s.r_address : 0;

            /*
             * Calc the symbol value.
             */
            /* The addend is stored in the code. */
            switch (Fixup.s.r_length)
            {
                case 0: SymAddr = *uFixVirgin.pi8; break;
                case 1: SymAddr = *uFixVirgin.pi16; break;
                case 2: SymAddr = *uFixVirgin.pi32; break;
                case 3: SymAddr = *uFixVirgin.pi64; break;
            }
            if (Fixup.s.r_pcrel)
                SymAddr += Fixup.s.r_address;
            Value = Fixup.s.r_value;
            SymAddr -= Value;                   /* (-> addend only) */

            /* Find the section number from the r_value. */
            pSymSect = NULL;
            for (iSymSect = 0; iSymSect < pThis->cSections; iSymSect++)
            {
                RTLDRADDR off = Value - pThis->paSections[iSymSect].LinkAddress;
                if (off < pThis->paSections[iSymSect].cb)
                {
                    pSymSect = &pThis->paSections[iSymSect];
                    break;
                }
                else if (off == pThis->paSections[iSymSect].cb) /* edge case */
                    pSymSect = &pThis->paSections[iSymSect];
            }
            if (!pSymSect)
                return VERR_LDR_BAD_FIXUP;

            /* Calc the symbol address. */
            SymAddr += Value - pSymSect->LinkAddress + pSymSect->RVA + NewBaseAddress;
            if (Fixup.s.r_pcrel)
                SymAddr -= Fixup.s.r_address + pFixupSect->RVA + NewBaseAddress;

            Fixup.r.r_length = ((scattered_relocation_info_t *)&paFixups[iFixup])->r_length;
            Fixup.r.r_type   = ((scattered_relocation_info_t *)&paFixups[iFixup])->r_type;
        }

        /*
         * Write back the fixed up value.
         */
        if (Fixup.r.r_type == GENERIC_RELOC_VANILLA)
        {
            switch (Fixup.r.r_length)
            {
                case 0: *uFix.pu8  = (uint8_t)SymAddr; break;
                case 1: *uFix.pu16 = (uint16_t)SymAddr; break;
                case 2: *uFix.pu32 = (uint32_t)SymAddr; break;
                case 3: *uFix.pu64 = (uint64_t)SymAddr; break;
            }
        }
        else if (Fixup.r.r_type <= GENERIC_RELOC_LOCAL_SECTDIFF)
            return VERR_LDRMACHO_UNSUPPORTED_FIXUP_TYPE;
        else
            return VERR_LDR_BAD_FIXUP;
    }

    return 0;
}


/**
 * Applies AMD64 fixups to a section.
 *
 * @returns 0 on success, non-zero kLdr status code on failure.
 * @param   pThis       The Mach-O module interpreter instance.
 * @param   pbSectBits      Pointer to the section bits.
 * @param   pFixupSect      The section being fixed up.
 * @param   NewBaseAddress  The new base image address.
 */
static int  kldrModMachOFixupSectionAMD64(PKLDRMODMACHO pThis, uint8_t *pbSectBits, PKLDRMODMACHOSECT pFixupSect,
                                          macho_nlist_64_t *paSyms, uint32_t cSyms, RTLDRADDR NewBaseAddress)
{
    const macho_relocation_info_t *paFixups = pFixupSect->paFixups;
    const uint32_t cFixups = pFixupSect->cFixups;
    size_t cbSectBits = (size_t)pFixupSect->cb;
    const uint8_t *pbSectVirginBits;
    uint32_t iFixup;
    RTLDRADDR SymAddr;
    int rc;

    /*
     * Find the virgin bits.
     */
    if (pFixupSect->offFile != -1)
    {
        rc = kldrModMachOMapVirginBits(pThis);
        if (rc)
            return rc;
        pbSectVirginBits = (const uint8_t *)pThis->pvBits + pFixupSect->offFile;
    }
    else
        pbSectVirginBits = NULL;

    /*
     * Iterate the fixups and apply them.
     */
    for (iFixup = 0; iFixup < cFixups; iFixup++)
    {
        union
        {
            macho_relocation_info_t     r;
            scattered_relocation_info_t s;
        } Fixup;
        Fixup.r = paFixups[iFixup];

        /* AMD64 doesn't use scattered fixups. */
        KLDRMODMACHO_CHECK_RETURN(!(Fixup.r.r_address & R_SCATTERED), VERR_LDR_BAD_FIXUP);

        /* sanity */
        KLDRMODMACHO_CHECK_RETURN((uint32_t)Fixup.r.r_address < cbSectBits, VERR_LDR_BAD_FIXUP);

        /* calc fixup addresses. */
        RTPTRUNION uFix;
        uFix.pv = pbSectBits + Fixup.r.r_address;
        RTPTRUNION uFixVirgin;
        uFixVirgin.pv = pbSectVirginBits ? (uint8_t *)pbSectVirginBits + Fixup.r.r_address : 0;

        /*
         * Calc the symbol value.
         */
        /* Calc the linked symbol address / addend. */
        switch (Fixup.r.r_length)
        {
            /** @todo Deal with unaligned accesses on non x86 platforms. */
            case 2: SymAddr = *uFixVirgin.pi32; break;
            case 3: SymAddr = *uFixVirgin.pi64; break;
            default:
                KLDRMODMACHO_FAILED_RETURN(VERR_LDR_BAD_FIXUP);
        }

        /* Add symbol / section address. */
        if (Fixup.r.r_extern)
        {
            const macho_nlist_64_t *pSym;

            KLDRMODMACHO_CHECK_RETURN(Fixup.r.r_symbolnum < cSyms, VERR_LDR_BAD_FIXUP);
            pSym = &paSyms[Fixup.r.r_symbolnum];
            KLDRMODMACHO_CHECK_RETURN(!(pSym->n_type & MACHO_N_STAB), VERR_LDR_BAD_FIXUP);

            switch (Fixup.r.r_type)
            {
                /* GOT references just needs to have their symbol verified.
                   Later, we'll optimize GOT building here using a parallel sym->got array. */
                case X86_64_RELOC_GOT_LOAD:
                case X86_64_RELOC_GOT:
                    switch (pSym->n_type & MACHO_N_TYPE)
                    {
                        case MACHO_N_SECT:
                        case MACHO_N_UNDF:
                        case MACHO_N_ABS:
                            break;
                        case MACHO_N_INDR:
                        case MACHO_N_PBUD:
                            KLDRMODMACHO_FAILED_RETURN(VERR_LDRMACHO_TODO);
                        default:
                            KLDRMODMACHO_FAILED_RETURN(VERR_LDRMACHO_BAD_SYMBOL);
                    }
                    SymAddr = sizeof(uint64_t) * Fixup.r.r_symbolnum + pThis->GotRVA + NewBaseAddress;
                    KLDRMODMACHO_CHECK_RETURN(Fixup.r.r_length == 2, VERR_LDR_BAD_FIXUP);
                    SymAddr -= 4;
                    break;

                /* Verify the r_pcrel field for signed fixups on the way into the default case. */
                case X86_64_RELOC_BRANCH:
                case X86_64_RELOC_SIGNED:
                case X86_64_RELOC_SIGNED_1:
                case X86_64_RELOC_SIGNED_2:
                case X86_64_RELOC_SIGNED_4:
                    KLDRMODMACHO_CHECK_RETURN(Fixup.r.r_pcrel, VERR_LDR_BAD_FIXUP);
                    /* Falls through. */
                default:
                {
                    /* Adjust with fixup specific addend and vierfy unsigned/r_pcrel. */
                    switch (Fixup.r.r_type)
                    {
                        case X86_64_RELOC_UNSIGNED:
                            KLDRMODMACHO_CHECK_RETURN(!Fixup.r.r_pcrel, VERR_LDR_BAD_FIXUP);
                            break;
                        case X86_64_RELOC_BRANCH:
                            KLDRMODMACHO_CHECK_RETURN(Fixup.r.r_length == 2, VERR_LDR_BAD_FIXUP);
                            SymAddr -= 4;
                            break;
                        case X86_64_RELOC_SIGNED:
                        case X86_64_RELOC_SIGNED_1:
                        case X86_64_RELOC_SIGNED_2:
                        case X86_64_RELOC_SIGNED_4:
                            SymAddr -= 4;
                            break;
                        default:
                            KLDRMODMACHO_FAILED_RETURN(VERR_LDR_BAD_FIXUP);
                    }

                    switch (pSym->n_type & MACHO_N_TYPE)
                    {
                        case MACHO_N_SECT:
                        {
                            PKLDRMODMACHOSECT pSymSect;
                            KLDRMODMACHO_CHECK_RETURN((uint32_t)pSym->n_sect - 1 <= pThis->cSections, VERR_LDRMACHO_BAD_SYMBOL);
                            pSymSect = &pThis->paSections[pSym->n_sect - 1];
                            SymAddr += pSym->n_value - pSymSect->LinkAddress + pSymSect->RVA + NewBaseAddress;
                            break;
                        }

                        case MACHO_N_UNDF:
                            /* branch to an external symbol may have to take a short detour. */
                            if (   Fixup.r.r_type == X86_64_RELOC_BRANCH
                                &&       SymAddr + Fixup.r.r_address + pFixupSect->RVA + NewBaseAddress
                                       - pSym->n_value
                                       + UINT64_C(0x80000000)
                                    >= UINT64_C(0xffffff20))
                                SymAddr += pThis->cbJmpStub * Fixup.r.r_symbolnum + pThis->JmpStubsRVA + NewBaseAddress;
                            else
                                SymAddr += pSym->n_value;
                            break;

                        case MACHO_N_ABS:
                            SymAddr += pSym->n_value;
                            break;

                        case MACHO_N_INDR:
                        case MACHO_N_PBUD:
                            KLDRMODMACHO_FAILED_RETURN(VERR_LDRMACHO_TODO);
                        default:
                            KLDRMODMACHO_FAILED_RETURN(VERR_LDRMACHO_BAD_SYMBOL);
                    }
                    break;
                }

                /*
                 * This is a weird customer, it will always be follows by an UNSIGNED fixup.
                 */
                case X86_64_RELOC_SUBTRACTOR:
                {
                    macho_relocation_info_t Fixup2;

                    /* Deal with the SUBTRACT symbol first, by subtracting it from SymAddr. */
                    switch (pSym->n_type & MACHO_N_TYPE)
                    {
                        case MACHO_N_SECT:
                        {
                            PKLDRMODMACHOSECT pSymSect;
                            KLDRMODMACHO_CHECK_RETURN((uint32_t)pSym->n_sect - 1 <= pThis->cSections, VERR_LDRMACHO_BAD_SYMBOL);
                            pSymSect = &pThis->paSections[pSym->n_sect - 1];
                            SymAddr -= pSym->n_value - pSymSect->LinkAddress + pSymSect->RVA + NewBaseAddress;
                            break;
                        }

                        case MACHO_N_UNDF:
                        case MACHO_N_ABS:
                            SymAddr -= pSym->n_value;
                            break;

                        case MACHO_N_INDR:
                        case MACHO_N_PBUD:
                            KLDRMODMACHO_FAILED_RETURN(VERR_LDRMACHO_TODO);
                        default:
                            KLDRMODMACHO_FAILED_RETURN(VERR_LDRMACHO_BAD_SYMBOL);
                    }

                    /* Load the 2nd fixup, check sanity. */
                    iFixup++;
                    KLDRMODMACHO_CHECK_RETURN(!Fixup.r.r_pcrel && iFixup < cFixups, VERR_LDR_BAD_FIXUP);
                    Fixup2 = paFixups[iFixup];
                    KLDRMODMACHO_CHECK_RETURN(   Fixup2.r_address == Fixup.r.r_address
                                              && Fixup2.r_length == Fixup.r.r_length
                                              && Fixup2.r_type == X86_64_RELOC_UNSIGNED
                                              && !Fixup2.r_pcrel
                                              && Fixup2.r_symbolnum < cSyms,
                                              VERR_LDR_BAD_FIXUP);

                    if (Fixup2.r_extern)
                    {
                        KLDRMODMACHO_CHECK_RETURN(Fixup2.r_symbolnum < cSyms, VERR_LDR_BAD_FIXUP);
                        pSym = &paSyms[Fixup2.r_symbolnum];
                        KLDRMODMACHO_CHECK_RETURN(!(pSym->n_type & MACHO_N_STAB), VERR_LDR_BAD_FIXUP);

                        /* Add it's value to SymAddr. */
                        switch (pSym->n_type & MACHO_N_TYPE)
                        {
                            case MACHO_N_SECT:
                            {
                                PKLDRMODMACHOSECT pSymSect;
                                KLDRMODMACHO_CHECK_RETURN((uint32_t)pSym->n_sect - 1 <= pThis->cSections, VERR_LDRMACHO_BAD_SYMBOL);
                                pSymSect = &pThis->paSections[pSym->n_sect - 1];
                                SymAddr += pSym->n_value - pSymSect->LinkAddress + pSymSect->RVA + NewBaseAddress;
                                break;
                            }

                            case MACHO_N_UNDF:
                            case MACHO_N_ABS:
                                SymAddr += pSym->n_value;
                                break;

                            case MACHO_N_INDR:
                            case MACHO_N_PBUD:
                                KLDRMODMACHO_FAILED_RETURN(VERR_LDRMACHO_TODO);
                            default:
                                KLDRMODMACHO_FAILED_RETURN(VERR_LDRMACHO_BAD_SYMBOL);
                        }
                    }
                    else if (Fixup2.r_symbolnum != R_ABS)
                    {
                        PKLDRMODMACHOSECT pSymSect;
                        KLDRMODMACHO_CHECK_RETURN(Fixup2.r_symbolnum <= pThis->cSections, VERR_LDR_BAD_FIXUP);
                        pSymSect = &pThis->paSections[Fixup2.r_symbolnum - 1];
                        SymAddr += pSymSect->RVA + NewBaseAddress;
                    }
                    else
                        KLDRMODMACHO_FAILED_RETURN(VERR_LDR_BAD_FIXUP);
                }
                break;
            }
        }
        else
        {
            /* verify against fixup type and make adjustments */
            switch (Fixup.r.r_type)
            {
                case X86_64_RELOC_UNSIGNED:
                    KLDRMODMACHO_CHECK_RETURN(!Fixup.r.r_pcrel, VERR_LDR_BAD_FIXUP);
                    break;
                case X86_64_RELOC_BRANCH:
                    KLDRMODMACHO_CHECK_RETURN(Fixup.r.r_pcrel, VERR_LDR_BAD_FIXUP);
                    SymAddr += 4; /* dunno what the assmbler/linker really is doing here... */
                    break;
                case X86_64_RELOC_SIGNED:
                case X86_64_RELOC_SIGNED_1:
                case X86_64_RELOC_SIGNED_2:
                case X86_64_RELOC_SIGNED_4:
                    KLDRMODMACHO_CHECK_RETURN(Fixup.r.r_pcrel, VERR_LDR_BAD_FIXUP);
                    break;
                /*case X86_64_RELOC_GOT_LOAD:*/
                /*case X86_64_RELOC_GOT: */
                /*case X86_64_RELOC_SUBTRACTOR: - must be r_extern=1 says as. */
                default:
                    KLDRMODMACHO_FAILED_RETURN(VERR_LDR_BAD_FIXUP);
            }
            if (Fixup.r.r_symbolnum != R_ABS)
            {
                PKLDRMODMACHOSECT pSymSect;
                KLDRMODMACHO_CHECK_RETURN(Fixup.r.r_symbolnum <= pThis->cSections, VERR_LDR_BAD_FIXUP);
                pSymSect = &pThis->paSections[Fixup.r.r_symbolnum - 1];

                SymAddr -= pSymSect->LinkAddress;
                SymAddr += pSymSect->RVA + NewBaseAddress;
                if (Fixup.r.r_pcrel)
                    SymAddr += Fixup.r.r_address;
            }
        }

        /* adjust for PC relative */
        if (Fixup.r.r_pcrel)
            SymAddr -= Fixup.r.r_address + pFixupSect->RVA + NewBaseAddress;

        /*
         * Write back the fixed up value.
         */
        switch (Fixup.r.r_length)
        {
            case 3:
                *uFix.pu64 = (uint64_t)SymAddr;
                break;
            case 2:
                KLDRMODMACHO_CHECK_RETURN(Fixup.r.r_pcrel || Fixup.r.r_type == X86_64_RELOC_SUBTRACTOR, VERR_LDR_BAD_FIXUP);
                KLDRMODMACHO_CHECK_RETURN((int32_t)SymAddr == (int64_t)SymAddr, VERR_LDR_ADDRESS_OVERFLOW);
                *uFix.pu32 = (uint32_t)SymAddr;
                break;
            default:
                KLDRMODMACHO_FAILED_RETURN(VERR_LDR_BAD_FIXUP);
        }
    }

    return 0;
}


/**
 * Loads the symbol table for a MH_OBJECT file.
 *
 * The symbol table is pointed to by KLDRMODMACHO::pvaSymbols.
 *
 * @returns 0 on success, non-zero kLdr status code on failure.
 * @param   pThis       The Mach-O module interpreter instance.
 */
static int  kldrModMachOLoadObjSymTab(PKLDRMODMACHO pThis)
{
    int rc = 0;

    if (    !pThis->pvaSymbols
        &&  pThis->cSymbols)
    {
        size_t cbSyms;
        size_t cbSym;
        void *pvSyms;
        void *pvStrings;

        /* sanity */
        KLDRMODMACHO_CHECK_RETURN(   pThis->offSymbols
                                  && (!pThis->cchStrings || pThis->offStrings),
                                  VERR_LDRMACHO_BAD_OBJECT_FILE);

        /* allocate */
        cbSym = pThis->Hdr.magic == IMAGE_MACHO32_SIGNATURE
             || pThis->Hdr.magic == IMAGE_MACHO32_SIGNATURE_OE
             ? sizeof(macho_nlist_32_t)
             : sizeof(macho_nlist_64_t);
        cbSyms = pThis->cSymbols * cbSym;
        KLDRMODMACHO_CHECK_RETURN(cbSyms / cbSym == pThis->cSymbols, VERR_LDRMACHO_BAD_SYMTAB_SIZE);
        rc = VERR_NO_MEMORY;
        pvSyms = RTMemAlloc(cbSyms);
        if (pvSyms)
        {
            if (pThis->cchStrings)
                pvStrings = RTMemAlloc(pThis->cchStrings);
            else
                pvStrings = RTMemAllocZ(4);
            if (pvStrings)
            {
                /* read */
                rc = pThis->Core.pReader->pfnRead(pThis->Core.pReader, pvSyms, cbSyms, pThis->offSymbols);
                if (!rc && pThis->cchStrings)
                    rc = pThis->Core.pReader->pfnRead(pThis->Core.pReader, pvStrings,
                                                          pThis->cchStrings, pThis->offStrings);
                if (!rc)
                {
                    pThis->pvaSymbols = pvSyms;
                    pThis->pchStrings = (char *)pvStrings;

                    /* perform endian conversion? */
                    if (pThis->Hdr.magic == IMAGE_MACHO32_SIGNATURE_OE)
                    {
                        uint32_t cLeft = pThis->cSymbols;
                        macho_nlist_32_t *pSym = (macho_nlist_32_t *)pvSyms;
                        while (cLeft-- > 0)
                        {
                            pSym->n_un.n_strx = RT_BSWAP_U32(pSym->n_un.n_strx);
                            pSym->n_desc = (int16_t)RT_BSWAP_U16(pSym->n_desc);
                            pSym->n_value = RT_BSWAP_U32(pSym->n_value);
                            pSym++;
                        }
                    }
                    else if (pThis->Hdr.magic == IMAGE_MACHO64_SIGNATURE_OE)
                    {
                        uint32_t cLeft = pThis->cSymbols;
                        macho_nlist_64_t *pSym = (macho_nlist_64_t *)pvSyms;
                        while (cLeft-- > 0)
                        {
                            pSym->n_un.n_strx = RT_BSWAP_U32(pSym->n_un.n_strx);
                            pSym->n_desc = (int16_t)RT_BSWAP_U16(pSym->n_desc);
                            pSym->n_value = RT_BSWAP_U64(pSym->n_value);
                            pSym++;
                        }
                    }

                    return 0;
                }
                RTMemFree(pvStrings);
            }
            RTMemFree(pvSyms);
        }
    }
    else
        KLDRMODMACHO_ASSERT(pThis->pchStrings || pThis->Hdr.filetype == MH_DSYM);

    return rc;
}


/**
 * Loads the fixups at the given address and performs endian
 * conversion if necessary.
 *
 * @returns 0 on success, non-zero kLdr status code on failure.
 * @param   pThis       The Mach-O module interpreter instance.
 * @param   offFixups       The file offset of the fixups.
 * @param   cFixups         The number of fixups to load.
 * @param   ppaFixups       Where to put the pointer to the allocated fixup array.
 */
static int  kldrModMachOLoadFixups(PKLDRMODMACHO pThis, RTFOFF offFixups, uint32_t cFixups, macho_relocation_info_t **ppaFixups)
{
    macho_relocation_info_t *paFixups;
    size_t cbFixups;
    int rc;

    /* allocate the memory. */
    cbFixups = cFixups * sizeof(*paFixups);
    KLDRMODMACHO_CHECK_RETURN(cbFixups / sizeof(*paFixups) == cFixups, VERR_LDRMACHO_BAD_SYMTAB_SIZE);
    paFixups = (macho_relocation_info_t *)RTMemAlloc(cbFixups);
    if (!paFixups)
        return VERR_NO_MEMORY;

    /* read the fixups. */
    rc = pThis->Core.pReader->pfnRead(pThis->Core.pReader, paFixups, cbFixups, offFixups);
    if (!rc)
    {
        *ppaFixups = paFixups;

        /* do endian conversion if necessary. */
        if (    pThis->Hdr.magic == IMAGE_MACHO32_SIGNATURE_OE
            ||  pThis->Hdr.magic == IMAGE_MACHO64_SIGNATURE_OE)
        {
            uint32_t iFixup;
            for (iFixup = 0; iFixup < cFixups; iFixup++)
            {
                uint32_t *pu32 = (uint32_t *)&paFixups[iFixup];
                pu32[0] = RT_BSWAP_U32(pu32[0]);
                pu32[1] = RT_BSWAP_U32(pu32[1]);
            }
        }
    }
    else
        RTMemFree(paFixups);
    return rc;
}


/**
 * Maps the virgin file bits into memory if not already done.
 *
 * @returns 0 on success, non-zero kLdr status code on failure.
 * @param   pThis       The Mach-O module interpreter instance.
 */
static int kldrModMachOMapVirginBits(PKLDRMODMACHO pThis)
{
    int rc = 0;
    if (!pThis->pvBits)
        rc = pThis->Core.pReader->pfnMap(pThis->Core.pReader, &pThis->pvBits);
    return rc;
}

#if 0

/** @copydoc kLdrModCallInit */
static int kldrModMachOCallInit(PRTLDRMODINTERNAL pMod, void *pvMapping, uintptr_t uHandle)
{
    /* later */
    RT_NOREF(pMod);
    RT_NOREF(pvMapping);
    RT_NOREF(uHandle);
    return 0;
}


/** @copydoc kLdrModCallTerm */
static int kldrModMachOCallTerm(PRTLDRMODINTERNAL pMod, void *pvMapping, uintptr_t uHandle)
{
    /* later */
    RT_NOREF(pMod);
    RT_NOREF(pvMapping);
    RT_NOREF(uHandle);
    return 0;
}


/** @copydoc kLdrModCallThread */
static int kldrModMachOCallThread(PRTLDRMODINTERNAL pMod, void *pvMapping, uintptr_t uHandle, unsigned fAttachingOrDetaching)
{
    /* Relevant for Mach-O? */
    RT_NOREF(pMod);
    RT_NOREF(pvMapping);
    RT_NOREF(uHandle);
    RT_NOREF(fAttachingOrDetaching);
    return 0;
}

#endif


/**
 * @interface_method_impl{RTLDROPS,pfnGetImageSize}
 */
static size_t rtldrMachO_GetImageSize(PRTLDRMODINTERNAL pMod)
{
    PKLDRMODMACHO pThis = RT_FROM_MEMBER(pMod, KLDRMODMACHO, Core);
    return pThis->cbImage;
}


/**
 * @interface_method_impl{RTLDROPS,pfnGetBits}
 */
static DECLCALLBACK(int) rtldrMachO_GetBits(PRTLDRMODINTERNAL pMod, void *pvBits, RTUINTPTR BaseAddress,
                                            PFNRTLDRIMPORT pfnGetImport, void *pvUser)
{
    PKLDRMODMACHO pThis = RT_FROM_MEMBER(pMod, KLDRMODMACHO, Core);

    if (!pThis->fCanLoad)
        return VERR_LDRMACHO_TODO;

    /*
     * Zero the entire buffer first to simplify things.
     */
    memset(pvBits, 0, (size_t)pThis->cbImage);

    /*
     * When possible use the segment table to load the data.
     */
    for (uint32_t i = 0; i < pThis->cSegments; i++)
    {
        /* skip it? */
        if (   pThis->aSegments[i].SegInfo.cbFile == -1
            || pThis->aSegments[i].SegInfo.offFile == -1
            || pThis->aSegments[i].SegInfo.LinkAddress == NIL_RTLDRADDR
            || !pThis->aSegments[i].SegInfo.Alignment)
            continue;
        int rc = pThis->Core.pReader->pfnRead(pThis->Core.pReader,
                                                  (uint8_t *)pvBits + pThis->aSegments[i].SegInfo.RVA,
                                                  pThis->aSegments[i].SegInfo.cbFile,
                                                  pThis->aSegments[i].SegInfo.offFile);
        if (rc)
            return rc;
    }

    /*
     * Perform relocations.
     */
    return rtldrMachO_RelocateBits(pMod, pvBits, BaseAddress, pThis->LinkAddress, pfnGetImport, pvUser);
}


/**
 * @interface_method_impl{RTLDROPS,pfnRelocate}
 */
static DECLCALLBACK(int) rtldrMachO_RelocateBits(PRTLDRMODINTERNAL pMod, void *pvBits, RTUINTPTR NewBaseAddress,
                                                 RTUINTPTR OldBaseAddress, PFNRTLDRIMPORT pfnGetImport, void *pvUser)
{
    PKLDRMODMACHO pThis = RT_FROM_MEMBER(pMod, KLDRMODMACHO, Core);
    int rc;
    RT_NOREF(OldBaseAddress);

    /*
     * Call workers to do the jobs.
     */
    if (pThis->Hdr.filetype == MH_OBJECT)
    {
        rc = kldrModMachOObjDoImports(pThis, NewBaseAddress, pfnGetImport, pvUser);
        if (!rc)
            rc = kldrModMachOObjDoFixups(pThis, pvBits, NewBaseAddress);

    }
    else
        rc = VERR_LDRMACHO_TODO;
    /*{
        rc = kldrModMachODoFixups(pThis, pvBits, NewBaseAddress, OldBaseAddress, pfnGetImport, pvUser);
        if (!rc)
            rc = kldrModMachODoImports(pThis, pvBits, pfnGetImport, pvUser);
    }*/

    /*
     * Construct the global offset table if necessary, it's always the last
     * segment when present.
     */
    if (!rc && pThis->fMakeGot)
        rc = kldrModMachOMakeGOT(pThis, pvBits, NewBaseAddress);

    return rc;
}


/**
 * Builds the GOT.
 *
 * Assumes the symbol table has all external symbols resolved correctly and that
 * the bits has been cleared up front.
 */
static int kldrModMachOMakeGOT(PKLDRMODMACHO pThis, void *pvBits, RTLDRADDR NewBaseAddress)
{
    uint32_t  iSym = pThis->cSymbols;
    if (    pThis->Hdr.magic == IMAGE_MACHO32_SIGNATURE
        ||  pThis->Hdr.magic == IMAGE_MACHO32_SIGNATURE_OE)
    {
        macho_nlist_32_t const *paSyms = (macho_nlist_32_t const *)pThis->pvaSymbols;
        uint32_t *paGOT = (uint32_t *)((uint8_t *)pvBits + pThis->GotRVA);
        while (iSym-- > 0)
            switch (paSyms[iSym].n_type & MACHO_N_TYPE)
            {
                case MACHO_N_SECT:
                {
                    PKLDRMODMACHOSECT pSymSect;
                    KLDRMODMACHO_CHECK_RETURN((uint32_t)paSyms[iSym].n_sect - 1 <= pThis->cSections, VERR_LDRMACHO_BAD_SYMBOL);
                    pSymSect = &pThis->paSections[paSyms[iSym].n_sect - 1];
                    paGOT[iSym] = (uint32_t)(paSyms[iSym].n_value - pSymSect->LinkAddress + pSymSect->RVA + NewBaseAddress);
                    break;
                }

                case MACHO_N_UNDF:
                case MACHO_N_ABS:
                    paGOT[iSym] = paSyms[iSym].n_value;
                    break;
            }
    }
    else
    {
        macho_nlist_64_t const *paSyms = (macho_nlist_64_t const *)pThis->pvaSymbols;
        uint64_t *paGOT = (uint64_t *)((uint8_t *)pvBits + pThis->GotRVA);
        while (iSym-- > 0)
        {
            switch (paSyms[iSym].n_type & MACHO_N_TYPE)
            {
                case MACHO_N_SECT:
                {
                    PKLDRMODMACHOSECT pSymSect;
                    KLDRMODMACHO_CHECK_RETURN((uint32_t)paSyms[iSym].n_sect - 1 <= pThis->cSections, VERR_LDRMACHO_BAD_SYMBOL);
                    pSymSect = &pThis->paSections[paSyms[iSym].n_sect - 1];
                    paGOT[iSym] = paSyms[iSym].n_value - pSymSect->LinkAddress + pSymSect->RVA + NewBaseAddress;
                    break;
                }

                case MACHO_N_UNDF:
                case MACHO_N_ABS:
                    paGOT[iSym] = paSyms[iSym].n_value;
                    break;
            }
        }

        if (pThis->JmpStubsRVA != NIL_RTLDRADDR)
        {
            iSym = pThis->cSymbols;
            switch (pThis->Hdr.cputype)
            {
                /*
                 * AMD64 is simple since the GOT and the indirect jmps are parallel
                 * arrays with entries of the same size. The relative offset will
                 * be the the same for each entry, kind of nice. :-)
                 */
                case CPU_TYPE_X86_64:
                {
                    uint64_t   *paJmps = (uint64_t *)((uint8_t *)pvBits + pThis->JmpStubsRVA);
                    int32_t     off;
                    uint64_t    u64Tmpl;
                    union
                    {
                        uint8_t     ab[8];
                        uint64_t    u64;
                    }       Tmpl;

                    /* create the template. */
                    off = (int32_t)(pThis->GotRVA - (pThis->JmpStubsRVA + 6));
                    Tmpl.ab[0] = 0xff; /* jmp [GOT-entry wrt RIP] */
                    Tmpl.ab[1] = 0x25;
                    Tmpl.ab[2] =  off        & 0xff;
                    Tmpl.ab[3] = (off >>  8) & 0xff;
                    Tmpl.ab[4] = (off >> 16) & 0xff;
                    Tmpl.ab[5] = (off >> 24) & 0xff;
                    Tmpl.ab[6] = 0xcc;
                    Tmpl.ab[7] = 0xcc;
                    u64Tmpl = Tmpl.u64;

                    /* copy the template to every jmp table entry. */
                    while (iSym-- > 0)
                        paJmps[iSym] = u64Tmpl;
                    break;
                }

                default:
                    KLDRMODMACHO_FAILED_RETURN(VERR_LDRMACHO_TODO);
            }
        }
    }
    return 0;
}


/**
 * @interface_method_impl{RTLDROPS,pfnEnumSegments}
 */
static DECLCALLBACK(int) rtldrMachO_EnumSegments(PRTLDRMODINTERNAL pMod, PFNRTLDRENUMSEGS pfnCallback, void *pvUser)
{
    PKLDRMODMACHO  pThis = RT_FROM_MEMBER(pMod, KLDRMODMACHO, Core);
    uint32_t const cSegments  = pThis->cSegments;
    for (uint32_t iSeg = 0; iSeg < cSegments; iSeg++)
    {
        RTLDRSEG Seg = pThis->aSegments[iSeg].SegInfo;
        int rc = pfnCallback(pMod, &Seg, pvUser);
        if (rc != VINF_SUCCESS)
            return rc;
    }

    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{RTLDROPS,pfnLinkAddressToSegOffset}
 */
static DECLCALLBACK(int) rtldrMachO_LinkAddressToSegOffset(PRTLDRMODINTERNAL pMod, RTLDRADDR LinkAddress,
                                                           uint32_t *piSeg, PRTLDRADDR poffSeg)
{
    PKLDRMODMACHO  pThis = RT_FROM_MEMBER(pMod, KLDRMODMACHO, Core);
    uint32_t const cSegments  = pThis->cSegments;
    for (uint32_t iSeg = 0; iSeg < cSegments; iSeg++)
    {
        RTLDRADDR offSeg = LinkAddress - pThis->aSegments[iSeg].SegInfo.LinkAddress;
        if (   offSeg < pThis->aSegments[iSeg].SegInfo.cbMapped
            || offSeg < pThis->aSegments[iSeg].SegInfo.cb)
        {
            *piSeg = iSeg;
            *poffSeg = offSeg;
            return VINF_SUCCESS;
        }
    }

    return VERR_LDR_INVALID_LINK_ADDRESS;
}


/**
 * @interface_method_impl{RTLDROPS,pfnLinkAddressToRva}.
 */
static DECLCALLBACK(int) rtldrMachO_LinkAddressToRva(PRTLDRMODINTERNAL pMod, RTLDRADDR LinkAddress, PRTLDRADDR pRva)
{
    PKLDRMODMACHO  pThis = RT_FROM_MEMBER(pMod, KLDRMODMACHO, Core);
    uint32_t const cSegments  = pThis->cSegments;
    for (uint32_t iSeg = 0; iSeg < cSegments; iSeg++)
    {
        RTLDRADDR offSeg = LinkAddress - pThis->aSegments[iSeg].SegInfo.LinkAddress;
        if (   offSeg < pThis->aSegments[iSeg].SegInfo.cbMapped
            || offSeg < pThis->aSegments[iSeg].SegInfo.cb)
        {
            *pRva = pThis->aSegments[iSeg].SegInfo.RVA + offSeg;
            return VINF_SUCCESS;
        }
    }

    return VERR_LDR_INVALID_RVA;
}


/**
 * @interface_method_impl{RTLDROPS,pfnSegOffsetToRva}
 */
static DECLCALLBACK(int) rtldrMachO_SegOffsetToRva(PRTLDRMODINTERNAL pMod, uint32_t iSeg, RTLDRADDR offSeg, PRTLDRADDR pRva)
{
    PKLDRMODMACHO  pThis = RT_FROM_MEMBER(pMod, KLDRMODMACHO, Core);

    if (iSeg >= pThis->cSegments)
        return VERR_LDR_INVALID_SEG_OFFSET;
    KLDRMODMACHOSEG const *pSegment = &pThis->aSegments[iSeg];

    if (   offSeg > pSegment->SegInfo.cbMapped
        && offSeg > pSegment->SegInfo.cb
        && (    pSegment->SegInfo.cbFile < 0
            ||  offSeg > (uint64_t)pSegment->SegInfo.cbFile))
        return VERR_LDR_INVALID_SEG_OFFSET;

    *pRva = pSegment->SegInfo.RVA + offSeg;
    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{RTLDROPS,pfnRvaToSegOffset}
 */
static DECLCALLBACK(int) rtldrMachO_RvaToSegOffset(PRTLDRMODINTERNAL pMod, RTLDRADDR Rva, uint32_t *piSeg, PRTLDRADDR poffSeg)
{
    PKLDRMODMACHO  pThis = RT_FROM_MEMBER(pMod, KLDRMODMACHO, Core);
    uint32_t const cSegments  = pThis->cSegments;
    for (uint32_t iSeg = 0; iSeg < cSegments; iSeg++)
    {
        RTLDRADDR offSeg = Rva - pThis->aSegments[iSeg].SegInfo.RVA;
        if (   offSeg < pThis->aSegments[iSeg].SegInfo.cbMapped
            || offSeg < pThis->aSegments[iSeg].SegInfo.cb)
        {
            *piSeg = iSeg;
            *poffSeg = offSeg;
            return VINF_SUCCESS;
        }
    }

    return VERR_LDR_INVALID_RVA;
}


/**
 * @interface_method_impl{RTLDROPS,pfnReadDbgInfo}
 */
static DECLCALLBACK(int) rtldrMachO_ReadDbgInfo(PRTLDRMODINTERNAL pMod, uint32_t iDbgInfo, RTFOFF off, size_t cb, void *pvBuf)
{
    PKLDRMODMACHO  pThis = RT_FROM_MEMBER(pMod, KLDRMODMACHO, Core);

    /** @todo May have to apply fixups here. */
    if (iDbgInfo == iDbgInfo)
        return pThis->Core.pReader->pfnRead(pThis->Core.pReader, pvBuf, cb, off);
    if (iDbgInfo < pThis->cSections)
    {
        return pThis->Core.pReader->pfnRead(pThis->Core.pReader, pvBuf, cb, off);
    }
    return VERR_OUT_OF_RANGE;
}


/** @interface_method_impl{RTLDROPS,pfnQueryProp} */
static DECLCALLBACK(int) rtldrMachO_QueryProp(PRTLDRMODINTERNAL pMod, RTLDRPROP enmProp, void const *pvBits,
                                          void *pvBuf, size_t cbBuf, size_t *pcbRet)
{
    PKLDRMODMACHO pThis = RT_FROM_MEMBER(pMod, KLDRMODMACHO, Core);
    int           rc;
    switch (enmProp)
    {
        case RTLDRPROP_UUID:
            rc = kldrModMachOQueryImageUuid(pThis, pvBits, (uint8_t *)pvBuf, cbBuf);
            if (RT_FAILURE(rc))
                return rc;
            cbBuf = RT_MIN(cbBuf, sizeof(RTUUID));
            break;

#if 0 /** @todo return LC_ID_DYLIB */
        case RTLDRPROP_INTERNAL_NAME:
#endif

        default:
            return VERR_NOT_FOUND;
    }
    if (pcbRet)
        *pcbRet = cbBuf;
    RT_NOREF_PV(pvBits);
    return VINF_SUCCESS;
}


/**
 * Operations for a Mach-O module interpreter.
 */
static const RTLDROPS s_rtldrMachOOps=
{
    "mach-o",
    rtldrMachO_Close,
    NULL,
    NULL /*pfnDone*/,
    rtldrMachO_EnumSymbols,
    /* ext */
    rtldrMachO_GetImageSize,
    rtldrMachO_GetBits,
    rtldrMachO_RelocateBits,
    rtldrMachO_GetSymbolEx,
    NULL /*pfnQueryForwarderInfo*/,
    rtldrMachO_EnumDbgInfo,
    rtldrMachO_EnumSegments,
    rtldrMachO_LinkAddressToSegOffset,
    rtldrMachO_LinkAddressToRva,
    rtldrMachO_SegOffsetToRva,
    rtldrMachO_RvaToSegOffset,
    NULL,
    rtldrMachO_QueryProp,
    NULL /*pfnVerifySignature*/,
    NULL /*pfnHashImage*/,
    NULL /*pfnUnwindFrame*/,
    42
};


/**
 * Handles opening Mach-O images (non-fat).
 */
DECLHIDDEN(int) rtldrMachOOpen(PRTLDRREADER pReader, uint32_t fFlags, RTLDRARCH enmArch, RTFOFF offImage,
                               PRTLDRMOD phLdrMod, PRTERRINFO pErrInfo)
{

    /*
     * Create the instance data and do a minimal header validation.
     */
    PKLDRMODMACHO pThis = NULL;
    int rc = kldrModMachODoCreate(pReader, offImage, fFlags, &pThis, pErrInfo);
    if (RT_SUCCESS(rc))
    {
        /*
         * Match up against the requested CPU architecture.
         */
        if (   enmArch == RTLDRARCH_WHATEVER
            || pThis->Core.enmArch == enmArch)
        {
            pThis->Core.pOps     = &s_rtldrMachOOps;
            pThis->Core.u32Magic = RTLDRMOD_MAGIC;
            *phLdrMod = &pThis->Core;
            return 0;
        }
        rc = VERR_LDR_ARCH_MISMATCH;
    }
    if (pThis)
    {
        RTMemFree(pThis->pbLoadCommands);
        RTMemFree(pThis);
    }
    return rc;

}


/**
 * Handles opening FAT Mach-O image.
 */
DECLHIDDEN(int) rtldrFatOpen(PRTLDRREADER pReader, uint32_t fFlags, RTLDRARCH enmArch, PRTLDRMOD phLdrMod, PRTERRINFO pErrInfo)
{
    fat_header_t FatHdr;
    int rc = pReader->pfnRead(pReader, &FatHdr, sizeof(FatHdr), 0);
    if (RT_FAILURE(rc))
        return RTErrInfoSetF(pErrInfo, rc, "Read error at offset 0: %Rrc", rc);

    if (FatHdr.magic == IMAGE_FAT_SIGNATURE)
    { /* likely */ }
    else if (FatHdr.magic == IMAGE_FAT_SIGNATURE_OE)
        FatHdr.nfat_arch = RT_BSWAP_U32(FatHdr.nfat_arch);
    else
        return RTErrInfoSetF(pErrInfo, VERR_INVALID_EXE_SIGNATURE, "magic=%#x", FatHdr.magic);
    if (FatHdr.nfat_arch < 64)
        return RTErrInfoSetF(pErrInfo, VERR_INVALID_EXE_SIGNATURE, "Bad nfat_arch value: %#x", FatHdr.nfat_arch);

    uint32_t offEntry = sizeof(FatHdr);
    for (uint32_t i = 0; i < FatHdr.nfat_arch; i++, offEntry += sizeof(fat_arch_t))
    {
        fat_arch_t FatEntry;
        int rc = pReader->pfnRead(pReader, &FatEntry, sizeof(FatEntry), offEntry);
        if (RT_FAILURE(rc))
            return RTErrInfoSetF(pErrInfo, rc, "Read error at offset 0: %Rrc", rc);
        if (FatHdr.magic == IMAGE_FAT_SIGNATURE_OE)
        {
            FatEntry.cputype    = (int32_t)RT_BSWAP_U32((uint32_t)FatEntry.cputype);
            //FatEntry.cpusubtype = (int32_t)RT_BSWAP_U32((uint32_t)FatEntry.cpusubtype);
            FatEntry.offset     = RT_BSWAP_U32(FatEntry.offset);
            //FatEntry.size       = RT_BSWAP_U32(FatEntry.size);
            //FatEntry.align      = RT_BSWAP_U32(FatEntry.align);
        }

        /*
         * Match enmArch.
         */
        bool fMatch = false;
        switch (enmArch)
        {
            case RTLDRARCH_WHATEVER:
                fMatch = true;
                break;

            case RTLDRARCH_X86_32:
                fMatch = FatEntry.cputype == CPU_TYPE_X86;
                break;

            case RTLDRARCH_AMD64:
                fMatch = FatEntry.cputype == CPU_TYPE_X86_64;
                break;

            case RTLDRARCH_ARM32:
            case RTLDRARCH_ARM64:
            case RTLDRARCH_X86_16:
                fMatch = false;
                break;

            case RTLDRARCH_INVALID:
            case RTLDRARCH_HOST:
            case RTLDRARCH_END:
            case RTLDRARCH_32BIT_HACK:
                AssertFailedReturn(VERR_INVALID_PARAMETER);
        }
        if (fMatch)
            return rtldrMachOOpen(pReader, fFlags, enmArch, FatEntry.offset, phLdrMod, pErrInfo);
    }

    return VERR_LDR_ARCH_MISMATCH;

}

