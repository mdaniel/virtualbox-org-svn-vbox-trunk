## @file
# This file implements the log mechanism for Python tools.
#
# Copyright (c) 2007 - 2014, Intel Corporation. All rights reserved.<BR>
# This program and the accompanying materials
# are licensed and made available under the terms and conditions of the BSD License
# which accompanies this distribution.  The full text of the license may be found at
# http://opensource.org/licenses/bsd-license.php
#
# THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
# WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
#

## Import modules
import Common.LongFilePathOs as os, sys, logging
import traceback
from  BuildToolError import *

## Log level constants
DEBUG_0 = 1
DEBUG_1 = 2
DEBUG_2 = 3
DEBUG_3 = 4
DEBUG_4 = 5
DEBUG_5 = 6
DEBUG_6 = 7
DEBUG_7 = 8
DEBUG_8 = 9
DEBUG_9 = 10
VERBOSE = 15
INFO    = 20
WARN    = 30
QUIET   = 40
ERROR   = 50

IsRaiseError = True

# Tool name
_ToolName = os.path.basename(sys.argv[0])

# For validation purpose
_LogLevels = [DEBUG_0, DEBUG_1, DEBUG_2, DEBUG_3, DEBUG_4, DEBUG_5, DEBUG_6, DEBUG_7, DEBUG_8, DEBUG_9, VERBOSE, WARN, INFO, ERROR, QUIET]

# For DEBUG level (All DEBUG_0~9 are applicable)
_DebugLogger = logging.getLogger("tool_debug")
_DebugFormatter = logging.Formatter("[%(asctime)s.%(msecs)d]: %(message)s", datefmt="%H:%M:%S")

# For VERBOSE, INFO, WARN level
_InfoLogger = logging.getLogger("tool_info")
_InfoFormatter = logging.Formatter("%(message)s")

# For ERROR level
_ErrorLogger = logging.getLogger("tool_error")
_ErrorFormatter = logging.Formatter("%(message)s")

# String templates for ERROR/WARN/DEBUG log message
_ErrorMessageTemplate = '\n\n%(tool)s...\n%(file)s(%(line)s): error %(errorcode)04X: %(msg)s\n\t%(extra)s'
_ErrorMessageTemplateWithoutFile = '\n\n%(tool)s...\n : error %(errorcode)04X: %(msg)s\n\t%(extra)s'
_WarningMessageTemplate = '%(tool)s...\n%(file)s(%(line)s): warning: %(msg)s'
_WarningMessageTemplateWithoutFile = '%(tool)s: : warning: %(msg)s'
_DebugMessageTemplate = '%(file)s(%(line)s): debug: \n    %(msg)s'

#
# Flag used to take WARN as ERROR.
# By default, only ERROR message will break the tools execution.
#
_WarningAsError = False

## Log debug message
#
#   @param  Level       DEBUG level (DEBUG0~9)
#   @param  Message     Debug information
#   @param  ExtraData   More information associated with "Message"
#
def debug(Level, Message, ExtraData=None):
    if _DebugLogger.level > Level:
        return
    if Level > DEBUG_9:
        return

    # Find out the caller method information
    CallerStack = traceback.extract_stack()[-2]
    TemplateDict = {
        "file"      : CallerStack[0],
        "line"      : CallerStack[1],
        "msg"       : Message,
    }

    if ExtraData != None:
        LogText = _DebugMessageTemplate % TemplateDict + "\n    %s" % ExtraData
    else:
        LogText = _DebugMessageTemplate % TemplateDict

    _DebugLogger.log(Level, LogText)

## Log verbose message
#
#   @param  Message     Verbose information
#
def verbose(Message):
    return _InfoLogger.log(VERBOSE, Message)

## Log warning message
#
#   Warning messages are those which might be wrong but won't fail the tool.
#
#   @param  ToolName    The name of the tool. If not given, the name of caller
#                       method will be used.
#   @param  Message     Warning information
#   @param  File        The name of file which caused the warning.
#   @param  Line        The line number in the "File" which caused the warning.
#   @param  ExtraData   More information associated with "Message"
#
def warn(ToolName, Message, File=None, Line=None, ExtraData=None):
    if _InfoLogger.level > WARN:
        return

    # if no tool name given, use caller's source file name as tool name
    if ToolName == None or ToolName == "":
        ToolName = os.path.basename(traceback.extract_stack()[-2][0])

    if Line == None:
        Line = "..."
    else:
        Line = "%d" % Line

    TemplateDict = {
        "tool"      : ToolName,
        "file"      : File,
        "line"      : Line,
        "msg"       : Message,
    }

    if File != None:
        LogText = _WarningMessageTemplate % TemplateDict
    else:
        LogText = _WarningMessageTemplateWithoutFile % TemplateDict

    if ExtraData != None:
        LogText += "\n    %s" % ExtraData

    _InfoLogger.log(WARN, LogText)

    # Raise an execption if indicated
    if _WarningAsError == True:
        raise FatalError(WARNING_AS_ERROR)

## Log INFO message
info    = _InfoLogger.info

