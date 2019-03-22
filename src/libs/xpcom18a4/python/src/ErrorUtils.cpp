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
 *   Mark Hammond <MarkH@ActiveState.com> (original author)
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
#include "nsReadableUtils.h"
#include <nsIConsoleService.h>
#ifdef VBOX
# include <nsIExceptionService.h>
# include <iprt/err.h>
# include <iprt/string.h>
#endif
#include "nspr.h" // PR_fprintf

static char *PyTraceback_AsString(PyObject *exc_tb);

// The internal helper that actually moves the
// formatted string to the target!

// Only used in really bad situations!
static void _PanicErrorWrite(const char *msg)
{
	nsCOMPtr<nsIConsoleService> consoleService = do_GetService(NS_CONSOLESERVICE_CONTRACTID);
	if (consoleService)
		consoleService->LogStringMessage(NS_ConvertASCIItoUCS2(msg).get());
	PR_fprintf(PR_STDERR,"%s\n", msg);
}

// Called when our "normal" error logger fails.
static void HandleLogError(const char *pszMessageText)
{
	nsCAutoString streamout;

	_PanicErrorWrite("Failed to log an error record");
	if (PyXPCOM_FormatCurrentException(streamout))
		_PanicErrorWrite(streamout.get());
	_PanicErrorWrite("Original error follows:");
	_PanicErrorWrite(pszMessageText);
}

static const char *LOGGER_WARNING = "warning";
static const char *LOGGER_ERROR = "error";
static const char *LOGGER_DEBUG = "debug";

// Our "normal" error logger - calls back to the logging module.
void DoLogMessage(const char *methodName, const char *pszMessageText)
{
	// We use the logging module now.  Originally this code called
	// the logging module directly by way of the C API's
	// PyImport_ImportModule/PyObject_CallMethod etc.  However, this
	// causes problems when there is no Python caller on the stack -
	// the logging module's findCaller method fails with a None frame.
	// We now work around this by calling PyRun_SimpleString - this
	// causes a new frame to be created for executing the compiled
	// string, and the logging module no longer fails.
	// XXX - this implementation is less than ideal - findCaller now
	// returns ("<string>", 2).  Ideally we would compile with a
	// filename something similar to "<pydom error reporter>".

	// But this also means we need a clear error state...
	PyObject *exc_typ = NULL, *exc_val = NULL, *exc_tb = NULL;
	PyErr_Fetch(&exc_typ, &exc_val, &exc_tb);
// We will execute:
//  import logging
//  logging.getLogger('xpcom').{warning/error/etc}("%s", {msg_text})
	nsCAutoString c("import logging\nlogging.getLogger('xpcom').");
	c += methodName;
	c += "('%s', ";
	// Pull a trick to ensure a valid string - use Python repr!
#if PY_MAJOR_VERSION <= 2
	PyObject *obMessage = PyString_FromString(pszMessageText);
#else
	PyObject *obMessage = PyUnicode_FromString(pszMessageText);
#endif
	if (obMessage) {
		PyObject *repr = PyObject_Repr(obMessage);
		if (repr) {
#if PY_MAJOR_VERSION <= 2
			c += PyString_AsString(repr);
#else
			c += PyUnicode_AsUTF8(repr);
#endif
			Py_DECREF(repr);
		}
		Py_DECREF(obMessage);
	}
	c += ")\n";
	if (PyRun_SimpleString(c.get()) != 0) {
		HandleLogError(pszMessageText);
	}
	PyErr_Restore(exc_typ, exc_val, exc_tb);
}

void LogMessage(const char *methodName, const char *pszMessageText)
{
	// Be careful to save and restore the Python exception state
	// before calling back to Python, or we lose the original error.
	PyObject *exc_typ = NULL, *exc_val = NULL, *exc_tb = NULL;
	PyErr_Fetch( &exc_typ, &exc_val, &exc_tb);
	DoLogMessage(methodName, pszMessageText);
	PyErr_Restore(exc_typ, exc_val, exc_tb);
}


void LogMessage(const char *methodName, nsACString &text)
{
	char *c = ToNewCString(text);
	LogMessage(methodName, c);
	nsCRT::free(c);
}

