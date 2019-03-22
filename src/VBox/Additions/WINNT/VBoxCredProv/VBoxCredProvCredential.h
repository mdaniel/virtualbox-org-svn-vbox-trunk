/* $Id$ */
/** @file
 * VBoxCredProvCredential - Class for keeping and handling the passed credentials.
 */

/*
 * Copyright (C) 2012-2017 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___VBOX_CREDPROV_CREDENTIAL_H___
#define ___VBOX_CREDPROV_CREDENTIAL_H___


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <iprt/win/windows.h>
#include <NTSecAPI.h>
#define SECURITY_WIN32
#include <Security.h>
#include <ShlGuid.h>
#include <strsafe.h>

#include <iprt/win/shlwapi.h>

#include <iprt/string.h>

#include "VBoxCredentialProvider.h"



class VBoxCredProvProvider;

class VBoxCredProvCredential : public ICredentialProviderCredential
{
public:

    VBoxCredProvCredential(void);

    virtual ~VBoxCredProvCredential(void);

    /** @name IUnknown methods
     * @{ */
    IFACEMETHODIMP_(ULONG) AddRef(void);
    IFACEMETHODIMP_(ULONG) Release(void);
    IFACEMETHODIMP         QueryInterface(REFIID interfaceID, void **ppvInterface);
    /** @} */

    /** @name ICredentialProviderCredential methods.
     * @{ */
    IFACEMETHODIMP Advise(ICredentialProviderCredentialEvents* pcpce);
    IFACEMETHODIMP UnAdvise(void);

    IFACEMETHODIMP SetSelected(PBOOL pfAutoLogon);
    IFACEMETHODIMP SetDeselected(void);

    IFACEMETHODIMP GetFieldState(DWORD dwFieldID,
                                 CREDENTIAL_PROVIDER_FIELD_STATE* pcpfs,
                                 CREDENTIAL_PROVIDER_FIELD_INTERACTIVE_STATE* pcpfis);

    IFACEMETHODIMP GetStringValue(DWORD dwFieldID, PWSTR *ppwsz);
    IFACEMETHODIMP GetBitmapValue(DWORD dwFieldID, HBITMAP *phbmp);
    IFACEMETHODIMP GetCheckboxValue(DWORD dwFieldID, PBOOL pfChecked, PWSTR *ppwszLabel);
    IFACEMETHODIMP GetComboBoxValueCount(DWORD dwFieldID, DWORD* pcItems, DWORD *pdwSelectedItem);
    IFACEMETHODIMP GetComboBoxValueAt(DWORD dwFieldID, DWORD dwItem, PWSTR *ppwszItem);
    IFACEMETHODIMP GetSubmitButtonValue(DWORD dwFieldID, DWORD *pdwAdjacentTo);

    IFACEMETHODIMP SetStringValue(DWORD dwFieldID, PCWSTR pwszValue);
    IFACEMETHODIMP SetCheckboxValue(DWORD dwFieldID, BOOL fChecked);
    IFACEMETHODIMP SetComboBoxSelectedValue(DWORD dwFieldID, DWORD dwSelectedItem);
    IFACEMETHODIMP CommandLinkClicked(DWORD dwFieldID);

    IFACEMETHODIMP GetSerialization(CREDENTIAL_PROVIDER_GET_SERIALIZATION_RESPONSE *pcpGetSerializationResponse,
                                    CREDENTIAL_PROVIDER_CREDENTIAL_SERIALIZATION *pcpCredentialSerialization,
                                    PWSTR *ppwszOptionalStatusText, CREDENTIAL_PROVIDER_STATUS_ICON *pcpsiOptionalStatusIcon);
    IFACEMETHODIMP ReportResult(NTSTATUS ntStatus, NTSTATUS ntSubStatus,
                                PWSTR *ppwszOptionalStatusText,
                                CREDENTIAL_PROVIDER_STATUS_ICON* pcpsiOptionalStatusIcon);
    /** @} */

    HRESULT setField(DWORD dwFieldID, const PRTUTF16 pcwszString, bool fNotifyUI);
    HRESULT Reset(void);
    HRESULT Initialize(CREDENTIAL_PROVIDER_USAGE_SCENARIO cpus);
    int RetrieveCredentials(void);
    BOOL TranslateAccountName(PWSTR pwszDisplayName, PWSTR *ppwszAccoutName);
    BOOL ExtractAccoutData(PWSTR pwszAccountData, PWSTR *ppwszAccoutName, PWSTR *ppwszDomain);

protected:
    HRESULT RTUTF16ToUnicode(PUNICODE_STRING pUnicodeDest, PRTUTF16 pwszSource, bool fCopy);
    HRESULT RTUTF16ToUnicodeA(PUNICODE_STRING pUnicodeDest, PRTUTF16 pwszSource);
    void UnicodeStringFree(PUNICODE_STRING pUnicode);

    HRESULT kerberosLogonCreate(KERB_INTERACTIVE_LOGON *pLogon,
                                CREDENTIAL_PROVIDER_USAGE_SCENARIO enmUsage,
                                PRTUTF16 pwszUser, PRTUTF16 pwszPassword, PRTUTF16 pwszDomain);
    void    kerberosLogonDestroy(KERB_INTERACTIVE_LOGON *pLogon);
    HRESULT kerberosLogonSerialize(const KERB_INTERACTIVE_LOGON *pLogon, PBYTE *ppPackage, DWORD *pcbPackage);

private:
    /** Internal reference count. */
    LONG                                  m_cRefs;
    /** The usage scenario for which we were enumerated. */
    CREDENTIAL_PROVIDER_USAGE_SCENARIO    m_enmUsageScenario;
    /** The actual credential provider fields.
     *  Must be allocated as long as the credential provider is in charge. */
    PRTUTF16                              m_apwszFields[VBOXCREDPROV_NUM_FIELDS];
    /** Pointer to event handler. */
    ICredentialProviderCredentialEvents  *m_pEvents;
    /** Flag indicating whether credentials already were retrieved. */
    bool                                  m_fHaveCreds;
    /** Flag indicating wheter a profile (user tile) current is selected or not. */
    bool                                  m_fIsSelected;
};
#endif /* !___VBOX_CREDPROV_CREDENTIAL_H___ */

