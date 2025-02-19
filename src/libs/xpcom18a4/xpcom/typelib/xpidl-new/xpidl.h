/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/*
 * Intramodule declarations.
 */

#ifndef __xpidl_h
#define __xpidl_h

#include <iprt/errcore.h>
#include <iprt/list.h>
#include <iprt/script.h>

#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <xpt_struct.h>


/**
 * An include path.
 */
typedef struct XPIDLINCLUDEDIR
{
    /** Node for the list of include paths. */
    RTLISTNODE          NdIncludes;
    /** The zero terminated include path. */
    const char          *pszPath;
} XPIDLINCLUDEDIR;
/** Pointer to an include path. */
typedef XPIDLINCLUDEDIR *PXPIDLINCLUDEDIR;
/** Pointer to a const include path. */
typedef const XPIDLINCLUDEDIR *PCXPIDLINCLUDEDIR;


/**
 * The input stream.
 */
typedef struct XPIDLINPUT
{
    /** Node for the list of inputs. */
    RTLISTNODE          NdInput;
    /** The list of includes this input generated. */
    RTLISTANCHOR        LstIncludes;
    /** The basename for this input. */
    char                *pszBasename;
    /** The filename for this input. */
    char                *pszFilename;
    /** The lexer instance for this input. */
    RTSCRIPTLEX         hIdlLex;
} XPIDLINPUT;
/** Pointer to an input stream. */
typedef XPIDLINPUT *PXPIDLINPUT;
/** Pointer to a const input stream. */
typedef const XPIDLINPUT *PCXPIDLINPUT;


/**
 * IDL node type.
 */
typedef enum XPIDLNDTYPE
{
    kXpidlNdType_Invalid = 0,
    kXpidlNdType_RawBlock,
    kXpidlNdType_Typedef,
    kXpidlNdType_Native,
    kXpidlNdType_Interface,
    kXpidlNdType_Forward_Decl
} XPIDLNDTYPE;


/**
 * A node attribute.
 */
typedef struct XPIDLATTR
{
    /** The attribute name. */
    const char          *pszName;
    /** The value assigned if any. */
    const char          *pszVal;
} XPIDLATTR;
/** Pointer to an attribute. */
typedef XPIDLATTR *PXPIDLATTR;
/** Pointer to a const attribute. */
typedef const XPIDLATTR *PCXPIDLATTR;


/** Pointer to an IDL node. */
typedef struct XPIDLNODE *PXPIDLNODE;
/** Pointer to a const IDL node. */
typedef const struct XPIDLNODE *PCXPIDLNODE;

/**
 * IDL node.
 */
typedef struct XPIDLNODE
{
    /** Node for the list this node is in. */
    RTLISTNODE          NdLst;
    /** The parent node (if any). */
    PCXPIDLNODE         pParent;
    /** The input stream this node was generated from (via #include's). */
    PCXPIDLINPUT        pInput;
    /** The node type. */
    XPIDLNDTYPE         enmType;
    /** Node type dependent data. */
    union
    {
        struct
        {
            const char *pszRaw;
            size_t     cchRaw;
        } RawBlock;
    } u;
} XPIDLNODE;


/**
 * The IDL parsing state.
 */
typedef struct XPIDLPARSE
{
    /** List of input files. */
    RTLISTANCHOR        LstInputs;
    /** The list of XPIDL nodes from the root. */
    RTLISTANCHOR        LstNodes;
    /** Extended error info. */
    RTERRINFOSTATIC     ErrInfo;
} XPIDLPARSE;
/** Pointer to an IDL parsing state. */
typedef XPIDLPARSE *PXPIDLPARSE;
/** Pointer to a const IDL parsing state. */
typedef const XPIDLPARSE *PCXPIDLPARSE;


/*
 * Internal operation flags.
 */
extern bool enable_debug;
extern bool enable_warnings;
extern bool verbose_mode;
extern bool emit_typelib_annotations;
extern bool explicit_output_filename;

extern PRUint8  major_version;
extern PRUint8  minor_version;

typedef struct TreeState TreeState;

/*
 * A function to handle an IDL_tree type.
 */
typedef bool (*nodeHandler)(TreeState *);

/*
 * Struct containing functions to define the behavior of a given output mode.
 */
typedef struct backend {
    nodeHandler *dispatch_table; /* nodeHandlers table, indexed by node type. */
    nodeHandler emit_prolog;     /* called at beginning of output generation. */
    nodeHandler emit_epilog;     /* called at end. */
} backend;

