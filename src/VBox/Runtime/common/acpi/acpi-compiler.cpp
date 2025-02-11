/* $Id$ */
/** @file
 * IPRT - Advanced Configuration and Power Interface (ACPI) Table generation API.
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


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#define LOG_GROUP RTLOGGROUP_ACPI
#include <iprt/acpi.h>
#include <iprt/asm.h>
#include <iprt/buildconfig.h>
#include <iprt/ctype.h>
#include <iprt/err.h>
#include <iprt/file.h>
#include <iprt/list.h>
#include <iprt/mem.h>
#include <iprt/script.h>
#include <iprt/string.h>
#include <iprt/uuid.h>

#include <iprt/formats/acpi-aml.h>
#include <iprt/formats/acpi-resources.h>

#include "internal/acpi.h"


/*********************************************************************************************************************************
*   Defined Constants And Macros                                                                                                 *
*********************************************************************************************************************************/


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/

/**
 * Terminals in the ACPI ASL lnaguage like keywords, operators and punctuators.
 */
typedef enum RTACPIASLTERMINAL
{
    RTACPIASLTERMINAL_INVALID = 2047,

    /** Miscelleanous keywords not appearing in the parser table. */
    RTACPIASLTERMINAL_KEYWORD_DEFINITION_BLOCK,
    RTACPIASLTERMINAL_KEYWORD_UNKNOWN_OBJ,
    RTACPIASLTERMINAL_KEYWORD_INT_OBJ,
    RTACPIASLTERMINAL_KEYWORD_STR_OBJ,
    RTACPIASLTERMINAL_KEYWORD_BUFF_OBJ,
    RTACPIASLTERMINAL_KEYWORD_PKG_OBJ,
    RTACPIASLTERMINAL_KEYWORD_FIELD_UNIT_OBJ,
    RTACPIASLTERMINAL_KEYWORD_DEVICE_OBJ,
    RTACPIASLTERMINAL_KEYWORD_EVENT_OBJ,
    RTACPIASLTERMINAL_KEYWORD_METHOD_OBJ,
    RTACPIASLTERMINAL_KEYWORD_MUTEX_OBJ,
    RTACPIASLTERMINAL_KEYWORD_OP_REGION_OBJ,
    RTACPIASLTERMINAL_KEYWORD_POWER_RES_OBJ,
    RTACPIASLTERMINAL_KEYWORD_THERMAL_ZONE_OBJ,
    RTACPIASLTERMINAL_KEYWORD_BUFF_FIELD_OBJ,
    RTACPIASLTERMINAL_KEYWORD_PROCESSOR_OBJ,
    RTACPIASLTERMINAL_KEYWORD_SERIALIZED,
    RTACPIASLTERMINAL_KEYWORD_NOT_SERIALIZED,
    RTACPIASLTERMINAL_KEYWORD_SYSTEM_IO,
    RTACPIASLTERMINAL_KEYWORD_SYSTEM_MEMORY,
    RTACPIASLTERMINAL_KEYWORD_PCI_CONFIG,
    RTACPIASLTERMINAL_KEYWORD_EMBEDDED_CONTROL,
    RTACPIASLTERMINAL_KEYWORD_SMBUS,
    RTACPIASLTERMINAL_KEYWORD_SYSTEM_CMOS,
    RTACPIASLTERMINAL_KEYWORD_PCI_BAR_TARGET,
    RTACPIASLTERMINAL_KEYWORD_IPMI,
    RTACPIASLTERMINAL_KEYWORD_GENERAL_PURPOSE_IO,
    RTACPIASLTERMINAL_KEYWORD_GENERIC_SERIAL_BUS,
    RTACPIASLTERMINAL_KEYWORD_PCC,
    RTACPIASLTERMINAL_KEYWORD_PRM,
    RTACPIASLTERMINAL_KEYWORD_FFIXED_HW,

    RTACPIASLTERMINAL_KEYWORD_ANY_ACC,
    RTACPIASLTERMINAL_KEYWORD_BYTE_ACC,
    RTACPIASLTERMINAL_KEYWORD_WORD_ACC,
    RTACPIASLTERMINAL_KEYWORD_DWORD_ACC,
    RTACPIASLTERMINAL_KEYWORD_QWORD_ACC,
    RTACPIASLTERMINAL_KEYWORD_BUFFER_ACC,

    RTACPIASLTERMINAL_KEYWORD_LOCK,
    RTACPIASLTERMINAL_KEYWORD_NO_LOCK,

    RTACPIASLTERMINAL_KEYWORD_PRESERVE,
    RTACPIASLTERMINAL_KEYWORD_WRITE_AS_ONES,
    RTACPIASLTERMINAL_KEYWORD_WRITE_AS_ZEROES,

    RTACPIASLTERMINAL_KEYWORD_OFFSET,

    RTACPIASLTERMINAL_KEYWORD_MEMORY32_FIXED,
    RTACPIASLTERMINAL_KEYWORD_READONLY,
    RTACPIASLTERMINAL_KEYWORD_READWRITE,

    RTACPIASLTERMINAL_KEYWORD_IRQ,
    RTACPIASLTERMINAL_KEYWORD_IRQ_NO_FLAGS,
    RTACPIASLTERMINAL_KEYWORD_EDGE,
    RTACPIASLTERMINAL_KEYWORD_LEVEL,
    RTACPIASLTERMINAL_KEYWORD_ACTIVE_HIGH,
    RTACPIASLTERMINAL_KEYWORD_ACTIVE_LOW,
    RTACPIASLTERMINAL_KEYWORD_SHARED,
    RTACPIASLTERMINAL_KEYWORD_EXCLUSIVE,
    RTACPIASLTERMINAL_KEYWORD_SHARED_AND_WAKE,
    RTACPIASLTERMINAL_KEYWORD_EXCLUSIVE_AND_WAKE,

    RTACPIASLTERMINAL_KEYWORD_IO,
    RTACPIASLTERMINAL_KEYWORD_DECODE_10,
    RTACPIASLTERMINAL_KEYWORD_DECODE_16,

    RTACPIASLTERMINAL_PUNCTUATOR_COMMA,
    RTACPIASLTERMINAL_PUNCTUATOR_OPEN_BRACKET,
    RTACPIASLTERMINAL_PUNCTUATOR_CLOSE_BRACKET,
    RTACPIASLTERMINAL_PUNCTUATOR_OPEN_CURLY_BRACKET,
    RTACPIASLTERMINAL_PUNCTUATOR_CLOSE_CURLY_BRACKET

} RTACPIASLTERMINAL;
/** Pointer to a terminal enum. */
typedef RTACPIASLTERMINAL *PRTACPIASLTERMINAL;
/** Pointer to a const terminal enum. */
typedef const RTACPIASLTERMINAL *PCRTACPIASLTERMINAL;


/**
 * The ACPI ASL compilation unit state.
 */
typedef struct RTACPIASLCU
{
    /** The lexer handle. */
    RTSCRIPTLEX             hLexSource;
    /** The VFS I/O input stream. */
    RTVFSIOSTREAM           hVfsIosIn;
    /** The ACPI table handle. */
    RTACPITBL               hAcpiTbl;
    /** Error information. */
    PRTERRINFO              pErrInfo;
    /** List of AST nodes for the DefinitionBlock() scope. */
    RTLISTANCHOR            LstStmts;
    /** The ACPI namespace. */
    PRTACPINSROOT           pNs;
} RTACPIASLCU;
/** Pointer to an ACPI ASL compilation unit state. */
typedef RTACPIASLCU *PRTACPIASLCU;
/** Pointer to a constant ACPI ASL compilation unit state. */
typedef const RTACPIASLCU *PCRTACPIASLCU;


/** Pointer to a const ASL keyword encoding entry. */
typedef const struct RTACPIASLKEYWORD *PCRTACPIASLKEYWORD;

/**
 * ACPI ASL -> AST parse callback.
 *
 * @returns IPRT status code.
 * @param   pThis               ACPI ASL compiler state.
 * @param   pKeyword            The keyword entry being processed.
 * @param   pAstNd              The AST node to initialize.
 */
typedef DECLCALLBACKTYPE(int, FNRTACPITBLASLPARSE,(PRTACPIASLCU pThis, PCRTACPIASLKEYWORD pKeyword, PRTACPIASTNODE pAstNd));
/** Pointer to a ACPI AML -> ASL decode callback. */
typedef FNRTACPITBLASLPARSE *PFNRTACPITBLASLPARSE;


/**
 * ASL keyword encoding entry.
 */
typedef struct RTACPIASLKEYWORD
{
    /** Name of the opcode. */
    const char               *pszOpc;
    /** The parsing callback to call, optional.
     * If not NULL this will have priority over the default parsing. */
    PFNRTACPITBLASLPARSE     pfnParse;
    /** Number of arguments required. */
    uint8_t                  cArgsReq;
    /** Number of optional arguments. */
    uint8_t                  cArgsOpt;
    /** Flags for the opcode. */
    uint32_t                 fFlags;
    /** Argument type for the required arguments. */
    RTACPIASTARGTYPE         aenmTypes[5];
    /** Arguments for optional arguments, including the default value if absent. */
    RTACPIASTARG             aArgsOpt[3];
} RTACPIASLKEYWORD;
/** Pointer to a ASL keyword encoding entry. */
typedef RTACPIASLKEYWORD *PRTACPIASLKEYWORD;


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/

static DECLCALLBACK(int) rtAcpiAslLexerParseNumber(RTSCRIPTLEX hScriptLex, char ch, PRTSCRIPTLEXTOKEN pToken, void *pvUser);
static DECLCALLBACK(int) rtAcpiAslLexerParseNameString(RTSCRIPTLEX hScriptLex, char ch, PRTSCRIPTLEXTOKEN pToken, void *pvUser);


static const char *s_aszSingleStart[] =
{
    "//",
    NULL
};


static const char *s_aszMultiStart[] =
{
    "/*",
    NULL
};


static const char *s_aszMultiEnd[] =
{
    "*/",
    NULL
};


