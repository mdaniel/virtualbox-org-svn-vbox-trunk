# -*- coding: utf-8 -*-
# $Id$

"""
Test Manager Web-UI - Graph Helpers - Simple/Stub Implementation.
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
__version__ = "$Revision$"

# Validation Kit imports.
from common.webutils                    import escapeAttr, escapeElem;
from testmanager.webui.wuihlpgraphbase  import WuiHlpGraphBase;



class WuiHlpBarGraph(WuiHlpGraphBase):
    """
    Bar graph.
    """

    def __init__(self, sId, oData, oDisp = None):
        WuiHlpGraphBase.__init__(self, sId, oData, oDisp);
        self.cxMaxBar = 480;
        self.fpMax = None;
        self.fpMin = 0.0;

    def setRangeMax(self, fpMax):
        """ Sets the max range."""
        self.fpMax = float(fpMax);
        return None;

    def renderGraph(self):
        aoTable  = self._oData.aoTable;
        sReport  = '<div class="tmbargraph">\n';

        # Figure the range.
        fpMin = self.fpMin;
        fpMax = self.fpMax;
        if self.fpMax is None:
            fpMax = float(aoTable[1].aoValues[0]);
        for i in range(1, len(aoTable)):
            for oValue in aoTable[i].aoValues:
                fpValue = float(oValue);
                if fpValue < fpMin:
                    fpMin = fpValue;
                if fpValue > fpMax:
                    fpMax = fpValue;
        assert fpMin >= 0;

        # Format the data.
        sReport += '<table class="tmbargraphl1" border="1" id="%s">\n' % (escapeAttr(self._sId),);
        for i in range(1, len(aoTable)):
            oRow = aoTable[i];
            sReport += '  <tr>\n' \
                       '    <td>%s</td>\n' \
                       '    <td height="100%%" width="%spx">\n' \
                       '      <table class="tmbargraphl2" height="100%%" width="100%%" ' \
                                    'border="0" cellspacing="0" cellpadding="0">\n' \
                     % (escapeElem(oRow.sName), escapeAttr(str(self.cxMaxBar + 2)));
            for j in range(len(oRow.aoValues)):
                oValue = oRow.aoValues[j];
                cPct   = int(float(oValue) * 100 / fpMax);
                cxBar  = int(float(oValue) * self.cxMaxBar / fpMax);
                sValue = escapeElem(oRow.asValues[j]);
                sColor = self.kasColors[j % len(self.kasColors)];
                sInvColor = 'white';
                if sColor[0] == '#' and len(sColor) == 7:
                    sInvColor = '#%06x' % (~int(sColor[1:],16) & 0xffffff,);

                sReport += '        <tr><td>\n' \
                           '          <table class="tmbargraphl3" height="100%%" border="0" cellspacing="0" cellpadding="0">\n' \
                           '            <tr>\n';
                if cPct >= 99:
                    sReport += '              <td width="%spx" nowrap bgcolor="%s" align="right" style="color:%s;">' \
                               '%s&nbsp;</td>\n' \
                             % (cxBar, sColor, sInvColor, sValue);
                elif cPct < 1:
                    sReport += '              <td width="%spx" nowrap style="color:%s;">%s</td>\n' \
                             % (self.cxMaxBar - cxBar, sColor, sValue);
                elif cPct >= 50:
                    sReport += '              <td width="%spx" nowrap bgcolor="%s" align="right" style="color:%s;">' \
                               '%s&nbsp;</td>\n' \
                               '              <td width="%spx" nowrap><div>&nbsp;</div></td>\n' \
                             % (cxBar, sColor, sInvColor, sValue, self.cxMaxBar - cxBar);
                else:
                    sReport += '              <td width="%spx" nowrap bgcolor="%s"></td>\n' \
                               '              <td width="%spx" nowrap>&nbsp;%s</td>\n' \
                             % (cxBar, sColor, self.cxMaxBar - cxBar, sValue);
                sReport += '            </tr>\n' \
                           '          </table>\n' \
                           '        </td></tr>\n'
            sReport += '      </table>\n' \
                       '    </td>\n' \
                       '  </tr>\n';
            if i + 1 < len(aoTable) and len(oRow.aoValues) > 1:
                sReport += '  <tr></tr>\n'

        sReport += '</table>\n';

        sReport += '<div class="tmgraphlegend">\n' \
                   '  <p>Legend:\n';
        for j in range(len(aoTable[0].asValues)):
            sColor = self.kasColors[j % len(self.kasColors)];
            sReport += '    <font color="%s">&#x25A0; %s</font>\n' \
                     % (sColor, escapeElem(aoTable[0].asValues[j]));
        sReport += '  </p>\n' \
                   '</div>\n';

        sReport += '</div>\n';
        return sReport;




class WuiHlpLineGraph(WuiHlpGraphBase):
    """
    Line graph.
    """

    def __init__(self, sId, oData, oDisp):
        WuiHlpGraphBase.__init__(self, sId, oData, oDisp);


class WuiHlpLineGraphErrorbarY(WuiHlpLineGraph):
    """
    Line graph with an errorbar for the Y axis.
    """

    pass;

