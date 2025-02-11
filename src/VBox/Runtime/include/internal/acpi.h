/* $Id$ */
/** @file
 * IPRT - Internal RTAcpi header.
 */

/*
 * Copyright (C) 2025 Oracle and/or its affiliates.
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

#ifndef IPRT_INCLUDED_INTERNAL_acpi_h
#define IPRT_INCLUDED_INTERNAL_acpi_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <iprt/acpi.h>
#include <iprt/list.h>

RT_C_DECLS_BEGIN

/** Pointer to an ACPI AST node. */
typedef struct RTACPIASTNODE *PRTACPIASTNODE;
/** Pointer to a const ACPI AST node. */
typedef const struct RTACPIASTNODE *PCRTACPIASTNODE;

/**
 * AST node argument type.
 */
typedef enum RTACPIASTARGTYPE
{
    kAcpiAstArgType_Invalid = 0,
    kAcpiAstArgType_AstNode,
    kAcpiAstArgType_NameString,
    kAcpiAstArgType_Bool,
    kAcpiAstArgType_U8,
    kAcpiAstArgType_U16,
    kAcpiAstArgType_U32,
    kAcpiAstArgType_U64,
    kAcpiAstArgType_ObjType,
    kAcpiAstArgType_RegionSpace,
    kAcpiAstArgType_FieldAcc,
    kAcpiAstArgType_FieldUpdate,
    kAcpiAstArgType_StringLiteral,
    kAcpiAstArgType_32Bit_Hack = 0x7fffffff
} RTACPIASTARGTYPE;


/**
 * An AST node argument
 */
typedef struct RTACPIASTARG
{
    /** Argument type. */
    RTACPIASTARGTYPE        enmType;
    /** Type dependent data. */
    union
    {
        uintptr_t           uPtrInternal;
        PRTACPIASTNODE      pAstNd;
        const char          *pszNameString;
        const char          *pszStrLit;
        bool                f;
        uint8_t             u8;
        uint16_t            u16;
        uint32_t            u32;
        uint64_t            u64;
        RTACPIOBJTYPE       enmObjType;
        RTACPIOPREGIONSPACE enmRegionSpace;
        RTACPIFIELDACC      enmFieldAcc;
        RTACPIFIELDUPDATE   enmFieldUpdate;
    } u;
} RTACPIASTARG;
/** Pointer to an AST node argument. */
typedef RTACPIASTARG *PRTACPIASTARG;
/** Pointer to a const AST node argument. */
typedef const RTACPIASTARG *PCRTACPIASTARG;


/**
 * The ACPI AST node op.
 */