static const RTSCRIPTLEXTOKMATCH s_aMatches[] =
{
    /* Keywords */
    { RT_STR_TUPLE("SCOPE"),                    RTSCRIPTLEXTOKTYPE_KEYWORD,    true,  kAcpiAstNodeOp_Scope                             },
    { RT_STR_TUPLE("PROCESSOR"),                RTSCRIPTLEXTOKTYPE_KEYWORD,    true,  kAcpiAstNodeOp_Processor                         },
    { RT_STR_TUPLE("EXTERNAL"),                 RTSCRIPTLEXTOKTYPE_KEYWORD,    true,  kAcpiAstNodeOp_External                          },
    { RT_STR_TUPLE("METHOD"),                   RTSCRIPTLEXTOKTYPE_KEYWORD,    true,  kAcpiAstNodeOp_Method                            },
    { RT_STR_TUPLE("DEVICE"),                   RTSCRIPTLEXTOKTYPE_KEYWORD,    true,  kAcpiAstNodeOp_Device                            },
    { RT_STR_TUPLE("IF"),                       RTSCRIPTLEXTOKTYPE_KEYWORD,    true,  kAcpiAstNodeOp_If                                },
    { RT_STR_TUPLE("ELSE"),                     RTSCRIPTLEXTOKTYPE_KEYWORD,    true,  kAcpiAstNodeOp_Else                              },
    { RT_STR_TUPLE("LAND"),                     RTSCRIPTLEXTOKTYPE_KEYWORD,    true,  kAcpiAstNodeOp_LAnd                              },
    { RT_STR_TUPLE("LEQUAL"),                   RTSCRIPTLEXTOKTYPE_KEYWORD,    true,  kAcpiAstNodeOp_LEqual                            },
    { RT_STR_TUPLE("LGREATER"),                 RTSCRIPTLEXTOKTYPE_KEYWORD,    true,  kAcpiAstNodeOp_LGreater                          },
    { RT_STR_TUPLE("LGREATEREQUAL"),            RTSCRIPTLEXTOKTYPE_KEYWORD,    true,  kAcpiAstNodeOp_LGreaterEqual                     },
    { RT_STR_TUPLE("LLESS"),                    RTSCRIPTLEXTOKTYPE_KEYWORD,    true,  kAcpiAstNodeOp_LLess                             },
    { RT_STR_TUPLE("LLESSEQUAL"),               RTSCRIPTLEXTOKTYPE_KEYWORD,    true,  kAcpiAstNodeOp_LLessEqual                        },
    { RT_STR_TUPLE("LNOT"),                     RTSCRIPTLEXTOKTYPE_KEYWORD,    true,  kAcpiAstNodeOp_LNot                              },
    { RT_STR_TUPLE("LNOTEQUAL"),                RTSCRIPTLEXTOKTYPE_KEYWORD,    true,  kAcpiAstNodeOp_LNotEqual                         },
    { RT_STR_TUPLE("ZERO"),                     RTSCRIPTLEXTOKTYPE_KEYWORD,    true,  kAcpiAstNodeOp_Zero                              },
    { RT_STR_TUPLE("ONE"),                      RTSCRIPTLEXTOKTYPE_KEYWORD,    true,  kAcpiAstNodeOp_One                               },
    { RT_STR_TUPLE("ONES"),                     RTSCRIPTLEXTOKTYPE_KEYWORD,    true,  kAcpiAstNodeOp_Ones                              },
    { RT_STR_TUPLE("RETURN"),                   RTSCRIPTLEXTOKTYPE_KEYWORD,    true,  kAcpiAstNodeOp_Return                            },
    { RT_STR_TUPLE("UNICODE"),                  RTSCRIPTLEXTOKTYPE_KEYWORD,    true,  kAcpiAstNodeOp_Unicode                           },
    { RT_STR_TUPLE("OPERATIONREGION"),          RTSCRIPTLEXTOKTYPE_KEYWORD,    true,  kAcpiAstNodeOp_OperationRegion                   },
    { RT_STR_TUPLE("FIELD"),                    RTSCRIPTLEXTOKTYPE_KEYWORD,    true,  kAcpiAstNodeOp_Field                             },
    { RT_STR_TUPLE("NAME"),                     RTSCRIPTLEXTOKTYPE_KEYWORD,    true,  kAcpiAstNodeOp_Name                              },
    { RT_STR_TUPLE("RESOURCETEMPLATE"),         RTSCRIPTLEXTOKTYPE_KEYWORD,    true,  kAcpiAstNodeOp_ResourceTemplate                  },
    { RT_STR_TUPLE("ARG0"),                     RTSCRIPTLEXTOKTYPE_KEYWORD,    true,  kAcpiAstNodeOp_Arg0                              },
    { RT_STR_TUPLE("ARG1"),                     RTSCRIPTLEXTOKTYPE_KEYWORD,    true,  kAcpiAstNodeOp_Arg1                              },
    { RT_STR_TUPLE("ARG2"),                     RTSCRIPTLEXTOKTYPE_KEYWORD,    true,  kAcpiAstNodeOp_Arg2                              },
    { RT_STR_TUPLE("ARG3"),                     RTSCRIPTLEXTOKTYPE_KEYWORD,    true,  kAcpiAstNodeOp_Arg3                              },
    { RT_STR_TUPLE("ARG4"),                     RTSCRIPTLEXTOKTYPE_KEYWORD,    true,  kAcpiAstNodeOp_Arg4                              },
    { RT_STR_TUPLE("ARG5"),                     RTSCRIPTLEXTOKTYPE_KEYWORD,    true,  kAcpiAstNodeOp_Arg5                              },
    { RT_STR_TUPLE("ARG6"),                     RTSCRIPTLEXTOKTYPE_KEYWORD,    true,  kAcpiAstNodeOp_Arg6                              },
    { RT_STR_TUPLE("LOCAL0"),                   RTSCRIPTLEXTOKTYPE_KEYWORD,    true,  kAcpiAstNodeOp_Local0                            },
    { RT_STR_TUPLE("LOCAL1"),                   RTSCRIPTLEXTOKTYPE_KEYWORD,    true,  kAcpiAstNodeOp_Local1                            },
    { RT_STR_TUPLE("LOCAL2"),                   RTSCRIPTLEXTOKTYPE_KEYWORD,    true,  kAcpiAstNodeOp_Local2                            },
    { RT_STR_TUPLE("LOCAL3"),                   RTSCRIPTLEXTOKTYPE_KEYWORD,    true,  kAcpiAstNodeOp_Local3                            },
    { RT_STR_TUPLE("LOCAL4"),                   RTSCRIPTLEXTOKTYPE_KEYWORD,    true,  kAcpiAstNodeOp_Local4                            },
    { RT_STR_TUPLE("LOCAL5"),                   RTSCRIPTLEXTOKTYPE_KEYWORD,    true,  kAcpiAstNodeOp_Local5                            },
    { RT_STR_TUPLE("LOCAL6"),                   RTSCRIPTLEXTOKTYPE_KEYWORD,    true,  kAcpiAstNodeOp_Local6                            },
    { RT_STR_TUPLE("LOCAL7"),                   RTSCRIPTLEXTOKTYPE_KEYWORD,    true,  kAcpiAstNodeOp_Local7                            },
    { RT_STR_TUPLE("PACKAGE"),                  RTSCRIPTLEXTOKTYPE_KEYWORD,    true,  kAcpiAstNodeOp_Package                           },
    { RT_STR_TUPLE("BUFFER"),                   RTSCRIPTLEXTOKTYPE_KEYWORD,    true,  kAcpiAstNodeOp_Buffer                            },
    { RT_STR_TUPLE("TOUUID"),                   RTSCRIPTLEXTOKTYPE_KEYWORD,    true,  kAcpiAstNodeOp_ToUuid                            },
    { RT_STR_TUPLE("DEREFOF"),                  RTSCRIPTLEXTOKTYPE_KEYWORD,    true,  kAcpiAstNodeOp_DerefOf                           },
    { RT_STR_TUPLE("INDEX"),                    RTSCRIPTLEXTOKTYPE_KEYWORD,    true,  kAcpiAstNodeOp_Index                             },
    { RT_STR_TUPLE("STORE"),                    RTSCRIPTLEXTOKTYPE_KEYWORD,    true,  kAcpiAstNodeOp_Store                             },
    { RT_STR_TUPLE("BREAK"),                    RTSCRIPTLEXTOKTYPE_KEYWORD,    true,  kAcpiAstNodeOp_Break                             },
    { RT_STR_TUPLE("CONTINUE"),                 RTSCRIPTLEXTOKTYPE_KEYWORD,    true,  kAcpiAstNodeOp_Continue                          },
    { RT_STR_TUPLE("ADD"),                      RTSCRIPTLEXTOKTYPE_KEYWORD,    true,  kAcpiAstNodeOp_Add                               },
    { RT_STR_TUPLE("SUBTRACT"),                 RTSCRIPTLEXTOKTYPE_KEYWORD,    true,  kAcpiAstNodeOp_Subtract                          },
    { RT_STR_TUPLE("AND"),                      RTSCRIPTLEXTOKTYPE_KEYWORD,    true,  kAcpiAstNodeOp_And                               },
    { RT_STR_TUPLE("NAND"),                     RTSCRIPTLEXTOKTYPE_KEYWORD,    true,  kAcpiAstNodeOp_Nand                              },
    { RT_STR_TUPLE("OR"),                       RTSCRIPTLEXTOKTYPE_KEYWORD,    true,  kAcpiAstNodeOp_Or                                },
    { RT_STR_TUPLE("XOR"),                      RTSCRIPTLEXTOKTYPE_KEYWORD,    true,  kAcpiAstNodeOp_Xor                               },
    { RT_STR_TUPLE("NOT"),                      RTSCRIPTLEXTOKTYPE_KEYWORD,    true,  kAcpiAstNodeOp_Not                               },
    { RT_STR_TUPLE("NOTIFY"),                   RTSCRIPTLEXTOKTYPE_KEYWORD,    true,  kAcpiAstNodeOp_Notify                            },
    { RT_STR_TUPLE("SIZEOF"),                   RTSCRIPTLEXTOKTYPE_KEYWORD,    true,  kAcpiAstNodeOp_SizeOf                            },
    { RT_STR_TUPLE("WHILE"),                    RTSCRIPTLEXTOKTYPE_KEYWORD,    true,  kAcpiAstNodeOp_While                             },
    { RT_STR_TUPLE("INCREMENT"),                RTSCRIPTLEXTOKTYPE_KEYWORD,    true,  kAcpiAstNodeOp_Increment                         },
    { RT_STR_TUPLE("DECREMENT"),                RTSCRIPTLEXTOKTYPE_KEYWORD,    true,  kAcpiAstNodeOp_Decrement                         },
    { RT_STR_TUPLE("CONDREFOF"),                RTSCRIPTLEXTOKTYPE_KEYWORD,    true,  kAcpiAstNodeOp_CondRefOf                         },
    { RT_STR_TUPLE("INDEXFIELD"),               RTSCRIPTLEXTOKTYPE_KEYWORD,    true,  kAcpiAstNodeOp_IndexField                        },
    { RT_STR_TUPLE("EISAID"),                   RTSCRIPTLEXTOKTYPE_KEYWORD,    true,  kAcpiAstNodeOp_EisaId                            },
    { RT_STR_TUPLE("CREATEFIELD"),              RTSCRIPTLEXTOKTYPE_KEYWORD,    true,  kAcpiAstNodeOp_CreateField                       },
    { RT_STR_TUPLE("CREATEBITFIELD"),           RTSCRIPTLEXTOKTYPE_KEYWORD,    true,  kAcpiAstNodeOp_CreateBitField                    },
    { RT_STR_TUPLE("CREATEBYTEFIELD"),          RTSCRIPTLEXTOKTYPE_KEYWORD,    true,  kAcpiAstNodeOp_CreateByteField                   },
    { RT_STR_TUPLE("CREATEWORDFIELD"),          RTSCRIPTLEXTOKTYPE_KEYWORD,    true,  kAcpiAstNodeOp_CreateWordField                   },
    { RT_STR_TUPLE("CREATEDWORDFIELD"),         RTSCRIPTLEXTOKTYPE_KEYWORD,    true,  kAcpiAstNodeOp_CreateDWordField                  },
    { RT_STR_TUPLE("CREATEQWORDFIELD"),         RTSCRIPTLEXTOKTYPE_KEYWORD,    true,  kAcpiAstNodeOp_CreateQWordField                  },

    /* Keywords not in the operation parser table. */
    { RT_STR_TUPLE("DEFINITIONBLOCK"),          RTSCRIPTLEXTOKTYPE_KEYWORD,    true,  RTACPIASLTERMINAL_KEYWORD_DEFINITION_BLOCK       },
    { RT_STR_TUPLE("UNKNOWNOBJ"),               RTSCRIPTLEXTOKTYPE_KEYWORD,    true,  RTACPIASLTERMINAL_KEYWORD_UNKNOWN_OBJ            },
    { RT_STR_TUPLE("INTOBJ"),                   RTSCRIPTLEXTOKTYPE_KEYWORD,    true,  RTACPIASLTERMINAL_KEYWORD_INT_OBJ                },
    { RT_STR_TUPLE("STROBJ"),                   RTSCRIPTLEXTOKTYPE_KEYWORD,    true,  RTACPIASLTERMINAL_KEYWORD_STR_OBJ                },
    { RT_STR_TUPLE("BUFFOBJ"),                  RTSCRIPTLEXTOKTYPE_KEYWORD,    true,  RTACPIASLTERMINAL_KEYWORD_BUFF_OBJ               },
    { RT_STR_TUPLE("PKGOBJ"),                   RTSCRIPTLEXTOKTYPE_KEYWORD,    true,  RTACPIASLTERMINAL_KEYWORD_PKG_OBJ                },
    { RT_STR_TUPLE("FIELDUNITOBJ"),             RTSCRIPTLEXTOKTYPE_KEYWORD,    true,  RTACPIASLTERMINAL_KEYWORD_FIELD_UNIT_OBJ         },
    { RT_STR_TUPLE("DEVICEOBJ"),                RTSCRIPTLEXTOKTYPE_KEYWORD,    true,  RTACPIASLTERMINAL_KEYWORD_DEVICE_OBJ             },
    { RT_STR_TUPLE("EVENTOBJ"),                 RTSCRIPTLEXTOKTYPE_KEYWORD,    true,  RTACPIASLTERMINAL_KEYWORD_EVENT_OBJ              },
    { RT_STR_TUPLE("METHODOBJ"),                RTSCRIPTLEXTOKTYPE_KEYWORD,    true,  RTACPIASLTERMINAL_KEYWORD_METHOD_OBJ             },
    { RT_STR_TUPLE("MUTEXOBJ"),                 RTSCRIPTLEXTOKTYPE_KEYWORD,    true,  RTACPIASLTERMINAL_KEYWORD_MUTEX_OBJ              },
    { RT_STR_TUPLE("OPREGIONOBJ"),              RTSCRIPTLEXTOKTYPE_KEYWORD,    true,  RTACPIASLTERMINAL_KEYWORD_OP_REGION_OBJ          },
    { RT_STR_TUPLE("POWERRESOBJ"),              RTSCRIPTLEXTOKTYPE_KEYWORD,    true,  RTACPIASLTERMINAL_KEYWORD_POWER_RES_OBJ          },
    { RT_STR_TUPLE("THERMALZONEOBJ"),           RTSCRIPTLEXTOKTYPE_KEYWORD,    true,  RTACPIASLTERMINAL_KEYWORD_THERMAL_ZONE_OBJ       },
    { RT_STR_TUPLE("BUFFFIELDOBJ"),             RTSCRIPTLEXTOKTYPE_KEYWORD,    true,  RTACPIASLTERMINAL_KEYWORD_BUFF_FIELD_OBJ         },
    { RT_STR_TUPLE("PROCESSOROBJ"),             RTSCRIPTLEXTOKTYPE_KEYWORD,    true,  RTACPIASLTERMINAL_KEYWORD_PROCESSOR_OBJ          },

    { RT_STR_TUPLE("SERIALIZED"),               RTSCRIPTLEXTOKTYPE_KEYWORD,    true,  RTACPIASLTERMINAL_KEYWORD_SERIALIZED             },
    { RT_STR_TUPLE("NOTSERIALIZED"),            RTSCRIPTLEXTOKTYPE_KEYWORD,    true,  RTACPIASLTERMINAL_KEYWORD_NOT_SERIALIZED         },

    { RT_STR_TUPLE("SYSTEMIO"),                 RTSCRIPTLEXTOKTYPE_KEYWORD,    true,  RTACPIASLTERMINAL_KEYWORD_SYSTEM_IO              },
    { RT_STR_TUPLE("SYSTEMMEMORY"),             RTSCRIPTLEXTOKTYPE_KEYWORD,    true,  RTACPIASLTERMINAL_KEYWORD_SYSTEM_MEMORY          },
    { RT_STR_TUPLE("PCI_CONFIG"),               RTSCRIPTLEXTOKTYPE_KEYWORD,    true,  RTACPIASLTERMINAL_KEYWORD_PCI_CONFIG             },
    { RT_STR_TUPLE("EMBEDDEDCONTROL"),          RTSCRIPTLEXTOKTYPE_KEYWORD,    true,  RTACPIASLTERMINAL_KEYWORD_EMBEDDED_CONTROL       },
    { RT_STR_TUPLE("SMBUS"),                    RTSCRIPTLEXTOKTYPE_KEYWORD,    true,  RTACPIASLTERMINAL_KEYWORD_SMBUS                  },
    { RT_STR_TUPLE("SYSTEMCMOS"),               RTSCRIPTLEXTOKTYPE_KEYWORD,    true,  RTACPIASLTERMINAL_KEYWORD_SYSTEM_CMOS            },
    { RT_STR_TUPLE("PCIBARTARGET"),             RTSCRIPTLEXTOKTYPE_KEYWORD,    true,  RTACPIASLTERMINAL_KEYWORD_PCI_BAR_TARGET         },
    { RT_STR_TUPLE("IPMI"),                     RTSCRIPTLEXTOKTYPE_KEYWORD,    true,  RTACPIASLTERMINAL_KEYWORD_IPMI                   },
    { RT_STR_TUPLE("GENERALPURPOSEIO"),         RTSCRIPTLEXTOKTYPE_KEYWORD,    true,  RTACPIASLTERMINAL_KEYWORD_GENERAL_PURPOSE_IO     },
    { RT_STR_TUPLE("GENERICSERIALBUS"),         RTSCRIPTLEXTOKTYPE_KEYWORD,    true,  RTACPIASLTERMINAL_KEYWORD_GENERIC_SERIAL_BUS     },
    { RT_STR_TUPLE("PCC"),                      RTSCRIPTLEXTOKTYPE_KEYWORD,    true,  RTACPIASLTERMINAL_KEYWORD_PCC                    },
    { RT_STR_TUPLE("PRM"),                      RTSCRIPTLEXTOKTYPE_KEYWORD,    true,  RTACPIASLTERMINAL_KEYWORD_PRM                    },
    { RT_STR_TUPLE("FFIXEDHW"),                 RTSCRIPTLEXTOKTYPE_KEYWORD,    true,  RTACPIASLTERMINAL_KEYWORD_FFIXED_HW              },

    { RT_STR_TUPLE("ANYACC"),                   RTSCRIPTLEXTOKTYPE_KEYWORD,    true,  RTACPIASLTERMINAL_KEYWORD_ANY_ACC                },
    { RT_STR_TUPLE("BYTEACC"),                  RTSCRIPTLEXTOKTYPE_KEYWORD,    true,  RTACPIASLTERMINAL_KEYWORD_BYTE_ACC               },
    { RT_STR_TUPLE("WORDACC"),                  RTSCRIPTLEXTOKTYPE_KEYWORD,    true,  RTACPIASLTERMINAL_KEYWORD_WORD_ACC               },
    { RT_STR_TUPLE("DWORDACC"),                 RTSCRIPTLEXTOKTYPE_KEYWORD,    true,  RTACPIASLTERMINAL_KEYWORD_DWORD_ACC              },
    { RT_STR_TUPLE("QWORDACC"),                 RTSCRIPTLEXTOKTYPE_KEYWORD,    true,  RTACPIASLTERMINAL_KEYWORD_QWORD_ACC              },
    { RT_STR_TUPLE("BUFFERACC"),                RTSCRIPTLEXTOKTYPE_KEYWORD,    true,  RTACPIASLTERMINAL_KEYWORD_BUFFER_ACC             },

    { RT_STR_TUPLE("LOCK"),                     RTSCRIPTLEXTOKTYPE_KEYWORD,    true,  RTACPIASLTERMINAL_KEYWORD_LOCK                   },
    { RT_STR_TUPLE("NOLOCK"),                   RTSCRIPTLEXTOKTYPE_KEYWORD,    true,  RTACPIASLTERMINAL_KEYWORD_NO_LOCK                },

    { RT_STR_TUPLE("PRESERVE"),                 RTSCRIPTLEXTOKTYPE_KEYWORD,    true,  RTACPIASLTERMINAL_KEYWORD_PRESERVE               },
    { RT_STR_TUPLE("WRITEASONES"),              RTSCRIPTLEXTOKTYPE_KEYWORD,    true,  RTACPIASLTERMINAL_KEYWORD_WRITE_AS_ONES          },
    { RT_STR_TUPLE("WRITEASZEROS"),             RTSCRIPTLEXTOKTYPE_KEYWORD,    true,  RTACPIASLTERMINAL_KEYWORD_WRITE_AS_ZEROES        },

    { RT_STR_TUPLE("OFFSET"),                   RTSCRIPTLEXTOKTYPE_KEYWORD,    true,  RTACPIASLTERMINAL_KEYWORD_OFFSET                 },
    { RT_STR_TUPLE("MEMORY32FIXED"),            RTSCRIPTLEXTOKTYPE_KEYWORD,    true,  RTACPIASLTERMINAL_KEYWORD_MEMORY32_FIXED         },
    { RT_STR_TUPLE("READONLY"),                 RTSCRIPTLEXTOKTYPE_KEYWORD,    true,  RTACPIASLTERMINAL_KEYWORD_READONLY               },
    { RT_STR_TUPLE("READWRITE"),                RTSCRIPTLEXTOKTYPE_KEYWORD,    true,  RTACPIASLTERMINAL_KEYWORD_READWRITE              },

    { RT_STR_TUPLE("IRQ"),                      RTSCRIPTLEXTOKTYPE_KEYWORD,    true,  RTACPIASLTERMINAL_KEYWORD_IRQ                    },
    { RT_STR_TUPLE("IRQNOFLAGS"),               RTSCRIPTLEXTOKTYPE_KEYWORD,    true,  RTACPIASLTERMINAL_KEYWORD_IRQ_NO_FLAGS           },
    { RT_STR_TUPLE("EDGE"),                     RTSCRIPTLEXTOKTYPE_KEYWORD,    true,  RTACPIASLTERMINAL_KEYWORD_EDGE                   },
    { RT_STR_TUPLE("LEVEL"),                    RTSCRIPTLEXTOKTYPE_KEYWORD,    true,  RTACPIASLTERMINAL_KEYWORD_LEVEL                  },
    { RT_STR_TUPLE("ACTIVEHIGH"),               RTSCRIPTLEXTOKTYPE_KEYWORD,    true,  RTACPIASLTERMINAL_KEYWORD_ACTIVE_HIGH            },
    { RT_STR_TUPLE("ACTIVELOW"),                RTSCRIPTLEXTOKTYPE_KEYWORD,    true,  RTACPIASLTERMINAL_KEYWORD_ACTIVE_LOW             },
    { RT_STR_TUPLE("SHARED"),                   RTSCRIPTLEXTOKTYPE_KEYWORD,    true,  RTACPIASLTERMINAL_KEYWORD_SHARED                 },
    { RT_STR_TUPLE("EXCLUSIVE"),                RTSCRIPTLEXTOKTYPE_KEYWORD,    true,  RTACPIASLTERMINAL_KEYWORD_EXCLUSIVE              },
    { RT_STR_TUPLE("SHAREDANDWAKE"),            RTSCRIPTLEXTOKTYPE_KEYWORD,    true,  RTACPIASLTERMINAL_KEYWORD_SHARED_AND_WAKE        },
    { RT_STR_TUPLE("EXCLUSIVEANDWAKE"),         RTSCRIPTLEXTOKTYPE_KEYWORD,    true,  RTACPIASLTERMINAL_KEYWORD_EXCLUSIVE_AND_WAKE     },

    { RT_STR_TUPLE("IO"),                       RTSCRIPTLEXTOKTYPE_KEYWORD,    true,  RTACPIASLTERMINAL_KEYWORD_IO                     },
    { RT_STR_TUPLE("DECODE10"),                 RTSCRIPTLEXTOKTYPE_KEYWORD,    true,  RTACPIASLTERMINAL_KEYWORD_DECODE_10              },
    { RT_STR_TUPLE("DECODE16"),                 RTSCRIPTLEXTOKTYPE_KEYWORD,    true,  RTACPIASLTERMINAL_KEYWORD_DECODE_16              },


    /* Punctuators */
    { RT_STR_TUPLE(","),                        RTSCRIPTLEXTOKTYPE_PUNCTUATOR, false, RTACPIASLTERMINAL_PUNCTUATOR_COMMA               },
    { RT_STR_TUPLE("("),                        RTSCRIPTLEXTOKTYPE_PUNCTUATOR, false, RTACPIASLTERMINAL_PUNCTUATOR_OPEN_BRACKET        },
    { RT_STR_TUPLE(")"),                        RTSCRIPTLEXTOKTYPE_PUNCTUATOR, false, RTACPIASLTERMINAL_PUNCTUATOR_CLOSE_BRACKET       },
    { RT_STR_TUPLE("{"),                        RTSCRIPTLEXTOKTYPE_PUNCTUATOR, false, RTACPIASLTERMINAL_PUNCTUATOR_OPEN_CURLY_BRACKET  },
    { RT_STR_TUPLE("}"),                        RTSCRIPTLEXTOKTYPE_PUNCTUATOR, false, RTACPIASLTERMINAL_PUNCTUATOR_CLOSE_CURLY_BRACKET },
    { NULL, 0,                                  RTSCRIPTLEXTOKTYPE_INVALID,    false, 0 }
};


static const RTSCRIPTLEXRULE s_aRules[] =
{
    { '\"', '\"',  RTSCRIPT_LEX_RULE_CONSUME, RTScriptLexScanStringLiteralC,      NULL},
    { '0',  '9',   RTSCRIPT_LEX_RULE_DEFAULT, rtAcpiAslLexerParseNumber,          NULL},
    { 'A',  'Z',   RTSCRIPT_LEX_RULE_DEFAULT, rtAcpiAslLexerParseNameString,      NULL},
    { '_',  '_',   RTSCRIPT_LEX_RULE_DEFAULT, rtAcpiAslLexerParseNameString,      NULL},
    { '^',  '^',   RTSCRIPT_LEX_RULE_DEFAULT, rtAcpiAslLexerParseNameString,      NULL},
    { '\\',  '\\', RTSCRIPT_LEX_RULE_DEFAULT, rtAcpiAslLexerParseNameString,      NULL},

    { '\0', '\0',  RTSCRIPT_LEX_RULE_DEFAULT, NULL,                               NULL}
};


static const RTSCRIPTLEXCFG s_AslLexCfg =
{
    /** pszName */
    "AcpiAsl",
    /** pszDesc */
    "ACPI ASL lexer",
    /** fFlags */
    RTSCRIPT_LEX_CFG_F_CASE_INSENSITIVE_UPPER,
    /** pszWhitespace */
    NULL,
    /** pszNewline */
    NULL,
    /** papszCommentMultiStart */
    s_aszMultiStart,
    /** papszCommentMultiEnd */
    s_aszMultiEnd,
    /** papszCommentSingleStart */
    s_aszSingleStart,
    /** paTokMatches */
    s_aMatches,
    /** paRules */
    s_aRules,
    /** pfnProdDef */
    NULL,
    /** pfnProdDefUser */
    NULL
};


/*********************************************************************************************************************************
*   Internal Functions                                                                                                           *
*********************************************************************************************************************************/

static DECLCALLBACK(int) rtAcpiAslLexerParseNumber(RTSCRIPTLEX hScriptLex, char ch, PRTSCRIPTLEXTOKEN pToken, void *pvUser)
{
    RT_NOREF(ch, pvUser);
    return RTScriptLexScanNumber(hScriptLex, 0 /*uBase*/, false /*fAllowReal*/, pToken);
}


