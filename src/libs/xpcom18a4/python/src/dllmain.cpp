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

//
// This code is part of the XPCOM extensions for Python.
//
// Written May 2000 by Mark Hammond.
//
// Based heavily on the Python COM support, which is
// (c) Mark Hammond and Greg Stein.
//
// (c) 2000, ActiveState corp.

#include "PyXPCOM_std.h"
#include "nsILocalFile.h"

#ifdef XP_WIN
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include "windows.h"
#endif

static PRInt32 g_cLockCount = 0;
static PRLock *g_lockMain = nsnull;

PYXPCOM_EXPORT PyObject *PyXPCOM_Error = NULL;

PyXPCOM_INTERFACE_DEFINE(Py_nsIComponentManager, nsIComponentManager, PyMethods_IComponentManager)
PyXPCOM_INTERFACE_DEFINE(Py_nsIInterfaceInfoManager, nsIInterfaceInfoManager, PyMethods_IInterfaceInfoManager)
PyXPCOM_INTERFACE_DEFINE(Py_nsIEnumerator, nsIEnumerator, PyMethods_IEnumerator)
PyXPCOM_INTERFACE_DEFINE(Py_nsISimpleEnumerator, nsISimpleEnumerator, PyMethods_ISimpleEnumerator)
PyXPCOM_INTERFACE_DEFINE(Py_nsIInterfaceInfo, nsIInterfaceInfo, PyMethods_IInterfaceInfo)
PyXPCOM_INTERFACE_DEFINE(Py_nsIInputStream, nsIInputStream, PyMethods_IInputStream)
PyXPCOM_INTERFACE_DEFINE(Py_nsIClassInfo, nsIClassInfo, PyMethods_IClassInfo)
PyXPCOM_INTERFACE_DEFINE(Py_nsIVariant, nsIVariant, PyMethods_IVariant)
// deprecated, but retained for backward compatibility:
PyXPCOM_INTERFACE_DEFINE(Py_nsIComponentManagerObsolete, nsIComponentManagerObsolete, PyMethods_IComponentManagerObsolete)

#ifndef PYXPCOM_USE_PYGILSTATE

////////////////////////////////////////////////////////////
// Thread-state helpers/global functions.
// Only used if there is no Python PyGILState_* API
//
static PyThreadState *ptsGlobal = nsnull;
PyInterpreterState *PyXPCOM_InterpreterState = nsnull;
PRUintn tlsIndex = 0;


// This function must be called at some time when the interpreter lock and state is valid.
// Called by init{module} functions and also COM factory entry point.
void PyXPCOM_InterpreterState_Ensure()
{
	if (PyXPCOM_InterpreterState==NULL) {
		PyThreadState *threadStateSave = PyThreadState_Swap(NULL);
		if (threadStateSave==NULL)
			Py_FatalError("Can not setup interpreter state, as current state is invalid");

		PyXPCOM_InterpreterState = threadStateSave->interp;
		PyThreadState_Swap(threadStateSave);
	}
}

void PyXPCOM_InterpreterState_Free()
{
	PyXPCOM_ThreadState_Free();
	PyXPCOM_InterpreterState = NULL; // Eek - should I be freeing something?
}

// This structure is stored in the TLS slot.  At this stage only a Python thread state
// is kept, but this may change in the future...
struct ThreadData{
	PyThreadState *ts;
};

// Ensure that we have a Python thread state available to use.
// If this is called for the first time on a thread, it will allocate
// the thread state.  This does NOT change the state of the Python lock.
// Returns TRUE if a new thread state was created, or FALSE if a
// thread state already existed.
PRBool PyXPCOM_ThreadState_Ensure()
{
	ThreadData *pData = (ThreadData *)PR_GetThreadPrivate(tlsIndex);
	if (pData==NULL) { /* First request on this thread */
		/* Check we have an interpreter state */
		if (PyXPCOM_InterpreterState==NULL) {
				Py_FatalError("Can not setup thread state, as have no interpreter state");
		}
		pData = (ThreadData *)nsMemory::Alloc(sizeof(ThreadData));
		if (!pData)
			Py_FatalError("Out of memory allocating thread state.");
		memset(pData, 0, sizeof(*pData));
		if (NS_FAILED( PR_SetThreadPrivate( tlsIndex, pData ) ) ) {
			NS_ABORT_IF_FALSE(0, "Could not create thread data for this thread!");
			Py_FatalError("Could not thread private thread data!");
		}
		pData->ts = PyThreadState_New(PyXPCOM_InterpreterState);
		return PR_TRUE; // Did create a thread state state
	}
	return PR_FALSE; // Thread state was previously created
}

// Asuming we have a valid thread state, acquire the Python lock.
void PyXPCOM_InterpreterLock_Acquire()
{
	ThreadData *pData = (ThreadData *)PR_GetThreadPrivate(tlsIndex);
	NS_ABORT_IF_FALSE(pData, "Have no thread data for this thread!");
	PyThreadState *thisThreadState = pData->ts;
	PyEval_AcquireThread(thisThreadState);
}

// Asuming we have a valid thread state, release the Python lock.
void PyXPCOM_InterpreterLock_Release()
{
	ThreadData *pData = (ThreadData *)PR_GetThreadPrivate(tlsIndex);
	NS_ABORT_IF_FALSE(pData, "Have no thread data for this thread!");
	PyThreadState *thisThreadState = pData->ts;
	PyEval_ReleaseThread(thisThreadState);
}

// Free the thread state for the current thread
// (Presumably previously create with a call to
// PyXPCOM_ThreadState_Ensure)
void PyXPCOM_ThreadState_Free()
{
	ThreadData *pData = (ThreadData *)PR_GetThreadPrivate(tlsIndex);
	if (!pData) return;
	PyThreadState *thisThreadState = pData->ts;
	PyThreadState_Delete(thisThreadState);
	PR_SetThreadPrivate(tlsIndex, NULL);
	nsMemory::Free(pData);
}