typedef enum RTACPIASTNODEOP
{
    kAcpiAstNodeOp_Invalid = 0,
    kAcpiAstNodeOp_Identifier,
    kAcpiAstNodeOp_StringLiteral,
    kAcpiAstNodeOp_Number,
    kAcpiAstNodeOp_Scope,
    kAcpiAstNodeOp_Processor,
    kAcpiAstNodeOp_External,
    kAcpiAstNodeOp_Method,
    kAcpiAstNodeOp_Device,
    kAcpiAstNodeOp_If,
    kAcpiAstNodeOp_Else,
    kAcpiAstNodeOp_LAnd,
    kAcpiAstNodeOp_LEqual,
    kAcpiAstNodeOp_LGreater,
    kAcpiAstNodeOp_LGreaterEqual,
    kAcpiAstNodeOp_LLess,
    kAcpiAstNodeOp_LLessEqual,
    kAcpiAstNodeOp_LNot,
    kAcpiAstNodeOp_LNotEqual,
    kAcpiAstNodeOp_Zero,
    kAcpiAstNodeOp_One,
    kAcpiAstNodeOp_Ones,
    kAcpiAstNodeOp_Return,
    kAcpiAstNodeOp_Unicode,
    kAcpiAstNodeOp_OperationRegion,
    kAcpiAstNodeOp_Field,
    kAcpiAstNodeOp_Name,
    kAcpiAstNodeOp_ResourceTemplate,
    kAcpiAstNodeOp_Arg0,
    kAcpiAstNodeOp_Arg1,
    kAcpiAstNodeOp_Arg2,
    kAcpiAstNodeOp_Arg3,
    kAcpiAstNodeOp_Arg4,
    kAcpiAstNodeOp_Arg5,
    kAcpiAstNodeOp_Arg6,
    kAcpiAstNodeOp_Local0,
    kAcpiAstNodeOp_Local1,
    kAcpiAstNodeOp_Local2,
    kAcpiAstNodeOp_Local3,
    kAcpiAstNodeOp_Local4,
    kAcpiAstNodeOp_Local5,
    kAcpiAstNodeOp_Local6,
    kAcpiAstNodeOp_Local7,
    kAcpiAstNodeOp_Package,
    kAcpiAstNodeOp_Buffer,
    kAcpiAstNodeOp_ToUuid,
    kAcpiAstNodeOp_DerefOf,
    kAcpiAstNodeOp_Index,
    kAcpiAstNodeOp_Store,
    kAcpiAstNodeOp_Break,
    kAcpiAstNodeOp_Continue,
    kAcpiAstNodeOp_Add,
    kAcpiAstNodeOp_Subtract,
    kAcpiAstNodeOp_And,
    kAcpiAstNodeOp_Nand,
    kAcpiAstNodeOp_Or,
    kAcpiAstNodeOp_Xor,
    kAcpiAstNodeOp_Not,
    kAcpiAstNodeOp_Notify,
    kAcpiAstNodeOp_SizeOf,
    kAcpiAstNodeOp_While,
    kAcpiAstNodeOp_Increment,
    kAcpiAstNodeOp_Decrement,
    kAcpiAstNodeOp_CondRefOf,
    kAcpiAstNodeOp_IndexField,
    kAcpiAstNodeOp_EisaId,
    kAcpiAstNodeOp_CreateField,
    kAcpiAstNodeOp_CreateBitField,
    kAcpiAstNodeOp_CreateByteField,
    kAcpiAstNodeOp_CreateWordField,
    kAcpiAstNodeOp_CreateDWordField,
    kAcpiAstNodeOp_CreateQWordField,
    kAcpiAstNodeOp_32Bit_Hack = 0x7fffffff
} RTACPIASTNODEOP;


/**
 * The core ACPI AST node.
 */
typedef struct RTACPIASTNODE
{
    /** List node. */
    RTLISTNODE              NdAst;
    /** The AML op defining the node. */
    RTACPIASTNODEOP         enmOp;
    /** Some additional flags. */
    uint32_t                fFlags;
    /** Operation dependent data. */
    union
    {
        /** List of other AST nodes for the opened scope if indicated by the AST flags (RTACPIASTNODE). */
        RTLISTANCHOR        LstScopeNodes;
        /** Pointer to the identifier if an identifier node. */
        const char          *pszIde;
        /** Pointer to the string literal if a string literal node. */
        const char          *pszStrLit;
        /** A number */
        uint64_t            u64;
        struct
        {
            /** Pointer to the field unit list - freed with RTMemFree() when the node is freed. */
            PRTACPIFIELDENTRY   paFields;
            /** Number of field entries. */
            uint32_t            cFields;
        } Fields;
        /** The resource template. */
        RTACPIRES           hAcpiRes;
    };
    /** Number of "arguments" for the opcode following (for example Scope(), Method(), If(), etc., i.e. anything requiring () after the keyword ). */
    uint8_t                 cArgs;
    /** Padding */
    uint8_t                 abRsvd[2];
    /** The AST node arguments - variable in size. */
    RTACPIASTARG            aArgs[1];
} RTACPIASTNODE;

/** Default flags. */
#define RTACPI_AST_NODE_F_DEFAULT       0
/** The AST node opens a new scope. */
#define RTACPI_AST_NODE_F_NEW_SCOPE     RT_BIT_32(0)
/** The AST node has an associated namespace entry. */
#define RTACPI_AST_NODE_F_NS_ENTRY      RT_BIT_32(1)


/** Pointer to an ACPI namespace entry. */
typedef struct RTACPINSENTRY *PRTACPINSENTRY;


/**
 * An ACPI namespace entry.
 */
