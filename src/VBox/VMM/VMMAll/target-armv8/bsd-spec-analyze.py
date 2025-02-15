#!/usr/bin/env python
# -*- coding: utf-8 -*-
# $Id$

"""
ARM BSD specification analyser.
"""

from __future__ import print_function;

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
__version__ = "$Revision$"

# Standard python imports.
import argparse;
import collections;
import json;
import os;
import sys;
import tarfile;


class ArmEncodesetField(object):
    """
    ARM Encodeset.Bits & Encodeset.Field.
    """
    def __init__(self, oJson, iFirstBit, cBitsWidth, fFixed, fValue, sName = None):
        self.oJson      = oJson;
        self.iFirstBit  = iFirstBit;
        self.cBitsWidth = cBitsWidth;
        self.fFixed     = fFixed;
        self.fValue     = fValue;
        self.sName      = sName; ##< None if Encodeset.Bits.

    def __str__(self):
        sRet = '[%2u:%-2u] = %#x/%#x/%#x' % (
            self.iFirstBit + self.cBitsWidth - 1, self.iFirstBit, self.fValue, self.fFixed, self.getMask()
        );
        if self.sName:
            sRet += ' # %s' % (self.sName,)
        return sRet;

    def __repr__(self):
        return self.__str__();

    def getMask(self):
        """ Field mask (unshifted). """
        return (1 << self.cBitsWidth) - 1;

    def getShiftedMask(self):
        """ Field mask, shifted. """
        return ((1 << self.cBitsWidth) - 1) << self.iFirstBit;

    @staticmethod
    def fromJson(oJson):
        """ """
        assert oJson['_type'] in ('Instruction.Encodeset.Field', 'Instruction.Encodeset.Bits'), oJson['_type'];

        oRange = oJson['range'];
        assert oRange['_type'] == 'Range';
        iFirstBit  = int(oRange['start']);
        cBitsWidth = int(oRange['width']);

        sValue = oJson['value']['value'];
        assert sValue[0] == '\'' and sValue[-1] == '\'', sValue;
        sValue = sValue[1:-1];
        assert len(sValue) == cBitsWidth, 'cBitsWidth=%s sValue=%s' % (cBitsWidth, sValue,);
        fFixed = 0;
        fValue = 0;
        for ch in sValue:
            assert ch in 'x10', 'ch=%s' % ch;
            fFixed <<= 1;
            fValue <<= 1;
            if ch != 'x':
                fFixed |= 1;
                if ch == '1':
                    fValue |= 1;

        sName = oJson['name'] if oJson['_type'] == 'Instruction.Encodeset.Field' else None;
        return ArmEncodesetField(oJson, iFirstBit, cBitsWidth, fFixed, fValue, sName);

    @staticmethod
    def fromJsonEncodeset(oJson, aoSet, fCovered):
        """ """
        assert oJson['_type'] == 'Instruction.Encodeset.Encodeset', oJson['_type'];
        for oJsonValue in oJson['values']:
            oNewField = ArmEncodesetField.fromJson(oJsonValue);
            fNewMask  = oNewField.getShiftedMask();
            if (fNewMask & fCovered) != fNewMask:
                aoSet.append(oNewField)
                fCovered |= fNewMask;
        return (aoSet, fCovered);


class ArmInstruction(object):
    """
    ARM instruction
    """
    def __init__(self, oJson, sName, sMemonic, aoEncodesets):
        self.oJson           = oJson;
        self.sName           = sName;
        self.sMnemonic       = sMemonic;
        self.aoEncodesets    = aoEncodesets;
        self.fFixedMask      = 0;
        self.fFixedValue     = 0;
        for oField in aoEncodesets:
            self.fFixedMask  |= oField.fFixed << oField.iFirstBit;
            self.fFixedValue |= oField.fValue << oField.iFirstBit;

    def __str__(self):
        sRet = 'sName=%s; sMnemonic=%s fFixedValue/Mask=%#x/%#x encoding=\n    %s' % (
            self.sName, self.sMnemonic, self.fFixedValue, self.fFixedMask,
            ',\n    '.join([str(s) for s in self.aoEncodesets]),
        );
        return sRet;

    def __repr__(self):
        return self.__str__();

## All the instructions.
g_aoAllArmInstructions = []             # type: List[ArmInstruction]

## All the instructions by name (not mnemonic.
g_dAllArmInstructionsByName = {}        # type: Dict[ArmInstruction]


def parseInstructions(aoStack, aoJson):
    for oJson in aoJson:
        if oJson['_type'] == "Instruction.InstructionSet":
            parseInstructions([oJson,] + aoStack, oJson['children']);
        elif oJson['_type'] == "Instruction.InstructionGroup":
            parseInstructions([oJson,] + aoStack, oJson['children']);
        elif oJson['_type'] == "Instruction.Instruction":
            #aoJsonEncodings = [oJson['encoding'],];
            (aoEncodesets, fCovered) = ArmEncodesetField.fromJsonEncodeset(oJson['encoding'], [], 0);
            for oParent in aoStack:
                if 'encoding' in oParent:
                    (aoEncodesets, fCovered) = ArmEncodesetField.fromJsonEncodeset(oParent['encoding'], aoEncodesets, fCovered);
            oInstr = ArmInstruction(oJson, oJson['name'], oJson['name'], aoEncodesets);

            g_aoAllArmInstructions.append(oInstr);
            assert oInstr.sName not in g_dAllArmInstructionsByName;
            g_dAllArmInstructionsByName[oInstr.sName] = oInstr;