static int rtAcpiAslLexerParseNameSeg(RTSCRIPTLEX hScriptLex, PRTSCRIPTLEXTOKEN pTok, char *pachNameSeg)
{
    /*
     * A Nameseg consist of a lead character and up to 3 following characters A-Z, 0-9 or _.
     * If the name segment is not 4 characters long the remainder is filled with _.
     */
    char ch = RTScriptLexGetCh(hScriptLex);
    if (   ch != '_'
        && (  ch < 'A'
            || ch > 'Z'))
        return RTScriptLexProduceTokError(hScriptLex, pTok, VERR_INVALID_PARAMETER, "Lexer: Name segment starts with invalid character '%c'", ch);
    RTScriptLexConsumeCh(hScriptLex);

    /* Initialize the default name segment. */
    pachNameSeg[0] = ch;
    pachNameSeg[1] = '_';
    pachNameSeg[2] = '_';
    pachNameSeg[3] = '_';

    for (uint8_t i = 1; i < 4; i++)
    {
        ch = RTScriptLexGetCh(hScriptLex);

        /* Anything not belonging to the allowed characters terminates the parsing. */
        if (   ch != '_'
            && (  ch < 'A'
                || ch > 'Z')
            && (  ch < '0'
                || ch > '9'))
            return VINF_SUCCESS;
        RTScriptLexConsumeCh(hScriptLex);
        pachNameSeg[i] = ch;
    }

    return VINF_SUCCESS;
}

static DECLCALLBACK(int) rtAcpiAslLexerParseNameString(RTSCRIPTLEX hScriptLex, char ch,
                                                       PRTSCRIPTLEXTOKEN pTok, void *pvUser)
{
    RT_NOREF(pvUser);

    char aszIde[513]; RT_ZERO(aszIde);
    unsigned idx = 0;

    if (ch == '^') /* PrefixPath */
    {
        aszIde[idx++] = '^';
        RTScriptLexConsumeCh(hScriptLex);

        ch = RTScriptLexGetCh(hScriptLex);
        while (   idx < sizeof(aszIde) - 1
               && ch == '^')
        {
            RTScriptLexConsumeCh(hScriptLex);
            aszIde[idx++] = ch;
            ch = RTScriptLexGetCh(hScriptLex);
        }

        if (idx == sizeof(aszIde) - 1)
            return RTScriptLexProduceTokError(hScriptLex, pTok, VERR_BUFFER_OVERFLOW, "Lexer: PrefixPath exceeds the allowed length");
    }
    else if (ch == '\\')
    {
        aszIde[idx++] = '\\';
        RTScriptLexConsumeCh(hScriptLex);
    }

    /* Now there is only a sequence of NameSeg allowed (separated by the . separator). */
    while (idx < sizeof(aszIde) - 1 - 4)
    {
        char achNameSeg[4];
        int rc = rtAcpiAslLexerParseNameSeg(hScriptLex, pTok, &achNameSeg[0]);
        if (RT_FAILURE(rc))
            return rc;

        aszIde[idx++] = achNameSeg[0];
        aszIde[idx++] = achNameSeg[1];
        aszIde[idx++] = achNameSeg[2];
        aszIde[idx++] = achNameSeg[3];

        ch = RTScriptLexGetCh(hScriptLex);
        if (ch != '.')
            break;
        aszIde[idx++] = '.';
        RTScriptLexConsumeCh(hScriptLex);
    }

    if (idx == sizeof(aszIde) - 1)
        return RTScriptLexProduceTokError(hScriptLex, pTok, VERR_BUFFER_OVERFLOW, "Lexer: Identifier exceeds the allowed length");

    return RTScriptLexProduceTokIde(hScriptLex, pTok, &aszIde[0], idx);
}


static DECLCALLBACK(int) rtAcpiAslLexerRead(RTSCRIPTLEX hScriptLex, size_t offBuf, char *pchCur,
                                            size_t cchBuf, size_t *pcchRead, void *pvUser)
{
    PCRTACPIASLCU pThis = (PCRTACPIASLCU)pvUser;
    RT_NOREF(hScriptLex, offBuf);

    size_t cbRead = 0;
    int rc = RTVfsIoStrmRead(pThis->hVfsIosIn, pchCur, cchBuf, true /*fBlocking*/, &cbRead);
    if (RT_FAILURE(rc))
        return rc;

    *pcchRead = cbRead * sizeof(char);
    if (!cbRead)
        return VINF_EOF;

    return VINF_SUCCESS;
}


DECLINLINE(bool) rtAcpiAslLexerIsPunctuator(PCRTACPIASLCU pThis, RTACPIASLTERMINAL enmTerm)
{
    PCRTSCRIPTLEXTOKEN pTok;
    int rc = RTScriptLexQueryToken(pThis->hLexSource, &pTok);
    if (RT_FAILURE(rc))
        return false;

    if (   pTok->enmType == RTSCRIPTLEXTOKTYPE_PUNCTUATOR
        && pTok->Type.Keyword.pKeyword->u64Val == (uint64_t)enmTerm)
        return true;

    return false;
}


static int rtAcpiAslLexerConsumeIfKeywordInList(PCRTACPIASLCU pThis, PCRTACPIASLTERMINAL paenmTerms, PRTACPIASLTERMINAL penmTerm)
{
    PCRTSCRIPTLEXTOKEN pTok;
    int rc = RTScriptLexQueryToken(pThis->hLexSource, &pTok);
    if (RT_FAILURE(rc))
        return RTErrInfoSetF(pThis->pErrInfo, rc, "Lexer: Failed to query keyword token with %Rrc", rc);

    if (pTok->enmType == RTSCRIPTLEXTOKTYPE_KEYWORD)
    {
        unsigned i = 0;
        do
        {
            if (pTok->Type.Keyword.pKeyword->u64Val == (uint64_t)paenmTerms[i])
            {
                RTScriptLexConsumeToken(pThis->hLexSource);
                *penmTerm = paenmTerms[i];
                return VINF_SUCCESS;
            }

            i++;
        } while (paenmTerms[i] != RTACPIASLTERMINAL_INVALID);
    }

    *penmTerm = RTACPIASLTERMINAL_INVALID;
    return VINF_SUCCESS;
}


static int rtAcpiAslLexerConsumeIfKeyword(PCRTACPIASLCU pThis, RTACPIASLTERMINAL enmTerm, bool *pfConsumed)
{
    PCRTSCRIPTLEXTOKEN pTok;
    int rc = RTScriptLexQueryToken(pThis->hLexSource, &pTok);
    if (RT_FAILURE(rc))
        return RTErrInfoSetF(pThis->pErrInfo, rc, "Lexer: Failed to query keyword token with %Rrc", rc);

    if (   pTok->enmType == RTSCRIPTLEXTOKTYPE_KEYWORD
        && pTok->Type.Keyword.pKeyword->u64Val == (uint64_t)enmTerm)
    {
        RTScriptLexConsumeToken(pThis->hLexSource);
        *pfConsumed = true;
        return VINF_SUCCESS;
    }

    *pfConsumed = false;
    return VINF_SUCCESS;
}


static int rtAcpiAslLexerConsumeIfPunctuator(PCRTACPIASLCU pThis, RTACPIASLTERMINAL enmTerm, bool *pfConsumed)
{
    PCRTSCRIPTLEXTOKEN pTok;
    int rc = RTScriptLexQueryToken(pThis->hLexSource, &pTok);
    if (RT_FAILURE(rc))
        return RTErrInfoSetF(pThis->pErrInfo, rc, "Lexer: Failed to query punctuator token with %Rrc", rc);

    if (   pTok->enmType == RTSCRIPTLEXTOKTYPE_PUNCTUATOR
        && pTok->Type.Keyword.pKeyword->u64Val == (uint64_t)enmTerm)
    {
        RTScriptLexConsumeToken(pThis->hLexSource);
        *pfConsumed = true;
        return VINF_SUCCESS;
    }

    *pfConsumed = false;
    return VINF_SUCCESS;
}


static int rtAcpiAslLexerConsumeIfStringLit(PCRTACPIASLCU pThis, const char **ppszStrLit)
{
    PCRTSCRIPTLEXTOKEN pTok;
    int rc = RTScriptLexQueryToken(pThis->hLexSource, &pTok);
    if (RT_FAILURE(rc))
        return RTErrInfoSetF(pThis->pErrInfo, rc, "Lexer: Failed to query string literal token with %Rrc", rc);

    if (pTok->enmType == RTSCRIPTLEXTOKTYPE_STRINGLIT)
    {
        *ppszStrLit = pTok->Type.StringLit.pszString;
        RTScriptLexConsumeToken(pThis->hLexSource);
        return VINF_SUCCESS;
    }

    return VINF_SUCCESS;
}


static int rtAcpiAslLexerConsumeIfIdentifier(PCRTACPIASLCU pThis, const char **ppszIde)
{
    PCRTSCRIPTLEXTOKEN pTok;
    int rc = RTScriptLexQueryToken(pThis->hLexSource, &pTok);
    if (RT_FAILURE(rc))
        return RTErrInfoSetF(pThis->pErrInfo, rc, "Lexer: Failed to query string literal token with %Rrc", rc);

    if (pTok->enmType == RTSCRIPTLEXTOKTYPE_IDENTIFIER)
    {
        *ppszIde = pTok->Type.Id.pszIde;
        RTScriptLexConsumeToken(pThis->hLexSource);
        return VINF_SUCCESS;
    }

    return VINF_SUCCESS;
}


static int rtAcpiAslLexerConsumeIfNatural(PCRTACPIASLCU pThis, uint64_t *pu64, bool *pfConsumed)
{
    PCRTSCRIPTLEXTOKEN pTok;
    int rc = RTScriptLexQueryToken(pThis->hLexSource, &pTok);
    if (RT_FAILURE(rc))
        return RTErrInfoSetF(pThis->pErrInfo, rc, "Lexer: Failed to query punctuator token with %Rrc", rc);

    if (   pTok->enmType == RTSCRIPTLEXTOKTYPE_NUMBER
        && pTok->Type.Number.enmType == RTSCRIPTLEXTOKNUMTYPE_NATURAL)
    {
        *pfConsumed = true;
        *pu64 = pTok->Type.Number.Type.u64;
        RTScriptLexConsumeToken(pThis->hLexSource);
        return VINF_SUCCESS;
    }

    *pfConsumed = false;
    return VINF_SUCCESS;
}


static int rtAcpiAslParserConsumeEos(PCRTACPIASLCU pThis)
{
    PCRTSCRIPTLEXTOKEN pTok;
    int rc = RTScriptLexQueryToken(pThis->hLexSource, &pTok);
    if (RT_FAILURE(rc))
        return RTErrInfoSetF(pThis->pErrInfo, rc, "Lexer: Failed to query punctuator token with %Rrc", rc);

    if (pTok->enmType == RTSCRIPTLEXTOKTYPE_EOS)
    {
        RTScriptLexConsumeToken(pThis->hLexSource);
        return VINF_SUCCESS;
    }

    return RTErrInfoSetF(pThis->pErrInfo, VERR_INVALID_PARAMETER,
                         "Parser: Found unexpected token after final closing }, expected end of stream");
}


/* Some parser helper macros. */
#define RTACPIASL_PARSE_KEYWORD(a_enmKeyword, a_pszKeyword) \
    do { \
        bool fConsumed2 = false; \
        int rc2 = rtAcpiAslLexerConsumeIfKeyword(pThis, a_enmKeyword, &fConsumed2); \
        if (RT_FAILURE(rc2)) \
            return rc2; \
        if (!fConsumed2) \
            return RTErrInfoSetF(pThis->pErrInfo, VERR_INVALID_PARAMETER, "Parser: Expected keyword '%s'", a_pszKeyword); \
    } while(0)

#define RTACPIASL_PARSE_KEYWORD_LIST(a_enmKeyword, a_aenmKeywordList) \
    RTACPIASLTERMINAL a_enmKeyword = RTACPIASLTERMINAL_INVALID; \
    do { \
        int rc2 = rtAcpiAslLexerConsumeIfKeywordInList(pThis, a_aenmKeywordList, &a_enmKeyword); \
        if (RT_FAILURE(rc2)) \
            return rc2; \
        if (a_enmKeyword == RTACPIASLTERMINAL_INVALID) \
            return RTErrInfoSetF(pThis->pErrInfo, VERR_INVALID_PARAMETER, "Parser: Unexpected keyword found"); \
    } while(0)

#define RTACPIASL_PARSE_OPTIONAL_KEYWORD_LIST(a_enmKeyword, a_aenmKeywordList, a_enmDefault) \
    RTACPIASLTERMINAL a_enmKeyword = a_enmDefault; \
    do { \
        int rc2 = rtAcpiAslLexerConsumeIfKeywordInList(pThis, a_aenmKeywordList, &a_enmKeyword); \
        if (RT_FAILURE(rc2)) \
            return rc2; \
    } while(0)

#define RTACPIASL_PARSE_PUNCTUATOR(a_enmPunctuator, a_chPunctuator) \
    do { \
        bool fConsumed2 = false; \
        int rc2 = rtAcpiAslLexerConsumeIfPunctuator(pThis, a_enmPunctuator, &fConsumed2); \
        if (RT_FAILURE(rc2)) \
            return rc2; \
        if (!fConsumed2) \
            return RTErrInfoSetF(pThis->pErrInfo, VERR_INVALID_PARAMETER, "Parser: Expected punctuator '%c'", a_chPunctuator); \
    } while(0)

#define RTACPIASL_PARSE_OPTIONAL_PUNCTUATOR(a_enmPunctuator) \
    do { \
        bool fConsumed2 = false; \
        int rc2 = rtAcpiAslLexerConsumeIfPunctuator(pThis, a_enmPunctuator, &fConsumed2); \
        if (RT_FAILURE(rc2)) \
            return rc2; \
    } while(0)

#define RTACPIASL_PARSE_STRING_LIT(a_pszStrLit) \
    const char *a_pszStrLit = NULL; \
    do { \
        int rc2 = rtAcpiAslLexerConsumeIfStringLit(pThis, &a_pszStrLit); \
        if (RT_FAILURE(rc2)) \
            return rc2; \
        if (!a_pszStrLit) \
            return RTErrInfoSetF(pThis->pErrInfo, VERR_INVALID_PARAMETER, "Parser: Expected a string literal"); \
    } while(0)

#define RTACPIASL_PARSE_NAME_STRING(a_pszIde) \
    const char *a_pszIde = NULL; \
    do { \
        int rc2 = rtAcpiAslLexerConsumeIfIdentifier(pThis, &a_pszIde); \
        if (RT_FAILURE(rc2)) \
            return rc2; \
        if (!a_pszIde) \
            return RTErrInfoSetF(pThis->pErrInfo, VERR_INVALID_PARAMETER, "Parser: Expected an identifier"); \
    } while(0)

#define RTACPIASL_PARSE_OPTIONAL_NAME_STRING(a_pszIde) \
    const char *a_pszIde = NULL; \
    do { \
        int rc2 = rtAcpiAslLexerConsumeIfIdentifier(pThis, &a_pszIde); \
        if (RT_FAILURE(rc2)) \
            return rc2; \
    } while(0)

#define RTACPIASL_PARSE_NATURAL(a_u64) \
    uint64_t a_u64 = 0; \
    do { \
        bool fConsumed2 = false; \
        int rc2 = rtAcpiAslLexerConsumeIfNatural(pThis, &a_u64, &fConsumed2); \
        if (RT_FAILURE(rc2)) \
            return rc2; \
        if (!fConsumed2) \
            return RTErrInfoSetF(pThis->pErrInfo, VERR_INVALID_PARAMETER, "Parser: Expected a natural number"); \
    } while(0)

#define RTACPIASL_SKIP_CURRENT_TOKEN() \
    RTScriptLexConsumeToken(pThis->hLexSource);


static int rtAcpiTblAslParseInner(PRTACPIASLCU pThis, PRTLISTANCHOR pLstStmts);
static int rtAcpiTblAslParseTermArg(PRTACPIASLCU pThis, PRTACPIASTNODE *ppAstNd);


static const RTACPIASLTERMINAL g_aenmObjTypeKeywords[] = {
    RTACPIASLTERMINAL_KEYWORD_UNKNOWN_OBJ,
    RTACPIASLTERMINAL_KEYWORD_INT_OBJ,
    RTACPIASLTERMINAL_KEYWORD_STR_OBJ,
    RTACPIASLTERMINAL_KEYWORD_BUFF_OBJ,
    RTACPIASLTERMINAL_KEYWORD_PKG_OBJ,
    RTACPIASLTERMINAL_KEYWORD_FIELD_UNIT_OBJ,
    RTACPIASLTERMINAL_KEYWORD_DEVICE_OBJ,
    RTACPIASLTERMINAL_KEYWORD_EVENT_OBJ,
    RTACPIASLTERMINAL_KEYWORD_METHOD_OBJ,
    RTACPIASLTERMINAL_KEYWORD_MUTEX_OBJ,
    RTACPIASLTERMINAL_KEYWORD_OP_REGION_OBJ,
    RTACPIASLTERMINAL_KEYWORD_POWER_RES_OBJ,
    RTACPIASLTERMINAL_KEYWORD_THERMAL_ZONE_OBJ,
    RTACPIASLTERMINAL_KEYWORD_BUFF_FIELD_OBJ,
    RTACPIASLTERMINAL_KEYWORD_PROCESSOR_OBJ,
    RTACPIASLTERMINAL_INVALID
};


static const RTACPIASLTERMINAL g_aenmSerializeRuleKeywords[] = {
    RTACPIASLTERMINAL_KEYWORD_SERIALIZED,
    RTACPIASLTERMINAL_KEYWORD_NOT_SERIALIZED,
    RTACPIASLTERMINAL_INVALID
};


static const RTACPIASLTERMINAL g_aenmRegionSpaceKeywords[] = {
    RTACPIASLTERMINAL_KEYWORD_SYSTEM_IO,
    RTACPIASLTERMINAL_KEYWORD_SYSTEM_MEMORY,
    RTACPIASLTERMINAL_KEYWORD_PCI_CONFIG,
    RTACPIASLTERMINAL_KEYWORD_EMBEDDED_CONTROL,
    RTACPIASLTERMINAL_KEYWORD_SMBUS,
    RTACPIASLTERMINAL_KEYWORD_SYSTEM_CMOS,
    RTACPIASLTERMINAL_KEYWORD_PCI_BAR_TARGET,
    RTACPIASLTERMINAL_KEYWORD_IPMI,
    RTACPIASLTERMINAL_KEYWORD_GENERAL_PURPOSE_IO,
    RTACPIASLTERMINAL_KEYWORD_GENERIC_SERIAL_BUS,
    RTACPIASLTERMINAL_KEYWORD_PCC,
    RTACPIASLTERMINAL_KEYWORD_PRM,
    RTACPIASLTERMINAL_KEYWORD_FFIXED_HW,
    RTACPIASLTERMINAL_INVALID
};


static const RTACPIASLTERMINAL g_aenmAccessTypeKeywords[] = {
    RTACPIASLTERMINAL_KEYWORD_ANY_ACC,
    RTACPIASLTERMINAL_KEYWORD_BYTE_ACC,
    RTACPIASLTERMINAL_KEYWORD_WORD_ACC,
    RTACPIASLTERMINAL_KEYWORD_DWORD_ACC,
    RTACPIASLTERMINAL_KEYWORD_QWORD_ACC,
    RTACPIASLTERMINAL_KEYWORD_BUFFER_ACC,
    RTACPIASLTERMINAL_INVALID
};


static const RTACPIASLTERMINAL g_aenmLockRuleKeywords[] = {
    RTACPIASLTERMINAL_KEYWORD_LOCK,
    RTACPIASLTERMINAL_KEYWORD_NO_LOCK,
    RTACPIASLTERMINAL_INVALID
};


static const RTACPIASLTERMINAL g_aenmUpdateRuleKeywords[] = {
    RTACPIASLTERMINAL_KEYWORD_PRESERVE,
    RTACPIASLTERMINAL_KEYWORD_WRITE_AS_ONES,
    RTACPIASLTERMINAL_KEYWORD_WRITE_AS_ZEROES,
    RTACPIASLTERMINAL_INVALID
};