void PyXPCOM_ThreadState_Clear()
{
	ThreadData *pData = (ThreadData *)PR_GetThreadPrivate(tlsIndex);
	PyThreadState *thisThreadState = pData->ts;
	PyThreadState_Clear(thisThreadState);
}
#endif // PYXPCOM_USE_PYGILSTATE

////////////////////////////////////////////////////////////
// Lock/exclusion global functions.
//
void PyXPCOM_AcquireGlobalLock(void)
{
	NS_PRECONDITION(g_lockMain != nsnull, "Cant acquire a NULL lock!");
	PR_Lock(g_lockMain);
}
void PyXPCOM_ReleaseGlobalLock(void)
{
	NS_PRECONDITION(g_lockMain != nsnull, "Cant release a NULL lock!");
	PR_Unlock(g_lockMain);
}

void PyXPCOM_DLLAddRef(void)
{
	// Must be thread-safe, although cant have the Python lock!
	CEnterLeaveXPCOMFramework _celf;
	PRInt32 cnt = PR_AtomicIncrement(&g_cLockCount);
	if (cnt==1) { // First call
		if (!Py_IsInitialized()) {
			Py_Initialize();
			// Make sure our Windows framework is all setup.
			PyXPCOM_Globals_Ensure();
			// Make sure we have _something_ as sys.argv.
			if (PySys_GetObject((char*)"argv")==NULL) {
				PyObject *path = PyList_New(0);
#if PY_MAJOR_VERSION <= 2
				PyObject *str = PyString_FromString("");
#else
				PyObject *str = PyUnicode_FromString("");
#endif
				PyList_Append(path, str);
				PySys_SetObject((char*)"argv", path);
				Py_XDECREF(path);
				Py_XDECREF(str);
			}

			/* Done automatically since python 3.7 and deprecated since python 3.9. */
#if PY_VERSION_HEX < 0x03090000
			// Must force Python to start using thread locks, as
			// we are free-threaded (maybe, I think, sometimes :-)
			PyEval_InitThreads();
#endif
#ifndef PYXPCOM_USE_PYGILSTATE
			// Release Python lock, as first thing we do is re-get it.
			ptsGlobal = PyEval_SaveThread();
#endif
			// NOTE: We never finalize Python!!
		}
	}
}
void PyXPCOM_DLLRelease(void)
{
	PR_AtomicDecrement(&g_cLockCount);
}

void pyxpcom_construct(void)
{
	g_lockMain = PR_NewLock();
#ifndef PYXPCOM_USE_PYGILSTATE
	PRStatus status;
	status = PR_NewThreadPrivateIndex( &tlsIndex, NULL );
	NS_WARN_IF_FALSE(status==0, "Could not allocate TLS storage");
	if (NS_FAILED(status)) {
		PR_DestroyLock(g_lockMain);
		return; // PR_FALSE;
	}
#endif // PYXPCOM_USE_PYGILSTATE
	return; // PR_TRUE;
}

void pyxpcom_destruct(void)
{
	PR_DestroyLock(g_lockMain);
#ifndef PYXPCOM_USE_PYGILSTATE
	// I can't locate a way to kill this -
	// should I pass a dtor to PR_NewThreadPrivateIndex??
	// TlsFree(tlsIndex);
#endif // PYXPCOM_USE_PYGILSTATE
}

// Yet another attempt at cross-platform library initialization and finalization.
struct DllInitializer {
	DllInitializer() {
		pyxpcom_construct();
	}
	~DllInitializer() {
		pyxpcom_destruct();
	}
} dll_initializer;

////////////////////////////////////////////////////////////
// Other helpers/global functions.
//
PRBool PyXPCOM_Globals_Ensure()
{
	PRBool rc = PR_TRUE;

#ifndef PYXPCOM_USE_PYGILSTATE
	PyXPCOM_InterpreterState_Ensure();
#endif

	// The exception object - we load it from .py code!
	if (PyXPCOM_Error == NULL) {
		rc = PR_FALSE;
		PyObject *mod = NULL;

		mod = PyImport_ImportModule("xpcom");
		if (mod!=NULL) {
			PyXPCOM_Error = PyObject_GetAttrString(mod, "Exception");
			Py_DECREF(mod);
		}
		rc = (PyXPCOM_Error != NULL);
	}
	if (!rc)
		return rc;

	static PRBool bHaveInitXPCOM = PR_FALSE;
	if (!bHaveInitXPCOM) {

		if (!NS_IsXPCOMInitialized()) {
			// not already initialized.
			// Elsewhere, Mozilla can find it itself (we hope!)
			nsresult rv = NS_InitXPCOM2(nsnull, nsnull, nsnull);
			if (NS_FAILED(rv)) {
				PyErr_SetString(PyExc_RuntimeError, "The XPCOM subsystem could not be initialized");
				return PR_FALSE;
			}
		}
		// Even if xpcom was already init, we want to flag it as init!
		bHaveInitXPCOM = PR_TRUE;
		// Register our custom interfaces.

		Py_nsISupports::InitType();
		Py_nsIComponentManager::InitType();
		Py_nsIInterfaceInfoManager::InitType();
		Py_nsIEnumerator::InitType();
		Py_nsISimpleEnumerator::InitType();
		Py_nsIInterfaceInfo::InitType();
		Py_nsIInputStream::InitType();
		Py_nsIClassInfo::InitType();
		Py_nsIVariant::InitType();
		// for backward compatibility:
		Py_nsIComponentManagerObsolete::InitType();

	}
	return rc;
}