// A helper for the various logging routines.
static void VLogF(const char *methodName, const char *fmt, va_list argptr)
{
	char buff[512];
#ifdef VBOX /* Enable the use of VBox formatting types. */
	RTStrPrintfV(buff, sizeof(buff), fmt, argptr);
#else
	// Use safer NS_ functions.
	PR_vsnprintf(buff, sizeof(buff), fmt, argptr);
#endif

	LogMessage(methodName, buff);
}

PRBool PyXPCOM_FormatCurrentException(nsCString &streamout)
{
	PRBool ok = PR_FALSE;
	PyObject *exc_typ = NULL, *exc_val = NULL, *exc_tb = NULL;
	PyErr_Fetch( &exc_typ, &exc_val, &exc_tb);
	PyErr_NormalizeException( &exc_typ, &exc_val, &exc_tb);
	if (exc_typ) {
		ok = PyXPCOM_FormatGivenException(streamout, exc_typ, exc_val,
						  exc_tb);
	}
	PyErr_Restore(exc_typ, exc_val, exc_tb);
	return ok;
}

PRBool PyXPCOM_FormatGivenException(nsCString &streamout,
				    PyObject *exc_typ, PyObject *exc_val,
				    PyObject *exc_tb)
{
	if (!exc_typ)
		return PR_FALSE;
	streamout += "\n";

	if (exc_tb) {
		const char *szTraceback = PyTraceback_AsString(exc_tb);
		if (szTraceback == NULL)
			streamout += "Can't get the traceback info!";
		else {
			streamout += "Traceback (most recent call last):\n";
			streamout += szTraceback;
			PyMem_Free((void *)szTraceback);
		}
	}
	PyObject *temp = PyObject_Str(exc_typ);
	if (temp) {
#if PY_MAJOR_VERSION <= 2
		streamout += PyString_AsString(temp);
#else
		streamout += PyUnicode_AsUTF8(temp);
#endif
		Py_DECREF(temp);
	} else
		streamout += "Can't convert exception to a string!";
	streamout += ": ";
	if (exc_val != NULL) {
		temp = PyObject_Str(exc_val);
		if (temp) {
#if PY_MAJOR_VERSION <= 2
			streamout += PyString_AsString(temp);
#else
			streamout += PyUnicode_AsUTF8(temp);
#endif
			Py_DECREF(temp);
		} else
			streamout += "Can't convert exception value to a string!";
	}
	return PR_TRUE;
}

void PyXPCOM_LogError(const char *fmt, ...)
{
	va_list marker;
	va_start(marker, fmt);
	// NOTE: It is tricky to use logger.exception here - the exception
	// state when called back from the C code is clear.  Only Python 2.4
	// and later allows an explicit exc_info tuple().

	// Don't use VLogF here, instead arrange for exception info and
	// traceback to be in the same buffer.
	char buff[512];
	PR_vsnprintf(buff, sizeof(buff), fmt, marker);
	// If we have a Python exception, also log that:
	nsCAutoString streamout(buff);
	if (PyXPCOM_FormatCurrentException(streamout)) {
		LogMessage(LOGGER_ERROR, streamout);
	}
    va_end(marker);
}

void PyXPCOM_LogWarning(const char *fmt, ...)
{
	va_list marker;
	va_start(marker, fmt);
	VLogF(LOGGER_WARNING, fmt, marker);
    va_end(marker);
}

void PyXPCOM_Log(const char *level, const nsCString &msg)
{
	DoLogMessage(level, msg.get());
}

#ifdef DEBUG
void PyXPCOM_LogDebug(const char *fmt, ...)
{
	va_list marker;
	va_start(marker, fmt);
	VLogF(LOGGER_DEBUG, fmt, marker);
    va_end(marker);
}
#endif