static const RTACPIASLTERMINAL g_aenmRwRoKeywords[] = {
    RTACPIASLTERMINAL_KEYWORD_READONLY,
    RTACPIASLTERMINAL_KEYWORD_READWRITE,
    RTACPIASLTERMINAL_INVALID
};



static DECLCALLBACK(int) rtAcpiTblAslParseExternal(PRTACPIASLCU pThis, PCRTACPIASLKEYWORD pKeyword, PRTACPIASTNODE pAstNd)
{
    RT_NOREF(pKeyword, pAstNd);

    RTACPIASL_PARSE_PUNCTUATOR(RTACPIASLTERMINAL_PUNCTUATOR_OPEN_BRACKET, '(');

    /* Namestring is required. */
    RTACPIASL_PARSE_NAME_STRING(pszNameString);
    pAstNd->aArgs[0].enmType         = kAcpiAstArgType_NameString;
    pAstNd->aArgs[0].u.pszNameString = pszNameString;

    /* Defaults for optional arguments. */
    pAstNd->aArgs[1].enmType      = kAcpiAstArgType_ObjType;
    pAstNd->aArgs[1].u.enmObjType = kAcpiObjType_Unknown;
    pAstNd->aArgs[2].enmType      = kAcpiAstArgType_U8;
    pAstNd->aArgs[2].u.u8         = 0;

    if (rtAcpiAslLexerIsPunctuator(pThis, RTACPIASLTERMINAL_PUNCTUATOR_COMMA))
    {
        RTACPIASL_PARSE_PUNCTUATOR(RTACPIASLTERMINAL_PUNCTUATOR_COMMA, ',');

        RTACPIASLTERMINAL enmKeyword;
        int rc = rtAcpiAslLexerConsumeIfKeywordInList(pThis, &g_aenmObjTypeKeywords[0], &enmKeyword);
        if (RT_FAILURE(rc))
            return rc;

        if (enmKeyword != RTACPIASLTERMINAL_INVALID)
        {
            switch (enmKeyword)
            {
                case RTACPIASLTERMINAL_KEYWORD_UNKNOWN_OBJ:      pAstNd->aArgs[1].u.enmObjType = kAcpiObjType_Unknown; break;
                case RTACPIASLTERMINAL_KEYWORD_INT_OBJ:          pAstNd->aArgs[1].u.enmObjType = kAcpiObjType_Int; break;
                case RTACPIASLTERMINAL_KEYWORD_STR_OBJ:          pAstNd->aArgs[1].u.enmObjType = kAcpiObjType_Str; break;
                case RTACPIASLTERMINAL_KEYWORD_BUFF_OBJ:         pAstNd->aArgs[1].u.enmObjType = kAcpiObjType_Buff; break;
                case RTACPIASLTERMINAL_KEYWORD_PKG_OBJ:          pAstNd->aArgs[1].u.enmObjType = kAcpiObjType_Pkg; break;
                case RTACPIASLTERMINAL_KEYWORD_FIELD_UNIT_OBJ:   pAstNd->aArgs[1].u.enmObjType = kAcpiObjType_FieldUnit; break;
                case RTACPIASLTERMINAL_KEYWORD_DEVICE_OBJ:       pAstNd->aArgs[1].u.enmObjType = kAcpiObjType_Device; break;
                case RTACPIASLTERMINAL_KEYWORD_EVENT_OBJ:        pAstNd->aArgs[1].u.enmObjType = kAcpiObjType_Event; break;
                case RTACPIASLTERMINAL_KEYWORD_METHOD_OBJ:       pAstNd->aArgs[1].u.enmObjType = kAcpiObjType_Method; break;
                case RTACPIASLTERMINAL_KEYWORD_MUTEX_OBJ:        pAstNd->aArgs[1].u.enmObjType = kAcpiObjType_MutexObj; break;
                case RTACPIASLTERMINAL_KEYWORD_OP_REGION_OBJ:    pAstNd->aArgs[1].u.enmObjType = kAcpiObjType_OpRegion; break;
                case RTACPIASLTERMINAL_KEYWORD_POWER_RES_OBJ:    pAstNd->aArgs[1].u.enmObjType = kAcpiObjType_PowerRes; break;
                case RTACPIASLTERMINAL_KEYWORD_THERMAL_ZONE_OBJ: pAstNd->aArgs[1].u.enmObjType = kAcpiObjType_ThermalZone; break;
                case RTACPIASLTERMINAL_KEYWORD_BUFF_FIELD_OBJ:   pAstNd->aArgs[1].u.enmObjType = kAcpiObjType_BuffField; break;
                case RTACPIASLTERMINAL_KEYWORD_PROCESSOR_OBJ:    pAstNd->aArgs[1].u.enmObjType = kAcpiObjType_Processor; break;
                default:
                    AssertFailedReturn(VERR_INTERNAL_ERROR);
            }
        }

        if (rtAcpiAslLexerIsPunctuator(pThis, RTACPIASLTERMINAL_PUNCTUATOR_COMMA))
        {
            RTACPIASL_PARSE_PUNCTUATOR(RTACPIASLTERMINAL_PUNCTUATOR_COMMA, ',');

            /** @todo ResultType */

            if (rtAcpiAslLexerIsPunctuator(pThis, RTACPIASLTERMINAL_PUNCTUATOR_COMMA))
            {
                RTACPIASL_PARSE_PUNCTUATOR(RTACPIASLTERMINAL_PUNCTUATOR_COMMA, ',');

                /** @todo ParameterTypes */
            }
        }
    }

    RTACPIASL_PARSE_PUNCTUATOR(RTACPIASLTERMINAL_PUNCTUATOR_CLOSE_BRACKET, ')');

    int rc = rtAcpiNsAddEntryAstNode(pThis->pNs, pAstNd->aArgs[0].u.pszNameString, pAstNd, true /*fSwitchTo*/);
    if (RT_FAILURE(rc))
        return RTErrInfoSetF(pThis->pErrInfo, rc, "Failed to add External(%s,,,) to namespace", pAstNd->aArgs[0].u.pszNameString);

    return VINF_SUCCESS;
}


static DECLCALLBACK(int) rtAcpiTblAslParseMethod(PRTACPIASLCU pThis, PCRTACPIASLKEYWORD pKeyword, PRTACPIASTNODE pAstNd)
{
    RT_NOREF(pKeyword, pAstNd);

    RTACPIASL_PARSE_PUNCTUATOR(RTACPIASLTERMINAL_PUNCTUATOR_OPEN_BRACKET, '(');

    /* Namestring is required. */
    RTACPIASL_PARSE_NAME_STRING(pszNameString);
    pAstNd->aArgs[0].enmType         = kAcpiAstArgType_NameString;
    pAstNd->aArgs[0].u.pszNameString = pszNameString;

    /* Defaults for optional arguments. */
    pAstNd->aArgs[1].enmType      = kAcpiAstArgType_U8;
    pAstNd->aArgs[1].u.u8         = 0;
    pAstNd->aArgs[2].enmType      = kAcpiAstArgType_Bool;
    pAstNd->aArgs[2].u.f          = false;
    pAstNd->aArgs[3].enmType      = kAcpiAstArgType_U8;
    pAstNd->aArgs[3].u.u8         = 0;

    if (rtAcpiAslLexerIsPunctuator(pThis, RTACPIASLTERMINAL_PUNCTUATOR_COMMA))
    {
        /* NumArgs */
        RTACPIASL_PARSE_PUNCTUATOR(RTACPIASLTERMINAL_PUNCTUATOR_COMMA, ',');

        uint64_t u64 = 0;
        bool fConsumed = false;
        int rc = rtAcpiAslLexerConsumeIfNatural(pThis, &u64, &fConsumed);
        if (RT_FAILURE(rc))
            return rc;

        if (fConsumed)
        {
            if (u64 >= 8)
                return RTErrInfoSetF(pThis->pErrInfo, VERR_INVALID_PARAMETER,
                                     "Argument count value is out of range [0..7]: %u", u64);
            pAstNd->aArgs[1].u.u8 = (uint8_t)u64;
        }

        if (rtAcpiAslLexerIsPunctuator(pThis, RTACPIASLTERMINAL_PUNCTUATOR_COMMA))
        {
            RTACPIASL_PARSE_PUNCTUATOR(RTACPIASLTERMINAL_PUNCTUATOR_COMMA, ',');

            /* Serialized|NotSerialized */
            RTACPIASLTERMINAL enmKeyword;
            rc = rtAcpiAslLexerConsumeIfKeywordInList(pThis, &g_aenmSerializeRuleKeywords[0], &enmKeyword);
            if (RT_FAILURE(rc))
                return rc;

            if (enmKeyword != RTACPIASLTERMINAL_INVALID)
            {
                Assert(enmKeyword == RTACPIASLTERMINAL_KEYWORD_SERIALIZED || enmKeyword == RTACPIASLTERMINAL_KEYWORD_NOT_SERIALIZED);
                pAstNd->aArgs[2].u.f =    enmKeyword == RTACPIASLTERMINAL_KEYWORD_SERIALIZED
                                        ? RTACPI_METHOD_F_SERIALIZED
                                        : RTACPI_METHOD_F_NOT_SERIALIZED;
            }

            if (rtAcpiAslLexerIsPunctuator(pThis, RTACPIASLTERMINAL_PUNCTUATOR_COMMA))
            {
                RTACPIASL_PARSE_PUNCTUATOR(RTACPIASLTERMINAL_PUNCTUATOR_COMMA, ',');

                /* Sync Level */
                u64 = 0;
                rc = rtAcpiAslLexerConsumeIfNatural(pThis, &u64, &fConsumed);
                if (RT_FAILURE(rc))
                    return rc;

                if (fConsumed)
                {
                    if (u64 >= 16)
                        return RTErrInfoSetF(pThis->pErrInfo, VERR_INVALID_PARAMETER,
                                             "SyncLevel value is out of range [0..15]: %u", u64);
                    pAstNd->aArgs[3].u.u8 = (uint8_t)u64;
                }

                if (rtAcpiAslLexerIsPunctuator(pThis, RTACPIASLTERMINAL_PUNCTUATOR_COMMA))
                {
                    RTACPIASL_PARSE_PUNCTUATOR(RTACPIASLTERMINAL_PUNCTUATOR_COMMA, ',');

                    /** @todo ReturnType */

                    if (rtAcpiAslLexerIsPunctuator(pThis, RTACPIASLTERMINAL_PUNCTUATOR_COMMA))
                    {
                        RTACPIASL_PARSE_PUNCTUATOR(RTACPIASLTERMINAL_PUNCTUATOR_COMMA, ',');

                        /** @todo ParameterTypes */
                    }
                }
            }
        }
    }

    RTACPIASL_PARSE_PUNCTUATOR(RTACPIASLTERMINAL_PUNCTUATOR_CLOSE_BRACKET, ')');

    int rc = rtAcpiNsAddEntryAstNode(pThis->pNs, pAstNd->aArgs[0].u.pszNameString, pAstNd, true /*fSwitchTo*/);
    if (RT_FAILURE(rc))
        return RTErrInfoSetF(pThis->pErrInfo, rc, "Failed to add Method(%s,,,) to namespace", pAstNd->aArgs[0].u.pszNameString);

    return VINF_SUCCESS;
}


static int rtAcpiTblParseFieldUnitList(PRTACPIASLCU pThis, PRTACPIASTNODE pAstNd)
{
    RTACPIFIELDENTRY aFieldEntries[128]; RT_ZERO(aFieldEntries); /** @todo Allow dynamic allocation? */
    uint32_t cFields = 0;

    for (;;)
    {
        /** @todo Is an empty list allowed (currently we allow that)? */
        if (rtAcpiAslLexerIsPunctuator(pThis, RTACPIASLTERMINAL_PUNCTUATOR_CLOSE_CURLY_BRACKET))
            break;

        /*
         * Two of the following are possible:
         *     Offset(Integer)
         *     NameSeg "," Integer
         */
        bool fConsumed = false;
        int rc = rtAcpiAslLexerConsumeIfKeyword(pThis, RTACPIASLTERMINAL_KEYWORD_OFFSET, &fConsumed);
        if (RT_FAILURE(rc))
            return rc;

        if (fConsumed)
        {
            RTACPIASL_PARSE_PUNCTUATOR(RTACPIASLTERMINAL_PUNCTUATOR_OPEN_BRACKET, '(');
            /* Must be an integer */
            RTACPIASL_PARSE_NATURAL(offBytes);
            aFieldEntries[cFields].pszName = NULL;
            aFieldEntries[cFields].cBits   = offBytes * sizeof(uint8_t);
            RTACPIASL_PARSE_PUNCTUATOR(RTACPIASLTERMINAL_PUNCTUATOR_CLOSE_BRACKET, ')');
        }
        else
        {
            /* This must be the second case. */
            RTACPIASL_PARSE_NAME_STRING(pszName); /** @todo Verify that the name string consists only of a single name segment. */
            RTACPIASL_PARSE_PUNCTUATOR(RTACPIASLTERMINAL_PUNCTUATOR_COMMA, ',');
            RTACPIASL_PARSE_NATURAL(cBits);
            aFieldEntries[cFields].pszName = pszName;
            aFieldEntries[cFields].cBits   = cBits;
        }

        cFields++;

        /* A following "," means there is another entry, otherwise the closing "}" should follow. */
        if (!rtAcpiAslLexerIsPunctuator(pThis, RTACPIASLTERMINAL_PUNCTUATOR_COMMA))
            break;

        RTACPIASL_SKIP_CURRENT_TOKEN(); /* Skip the "," */

        if (cFields == RT_ELEMENTS(aFieldEntries))
            return RTErrInfoSetF(pThis->pErrInfo, VERR_BUFFER_OVERFLOW,
                                 "The field list overflows the current allowed maximum of %u fields", RT_ELEMENTS(aFieldEntries));
    }

    RTACPIASL_PARSE_PUNCTUATOR(RTACPIASLTERMINAL_PUNCTUATOR_CLOSE_CURLY_BRACKET, '}');

    /* Allocate the list to the node. */
    pAstNd->Fields.paFields = (PRTACPIFIELDENTRY)RTMemAllocZ(cFields * sizeof(aFieldEntries[0]));
    if (!pAstNd->Fields.paFields)
        return RTErrInfoSetF(pThis->pErrInfo, VERR_NO_MEMORY, "Out of memory allocating field unit list with %u entries", cFields);

    for (uint32_t i = 0; i < cFields; i++)
        pAstNd->Fields.paFields[i] = aFieldEntries[i];

    pAstNd->Fields.cFields = cFields;
    return VINF_SUCCESS;
}


static DECLCALLBACK(int) rtAcpiTblAslParseFieldOrIndexField(PRTACPIASLCU pThis, PCRTACPIASLKEYWORD pKeyword, PRTACPIASTNODE pAstNd)
{
    RT_NOREF(pKeyword);

    RTACPIASL_PARSE_PUNCTUATOR(RTACPIASLTERMINAL_PUNCTUATOR_OPEN_BRACKET, '(');

    uint8_t idxArg = 0;

    if (pKeyword->cArgsReq == 5)
    {
        /* This is an IndexField. */

        /* Namestring is required. */
        RTACPIASL_PARSE_NAME_STRING(pszNameString);
        pAstNd->aArgs[idxArg].enmType         = kAcpiAstArgType_NameString;
        pAstNd->aArgs[idxArg].u.pszNameString = pszNameString;
        idxArg++;

        RTACPIASL_PARSE_PUNCTUATOR(RTACPIASLTERMINAL_PUNCTUATOR_COMMA, ',');
    }
    else /* Field */
        Assert(pKeyword->cArgsReq == 4);

    /* Namestring is required. */
    RTACPIASL_PARSE_NAME_STRING(pszNameString);
    pAstNd->aArgs[idxArg].enmType         = kAcpiAstArgType_NameString;
    pAstNd->aArgs[idxArg].u.pszNameString = pszNameString;
    idxArg++;

    RTACPIASL_PARSE_PUNCTUATOR(RTACPIASLTERMINAL_PUNCTUATOR_COMMA, ',');

    /* Must have an access type defined. */
    RTACPIASL_PARSE_KEYWORD_LIST(enmAccessType, g_aenmAccessTypeKeywords);
    pAstNd->aArgs[idxArg].enmType = kAcpiAstArgType_FieldAcc;
    switch (enmAccessType)
    {
        case RTACPIASLTERMINAL_KEYWORD_ANY_ACC:    pAstNd->aArgs[idxArg].u.enmFieldAcc = kAcpiFieldAcc_Any;    break;
        case RTACPIASLTERMINAL_KEYWORD_BYTE_ACC:   pAstNd->aArgs[idxArg].u.enmFieldAcc = kAcpiFieldAcc_Byte;   break;
        case RTACPIASLTERMINAL_KEYWORD_WORD_ACC:   pAstNd->aArgs[idxArg].u.enmFieldAcc = kAcpiFieldAcc_Word;   break;
        case RTACPIASLTERMINAL_KEYWORD_DWORD_ACC:  pAstNd->aArgs[idxArg].u.enmFieldAcc = kAcpiFieldAcc_DWord;  break;
        case RTACPIASLTERMINAL_KEYWORD_QWORD_ACC:  pAstNd->aArgs[idxArg].u.enmFieldAcc = kAcpiFieldAcc_QWord;  break;
        case RTACPIASLTERMINAL_KEYWORD_BUFFER_ACC: pAstNd->aArgs[idxArg].u.enmFieldAcc = kAcpiFieldAcc_Buffer; break;
        default:
            AssertFailedReturn(VERR_INTERNAL_ERROR);
    }
    idxArg++;

    RTACPIASL_PARSE_PUNCTUATOR(RTACPIASLTERMINAL_PUNCTUATOR_COMMA, ',');

    /* Must have a lock rule defined. */
    RTACPIASL_PARSE_KEYWORD_LIST(enmLockRule, g_aenmLockRuleKeywords);
    pAstNd->aArgs[idxArg].enmType = kAcpiAstArgType_Bool;
    switch (enmLockRule)
    {
        case RTACPIASLTERMINAL_KEYWORD_LOCK:    pAstNd->aArgs[idxArg].u.f = true;  break;
        case RTACPIASLTERMINAL_KEYWORD_NO_LOCK: pAstNd->aArgs[idxArg].u.f = false; break;
        default:
            AssertFailedReturn(VERR_INTERNAL_ERROR);
    }
    idxArg++;

    RTACPIASL_PARSE_PUNCTUATOR(RTACPIASLTERMINAL_PUNCTUATOR_COMMA, ',');

    /* Must have an update rule defined. */
    RTACPIASL_PARSE_KEYWORD_LIST(enmUpdateRule, g_aenmUpdateRuleKeywords);
    pAstNd->aArgs[idxArg].enmType = kAcpiAstArgType_FieldUpdate;
    switch (enmUpdateRule)
    {
        case RTACPIASLTERMINAL_KEYWORD_PRESERVE:        pAstNd->aArgs[idxArg].u.enmFieldUpdate = kAcpiFieldUpdate_Preserve;      break;
        case RTACPIASLTERMINAL_KEYWORD_WRITE_AS_ONES:   pAstNd->aArgs[idxArg].u.enmFieldUpdate = kAcpiFieldUpdate_WriteAsOnes;   break;
        case RTACPIASLTERMINAL_KEYWORD_WRITE_AS_ZEROES: pAstNd->aArgs[idxArg].u.enmFieldUpdate = kAcpiFieldUpdate_WriteAsZeroes; break;
        default:
            AssertFailedReturn(VERR_INTERNAL_ERROR);
    }
    idxArg++;

    RTACPIASL_PARSE_PUNCTUATOR(RTACPIASLTERMINAL_PUNCTUATOR_CLOSE_BRACKET, ')');

    /* Parse the field unit list. */
    RTACPIASL_PARSE_PUNCTUATOR(RTACPIASLTERMINAL_PUNCTUATOR_OPEN_CURLY_BRACKET, '{');
    return rtAcpiTblParseFieldUnitList(pThis, pAstNd);
}