typedef struct RTACPINSENTRY
{
    /** Node for the namespace list. */
    RTLISTNODE              NdNs;
    /** Pointer to the parent in the namespace, NULL if this is the root. */
    PRTACPINSENTRY          pParent;
    /** The name segment identifying the entry. */
    char                    achNameSeg[4];
    /** Flag whether the name space entry points to a node or just has some
     * integer data attached. */
    bool                    fAstNode;
    /** Type dependent data. */
    union
    {
        PCRTACPIASTNODE     pAstNd;
        uint64_t            u64Val;
    } u;
    /** List of namespace entries below this entry. */
    RTLISTANCHOR            LstNsEntries;
} RTACPINSENTRY;
/** Pointer to a const ACPI namespace entry. */
typedef const RTACPINSENTRY *PCRTACPINSENTRY;


/**
 * An ACPI namespace root
 */
typedef struct RTACPINSROOT
{
    /** Root namespace entry. */
    RTACPINSENTRY           RootEntry;
    /** Current top of the stack. */
    uint8_t                 idxNsStack;
    /** Stack of name space entries for navigation - 255 entries
     * is enough because a path name can only be encoded with 255 entries. */
    PRTACPINSENTRY          aNsStack[255];
} RTACPINSROOT;
/** Pointer to an ACPI namespace root. */
typedef RTACPINSROOT *PRTACPINSROOT;
/** Pointer to a const ACPI namespace root. */
typedef const RTACPINSROOT *PCRTACPINSROOT;


/**
 * Allocates a new ACPI AST node initialized with the given properties.
 *
 * @returns Pointer to the new ACPI AST node or NULL if out of memory.
 * @param   enmOp               The operation of the AST node.
 * @param   fFlags              Flags for this node.
 * @param   cArgs               Number of arguments to allocate.
 */
DECLHIDDEN(PRTACPIASTNODE) rtAcpiAstNodeAlloc(RTACPIASTNODEOP enmOp, uint32_t fFlags, uint8_t cArgs);


/**
 * Frees the given AST node and all children nodes linked to this one.
 *
 * @param   pAstNd              The AST node to free.
 */
DECLHIDDEN(void) rtAcpiAstNodeFree(PRTACPIASTNODE pAstNd);


/**
 * Does a few transformations on the given AST node and its children where required.
 *
 * @returns IPRT status.
 * @param   pAstNd              The AST node to transform.
 * @param   pErrInfo            Some additional error information on failure.
 *
 * @note This currently only implements merging if ... else ... nodes but can be extended to
 *       also do some optimizations and proper checking.
 */
DECLHIDDEN(int) rtAcpiAstNodeTransform(PRTACPIASTNODE pAstNd, PRTERRINFO pErrInfo);


/**
 * Creates a new namespace and returns the root.
 *
 * @returns Pointer to the namespace root or NULL if out of memory.
 */
DECLHIDDEN(PRTACPINSROOT) rtAcpiNsCreate(void);


/**
 * Destroys the given namespace, freeing all allocated resources,
 * including all namespace entries int it.
 *
 * @param   pNsRoot             The namespace root to destroy.
 */
DECLHIDDEN(void) rtAcpiNsDestroy(PRTACPINSROOT pNsRoot);


/**
 * Adds a new namespace entry to the given name space - AST node variant.
 *
 * @returns IPRT status code.
 * @param   pNsRoot             The namespace root to add the entry to.
 * @param   pszNameString       An ACPI NameString (either segment or path).
 * @param   pAstNd              The AST node to associate with the entry.
 * @param   fSwitchTo           Flag whether to switch the current point for the namespace to this entry.
 */
DECLHIDDEN(int) rtAcpiNsAddEntryAstNode(PRTACPINSROOT pNsRoot, const char *pszNameString, PCRTACPIASTNODE pAstNd, bool fSwitchTo);


/**
 * Adds a new namespace entry to the given name space - 64-bit value variant.
 *
 * @returns IPRT status code.
 * @param   pNsRoot             The namespace root to add the entry to.
 * @param   pszNameString       An ACPI NameString (either segment or path).
 * @param   u64Val              The 64-bit value to associate with the entry.
 * @param   fSwitchTo           Flag whether to switch the current point for the namespace to this entry.
 */
