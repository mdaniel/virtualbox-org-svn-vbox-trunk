# -*- coding: utf-8 -*-
# $Id$

"""
Test Manager WUI - User accounts.
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
from testmanager.webui.wuicontentbase   import WuiFormContentBase, WuiListContentBase, WuiTmLink;
from testmanager.core.useraccount       import UserAccountData


class WuiUserAccount(WuiFormContentBase):
    """
    WUI user account content generator.
    """
    def __init__(self, oData, sMode, oDisp):
        if sMode == WuiFormContentBase.ksMode_Add:
            sTitle = 'Add User Account';
        elif sMode == WuiFormContentBase.ksMode_Edit:
            sTitle = 'Modify User Account';
        else:
            assert sMode == WuiFormContentBase.ksMode_Show;
            sTitle = 'User Account';
        WuiFormContentBase.__init__(self, oData, sMode, 'User', oDisp, sTitle);

    def _populateForm(self, oForm, oData):
        oForm.addIntRO(      UserAccountData.ksParam_uid,         oData.uid,         'User ID');
        oForm.addTimestampRO(UserAccountData.ksParam_tsEffective, oData.tsEffective, 'Effective Date');
        oForm.addTimestampRO(UserAccountData.ksParam_tsExpire,    oData.tsExpire,    'Effective Date');
        oForm.addIntRO(      UserAccountData.ksParam_uidAuthor,   oData.uidAuthor,   'Changed by UID');
        oForm.addText(       UserAccountData.ksParam_sLoginName,  oData.sLoginName,  'Login name')
        oForm.addText(       UserAccountData.ksParam_sUsername,   oData.sUsername,   'User name')
        oForm.addText(       UserAccountData.ksParam_sFullName,   oData.sFullName,   'Full name')
        oForm.addText(       UserAccountData.ksParam_sEmail,      oData.sEmail,      'E-mail')
        oForm.addCheckBox(   UserAccountData.ksParam_fReadOnly,   oData.fReadOnly,   'Only read access')
        if self._sMode != WuiFormContentBase.ksMode_Show:
            oForm.addSubmit('Add User' if self._sMode == WuiFormContentBase.ksMode_Add else 'Change User');
        return True;


class WuiUserAccountList(WuiListContentBase):
    """
    WUI user account list content generator.
    """

    def __init__(self, aoEntries, iPage, cItemsPerPage, tsEffective, fnDPrint, oDisp, aiSelectedSortColumns = None):
        WuiListContentBase.__init__(self, aoEntries, iPage, cItemsPerPage, tsEffective,
                                    sTitle = 'Registered User Accounts', sId = 'users', fnDPrint = fnDPrint, oDisp = oDisp,
                                    aiSelectedSortColumns = aiSelectedSortColumns);
        self._asColumnHeaders = ['User ID', 'Name', 'E-mail', 'Full Name', 'Login Name', 'Access', 'Actions'];
        self._asColumnAttribs = ['align="center"', 'align="center"', 'align="center"', 'align="center"', 'align="center"',
                                 'align="center"', 'align="center"', ];

    def _formatListEntry(self, iEntry):
        from testmanager.webui.wuiadmin import WuiAdmin;
        oEntry  = self._aoEntries[iEntry];
        aoActions = [
            WuiTmLink('Details', WuiAdmin.ksScriptName,
                      { WuiAdmin.ksParamAction: WuiAdmin.ksActionUserDetails,
                        UserAccountData.ksParam_uid: oEntry.uid } ),
        ];
        if self._oDisp is None or not self._oDisp.isReadOnlyUser():
            aoActions += [
                WuiTmLink('Modify', WuiAdmin.ksScriptName,
                          { WuiAdmin.ksParamAction: WuiAdmin.ksActionUserEdit,
                            UserAccountData.ksParam_uid: oEntry.uid } ),
                WuiTmLink('Remove', WuiAdmin.ksScriptName,
                          { WuiAdmin.ksParamAction: WuiAdmin.ksActionUserDelPost,
                            UserAccountData.ksParam_uid: oEntry.uid },
                          sConfirm = 'Are you sure you want to remove user #%d?' % (oEntry.uid,)),
            ];

        return [ oEntry.uid, oEntry.sUsername, oEntry.sEmail, oEntry.sFullName, oEntry.sLoginName,
                 'read only' if oEntry.fReadOnly else 'full',
                 aoActions, ];