static int rtAcpiTblParseResourceMemory32Fixed(PRTACPIASLCU pThis, RTACPIRES hAcpiRes, PRTACPIASTNODE pAstNd)
{
    RTACPIASL_PARSE_PUNCTUATOR(RTACPIASLTERMINAL_PUNCTUATOR_OPEN_BRACKET, '(');
    RTACPIASL_PARSE_KEYWORD_LIST(enmKeywordAccess, g_aenmRwRoKeywords);
    RTACPIASL_PARSE_PUNCTUATOR(RTACPIASLTERMINAL_PUNCTUATOR_COMMA, ',');
    RTACPIASL_PARSE_NATURAL(u64PhysAddrStart);
    RTACPIASL_PARSE_PUNCTUATOR(RTACPIASLTERMINAL_PUNCTUATOR_COMMA, ',');
    RTACPIASL_PARSE_NATURAL(cbRegion);
    RTACPIASL_PARSE_PUNCTUATOR(RTACPIASLTERMINAL_PUNCTUATOR_COMMA, ',');
    RTACPIASL_PARSE_OPTIONAL_NAME_STRING(pszName);
    RTACPIASL_PARSE_PUNCTUATOR(RTACPIASLTERMINAL_PUNCTUATOR_CLOSE_BRACKET, ')');

    /* Check that the given range is within bounds. */
    if (   u64PhysAddrStart >= _4G
        || cbRegion >= _4G
        || u64PhysAddrStart + cbRegion >= _4G
        || u64PhysAddrStart + cbRegion < u64PhysAddrStart)
        return RTErrInfoSetF(pThis->pErrInfo, VERR_INVALID_PARAMETER,
                             "The given memory range does not fit into a 32-bit memory address space: Start=%#RX64 Size=%#RX64",
                             u64PhysAddrStart, cbRegion);

    if (pszName)
    {
        /* Create namespace entries. */
        uint32_t const offResource = RTAcpiResourceGetOffset(hAcpiRes);
        int rc = rtAcpiNsAddEntryAstNode(pThis->pNs, pszName, pAstNd, true /*fSwitchTo*/);
        if (RT_SUCCESS(rc))
        {
            rc = rtAcpiNsAddEntryU64(pThis->pNs, "_BAS", offResource + 4, false /*fSwitchTo*/);
            if (RT_FAILURE(rc))
                return RTErrInfoSetF(pThis->pErrInfo, rc, "Failed to add '%s._BAS' to namespace",
                                     pszName);

            rc = rtAcpiNsAddEntryU64(pThis->pNs, "_LEN", offResource + 8, false /*fSwitchTo*/);
            if (RT_FAILURE(rc))
                return RTErrInfoSetF(pThis->pErrInfo, rc, "Failed to add '%s._LEN' to namespace",
                                     pszName);

            rtAcpiNsPop(pThis->pNs);
        }
        else
            return RTErrInfoSetF(pThis->pErrInfo, rc,
                                 "Failed to add Memory32Fixed(, %#RX64 Size=%#RX64, %s) to namespace",
                                 u64PhysAddrStart, cbRegion, pszName);
    }

    int rc = RTAcpiResourceAdd32BitFixedMemoryRange(hAcpiRes, u64PhysAddrStart, cbRegion, enmKeywordAccess == RTACPIASLTERMINAL_KEYWORD_READWRITE);
    if (RT_FAILURE(rc))
        return RTErrInfoSetF(pThis->pErrInfo, rc,
                             "Failed to add Memory32Fixed(fRw=%RTbool, %#RX64 Size=%#RX64, %s)",
                             enmKeywordAccess == RTACPIASLTERMINAL_KEYWORD_READWRITE, u64PhysAddrStart, cbRegion,
                             pszName ? pszName : "<NONE>");

    return VINF_SUCCESS;
}


static int rtAcpiTblParseIrqList(PRTACPIASLCU pThis, uint16_t *pbmIntrs)
{
    uint16_t bmIntrs = 0;
    RTACPIASL_PARSE_PUNCTUATOR(RTACPIASLTERMINAL_PUNCTUATOR_OPEN_CURLY_BRACKET, '{');
    for (;;)
    {
        if (rtAcpiAslLexerIsPunctuator(pThis, RTACPIASLTERMINAL_PUNCTUATOR_CLOSE_CURLY_BRACKET))
            break;

        RTACPIASL_PARSE_NATURAL(u64Intr);
        if (u64Intr > 15)
            return RTErrInfoSetF(pThis->pErrInfo, VERR_INVALID_PARAMETER,
                                 "Interrupt number %u is out of range [0..15]: %RU64",
                                 u64Intr);
        if (bmIntrs & RT_BIT(u64Intr))
            return RTErrInfoSetF(pThis->pErrInfo, VERR_INVALID_PARAMETER, "Duplicate interrupt %u in list", u64Intr);

        bmIntrs |= RT_BIT(u64Intr);

        /* A following "," means there is another entry, otherwise the closing "}" should follow. */
        if (!rtAcpiAslLexerIsPunctuator(pThis, RTACPIASLTERMINAL_PUNCTUATOR_COMMA))
            break;

        RTACPIASL_SKIP_CURRENT_TOKEN(); /* Skip the "," */
    }
    RTACPIASL_PARSE_PUNCTUATOR(RTACPIASLTERMINAL_PUNCTUATOR_CLOSE_CURLY_BRACKET, '}');

    *pbmIntrs = bmIntrs;
    return VINF_SUCCESS;
}

static int rtAcpiTblParseResourceIrq(PRTACPIASLCU pThis, RTACPIRES hAcpiRes, PRTACPIASTNODE pAstNd)
{
    static const RTACPIASLTERMINAL s_aenmEdgeLevelKeywords[]   = { RTACPIASLTERMINAL_KEYWORD_EDGE,            RTACPIASLTERMINAL_KEYWORD_LEVEL,              RTACPIASLTERMINAL_INVALID };
    static const RTACPIASLTERMINAL s_aenmActiveLevelKeywords[] = { RTACPIASLTERMINAL_KEYWORD_ACTIVE_HIGH,     RTACPIASLTERMINAL_KEYWORD_ACTIVE_LOW,         RTACPIASLTERMINAL_INVALID };
    static const RTACPIASLTERMINAL s_aenmSharedExclKeywords[]  = { RTACPIASLTERMINAL_KEYWORD_SHARED,          RTACPIASLTERMINAL_KEYWORD_EXCLUSIVE,
                                                                   RTACPIASLTERMINAL_KEYWORD_SHARED_AND_WAKE, RTACPIASLTERMINAL_KEYWORD_EXCLUSIVE_AND_WAKE, RTACPIASLTERMINAL_INVALID };

    RTACPIASL_PARSE_PUNCTUATOR(RTACPIASLTERMINAL_PUNCTUATOR_OPEN_BRACKET, '(');
    RTACPIASL_PARSE_KEYWORD_LIST(enmEdgeLevel, s_aenmEdgeLevelKeywords);
    RTACPIASL_PARSE_PUNCTUATOR(RTACPIASLTERMINAL_PUNCTUATOR_COMMA, ',');
    RTACPIASL_PARSE_KEYWORD_LIST(enmActiveHigh, s_aenmActiveLevelKeywords);
    RTACPIASL_PARSE_PUNCTUATOR(RTACPIASLTERMINAL_PUNCTUATOR_COMMA, ',');
    RTACPIASL_PARSE_OPTIONAL_KEYWORD_LIST(enmSharedExcl, s_aenmSharedExclKeywords, RTACPIASLTERMINAL_KEYWORD_EXCLUSIVE);
    RTACPIASL_PARSE_OPTIONAL_PUNCTUATOR(RTACPIASLTERMINAL_PUNCTUATOR_COMMA);
    RTACPIASL_PARSE_OPTIONAL_NAME_STRING(pszName);
    RTACPIASL_PARSE_PUNCTUATOR(RTACPIASLTERMINAL_PUNCTUATOR_CLOSE_BRACKET, ')');

    uint16_t bmIntrs = 0;
    int rc = rtAcpiTblParseIrqList(pThis, &bmIntrs);
    if (RT_FAILURE(rc))
        return rc;

    if (pszName)
    {
        /* Create namespace entries. */
        uint32_t const offResource = RTAcpiResourceGetOffset(hAcpiRes);
        rc = rtAcpiNsAddEntryAstNode(pThis->pNs, pszName, pAstNd, true /*fSwitchTo*/);
        if (RT_SUCCESS(rc))
        {
            rc = rtAcpiNsAddEntryU64(pThis->pNs, "_HE", offResource, false /*fSwitchTo*/); /** @todo */
            if (RT_FAILURE(rc))
                return RTErrInfoSetF(pThis->pErrInfo, rc, "Failed to add '%s._HE' to namespace",
                                     pszName);

            rc = rtAcpiNsAddEntryU64(pThis->pNs, "_LL", offResource, false /*fSwitchTo*/); /** @todo */
            if (RT_FAILURE(rc))
                return RTErrInfoSetF(pThis->pErrInfo, rc, "Failed to add '%s._LL' to namespace",
                                     pszName);

            rtAcpiNsPop(pThis->pNs);
        }
        else
            return RTErrInfoSetF(pThis->pErrInfo, rc, "Failed to add IRQ(,,,,, %s) to namespace", pszName);
    }

    rc = RTAcpiResourceAddIrq(hAcpiRes,
                              enmEdgeLevel == RTACPIASLTERMINAL_KEYWORD_EDGE,
                              enmActiveHigh == RTACPIASLTERMINAL_KEYWORD_ACTIVE_LOW,
                              enmSharedExcl == RTACPIASLTERMINAL_KEYWORD_SHARED || enmSharedExcl == RTACPIASLTERMINAL_KEYWORD_SHARED_AND_WAKE,
                              enmSharedExcl == RTACPIASLTERMINAL_KEYWORD_SHARED_AND_WAKE || enmSharedExcl == RTACPIASLTERMINAL_KEYWORD_EXCLUSIVE_AND_WAKE,
                              bmIntrs);
    if (RT_FAILURE(rc))
        return RTErrInfoSetF(pThis->pErrInfo, rc,
                             "Failed to add IRQ(,,,,, %s)", pszName ? pszName : "<NONE>");

    return VINF_SUCCESS;
}


static int rtAcpiTblParseResourceIrqNoFlags(PRTACPIASLCU pThis, RTACPIRES hAcpiRes, PRTACPIASTNODE pAstNd)
{
    RTACPIASL_PARSE_PUNCTUATOR(RTACPIASLTERMINAL_PUNCTUATOR_OPEN_BRACKET, '(');
    RTACPIASL_PARSE_OPTIONAL_NAME_STRING(pszName);
    RTACPIASL_PARSE_PUNCTUATOR(RTACPIASLTERMINAL_PUNCTUATOR_CLOSE_BRACKET, ')');

    uint16_t bmIntrs = 0;
    int rc = rtAcpiTblParseIrqList(pThis, &bmIntrs);
    if (RT_FAILURE(rc))
        return rc;

    if (pszName)
    {
        /* Create namespace entries. */
        uint32_t const offResource = RTAcpiResourceGetOffset(hAcpiRes); RT_NOREF(offResource);
        rc = rtAcpiNsAddEntryAstNode(pThis->pNs, pszName, pAstNd, false /*fSwitchTo*/);
        if (RT_FAILURE(rc))
            return RTErrInfoSetF(pThis->pErrInfo, rc, "Failed to add IRQNoFlags(%s) to namespace", pszName);
    }

    rc = RTAcpiResourceAddIrq(hAcpiRes, true /*fEdgeTriggered*/, false /*fActiveLow*/, false /*fShared*/, false /*fWakeCapable*/, bmIntrs);
    if (RT_FAILURE(rc))
        return RTErrInfoSetF(pThis->pErrInfo, rc,
                             "Failed to add IRQNoFlags(%s)", pszName ? pszName : "<NONE>");

    return VINF_SUCCESS;
}


static int rtAcpiTblParseResourceIo(PRTACPIASLCU pThis, RTACPIRES hAcpiRes, PRTACPIASTNODE pAstNd)
{
    static const RTACPIASLTERMINAL s_aenmDecodeKeywords[] = { RTACPIASLTERMINAL_KEYWORD_DECODE_10, RTACPIASLTERMINAL_KEYWORD_DECODE_16, RTACPIASLTERMINAL_INVALID };

    RTACPIASL_PARSE_PUNCTUATOR(RTACPIASLTERMINAL_PUNCTUATOR_OPEN_BRACKET, '(');
    RTACPIASL_PARSE_KEYWORD_LIST(enmDecode, s_aenmDecodeKeywords);
    RTACPIASL_PARSE_PUNCTUATOR(RTACPIASLTERMINAL_PUNCTUATOR_COMMA, ',');
    RTACPIASL_PARSE_NATURAL(u64AddrMin);
    RTACPIASL_PARSE_PUNCTUATOR(RTACPIASLTERMINAL_PUNCTUATOR_COMMA, ',');
    RTACPIASL_PARSE_NATURAL(u64AddrMax);
    RTACPIASL_PARSE_PUNCTUATOR(RTACPIASLTERMINAL_PUNCTUATOR_COMMA, ',');
    RTACPIASL_PARSE_NATURAL(u64AddrAlignment);
    RTACPIASL_PARSE_PUNCTUATOR(RTACPIASLTERMINAL_PUNCTUATOR_COMMA, ',');
    RTACPIASL_PARSE_NATURAL(u64RangeLength);
    RTACPIASL_PARSE_OPTIONAL_PUNCTUATOR(RTACPIASLTERMINAL_PUNCTUATOR_COMMA);
    RTACPIASL_PARSE_OPTIONAL_NAME_STRING(pszName);
    RTACPIASL_PARSE_PUNCTUATOR(RTACPIASLTERMINAL_PUNCTUATOR_CLOSE_BRACKET, ')');

    if (   u64AddrMin > UINT16_MAX
        || u64AddrMax > UINT16_MAX
        || u64AddrAlignment > UINT8_MAX
        || u64RangeLength > UINT8_MAX)
        return RTErrInfoSetF(pThis->pErrInfo, VERR_INVALID_PARAMETER,
                             "Invalid parameters given to IO macro: AddressMin=%#RX16 AddressMax=%#RX16 AddressAlignment=%#RX8 RangeLength=%#RX8",
                             u64AddrMin, u64AddrMax, u64AddrAlignment, u64RangeLength);

    if (pszName)
    {
        /* Create namespace entries. */
        uint32_t const offResource = RTAcpiResourceGetOffset(hAcpiRes);
        int rc = rtAcpiNsAddEntryAstNode(pThis->pNs, pszName, pAstNd, true /*fSwitchTo*/);
        if (RT_SUCCESS(rc))
        {
            rc = rtAcpiNsAddEntryU64(pThis->pNs, "_DEC", offResource, false /*fSwitchTo*/); /** @todo */
            if (RT_FAILURE(rc))
                return RTErrInfoSetF(pThis->pErrInfo, rc, "Failed to add '%s._DEC' to namespace",
                                     pszName);

            rc = rtAcpiNsAddEntryU64(pThis->pNs, "_MIN", offResource + 2, false /*fSwitchTo*/);
            if (RT_FAILURE(rc))
                return RTErrInfoSetF(pThis->pErrInfo, rc, "Failed to add '%s._MIN' to namespace",
                                     pszName);

            rc = rtAcpiNsAddEntryU64(pThis->pNs, "_MAX", offResource + 4, false /*fSwitchTo*/);
            if (RT_FAILURE(rc))
                return RTErrInfoSetF(pThis->pErrInfo, rc, "Failed to add '%s._MAX' to namespace",
                                     pszName);

            rc = rtAcpiNsAddEntryU64(pThis->pNs, "_ALN", offResource + 6, false /*fSwitchTo*/);
            if (RT_FAILURE(rc))
                return RTErrInfoSetF(pThis->pErrInfo, rc, "Failed to add '%s._ALN' to namespace",
                                     pszName);

            rc = rtAcpiNsAddEntryU64(pThis->pNs, "_LEN", offResource + 7, false /*fSwitchTo*/);
            if (RT_FAILURE(rc))
                return RTErrInfoSetF(pThis->pErrInfo, rc, "Failed to add '%s._LEN' to namespace",
                                     pszName);

            rtAcpiNsPop(pThis->pNs);
        }
        else
            return RTErrInfoSetF(pThis->pErrInfo, rc, "Failed to add IO(,,,,, %s) to namespace", pszName);
    }

    int rc = RTAcpiResourceAddIo(hAcpiRes, enmDecode == RTACPIASLTERMINAL_KEYWORD_DECODE_10 ? kAcpiResIoDecodeType_Decode10 : kAcpiResIoDecodeType_Decode16,
                                 (uint16_t)u64AddrMin, (uint16_t)u64AddrMax, (uint8_t)u64AddrAlignment, (uint8_t)u64RangeLength);
    if (RT_FAILURE(rc))
        return RTErrInfoSetF(pThis->pErrInfo, rc,
                             "Failed to add IO(,,,,, %s)", pszName ? pszName : "<NONE>");

    return VINF_SUCCESS;
}


static DECLCALLBACK(int) rtAcpiTblAslParseResourceTemplate(PRTACPIASLCU pThis, PCRTACPIASLKEYWORD pKeyword, PRTACPIASTNODE pAstNd)
{
    RT_NOREF(pKeyword);

    static const RTACPIASLTERMINAL s_aenmResourceTemplateKeywords[] = {
        RTACPIASLTERMINAL_KEYWORD_MEMORY32_FIXED,
        RTACPIASLTERMINAL_KEYWORD_IRQ,
        RTACPIASLTERMINAL_KEYWORD_IRQ_NO_FLAGS,
        RTACPIASLTERMINAL_KEYWORD_IO,
        RTACPIASLTERMINAL_INVALID
    };

    RTACPIASL_PARSE_PUNCTUATOR(RTACPIASLTERMINAL_PUNCTUATOR_OPEN_BRACKET, '(');
    RTACPIASL_PARSE_PUNCTUATOR(RTACPIASLTERMINAL_PUNCTUATOR_CLOSE_BRACKET, ')');

    RTACPIASL_PARSE_PUNCTUATOR(RTACPIASLTERMINAL_PUNCTUATOR_OPEN_CURLY_BRACKET, '{');

    RTACPIRES hAcpiRes = NULL;
    int rc = RTAcpiResourceCreate(&hAcpiRes);
    if (RT_FAILURE(rc))
        return RTErrInfoSetF(pThis->pErrInfo, rc, "Creating the ACPI resource template failed while parsing");

    /* Assign here already to have the ACPI resource freed when the node gets destroyed, even if there is an error while parsing. */
    pAstNd->hAcpiRes = hAcpiRes;

    /* Get to work */
    for (;;)
    {
        RTACPIASL_PARSE_KEYWORD_LIST(enmResourceKeyword, s_aenmResourceTemplateKeywords);
        switch (enmResourceKeyword)
        {
            case RTACPIASLTERMINAL_KEYWORD_MEMORY32_FIXED:
            {
                rc = rtAcpiTblParseResourceMemory32Fixed(pThis, hAcpiRes, pAstNd);
                break;
            }
            case RTACPIASLTERMINAL_KEYWORD_IRQ:
            {
                rc = rtAcpiTblParseResourceIrq(pThis, hAcpiRes, pAstNd);
                break;
            }
            case RTACPIASLTERMINAL_KEYWORD_IRQ_NO_FLAGS:
            {
                rc = rtAcpiTblParseResourceIrqNoFlags(pThis, hAcpiRes, pAstNd);
                break;
            }
            case RTACPIASLTERMINAL_KEYWORD_IO:
            {
                rc = rtAcpiTblParseResourceIo(pThis, hAcpiRes, pAstNd);
                break;
            }
            default: /* This should never occur. */
                AssertReleaseFailed();
        }
        if (RT_FAILURE(rc))
            return rc;

        /* Done processing (indicated by the closing "}")?. */
        if (rtAcpiAslLexerIsPunctuator(pThis, RTACPIASLTERMINAL_PUNCTUATOR_CLOSE_CURLY_BRACKET))
            break;
    }

    RTACPIASL_PARSE_PUNCTUATOR(RTACPIASLTERMINAL_PUNCTUATOR_CLOSE_CURLY_BRACKET, '}');
    rc = RTAcpiResourceSeal(hAcpiRes);
    if (RT_FAILURE(rc))
        rc = RTErrInfoSetF(pThis->pErrInfo, rc, "Failed to seal the resource after being done parsing");
    return rc;
}