DECLHIDDEN(int) rtAcpiNsAddEntryU64(PRTACPINSROOT pNsRoot, const char *pszNameString, uint64_t u64Val, bool fSwitchTo);


/**
 * Adds a new namespace entry to the given name space - 64-bit value variant with format name string.
 *
 * @returns IPRT status code.
 * @param   pNsRoot             The namespace root to add the entry to.
 * @param   u64Val              The 64-bit value to associate with the entry.
 * @param   fSwitchTo           Flag whether to switch the current point for the namespace to this entry.
 * @param   pszNameStringFmt    An ACPI NameString (either segment or path) as a format string.
 * @param   ...                 The arguments for the format string.
 */
DECLHIDDEN(int) rtAcpiNsAddEntryU64F(PRTACPINSROOT pNsRoot, uint64_t u64Val, bool fSwitchTo, const char *pszNameStringFmt, ...); RT_IPRT_FORMAT_ATTR(4, 5);


/**
 * Adds a new namespace entry to the given name space - 64-bit value variant with format name string.
 *
 * @returns IPRT status code.
 * @param   pNsRoot             The namespace root to add the entry to.
 * @param   u64Val              The 64-bit value to associate with the entry.
 * @param   fSwitchTo           Flag whether to switch the current point for the namespace to this entry.
 * @param   pszNameStringFmt    An ACPI NameString (either segment or path) as a format string.
 * @param   va                  The format arguments.
 */
DECLHIDDEN(int) rtAcpiNsAddEntryU64V(PRTACPINSROOT pNsRoot, uint64_t u64Val, bool fSwitchTo, const char *pszNameStringFmt, va_list va); RT_IPRT_FORMAT_ATTR(4, 0);


/**
 * Pops the current name space entry from the stack and returns to the previous one.
 *
 * @returns IPRT status code.
 * @param   pNsRoot             The namespace root.
 */
DECLHIDDEN(int) rtAcpiNsPop(PRTACPINSROOT pNsRoot);


/**
 * Looks up the given name string and returns the namespace entry if found.
 *
 * @returns Pointer to the namespace entry or NULL if not found.
 * @param   pNsRoot             The namespace root.
 * @param   pszNameString       The ACPI NameString (either segment or path) to lookup.
 */
DECLHIDDEN(PCRTACPINSENTRY) rtAcpiNsLookup(PRTACPINSROOT pNsRoot, const char *pszNameString);


/**
 * Dumps the given AST node and everything it references to the given ACPI table.
 *
 * @returns IPRT status code.
 * @param   pAstNd              The AST node to dump.
 * @param   hAcpiTbl            The ACPI table to dump to.
 */
DECLHIDDEN(int) rtAcpiAstDumpToTbl(PCRTACPIASTNODE pAstNd, RTACPITBL hAcpiTbl);


/**
 * Worker for decompiling AML bytecode to the ASL source language.
 *
 * @returns IPRT status code.
 * @param   hVfsIosOut          The VFS I/O stream handle to output the result to.
 * @param   hVfsIosIn           The VFS I/O stream handle to read the ACPI table from.
 * @param   pErrInfo            Where to return additional error information.
 */
DECLHIDDEN(int) rtAcpiTblConvertFromAmlToAsl(RTVFSIOSTREAM hVfsIosOut, RTVFSIOSTREAM hVfsIosIn, PRTERRINFO pErrInfo);


/**
 * Worker for compiling ASL to the AML bytecode.
 *
 * @returns IPRT status code.
 * @param   hVfsIosOut          The VFS I/O stream handle to output the result to.
 * @param   hVfsIosIn           The VFS I/O stream handle to read the ACPI table from.
 * @param   pErrInfo            Where to return additional error information.
 */
DECLHIDDEN(int) rtAcpiTblConvertFromAslToAml(RTVFSIOSTREAM hVfsIosOut, RTVFSIOSTREAM hVfsIosIn, PRTERRINFO pErrInfo);


RT_C_DECLS_END

#endif /* !IPRT_INCLUDED_INTERNAL_acpi_h */

