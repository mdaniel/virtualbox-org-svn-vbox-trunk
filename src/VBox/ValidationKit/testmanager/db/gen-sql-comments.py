#!/usr/bin/env python
# -*- coding: utf-8 -*-
# $Id$

"""
Converts doxygen style comments in SQL script to COMMENT ON statements.
"""

__copyright__ = \
"""
Copyright (C) 2012-2017 Oracle Corporation

This file is part of VirtualBox Open Source Edition (OSE), as
available from http://www.virtualbox.org. This file is free software;
you can redistribute it and/or modify it under the terms of the GNU
General Public License (GPL) as published by the Free Software
Foundation, in version 2 as it comes in the "COPYING" file of the
VirtualBox OSE distribution. VirtualBox OSE is distributed in the
hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.

The contents of this file may alternatively be used under the terms
of the Common Development and Distribution License Version 1.0
(CDDL) only, as it comes in the "COPYING.CDDL" file of the
VirtualBox OSE distribution, in which case the provisions of the
CDDL are applicable instead of those of the GPL.

You may elect to license modified versions of this file under the
terms and conditions of either the GPL or the CDDL or both.
"""

import sys;
import re;


def errorMsg(sMsg):
    sys.stderr.write('error: %s\n' % (sMsg,));
    return 1;

class SqlDox(object):
    """
    Class for parsing relevant comments out of a pgsql file
    and emit COMMENT ON statements from it.
    """

    def __init__(self, oFile, sFilename):
        self.oFile              = oFile;
        self.sFilename          = sFilename;
        self.iLine              = 0;            # The current input line number.
        self.sComment           = None;         # The current comment.
        self.fCommentComplete   = False;        # Indicates that the comment has ended.
        self.sCommentSqlObj     = None;         # SQL object indicated by the comment (@table).
        self.sOuterSqlObj       = None;         # Like 'table yyyy' or 'type zzzz'.
        self.sPrevSqlObj        = None;         # Like 'table xxxx'.


    def error(self, sMsg):
        return errorMsg('%s(%d): %s' % (self.sFilename, self.iLine, sMsg,));

    def dprint(self, sMsg):
        sys.stderr.write('debug: %s\n' % (sMsg,));
        return True;

    def resetComment(self):
        self.sComment           = None;
        self.fCommentComplete   = False;
        self.sCommentSqlObj     = None;

    def quoteSqlString(self, s):
        return s.replace("'", "''");

    def commitComment2(self, sSqlObj):
        if self.sComment is not None and sSqlObj is not None:
            print("COMMENT ON %s IS\n  '%s';\n" % (sSqlObj, self.quoteSqlString(self.sComment.strip())));
        self.resetComment();
        return True;

    def commitComment(self):
        return self.commitComment2(self.sCommentSqlObj);

    def process(self):
        for sLine in self.oFile:
            self.iLine += 1;

            sLine = sLine.strip();
            self.dprint('line %d: %s\n' % (self.iLine, sLine));
            if sLine.startswith('--'):
                if sLine.startswith('--- '):
                    #
                    # New comment.
                    # The first list may have a @table, @type or similar that we're interested in.
                    #
                    self.commitComment();

                    sLine = sLine.lstrip('- ');
                    if sLine.startswith('@table '):
                        self.sCommentSqlObj = 'TABLE ' + (sLine[7:]).rstrip();
                        self.sComment = '';
                    elif sLine.startswith('@type '):
                        self.sCommentSqlObj = 'TYPE ' + (sLine[6:]).rstrip();
                        self.sComment = '';
                    elif sLine.startswith('@todo') \
                      or sLine.startswith('@file') \
                      or sLine.startswith('@page') \
                      or sLine.startswith('@name') \
                      or sLine.startswith('@{') \
                      or sLine.startswith('@}'):
                        # Ignore.
                        pass;
                    elif sLine.startswith('@'):
                        return self.error('Unknown tag: %s' % (sLine,));
                    else:
                        self.sComment = sLine;

                elif (sLine.startswith('-- ') or sLine == '--') \
                    and self.sComment is not None and self.fCommentComplete is False:
                    #
                    # Append line to comment.
                    #
                    if sLine == '--':
                        sLine = '';
                    else:
                        sLine = (sLine[3:]);
                    if self.sComment == '':
                        self.sComment = sLine;
                    else:
                        self.sComment += "\n" + sLine;

                elif sLine.startswith('--< '):
                    #
                    # Comment that starts on the same line as the object it describes.
                    #
                    sLine = (sLine[4:]).rstrip();
                    # => Later/never.
                else:
                    #
                    # Not a comment that interests us. So, complete any open
                    # comment and commit it if we know which SQL object it
                    # applies to.
                    #
                    self.fCommentComplete = True;
                    if self.sCommentSqlObj is not None:
                        self.commitComment();
            else:
                #
                # Not a comment. As above, we complete and optionally commit
                # any open comment.
                #
                self.fCommentComplete = True;
                if self.sCommentSqlObj is not None:
                    self.commitComment();

                #
                # Check for SQL (very fuzzy and bad).
                #
                asWords = sLine.split(' ');
                if    len(asWords) >= 3 \
                  and asWords[0] == 'CREATE':
                    # CREATE statement.
                    sType = asWords[1];
                    sName = asWords[2];
                    if sType == 'UNIQUE' and sName == 'INDEX' and len(asWords) >= 4:
                        sType = asWords[2];
                        sName = asWords[3];
                    if sType in ('TABLE', 'TYPE', 'INDEX', 'VIEW'):
                        self.sOuterSqlObj = sType + ' ' + sName;
                        self.sPrevSqlObj  = self.sOuterSqlObj;
                        self.dprint('%s' % (self.sOuterSqlObj,));
                        self.commitComment2(self.sOuterSqlObj);
                elif len(asWords) >= 1 \
                  and self.sOuterSqlObj is not None \
                  and self.sOuterSqlObj.startswith('TABLE ') \
                  and re.search("^(as|al|bm|c|enm|f|i|l|s|ts|uid|uuid)[A-Z][a-zA-Z0-9]*$", asWords[0]) is not None:
                    # Possibly a column name.
                    self.sPrevSqlObj = 'COLUMN ' + self.sOuterSqlObj[6:] + '.' + asWords[0];
                    self.dprint('column? %s' % (self.sPrevSqlObj));
                    self.commitComment2(self.sPrevSqlObj);

                #
                # Check for semicolon.
                #
                if sLine.find(");") >= 0:
                    self.sOuterSqlObj = None;

        return 0;


def usage():
    sys.stderr.write('usage: gen-sql-comments.py <filename.pgsql>\n'
                     '\n'
                     'The output goes to stdout.\n');
    return 0;


def main(asArgs):
    # Parse the argument. :-)
    sInput = None;
    if (len(asArgs) != 2):
        sys.stderr.write('syntax error: expected exactly 1 argument, a psql file\n');
        usage();
        return 2;
    sInput = asArgs[1];

    # Do the job, outputting to standard output.
    try:
        oFile = open(sInput, 'r');
    except:
        return errorMsg("failed to open '%s' for reading" % (sInput,));

    # header.
    print("-- $" "Id" "$");
    print("--- @file");
    print("-- Autogenerated from %s.  Do not edit!" % (sInput,));
    print("--");
    print("");
    for sLine in __copyright__.split('\n'):
        if len(sLine) > 0:
            print("-- %s" % (sLine,));
        else:
            print("--");
    print("");
    print("");
    me = SqlDox(oFile, sInput);
    return me.process();

sys.exit(main(sys.argv));