static DECLCALLBACK(int) rtAcpiTblAslParsePackageOrBuffer(PRTACPIASLCU pThis, PCRTACPIASLKEYWORD pKeyword, PRTACPIASTNODE pAstNd)
{
    RT_NOREF(pKeyword);

    /* The scope flag manually because the parsing is done here already. */
    RTListInit(&pAstNd->LstScopeNodes);
    pAstNd->fFlags |= RTACPI_AST_NODE_F_NEW_SCOPE;

    pAstNd->aArgs[0].enmType = kAcpiAstArgType_AstNode;
    pAstNd->aArgs[0].u.pAstNd = NULL;

    RTACPIASL_PARSE_PUNCTUATOR(RTACPIASLTERMINAL_PUNCTUATOR_OPEN_BRACKET, '(');
    if (!rtAcpiAslLexerIsPunctuator(pThis, RTACPIASLTERMINAL_PUNCTUATOR_CLOSE_BRACKET))
    {
        PRTACPIASTNODE pAstNdSize = NULL;
        int rc = rtAcpiTblAslParseTermArg(pThis, &pAstNdSize);
        if (RT_FAILURE(rc))
            return rc;
        pAstNd->aArgs[0].u.pAstNd = pAstNdSize;
    }
    RTACPIASL_PARSE_PUNCTUATOR(RTACPIASLTERMINAL_PUNCTUATOR_CLOSE_BRACKET, ')');

    RTACPIASL_PARSE_PUNCTUATOR(RTACPIASLTERMINAL_PUNCTUATOR_OPEN_CURLY_BRACKET, '{');

    /* Get to work */
    for (;;)
    {
        /** @todo Is an empty list allowed (currently we allow that)? */
        if (rtAcpiAslLexerIsPunctuator(pThis, RTACPIASLTERMINAL_PUNCTUATOR_CLOSE_CURLY_BRACKET))
            break;

        /* Parse the object */
        PRTACPIASTNODE pAstNdPkg = NULL;
        int rc = rtAcpiTblAslParseTermArg(pThis, &pAstNdPkg);
        if (RT_FAILURE(rc))
            return rc;

        RTListAppend(&pAstNd->LstScopeNodes, &pAstNdPkg->NdAst);

        /* A following "," means there is another entry, otherwise the closing "}" should follow. */
        if (!rtAcpiAslLexerIsPunctuator(pThis, RTACPIASLTERMINAL_PUNCTUATOR_COMMA))
            break;

        RTACPIASL_SKIP_CURRENT_TOKEN(); /* Skip the "," */
    }

    RTACPIASL_PARSE_PUNCTUATOR(RTACPIASLTERMINAL_PUNCTUATOR_CLOSE_CURLY_BRACKET, '}');
    return VINF_SUCCESS;
}


static DECLCALLBACK(int) rtAcpiTblAslParseReturn(PRTACPIASLCU pThis, PCRTACPIASLKEYWORD pKeyword, PRTACPIASTNODE pAstNd)
{
    RT_NOREF(pKeyword);

    pAstNd->aArgs[0].enmType = kAcpiAstArgType_AstNode;
    pAstNd->aArgs[0].u.pAstNd = NULL;

    /*
     * Return has three valid forms:
     *    Return
     *    Return ()
     *    Return (TermArg)
     */
    if (rtAcpiAslLexerIsPunctuator(pThis, RTACPIASLTERMINAL_PUNCTUATOR_OPEN_BRACKET))
    {
        RTACPIASL_SKIP_CURRENT_TOKEN(); /* Skip the "(" */

        if (!rtAcpiAslLexerIsPunctuator(pThis, RTACPIASLTERMINAL_PUNCTUATOR_CLOSE_BRACKET))
        {
            PRTACPIASTNODE pAstNdSize = NULL;
            int rc = rtAcpiTblAslParseTermArg(pThis, &pAstNdSize);
            if (RT_FAILURE(rc))
                return rc;
            pAstNd->aArgs[0].u.pAstNd = pAstNdSize;
        }
        RTACPIASL_PARSE_PUNCTUATOR(RTACPIASLTERMINAL_PUNCTUATOR_CLOSE_BRACKET, ')');
    }

    return VINF_SUCCESS;
}

#define RTACPI_ASL_KEYWORD_DEFINE_INVALID \
    { \
        NULL, NULL, 0, 0, RTACPI_AST_NODE_F_DEFAULT, \
        {   kAcpiAstArgType_Invalid, \
            kAcpiAstArgType_Invalid, \
            kAcpiAstArgType_Invalid, \
            kAcpiAstArgType_Invalid, \
            kAcpiAstArgType_Invalid}, \
        { \
            { kAcpiAstArgType_Invalid, { 0 } }, \
            { kAcpiAstArgType_Invalid, { 0 } }, \
            { kAcpiAstArgType_Invalid, { 0 } } \
        } \
    }

#define RTACPI_ASL_KEYWORD_DEFINE_HANDLER(a_szKeyword, a_pfnHandler, a_cArgReq, a_cArgOpt, a_fFlags) \
    { \
        a_szKeyword, a_pfnHandler, a_cArgReq, a_cArgOpt, a_fFlags, \
        {   kAcpiAstArgType_Invalid, \
            kAcpiAstArgType_Invalid, \
            kAcpiAstArgType_Invalid, \
            kAcpiAstArgType_Invalid, \
            kAcpiAstArgType_Invalid}, \
        { \
            { kAcpiAstArgType_Invalid, { 0 } }, \
            { kAcpiAstArgType_Invalid, { 0 } }, \
            { kAcpiAstArgType_Invalid, { 0 } } \
        } \
    }

#define RTACPI_ASL_KEYWORD_DEFINE_0REQ_0OPT(a_szKeyword, a_fFlags) \
    { \
        a_szKeyword, NULL, 0, 0, a_fFlags, \
        {   kAcpiAstArgType_Invalid, \
            kAcpiAstArgType_Invalid, \
            kAcpiAstArgType_Invalid, \
            kAcpiAstArgType_Invalid, \
            kAcpiAstArgType_Invalid}, \
        { \
            { kAcpiAstArgType_Invalid, { 0 } }, \
            { kAcpiAstArgType_Invalid, { 0 } }, \
            { kAcpiAstArgType_Invalid, { 0 } } \
        } \
    }

#define RTACPI_ASL_KEYWORD_DEFINE_0REQ_1OPT(a_szKeyword, a_fFlags, a_enmArgTypeOpt0) \
    { \
        a_szKeyword, NULL, 0, 0, a_fFlags, \
        {   kAcpiAstArgType_Invalid, \
            kAcpiAstArgType_Invalid, \
            kAcpiAstArgType_Invalid, \
            kAcpiAstArgType_Invalid, \
            kAcpiAstArgType_Invalid}, \
        { \
            { a_enmArgTypeOpt0,        { 0 } }, \
            { kAcpiAstArgType_Invalid, { 0 } }, \
            { kAcpiAstArgType_Invalid, { 0 } } \
        } \
    }

#define RTACPI_ASL_KEYWORD_DEFINE_1REQ_0OPT(a_szKeyword, a_fFlags, a_enmArgType0) \
    { \
        a_szKeyword, NULL, 1, 0, a_fFlags, \
        {   a_enmArgType0, \
            kAcpiAstArgType_Invalid, \
            kAcpiAstArgType_Invalid, \
            kAcpiAstArgType_Invalid, \
            kAcpiAstArgType_Invalid}, \
        { \
            { kAcpiAstArgType_Invalid, { 0 } }, \
            { kAcpiAstArgType_Invalid, { 0 } }, \
            { kAcpiAstArgType_Invalid, { 0 } } \
        } \
    }

#define RTACPI_ASL_KEYWORD_DEFINE_2REQ_0OPT(a_szKeyword, a_fFlags, a_enmArgType0, a_enmArgType1) \
    { \
        a_szKeyword, NULL, 2, 0, a_fFlags, \
        {   a_enmArgType0, \
            a_enmArgType1, \
            kAcpiAstArgType_Invalid, \
            kAcpiAstArgType_Invalid, \
            kAcpiAstArgType_Invalid}, \
        { \
            { kAcpiAstArgType_Invalid, { 0 } }, \
            { kAcpiAstArgType_Invalid, { 0 } }, \
            { kAcpiAstArgType_Invalid, { 0 } } \
        } \
    }

#define RTACPI_ASL_KEYWORD_DEFINE_3REQ_0OPT(a_szKeyword, a_fFlags, a_enmArgType0, a_enmArgType1, a_enmArgType2) \
    { \
        a_szKeyword, NULL, 3, 0, a_fFlags, \
        {   a_enmArgType0, \
            a_enmArgType1, \
            a_enmArgType2, \
            kAcpiAstArgType_Invalid, \
            kAcpiAstArgType_Invalid}, \
        { \
            { kAcpiAstArgType_Invalid, { 0 } }, \
            { kAcpiAstArgType_Invalid, { 0 } }, \
            { kAcpiAstArgType_Invalid, { 0 } } \
        } \
    }

#define RTACPI_ASL_KEYWORD_DEFINE_4REQ_0OPT(a_szKeyword, a_fFlags, a_enmArgType0, a_enmArgType1, a_enmArgType2, a_enmArgType3) \
    { \
        a_szKeyword, NULL, 4, 0, a_fFlags, \
        {   a_enmArgType0, \
            a_enmArgType1, \
            a_enmArgType2, \
            a_enmArgType3, \
            kAcpiAstArgType_Invalid}, \
        { \
            { kAcpiAstArgType_Invalid, { 0 } }, \
            { kAcpiAstArgType_Invalid, { 0 } }, \
            { kAcpiAstArgType_Invalid, { 0 } } \
        } \
    }

#define RTACPI_ASL_KEYWORD_DEFINE_2REQ_1OPT(a_szKeyword, a_fFlags, a_enmArgType0, a_enmArgType1, a_enmArgTypeOpt0) \
    { \
        a_szKeyword, NULL, 2, 1, a_fFlags, \
        {   a_enmArgType0, \
            a_enmArgType1, \
            kAcpiAstArgType_Invalid, \
            kAcpiAstArgType_Invalid, \
            kAcpiAstArgType_Invalid}, \
        { \
            { a_enmArgTypeOpt0,        { 0 } }, \
            { kAcpiAstArgType_Invalid, { 0 } }, \
            { kAcpiAstArgType_Invalid, { 0 } } \
        } \
    }

#define RTACPI_ASL_KEYWORD_DEFINE_1REQ_1OPT(a_szKeyword, a_fFlags, a_enmArgType0, a_enmArgTypeOpt0) \
    { \
        a_szKeyword, NULL, 1, 1, a_fFlags, \
        {   a_enmArgType0, \
            kAcpiAstArgType_Invalid, \
            kAcpiAstArgType_Invalid, \
            kAcpiAstArgType_Invalid, \
            kAcpiAstArgType_Invalid}, \
        { \
            { a_enmArgTypeOpt0,        { 0 } }, \
            { kAcpiAstArgType_Invalid, { 0 } }, \
            { kAcpiAstArgType_Invalid, { 0 } } \
        } \
    }

/**
 * Operations encoding table, indexed by kAcpiAstNodeOp_XXX.
 */
