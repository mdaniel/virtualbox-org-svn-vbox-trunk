/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Python XPCOM language bindings.
 *
 * The Initial Developer of the Original Code is
 * ActiveState Tool Corp.
 * Portions created by the Initial Developer are Copyright (C) 2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Mark Hammond <mhammond@skippinet.com.au> (original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

// PyXPCOM.h - the main header file for the Python XPCOM support.
//
// This code is part of the XPCOM extensions for Python.
//
// Written May 2000 by Mark Hammond.
//
// Based heavily on the Python COM support, which is
// (c) Mark Hammond and Greg Stein.
//
// (c) 2000, ActiveState corp.

#ifndef __PYXPCOM_H__
#define __PYXPCOM_H__

#include "nsIWeakReference.h"
#include "nsIInterfaceInfoManager.h"
#include "nsIClassInfo.h"
#include "nsIComponentManager.h"
#include "nsIComponentManagerObsolete.h"
#include "nsIServiceManager.h"
#include "nsIInputStream.h"
#include "nsIVariant.h"
#include "nsIModule.h"

#include "nsXPIDLString.h"
#include "nsCRT.h"
#include "xptcall.h"
#include "xpt_xdr.h"

#ifdef VBOX_DEBUG_LIFETIMES
# include <iprt/critsect.h>
# include <iprt/list.h>
# include <iprt/once.h>
#endif

#ifdef HAVE_LONG_LONG
	// Mozilla also defines this - we undefine it to
	// prevent a compiler warning.
#	undef HAVE_LONG_LONG
#endif // HAVE_LONG_LONG

#ifdef _POSIX_C_SOURCE // Ditto here
#	undef _POSIX_C_SOURCE
#endif // _POSIX_C_SOURCE

#ifdef VBOX_PYXPCOM
// unfortunatelly, if SOLARIS is defined Python porting layer
// defines gethostname() in invalid fashion what kills compilation
# ifdef SOLARIS
#  undef SOLARIS
#  define SOLARIS_WAS_DEFINED
# endif

// Python.h/pyconfig.h redefines _XOPEN_SOURCE on some hosts
# ifdef _XOPEN_SOURCE
#  define VBOX_XOPEN_SOURCE_DEFINED _XOPEN_SOURCE
#  undef _XOPEN_SOURCE
# endif

#endif /* VBOX_PYXPCOM */

#include <Python.h>

#ifdef VBOX_PYXPCOM

# ifdef SOLARIS_WAS_DEFINED
#  define SOLARIS
#  undef SOLARIS_WAS_DEFINED
# endif

// restore the old value of _XOPEN_SOURCE if not defined by Python.h/pyconfig.h
# if !defined(_XOPEN_SOURCE) && defined(VBOX_XOPEN_SOURCE_DEFINED)
#  define _XOPEN_SOURCE VBOX_XOPEN_SOURCE_DEFINED
# endif
# undef VBOX_XOPEN_SOURCE_DEFINED

# if (PY_VERSION_HEX <= 0x02040000)
// although in more recent versions of Python this type is ssize_t, earlier
// it was used as int
typedef int Py_ssize_t;
# endif

# if (PY_VERSION_HEX <= 0x02030000)
// this one not defined before
inline PyObject *PyBool_FromLong(long ok)
{
  PyObject *result;
   if (ok)
     result = Py_True;
   else
     result = Py_False;
   Py_INCREF(result);
   return result;
}
# endif

# if PY_MAJOR_VERSION >= 3
#  define PyInt_FromLong(l) PyLong_FromLong(l)
#  define PyInt_Check(o) PyLong_Check(o)
#  define PyInt_AsLong(o) PyLong_AsLong(o)
#  define PyNumber_Int(o) PyNumber_Long(o)
#  if !defined(Py_LIMITED_API) && PY_VERSION_HEX <= 0x03030000 /* 3.3 added PyUnicode_AsUTF8AndSize */
#   ifndef PyUnicode_AsUTF8
#    define PyUnicode_AsUTF8(o) _PyUnicode_AsString(o)
#   endif
#   ifndef PyUnicode_AsUTF8AndSize
#    define PyUnicode_AsUTF8AndSize(o,s) _PyUnicode_AsStringAndSize(o,s)
#   endif
#  endif
typedef struct PyMethodChain
{
    PyMethodDef *methods;
    struct PyMethodChain *link;
} PyMethodChain;
# endif

#endif /* VBOX_PYXPCOM */

#ifdef BUILD_PYXPCOM
    /* We are building the main dll */
#   define PYXPCOM_EXPORT NS_EXPORT
#else
    /* This module uses the dll */
#   define PYXPCOM_EXPORT NS_IMPORT
#endif // BUILD_PYXPCOM

// An IID we treat as NULL when passing as a reference.
extern PYXPCOM_EXPORT nsIID Py_nsIID_NULL;

class Py_nsISupports;


/** @name VBox limited API hacks:
 * @{  */
#ifndef Py_LIMITED_API

# define PyXPCOM_ObTypeName(obj) (Py_TYPE(obj)->tp_name)

#else /* Py_LIMITED_API */

# if PY_VERSION_HEX <= 0x03030000
#  error "Py_LIMITED_API mode only works for Python 3.3 and higher."
# endif

const char *PyXPCOMGetObTypeName(PyTypeObject *pTypeObj);
# define PyXPCOM_ObTypeName(obj) PyXPCOMGetObTypeName(Py_TYPE(obj))

# if Py_LIMITED_API < 0x030A0000
/* Note! While we should not technically be using PyUnicode_AsUTF8AndSize, it was
   made part of the limited API in 3.10 (see https://bugs.python.org/issue41784 and
   https://github.com/python/cpython/commit/a05195ac61f1908ac5990cccb5aa82442bdaf15d). */
extern "C" PyAPI_FUNC(const char *) PyUnicode_AsUTF8AndSize(PyObject *, Py_ssize_t *);
# endif

/* PyUnicode_AsUTF8 is just PyUnicode_AsUTF8AndSize without returning a size. */
# define PyUnicode_AsUTF8(o) PyUnicode_AsUTF8AndSize(o, NULL)

DECLINLINE(int) PyRun_SimpleString(const char *pszCode)
{
    /* Get the main mode dictionary: */
    PyObject *pMainMod = PyImport_AddModule("__main__");
    if (pMainMod) {
	PyObject *pMainModDict = PyModule_GetDict(pMainMod);

	/* Compile and run the code. */
	PyObject *pCodeObject = Py_CompileString(pszCode, "PyXPCOM", Py_file_input);
	if (pCodeObject) {
	    PyObject *pResult = PyEval_EvalCode(pCodeObject, pMainModDict, pMainModDict);
	    Py_DECREF(pCodeObject);
	    if (pResult) {
		Py_DECREF(pResult);
		return 0;
	    }
	    PyErr_Print();
	}
    }
    return -1;
}

DECLINLINE(PyObject *) PyTuple_GET_ITEM(PyObject *pTuple, Py_ssize_t idx)
{
    return PyTuple_GetItem(pTuple, idx);
}

DECLINLINE(int) PyTuple_SET_ITEM(PyObject *pTuple, Py_ssize_t idx, PyObject *pItem)
{
    int rc = PyTuple_SetItem(pTuple, idx, pItem); /* Steals pItem ref, just like PyTuple_SET_ITEM. */
    Assert(rc == 0);
    return rc;
}

DECLINLINE(int) PyList_SET_ITEM(PyObject *pList, Py_ssize_t idx, PyObject *pItem)
{
    int rc = PyList_SetItem(pList, idx, pItem); /* Steals pItem ref, just like PyList_SET_ITEM. */
    Assert(rc == 0);
    return rc;
}

DECLINLINE(Py_ssize_t) PyBytes_GET_SIZE(PyObject *pBytes)
{
    return PyBytes_Size(pBytes);
}

DECLINLINE(const char *) PyBytes_AS_STRING(PyObject *pBytes)
{
    return PyBytes_AsString(pBytes);
}

DECLINLINE(Py_ssize_t) PyUnicode_GET_SIZE(PyObject *pUnicode)
{
    /* Note! Currently only used for testing for zero or 1 codepoints, so we don't
       really need to deal with the different way these two treats surrogate pairs. */
# if Py_LIMITED_API >= 0x03030000
    return PyUnicode_GetLength(pUnicode);
# else
    return PyUnicode_GetSize(pUnicode);
# endif
}

# define PyObject_CheckBuffer(pAllegedBuffer) PyObject_CheckReadBuffer(pAllegedBuffer)

# include <iprt/asm.h>
DECLINLINE(Py_hash_t) _Py_HashPointer(void *p)
{
    Py_hash_t uHash = (Py_hash_t)RT_CONCAT(ASMRotateRightU,ARCH_BITS)((uintptr_t)p, 4);
    return uHash != -1 ? uHash : -2;
}

#endif /* Py_LIMITED_API */
/** @} */


/*************************************************************************
**************************************************************************

 Error and exception related function.

**************************************************************************
*************************************************************************/

#define NS_PYXPCOM_NO_SUCH_METHOD \
  NS_ERROR_GENERATE_SUCCESS(NS_ERROR_MODULE_PYXPCOM, 0)

// The exception object (loaded from the xpcom .py code)
extern PYXPCOM_EXPORT PyObject *PyXPCOM_Error;

// Client related functions - generally called by interfaces before
// they return NULL back to Python to indicate the error.
// All these functions return NULL so interfaces can generally
// just "return PyXPCOM_BuildPyException(hr, punk, IID_IWhatever)"
PYXPCOM_EXPORT PyObject *PyXPCOM_BuildPyException(nsresult res);

#ifdef VBOX
// Build human readable error message out of XPCOM error
PYXPCOM_EXPORT PyObject *PyXPCOM_BuildErrorMessage(nsresult r);
#endif

// Used in gateways to handle the current Python exception
// NOTE: this function assumes it is operating within the Python context
PYXPCOM_EXPORT nsresult PyXPCOM_SetCOMErrorFromPyException();

// Write current exception and traceback to a string.
PYXPCOM_EXPORT bool PyXPCOM_FormatCurrentException(nsCString &streamout);
// Write specified exception and traceback to a string.
PYXPCOM_EXPORT bool PyXPCOM_FormatGivenException(nsCString &streamout,
                                PyObject *exc_typ, PyObject *exc_val,
                                PyObject *exc_tb);

// A couple of logging/error functions.  These probably end up
// being written to the console service.

// Log a warning for the user - something at runtime
// they may care about, but nothing that prevents us actually
// working.
// As it's designed for user error/warning, it exists in non-debug builds.
PYXPCOM_EXPORT void PyXPCOM_LogWarning(const char *fmt, ...);

// Log an error for the user - something that _has_ prevented
// us working.  This is probably accompanied by a traceback.
// As it's designed for user error/warning, it exists in non-debug builds.
PYXPCOM_EXPORT void PyXPCOM_LogError(const char *fmt, ...);

// The raw one
PYXPCOM_EXPORT void PyXPCOM_Log(const char *level, const nsCString &msg);

#ifdef DEBUG
// Mainly designed for developers of the XPCOM package.
// Only enabled in debug builds.
PYXPCOM_EXPORT void PyXPCOM_LogDebug(const char *fmt, ...);
#define PYXPCOM_LOG_DEBUG PyXPCOM_LogDebug
#else
#define PYXPCOM_LOG_DEBUG()
#endif // DEBUG

// Some utility converters
// moz strings to PyObject.
PYXPCOM_EXPORT PyObject *PyObject_FromNSString( const nsACString &s,
                                                PRBool bAssumeUTF8 = PR_FALSE );
PYXPCOM_EXPORT PyObject *PyObject_FromNSString( const nsAString &s );
PYXPCOM_EXPORT PyObject *PyObject_FromNSString( const PRUnichar *s,
                                                PRUint32 len = (PRUint32)-1);

// PyObjects to moz strings.  As per the moz string guide, we pass a reference
// to an abstract string
PYXPCOM_EXPORT PRBool PyObject_AsNSString( PyObject *ob, nsAString &aStr);

// Variants.
PYXPCOM_EXPORT nsresult PyObject_AsVariant( PyObject *ob, nsIVariant **aRet);
PYXPCOM_EXPORT PyObject *PyObject_FromVariant( Py_nsISupports *parent,
                                               nsIVariant *v);

// Interfaces - these are the "official" functions
PYXPCOM_EXPORT PyObject *PyObject_FromNSInterface( nsISupports *aInterface,
                                                   const nsIID &iid,
                                                   PRBool bMakeNicePyObject = PR_TRUE);

/*************************************************************************
**************************************************************************

 Support for CALLING (ie, using) interfaces.

**************************************************************************
*************************************************************************/

typedef Py_nsISupports* (* PyXPCOM_I_CTOR)(nsISupports *, const nsIID &);

//////////////////////////////////////////////////////////////////////////
// class PyXPCOM_TypeObject
// Base class for (most of) the type objects.

#ifndef Py_LIMITED_API
class PYXPCOM_EXPORT PyXPCOM_TypeObject : public PyTypeObject {
#else
class PYXPCOM_EXPORT PyXPCOM_TypeObject : public PyObject {
#endif
public:
	PyXPCOM_TypeObject(
		const char *name,
		PyXPCOM_TypeObject *pBaseType,
		int typeSize,
		struct PyMethodDef* methodList,
		PyXPCOM_I_CTOR ctor);
	~PyXPCOM_TypeObject();

	PyMethodChain chain;
	PyXPCOM_TypeObject *baseType;
	PyXPCOM_I_CTOR ctor;

	static PRBool IsType(PyTypeObject *t);
	// Static methods for the Python type.
	static void Py_dealloc(PyObject *ob);
	static PyObject *Py_repr(PyObject *ob);
	static PyObject *Py_str(PyObject *ob);
	static PyObject *Py_getattr(PyObject *self, char *name);
	static int Py_setattr(PyObject *op, char *name, PyObject *v);
	static int Py_cmp(PyObject *ob1, PyObject *ob2);
	static PyObject *Py_richcmp(PyObject *ob1, PyObject *ob2, int op);
#if PY_VERSION_HEX >= 0x03020000
	static Py_hash_t Py_hash(PyObject *self);
#else
	static long Py_hash(PyObject *self);
#endif
#ifdef Py_LIMITED_API
        PyTypeObject *m_pTypeObj; /**< The python type object we wrap. */
#endif
};

//////////////////////////////////////////////////////////////////////////
// class Py_nsISupports
// This class serves 2 purposes:
// * It is a base class for other interfaces we support "natively"
// * It is instantiated for _all_ other interfaces.
//
// This is different than win32com, where a PyIUnknown only
// ever holds an IUnknown - but here, we could be holding
// _any_ interface.
class PYXPCOM_EXPORT Py_nsISupports : public PyObject
{
public:
	// Check if a Python object can safely be cast to an Py_nsISupports,
	// and optionally check that the object is wrapping the specified
	// interface.
	static PRBool Check( PyObject *ob, const nsIID &checkIID = Py_nsIID_NULL) {
		Py_nsISupports *self = static_cast<Py_nsISupports *>(ob);
		if (ob==NULL || !PyXPCOM_TypeObject::IsType(ob->ob_type ))
			return PR_FALSE;
		if (!checkIID.Equals(Py_nsIID_NULL))
			return self->m_iid.Equals(checkIID) != 0;
		return PR_TRUE;
	}
	// Get the nsISupports interface from the PyObject WITH NO REF COUNT ADDED
	static nsISupports *GetI(PyObject *self, nsIID *ret_iid = NULL);
	nsCOMPtr<nsISupports> m_obj;
	nsIID m_iid;
#ifdef Py_LIMITED_API
        /** Because PyXPCOM_TypeObject cannot inherit from PyTypeObject in
         * Py_LIMITED_API mode, we cannot use ob_type to get to the method list.
         * Instead of we store it here. */
        PyXPCOM_TypeObject *m_pMyTypeObj;
#endif

	// Given an nsISupports and an Interface ID, create and return an object
	// Does not QI the object - the caller must ensure the nsISupports object
	// is really a pointer to an object identified by the IID (although
	// debug builds should check this)
	// PRBool bMakeNicePyObject indicates if we should call back into
	//  Python to wrap the object.  This allows Python code to
	//  see the correct xpcom.client.Interface object even when calling
	//  xpcom functions directly from C++.
	// NOTE: There used to be a bAddRef param to this as an internal
	// optimization, but  since removed.  This function *always* takes a
	// reference to the nsISupports.
	static PyObject *PyObjectFromInterface(nsISupports *ps,
	                                       const nsIID &iid,
	                                       PRBool bMakeNicePyObject = PR_TRUE,
	                                       PRBool bIsInternalCall = PR_FALSE);

	// Given a Python object that is a registered COM type, return a given
	// interface pointer on its underlying object, with a NEW REFERENCE ADDED.
	// bTryAutoWrap indicates if a Python instance object should attempt to
	//  be automatically wrapped in an XPCOM object.  This is really only
	//  provided to stop accidental recursion should the object returned by
	//  the wrap process itself be in instance (where it should already be
	//  a COM object.
	// If |iid|==nsIVariant, then arbitary Python objects will be wrapped
	// in an nsIVariant.
	static PRBool InterfaceFromPyObject(
		PyObject *ob,
		const nsIID &iid,
		nsISupports **ppret,
		PRBool bNoneOK,
		PRBool bTryAutoWrap = PR_TRUE);

	// Given a Py_nsISupports, return an interface.
	// Object *must* be Py_nsISupports - there is no
	// "autowrap", no "None" support, etc
	static PRBool InterfaceFromPyISupports(PyObject *ob,
	                                       const nsIID &iid,
	                                       nsISupports **ppv);

	static Py_nsISupports *Constructor(nsISupports *pInitObj, const nsIID &iid);
	// The Python methods
	static PyObject *QueryInterface(PyObject *self, PyObject *args);

	// Internal (sort-of) objects.
	static NS_EXPORT_STATIC_MEMBER_(PyXPCOM_TypeObject) *type;
	static NS_EXPORT_STATIC_MEMBER_(PyMethodDef) methods[];
	static PyObject *mapIIDToType;
	static void SafeRelease(Py_nsISupports *ob);
#ifndef Py_LIMITED_API
	static void RegisterInterface( const nsIID &iid, PyTypeObject *t);
#else
	static void RegisterInterface( const nsIID &iid, PyXPCOM_TypeObject *t);
#endif
	static void InitType();
#ifdef VBOX_DEBUG_LIFETIMES
	static void dumpList(void);
	static void dumpListToStdOut(void);
#endif

	virtual ~Py_nsISupports();
	virtual PyObject *getattr(const char *name);
	virtual int setattr(const char *name, PyObject *val);
	// A virtual function to sub-classes can customize the way
	// nsISupports objects are returned from their methods.
	// ps is a new object just obtained from some operation performed on us
	virtual PyObject *MakeInterfaceResult(nsISupports *ps, const nsIID &iid,
	                                      PRBool bMakeNicePyObject = PR_TRUE) {
		return PyObjectFromInterface(ps, iid, bMakeNicePyObject);
	}

protected:
	// ctor is protected - must create objects via
	// PyObjectFromInterface()
	Py_nsISupports(nsISupports *p,
		            const nsIID &iid,
#ifndef Py_LIMITED_API
			    PyTypeObject *type);
#else
			    PyXPCOM_TypeObject *type);
#endif

	// Make a default wrapper for an ISupports (which is an
	// xpcom.client.Component instance)
	static PyObject *MakeDefaultWrapper(PyObject *pyis, const nsIID &iid);

#ifdef VBOX_DEBUG_LIFETIMES
	static DECLCALLBACK(int32_t) initOnceCallback(void *pvUser1);

	RTLISTNODE              m_ListEntry; /**< List entry. */

	static RTONCE           g_Once;      /**< Init list and critsect once. */
	static RTCRITSECT       g_CritSect;  /**< Critsect protecting the list. */
	static RTLISTANCHOR     g_List;      /**< List of live interfaces.*/
#endif
};

// Python/XPCOM IID support
class PYXPCOM_EXPORT Py_nsIID : public PyObject
{
public:
	Py_nsIID(const nsIID &riid);
	nsIID m_iid;

	PRBool
	IsEqual(const nsIID &riid) {
		return m_iid.Equals(riid);
	}

	PRBool
	IsEqual(PyObject *ob) {
		return ob &&
#ifndef Py_LIMITED_API
		       ob->ob_type== &type &&
#else
		       ob->ob_type == s_pType &&
#endif
		       m_iid.Equals(((Py_nsIID *)ob)->m_iid);
	}

	PRBool
	IsEqual(Py_nsIID &iid) {
		return m_iid.Equals(iid.m_iid);
	}

	static PyObject *
	PyObjectFromIID(const nsIID &iid) {
		return new Py_nsIID(iid);
	}

	static PRBool IIDFromPyObject(PyObject *ob, nsIID *pRet);
	/* Python support */
	static PyObject *PyTypeMethod_getattr(PyObject *self, char *name);
#if PY_MAJOR_VERSION <= 2
	static int PyTypeMethod_compare(PyObject *self, PyObject *ob);
#endif
	static PyObject *PyTypeMethod_richcompare(PyObject *self, PyObject *ob, int op);
	static PyObject *PyTypeMethod_repr(PyObject *self);
#if PY_VERSION_HEX >= 0x03020000
	static Py_hash_t PyTypeMethod_hash(PyObject *self);
#else
	static long PyTypeMethod_hash(PyObject *self);
#endif
	static PyObject *PyTypeMethod_str(PyObject *self);
	static void PyTypeMethod_dealloc(PyObject *self);
#ifndef Py_LIMITED_API
	static NS_EXPORT_STATIC_MEMBER_(PyTypeObject) type;
#else
	static NS_EXPORT_STATIC_MEMBER_(PyTypeObject *) s_pType;
        static PyTypeObject *GetTypeObject(void);
#endif
	static NS_EXPORT_STATIC_MEMBER_(PyMethodDef) methods[];
};

///////////////////////////////////////////////////////
//
// Helper classes for managing arrays of variants.
class PythonTypeDescriptor; // Forward declare.

class PYXPCOM_EXPORT PyXPCOM_InterfaceVariantHelper {
public:
  PyXPCOM_InterfaceVariantHelper(Py_nsISupports *parent, int methodindex);
	~PyXPCOM_InterfaceVariantHelper();
	PRBool Init(PyObject *obParams);
	PRBool FillArray();

	PyObject *MakePythonResult();

	nsXPTCVariant *m_var_array;
	int m_num_array;
        int m_methodindex;
protected:
	PyObject *MakeSinglePythonResult(int index);
	PRBool FillInVariant(const PythonTypeDescriptor &, int, int);
	PRBool PrepareOutVariant(const PythonTypeDescriptor &td, int value_index);
	PRBool SetSizeIs( int var_index, PRBool is_arg1, PRUint32 new_size);
	PRUint32 GetSizeIs( int var_index, PRBool is_arg1);

	PyObject *m_pyparams; // sequence of actual params passed (ie, not including hidden)
	PyObject *m_typedescs; // desc of _all_ params, including hidden.
	PythonTypeDescriptor *m_python_type_desc_array;
	void **m_buffer_array;
	Py_nsISupports *m_parent;

};

/*************************************************************************
**************************************************************************

 Support for IMPLEMENTING interfaces.

**************************************************************************
*************************************************************************/
#define NS_IINTERNALPYTHON_IID_STR "AC7459FC-E8AB-4f2e-9C4F-ADDC53393A20"
#define NS_IINTERNALPYTHON_IID \
	{ 0xac7459fc, 0xe8ab, 0x4f2e, { 0x9c, 0x4f, 0xad, 0xdc, 0x53, 0x39, 0x3a, 0x20 } }

class PyXPCOM_GatewayWeakReference;

// This interface is needed primarily to give us a known vtable base.
// If we QI a Python object for this interface, we can safely cast the result
// to a PyG_Base.  Any other interface, we do now know which vtable we will get.
// We also allow the underlying PyObject to be extracted
class nsIInternalPython : public nsISupports {
public:
	NS_DEFINE_STATIC_IID_ACCESSOR(NS_IINTERNALPYTHON_IID)
	// Get the underlying Python object with new reference added
	virtual PyObject *UnwrapPythonObject(void) = 0;
};

// This is roughly equivalent to PyGatewayBase in win32com
//
class PYXPCOM_EXPORT PyG_Base : public nsIInternalPython, public nsISupportsWeakReference
{
public:
	NS_DECL_ISUPPORTS
	NS_DECL_NSISUPPORTSWEAKREFERENCE
	PyObject *UnwrapPythonObject(void);

	// A static "constructor" - the real ctor is protected.
	static nsresult CreateNew(PyObject *pPyInstance,
		                  const nsIID &iid,
				  void **ppResult);

	// A utility to auto-wrap an arbitary Python instance
	// in a COM gateway.
	static PRBool AutoWrapPythonInstance(PyObject *ob,
		                           const nsIID &iid,
					   nsISupports **ppret);


	// A helper that creates objects to be passed for nsISupports
	// objects.  See extensive comments in PyG_Base.cpp.
	PyObject *MakeInterfaceParam(nsISupports *pis,
	                                     const nsIID *piid,
					     int methodIndex = -1,
					     const XPTParamDescriptor *d = NULL,
					     int paramIndex = -1);

	// A helper that ensures all casting and vtable offsetting etc
	// done against this object happens in the one spot!
	virtual void *ThisAsIID( const nsIID &iid ) = 0;

	// Helpers for "native" interfaces.
	// Not used by the generic stub interface.
	nsresult HandleNativeGatewayError(const char *szMethodName);

	// These data members used by the converter helper functions - hence public
	nsIID m_iid;
	PyObject * m_pPyObject;
	// We keep a reference count on this object, and the object
	// itself uses normal refcount rules - thus, it will only
	// die when we die, and all external references are removed.
	// This means that once we have created it (and while we
	// are alive) it will never die.
	nsCOMPtr<nsIWeakReference> m_pWeakRef;
#ifdef NS_BUILD_REFCNT_LOGGING
	char refcntLogRepr[64]; // sigh - I wish I knew how to use the Moz string classes :(  OK for debug only tho.
#endif
protected:
	PyG_Base(PyObject *instance, const nsIID &iid);
	virtual ~PyG_Base();
	PyG_Base *m_pBaseObject; // A chain to implement identity rules.
	nsresult InvokeNativeViaPolicy(	const char *szMethodName,
			PyObject **ppResult = NULL,
			const char *szFormat = NULL,
			...
			);
	nsresult InvokeNativeViaPolicyInternal(	const char *szMethodName,
			PyObject **ppResult,
			const char *szFormat,
			va_list va);
	nsresult InvokeNativeGetViaPolicy(const char *szPropertyName,
			PyObject **ppResult = NULL
			);
	nsresult InvokeNativeSetViaPolicy(const char *szPropertyName,
			...);
};

class PYXPCOM_EXPORT PyXPCOM_XPTStub : public PyG_Base, public nsXPTCStubBase
{
friend class PyG_Base;
public:
	NS_IMETHOD QueryInterface(REFNSIID aIID, void** aInstancePtr)      \
		{return PyG_Base::QueryInterface(aIID, aInstancePtr);}     \
	NS_IMETHOD_(nsrefcnt) AddRef(void) {return PyG_Base::AddRef();}    \
	NS_IMETHOD_(nsrefcnt) Release(void) {return PyG_Base::Release();}  \

	NS_IMETHOD GetInterfaceInfo(nsIInterfaceInfo** info);
	// call this method and return result
	NS_IMETHOD CallMethod(PRUint16 methodIndex,
                          const nsXPTMethodInfo* info,
                          nsXPTCMiniVariant* params);

	virtual void *ThisAsIID(const nsIID &iid);
protected:
	PyXPCOM_XPTStub(PyObject *instance, const nsIID &iid) : PyG_Base(instance, iid) {;}
private:
};

// For the Gateways we manually implement.
#define PYGATEWAY_BASE_SUPPORT(INTERFACE, GATEWAY_BASE)                    \
	NS_IMETHOD QueryInterface(REFNSIID aIID, void** aInstancePtr)      \
		{return PyG_Base::QueryInterface(aIID, aInstancePtr);}     \
	NS_IMETHOD_(nsrefcnt) AddRef(void) {return PyG_Base::AddRef();}    \
	NS_IMETHOD_(nsrefcnt) Release(void) {return PyG_Base::Release();}  \
	virtual void *ThisAsIID(const nsIID &iid) {                        \
		if (iid.Equals(NS_GET_IID(INTERFACE))) return (INTERFACE *)this; \
		return GATEWAY_BASE::ThisAsIID(iid);                       \
	}                                                                  \

extern PYXPCOM_EXPORT void AddDefaultGateway(PyObject *instance, nsISupports *gateway);

extern PYXPCOM_EXPORT uint32_t _PyXPCOM_GetGatewayCount(void);
extern PYXPCOM_EXPORT uint32_t _PyXPCOM_GetInterfaceCount(void);
#ifdef VBOX_DEBUG_LIFETIMES
extern PYXPCOM_EXPORT PRInt32 _PyXPCOM_DumpInterfaces(void);
#endif


// Weak Reference class.  This is a true COM object, representing
// a weak reference to a Python object.  For each Python XPCOM object,
// there is exactly zero or one corresponding weak reference instance.
// When both are alive, each holds a pointer to the other.  When the main
// object dies due to XPCOM reference counting, it zaps the pointer
// in its corresponding weak reference object.  Thus, the weak-reference
// can live beyond the object (possibly with a NULL pointer back to the
// "real" object, but as implemented, the weak reference will never be
// destroyed  before the object
class PYXPCOM_EXPORT PyXPCOM_GatewayWeakReference : public nsIWeakReference {
public:
	PyXPCOM_GatewayWeakReference(PyG_Base *base);
	virtual ~PyXPCOM_GatewayWeakReference();
	NS_DECL_ISUPPORTS
	NS_DECL_NSIWEAKREFERENCE
	PyG_Base *m_pBase; // NO REF COUNT!!!
#ifdef NS_BUILD_REFCNT_LOGGING
	char refcntLogRepr[41];
#endif
};


// Helpers classes for our gateways.
class PYXPCOM_EXPORT PyXPCOM_GatewayVariantHelper
{
public:
	PyXPCOM_GatewayVariantHelper( PyG_Base *gateway,
	                              int methodIndex,
	                              const nsXPTMethodInfo *info,
	                              nsXPTCMiniVariant* params );
	~PyXPCOM_GatewayVariantHelper();
	PyObject *MakePyArgs();
	nsresult ProcessPythonResult(PyObject *ob);
	PyG_Base *m_gateway;
private:
	nsresult BackFillVariant( PyObject *ob, int index);
	PyObject *MakeSingleParam(int index, PythonTypeDescriptor &td);
	PRBool GetIIDForINTERFACE_ID(int index, const nsIID **ppret);
	nsresult GetArrayType(PRUint8 index, PRUint8 *ret, nsIID **ppiid);
	PRUint32 GetSizeIs( int var_index, PRBool is_arg1);
	PRBool SetSizeIs( int var_index, PRBool is_arg1, PRUint32 new_size);
	PRBool CanSetSizeIs( int var_index, PRBool is_arg1 );
	nsIInterfaceInfo *GetInterfaceInfo(); // NOTE: no ref count on result.


	nsXPTCMiniVariant* m_params;
	const nsXPTMethodInfo *m_info;
	int m_method_index;
	PythonTypeDescriptor *m_python_type_desc_array;
	int m_num_type_descs;
	nsCOMPtr<nsIInterfaceInfo> m_interface_info;
};

// Misc converters.
PyObject *PyObject_FromXPTType( const nsXPTType *d);
// XPTTypeDescriptor derived from XPTType - latter is automatically processed via PyObject_FromXPTTypeDescriptor XPTTypeDescriptor
PyObject *PyObject_FromXPTTypeDescriptor( const XPTTypeDescriptor *d);

PyObject *PyObject_FromXPTParamDescriptor( const XPTParamDescriptor *d);
PyObject *PyObject_FromXPTMethodDescriptor( const XPTMethodDescriptor *d);
PyObject *PyObject_FromXPTConstant( const XPTConstDescriptor *d);

// DLL reference counting functions.
// Although we maintain the count, we never actually
// finalize Python when it hits zero!
void PyXPCOM_DLLAddRef();
void PyXPCOM_DLLRelease();

/*************************************************************************
**************************************************************************

 LOCKING AND THREADING

**************************************************************************
*************************************************************************/

//
// We have 2 discrete locks in use (when no free-threaded is used, anyway).
// The first type of lock is the global Python lock.  This is the standard lock
// in use by Python, and must be used as documented by Python.  Specifically, no
// 2 threads may _ever_ call _any_ Python code (including INCREF/DECREF) without
// first having this thread lock.
//
// The second type of lock is a "global framework lock", and used whenever 2 threads
// of C code need access to global data.  This is different than the Python
// lock - this lock is used when no Python code can ever be called by the
// threads, but the C code still needs thread-safety.

// We also supply helper classes which make the usage of these locks a one-liner.

// The "framework" lock, implemented as a PRLock
PYXPCOM_EXPORT void PyXPCOM_AcquireGlobalLock(void);
PYXPCOM_EXPORT void PyXPCOM_ReleaseGlobalLock(void);

// Helper class for the DLL global lock.
//
// This class magically waits for PyXPCOM framework global lock, and releases it
// when finished.
// NEVER new one of these objects - only use on the stack!
class CEnterLeaveXPCOMFramework {
public:
	CEnterLeaveXPCOMFramework() {PyXPCOM_AcquireGlobalLock();}
	~CEnterLeaveXPCOMFramework() {PyXPCOM_ReleaseGlobalLock();}
};

// Python thread-lock stuff.  Free-threading patches use different semantics, but
// these are abstracted away here...
//#include <threadstate.h>

// Helper class for Enter/Leave Python
//
// This class magically waits for the Python global lock, and releases it
// when finished.

// Nested invocations will deadlock, so be careful.

// NEVER new one of these objects - only use on the stack!

PYXPCOM_EXPORT void PyXPCOM_MakePendingCalls();
PYXPCOM_EXPORT PRBool PyXPCOM_Globals_Ensure();

class CEnterLeavePython {
public:
	CEnterLeavePython() {
		state = PyGILState_Ensure();
		// See "pending calls" comment below.  We reach into the Python
		// implementation to see if we are the first call on the stack.
# ifndef Py_LIMITED_API
		if (PyThreadState_Get()->gilstate_counter==1) {
# else
		if (state == PyGILState_UNLOCKED) {
# endif
			PyXPCOM_MakePendingCalls();
		}
	}
	~CEnterLeavePython() {
		PyGILState_Release(state);
	}
	PyGILState_STATE state;
};

// Our classes.
// Hrm - So we can't have templates, eh??
// preprocessor to the rescue, I guess.
#define PyXPCOM_INTERFACE_DECLARE(ClassName, InterfaceName, Methods )     \
                                                                          \
extern struct PyMethodDef Methods[];                                      \
                                                                          \
class ClassName : public Py_nsISupports                                   \
{                                                                         \
public:                                                                   \
	static PYXPCOM_EXPORT PyXPCOM_TypeObject *type;                                  \
	static Py_nsISupports *Constructor(nsISupports *pInitObj, const nsIID &iid) { \
		return new ClassName(pInitObj, iid);                      \
	}                                                                 \
	static void InitType() {                                          \
		type = new PyXPCOM_TypeObject(                            \
				#InterfaceName,                           \
				Py_nsISupports::type,                     \
				sizeof(ClassName),                        \
				Methods,                                  \
				Constructor);                             \
		const nsIID &iid = NS_GET_IID(InterfaceName);             \
		RegisterInterface(iid, type);                             \
	}                                                                 \
protected:                                                                \
	ClassName(nsISupports *p, const nsIID &iid) :                     \
		Py_nsISupports(p, iid, type) {                            \
		/* The IID _must_ be the IID of the interface we are wrapping! */    \
		NS_ABORT_IF_FALSE(iid.Equals(NS_GET_IID(InterfaceName)), "Bad IID"); \
	}                                                                 \
};                                                                        \
                                                                          \
// End of PyXPCOM_INTERFACE_DECLARE macro

#define PyXPCOM_ATTR_INTERFACE_DECLARE(ClassName, InterfaceName, Methods )\
                                                                          \
extern struct PyMethodDef Methods[];                                      \
                                                                          \
class ClassName : public Py_nsISupports                                   \
{                                                                         \
public:                                                                   \
	static PyXPCOM_TypeObject *type;                                  \
	static Py_nsISupports *Constructor(nsISupports *pInitObj, const nsIID &iid) { \
		return new ClassName(pInitObj, iid);                      \
	}                                                                 \
	static void InitType() {                                          \
		type = new PyXPCOM_TypeObject(                            \
				#InterfaceName,                           \
				Py_nsISupports::type,                     \
				sizeof(ClassName),                        \
				Methods,                                  \
				Constructor);                             \
		const nsIID &iid = NS_GET_IID(InterfaceName);             \
		RegisterInterface(iid, type);                             \
}                                                                         \
	virtual PyObject *getattr(const char *name);                      \
	virtual int setattr(const char *name, PyObject *val);             \
protected:                                                                \
	ClassName(nsISupports *p, const nsIID &iid) :                     \
		Py_nsISupports(p, iid, type) {                            \
		/* The IID _must_ be the IID of the interface we are wrapping! */    \
		NS_ABORT_IF_FALSE(iid.Equals(NS_GET_IID(InterfaceName)), "Bad IID"); \
	}                                                                 \
};                                                                        \
                                                                          \
// End of PyXPCOM_ATTR_INTERFACE_DECLARE macro

#define PyXPCOM_INTERFACE_DEFINE(ClassName, InterfaceName, Methods )      \
PyXPCOM_TypeObject *ClassName::type = NULL;


// And the classes
PyXPCOM_INTERFACE_DECLARE(Py_nsIComponentManager, nsIComponentManager, PyMethods_IComponentManager)
PyXPCOM_INTERFACE_DECLARE(Py_nsIInterfaceInfoManager, nsIInterfaceInfoManager, PyMethods_IInterfaceInfoManager)
PyXPCOM_INTERFACE_DECLARE(Py_nsIEnumerator, nsIEnumerator, PyMethods_IEnumerator)
PyXPCOM_INTERFACE_DECLARE(Py_nsISimpleEnumerator, nsISimpleEnumerator, PyMethods_ISimpleEnumerator)
PyXPCOM_INTERFACE_DECLARE(Py_nsIInterfaceInfo, nsIInterfaceInfo, PyMethods_IInterfaceInfo)
PyXPCOM_INTERFACE_DECLARE(Py_nsIInputStream, nsIInputStream, PyMethods_IInputStream)
PyXPCOM_ATTR_INTERFACE_DECLARE(Py_nsIClassInfo, nsIClassInfo, PyMethods_IClassInfo)
PyXPCOM_ATTR_INTERFACE_DECLARE(Py_nsIVariant, nsIVariant, PyMethods_IVariant)
// deprecated, but retained for backward compatibility:
PyXPCOM_INTERFACE_DECLARE(Py_nsIComponentManagerObsolete, nsIComponentManagerObsolete, PyMethods_IComponentManagerObsolete)
#endif // __PYXPCOM_H__