def bsdSpecAnalysis(asArgs):
    """ Main function. """

    #
    # Parse arguments.
    #
    oArgParser = argparse.ArgumentParser(add_help = False);
    oArgParser.add_argument('--tar',
                            metavar = 'AARCHMRS_BSD_A_profile-2024-12.tar.gz',
                            dest    = 'sTarFile',
                            action  = 'store',
                            default = None,
                            help    = 'Specification TAR file to get the files from.');
    oArgParser.add_argument('--instructions',
                            metavar = 'Instructions.json',
                            dest    = 'sFileInstructions',
                            action  = 'store',
                            default = 'Instructions.json',
                            help    = 'The path to the instruction specficiation file.');
    oArgParser.add_argument('--features',
                            metavar = 'Features.json',
                            dest    = 'sFileFeatures',
                            action  = 'store',
                            default = 'Features.json',
                            help    = 'The path to the features specficiation file.');
    oArgParser.add_argument('--registers',
                            metavar = 'Registers.json',
                            dest    = 'sFileRegisters',
                            action  = 'store',
                            default = 'Registers.json',
                            help    = 'The path to the registers specficiation file.');
    oArgParser.add_argument('--spec-dir',
                            metavar = 'dir',
                            dest    = 'sSpecDir',
                            action  = 'store',
                            default = '',
                            help    = 'Specification directory to prefix the specficiation files with.');
    oOptions = oArgParser.parse_args(asArgs[1:]);

    #
    # Load the files.
    #
    print("loading specs ...");
    if oOptions.sTarFile:
        with tarfile.open(oOptions.sTarFile, 'r') as oTarFile:
            with oTarFile.extractfile(oOptions.sFileInstructions) as oFile:
                dRawInstructions = json.load(oFile);
            #with open(sFileFeatures, 'r', encoding = 'utf-8') as oFile:
            #    dRawFeatures     = json.load(oFile);
            #with open(sFileRegisters, 'r', encoding = 'utf-8') as oFile:
            #    dRawRegisters    = json.load(oFile);
    else:
        if oOptions.sSpecDir:
            if not os.path.isabs(oOptions.sFileInstructions):
                oOptions.sFileInstructions = os.path.normpath(os.path.join(oOptions.sSpecDir, oOptions.sFileInstructions));
            if not os.path.isabs(oOptions.sFileFeatures):
                oOptions.sFileFeatures     = os.path.normpath(os.path.join(oOptions.sSpecDir, oOptions.sFileFeatures));
            if not os.path.isabs(oOptions.sFileRegisters):
                oOptions.sFileRegisters    = os.path.normpath(os.path.join(oOptions.sSpecDir, oOptions.sFileRegisters));

        with open(oOptions.sFileInstructions, 'r', encoding = 'utf-8') as oFile:
            dRawInstructions = json.load(oFile);
        #with open(oOptions.sFileFeatures, 'r', encoding = 'utf-8') as oFile:
        #    dRawFeatures     = json.load(oFile);
        #with open(oOptions.sFileRegisters, 'r', encoding = 'utf-8') as oFile:
        #    dRawRegisters    = json.load(oFile);
    print("... done loading.");

    #
    # Parse the Instructions.
    #
    print("parsing instructions ...");
    parseInstructions([], dRawInstructions['instructions']);
    print("Found %u instructions." % (len(g_aoAllArmInstructions),));

    #oBrk = g_dAllArmInstructionsByName['BRK_EX_exception'];
    #print("oBrk=%s" % (oBrk,))

    if False:
        for oInstr in g_aoAllArmInstructions:
            print('%08x/%08x %s' % (oInstr.fFixedMask, oInstr.fFixedValue, oInstr.sName));

    # Gather stats on fixed bits:
    if True:
        dCounts = collections.Counter();
        for oInstr in g_aoAllArmInstructions:
            cPopCount = bin(oInstr.fFixedMask).count('1');
            dCounts[cPopCount] += 1;

        print('');
        print('Fixed bit pop count distribution:');
        for i in range(33):
            if i in dCounts:
                print('  %2u: %u' % (i, dCounts[i]));

    # Top 10 fixed masks.
    if True:
        dCounts = collections.Counter();
        for oInstr in g_aoAllArmInstructions:
            dCounts[oInstr.fFixedMask] += 1;

        print('');
        print('Top 20 fixed masks:');
        for fFixedMask, cHits in dCounts.most_common(20):
            print('  %#x: %u times' % (fFixedMask, cHits,));

    return 0;


if __name__ == '__main__':
    sys.exit(bsdSpecAnalysis(sys.argv));