static const RTACPIASLKEYWORD g_aAslOps[] =
{
    /* kAcpiAstNodeOp_Invalid           */  RTACPI_ASL_KEYWORD_DEFINE_INVALID,
    /* kAcpiAstNodeOp_Identifier        */  RTACPI_ASL_KEYWORD_DEFINE_INVALID,
    /* kAcpiAstNodeOp_StringLiteral     */  RTACPI_ASL_KEYWORD_DEFINE_INVALID,
    /* kAcpiAstNodeOp_Number            */  RTACPI_ASL_KEYWORD_DEFINE_INVALID,
    /* kAcpiAstNodeOp_Scope             */  RTACPI_ASL_KEYWORD_DEFINE_1REQ_0OPT("Scope",            RTACPI_AST_NODE_F_NEW_SCOPE | RTACPI_AST_NODE_F_NS_ENTRY,   kAcpiAstArgType_NameString),
    /* kAcpiAstNodeOp_Processor         */  {
                                                "Processor", NULL, 2, 2, RTACPI_AST_NODE_F_NEW_SCOPE | RTACPI_AST_NODE_F_NS_ENTRY,
                                                {
                                                    kAcpiAstArgType_NameString,
                                                    kAcpiAstArgType_U8,
                                                    kAcpiAstArgType_Invalid,
                                                    kAcpiAstArgType_Invalid,
                                                    kAcpiAstArgType_Invalid
                                                },
                                                {
                                                    { kAcpiAstArgType_U32,     { 0 } },
                                                    { kAcpiAstArgType_U8,      { 0 } },
                                                    { kAcpiAstArgType_Invalid, { 0 } }
                                                }
                                            },
    /* kAcpiAstNodeOp_External          */  RTACPI_ASL_KEYWORD_DEFINE_HANDLER(  "External",         rtAcpiTblAslParseExternal, 1, 2, RTACPI_AST_NODE_F_DEFAULT),
    /* kAcpiAstNodeOp_Method            */  RTACPI_ASL_KEYWORD_DEFINE_HANDLER(  "Method",           rtAcpiTblAslParseMethod,   1, 3, RTACPI_AST_NODE_F_NEW_SCOPE),
    /* kAcpiAstNodeOp_Device            */  RTACPI_ASL_KEYWORD_DEFINE_1REQ_0OPT("Device",           RTACPI_AST_NODE_F_NEW_SCOPE | RTACPI_AST_NODE_F_NS_ENTRY,   kAcpiAstArgType_NameString),
    /* kAcpiAstNodeOp_If                */  RTACPI_ASL_KEYWORD_DEFINE_1REQ_0OPT("If",               RTACPI_AST_NODE_F_NEW_SCOPE,                                kAcpiAstArgType_AstNode),
    /* kAcpiAstNodeOp_Else              */  RTACPI_ASL_KEYWORD_DEFINE_0REQ_0OPT("Else",             RTACPI_AST_NODE_F_NEW_SCOPE),
    /* kAcpiAstNodeOp_LAnd              */  RTACPI_ASL_KEYWORD_DEFINE_2REQ_0OPT("LAnd",             RTACPI_AST_NODE_F_DEFAULT,                                  kAcpiAstArgType_AstNode, kAcpiAstArgType_AstNode),
    /* kAcpiAstNodeOp_LEqual            */  RTACPI_ASL_KEYWORD_DEFINE_2REQ_0OPT("LEqual",           RTACPI_AST_NODE_F_DEFAULT,                                  kAcpiAstArgType_AstNode, kAcpiAstArgType_AstNode),
    /* kAcpiAstNodeOp_LGreater          */  RTACPI_ASL_KEYWORD_DEFINE_2REQ_0OPT("LGreater",         RTACPI_AST_NODE_F_DEFAULT,                                  kAcpiAstArgType_AstNode, kAcpiAstArgType_AstNode),
    /* kAcpiAstNodeOp_LGreaterEqual     */  RTACPI_ASL_KEYWORD_DEFINE_2REQ_0OPT("LGreaterEqual",    RTACPI_AST_NODE_F_DEFAULT,                                  kAcpiAstArgType_AstNode, kAcpiAstArgType_AstNode),
    /* kAcpiAstNodeOp_LLess             */  RTACPI_ASL_KEYWORD_DEFINE_2REQ_0OPT("LLess",            RTACPI_AST_NODE_F_DEFAULT,                                  kAcpiAstArgType_AstNode, kAcpiAstArgType_AstNode),
    /* kAcpiAstNodeOp_LLessEqual        */  RTACPI_ASL_KEYWORD_DEFINE_2REQ_0OPT("LLessEqual",       RTACPI_AST_NODE_F_DEFAULT,                                  kAcpiAstArgType_AstNode, kAcpiAstArgType_AstNode),
    /* kAcpiAstNodeOp_LNot              */  RTACPI_ASL_KEYWORD_DEFINE_1REQ_0OPT("LNot",             RTACPI_AST_NODE_F_DEFAULT,                                  kAcpiAstArgType_AstNode),
    /* kAcpiAstNodeOp_LNotEqual         */  RTACPI_ASL_KEYWORD_DEFINE_2REQ_0OPT("LNotEqual",        RTACPI_AST_NODE_F_DEFAULT,                                  kAcpiAstArgType_AstNode, kAcpiAstArgType_AstNode),
    /* kAcpiAstNodeOp_Zero              */  RTACPI_ASL_KEYWORD_DEFINE_0REQ_0OPT("Zero",             RTACPI_AST_NODE_F_DEFAULT),
    /* kAcpiAstNodeOp_One               */  RTACPI_ASL_KEYWORD_DEFINE_0REQ_0OPT("One",              RTACPI_AST_NODE_F_DEFAULT),
    /* kAcpiAstNodeOp_Ones              */  RTACPI_ASL_KEYWORD_DEFINE_0REQ_0OPT("Ones",             RTACPI_AST_NODE_F_DEFAULT),
    /* kAcpiAstNodeOp_Return            */  RTACPI_ASL_KEYWORD_DEFINE_HANDLER(  "Return",           rtAcpiTblAslParseReturn,  0, 1, RTACPI_AST_NODE_F_DEFAULT),
    /* kAcpiAstNodeOp_Unicode           */  RTACPI_ASL_KEYWORD_DEFINE_1REQ_0OPT("Unicode",          RTACPI_AST_NODE_F_DEFAULT,                                  kAcpiAstArgType_AstNode), /* Actually only String allowed here */
    /* kAcpiAstNodeOp_OperationRegion   */  RTACPI_ASL_KEYWORD_DEFINE_4REQ_0OPT("OperationRegion",  RTACPI_AST_NODE_F_DEFAULT | RTACPI_AST_NODE_F_NS_ENTRY,     kAcpiAstArgType_NameString, kAcpiAstArgType_RegionSpace, kAcpiAstArgType_AstNode, kAcpiAstArgType_AstNode),
    /* kAcpiAstNodeOp_Field             */  RTACPI_ASL_KEYWORD_DEFINE_HANDLER(  "Field",            rtAcpiTblAslParseFieldOrIndexField, 4, 0, RTACPI_AST_NODE_F_DEFAULT),
    /* kAcpiAstNodeOp_Name              */  RTACPI_ASL_KEYWORD_DEFINE_2REQ_0OPT("Name",             RTACPI_AST_NODE_F_NS_ENTRY,                                 kAcpiAstArgType_NameString, kAcpiAstArgType_AstNode),
    /* kAcpiAstNodeOp_ResourceTemplate  */  RTACPI_ASL_KEYWORD_DEFINE_HANDLER(  "ResourceTemplate", rtAcpiTblAslParseResourceTemplate,  0, 0, RTACPI_AST_NODE_F_DEFAULT),
    /* kAcpiAstNodeOp_Arg0              */  RTACPI_ASL_KEYWORD_DEFINE_0REQ_0OPT("Arg0",             RTACPI_AST_NODE_F_DEFAULT),
    /* kAcpiAstNodeOp_Arg1              */  RTACPI_ASL_KEYWORD_DEFINE_0REQ_0OPT("Arg1",             RTACPI_AST_NODE_F_DEFAULT),
    /* kAcpiAstNodeOp_Arg2              */  RTACPI_ASL_KEYWORD_DEFINE_0REQ_0OPT("Arg2",             RTACPI_AST_NODE_F_DEFAULT),
    /* kAcpiAstNodeOp_Arg3              */  RTACPI_ASL_KEYWORD_DEFINE_0REQ_0OPT("Arg3",             RTACPI_AST_NODE_F_DEFAULT),
    /* kAcpiAstNodeOp_Arg4              */  RTACPI_ASL_KEYWORD_DEFINE_0REQ_0OPT("Arg4",             RTACPI_AST_NODE_F_DEFAULT),
    /* kAcpiAstNodeOp_Arg5              */  RTACPI_ASL_KEYWORD_DEFINE_0REQ_0OPT("Arg5",             RTACPI_AST_NODE_F_DEFAULT),
    /* kAcpiAstNodeOp_Arg6              */  RTACPI_ASL_KEYWORD_DEFINE_0REQ_0OPT("Arg6",             RTACPI_AST_NODE_F_DEFAULT),
    /* kAcpiAstNodeOp_Local0            */  RTACPI_ASL_KEYWORD_DEFINE_0REQ_0OPT("Local0",           RTACPI_AST_NODE_F_DEFAULT),
    /* kAcpiAstNodeOp_Local1            */  RTACPI_ASL_KEYWORD_DEFINE_0REQ_0OPT("Local1",           RTACPI_AST_NODE_F_DEFAULT),
    /* kAcpiAstNodeOp_Local2            */  RTACPI_ASL_KEYWORD_DEFINE_0REQ_0OPT("Local2",           RTACPI_AST_NODE_F_DEFAULT),
    /* kAcpiAstNodeOp_Local3            */  RTACPI_ASL_KEYWORD_DEFINE_0REQ_0OPT("Local3",           RTACPI_AST_NODE_F_DEFAULT),
    /* kAcpiAstNodeOp_Local4            */  RTACPI_ASL_KEYWORD_DEFINE_0REQ_0OPT("Local4",           RTACPI_AST_NODE_F_DEFAULT),
    /* kAcpiAstNodeOp_Local5            */  RTACPI_ASL_KEYWORD_DEFINE_0REQ_0OPT("Local5",           RTACPI_AST_NODE_F_DEFAULT),
    /* kAcpiAstNodeOp_Local6            */  RTACPI_ASL_KEYWORD_DEFINE_0REQ_0OPT("Local6",           RTACPI_AST_NODE_F_DEFAULT),
    /* kAcpiAstNodeOp_Local7            */  RTACPI_ASL_KEYWORD_DEFINE_0REQ_0OPT("Local7",           RTACPI_AST_NODE_F_DEFAULT),
    /* kAcpiAstNodeOp_Package           */  RTACPI_ASL_KEYWORD_DEFINE_HANDLER(  "Package",          rtAcpiTblAslParsePackageOrBuffer, 0, 1, RTACPI_AST_NODE_F_DEFAULT),
    /* kAcpiAstNodeOp_Buffer            */  RTACPI_ASL_KEYWORD_DEFINE_HANDLER(  "Buffer",           rtAcpiTblAslParsePackageOrBuffer, 0, 1, RTACPI_AST_NODE_F_DEFAULT),
    /* kAcpiAstNodeOp_ToUUid            */  RTACPI_ASL_KEYWORD_DEFINE_1REQ_0OPT("ToUUID",           RTACPI_AST_NODE_F_DEFAULT,    kAcpiAstArgType_AstNode),
    /* kAcpiAstNodeOp_DerefOf           */  RTACPI_ASL_KEYWORD_DEFINE_1REQ_0OPT("DerefOf",          RTACPI_AST_NODE_F_DEFAULT,    kAcpiAstArgType_AstNode),
    /* kAcpiAstNodeOp_Index             */  RTACPI_ASL_KEYWORD_DEFINE_2REQ_1OPT("Index",            RTACPI_AST_NODE_F_DEFAULT,    kAcpiAstArgType_AstNode, kAcpiAstArgType_AstNode, kAcpiAstArgType_AstNode),
    /* kAcpiAstNodeOp_Store             */  RTACPI_ASL_KEYWORD_DEFINE_2REQ_0OPT("Store",            RTACPI_AST_NODE_F_DEFAULT,    kAcpiAstArgType_AstNode, kAcpiAstArgType_AstNode),

    /* kAcpiAstNodeOp_Break             */  RTACPI_ASL_KEYWORD_DEFINE_0REQ_0OPT("Break",            RTACPI_AST_NODE_F_DEFAULT),
    /* kAcpiAstNodeOp_Continue          */  RTACPI_ASL_KEYWORD_DEFINE_0REQ_0OPT("Continue",         RTACPI_AST_NODE_F_DEFAULT),
    /* kAcpiAstNodeOp_Add               */  RTACPI_ASL_KEYWORD_DEFINE_2REQ_1OPT("Add",              RTACPI_AST_NODE_F_DEFAULT,    kAcpiAstArgType_AstNode, kAcpiAstArgType_AstNode, kAcpiAstArgType_AstNode),
    /* kAcpiAstNodeOp_Subtract          */  RTACPI_ASL_KEYWORD_DEFINE_2REQ_1OPT("Subtract",         RTACPI_AST_NODE_F_DEFAULT,    kAcpiAstArgType_AstNode, kAcpiAstArgType_AstNode, kAcpiAstArgType_AstNode),
    /* kAcpiAstNodeOp_And               */  RTACPI_ASL_KEYWORD_DEFINE_2REQ_1OPT("And",              RTACPI_AST_NODE_F_DEFAULT,    kAcpiAstArgType_AstNode, kAcpiAstArgType_AstNode, kAcpiAstArgType_AstNode),
    /* kAcpiAstNodeOp_Nand              */  RTACPI_ASL_KEYWORD_DEFINE_2REQ_1OPT("Nand",             RTACPI_AST_NODE_F_DEFAULT,    kAcpiAstArgType_AstNode, kAcpiAstArgType_AstNode, kAcpiAstArgType_AstNode),
    /* kAcpiAstNodeOp_Or                */  RTACPI_ASL_KEYWORD_DEFINE_2REQ_1OPT("Or",               RTACPI_AST_NODE_F_DEFAULT,    kAcpiAstArgType_AstNode, kAcpiAstArgType_AstNode, kAcpiAstArgType_AstNode),
    /* kAcpiAstNodeOp_Xor               */  RTACPI_ASL_KEYWORD_DEFINE_2REQ_1OPT("Xor",              RTACPI_AST_NODE_F_DEFAULT,    kAcpiAstArgType_AstNode, kAcpiAstArgType_AstNode, kAcpiAstArgType_AstNode),
    /* kAcpiAstNodeOp_Not               */  RTACPI_ASL_KEYWORD_DEFINE_1REQ_1OPT("Not",              RTACPI_AST_NODE_F_DEFAULT,    kAcpiAstArgType_AstNode, kAcpiAstArgType_AstNode),
    /* kAcpiAstNodeOp_Notify            */  RTACPI_ASL_KEYWORD_DEFINE_2REQ_0OPT("Notify",           RTACPI_AST_NODE_F_DEFAULT,    kAcpiAstArgType_AstNode, kAcpiAstArgType_AstNode),
    /* kAcpiAstNodeOp_SizeOf            */  RTACPI_ASL_KEYWORD_DEFINE_1REQ_0OPT("SizeOf",           RTACPI_AST_NODE_F_DEFAULT,    kAcpiAstArgType_AstNode),
    /* kAcpiAstNodeOp_While             */  RTACPI_ASL_KEYWORD_DEFINE_1REQ_0OPT("While",            RTACPI_AST_NODE_F_NEW_SCOPE,  kAcpiAstArgType_AstNode),
    /* kAcpiAstNodeOp_Increment         */  RTACPI_ASL_KEYWORD_DEFINE_1REQ_0OPT("Increment",        RTACPI_AST_NODE_F_DEFAULT,    kAcpiAstArgType_AstNode),
    /* kAcpiAstNodeOp_Decrement         */  RTACPI_ASL_KEYWORD_DEFINE_1REQ_0OPT("Decrement",        RTACPI_AST_NODE_F_DEFAULT,    kAcpiAstArgType_AstNode),
    /* kAcpiAstNodeOp_CondRefOf         */  RTACPI_ASL_KEYWORD_DEFINE_1REQ_1OPT("CondRefOf",        RTACPI_AST_NODE_F_DEFAULT,    kAcpiAstArgType_AstNode, kAcpiAstArgType_AstNode),
    /* kAcpiAstNodeOp_IndexField        */  RTACPI_ASL_KEYWORD_DEFINE_HANDLER(  "IndexField",       rtAcpiTblAslParseFieldOrIndexField, 5, 0, RTACPI_AST_NODE_F_DEFAULT),
    /* kAcpiAstNodeOp_EisaId            */  RTACPI_ASL_KEYWORD_DEFINE_1REQ_0OPT("EisaId",           RTACPI_AST_NODE_F_DEFAULT,    kAcpiAstArgType_StringLiteral),
    /* kAcpiAstNodeOp_CreateField       */  RTACPI_ASL_KEYWORD_DEFINE_4REQ_0OPT("CreateField",      RTACPI_AST_NODE_F_DEFAULT,    kAcpiAstArgType_AstNode, kAcpiAstArgType_AstNode, kAcpiAstArgType_AstNode, kAcpiAstArgType_NameString),
    /* kAcpiAstNodeOp_CreateBitField    */  RTACPI_ASL_KEYWORD_DEFINE_3REQ_0OPT("CreateBitField",   RTACPI_AST_NODE_F_DEFAULT,    kAcpiAstArgType_AstNode, kAcpiAstArgType_AstNode, kAcpiAstArgType_NameString),
    /* kAcpiAstNodeOp_CreateByteField   */  RTACPI_ASL_KEYWORD_DEFINE_3REQ_0OPT("CreateByteField",  RTACPI_AST_NODE_F_DEFAULT,    kAcpiAstArgType_AstNode, kAcpiAstArgType_AstNode, kAcpiAstArgType_NameString),
    /* kAcpiAstNodeOp_CreateWordField   */  RTACPI_ASL_KEYWORD_DEFINE_3REQ_0OPT("CreateWordField",  RTACPI_AST_NODE_F_DEFAULT,    kAcpiAstArgType_AstNode, kAcpiAstArgType_AstNode, kAcpiAstArgType_NameString),
    /* kAcpiAstNodeOp_CreateDWordField  */  RTACPI_ASL_KEYWORD_DEFINE_3REQ_0OPT("CreateDWordField", RTACPI_AST_NODE_F_DEFAULT,    kAcpiAstArgType_AstNode, kAcpiAstArgType_AstNode, kAcpiAstArgType_NameString),
    /* kAcpiAstNodeOp_CreateQWordField  */  RTACPI_ASL_KEYWORD_DEFINE_3REQ_0OPT("CreateQWordField", RTACPI_AST_NODE_F_DEFAULT,    kAcpiAstArgType_AstNode, kAcpiAstArgType_AstNode, kAcpiAstArgType_NameString),
};


static int rtAcpiTblAslParseArgument(PRTACPIASLCU pThis, const char *pszKeyword, uint8_t iArg, RTACPIASTARGTYPE enmArgType, PRTACPIASTARG pArg)
{
    switch (enmArgType)
    {
        case kAcpiAstArgType_AstNode:
        {
            PRTACPIASTNODE pAstNd = NULL;
            int rc = rtAcpiTblAslParseTermArg(pThis, &pAstNd);
            if (RT_FAILURE(rc))
                return rc;
            pArg->enmType  = kAcpiAstArgType_AstNode;
            pArg->u.pAstNd = pAstNd;
            break;
        }
        case kAcpiAstArgType_NameString:
        {
            RTACPIASL_PARSE_NAME_STRING(pszNameString);
            pArg->enmType         = kAcpiAstArgType_NameString;
            pArg->u.pszNameString = pszNameString;
            break;
        }
        case kAcpiAstArgType_U8:
        {
            RTACPIASL_PARSE_NATURAL(u64);
            if (u64 > UINT8_MAX)
                return RTErrInfoSetF(pThis->pErrInfo, VERR_INVALID_PARAMETER,
                                     "Value for byte parameter %u is out of range (%#RX64) while processing keyword '%s'",
                                     iArg, u64, pszKeyword);

            pArg->enmType = kAcpiAstArgType_U8;
            pArg->u.u8    = (uint8_t)u64;
            break;
        }
        case kAcpiAstArgType_U16:
        {
            RTACPIASL_PARSE_NATURAL(u64);
            if (u64 > UINT16_MAX)
                return RTErrInfoSetF(pThis->pErrInfo, VERR_INVALID_PARAMETER,
                                     "Value for word parameter %u is out of range (%#RX64) while processing keyword '%s'",
                                     iArg, u64, pszKeyword);

            pArg->enmType = kAcpiAstArgType_U16;
            pArg->u.u16   = (uint16_t)u64;
            break;
        }
        case kAcpiAstArgType_U32:
        {
            RTACPIASL_PARSE_NATURAL(u64);
            if (u64 > UINT32_MAX)
                return RTErrInfoSetF(pThis->pErrInfo, VERR_INVALID_PARAMETER,
                                     "Value for 32-bit parameter %u is out of range (%#RX64) while processing keyword '%s'",
                                     iArg, u64, pszKeyword);

            pArg->enmType = kAcpiAstArgType_U32;
            pArg->u.u32   = (uint32_t)u64;
            break;
        }
        case kAcpiAstArgType_U64:
        {
            RTACPIASL_PARSE_NATURAL(u64);
            pArg->enmType = kAcpiAstArgType_U64;
            pArg->u.u64   = u64;
            break;
        }
        case kAcpiAstArgType_RegionSpace:
        {
            RTACPIASLTERMINAL enmKeyword;
            int rc = rtAcpiAslLexerConsumeIfKeywordInList(pThis, &g_aenmRegionSpaceKeywords[0], &enmKeyword);
            if (RT_FAILURE(rc))
                return rc;

            if (enmKeyword != RTACPIASLTERMINAL_INVALID)
            {
                pArg->enmType = kAcpiAstArgType_RegionSpace;
                switch (enmKeyword)
                {
                    case RTACPIASLTERMINAL_KEYWORD_SYSTEM_IO:          pArg->u.enmRegionSpace = kAcpiOperationRegionSpace_SystemIo;         break;
                    case RTACPIASLTERMINAL_KEYWORD_SYSTEM_MEMORY:      pArg->u.enmRegionSpace = kAcpiOperationRegionSpace_SystemMemory;     break;
                    case RTACPIASLTERMINAL_KEYWORD_PCI_CONFIG:         pArg->u.enmRegionSpace = kAcpiOperationRegionSpace_PciConfig;        break;
                    case RTACPIASLTERMINAL_KEYWORD_EMBEDDED_CONTROL:   pArg->u.enmRegionSpace = kAcpiOperationRegionSpace_EmbeddedControl;  break;
                    case RTACPIASLTERMINAL_KEYWORD_SMBUS:              pArg->u.enmRegionSpace = kAcpiOperationRegionSpace_SmBus;            break;
                    case RTACPIASLTERMINAL_KEYWORD_SYSTEM_CMOS:        pArg->u.enmRegionSpace = kAcpiOperationRegionSpace_SystemCmos;       break;
                    case RTACPIASLTERMINAL_KEYWORD_PCI_BAR_TARGET:     pArg->u.enmRegionSpace = kAcpiOperationRegionSpace_PciBarTarget;     break;
                    case RTACPIASLTERMINAL_KEYWORD_IPMI:               pArg->u.enmRegionSpace = kAcpiOperationRegionSpace_Ipmi;             break;
                    case RTACPIASLTERMINAL_KEYWORD_GENERAL_PURPOSE_IO: pArg->u.enmRegionSpace = kAcpiOperationRegionSpace_Gpio;             break;
                    case RTACPIASLTERMINAL_KEYWORD_GENERIC_SERIAL_BUS: pArg->u.enmRegionSpace = kAcpiOperationRegionSpace_GenericSerialBus; break;
                    case RTACPIASLTERMINAL_KEYWORD_PCC:                pArg->u.enmRegionSpace = kAcpiOperationRegionSpace_Pcc;              break;
                    case RTACPIASLTERMINAL_KEYWORD_PRM:
                    case RTACPIASLTERMINAL_KEYWORD_FFIXED_HW:
                    default:
                        AssertFailedReturn(VERR_INTERNAL_ERROR);
                }
            }
            else
                return RTErrInfoSetF(pThis->pErrInfo, VERR_INVALID_PARAMETER, "Unknown RegionSpace keyword encountered");
            break;
        }
        case kAcpiAstArgType_StringLiteral:
        {
            RTACPIASL_PARSE_STRING_LIT(psz);
            pArg->enmType     = kAcpiAstArgType_StringLiteral;
            pArg->u.pszStrLit = psz;
            break;
        }
        default:
            AssertReleaseFailed();
    }

    return VINF_SUCCESS;
}


