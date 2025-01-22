# -*- coding: utf-8 -*-
# $Id$

"""
Test eXecution Service Shell.
"""
__copyright__ = \
"""
Copyright (C) 2025 Oracle and/or its affiliates.

This file is part of VirtualBox base platform packages, as
available from https://www.virtualbox.org.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation, in version 3 of the
License.

This program is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, see <https://www.gnu.org/licenses>.

The contents of this file may alternatively be used under the terms
of the Common Development and Distribution License Version 1.0
(CDDL), a copy of it is provided in the "COPYING.CDDL" file included
in the VirtualBox distribution, in which case the provisions of the
CDDL are applicable instead of those of the GPL.

You may elect to license modified versions of this file under the
terms and conditions of either the GPL or the CDDL or both.

SPDX-License-Identifier: GPL-3.0-only OR CDDL-1.0
"""
__version__ = '$Revision$'

import code;
import os;
import sys;
import time;

# Make sure that the ModuleNotFoundError exception is found by Python 2.x.
try:
    ModuleNotFoundError
except NameError:
    ModuleNotFoundError = ImportError # pylint: disable=redefined-builtin

try:    __file__                      # pylint: disable=used-before-assignment
except: __file__ = sys.argv[0];
g_ksValidationKitDir = os.path.dirname(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', '..', '..'));
sys.path.append(g_ksValidationKitDir);
try:
    from testdriver import reporter;
    from testdriver import txsclient;
except ModuleNotFoundError:
    print('Error: Validation Kit testdriver module not found. Tweak your module import paths.');
    print('Search path is: %s' % (sys.path,));
    sys.exit(1);

class TxsShell(code.InteractiveConsole):
    """
    The TxsShell implementation, derivred from code.InteractiveConsole.
    """
    def __init__(self, shellLocals=None, filename='<console>'):
        """
        Constructor.
        """
        code.InteractiveConsole.__init__(self, locals=shellLocals, filename=filename);
        self.prompt = '>>> ';
        self.cmds = {
            'c': self.cmdConnect,
            'connect': self.cmdConnect,
            'd': self.cmdDisconnect,
            'disconnect': self.cmdDisconnect,
            'h': self.cmdHelp,
            'help': self.cmdHelp,
            'q': self.cmdQuit,
            'quit': self.cmdQuit,
            'r': self.cmdReconnect,
            'reconnect': self.cmdReconnect,
            's': self.cmdStatus,
            'status': self.cmdStatus,
            'v': self.cmdVerbose,
            'verbose': self.cmdVerbose,
        };
        self.fTxsReversedSetup = None;
        self.oTxsTransport = None;
        self.oTxsSession = None;

        self.setConnection();

    def mainLoop(self):
        """
        The REPL main loop.
        """
        self.log('Welcome to VirtualBox TxsShell!\n\n'
                 'Type \'!quit\' or press Ctrl+D / CTRL + Z to exit.\n'
                 'Type \'!help\' for help.\n');
        self.log('Default connection is set to %s:%u (%s)\n' \
                 % (self.sTxsHost, self.uTxsPort, 'reversed, as server' if self.fTxsReversedSetup else 'as client'));

        try:
            self.interact(banner = '');
        except SystemExit:
            pass
        finally:
            self.disconnect(fQuiet = True);
            self.log('\nExiting. Have a nice day!');

    def setConnection(self, sHostname=None, uPort=None, fReversed=None):
        """
        Set the TXS connection parameters.

        Those will be used for retrieving the lastly used connection parameters.
        """
        try:
            if not fReversed: # Use a reversed connection by default (if not explicitly specified).
                fReversed = True;
            self.sTxsHost = '127.0.0.1' if sHostname is None else sHostname;
            assert self.sTxsHost is not None;
            if uPort is None:
                self.uTxsPort = 5048 if fReversed else 5042;
            else:
                self.uTxsPort = int(uPort);
            self.fTxsReversedSetup = True if fReversed is None else fReversed;
            self.oTxsTransport = None;
            self.oTxsSession = None;
        except ValueError:
            self.log('Invalid parameters given!');
            return False;
        return True;

    def connect(self, sHostname=None, uPort=None, fReversed=None, cMsTimeout = 30 * 1000):
        """
        Connects to a TXS instance (server or client).
        """
        if not sHostname:
            sHostname = self.sTxsHost;
        if not uPort:
            uPort = self.uTxsPort;
        if not fReversed:
            fReversed = self.fTxsReversedSetup;
        self.log('%s (%dms timeout) ...'
                 % ('Waiting for connection from TXS' if fReversed is True else 'Connecting to TXS', cMsTimeout));
        try:
            self.oTxsTransport = txsclient.TransportTcp(sHostname, uPort, fReversed);
            if not self.oTxsTransport:
                return False;
            self.oTxsSession   = txsclient.Session(self.oTxsTransport, cMsTimeout, cMsIdleFudge = 500, fTryConnect = True);
            if not self.oTxsSession:
                return False;
            while not self.oTxsSession.isSuccess():
                time.sleep(self.oTxsSession.getMsLeft(1, 1000) / 1000.0);
                self.log('.', end='', flush=True);
        except KeyboardInterrupt:
            self.oTxsSession.cancelTask();
            self.log('Connection cancelled');
            self.disconnect();
            return False;
        self.log(''); # New line after connection dots.
        if self.oTxsSession.hasTimedOut() \
        or self.oTxsSession.isCancelled():
            self.log('Connection error');
            self.disconnect();
            return False;

        self.log('Connection successful');
        return True;

    def disconnect(self, fQuiet = True):
        """
        Disconnects from a TXS instance.
        """
        if self.oTxsSession:
            if self.oTxsTransport:
                rc = self.oTxsSession.sendMsg('BYE');
                if rc is True:
                    rc = self.oTxsSession.recvAckLogged('BYE');
                self.oTxsTransport.disconnect();
                self.oTxsTransport = None;
            self.oTxsSession = None;
            if not fQuiet:
                self.log('Disconnected.');
        elif not fQuiet:
            self.log('Not connected.');

    def reconnect(self, fQuiet = False):
        """
        Re-connects to a TXS instance.
        """
        self.disconnect(fQuiet);
        self.connect(fQuiet);

    def sendMsg(self, sOpcode, asArgs):
        """
        Sends a message to a TXS instance.
        """
        if not self.oTxsSession:
            self.log('Not connected.');
            return;
        fRc = self.oTxsSession.sendMsg(sOpcode, aoPayload = asArgs);
        if fRc:
            fRc = self.oTxsSession.recvAckLogged(sOpcode, False);

    def cmdConnect(self, sHostname=None, uPort=None, fReversed=None, fQuiet = False):
        """
        Handles the '!connect' / '!c' command.
        """
        self.disconnect();
        if not self.setConnection(sHostname, uPort, fReversed):
            return;

        if not fQuiet:
            self.log('Connecting to %s:%u (%s) ...' % \
                       (self.sTxsHost, self.uTxsPort, 'reversed, as server' if self.fTxsReversedSetup else 'as client'));
        fRc = self.connect(sHostname, uPort, fReversed);
        if not fRc:
            self.log('Hint: Is connecting to the host allowed by the VM?\n' \
                     '      Try VBoxManage modifyvm <VM> --nat-localhostreachable1=on\n\n');

    def cmdVerbose(self):
        """
        Handles the '!verbose' / '!v' command.
        """
        reporter.incVerbosity();
        reporter.incVerbosity();
        reporter.incVerbosity();

    def cmdDisconnect(self):
        """
        Handles the '!disconnect' / '!d' command.
        """
        self.disconnect(fQuiet = False);

    def cmdHelp(self):
        """
        Handles the '!help' / '!h' command.
        """
        self.log('!connect|!c');
        self.log('    Connects to a TXS instance.');
        self.log('!disconnect|!d');
        self.log('    Disconnects from a TXS instance.');
        self.log('!help|!h');
        self.log('    Displays this help.');
        self.log('!quit|!q');
        self.log('    Quits this application.');
        self.log('!reconnect|!r');
        self.log('    Reconnects to the last connected TXS instance.');
        self.log('!status|!s');
        self.log('    Prints the current connection status.');
        self.log('!verbose|!v');
        self.log('    Increases the logging verbosity.');
        self.log();
        self.log('When connected to a TXS instance, anything else will be sent');
        self.log('directly to TXS (including parameters), for example:');
        self.log('    ISDIR /tmp/');
        self.log();

    def cmdQuit(self):
        """
        Handles the '!quit' / '!q' command.
        """
        raise SystemExit;

    def cmdReconnect(self):
        """
        Handles the '!reconnect' / '!r' command.
        """
        self.reconnect();

    def cmdStatus(self):
        """
        Handles the '!s' / '!s' command.
        """
        if self.oTxsSession:
            self.log('Connected to %s:%u (%s)' % \
                     (self.sTxsHost, self.uTxsPort, 'reversed, as server' if self.fTxsReversedSetup else 'as client'));
        else:
            self.log('Not connected.');

    def log(self, sText = None, end = '\n', flush = None):
        """
        Prints a message
        """
        if sText:
            sys.stdout.write(sText);
        if end:
            sys.stdout.write(end);
        if flush is True:
            sys.stdout.flush();

    def raw_input(self, prompt=''):
        """
        Overwrites raw_input() to handle custom commands.
        """
        try:
            sLine = code.InteractiveConsole.raw_input(self, prompt);
        except EOFError:
            raise SystemExit;
        except KeyboardInterrupt:
            self.log();
            return self.raw_input(prompt);

        # Check if the line is a custom command.
        if sLine.startswith('!'):
            asCmdTokens = sLine[1:].strip().split();
            if not asCmdTokens:
                self.log('No command specified.');
                return self.raw_input(prompt);

            sCmdName = asCmdTokens[0];
            asArgs = asCmdTokens[1:];

            if sCmdName in self.cmds:
                try:
                    # Call the custom command with the provided arguments.
                    self.cmds[sCmdName](*asArgs);
                except TypeError as e:
                    self.log('*** Error: %s' % (e, ));
                return self.raw_input(prompt);

            self.log('*** Unknown command: %s' % (sCmdName, ));
            return self.raw_input(prompt);

        # Send to TXS.
        asCmdTokens = sLine.strip().split();
        if asCmdTokens:
            self.sendMsg(asCmdTokens[0], asCmdTokens[1:]);

        return ''; # Don't evaluate as normal Python code.

if __name__ == '__main__':
    TxsShell(shellLocals=globals()).mainLoop();