/* Function that produces a struct of output-generation functions */
typedef backend *(*backendFactory)();

extern backend *xpidl_header_dispatch(void);
extern backend *xpidl_typelib_dispatch(void);

typedef struct ModeData {
    char               *mode;
    char               *modeInfo;
    char               *suffix;
    backendFactory     factory;
} ModeData;


struct TreeState {
    FILE             *file;
    /* Maybe supplied by -o. Not related to (g_)basename from string.h or glib */
    char             *basename;
    RTLISTANCHOR     *base_includes;
    nodeHandler      *dispatch;
    void             *priv;     /* mode-private data */
};

/*
 * Process an IDL file, generating InterfaceInfo, documentation and headers as
 * appropriate.
 */
int
xpidl_process_idl(char *filename, PRTLISTANCHOR pLstIncludePaths,
                  char *file_basename, ModeData *mode);

/*
 * Wrapper whines to stderr then exits after null return from malloc or strdup.
 */
void *
xpidl_malloc(size_t nbytes);

char *
xpidl_strdup(const char *s);

/*
 * Return a newly allocated string to the start of the base filename of path.
 * Free with g_free().
 */
char *
xpidl_basename(const char * path);

/*
 * Process an XPIDL node and its kids, if any.
 */
bool
xpidl_process_node(TreeState *state);

/*
 * Write a newline folllowed by an indented, one-line comment containing IDL
 * source decompiled from state->tree.
 */
void
xpidl_write_comment(TreeState *state, int indent);



/*
 * Functions for parsing and printing UUIDs.
 */

/*
 * How large should the buffer supplied to xpidl_sprint_IID be?
 */
#define UUID_LENGTH 37

/*
 * Print an iid to into a supplied buffer; the buffer should be at least
 * UUID_LENGTH bytes.
 */
bool
xpidl_sprint_iid(nsID *iid, char iidbuf[]);

/*
 * Parse a uuid string into an nsID struct.  We cannot link against libxpcom,
 * so we re-implement nsID::Parse here.
 */
bool
xpidl_parse_iid(nsID *id, const char *str);


#if 0
/* Try to common a little node-handling stuff. */

/* is this node from an aggregate type (interface)? */
#define UP_IS_AGGREGATE(node)                                                 \
    (IDL_NODE_UP(node) &&                                                     \
     (IDL_NODE_TYPE(IDL_NODE_UP(node)) == IDLN_INTERFACE ||                   \
      IDL_NODE_TYPE(IDL_NODE_UP(node)) == IDLN_FORWARD_DCL))

#define UP_IS_NATIVE(node)                                                    \
    (IDL_NODE_UP(node) &&                                                     \
     IDL_NODE_TYPE(IDL_NODE_UP(node)) == IDLN_NATIVE)

/* is this type output in the form "<foo> *"? */
#define STARRED_TYPE(node) (IDL_NODE_TYPE(node) == IDLN_TYPE_STRING ||        \
                            IDL_NODE_TYPE(node) == IDLN_TYPE_WIDE_STRING ||   \
                            (IDL_NODE_TYPE(node) == IDLN_IDENT &&             \
                             UP_IS_AGGREGATE(node)))

#define DIPPER_TYPE(node)                                                     \
    (NULL != IDL_tree_property_get(node, "domstring")  ||                     \
     NULL != IDL_tree_property_get(node, "utf8string") ||                     \
     NULL != IDL_tree_property_get(node, "cstring")    ||                     \
     NULL != IDL_tree_property_get(node, "astring"))

/*
 * Find the underlying type of an identifier typedef.  Returns NULL
 * (and doesn't complain) on failure.
 */
IDL_tree /* IDL_TYPE_DCL */
find_underlying_type(IDL_tree typedef_ident);

/*
 * Check that const declarations match their stated sign and are of the
 * appropriate types.
 */
gboolean
verify_const_declaration(IDL_tree const_tree);

/*
 * Check that scriptable attributes in scriptable interfaces actually are.
 */
gboolean
verify_attribute_declaration(IDL_tree method_tree);

/*
 * Perform various validation checks on methods.
 */
gboolean
verify_method_declaration(IDL_tree method_tree);

/*
 * Verifies the interface declaration
 */
gboolean
verify_interface_declaration(IDL_tree method_tree);

/*
 * Verify that a native declaration has an associated C++ expression, i.e. that
 * it's of the form native <idl-name>(<c++-name>)
 */
gboolean
check_native(TreeState *state);

void
printlist(FILE *outfile, GSList *slist);
#endif

#endif /* __xpidl_h */
