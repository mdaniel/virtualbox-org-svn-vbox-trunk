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

SPDX-License-Identifier: GPL-3.0-only
"""
__version__ = "$Revision$"

# Standard python imports.
import argparse;
import ast;
import collections;
import datetime;
import json;
import operator;
import os;
import re;
import sys;
import tarfile;
import traceback;


#
# The ARM instruction AST stuff.
#

class ArmAstBase(object):
    """
    ARM instruction AST base class.
    """

    kTypeBinaryOp   = 'AST.BinaryOp';
    kTypeBool       = 'AST.Bool';
    kTypeConcat     = 'AST.Concat';
    kTypeFunction   = 'AST.Function';
    kTypeIdentifier = 'AST.Identifier';
    kTypeInteger    = 'AST.Integer';
    kTypeSet        = 'AST.Set';
    kTypeSquareOp   = 'AST.SquareOp';
    kTypeUnaryOp    = 'AST.UnaryOp';
    kTypeValue      = 'Values.Value';

    def __init__(self, sType):
        self.sType = sType;

    def assertAttribsInSet(oJson, oAttribSet):
        """ Checks that the JSON element has all the attributes in the set and nothing else. """
        assert set(oJson) == oAttribSet, '%s - %s' % (set(oJson) ^ oAttribSet, oJson,);

    kAttribSetBinaryOp = frozenset(['_type', 'left', 'op', 'right']);
    @staticmethod
    def fromJsonBinaryOp(oJson):
        ArmAstBase.assertAttribsInSet(oJson, ArmAstBase.kAttribSetBinaryOp);
        return ArmAstBinaryOp(ArmAstBase.fromJson(oJson['left']), oJson['op'], ArmAstBase.fromJson(oJson['right']));

    kAttribSetUnaryOp = frozenset(['_type', 'op', 'expr']);
    @staticmethod
    def fromJsonUnaryOp(oJson):
        ArmAstBase.assertAttribsInSet(oJson, ArmAstBase.kAttribSetUnaryOp);
        return ArmAstUnaryOp(oJson['op'], ArmAstBase.fromJson(oJson['expr']));

    kAttribSetSquareOp = frozenset(['_type', 'var', 'arguments']);
    @staticmethod
    def fromJsonSquareOp(oJson):
        ArmAstBase.assertAttribsInSet(oJson, ArmAstBase.kAttribSetSquareOp);
        return ArmAstSquareOp(ArmAstBase.fromJson(oJson['var']), [ArmAstBase.fromJson(oArg) for oArg in oJson['arguments']]);

    kAttribSetConcat = frozenset(['_type', 'values']);
    @staticmethod
    def fromJsonConcat(oJson):
        ArmAstBase.assertAttribsInSet(oJson, ArmAstBase.kAttribSetConcat);
        return ArmAstConcat([ArmAstBase.fromJson(oArg) for oArg in oJson['values']]);

    kAttribSetFunction = frozenset(['_type', 'name', 'arguments']);
    @staticmethod
    def fromJsonFunction(oJson):
        ArmAstBase.assertAttribsInSet(oJson, ArmAstBase.kAttribSetFunction);
        return ArmAstFunction(oJson['name'], [ArmAstBase.fromJson(oArg) for oArg in oJson['arguments']]);

    kAttribSetIdentifier = frozenset(['_type', 'value']);
    @staticmethod
    def fromJsonIdentifier(oJson):
        ArmAstBase.assertAttribsInSet(oJson, ArmAstBase.kAttribSetIdentifier);
        return ArmAstIdentifier(oJson['value']);

    kAttribSetBool = frozenset(['_type', 'value']);
    @staticmethod
    def fromJsonBool(oJson):
        ArmAstBase.assertAttribsInSet(oJson, ArmAstBase.kAttribSetBool);
        return ArmAstBool(oJson['value']);

    kAttribSetInteger = frozenset(['_type', 'value']);
    @staticmethod
    def fromJsonInteger(oJson):
        ArmAstBase.assertAttribsInSet(oJson, ArmAstBase.kAttribSetInteger);
        return ArmAstInteger(oJson['value']);

    kAttribSetSet = frozenset(['_type', 'values']);
    @staticmethod
    def fromJsonSet(oJson):
        ArmAstBase.assertAttribsInSet(oJson, ArmAstBase.kAttribSetSet);
        return ArmAstSet([ArmAstBase.fromJson(oArg) for oArg in oJson['values']]);

    kAttribSetValue = frozenset(['_type', 'value', 'meaning']);
    @staticmethod
    def fromJsonValue(oJson):
        ArmAstBase.assertAttribsInSet(oJson, ArmAstBase.kAttribSetValue);
        return ArmAstValue(oJson['value']);

    kfnTypeMap = {
        kTypeBinaryOp:      fromJsonBinaryOp,
        kTypeUnaryOp:       fromJsonUnaryOp,
        kTypeSquareOp:      fromJsonSquareOp,
        kTypeConcat:        fromJsonConcat,
        kTypeFunction:      fromJsonFunction,
        kTypeIdentifier:    fromJsonIdentifier,
        kTypeBool:          fromJsonBool,
        kTypeInteger:       fromJsonInteger,
        kTypeSet:           fromJsonSet,
        kTypeValue:         fromJsonValue,
    };

    @staticmethod
    def fromJson(oJson):
        """ Decodes an AST/Values expression. """
        #print('debug ast: %s' % oJson['_type'])
        return ArmAstBase.kfnTypeMap[oJson['_type']](oJson);


class ArmAstBinaryOp(ArmAstBase):
    kOpTypeCompare      = 'cmp';
    kOpTypeLogical      = 'log';
    kOpTypeArithmetical = 'arit';
    kOpTypeSet          = 'set';
    kdOps = {
        '||': kOpTypeLogical,
        '&&': kOpTypeLogical,
        '==': kOpTypeCompare,
        '!=': kOpTypeCompare,
        '>':  kOpTypeCompare,
        '>=': kOpTypeCompare,
        '<=': kOpTypeCompare,
        'IN': kOpTypeSet,
        '+':  kOpTypeArithmetical,
    };

    def __init__(self, oLeft, sOp, oRight):
        ArmAstBase.__init__(self, ArmAstBase.kTypeBinaryOp);
        assert sOp in ArmAstBinaryOp.kdOps, 'sOp="%s"' % (sOp,);
        self.oLeft  = oLeft;
        self.sOp    = sOp;
        self.oRight = oRight;

        # Switch value == field non-sense (simplifies transferConditionsToEncoding and such):
        if (    isinstance(oRight, ArmAstIdentifier)
            and isinstance(oLeft, [ArmAstValue, ArmAstInteger])
            and sOp in ['==', '!=']):
            self.oLeft  = oRight;
            self.oRight = oLeft;

    @staticmethod
    def needParentheses(oNode, sOp = '&&'):
        if isinstance(oNode, ArmAstBinaryOp):
            if sOp != '&&'  or  oNode.sOp in ('||', '+'):
                return True;
        return False;

    def toString(self):
        sLeft = self.oLeft.toString();
        if ArmAstBinaryOp.needParentheses(self.oLeft, self.sOp):
            sLeft = '(%s)' % (sLeft);

        sRight = self.oRight.toString();
        if ArmAstBinaryOp.needParentheses(self.oRight, self.sOp):
            sRight = '(%s)' % (sRight);

        return '%s %s %s' % (sLeft, self.sOp, sRight);

class ArmAstUnaryOp(ArmAstBase):
    kOpTypeLogical      = 'log';
    kdOps = {
        '!': kOpTypeLogical,
    };

    def __init__(self, sOp, oExpr):
        ArmAstBase.__init__(self, ArmAstBase.kTypeUnaryOp);
        assert sOp in ArmAstUnaryOp.kdOps, 'sOp=%s' % (sOp,);
        self.sOp   = sOp;
        self.oExpr = oExpr;

    def toString(self):
        if ArmAstBinaryOp.needParentheses(self.oExpr):
            return '%s(%s)' % (self.sOp, self.oExpr.toString(),);
        return '%s%s' % (self.sOp, self.oExpr.toString(),);

class ArmAstSquareOp(ArmAstBase):
    def __init__(self, aoValues):
        ArmAstBase.__init__(self, ArmAstBase.kTypeSquareOp);
        self.aoValues = aoValues;

    def toString(self):
        return '<%s>' % (','.join([oValue.toString() for oValue in self.aoValues]),);


class ArmAstConcat(ArmAstBase):
    def __init__(self, aoValues):
        ArmAstBase.__init__(self, ArmAstBase.kTypeConcat);
        self.aoValues = aoValues;

    def toString(self):
        sRet = '';
        for oValue in self.aoValues:
            if sRet:
                sRet += ':'
            if isinstance(oValue, ArmAstIdentifier):
                sRet += oValue.sName;
            else:
                sRet += '(%s)' % (oValue.toString());
        return sRet;

class ArmAstFunction(ArmAstBase):
    s_oReValidName = re.compile('^[_A-Za-z][_A-Za-z0-9]+$');

    def __init__(self, sName, aoArgs):
        ArmAstBase.__init__(self, ArmAstBase.kTypeFunction);
        assert self.s_oReValidName.match(sName), 'sName=%s' % (sName);
        self.sName  = sName;
        self.aoArgs = aoArgs;

    def toString(self):
        return '%s(%s)' % (self.sName, ','.join([oArg.toString() for oArg in self.aoArgs]),);

class ArmAstIdentifier(ArmAstBase):
    s_oReValidName = re.compile('^[_A-Za-z][_A-Za-z0-9]*$');

    def __init__(self, sName):
        ArmAstBase.__init__(self, ArmAstBase.kTypeIdentifier);
        assert self.s_oReValidName.match(sName), 'sName=%s' % (sName);
        self.sName = sName;

    def toString(self):
        return self.sName;

class ArmAstBool(ArmAstBase):
    def __init__(self, fValue):
        ArmAstBase.__init__(self, ArmAstBase.kTypeBool);
        assert fValue is True or fValue is False, '%s' % (fValue,);
        self.fValue = fValue;

    def toString(self):
        return 'true' if self.fValue is True else 'false';


class ArmAstInteger(ArmAstBase):
    def __init__(self, iValue):
        ArmAstBase.__init__(self, ArmAstBase.kTypeInteger);
        self.iValue = int(iValue);

    def toString(self):
        return '%#x' % (self.iValue,);

class ArmAstSet(ArmAstBase):
    def __init__(self, aoValues):
        ArmAstBase.__init__(self, ArmAstBase.kTypeSet);
        self.aoValues = aoValues;

    def toString(self):
        return '(%s)' % (', '.join([oValue.toString() for oValue in self.aoValues]),);

class ArmAstValue(ArmAstBase):
    def __init__(self, sValue):
        ArmAstBase.__init__(self, ArmAstBase.kTypeValue);
        self.sValue = sValue;

    def toString(self):
        return self.sValue;


#
# Instructions and their properties.
#

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
    def parseValue(sValue, cBitsWidth):
        """
        Returns (fValue, fFixed) tuple on success, raises AssertionError otherwise.
        """
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
        return (fValue, fFixed);

    @staticmethod
    def fromJson(oJson):
        """ """
        assert oJson['_type'] in ('Instruction.Encodeset.Field', 'Instruction.Encodeset.Bits'), oJson['_type'];

        oRange = oJson['range'];
        assert oRange['_type'] == 'Range';
        iFirstBit        = int(oRange['start']);
        cBitsWidth       = int(oRange['width']);
        sName            = oJson['name'] if oJson['_type'] == 'Instruction.Encodeset.Field' else None;
        (fValue, fFixed) = ArmEncodesetField.parseValue(oJson['value']['value'], cBitsWidth);
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
    s_oReValidName = re.compile('^[_A-Za-z][_A-Za-z0-9]+$');

    def __init__(self, oJson, sName, sMemonic, aoEncodesets, oCondition):
        assert self.s_oReValidName.match(sName), 'sName=%s' % (sName);
        self.oJson           = oJson;
        self.sName           = sName;
        self.sMnemonic       = sMemonic;
        self.sAsmDisplay     = '';
        self.aoEncodesets    = aoEncodesets;
        self.oCondition      = oCondition;
        self.fFixedMask      = 0;
        self.fFixedValue     = 0;
        for oField in aoEncodesets:
            self.fFixedMask  |= oField.fFixed << oField.iFirstBit;
            self.fFixedValue |= oField.fValue << oField.iFirstBit;

        # State related to decoder.
        self.fDecoderLeafCheckNeeded = False;    ##< Whether we need to check fixed value/mask in leaf decoder functions.

    def __str__(self):
        sRet = 'sName=%s; sMnemonic=%s fFixedValue/Mask=%#x/%#x encoding=\n    %s' % (
            self.sName, self.sMnemonic, self.fFixedValue, self.fFixedMask,
            ',\n    '.join([str(s) for s in self.aoEncodesets]),
        );
        return sRet;

    def __repr__(self):
        return self.__str__();

    def getCName(self):
        # Get rid of trailing underscore as it seems pointless.
        if self.sName[-1] != '_' or self.sName[:-1] in g_dAllArmInstructionsByName:
            return self.sName;
        return self.sName[:-1];


#
# AArch64 Specification Loader.
#

## All the instructions.
g_aoAllArmInstructions = []             # type: List[ArmInstruction]

## All the instructions by name (not mnemonic.
g_dAllArmInstructionsByName = {}        # type: Dict[ArmInstruction]

#
#  Pass #1 - Snoop up all the instructions and their encodings.
#
def parseInstructions(aoStack, aoJson):
    for oJson in aoJson:
        if oJson['_type'] == "Instruction.InstructionSet":
            parseInstructions([oJson,] + aoStack, oJson['children']);
        elif oJson['_type'] == "Instruction.InstructionGroup":
            parseInstructions([oJson,] + aoStack, oJson['children']);
        elif oJson['_type'] == "Instruction.Instruction":
            sInstrNm = oJson['name'];

            (aoEncodesets, fCovered) = ArmEncodesetField.fromJsonEncodeset(oJson['encoding'], [], 0);
            for oParent in aoStack:
                if 'encoding' in oParent:
                    (aoEncodesets, fCovered) = ArmEncodesetField.fromJsonEncodeset(oParent['encoding'], aoEncodesets, fCovered);

            oCondition = ArmAstBase.fromJson(oJson['condition']);
            print('debug transfer in:  %s' % (oCondition.toString()));
            (oCondition, fMod) = transferConditionsToEncoding(oCondition, aoEncodesets, collections.defaultdict(list), sInstrNm);
            if fMod:
                print('debug transfer out: %s' % (oCondition.toString()));

            oInstr = ArmInstruction(oJson, sInstrNm, sInstrNm, aoEncodesets, oCondition);

            g_aoAllArmInstructions.append(oInstr);
            assert oInstr.sName not in g_dAllArmInstructionsByName;
            g_dAllArmInstructionsByName[oInstr.sName] = oInstr;
    return True;

def transferConditionsToEncoding(oCondition, aoEncodesets, dPendingNotEq, sInstrNm, uDepth = 0, fMod = False):
    """
    This is for dealing with stuff like asr_z_p_zi_ and lsr_z_p_zi_ which has
    the same fixed encoding fields in the specs, but differs in the value of
    the named field 'U' as expressed in the conditions.

    This function will recursively take 'Field == value/integer' expression out
    of the condition tree and add them to the encodeset conditions when possible.

    The dPendingNotEq stuff is a hack to deal with stuff like this:
        sdot_z_zzz_:     U == '0' && size != '01' && size != '00'
                     && (IsFeatureImplemented(FEAT_SVE) || IsFeatureImplemented(FEAT_SME))
    The checks can be morphed into the 'size' field encoding criteria as '0b0x'.
    """
    def isBoolTrue(oNode):
        return isinstance(oNode, ArmAstBool) and oNode.fValue is True;

    if isinstance(oCondition, ArmAstBinaryOp):
        if oCondition.sOp == '&&':
            print('debug transfer: %s: recursion...' % (sInstrNm,));
            # Recurse into each side of an AND expression.
            (oCondition.oLeft, fMod)  = transferConditionsToEncoding(oCondition.oLeft,  aoEncodesets, dPendingNotEq,
                                                                     sInstrNm, uDepth + 1, fMod);
            (oCondition.oRight, fMod) = transferConditionsToEncoding(oCondition.oRight, aoEncodesets, dPendingNotEq,
                                                                     sInstrNm, uDepth + 1, fMod);
            if isBoolTrue(oCondition.oLeft):
                return (oCondition.oRight, fMod);
            if isBoolTrue(oCondition.oRight):
                return (oCondition.oLeft, fMod);

        elif oCondition.sOp in ('==', '!='):
            # The pattern we're looking for is identifier (field) == fixed value.
            print('debug transfer: %s: binaryop %s vs %s ...' % (sInstrNm, oCondition.oLeft.sType, oCondition.oRight.sType));
            if (    isinstance(oCondition.oLeft, ArmAstIdentifier)
                and isinstance(oCondition.oRight, (ArmAstValue, ArmAstInteger))):
                sFieldName = oCondition.oLeft.sName;
                oValue     = oCondition.oRight;
                print('debug transfer: %s: binaryop step 2...' % (sInstrNm,));
                for oField in aoEncodesets: # ArmEncodesetField
                    if oField.sName and oField.sName == sFieldName:
                        # ArmAstInteger (unlikely):
                        if isinstance(oValue, ArmAstInteger):
                            if oField.fFixed != 0:
                                raise Exception('%s: Condition checks fixed field value: %s (%#x/%#x) %s %s'
                                                % (sInstrNm, oField.sName, oField.fValue, oField.fFixed,
                                                   oCondition.sOp, oValue.iValue,));
                            assert oField.fValue == 0;
                            if oValue.iValue.bit_length() > oField.cBitsWidth:
                                raise Exception('%s: Condition field value check too wide: %s is %u bits, test value is %s (%u bits)'
                                                % (sInstrNm, oField.sName, oField.cBitsWidth, oValue.iValue,
                                                   oValue.iValue.bit_count(),));
                            if oValue.iValue < 0:
                                raise Exception('%s: Condition field checks against negative value: %s, test value is %s'
                                                % (sInstrNm, oField.sName, oValue.iValue));
                            fFixed = (1 << oField.cBitsWidth) - 1;
                            if oCondition.sOp == '!=' and oField.cBitsWidth > 1:
                                dPendingNotEq[oField.sName] += [(oField, oValue.iValue, fFixed, oCondition)];
                                break;

                            print('debug transfer: %s: integer binaryop -> encoding!' % (sInstrNm,));
                            if oCondition.sOp == '==':
                                oField.fValue = oValue.iValue;
                            else:
                                oField.fValue = ~oValue.iValue & fFixed;
                            oField.fFixed = fFixed;
                            return (ArmAstBool(True), True);

                        # ArmAstValue.
                        assert isinstance(oValue, ArmAstValue);
                        (fValue, fFixed) = ArmEncodesetField.parseValue(oValue.sValue, oField.cBitsWidth);

                        if oCondition.sOp == '!=' and oField.cBitsWidth > 1 and (fFixed & (fFixed - 1)) != 0:
                            dPendingNotEq[oField.sName] += [(oField, fValue, fFixed, oCondition)];
                            break;
                        if fFixed & oField.fFixed:
                            raise Exception('%s: Condition checks fixed field value: %s (%#x/%#x) %s %s (%#x/%#x)'
                                            % (sInstrNm, oField.sName, oField.fValue, oField.fFixed, oCondition.sOp,
                                               oValue.sValue, fValue, fFixed));
                        print('debug transfer: %s: value binaryop -> encoding! %s %s %#x (fFixed=%#x)'
                              % (sInstrNm, oField.sName, oCondition.sOp, fValue, fFixed,));
                        if oCondition.sOp == '==':
                            oField.fValue |= fValue;
                        else:
                            oField.fValue |= ~fValue & fFixed;
                        oField.fFixed |= fFixed;
                        return (ArmAstBool(True), True);

    #
    # Deal with pending '!=' optimizations for fields larger than a single bit.
    # Currently we only deal with two bit fields.
    #
    if uDepth == 0 and dPendingNotEq:
        for sFieldNm, atOccurences in dPendingNotEq.items():
            # For a two bit field, we need at least two occurences to get any kind of fixed value.
            oField = atOccurences[0][0];
            if oField.cBitsWidth == 2 and len(atOccurences) >= 2:
                dValues = {};
                dFixed  = {};
                for oCurField, fValue, fFixed, _ in atOccurences:
                    assert oCurField is oField;
                    dValues[fValue] = 1;
                    dFixed[fFixed]  = 1;
                if len(dValues) in (2, 3) and len(dFixed) == 1 and 3 in dFixed:
                    afValues = list(dValues);
                    if len(dValues) == 2:
                        fFixed = 2 if (afValues[0] ^ afValues[1]) & 1 else 1; # One of the bits are fixed, the other ignored.
                    else:
                        fFixed = 3;                                           # Both bits are fixed.
                    fValue = afValues[0] & fFixed;
                    print('debug transfer: %s: %u binaryops -> encoding! %s != %#x/%#x'
                          % (sInstrNm, len(atOccurences), oField.sName, fValue, fFixed,));

                    # Remove the associated conditions (they'll be leaves).
                    aoToRemove = [oCondition for _, _, _, oCondition in atOccurences];
                    def recursiveRemove(oCondition):
                        if isinstance(oCondition, ArmAstBinaryOp):
                            if oCondition.sOp == '&&':
                                oCondition.oLeft  = recursiveRemove(oCondition.oLeft);
                                oCondition.oRight = recursiveRemove(oCondition.oLeft);
                                if isBoolTrue(oCondition.oLeft):    return oCondition.oRight;
                                if isBoolTrue(oCondition.oRight):   return oCondition.oLeft;
                            elif oCondition in aoToRemove:
                                assert isinstance(oCondition.oLeft, ArmAstIdentifier);
                                assert isinstance(oCondition.oRight, (ArmAstValue, ArmAstInteger));
                                assert oCondition.sOp == '!=';
                                return ArmAstBool(True);
                        return oCondition;
                    oCondition = recursiveRemove(oCondition);
                    fMod = True;
                else:
                    print('info: %s: transfer cond to enc failed for: %s dValues=%s dFixed=%s'
                          % (sInstrNm, oField.sName, dValues, dFixed));
            elif oField.cBitsWidth == 3 and len(atOccurences) >= 7:
                print('info: %s: TODO: transfer cond to enc for 3 bit field: %s (%s)' % (sInstrNm, oField.sName, atOccurences,));

    return (oCondition, fMod);


#
# Pass #2 - Assembly syntax formatting (for display purposes)
#
def asmSymbolsToDisplayText(adSymbols, ddAsmRules, oInstr):
    sText = '';
    for dSym in adSymbols:
        sType = dSym['_type'];
        if sType == 'Instruction.Symbols.Literal':
            sText += dSym['value'];
        elif sType == 'Instruction.Symbols.RuleReference':
            sRuleId = dSym['rule_id'];
            sText += asmRuleIdToDisplayText(sRuleId, ddAsmRules, oInstr);
        else:
            raise Exception('%s: Unknown assembly symbol type: %s' % (oInstr.sMnemonic, sType,));
    return sText;

def asmChoicesFilterOutDefaultAndAbsent(adChoices, ddAsmRules):
    # There are sometime a 'none' tail entry.
    if adChoices[-1] is None:
        adChoices = adChoices[:-1];
    if len(adChoices) > 1:
        # Typically, one of the choices is 'absent' or 'default', eliminate it before we start...
        for iChoice, dChoice in enumerate(adChoices):
            fAllAbsentOrDefault = True;
            for dSymbol in dChoice['symbols']:
                if dSymbol['_type'] != 'Instruction.Symbols.RuleReference':
                    fAllAbsentOrDefault = False;
                    break;
                sRuleId = dSymbol['rule_id'];
                oRule = ddAsmRules[sRuleId];
                if (   ('display' in oRule and oRule['display'])
                    or ('symbols' in oRule and oRule['symbols'])):
                    fAllAbsentOrDefault = False;
                    break;
            if fAllAbsentOrDefault:
                return adChoices[:iChoice] + adChoices[iChoice + 1:];
    return adChoices;

def asmRuleIdToDisplayText(sRuleId, ddAsmRules, oInstr):
    dRule = ddAsmRules[sRuleId];
    sRuleType = dRule['_type'];
    if sRuleType == 'Instruction.Rules.Token':
        assert dRule['default'], '%s: %s' % (oInstr.sMnemonic, sRuleId);
        return dRule['default'];
    if sRuleType == 'Instruction.Rules.Rule':
        assert dRule['display'], '%s: %s' % (oInstr.sMnemonic, sRuleId);
        return dRule['display'];
    if sRuleType == 'Instruction.Rules.Choice':
        # Some of these has display = None and we need to sort it out ourselves.
        if dRule['display']:
            return dRule['display'];
        sText = '{';
        assert len(dRule['choices']) > 1;
        for iChoice, dChoice in enumerate(asmChoicesFilterOutDefaultAndAbsent(dRule['choices'], ddAsmRules)):
            if iChoice > 0:
                sText += ' | ';
            sText += asmSymbolsToDisplayText(dChoice['symbols'], ddAsmRules, oInstr);
        sText += '}';

        # Cache it.
        dRule['display'] = sText;
        return sText;

    raise Exception('%s: Unknown assembly rule type: %s for %s' % (oInstr.sMnemonic, sRuleType, sRuleId));

def parseInstructionsPass2(aoInstructions, ddAsmRules):
    """
    Uses the assembly rules to construct some assembly syntax string for each
    instruction in the array.
    """
    for oInstr in aoInstructions:
        if 'assembly' in oInstr.oJson:
            oAsm = oInstr.oJson['assembly'];
            assert oAsm['_type'] == 'Instruction.Assembly';
            assert 'symbols' in oAsm;
            oInstr.sAsmDisplay = asmSymbolsToDisplayText(oAsm['symbols'], ddAsmRules, oInstr);
        else:
            oInstr.sAsmDisplay = oInstr.sMnemonic;
    return True;

def LoadArmOpenSourceSpecification(oOptions):
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
    # Pass #1: Collect the instructions.
    parseInstructions([], dRawInstructions['instructions']);
    # Pass #2: Assembly syntax.
    global g_aoAllArmInstructions;
    parseInstructionsPass2(g_aoAllArmInstructions, dRawInstructions['assembly_rules']);

    # Sort the instruction array by name.
    g_aoAllArmInstructions = sorted(g_aoAllArmInstructions, key = operator.attrgetter('sName', 'sAsmDisplay'));

    print("Found %u instructions." % (len(g_aoAllArmInstructions),));
    #oBrk = g_dAllArmInstructionsByName['BRK_EX_exception'];
    #print("oBrk=%s" % (oBrk,))

    if True:
        for oInstr in g_aoAllArmInstructions:
            print('%08x/%08x %s %s' % (oInstr.fFixedMask, oInstr.fFixedValue, oInstr.getCName(), oInstr.sAsmDisplay));

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

    return True;


#
# Decoder structure helpers.
#

class MaskIterator(object):
    """ Helper class for DecoderNode.constructNextLevel(). """

    ## Maximum number of mask sub-parts.
    # Lower number means fewer instructions required to convert it into an index.
    kcMaxMaskParts = 3

    def __init__(self, fMask, cMaxTableSizeInBits, dDictDoneAlready):
        self.fMask = fMask;
        self.afVariations = self.variationsForMask(fMask, cMaxTableSizeInBits, dDictDoneAlready);

    def __iter__(self):
        ## @todo make this more dynamic...
        return iter(self.afVariations);

    @staticmethod
    def variationsForMask(fMask, cMaxTableSizeInBits, dDictDoneAlready):
        dBits = collections.OrderedDict();
        for iBit in range(32):
            if fMask & (1 << iBit):
                dBits[iBit] = 1;

        if len(dBits) > cMaxTableSizeInBits or fMask in dDictDoneAlready:
            aiRet = [];
        elif len(dBits) > 0:
            aaiMaskAlgo = DecoderNode.compactMaskAsList(list(dBits));
            if len(aaiMaskAlgo) <= MaskIterator.kcMaxMaskParts:
                dDictDoneAlready[fMask] = 1;
                aiRet = [(fMask, list(dBits), aaiMaskAlgo)];
            else:
                aiRet = [];
        else:
            return [];

        def recursive(fMask, dBits):
            if len(dBits) > 0 and fMask not in dDictDoneAlready:
                if len(dBits) <= cMaxTableSizeInBits:
                    aaiMaskAlgo = DecoderNode.compactMaskAsList(list(dBits));
                    if len(aaiMaskAlgo) <= MaskIterator.kcMaxMaskParts:
                        dDictDoneAlready[fMask] = 1;
                        aiRet.append((fMask, list(dBits), aaiMaskAlgo));
                if len(dBits) > 1:
                    dChildBits = collections.OrderedDict(dBits);
                    for iBit in dBits.keys():
                        del dChildBits[iBit];
                        recursive(fMask & ~(1 << iBit), dChildBits)

        if len(dBits) > 1:
            dChildBits = collections.OrderedDict(dBits);
            for iBit in dBits.keys():
                del dChildBits[iBit];
                recursive(fMask & ~(1 << iBit), dChildBits)

        print("debug: fMask=%#x len(aiRet)=%d" % (fMask, len(aiRet),));
        return aiRet;

class DecoderNode(object):

    ## The absolute maximum table size in bits index by the log2 of the instruction count.
    kacMaxTableSizesInBits = (
        2,      # [2^0 =     1] =>     4
        4,      # [2^1 =     2] =>    16
        5,      # [2^2 =     4] =>    32
        6,      # [2^3 =     8] =>    64
        7,      # [2^4 =    16] =>   128
        7,      # [2^5 =    32] =>   128
        8,      # [2^6 =    64] =>   256
        9,      # [2^7 =   128] =>   512
        10,     # [2^8 =   256] =>  1024
        11,     # [2^9 =   512] =>  2048
        12,     # [2^10 = 1024] =>  4096
        13,     # [2^11 = 2048] =>  8192
        14,     # [2^12 = 4096] => 16384
        14,     # [2^13 = 8192] => 16384
        15,     # [2^14 =16384] => 32768
    );

    def __init__(self, aoInstructions: list[ArmInstruction], fCheckedMask: int, fCheckedValue: int, uDepth: int):
        assert (~fCheckedMask & fCheckedValue) == 0;
        for idxInstr, oInstr in enumerate(aoInstructions):
            assert ((oInstr.fFixedValue ^ fCheckedValue) & fCheckedMask & oInstr.fFixedMask) == 0, \
                    '%s: fFixedValue=%#x fFixedMask=%#x fCheckedValue=%#x fCheckedMask=%#x -> %#x\n %s' \
                    % (idxInstr, oInstr.fFixedValue, oInstr.fFixedMask, fCheckedValue, fCheckedMask,
                       (oInstr.fFixedValue ^ fCheckedValue) & fCheckedMask & oInstr.fFixedMask,
                       '\n '.join(['%s: %#010x/%#010x %s' % (i, oInstr2.fFixedValue, oInstr2.fFixedMask, oInstr2.sName)
                                     for i, oInstr2 in enumerate(aoInstructions[:idxInstr+2])]));

        self.aoInstructions     = aoInstructions;   ##< The instructions at this level.
        self.fCheckedMask       = fCheckedMask;     ##< The opcode bit mask covered thus far.
        self.fCheckedValue      = fCheckedValue;    ##< The value that goes with fCheckedMask.
        self.uDepth             = uDepth;           ##< The current node depth.
        self.uCost              = 0;                ##< The cost at this level.
        self.fLeafCheckNeeded   = len(aoInstructions) == 1 and (aoInstructions[0].fFixedMask & ~self.fCheckedMask) != 0;
        self.fChildMask         = 0;                ##< The mask used to separate the children.
        self.aoChildren         = [];               ##< Children, populated by constructNextLevel().

    @staticmethod
    def compactMask(fMask):
        """
        Returns an with instructions for extracting the bits from the mask into
        a compacted form. Each array entry is an array/tuple of source bit [0],
        destination bit [1], and bit counts [2].
        """
        aaiAlgo   = [];
        iSrcBit   = 0;
        iDstBit   = 0;
        while fMask > 0:
            if fMask & 1:
                cCount = 1
                fMask >>= 1;
                while fMask & 1:
                    fMask >>= 1;
                    cCount += 1
                aaiAlgo.append([iSrcBit, iDstBit, cCount])
                iSrcBit   += cCount;
                iDstBit   += cCount;
            else:
                iSrcBit += 1;
        return aaiAlgo;

    @staticmethod
    def compactMaskAsList(dOrderedDict):
        """
        Returns an with instructions for extracting the bits from the mask into
        a compacted form. Each array entry is an array/tuple of source bit [0],
        destination bit [1], and mask (shifted to pos 0) [2].
        """
        aaiAlgo = [];
        iDstBit = 0;
        i       = 0;
        while i < len(dOrderedDict):
            iSrcBit = dOrderedDict[i];
            cCount  = 1;
            i      += 1;
            while i < len(dOrderedDict) and dOrderedDict[i] == iSrcBit + cCount:
                cCount += 1;
                i      += 1;
            aaiAlgo.append([iSrcBit, iDstBit, (1 << cCount) - 1])
            iDstBit += cCount;
        return aaiAlgo;

    @staticmethod
    def compactDictAlgoToLambda(aaiAlgo):
        assert(aaiAlgo)
        sBody = '';
        for iSrcBit, iDstBit, fMask in aaiAlgo:
            if sBody:
                sBody += ' | ';
            assert iSrcBit >= iDstBit;
            if iDstBit == 0:
                if iSrcBit == 0:
                    sBody += '(uValue & %#x)' % (fMask,);
                else:
                    sBody += '((uValue >> %u) & %#x)' % (iSrcBit, fMask);
            else:
                sBody += '((uValue >> %u) & %#x)' % (iSrcBit - iDstBit, fMask << iDstBit);
        return eval('lambda uValue: ' + sBody);

    @staticmethod
    def compactDictAlgoToLambdaRev(aaiAlgo):
        assert(aaiAlgo)
        sBody = '';
        for iSrcBit, iDstBit, fMask in aaiAlgo:
            if sBody:
                sBody += ' | ';
            if iDstBit == 0:
                if iSrcBit == 0:
                    sBody += '(uIdx & %#x)' % (fMask,);
                else:
                    sBody += '((uIdx & %#x) << %u)' % (fMask, iSrcBit);
            else:
                sBody += '((uIdx << %u) & %#x)' % (iSrcBit - iDstBit, fMask << iSrcBit);
        return eval('lambda uIdx: ' + sBody);

    @staticmethod
    def toIndexByMask(uValue, aaiAlgo):
        idxRet = 0;
        for iSrcBit, iDstBit, fMask in aaiAlgo:
            idxRet |= ((uValue >> iSrcBit) & fMask) << iDstBit;
        return idxRet;

    @staticmethod
    def popCount(uValue):
        cBits = 0;
        while uValue:
            cBits += 1;
            uValue &= uValue - 1;
        return cBits;

    def constructNextLevel(self):
        """
        Recursively constructs the
        """
        # Special case: leaf.
        if len(self.aoInstructions) <= 1:
            assert len(self.aoChildren) == 0;
            return 16 if self.fLeafCheckNeeded else 0;

        # Do an inventory of the fixed masks used by the instructions.
        dMaskCounts = collections.Counter();
        for oInstr in self.aoInstructions:
            dMaskCounts[oInstr.fFixedMask & ~self.fCheckedMask] += 1;
        assert 0 not in dMaskCounts or dMaskCounts[0] <= 1, \
                'dMaskCounts=%s len(self.aoInstructions)=%s\n%s' % (dMaskCounts, len(self.aoInstructions),self.aoInstructions);

        # Determine the max table size for the number of instructions we have.
        cInstructionsAsShift = 1;
        while (1 << cInstructionsAsShift) < len(self.aoInstructions):
            cInstructionsAsShift += 1;
        #cMaxTableSizeInBits = self.kacMaxTableSizesInBits[cInstructionsAsShift];

        # Work thru the possible masks and test out the possible variations (brute force style).
        uCostBest        = 0x7fffffffffffffff;
        fChildrenBest    = 0;
        aoChildrenBest   = None;
        dDictDoneAlready = {}
        for fOrgMask, cOccurences in dMaskCounts.most_common(8):
            cOccurencesAsShift = 1;
            while (1 << cOccurencesAsShift) < cOccurences:
                cOccurencesAsShift += 1;
            cMaxTableSizeInBits = self.kacMaxTableSizesInBits[cOccurencesAsShift]; # Not quite sure about this...
            print('debug: %#010x (%u) - %u instructions - max tab size %u'
                  % (fOrgMask, self.popCount(fOrgMask), cOccurences, cMaxTableSizeInBits,));

            # Skip pointless stuff.
            if cOccurences >= 2 and fOrgMask > 0 and fOrgMask != 0xffffffff:

                # Brute force all the mask variations (minus those which are too wide).
                for fMask, dOrderedDictMask, aaiMaskToIdxAlgo in MaskIterator(fOrgMask, cMaxTableSizeInBits, dDictDoneAlready):
                    print('debug: >>> fMask=%#010x...' % (fMask,));
                    assert len(dOrderedDictMask) <= cMaxTableSizeInBits;
                    fnToIndex   = self.compactDictAlgoToLambda(aaiMaskToIdxAlgo);
                    fnFromIndex = self.compactDictAlgoToLambdaRev(aaiMaskToIdxAlgo);

                    aaoTmp = [];
                    for i in range(1 << len(dOrderedDictMask)):
                        aaoTmp.append(list());

                    for oInstr in self.aoInstructions:
                        idx = fnToIndex(oInstr.fFixedValue);
                        #idx = self.toIndexByMask(oInstr.fFixedValue, aaiMaskToIdxAlgo)
                        assert idx == self.toIndexByMask(oInstr.fFixedValue & fMask, aaiMaskToIdxAlgo);
                        print('debug: %#010x -> %#05x %s' % (oInstr.fFixedValue, idx, oInstr.sName));
                        aoList = aaoTmp[idx];
                        aoList.append(oInstr);

                    aoChildrenTmp = [];
                    uCostTmp      = 0;
                    for idx, aoInstrs in enumerate(aaoTmp):
                        oChild = DecoderNode(aoInstrs,
                                             self.fCheckedMask  | fMask,
                                             self.fCheckedValue | fnFromIndex(idx),
                                             self.uDepth + 1);
                        aoChildrenTmp.append(oChild);
                        uCostTmp += oChild.constructNextLevel();

                    if uCostTmp < uCostBest:
                        uCostBest      = uCostTmp;
                        fChildrenBest  = fMask;
                        aoChildrenBest = aoChildrenTmp;

        if aoChildrenBest is None:
            pass; ## @todo
        return uCostBest;









#
# Generators
#

class IEMArmGenerator(object):

    def __init__(self):
        self.oDecoderRoot = None;


    def constructDecoder(self):
        """
        Creates the decoder to the best our abilities.
        """
        global g_aoAllArmInstructions;
        self.oDecoderRoot = DecoderNode(g_aoAllArmInstructions, 0, 0, 0);
        self.oDecoderRoot.constructNextLevel();


    def generateLicenseHeader(self):
        """
        Returns the lines for a license header.
        """
        return [
            '/*',
            ' * Autogenerated by $Id$ ',
            ' * Do not edit!',
            ' */',
            '',
            '/*',
            ' * Copyright (C) 2025-' + str(datetime.date.today().year) + ' Oracle and/or its affiliates.',
            ' *',
            ' * This file is part of VirtualBox base platform packages, as',
            ' * available from https://www.virtualbox.org.',
            ' *',
            ' * This program is free software; you can redistribute it and/or',
            ' * modify it under the terms of the GNU General Public License',
            ' * as published by the Free Software Foundation, in version 3 of the',
            ' * License.',
            ' *',
            ' * This program is distributed in the hope that it will be useful, but',
            ' * WITHOUT ANY WARRANTY; without even the implied warranty of',
            ' * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU',
            ' * General Public License for more details.',
            ' *',
            ' * You should have received a copy of the GNU General Public License',
            ' * along with this program; if not, see <https://www.gnu.org/licenses>.',
            ' *',
            ' * The contents of this file may alternatively be used under the terms',
            ' * of the Common Development and Distribution License Version 1.0',
            ' * (CDDL), a copy of it is provided in the "COPYING.CDDL" file included',
            ' * in the VirtualBox distribution, in which case the provisions of the',
            ' * CDDL are applicable instead of those of the GPL.',
            ' *',
            ' * You may elect to license modified versions of this file under the',
            ' * terms and conditions of either the GPL or the CDDL or both.',
            ' *',
            ' * SPDX-License-Identifier: GPL-3.0-only OR CDDL-1.0',
            ' */',
            '',
            '',
            '',
        ];

    def generateImplementationStubs(self):
        """
        Generate implementation stubs.
        """
        return [];


    def generateDecoderFunctions(self):
        """
        Generates the leaf decoder functions.
        """
        asLines = [];
        for oInstr in g_aoAllArmInstructions:
            sCName = oInstr.getCName();
            asLines.extend([
                '',
                '/* %08x/%08x: %s */' % (oInstr.fFixedMask, oInstr.fFixedValue, oInstr.sAsmDisplay,),
                'FNIEMOP_DEF_1(iemDecode_%s, uint32_t, uOpcode)' % (sCName,),
                '{',
            ]);

            # The final decoding step, if needed.
            if oInstr.fDecoderLeafCheckNeeded:
                asLines.extend([
                    '    if ((uOpcode & %#x) == %#x) { /* likely */ }' % (oInstr.fFixedMask, oInstr.fFixedValue,),
                    '    else',
                    '    {',
                    '        LogFlow(("Invalid instruction %%#x at %%x\n", uOpcode, pVCpu->cpum.GstCtx.Pc.u64));',
                    '        return IEMOP_RAISE_INVALID_OPCODE_RET();',
                    '    }',
                ]);

            # Decode the fields and prepare for passing them as arguments.
            asArgs  = [];
            sLogFmt = '';
            ## @todo Most of this should be done kept in the instruction.

            asLines.extend([
                '    LogFlow(("%s%s\\n"%s));' % (sCName, sLogFmt, ', '.join(asArgs),),
                '#ifdef HAS_IMPL_%s' % (sCName,),
                '    return iemImpl_%s(%s);' % (sCName, ', '.join(asArgs),),
                '#else',
                '    RT_NOREF(%s);' % (', '.join(asArgs) if asArgs else 'uOpcode') ,
                '    return VERR_IEM_INSTR_NOT_IMPLEMENTED;',
                '#endif',
                '}',
            ]);
        return asLines;


    def generateDecoderCpp(self, iPartNo):
        """ Generates the decoder data & code. """
        _ = iPartNo;
        asLines = self.generateLicenseHeader();
        asLines.extend([
            '#define LOG_GROUP LOG_GROUP_IEM',
            '#define VMCPU_INCL_CPUM_GST_CTX',
            '#include "IEMInternal.h"',
            '#include "vm.h"',
            '',
            '#include "iprt/armv8.h"',
            '',
            '',
        ]);


        asLines += self.generateDecoderFunctions();

        return (True, asLines);


    def main(self, asArgs):
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
        oArgParser.add_argument('--out-decoder',
                                metavar = 'file-decoder.cpp',
                                dest    = 'sFileDecoderCpp',
                                action  = 'store',
                                default = '-',
                                help    = 'The output C++ file for the decoder.');
        oOptions = oArgParser.parse_args(asArgs[1:]);

        #
        # Load the specification.
        #
        if LoadArmOpenSourceSpecification(oOptions):
            #
            # Sort out the decoding.
            #
            self.constructDecoder();

            #
            # Output.
            #
            aaoOutputFiles = [
                 ( oOptions.sFileDecoderCpp,      self. generateDecoderCpp, 0, ),
            ];
            fRc = True;
            for sOutFile, fnGenMethod, iPartNo in aaoOutputFiles:
                if sOutFile == '-':
                    oOut = sys.stdout;
                else:
                    try:
                        oOut = open(sOutFile, 'w');                 # pylint: disable=consider-using-with,unspecified-encoding
                    except Exception as oXcpt:
                        print('error! Failed open "%s" for writing: %s' % (sOutFile, oXcpt,), file = sys.stderr);
                        return 1;

                (fRc2, asLines) = fnGenMethod(iPartNo);
                fRc = fRc2 and fRc;

                oOut.write('\n'.join(asLines));
                if oOut != sys.stdout:
                    oOut.close();
            if fRc:
                return 0;

        return 1;


if __name__ == '__main__':
    try:
        sys.exit(IEMArmGenerator().main(sys.argv));
    except Exception as oXcpt:
        print('Exception Caught!', flush = True);
        cMaxLines = 1;
        try:    cchMaxLen = os.get_terminal_size()[0] * cMaxLines;
        except: cchMaxLen = 80 * cMaxLines;
        cchMaxLen -= len('     =  ...');

        oTB = traceback.TracebackException.from_exception(oXcpt, limit = None, capture_locals = True);
        # No locals for the outer frame.
        oTB.stack[0].locals = {};
        # Suppress insanely long variable values.
        for oFrameSummary in oTB.stack:
            if oFrameSummary.locals:
                #for sToDelete in ['ddAsmRules', 'aoInstructions',]:
                #    if sToDelete in oFrameSummary.locals:
                #        del oFrameSummary.locals[sToDelete];
                for sKey, sValue in oFrameSummary.locals.items():
                    if len(sValue) > cchMaxLen - len(sKey):
                        sValue = sValue[:cchMaxLen - len(sKey)] + ' ...';
                    if '\n' in sValue:
                        sValue = sValue.split('\n')[0] + ' ...';
                    oFrameSummary.locals[sKey] = sValue;
        oTB.print();