## Log ERROR message
#
#   Once an error messages is logged, the tool's execution will be broken by raising
# an execption. If you don't want to break the execution later, you can give
# "RaiseError" with "False" value.
#
#   @param  ToolName    The name of the tool. If not given, the name of caller
#                       method will be used.
#   @param  ErrorCode   The error code
#   @param  Message     Warning information
#   @param  File        The name of file which caused the error.
#   @param  Line        The line number in the "File" which caused the warning.
#   @param  ExtraData   More information associated with "Message"
#   @param  RaiseError  Raise an exception to break the tool's executuion if
#                       it's True. This is the default behavior.
#
def error(ToolName, ErrorCode, Message=None, File=None, Line=None, ExtraData=None, RaiseError=IsRaiseError):
    if Line == None:
        Line = "..."
    else:
        Line = "%d" % Line

    if Message == None:
        if ErrorCode in gErrorMessage:
            Message = gErrorMessage[ErrorCode]
        else:
            Message = gErrorMessage[UNKNOWN_ERROR]

    if ExtraData == None:
        ExtraData = ""

    TemplateDict = {
        "tool"      : _ToolName,
        "file"      : File,
        "line"      : Line,
        "errorcode" : ErrorCode,
        "msg"       : Message,
        "extra"     : ExtraData
    }

    if File != None:
        LogText =  _ErrorMessageTemplate % TemplateDict
    else:
        LogText = _ErrorMessageTemplateWithoutFile % TemplateDict

    # VBox - begin
    LogText += '\n' + getSimpleStack(3);
    # VBox - endi

    _ErrorLogger.log(ERROR, LogText)
    if RaiseError:
        raise FatalError(ErrorCode)

# Log information which should be always put out
quiet   = _ErrorLogger.error

# VBox - begin

## Get caller info
#   @return     String with  caller name, file and line number.
#   @param      iFrame The frame number of the caller to get.
def getCallerName(oFrame = None, iFrame = 2):
    if oFrame is None:
        try:
            raise Exception();
        except:
            oFrame = sys.exc_info()[2].tb_frame.f_back;
        while iFrame > 1:
            if oFrame is not None:
                oFrame = oFrame.f_back;
            iFrame = iFrame - 1;
    if oFrame is not None:
        return '%s %s:%u' % (oFrame.f_code.co_name, oFrame.f_code.co_filename, oFrame.f_lineno);
    return None;

## @Get a simple stack trace.
#   @return simple stack trace (string).
#   @param      iFrame      The frame to start with.
#   @param      cMaxFrames  The maximum number of frames to dump.
def getSimpleStack(iFrame = 2, cMaxFrames = 7):
    sStack = 'Stack:\n'
    for i in range(cMaxFrames):
        sCaller = getCallerName(iFrame = iFrame + i);
        if sCaller is None:
            break;
        sStack += '[%u] %s\n' % (i + 1, sCaller);
    return sStack;

# VBox - end

## Initialize log system
def Initialize():
    #
    # Since we use different format to log different levels of message into different
    # place (stdout or stderr), we have to use different "Logger" objects to do this.
    #
    # For DEBUG level (All DEBUG_0~9 are applicable)
    _DebugLogger.setLevel(INFO)
    _DebugChannel = logging.StreamHandler(sys.stdout)
    _DebugChannel.setFormatter(_DebugFormatter)
    _DebugLogger.addHandler(_DebugChannel)

    # For VERBOSE, INFO, WARN level
    _InfoLogger.setLevel(INFO)
    _InfoChannel = logging.StreamHandler(sys.stdout)
    _InfoChannel.setFormatter(_InfoFormatter)
    _InfoLogger.addHandler(_InfoChannel)

    # For ERROR level
    _ErrorLogger.setLevel(INFO)
    _ErrorCh = logging.StreamHandler(sys.stderr)
    _ErrorCh.setFormatter(_ErrorFormatter)
    _ErrorLogger.addHandler(_ErrorCh)

## Set log level
#
#   @param  Level   One of log level in _LogLevel
def SetLevel(Level):
    if Level not in _LogLevels:
        info("Not supported log level (%d). Use default level instead." % Level)
        Level = INFO
    _DebugLogger.setLevel(Level)
    _InfoLogger.setLevel(Level)
    _ErrorLogger.setLevel(Level)

## Get current log level
def GetLevel():
    return _InfoLogger.getEffectiveLevel()

## Raise up warning as error
def SetWarningAsError():
    global _WarningAsError
    _WarningAsError = True

## Specify a file to store the log message as well as put on console
#
#   @param  LogFile     The file path used to store the log message
#
def SetLogFile(LogFile):
    if os.path.exists(LogFile):
        os.remove(LogFile)

    _Ch = logging.FileHandler(LogFile)
    _Ch.setFormatter(_DebugFormatter)
    _DebugLogger.addHandler(_Ch)

    _Ch= logging.FileHandler(LogFile)
    _Ch.setFormatter(_InfoFormatter)
    _InfoLogger.addHandler(_Ch)

    _Ch = logging.FileHandler(LogFile)
    _Ch.setFormatter(_ErrorFormatter)
    _ErrorLogger.addHandler(_Ch)

if __name__ == '__main__':
    pass

