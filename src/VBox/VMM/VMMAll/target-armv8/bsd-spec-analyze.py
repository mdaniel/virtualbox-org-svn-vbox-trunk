#!/usr/bin/env python
# -*- coding: utf-8 -*-
# $Id$
# pylint: disable=invalid-name

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
import collections;
import datetime;
import io;
import json;
import operator;
import os;
import re;
import sys;
import tarfile;
import traceback;
# profiling:
import cProfile;
import pstats


#
# The ARM instruction AST stuff.
#

class ArmAstBase(object):
    """
    ARM instruction AST base class.
    """

    ksTypeBinaryOp   = 'AST.BinaryOp';
    ksTypeBool       = 'AST.Bool';
    ksTypeConcat     = 'AST.Concat';
    ksTypeFunction   = 'AST.Function';
    ksTypeIdentifier = 'AST.Identifier';
    ksTypeInteger    = 'AST.Integer';
    ksTypeSet        = 'AST.Set';
    ksTypeSquareOp   = 'AST.SquareOp';
    ksTypeUnaryOp    = 'AST.UnaryOp';
    ksTypeValue      = 'Values.Value';

    def __init__(self, sType):
        self.sType = sType;

    @staticmethod
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
        ksTypeBinaryOp:     fromJsonBinaryOp,
        ksTypeUnaryOp:      fromJsonUnaryOp,
        ksTypeSquareOp:     fromJsonSquareOp,
        ksTypeConcat:       fromJsonConcat,
        ksTypeFunction:     fromJsonFunction,
        ksTypeIdentifier:   fromJsonIdentifier,
        ksTypeBool:         fromJsonBool,
        ksTypeInteger:      fromJsonInteger,
        ksTypeSet:          fromJsonSet,
        ksTypeValue:        fromJsonValue,
    };

    @staticmethod
    def fromJson(oJson):
        """ Decodes an AST/Values expression. """
        #print('debug ast: %s' % oJson['_type'])
        return ArmAstBase.kfnTypeMap[oJson['_type']](oJson);

    def isBoolAndTrue(self):
        """ Check if this is a boolean with the value True. """
        #return isinstance(self, ArmAstBool) and self.fValue is True;
        if isinstance(self, ArmAstBool):
            return self.fValue is True;
        return False;


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
        ArmAstBase.__init__(self, ArmAstBase.ksTypeBinaryOp);
        assert sOp in ArmAstBinaryOp.kdOps, 'sOp="%s"' % (sOp,);
        self.oLeft  = oLeft;
        self.sOp    = sOp;
        self.oRight = oRight;

        # Switch value == field non-sense (simplifies transferConditionsToEncoding and such):
        if (    isinstance(oRight, ArmAstIdentifier)
            and isinstance(oLeft, (ArmAstValue, ArmAstInteger))
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
        ArmAstBase.__init__(self, ArmAstBase.ksTypeUnaryOp);
        assert sOp in ArmAstUnaryOp.kdOps, 'sOp=%s' % (sOp,);
        self.sOp   = sOp;
        self.oExpr = oExpr;

    def toString(self):
        if ArmAstBinaryOp.needParentheses(self.oExpr):
            return '%s(%s)' % (self.sOp, self.oExpr.toString(),);
        return '%s%s' % (self.sOp, self.oExpr.toString(),);

class ArmAstSquareOp(ArmAstBase):
    def __init__(self, oVar, aoValues):
        ArmAstBase.__init__(self, ArmAstBase.ksTypeSquareOp);
        self.oVar     = oVar;
        self.aoValues = aoValues;

    def toString(self):
        return '%s<%s>' % (self.oVar.toString(), ','.join([oValue.toString() for oValue in self.aoValues]),);


class ArmAstConcat(ArmAstBase):
    def __init__(self, aoValues):
        ArmAstBase.__init__(self, ArmAstBase.ksTypeConcat);
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
        ArmAstBase.__init__(self, ArmAstBase.ksTypeFunction);
        assert self.s_oReValidName.match(sName), 'sName=%s' % (sName);
        self.sName  = sName;
        self.aoArgs = aoArgs;

    def toString(self):
        return '%s(%s)' % (self.sName, ','.join([oArg.toString() for oArg in self.aoArgs]),);

class ArmAstIdentifier(ArmAstBase):
    s_oReValidName = re.compile('^[_A-Za-z][_A-Za-z0-9]*$');

    def __init__(self, sName):
        ArmAstBase.__init__(self, ArmAstBase.ksTypeIdentifier);
        assert self.s_oReValidName.match(sName), 'sName=%s' % (sName);
        self.sName = sName;

    def toString(self):
        return self.sName;

class ArmAstBool(ArmAstBase):
    def __init__(self, fValue):
        ArmAstBase.__init__(self, ArmAstBase.ksTypeBool);
        assert fValue is True or fValue is False, '%s' % (fValue,);
        self.fValue = fValue;

    def toString(self):
        return 'true' if self.fValue is True else 'false';


class ArmAstInteger(ArmAstBase):
    def __init__(self, iValue):
        ArmAstBase.__init__(self, ArmAstBase.ksTypeInteger);
        self.iValue = int(iValue);

    def toString(self):
        return '%#x' % (self.iValue,);


class ArmAstSet(ArmAstBase):
    def __init__(self, aoValues):
        ArmAstBase.__init__(self, ArmAstBase.ksTypeSet);
        self.aoValues = aoValues;

    def toString(self):
        return '(%s)' % (', '.join([oValue.toString() for oValue in self.aoValues]),);


class ArmAstValue(ArmAstBase):
    def __init__(self, sValue):
        ArmAstBase.__init__(self, ArmAstBase.ksTypeValue);
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
            #sCondBefore = oCondition.toString();
            #print('debug transfer: %s: org:  %s' % (sInstrNm, sCondBefore));
            (oCondition, fMod) = transferConditionsToEncoding(oCondition, aoEncodesets, collections.defaultdict(list), sInstrNm);
            #if fMod:
            #    print('debug transfer: %s: %s  ---->  %s' % (sInstrNm, sCondBefore, oCondition.toString()));
            _ = fMod;

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
    if isinstance(oCondition, ArmAstBinaryOp):
        if oCondition.sOp == '&&':
            # Recurse into each side of an AND expression.
            #print('debug transfer: %s: recursion...' % (sInstrNm,));
            (oCondition.oLeft, fMod)  = transferConditionsToEncoding(oCondition.oLeft,  aoEncodesets, dPendingNotEq,
                                                                     sInstrNm, uDepth + 1, fMod);
            (oCondition.oRight, fMod) = transferConditionsToEncoding(oCondition.oRight, aoEncodesets, dPendingNotEq,
                                                                     sInstrNm, uDepth + 1, fMod);
            if oCondition.oLeft.isBoolAndTrue():
                return (oCondition.oRight, fMod);
            if oCondition.oRight.isBoolAndTrue():
                return (oCondition.oLeft, fMod);

        elif oCondition.sOp in ('==', '!='):
            # The pattern we're looking for is identifier (field) == fixed value.
            #print('debug transfer: %s: binaryop %s vs %s ...' % (sInstrNm, oCondition.oLeft.sType, oCondition.oRight.sType));
            if (    isinstance(oCondition.oLeft, ArmAstIdentifier)
                and isinstance(oCondition.oRight, (ArmAstValue, ArmAstInteger))):
                sFieldName = oCondition.oLeft.sName;
                oValue     = oCondition.oRight;
                #print('debug transfer: %s: binaryop step 2...' % (sInstrNm,));
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
                                raise Exception('%s: Condition field value check too wide: %s is %u bits, test value %s (%u bits)'
                                                % (sInstrNm, oField.sName, oField.cBitsWidth, oValue.iValue,
                                                   oValue.iValue.bit_count(),));
                            if oValue.iValue < 0:
                                raise Exception('%s: Condition field checks against negative value: %s, test value is %s'
                                                % (sInstrNm, oField.sName, oValue.iValue));
                            fFixed = (1 << oField.cBitsWidth) - 1;
                            if oCondition.sOp == '!=' and oField.cBitsWidth > 1:
                                dPendingNotEq[oField.sName] += [(oField, oValue.iValue, fFixed, oCondition)];
                                break;

                            #print('debug transfer: %s: integer binaryop -> encoding: %s %s %#x/%#x'
                            #      % (sInstrNm, oField.sName, oCondition.sOp, oValue.iValue, fFixed));
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
                        #print('debug transfer: %s: value binaryop -> encoding: %s %s %#x (fFixed=%#x)'
                        #      % (sInstrNm, oField.sName, oCondition.sOp, fValue, fFixed,));
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
        def recursiveRemove(oCondition, aoToRemove):
            if isinstance(oCondition, ArmAstBinaryOp):
                if oCondition.sOp == '&&':
                    oCondition.oLeft  = recursiveRemove(oCondition.oLeft, aoToRemove);
                    oCondition.oRight = recursiveRemove(oCondition.oLeft, aoToRemove);
                    if oCondition.oLeft.isBoolAndTrue():    return oCondition.oRight;
                    if oCondition.oRight.isBoolAndTrue():   return oCondition.oLeft;
                elif oCondition in aoToRemove:
                    assert isinstance(oCondition.oLeft, ArmAstIdentifier);
                    assert isinstance(oCondition.oRight, (ArmAstValue, ArmAstInteger));
                    assert oCondition.sOp == '!=';
                    return ArmAstBool(True);
            return oCondition;

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
                    print('debug transfer: %s: %u binaryops -> encoding: %s == %#x/%#x'
                          % (sInstrNm, len(atOccurences), sFieldNm, ~fValue & fFixed, fFixed,));
                    oField.fValue |= ~fValue & fFixed;
                    oField.fFixed |= fFixed;


                    # Remove the associated conditions (they'll be leaves).
                    oCondition = recursiveRemove(oCondition, [oCondition for _, _, _, oCondition in atOccurences]);
                    fMod = True;
                else:
                    print('info: %s: transfer cond to enc failed for: %s dValues=%s dFixed=%s'
                          % (sInstrNm, sFieldNm, dValues, dFixed));
            elif oField.cBitsWidth == 3 and len(atOccurences) >= 7:
                print('info: %s: TODO: transfer cond to enc for 3 bit field: %s (%s)' % (sInstrNm, sFieldNm, atOccurences,));

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

    if oOptions.fPrintInstructions:
        for oInstr in g_aoAllArmInstructions:
            print('%08x/%08x %s %s' % (oInstr.fFixedMask, oInstr.fFixedValue, oInstr.getCName(), oInstr.sAsmDisplay));

    # Gather stats on fixed bits:
    if oOptions.fPrintFixedMaskStats:
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
    if oOptions.fPrintFixedMaskTop10:
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

class MaskZipper(object):
    """
    This is mainly a class for putting static methods relating to mask
    packing and unpack.
    """

    def __init__(self):
        pass;

    @staticmethod
    def compileAlgo(fMask):
        """
        Returns an with instructions for extracting the bits from the mask into
        a compacted form. Each array entry is an array/tuple of source bit [0],
        destination bit [1], and bit counts [2].
        """
        aaiAlgo   = [];
        iSrcBit   = 0;
        iDstBit   = 0;
        while fMask > 0:
            # Skip leading zeros.
            cSkip    = (fMask & -fMask).bit_length() - 1;
            #assert (fMask & ((1 << cSkip) - 1)) == 0 and ((fMask >> cSkip) & 1), 'fMask=%#x cSkip=%d' % (fMask, cSkip)
            iSrcBit += cSkip;
            fMask  >>= cSkip;

            # Calculate leading ones the same way.
            cCount = (~fMask & -~fMask).bit_length() - 1;
            #assert (fMask & ((1 << cCount) - 1)) == ((1 << cCount) - 1) and (fMask & (1 << cCount)) == 0

            # Append to algo list.
            aaiAlgo.append((iSrcBit, iDstBit, (1 << cCount) - 1));

            # Advance.
            iDstBit += cCount;
            iSrcBit += cCount;
            fMask  >>= cCount;
        return aaiAlgo;

    @staticmethod
    def compileAlgoLimited(fMask):
        """
        Version of compileAlgo that returns an empty list if there are
        more than three sections.
        """
        assert fMask;

        #
        # Chunk 0:
        #

        # Skip leading zeros.
        iSrcBit0 = (fMask & -fMask).bit_length() - 1;
        fMask  >>= iSrcBit0;
        # Calculate leading ones the same way.
        cCount0  = (~fMask & -~fMask).bit_length() - 1;
        fMask  >>= cCount0;
        if not fMask:
            return [(iSrcBit0, 0, (1 << cCount0) - 1)];

        #
        # Chunk 1:
        #

        # Skip leading zeros.
        cSrcGap1 = (fMask & -fMask).bit_length() - 1;
        fMask  >>= cSrcGap1;
        # Calculate leading ones the same way.
        cCount1  = (~fMask & -~fMask).bit_length() - 1;
        fMask  >>= cCount1;
        if not fMask:
            return [ (iSrcBit0, 0, (1 << cCount0) - 1),
                     (iSrcBit0 + cCount0 + cSrcGap1, cCount0, (1 << cCount1) - 1)];

        #
        # Chunk 2:
        #

        # Skip leading zeros.
        cSrcGap2 = (fMask & -fMask).bit_length() - 1;
        fMask  >>= cSrcGap2;
        # Calculate leading ones the same way.
        cCount2  = (~fMask & -~fMask).bit_length() - 1;
        fMask  >>= cCount2;
        if not fMask:
            iSrcBit1 = iSrcBit0 + cCount0 + cSrcGap1;
            return [ (iSrcBit0, 0, (1 << cCount0) - 1),
                     (iSrcBit1, cCount0, (1 << cCount1) - 1),
                     (iSrcBit1 + cCount1 + cSrcGap2, cCount0 + cCount1, (1 << cCount2) - 1), ];

        # Too many fragments.
        return [];

    @staticmethod
    def compileAlgoFromList(aiOrderedBits):
        """
        Returns an with instructions for extracting the bits from the mask into
        a compacted form. Each array entry is an array/tuple of source bit [0],
        destination bit [1], and mask (shifted to pos 0) [2].
        """
        aaiAlgo = [];
        iDstBit = 0;
        i       = 0;
        while i < len(aiOrderedBits):
            iSrcBit = aiOrderedBits[i];
            cCount  = 1;
            i      += 1;
            while i < len(aiOrderedBits) and aiOrderedBits[i] == iSrcBit + cCount:
                cCount += 1;
                i      += 1;
            aaiAlgo.append([iSrcBit, iDstBit, (1 << cCount) - 1])
            iDstBit += cCount;
        return aaiAlgo;

    @staticmethod
    def algoToBitList(aaiAlgo):
        aiRet = [];
        for iSrcBit, _, fMask in aaiAlgo:
            cCount = fMask.bit_count();
            aiRet += [iSrcBit + i for i in range(cCount)];
        return aiRet;

    @staticmethod
    def zipMask(uValue, aaiAlgo):
        idxRet = 0;
        for iSrcBit, iDstBit, fMask in aaiAlgo:
            idxRet |= ((uValue >> iSrcBit) & fMask) << iDstBit;
        return idxRet;

    @staticmethod
    def __zipMask1(uValue, aaiAlgo):
        iSrcBit, _, fMask = aaiAlgo[0];
        return (uValue >> iSrcBit) & fMask;

    @staticmethod
    def __zipMask2(uValue, aaiAlgo):
        iSrcBit0, _,        fMask0 = aaiAlgo[0];
        iSrcBit1, iDstBit1, fMask1 = aaiAlgo[1];
        return ((uValue >> iSrcBit0) & fMask0) | (((uValue >> iSrcBit1) & fMask1) << iDstBit1);

    @staticmethod
    def __zipMask3(uValue, aaiAlgo):
        iSrcBit0, _,        fMask0 = aaiAlgo[0];
        iSrcBit1, iDstBit1, fMask1 = aaiAlgo[1];
        iSrcBit2, iDstBit2, fMask2 = aaiAlgo[2];
        return ((uValue >> iSrcBit0) & fMask0) \
             | (((uValue >> iSrcBit1) & fMask1) << iDstBit1) \
             | (((uValue >> iSrcBit2) & fMask2) << iDstBit2);

    @staticmethod
    def algoToZipLambda(aaiAlgo, fAlgoMask, fCompileIt = True):
        assert aaiAlgo;
        if not fCompileIt:
            if len(aaiAlgo) == 1: return MaskZipper.__zipMask1;
            if len(aaiAlgo) == 2: return MaskZipper.__zipMask2;
            if len(aaiAlgo) == 3: return MaskZipper.__zipMask3;
            return MaskZipper.zipMask;
        # Compile it:
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
        _ = fAlgoMask
        #sFn = 'zipMaskCompiled_%#010x' % (fAlgoMask,);
        #sFn = 'zipMaskCompiled';
        #dTmp = {};
        #exec('def %s(uValue,_): return %s' % (sFn, sBody), globals(), dTmp);
        #return dTmp[sFn];
        return eval('lambda uValue,_: ' + sBody);

    @staticmethod
    def unzipMask(uValue, aaiAlgo):
        fRet = 0;
        for iSrcBit, iDstBit, fMask in aaiAlgo:
            fRet |= ((uValue >> iDstBit) & fMask) << iSrcBit;
        return fRet;

    @staticmethod
    def __unzipMask1(uValue, aaiAlgo):
        return uValue << aaiAlgo[0][0];

    @staticmethod
    def __unzipMask2(uValue, aaiAlgo):
        iSrcBit0, _,        fMask0 = aaiAlgo[0];
        iSrcBit1, iDstBit1, fMask1 = aaiAlgo[1];
        return ((uValue & fMask0) << iSrcBit0) | (((uValue >> iDstBit1) & fMask1) << iSrcBit1);

    @staticmethod
    def __unzipMask3(uValue, aaiAlgo):
        iSrcBit0, _,        fMask0 = aaiAlgo[0];
        iSrcBit1, iDstBit1, fMask1 = aaiAlgo[1];
        iSrcBit2, iDstBit2, fMask2 = aaiAlgo[2];
        return ((uValue & fMask0) << iSrcBit0) \
             | (((uValue >> iDstBit1) & fMask1) << iSrcBit1) \
             | (((uValue >> iDstBit2) & fMask2) << iSrcBit2);

    @staticmethod
    def algoToUnzipLambda(aaiAlgo, fAlgoMask, fCompileIt = True):
        assert aaiAlgo;
        if not fCompileIt:
            if len(aaiAlgo) == 1: return MaskZipper.__unzipMask1;
            if len(aaiAlgo) == 2: return MaskZipper.__unzipMask2;
            if len(aaiAlgo) == 3: return MaskZipper.__unzipMask3;
            return MaskZipper.unzipMask;
        # Compile it:
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

        _ = fAlgoMask
        #dTmp = {};
        #sFn = 'unzipMaskCompiled';
        #sFn = 'unzipMaskCompiled_%#010x' % (fAlgoMask,);
        #exec('def %s(uIdx,_): return %s' % (sFn, sBody), globals(), dTmp);
        #return dTmp[sFn];
        return eval('lambda uIdx,_: ' + sBody);


class MaskIterator(object):
    """ Helper class for DecoderNode.constructNextLevel(). """

    ## Maximum number of mask sub-parts.
    # Lower number means fewer instructions required to convert it into an index.
    # This is implied by the code in MaskZipper.compileAlgoLimited.
    kcMaxMaskParts = 3

    def __init__(self, fMask, cMaxTableSizeInBits, dDictDoneAlready, fCompileIt = False):
        self.fMask               = fMask;
        self.aaiAlgo             = MaskZipper.compileAlgo(fMask);
        self.fCompactMask        = MaskZipper.zipMask(fMask, self.aaiAlgo);
        #print('debug: fMask=%#x -> fCompactMask=%#x aaiAlgo=%s' % (fMask, self.fCompactMask, self.aaiAlgo));
        self.fnExpandMask        = MaskZipper.algoToUnzipLambda(self.aaiAlgo, fMask, fCompileIt);
        self.cMaxTableSizeInBits = cMaxTableSizeInBits;
        self.dDictDoneAlready    = dDictDoneAlready;
        self.cReturned           = 0;

    def __iter__(self):
        return self;

    def __next__(self):
        fCompactMask = self.fCompactMask;
        while fCompactMask != 0:
            cCurBits = fCompactMask.bit_count();
            if cCurBits <= self.cMaxTableSizeInBits:
                fMask = self.fnExpandMask(fCompactMask, self.aaiAlgo);
                if fMask not in self.dDictDoneAlready:
                    aaiMaskAlgo = MaskZipper.compileAlgoLimited(fMask);
                    if aaiMaskAlgo:
                        #assert aaiMaskAlgo == MaskZipper.compileAlgo(fMask), \
                        #    '%s vs %s' % (aaiMaskAlgo, MaskZipper.compileAlgo(fMask));
                        self.dDictDoneAlready[fMask] = 1;
                        self.fCompactMask = fCompactMask - 1;
                        self.cReturned += 1;
                        return (fMask, MaskZipper.algoToBitList(aaiMaskAlgo), aaiMaskAlgo);
            fCompactMask -= 1;
        self.fCompactMask = 0;
        #print('MaskIterator: fMask=%#x -> %u items returned' % (self.fMask, self.cReturned));
        raise StopIteration;


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

    def __init__(self, aoInstructions, fCheckedMask, fCheckedValue, uDepth):
        assert (~fCheckedMask & fCheckedValue) == 0;
        for idxInstr, oInstr in enumerate(aoInstructions):
            assert ((oInstr.fFixedValue ^ fCheckedValue) & fCheckedMask & oInstr.fFixedMask) == 0, \
                    '%s: fFixedValue=%#x fFixedMask=%#x fCheckedValue=%#x fCheckedMask=%#x -> %#x\naoInstructions: len=%s\n %s' \
                    % (idxInstr, oInstr.fFixedValue, oInstr.fFixedMask, fCheckedValue, fCheckedMask,
                       (oInstr.fFixedValue ^ fCheckedValue) & fCheckedMask & oInstr.fFixedMask,
                       len(aoInstructions),
                       '\n '.join(['%s%s: %#010x/%#010x %s' % ('*' if i == idxInstr else ' ', i,
                                                               oInstr2.fFixedValue, oInstr2.fFixedMask, oInstr2.sName)
                                   for i, oInstr2 in enumerate(aoInstructions[:idxInstr+8])]));

        self.aoInstructions     = aoInstructions;   ##< The instructions at this level.
        self.fCheckedMask       = fCheckedMask;     ##< The opcode bit mask covered thus far.
        self.fCheckedValue      = fCheckedValue;    ##< The value that goes with fCheckedMask.
        self.uDepth             = uDepth;           ##< The current node depth.
        self.uCost              = 0;                ##< The cost at this level.
        self.fLeafCheckNeeded   = len(aoInstructions) == 1 and (aoInstructions[0].fFixedMask & ~self.fCheckedMask) != 0;
        self.fChildMask         = 0;                ##< The mask used to separate the children.
        self.aoChildren         = [];               ##< Children, populated by constructNextLevel().

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
        #
        # Special case: leaf.
        #
        if len(self.aoInstructions) <= 1:
            assert len(self.aoChildren) == 0;
            return 16 if self.fLeafCheckNeeded else 0;
        sDbgPrefix = 'debug/%u: %s' % (self.uDepth, '  ' * self.uDepth);

        #
        # Do an inventory of the fixed masks used by the instructions.
        #
        dMaskCounts = collections.Counter();
        for oInstr in self.aoInstructions:
            dMaskCounts[oInstr.fFixedMask & ~self.fCheckedMask] += 1;
        assert 0 not in dMaskCounts or dMaskCounts[0] <= 1, \
                'dMaskCounts=%s len(self.aoInstructions)=%s\n%s' % (dMaskCounts, len(self.aoInstructions), self.aoInstructions);

        ## Determine the max table size for the number of instructions we have.
        #cInstructionsAsShift = 1;
        #while (1 << cInstructionsAsShift) < len(self.aoInstructions):
        #    cInstructionsAsShift += 1;
        #cMaxTableSizeInBits = self.kacMaxTableSizesInBits[cInstructionsAsShift];

        #
        # Whether to bother compiling the mask zip/unzip functions.
        #
        # The goal here is to keep the {built-in method builtins.eval} line far
        # away from top of the profiler stats, while at the same time keeping the
        # __zipMaskN and __unzipMaskN methods from taking up too much time.
        #
        fCompileMaskZipUnzip = len(self.aoInstructions) >= 12;

        #
        # Work thru the possible masks and test out the variations (brute force style).
        #
        uCostBest        = 0x7fffffffffffffff;
        fChildrenBest    = 0;
        aoChildrenBest   = [];

        dDictDoneAlready = {};
        for fOrgMask, cOccurences in dMaskCounts.most_common(8):
            cOccurencesAsShift = 1;
            while (1 << cOccurencesAsShift) < cOccurences:
                cOccurencesAsShift += 1;
            cMaxTableSizeInBits = self.kacMaxTableSizesInBits[cOccurencesAsShift]; # Not quite sure about this...
            if self.uDepth <= 1:
                print('%s===== Start: %#010x (%u) - %u instructions - max tab size %u ====='
                      % (sDbgPrefix, fOrgMask, self.popCount(fOrgMask), cOccurences, cMaxTableSizeInBits,));

            # Skip pointless stuff.
            if cOccurences >= 2 and fOrgMask > 0 and fOrgMask != 0xffffffff:
                #
                # Brute force relevant mask variations.
                # (The MaskIterator skips masks that are too wide and too fragmented.)
                #
                for fMask, dOrderedDictMask, aaiMaskToIdxAlgo in MaskIterator(fOrgMask, cMaxTableSizeInBits,
                                                                              dDictDoneAlready, fCompileMaskZipUnzip):
                    #print('%s>>> fMask=%#010x dOrderedDictMask=%s aaiMaskToIdxAlgo=%s)...'
                    #      % (sDbgPrefix, fMask, dOrderedDictMask, aaiMaskToIdxAlgo));
                    assert len(dOrderedDictMask) <= cMaxTableSizeInBits;

                    # Compile the indexing/unindexing functions.
                    fnToIndex   = MaskZipper.algoToZipLambda(aaiMaskToIdxAlgo, fMask, fCompileMaskZipUnzip);
                    fnFromIndex = MaskZipper.algoToUnzipLambda(aaiMaskToIdxAlgo, fMask, fCompileMaskZipUnzip);

                    # Create an temporary table empty with empty lists as entries.
                    ## @todo is there a better way for doing this? collections.defaultdict?
                    aaoTmp = [];
                    for _ in range(1 << len(dOrderedDictMask)):
                        aaoTmp.append([]);

                    # Insert the instructions into the temporary table.
                    for oInstr in self.aoInstructions:
                        idx = fnToIndex(oInstr.fFixedValue, aaiMaskToIdxAlgo);
                        #assert idx == MaskZipper.zipMask(oInstr.fFixedValue & fMask, aaiMaskToIdxAlgo);
                        #assert idx == fnToIndex(fnFromIndex(idx, aaiMaskToIdxAlgo), aaiMaskToIdxAlgo);
                        #assert idx == MaskZipper.zipMask(MaskZipper.unzipMask(idx, aaiMaskToIdxAlgo), aaiMaskToIdxAlgo);

                        #print('%s%#010x -> %#05x %s' % (sDbgPrefix, oInstr.fFixedValue, idx, oInstr.sName));
                        aoList = aaoTmp[idx];
                        aoList.append(oInstr);

                    # Construct decoder nodes from the aaoTmp lists, construct sub-levels and calculate costs.
                    uCostTmp      = 0; ## @todo calc base cost from table size and depth.
                    aoChildrenTmp = [];
                    for idx, aoInstrs in enumerate(aaoTmp):
                        oChild = DecoderNode(aoInstrs,
                                             self.fCheckedMask  | fMask,
                                             self.fCheckedValue | fnFromIndex(idx, aaiMaskToIdxAlgo),
                                             self.uDepth + 1);
                        aoChildrenTmp.append(oChild);
                        uCostTmp += oChild.constructNextLevel();

                    # Is this mask better than the previous?
                    if uCostTmp < uCostBest:
                        if self.uDepth <= 1:
                            print('%s~~~ New best! fMask=%#010x uCost=%#x (previous %#010x / %#x) ...'
                                  % (sDbgPrefix, fMask, uCostTmp, fChildrenBest, uCostBest, ));
                        uCostBest      = uCostTmp;
                        fChildrenBest  = fMask;
                        aoChildrenBest = aoChildrenTmp;

        if self.uDepth <= 1:
            print('%s===== Final: fMask=%#010x uCost=%#x TabSize=%#x Instructions=%u...'
                  % (sDbgPrefix, fChildrenBest, uCostBest, len(aoChildrenBest), len(self.aoInstructions)));
        if aoChildrenBest is None:
            pass; ## @todo

        # Done.
        self.fChildMask = fChildrenBest;
        self.aoChildren = aoChildrenBest;
        self.uCost      = uCostBest;

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

        #for _ in MaskIterator(0xffc0ff00, 12, {}):
        #    pass;
        #return 2;

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
        # debug:
        oArgParser.add_argument('--print-instructions',
                                dest    = 'fPrintInstructions',
                                action  = 'store_true',
                                default = False,
                                help    = 'List the instructions after loading.');
        oArgParser.add_argument('--print-fixed-mask-stats',
                                dest    = 'fPrintFixedMaskStats',
                                action  = 'store_true',
                                default = False,
                                help    = 'List statistics on fixed bit masks.');
        oArgParser.add_argument('--print-fixed-mask-top-10',
                                dest    = 'fPrintFixedMaskTop10',
                                action  = 'store_true',
                                default = False,
                                help    = 'List the 10 top fixed bit masks.');
        # Do it!
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

def printException(oXcpt):
    print('----- Exception Caught! -----', flush = True);
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
    idxFrame = 0;
    asFormatted = [];
    oReFirstFrameLine = re.compile(r'^  File ".*", line \d+, in ')
    for sLine in oTB.format():
        if oReFirstFrameLine.match(sLine):
            idxFrame += 1;
        asFormatted.append(sLine);
    for sLine in asFormatted:
        if oReFirstFrameLine.match(sLine):
            idxFrame -= 1;
            sLine = '#%u %s' % (idxFrame, sLine.lstrip());
        print(sLine);
    print('----', flush = True);

if __name__ == '__main__':
    #for fOrgMask in (1, 3, 7, 15, 31):
    #    print('Test %#x:' % (fOrgMask,));
    #    for x in MaskIterator(fOrgMask, 16, {}):
    #        print('MaskIterator: fMask=%#04x aiBits=%20s aaiAlgo=%s' % (x[0],x[1],x[2]));
    fProfileIt = True;
    oProfiler = cProfile.Profile() if fProfileIt else None;
    try:
        if not oProfiler:
            rcExit = IEMArmGenerator().main(sys.argv);
        else:
            rcExit = oProfiler.runcall(IEMArmGenerator().main, sys.argv);
    except Exception as oXcptOuter:
        printException(oXcptOuter);
        rcExit = 2;
    except KeyboardInterrupt as oXcptOuter:
        printException(oXcptOuter);
        rcExit = 2;
    if oProfiler:
        if not oProfiler:
            oProfiler.print_stats(sort='tottime');
        else:
            oStringStream = io.StringIO();
            pstats.Stats(oProfiler, stream = oStringStream).strip_dirs().sort_stats('tottime').print_stats(64);
            for iStatLine, sStatLine in enumerate(oStringStream.getvalue().split('\n')):
                if iStatLine > 20:
                    asStatWords = sStatLine.split();
                    if asStatWords[1] in { '0.000', '0.001', '0.002', '0.003', '0.004', '0.005' }:
                        break;
                print(sStatLine);
    sys.exit(rcExit);