static int rtAcpiTblAslParseOp(PRTACPIASLCU pThis, RTACPIASTNODEOP enmOp, PRTACPIASTNODE *ppAstNd)
{
    int rc = VINF_SUCCESS;

    AssertReturn(enmOp > kAcpiAstNodeOp_Invalid && (unsigned)enmOp < RT_ELEMENTS(g_aAslOps), VERR_INTERNAL_ERROR);

    *ppAstNd = NULL;

    PCRTACPIASLKEYWORD pAslKeyword = &g_aAslOps[enmOp];
    PRTACPIASTNODE pAstNd = rtAcpiAstNodeAlloc(enmOp, pAslKeyword->fFlags, pAslKeyword->cArgsReq + pAslKeyword->cArgsOpt);
    if (!pAstNd)
        return RTErrInfoSetF(pThis->pErrInfo, VERR_NO_MEMORY, "Failed to allocate ACPI AST node when processing keyword '%s'", pAslKeyword->pszOpc);

    *ppAstNd = pAstNd;

    /* Call and parse callback if present, otherwise do the default parsing. */
    if (pAslKeyword->pfnParse)
    {
        rc = pAslKeyword->pfnParse(pThis, pAslKeyword, pAstNd);
        if (RT_FAILURE(rc))
            return rc;
    }
    else if (pAslKeyword->cArgsReq || pAslKeyword->cArgsOpt)
    {
        RTACPIASL_PARSE_PUNCTUATOR(RTACPIASLTERMINAL_PUNCTUATOR_OPEN_BRACKET, '(');

        /* Process any required arguments. */
        for (uint8_t i = 0; i < pAslKeyword->cArgsReq; i++)
        {
            rc = rtAcpiTblAslParseArgument(pThis, pAslKeyword->pszOpc, i, pAslKeyword->aenmTypes[i], &pAstNd->aArgs[i]);
            if (RT_FAILURE(rc))
                return rc;

            if (i == 0 && (pAslKeyword->fFlags & RTACPI_AST_NODE_F_NS_ENTRY))
            {
                /*
                 * Create a new namespace entry, we currently assume that the first argument is a namestring
                 * which gives the path.
                 */
                AssertReturn(pAstNd->aArgs[0].enmType == kAcpiAstArgType_NameString, VERR_NOT_SUPPORTED);

                rc = rtAcpiNsAddEntryAstNode(pThis->pNs, pAstNd->aArgs[0].u.pszNameString, pAstNd, true /*fSwitchTo*/);
                if (RT_FAILURE(rc))
                    return rc;
            }

            /* There must be a "," between required arguments, not counting the last required argument because it can be closed with ")". */
            if (i < (uint8_t)(pAslKeyword->cArgsReq - 1))
                RTACPIASL_PARSE_PUNCTUATOR(RTACPIASLTERMINAL_PUNCTUATOR_COMMA, ',');
        }

        /* Process any optional arguments, this is a bit ugly. */
        uint8_t iArg = 0;
        while (iArg < pAslKeyword->cArgsOpt)
        {
            if (rtAcpiAslLexerIsPunctuator(pThis, RTACPIASLTERMINAL_PUNCTUATOR_CLOSE_BRACKET))
                break; /* The end of the argument list was reached. */

            /*
             * It is possible to have empty arguments in the list by having nothing to parse between the "," or something like ",)"
             * (like "Method(NAM, 0,,)" for example).
             */
            if (rtAcpiAslLexerIsPunctuator(pThis, RTACPIASLTERMINAL_PUNCTUATOR_COMMA))
            {
                RTACPIASL_SKIP_CURRENT_TOKEN(); /* Skip "," */

                /*
                 * If the next token is also a "," there is a hole in the argument list and we have to fill in the default,
                 * if it is ")" we reached the end.
                 */
                if (rtAcpiAslLexerIsPunctuator(pThis, RTACPIASLTERMINAL_PUNCTUATOR_CLOSE_BRACKET))
                    break;
                else if (rtAcpiAslLexerIsPunctuator(pThis, RTACPIASLTERMINAL_PUNCTUATOR_COMMA))
                {
                    pAstNd->aArgs[pAslKeyword->cArgsReq + iArg] = pAslKeyword->aArgsOpt[iArg];
                    iArg++;
                    continue; /* Continue with the next argument. */
                }

                /* So there is an argument we need to parse. */
                rc = rtAcpiTblAslParseArgument(pThis, pAslKeyword->pszOpc, iArg, pAslKeyword->aArgsOpt[iArg].enmType, &pAstNd->aArgs[pAslKeyword->cArgsReq + iArg]);
                if (RT_FAILURE(rc))
                    return rc;

                iArg++;
            }
        }

        /* Fill remaining optional arguments with the defaults. */
        for (; iArg < pAslKeyword->cArgsOpt; iArg++)
            pAstNd->aArgs[pAslKeyword->cArgsReq + iArg] = pAslKeyword->aArgsOpt[iArg];

        /* Now there must be a closing ) */
        RTACPIASL_PARSE_PUNCTUATOR(RTACPIASLTERMINAL_PUNCTUATOR_CLOSE_BRACKET, ')');
    }

    /* For keywords opening a new scope do the parsing now. */
    if (pAslKeyword->fFlags & RTACPI_AST_NODE_F_NEW_SCOPE)
    {
        RTACPIASL_PARSE_PUNCTUATOR(RTACPIASLTERMINAL_PUNCTUATOR_OPEN_CURLY_BRACKET, '{');
        rc = rtAcpiTblAslParseInner(pThis, &pAstNd->LstScopeNodes);
        if (RT_SUCCESS(rc))
            RTACPIASL_PARSE_PUNCTUATOR(RTACPIASLTERMINAL_PUNCTUATOR_CLOSE_CURLY_BRACKET, '}');
    }

    if (pAslKeyword->fFlags & RTACPI_AST_NODE_F_NS_ENTRY)
        rtAcpiNsPop(pThis->pNs);

    return rc;
}


/**
 * Parses what looks like an name string, possibly with a call.
 *
 * @returns IPRT status code.
 * @param   pThis           The ACPI compilation unit state.
 * @param   pszIde          The identifier.
 * @param   ppAstNd         Where to store the AST node on success.
 */
static int rtAcpiTblAslParseIde(PRTACPIASLCU pThis, const char *pszIde, PRTACPIASTNODE *ppAstNd)
{
    *ppAstNd = NULL;

    /* If there is a ( following this looks like a function call which can have up to 8 arguments. */
    uint8_t cArgs = 0;
    RTACPIASTARG aArgs[8]; RT_ZERO(aArgs);
    if (rtAcpiAslLexerIsPunctuator(pThis, RTACPIASLTERMINAL_PUNCTUATOR_OPEN_BRACKET))
    {
        RTACPIASL_SKIP_CURRENT_TOKEN(); /* Skip "(" */

        while (cArgs < RT_ELEMENTS(aArgs))
        {
            PRTACPIASTNODE pAstNd = NULL;
            int rc = rtAcpiTblAslParseTermArg(pThis, &pAstNd);
            if (RT_FAILURE(rc))
                return rc;

            aArgs[cArgs].enmType  = kAcpiAstArgType_AstNode;
            aArgs[cArgs].u.pAstNd = pAstNd;
            cArgs++;

            /* ")" means we are done here. */
            if (rtAcpiAslLexerIsPunctuator(pThis, RTACPIASLTERMINAL_PUNCTUATOR_CLOSE_BRACKET))
                break;

            /* Arguments are separated by "," */
            RTACPIASL_PARSE_PUNCTUATOR(RTACPIASLTERMINAL_PUNCTUATOR_COMMA, ',');
        }

        /* Now there must be a closing ) */
        RTACPIASL_PARSE_PUNCTUATOR(RTACPIASLTERMINAL_PUNCTUATOR_CLOSE_BRACKET, ')');
    }

    PRTACPIASTNODE pAstNd = rtAcpiAstNodeAlloc(kAcpiAstNodeOp_Identifier, RTACPI_AST_NODE_F_DEFAULT, cArgs);
    if (!pAstNd)
        return RTErrInfoSetF(pThis->pErrInfo, VERR_NO_MEMORY, "Failed to allocate ACPI AST node when processing identifier '%s'", pszIde);

    pAstNd->pszIde = pszIde;

    /* Fill in the arguments. */
    for (uint8_t i = 0; i < cArgs; i++)
        pAstNd->aArgs[i] = aArgs[i];

    *ppAstNd = pAstNd;
    return VINF_SUCCESS;
}


static int rtAcpiTblAslParseTermArg(PRTACPIASLCU pThis, PRTACPIASTNODE *ppAstNd)
{
    PCRTSCRIPTLEXTOKEN pTok;
    int rc = RTScriptLexQueryToken(pThis->hLexSource, &pTok);
    if (RT_FAILURE(rc))
        return RTErrInfoSetF(pThis->pErrInfo, rc, "Parser: Failed to query next token with %Rrc", rc);

    if (pTok->enmType == RTSCRIPTLEXTOKTYPE_ERROR)
        return RTErrInfoSet(pThis->pErrInfo, VERR_INVALID_PARAMETER, pTok->Type.Error.pErr->pszMsg);
    if (pTok->enmType == RTSCRIPTLEXTOKTYPE_EOS)
        return RTErrInfoSetF(pThis->pErrInfo, VERR_INVALID_PARAMETER, "Parser: Unexpected end of stream");

    PRTACPIASTNODE pAstNd = NULL;
    if (pTok->enmType == RTSCRIPTLEXTOKTYPE_KEYWORD)
    {
        uint64_t idKeyword = pTok->Type.Keyword.pKeyword->u64Val;
        if (idKeyword < RT_ELEMENTS(g_aAslOps))
        {
            RTScriptLexConsumeToken(pThis->hLexSource); /* This must come here as rtAcpiTblAslParseOp() will continue parsing. */
            rc = rtAcpiTblAslParseOp(pThis, (RTACPIASTNODEOP)idKeyword, &pAstNd);
        }
        else
            return RTErrInfoSetF(pThis->pErrInfo, VERR_INVALID_PARAMETER, "Parser: Unexpected keyword '%s' encountered", pTok->Type.Keyword.pKeyword->pszMatch);
    }
    else if (pTok->enmType == RTSCRIPTLEXTOKTYPE_IDENTIFIER)
    {
        /* We can safely consume the token here after getting the pointer to the identifier string as the string is cached and doesn't go away. */
        const char *pszIde = pTok->Type.Id.pszIde;
        RTScriptLexConsumeToken(pThis->hLexSource);
        rc = rtAcpiTblAslParseIde(pThis, pszIde, &pAstNd);
    }
    else if (pTok->enmType == RTSCRIPTLEXTOKTYPE_STRINGLIT)
    {
        pAstNd = rtAcpiAstNodeAlloc(kAcpiAstNodeOp_StringLiteral, RTACPI_AST_NODE_F_DEFAULT, 0);
        if (!pAstNd)
            return RTErrInfoSetF(pThis->pErrInfo, VERR_NO_MEMORY, "Failed to allocate ACPI AST node when processing identifier '%s'",
                                 pTok->Type.StringLit.pszString);

        pAstNd->pszStrLit = pTok->Type.StringLit.pszString;
        RTScriptLexConsumeToken(pThis->hLexSource);
    }
    else if (pTok->enmType == RTSCRIPTLEXTOKTYPE_NUMBER)
    {
        Assert(pTok->Type.Number.enmType == RTSCRIPTLEXTOKNUMTYPE_NATURAL);
        pAstNd = rtAcpiAstNodeAlloc(kAcpiAstNodeOp_Number, RTACPI_AST_NODE_F_DEFAULT, 0);
        if (!pAstNd)
            return RTErrInfoSetF(pThis->pErrInfo, VERR_NO_MEMORY, "Failed to allocate ACPI AST node when processing number '%#RX64'",
                                 pTok->Type.Number.Type.u64);

        pAstNd->u64 = pTok->Type.Number.Type.u64;
        RTScriptLexConsumeToken(pThis->hLexSource);
    }
    else
    {
        AssertFailed();
        return RTErrInfoSetF(pThis->pErrInfo, VERR_INVALID_PARAMETER, "Parser: Unexpected token encountered");
    }

    if (RT_FAILURE(rc))
    {
        if (pAstNd)
            rtAcpiAstNodeFree(pAstNd);
        return rc;
    }

    AssertPtr(pAstNd);
    *ppAstNd = pAstNd;
    return VINF_SUCCESS;
}


static int rtAcpiTblAslParseInner(PRTACPIASLCU pThis, PRTLISTANCHOR pLstStmts)
{
    for (;;)
    {
        /* Need to break out of the loop if done processing this scope (consumption is done by the caller). */
        if (rtAcpiAslLexerIsPunctuator(pThis, RTACPIASLTERMINAL_PUNCTUATOR_CLOSE_CURLY_BRACKET))
            return VINF_SUCCESS;

        PRTACPIASTNODE pAstNd = NULL;
        int rc = rtAcpiTblAslParseTermArg(pThis, &pAstNd);
        if (RT_FAILURE(rc))
            return rc;

        Assert(pAstNd);
        RTListAppend(pLstStmts, &pAstNd->NdAst);
    }
}


static int rtAcpiTblAslParserParse(PRTACPIASLCU pThis)
{
    /*
     * The first keyword must be DefinitionBlock:
     *
     *     DefinitionBlock ("SSDT.aml", "SSDT", 1, "VBOX  ", "VBOXCPUT", 2)
     */
    RTACPIASL_PARSE_KEYWORD(RTACPIASLTERMINAL_KEYWORD_DEFINITION_BLOCK, "DefinitionBlock");
    RTACPIASL_PARSE_PUNCTUATOR(RTACPIASLTERMINAL_PUNCTUATOR_OPEN_BRACKET, '(');
    RTACPIASL_PARSE_STRING_LIT(pszOutFile);
    RT_NOREF(pszOutFile); /* We ignore the output file hint. */
    RTACPIASL_PARSE_PUNCTUATOR(RTACPIASLTERMINAL_PUNCTUATOR_COMMA, ',');
    RTACPIASL_PARSE_STRING_LIT(pszTblSig);
    RTACPIASL_PARSE_PUNCTUATOR(RTACPIASLTERMINAL_PUNCTUATOR_COMMA, ',');
    RTACPIASL_PARSE_NATURAL(u64ComplianceRev);
    RTACPIASL_PARSE_PUNCTUATOR(RTACPIASLTERMINAL_PUNCTUATOR_COMMA, ',');
    RTACPIASL_PARSE_STRING_LIT(pszOemId);
    RTACPIASL_PARSE_PUNCTUATOR(RTACPIASLTERMINAL_PUNCTUATOR_COMMA, ',');
    RTACPIASL_PARSE_STRING_LIT(pszOemTblId);
    RTACPIASL_PARSE_PUNCTUATOR(RTACPIASLTERMINAL_PUNCTUATOR_COMMA, ',');
    RTACPIASL_PARSE_NATURAL(u64OemRev);
    RTACPIASL_PARSE_PUNCTUATOR(RTACPIASLTERMINAL_PUNCTUATOR_CLOSE_BRACKET, ')');

    /* Some additional checks. */
    uint32_t u32TblSig = ACPI_TABLE_HDR_SIGNATURE_MISC;
    if (!strcmp(pszTblSig, "DSDT"))
        u32TblSig = ACPI_TABLE_HDR_SIGNATURE_DSDT;
    else if (!strcmp(pszTblSig, "SSDT"))
        u32TblSig = ACPI_TABLE_HDR_SIGNATURE_SSDT;

    if (u32TblSig == ACPI_TABLE_HDR_SIGNATURE_MISC)
        return RTErrInfoSetF(pThis->pErrInfo, VERR_INVALID_PARAMETER, "Table signature must be either 'DSDT' or 'SSDT': %s", pszTblSig);

    if (u64ComplianceRev > UINT8_MAX)
        return RTErrInfoSetF(pThis->pErrInfo, VERR_INVALID_PARAMETER, "Compliance revision %RU64 is out of range, must be in range [0..255]", u64ComplianceRev);

    if (strlen(pszOemId) > 6)
        return RTErrInfoSetF(pThis->pErrInfo, VERR_INVALID_PARAMETER, "OEM ID string must be at most 6 characters long");

    if (strlen(pszOemTblId) > 8)
        return RTErrInfoSetF(pThis->pErrInfo, VERR_INVALID_PARAMETER, "OEM table ID string must be at most 8 characters long");

    if (u64OemRev > UINT32_MAX)
        return RTErrInfoSetF(pThis->pErrInfo, VERR_INVALID_PARAMETER, "OEM revision ID %RU64 is out of range, must fit into 32-bit unsigned integer", u64OemRev);

    int rc = RTAcpiTblCreate(&pThis->hAcpiTbl, u32TblSig, (uint8_t)u64ComplianceRev, pszOemId,
                             pszOemTblId, (uint32_t)u64OemRev, "VBOX", RTBldCfgRevision());
    if (RT_SUCCESS(rc))
    {
        RTACPIASL_PARSE_PUNCTUATOR(RTACPIASLTERMINAL_PUNCTUATOR_OPEN_CURLY_BRACKET, '{');
        rc = rtAcpiTblAslParseInner(pThis, &pThis->LstStmts);
        if (RT_SUCCESS(rc))
        {
            RTACPIASL_PARSE_PUNCTUATOR(RTACPIASLTERMINAL_PUNCTUATOR_CLOSE_CURLY_BRACKET, '}');
            rc = rtAcpiAslParserConsumeEos(pThis); /* No junk after the final closing bracket. */
        }
    }
    else
        rc = RTErrInfoSetF(pThis->pErrInfo, rc, "Call to RTAcpiTblCreate() failed");

    return rc;
}


DECLHIDDEN(int) rtAcpiTblConvertFromAslToAml(RTVFSIOSTREAM hVfsIosOut, RTVFSIOSTREAM hVfsIosIn, PRTERRINFO pErrInfo)
{
    int rc;
    PRTACPIASLCU pThis = (PRTACPIASLCU)RTMemAllocZ(sizeof(*pThis));
    if (pThis)
    {
        pThis->hVfsIosIn  = hVfsIosIn;
        pThis->pErrInfo   = pErrInfo;
        RTListInit(&pThis->LstStmts);

        pThis->pNs = rtAcpiNsCreate();
        if (pThis->pNs)
        {
            rc = RTScriptLexCreateFromReader(&pThis->hLexSource, rtAcpiAslLexerRead,
                                             NULL /*pfnDtor*/, pThis /*pvUser*/, 0 /*cchBuf*/,
                                             NULL /*phStrCacheId*/, NULL /*phStrCacheStringLit*/,
                                             &s_AslLexCfg);
            if (RT_SUCCESS(rc))
            {
                rc = rtAcpiTblAslParserParse(pThis);
                if (RT_SUCCESS(rc))
                {
                    /* 2. - Optimize AST (constant folding, etc). */

                    /* 3. - Traverse AST and output table. */
                    PRTACPIASTNODE pIt;
                    RTListForEach(&pThis->LstStmts, pIt, RTACPIASTNODE, NdAst)
                    {
                        rc = rtAcpiAstNodeTransform(pIt, pErrInfo);
                        if (RT_FAILURE(rc))
                            break;

                        rc = rtAcpiAstDumpToTbl(pIt, pThis->hAcpiTbl);
                        if (RT_FAILURE(rc))
                            break;
                    }

                    /* Finalize and write to the VFS I/O stream. */
                    if (RT_SUCCESS(rc))
                    {
                        rc = RTAcpiTblFinalize(pThis->hAcpiTbl);
                        if (RT_SUCCESS(rc))
                        {
                            rc = RTAcpiTblDumpToVfsIoStrm(pThis->hAcpiTbl, RTACPITBLTYPE_AML, hVfsIosOut);
                            if (RT_FAILURE(rc))
                                rc = RTErrInfoSetF(pErrInfo, rc, "Writing the ACPI table failed with %Rrc", rc);
                        }
                        else
                            rc = RTErrInfoSetF(pErrInfo, rc, "Finalizing the ACPI table failed with %Rrc", rc);
                    }
                    else
                        rc = RTErrInfoSetF(pErrInfo, rc, "Dumping AST to ACPI table failed with %Rrc", rc);
                }

                RTScriptLexDestroy(pThis->hLexSource);
            }
            else
                rc = RTErrInfoSetF(pErrInfo, rc, "Creating the ASL lexer failed with %Rrc", rc);

            /* Destroy the AST nodes. */
            PRTACPIASTNODE pIt, pItNext;
            RTListForEachSafe(&pThis->LstStmts, pIt, pItNext, RTACPIASTNODE, NdAst)
            {
                RTListNodeRemove(&pIt->NdAst);
                rtAcpiAstNodeFree(pIt);
            }

            rtAcpiNsDestroy(pThis->pNs);
        }
        else
            rc = RTErrInfoSetF(pErrInfo, VERR_NO_MEMORY, "Out of memory allocating the ACPI namespace state");

        RTMemFree(pThis);
    }
    else
        rc = RTErrInfoSetF(pErrInfo, VERR_NO_MEMORY, "Out of memory allocating the ASL compilation unit state");

    return rc;
}