#ifdef VBOX
PyObject *PyXPCOM_BuildErrorMessage(nsresult r)
{
    char msg[512];
    bool gotMsg = false;

    if (!gotMsg)
    {
        nsresult rc;
        nsCOMPtr <nsIExceptionService> es;
        es = do_GetService (NS_EXCEPTIONSERVICE_CONTRACTID, &rc);
        if (NS_SUCCEEDED (rc))
        {
            nsCOMPtr <nsIExceptionManager> em;
            rc = es->GetCurrentExceptionManager (getter_AddRefs (em));
            if (NS_SUCCEEDED (rc))
            {
                nsCOMPtr <nsIException> ex;
                rc = em->GetExceptionFromProvider(r, NULL, getter_AddRefs (ex));
                if  (NS_SUCCEEDED (rc) && ex)
                {
                    nsXPIDLCString emsg;
                    ex->GetMessage(getter_Copies(emsg));
                    PR_snprintf(msg, sizeof(msg), "%s",
                                emsg.get());
                    gotMsg = true;
                }
            }
        }
    }

    if (!gotMsg)
    {
        const RTCOMERRMSG* pMsg = RTErrCOMGet(r);
        if (strncmp(pMsg->pszMsgFull, "Unknown", 7) != 0)
        {
            PR_snprintf(msg, sizeof(msg), "%s (%s)",
                        pMsg->pszMsgFull, pMsg->pszDefine);
            gotMsg = true;
        }
    }

    if (!gotMsg)
    {
        PR_snprintf(msg, sizeof(msg), "Error 0x%x in module 0x%x",
                    NS_ERROR_GET_CODE(r), NS_ERROR_GET_MODULE(r));
    }
    PyObject *evalue = Py_BuildValue("is", r, msg);
    return evalue;
}
#endif

PyObject *PyXPCOM_BuildPyException(nsresult r)
{
#ifndef VBOX
	// Need the message etc.
	PyObject *evalue = Py_BuildValue("i", r);
#else
        PyObject *evalue = PyXPCOM_BuildErrorMessage(r);
#endif
	PyErr_SetObject(PyXPCOM_Error, evalue);
	Py_XDECREF(evalue);
	return NULL;
}

nsresult PyXPCOM_SetCOMErrorFromPyException()
{
	if (!PyErr_Occurred())
		// No error occurred
		return NS_OK;
	nsresult rv = NS_ERROR_FAILURE;
	if (PyErr_ExceptionMatches(PyExc_MemoryError))
		rv = NS_ERROR_OUT_OF_MEMORY;
	// todo:
	// * Set an exception using the exception service.

	// Once we have returned to the xpcom caller, we don't want to leave a
	// Python exception pending - it may get noticed when the next call
	// is made on the same thread.
	PyErr_Clear();
	return rv;
}

/* Obtains a string from a Python traceback.
   This is the exact same string as "traceback.print_exc" would return.

   Pass in a Python traceback object (probably obtained from PyErr_Fetch())
   Result is a string which must be free'd using PyMem_Free()
*/
#define TRACEBACK_FETCH_ERROR(what) {errMsg = what; goto done;}

