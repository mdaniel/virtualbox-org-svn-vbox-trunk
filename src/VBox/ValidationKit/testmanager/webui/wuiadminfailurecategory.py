# -*- coding: utf-8 -*-
# $Id$

"""
Test Manager WUI - Failure Categories Web content generator.
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
from testmanager.webui.wuicontentbase           import WuiFormContentBase, WuiContentBase, WuiListContentBase, WuiTmLink;
from testmanager.webui.wuiadminfailurereason    import WuiAdminFailureReasonList;
from testmanager.core.failurecategory           import FailureCategoryData;
from testmanager.core.failurereason             import FailureReasonLogic;


class WuiFailureReasonCategoryLink(WuiTmLink):
    """ Link to a failure category. """
    def __init__(self, idFailureCategory, sName = WuiContentBase.ksShortDetailsLink, sTitle = None, fBracketed = None):
        if fBracketed is None:
            fBracketed = len(sName) > 2;
        from testmanager.webui.wuiadmin import WuiAdmin;
        WuiTmLink.__init__(self, sName = sName,
                           sUrlBase = WuiAdmin.ksScriptName,
                           dParams = { WuiAdmin.ksParamAction: WuiAdmin.ksActionFailureCategoryDetails,
                                       FailureCategoryData.ksParam_idFailureCategory: idFailureCategory, },
                           fBracketed = fBracketed,
                           sTitle = sTitle);
        self.idFailureCategory = idFailureCategory;



class WuiFailureCategory(WuiFormContentBase):
    """
    WUI Failure Category HTML content generator.
    """

    def __init__(self, oData, sMode, oDisp):
        """
        Prepare & initialize parent
        """

        sTitle = 'Failure Category';
        if sMode == WuiFormContentBase.ksMode_Add:
            sTitle = 'Add ' + sTitle;
        elif sMode == WuiFormContentBase.ksMode_Edit:
            sTitle = 'Edit ' + sTitle;
        else:
            assert sMode == WuiFormContentBase.ksMode_Show;

        WuiFormContentBase.__init__(self, oData, sMode, 'FailureCategory', oDisp, sTitle);

    def _populateForm(self, oForm, oData):
        """
        Construct an HTML form
        """

        oForm.addIntRO      (FailureCategoryData.ksParam_idFailureCategory,   oData.idFailureCategory,  'Failure Category ID')
        oForm.addTimestampRO(FailureCategoryData.ksParam_tsEffective,         oData.tsEffective,        'Last changed')
        oForm.addTimestampRO(FailureCategoryData.ksParam_tsExpire,            oData.tsExpire,           'Expires (excl)')
        oForm.addIntRO      (FailureCategoryData.ksParam_uidAuthor,           oData.uidAuthor,          'Changed by UID')
        oForm.addText       (FailureCategoryData.ksParam_sShort,              oData.sShort,             'Short Description')
        oForm.addText       (FailureCategoryData.ksParam_sFull,               oData.sFull,              'Full Description')

        oForm.addSubmit()

        return True;

    def _generatePostFormContent(self, oData):
        """
        Adds a table with the category members below the form.
        """
        if oData.idFailureCategory is not None and oData.idFailureCategory >= 0:
            oLogic    = FailureReasonLogic(self._oDisp.getDb());
            tsNow     = self._oDisp.getNow();
            cMax      = 4096;
            aoEntries = oLogic.fetchForListingInCategory(0, cMax, tsNow, oData.idFailureCategory)
            if aoEntries:
                oList = WuiAdminFailureReasonList(aoEntries, 0, cMax, tsNow, fnDPrint = None, oDisp = self._oDisp);
                return [ [ 'Members', oList.show(fShowNavigation = False)[1]], ];
        return [];



class WuiFailureCategoryList(WuiListContentBase):
    """
    WUI Admin Failure Category Content Generator.
    """

    def __init__(self, aoEntries, iPage, cItemsPerPage, tsEffective, fnDPrint, oDisp, aiSelectedSortColumns = None):
        WuiListContentBase.__init__(self, aoEntries, iPage, cItemsPerPage, tsEffective,
                                    sTitle = 'Failure Categories', sId = 'failureCategories',
                                    fnDPrint = fnDPrint, oDisp = oDisp, aiSelectedSortColumns = aiSelectedSortColumns);

        self._asColumnHeaders = ['ID', 'Short Description', 'Full Description', 'Actions' ]
        self._asColumnAttribs = ['align="right"', 'align="center"', 'align="center"', 'align="center"']

    def _formatListEntry(self, iEntry):
        from testmanager.webui.wuiadmin import WuiAdmin
        oEntry = self._aoEntries[iEntry]

        aoActions = [
            WuiTmLink('Details', WuiAdmin.ksScriptName,
                      { WuiAdmin.ksParamAction: WuiAdmin.ksActionFailureCategoryDetails,
                        FailureCategoryData.ksParam_idFailureCategory: oEntry.idFailureCategory }),
        ];
        if self._oDisp is None or not self._oDisp.isReadOnlyUser():
            aoActions += [
                WuiTmLink('Modify', WuiAdmin.ksScriptName,
                          { WuiAdmin.ksParamAction: WuiAdmin.ksActionFailureCategoryEdit,
                            FailureCategoryData.ksParam_idFailureCategory: oEntry.idFailureCategory }),
                WuiTmLink('Remove', WuiAdmin.ksScriptName,
                          { WuiAdmin.ksParamAction: WuiAdmin.ksActionFailureCategoryDoRemove,
                            FailureCategoryData.ksParam_idFailureCategory: oEntry.idFailureCategory },
                          sConfirm = 'Do you really want to remove failure cateogry #%d?' % (oEntry.idFailureCategory,)),
            ]

        return [ oEntry.idFailureCategory,
                 oEntry.sShort,
                 oEntry.sFull,
                 aoActions,
        ];

