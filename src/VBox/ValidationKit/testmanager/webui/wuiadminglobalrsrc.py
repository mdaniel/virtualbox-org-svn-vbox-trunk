# -*- coding: utf-8 -*-
# $Id$

"""
Test Manager WUI - Global resources.
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
from testmanager.webui.wuibase          import WuiException
from testmanager.webui.wuicontentbase   import WuiContentBase
from testmanager.webui.wuihlpform       import WuiHlpForm
from testmanager.core.globalresource    import GlobalResourceData
from testmanager.webui.wuicontentbase   import WuiListContentBase, WuiTmLink


class WuiGlobalResource(WuiContentBase):
    """
    WUI global resources content generator.
    """

    def __init__(self, oData, fnDPrint = None):
        """
        Do necessary initializations
        """
        WuiContentBase.__init__(self, fnDPrint)
        self._oData  = oData

    def showAddModifyPage(self, sAction, dErrors = None):
        """
        Render add global resource HTML form.
        """
        from testmanager.webui.wuiadmin import WuiAdmin

        sFormActionUrl = '%s?%s=%s' % (WuiAdmin.ksScriptName,
                                       WuiAdmin.ksParamAction, sAction)
        if sAction == WuiAdmin.ksActionGlobalRsrcAdd:
            sTitle = 'Add Global Resource'
        elif sAction == WuiAdmin.ksActionGlobalRsrcEdit:
            sTitle = 'Modify Global Resource'
            sFormActionUrl += '&%s=%s' % (GlobalResourceData.ksParam_idGlobalRsrc, self._oData.idGlobalRsrc)
        else:
            raise WuiException('Invalid paraemter "%s"' % (sAction,))

        oForm = WuiHlpForm('globalresourceform',
                           sFormActionUrl,
                           dErrors if dErrors is not None else dict())

        if sAction == WuiAdmin.ksActionGlobalRsrcAdd:
            oForm.addIntRO  (GlobalResourceData.ksParam_idGlobalRsrc,    self._oData.idGlobalRsrc,   'Global Resource ID')
        oForm.addTimestampRO(GlobalResourceData.ksParam_tsEffective,     self._oData.tsEffective,    'Last changed')
        oForm.addTimestampRO(GlobalResourceData.ksParam_tsExpire,        self._oData.tsExpire,       'Expires (excl)')
        oForm.addIntRO      (GlobalResourceData.ksParam_uidAuthor,       self._oData.uidAuthor,      'Changed by UID')
        oForm.addText       (GlobalResourceData.ksParam_sName,           self._oData.sName,          'Name')
        oForm.addText       (GlobalResourceData.ksParam_sDescription,    self._oData.sDescription,   'Description')
        oForm.addCheckBox   (GlobalResourceData.ksParam_fEnabled,        self._oData.fEnabled,       'Enabled')

        oForm.addSubmit('Submit')

        return (sTitle, oForm.finalize())


class WuiGlobalResourceList(WuiListContentBase):
    """
    WUI Content Generator.
    """

    def __init__(self, aoEntries, iPage, cItemsPerPage, tsEffective, fnDPrint, oDisp, aiSelectedSortColumns = None):
        WuiListContentBase.__init__(self, aoEntries, iPage, cItemsPerPage, tsEffective,
                                    sTitle = 'Global Resources', sId = 'globalResources',
                                    fnDPrint = fnDPrint, oDisp = oDisp, aiSelectedSortColumns = aiSelectedSortColumns);

        self._asColumnHeaders = ['ID', 'Name', 'Description', 'Enabled', 'Actions' ]
        self._asColumnAttribs = ['align="right"', 'align="center"', 'align="center"',
                                 'align="center"', 'align="center"']

    def _formatListEntry(self, iEntry):
        from testmanager.webui.wuiadmin import WuiAdmin
        oEntry = self._aoEntries[iEntry]

        aoActions = [ ];
        if self._oDisp is None or not self._oDisp.isReadOnlyUser():
            aoActions += [
                WuiTmLink('Modify', WuiAdmin.ksScriptName,
                          { WuiAdmin.ksParamAction: WuiAdmin.ksActionGlobalRsrcShowEdit,
                            GlobalResourceData.ksParam_idGlobalRsrc: oEntry.idGlobalRsrc }),
                WuiTmLink('Remove', WuiAdmin.ksScriptName,
                          { WuiAdmin.ksParamAction: WuiAdmin.ksActionGlobalRsrcDel,
                            GlobalResourceData.ksParam_idGlobalRsrc: oEntry.idGlobalRsrc },
                          sConfirm = 'Are you sure you want to remove global resource #%d?' % (oEntry.idGlobalRsrc,)),
            ];

        return [ oEntry.idGlobalRsrc,
                 oEntry.sName,
                 oEntry.sDescription,
                 oEntry.fEnabled,
                 aoActions, ];

