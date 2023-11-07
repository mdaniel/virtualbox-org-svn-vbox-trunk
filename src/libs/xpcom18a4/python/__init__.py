# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is the Python XPCOM language bindings.
#
# The Initial Developer of the Original Code is
# Activestate Tool Corp.
# Portions created by the Initial Developer are Copyright (C) 2000
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   Mark Hammond <MarkH@activestate.com>
#
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
#
# ***** END LICENSE BLOCK *****
#

# The XPCOM (Cross Platform COM) package.
from __future__ import print_function
import sys
if sys.version_info[0] <= 2:
    import exceptions
    XPCOMBaseException = exceptions.Exception
else:
    XPCOMBaseException = Exception

# A global "verbose" flag - currently used by the
# server package to print trace messages
verbose = 0
# Map of nsresult -> constant_name.
hr_map = {}

# The standard XPCOM exception object.
# Instances of this class are raised by the XPCOM extension module.
class Exception(XPCOMBaseException):
    def __init__(self, errno, message = None):
        assert int(errno) == errno, "The errno param must be an integer"
        self.errno = errno
        self.msg = message
        XPCOMBaseException.__init__(self, errno)
    def __str__(self):
        if not hr_map:
            from . import nsError
            for name, val in list(nsError.__dict__.items()):
                if type(val)==type(0):
                    hr_map[val] = name
        message = self.msg
        if message is None:
            message = hr_map.get(self.errno)
            if message is None:
                message = ""
        return "0x%x (%s)" % (self.errno & 0xFFFFFFFF, message)

# An alias for Exception - allows code to say "from xpcom import COMException"
# rather than "Exception", preventing clashes with the builtin Exception
COMException = Exception

# Exceptions thrown by servers.  It can be good for diagnostics to
# differentiate between a ServerException (which was presumably explicitly thrown)
# and a normal exception which may simply be propagating down.
# (When ServerException objects are thrown across the XPConnect
# gateway they will be converted back to normal client exceptions if
# subsequently re-caught by Python)
class ServerException(Exception):
    def __init__(self, errno=None, *args, **kw):
        if errno is None:
            from . import nsError
            errno = nsError.NS_ERROR_FAILURE
        Exception.__init__(self, errno, *args, **kw)

# Logging support - setup the 'xpcom' logger to write to sys.stderr,
# or optionally a file.
# Environment variables supports:
# PYXPCOM_LOG_FILE=filename - if set, used instead of sys.stderr.
# PYXPCOM_LOG_LEVEL=level - level may be a number or a logging level
#                           constant (eg, 'debug', 'error')
# Later it may make sense to allow a different log level to be set for
# the file.
import logging
def setupLogging():
    import os
    if sys.version_info[0] <= 2:
        import threading, thread

    # Add a handler to print to stderr, or optionally a file
    # PYXPCOM_LOG_FILE can specify a filename
    filename = os.environ.get("PYXPCOM_LOG_FILE")
    stream = sys.stderr # this is what logging uses as default
    if filename:
        try:
            # open without buffering so never pending output
            stream = open(filename, "wU", 0)
        except IOError as why:
            print("pyxpcom failed to open log file '%s': %s"  % (filename, why), file=sys.stderr)
            # stream remains default

    hdlr = logging.StreamHandler(stream)
    # see above - fix a deadlock problem on this handler too.
    if sys.version_info[0] <= 2:
        if type(hdlr.lock) == thread.LockType:
            hdlr.lock = threading.RLock()

    fmt = logging.Formatter(logging.BASIC_FORMAT)
    hdlr.setFormatter(fmt)
    logger.addHandler(hdlr)
    # Allow PYXPCOM_LOG_LEVEL to set the level
    level = os.environ.get("PYXPCOM_LOG_LEVEL")
    if level:
        try:
            level = int(level)
        except ValueError:
            try:
                # might be a symbolic name - all are upper-case
                level = int(getattr(logging, level.upper()))
            except (AttributeError, ValueError):
                logger.warning("The PYXPCOM_LOG_LEVEL variable specifies an "
                               "invalid level")
                level = None
        if level:
            logger.setLevel(level)

logger = logging.getLogger('xpcom')
# If someone else has already setup this logger, leave things alone.
if len(logger.handlers) == 0:
    setupLogging()

# Cleanup namespace - but leave 'logger' there for people to use, so they
# don't need to know the exact name of the logger.
del logging, setupLogging