char *PyTraceback_AsString(PyObject *exc_tb)
{
	const char *errMsg = NULL; /* holds a local error message */
	char *result = NULL; /* a valid, allocated result. */
	PyObject *modStringIO = NULL;
	PyObject *modTB = NULL;
	PyObject *obFuncStringIO = NULL;
	PyObject *obStringIO = NULL;
	PyObject *obFuncTB = NULL;
	PyObject *argsTB = NULL;
	PyObject *obResult = NULL;

#if PY_MAJOR_VERSION <= 2
	/* Import the modules we need - cStringIO and traceback */
	modStringIO = PyImport_ImportModule("cStringIO");
	if (modStringIO==NULL)
		TRACEBACK_FETCH_ERROR("cant import cStringIO\n");

	modTB = PyImport_ImportModule("traceback");
	if (modTB==NULL)
		TRACEBACK_FETCH_ERROR("cant import traceback\n");
	/* Construct a cStringIO object */
	obFuncStringIO = PyObject_GetAttrString(modStringIO, "StringIO");
	if (obFuncStringIO==NULL)
		TRACEBACK_FETCH_ERROR("cant find cStringIO.StringIO\n");
	obStringIO = PyObject_CallObject(obFuncStringIO, NULL);
	if (obStringIO==NULL)
		TRACEBACK_FETCH_ERROR("cStringIO.StringIO() failed\n");
#else
	/* Import the modules we need - io and traceback */
	modStringIO = PyImport_ImportModule("io");
	if (modStringIO==NULL)
		TRACEBACK_FETCH_ERROR("cant import io\n");

	modTB = PyImport_ImportModule("traceback");
	if (modTB==NULL)
		TRACEBACK_FETCH_ERROR("cant import traceback\n");
	/* Construct a StringIO object */
	obFuncStringIO = PyObject_GetAttrString(modStringIO, "StringIO");
	if (obFuncStringIO==NULL)
		TRACEBACK_FETCH_ERROR("cant find io.StringIO\n");
	obStringIO = PyObject_CallObject(obFuncStringIO, NULL);
	if (obStringIO==NULL)
		TRACEBACK_FETCH_ERROR("io.StringIO() failed\n");
#endif
	/* Get the traceback.print_exception function, and call it. */
	obFuncTB = PyObject_GetAttrString(modTB, "print_tb");
	if (obFuncTB==NULL)
		TRACEBACK_FETCH_ERROR("cant find traceback.print_tb\n");

	argsTB = Py_BuildValue("OOO",
			exc_tb  ? exc_tb  : Py_None,
			Py_None,
			obStringIO);
	if (argsTB==NULL)
		TRACEBACK_FETCH_ERROR("cant make print_tb arguments\n");

	obResult = PyObject_CallObject(obFuncTB, argsTB);
	if (obResult==NULL)
		TRACEBACK_FETCH_ERROR("traceback.print_tb() failed\n");
	/* Now call the getvalue() method in the StringIO instance */
	Py_DECREF(obFuncStringIO);
	obFuncStringIO = PyObject_GetAttrString(obStringIO, "getvalue");
	if (obFuncStringIO==NULL)
		TRACEBACK_FETCH_ERROR("cant find getvalue function\n");
	Py_DECREF(obResult);
	obResult = PyObject_CallObject(obFuncStringIO, NULL);
	if (obResult==NULL)
		TRACEBACK_FETCH_ERROR("getvalue() failed.\n");

	/* And it should be a string all ready to go - duplicate it. */
#if PY_MAJOR_VERSION <= 2
	if (!PyString_Check(obResult))
#else
	if (!PyUnicode_Check(obResult))
#endif
			TRACEBACK_FETCH_ERROR("getvalue() did not return a string\n");

	{ // a temp scope so I can use temp locals.
#if PY_MAJOR_VERSION <= 2
	char *tempResult = PyString_AsString(obResult);
#else
    /* PyUnicode_AsUTF8() is const char * as of Python 3.7, char * earlier. */
	const char *tempResult = (const char *)PyUnicode_AsUTF8(obResult);
#endif
	result = (char *)PyMem_Malloc(strlen(tempResult)+1);
	if (result==NULL)
		TRACEBACK_FETCH_ERROR("memory error duplicating the traceback string\n");

	strcpy(result, tempResult);
	} // end of temp scope.
done:
	/* All finished - first see if we encountered an error */
	if (result==NULL && errMsg != NULL) {
		result = (char *)PyMem_Malloc(strlen(errMsg)+1);
		if (result != NULL)
			/* if it does, not much we can do! */
			strcpy(result, errMsg);
	}
	Py_XDECREF(modStringIO);
	Py_XDECREF(modTB);
	Py_XDECREF(obFuncStringIO);
	Py_XDECREF(obStringIO);
	Py_XDECREF(obFuncTB);
	Py_XDECREF(argsTB);
	Py_XDECREF(obResult);
	return result;
}

// See comments in PyXPCOM.h for why we need this!
void PyXPCOM_MakePendingCalls()
{
	while (1) {
		int rc = Py_MakePendingCalls();
		if (rc == 0)
			break;
		// An exception - just report it as normal.
		// Note that a traceback is very unlikely!
		PyXPCOM_LogError("Unhandled exception detected before entering Python.\n");
		PyErr_Clear();
		// And loop around again until we are told everything is done!
	}
}
