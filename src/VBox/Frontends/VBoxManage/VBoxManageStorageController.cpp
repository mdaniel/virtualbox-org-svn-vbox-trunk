/* $Id$ */
/** @file
 * VBoxManage - The storage controller related commands.
 */

/*
 * Copyright (C) 2006-2017 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef VBOX_ONLY_DOCS


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#include <VBox/com/com.h>
#include <VBox/com/array.h>
#include <VBox/com/ErrorInfo.h>
#include <VBox/com/errorprint.h>
#include <VBox/com/VirtualBox.h>

#include <iprt/path.h>
#include <iprt/param.h>
#include <iprt/string.h>
#include <iprt/ctype.h>
#include <iprt/stream.h>
#include <iprt/getopt.h>
#include <VBox/log.h>

#include "VBoxManage.h"
using namespace com;


// funcs
///////////////////////////////////////////////////////////////////////////////


static const RTGETOPTDEF g_aStorageAttachOptions[] =
{
    { "--storagectl",       's', RTGETOPT_REQ_STRING },
    { "--port",             'p', RTGETOPT_REQ_UINT32 },
    { "--device",           'd', RTGETOPT_REQ_UINT32 },
    { "--type",             't', RTGETOPT_REQ_STRING },
    { "--medium",           'm', RTGETOPT_REQ_STRING },
    { "--mtype",            'M', RTGETOPT_REQ_STRING },
    { "--passthrough",      'h', RTGETOPT_REQ_STRING },
    { "--tempeject",        'e', RTGETOPT_REQ_STRING },
    { "--nonrotational",    'n', RTGETOPT_REQ_STRING },
    { "--discard",          'u', RTGETOPT_REQ_STRING },
    { "--hotpluggable",     'o', RTGETOPT_REQ_STRING },
    { "--bandwidthgroup",   'b', RTGETOPT_REQ_STRING },
    { "--forceunmount",     'f', RTGETOPT_REQ_NOTHING },
    { "--comment",          'C', RTGETOPT_REQ_STRING },
    { "--setuuid",          'q', RTGETOPT_REQ_STRING },
    { "--setparentuuid",    'Q', RTGETOPT_REQ_STRING },
    // iSCSI options
    { "--server",           'S', RTGETOPT_REQ_STRING },
    { "--target",           'T', RTGETOPT_REQ_STRING },
    { "--tport",            'P', RTGETOPT_REQ_STRING },
    { "--lun",              'L', RTGETOPT_REQ_STRING },
    { "--encodedlun",       'E', RTGETOPT_REQ_STRING },
    { "--username",         'U', RTGETOPT_REQ_STRING },
    { "--password",         'W', RTGETOPT_REQ_STRING },
    { "--initiator",        'N', RTGETOPT_REQ_STRING },
    { "--intnet",           'I', RTGETOPT_REQ_NOTHING },
};

RTEXITCODE handleStorageAttach(HandlerArg *a)
{
    int c = VERR_INTERNAL_ERROR;        /* initialized to shut up gcc */
    HRESULT rc = S_OK;
    ULONG port   = ~0U;
    ULONG device = ~0U;
    bool fForceUnmount = false;
    bool fSetMediumType = false;
    bool fSetNewUuid = false;
    bool fSetNewParentUuid = false;
    MediumType_T enmMediumType = MediumType_Normal;
    Bstr bstrComment;
    const char *pszCtl  = NULL;
    DeviceType_T devTypeRequested = DeviceType_Null;
    const char *pszMedium = NULL;
    const char *pszPassThrough = NULL;
    const char *pszTempEject = NULL;
    const char *pszNonRotational = NULL;
    const char *pszDiscard = NULL;
    const char *pszHotPluggable = NULL;
    const char *pszBandwidthGroup = NULL;
    Bstr bstrNewUuid;
    Bstr bstrNewParentUuid;
    // iSCSI options
    Bstr bstrServer;
    Bstr bstrTarget;
    Bstr bstrPort;
    Bstr bstrLun;
    Bstr bstrUsername;
    Bstr bstrPassword;
    Bstr bstrInitiator;
    Bstr bstrIso;
    Utf8Str strIso;
    bool fIntNet = false;

    RTGETOPTUNION ValueUnion;
    RTGETOPTSTATE GetState;
    ComPtr<IMachine> machine;
    ComPtr<IStorageController> storageCtl;
    ComPtr<ISystemProperties> systemProperties;

    RTGetOptInit(&GetState, a->argc, a->argv, g_aStorageAttachOptions,
                 RT_ELEMENTS(g_aStorageAttachOptions), 1, RTGETOPTINIT_FLAGS_NO_STD_OPTS);

    while (   SUCCEEDED(rc)
           && (c = RTGetOpt(&GetState, &ValueUnion)))
    {
        switch (c)
        {
            case 's':   // storage controller name
            {
                if (ValueUnion.psz)
                    pszCtl = ValueUnion.psz;
                else
                    rc = E_FAIL;
                break;
            }

            case 'p':   // port
            {
                port = ValueUnion.u32;
                break;
            }

            case 'd':   // device
            {
                device = ValueUnion.u32;
                break;
            }

            case 'm':   // medium <none|emptydrive|additions|uuid|filename|host:<drive>|iSCSI>
            {
                if (ValueUnion.psz)
                    pszMedium = ValueUnion.psz;
                else
                    rc = E_FAIL;
                break;
            }

            case 't':   // type <dvddrive|hdd|fdd>
            {
                if (ValueUnion.psz)
                {
                    if (!RTStrICmp(ValueUnion.psz, "hdd"))
                        devTypeRequested = DeviceType_HardDisk;
                    else if (!RTStrICmp(ValueUnion.psz, "fdd"))
                        devTypeRequested = DeviceType_Floppy;
                    else if (!RTStrICmp(ValueUnion.psz, "dvddrive"))
                        devTypeRequested = DeviceType_DVD;
                    else
                        return errorArgument("Invalid --type argument '%s'", ValueUnion.psz);
                }
                else
                    rc = E_FAIL;
                break;
            }

            case 'h':   // passthrough <on|off>
            {
                if (ValueUnion.psz)
                    pszPassThrough = ValueUnion.psz;
                else
                    rc = E_FAIL;
                break;
            }

            case 'e':   // tempeject <on|off>
            {
                if (ValueUnion.psz)
                    pszTempEject = ValueUnion.psz;
                else
                    rc = E_FAIL;
                break;
            }

            case 'n':   // nonrotational <on|off>
            {
                if (ValueUnion.psz)
                    pszNonRotational = ValueUnion.psz;
                else
                    rc = E_FAIL;
                break;
            }

            case 'u':   // discard <on|off>
            {
                if (ValueUnion.psz)
                    pszDiscard = ValueUnion.psz;
                else
                    rc = E_FAIL;
                break;
            }

            case 'o':   // hotpluggable <on|off>
            {
                if (ValueUnion.psz)
                    pszHotPluggable = ValueUnion.psz;
                else
                    rc = E_FAIL;
                break;
            }

            case 'b':   // bandwidthgroup <name>
            {
                if (ValueUnion.psz)
                    pszBandwidthGroup = ValueUnion.psz;
                else
                    rc = E_FAIL;
                break;
            }

            case 'f':   // force unmount medium during runtime
            {
                fForceUnmount = true;
                break;
            }

            case 'C':
                if (ValueUnion.psz)
                    bstrComment = ValueUnion.psz;
                else
                    rc = E_FAIL;
                break;

            case 'q':
                if (ValueUnion.psz)
                {
                    bstrNewUuid = ValueUnion.psz;
                    fSetNewUuid = true;
                }
                else
                    rc = E_FAIL;
                break;

            case 'Q':
                if (ValueUnion.psz)
                {
                    bstrNewParentUuid = ValueUnion.psz;
                    fSetNewParentUuid = true;
                }
                else
                    rc = E_FAIL;
                break;

            case 'S':   // --server
                bstrServer = ValueUnion.psz;
                break;

            case 'T':   // --target
                bstrTarget = ValueUnion.psz;
                break;

            case 'P':   // --tport
                bstrPort = ValueUnion.psz;
                break;

            case 'L':   // --lun
                bstrLun = ValueUnion.psz;
                break;

            case 'E':   // --encodedlun
                bstrLun = BstrFmt("enc%s", ValueUnion.psz);
                break;

            case 'U':   // --username
                bstrUsername = ValueUnion.psz;
                break;

            case 'W':   // --password
                bstrPassword = ValueUnion.psz;
                break;

            case 'N':   // --initiator
                bstrInitiator = ValueUnion.psz;
                break;

            case 'M':   // --type
            {
                int vrc = parseMediumType(ValueUnion.psz, &enmMediumType);
                if (RT_FAILURE(vrc))
                    return errorArgument("Invalid medium type '%s'", ValueUnion.psz);
                fSetMediumType = true;
                break;
            }

            case 'I':   // --intnet
                fIntNet = true;
                break;

            default:
            {
                errorGetOpt(USAGE_STORAGEATTACH, c, &ValueUnion);
                rc = E_FAIL;
                break;
            }
        }
    }

    if (FAILED(rc))
        return RTEXITCODE_FAILURE;

    if (!pszCtl)
        return errorSyntax(USAGE_STORAGEATTACH, "Storage controller name not specified");

    /* get the virtualbox system properties */
    CHECK_ERROR_RET(a->virtualBox, COMGETTER(SystemProperties)(systemProperties.asOutParam()), RTEXITCODE_FAILURE);

    // find the machine, lock it, get the mutable session machine
    CHECK_ERROR_RET(a->virtualBox, FindMachine(Bstr(a->argv[0]).raw(),
                                               machine.asOutParam()), RTEXITCODE_FAILURE);
    CHECK_ERROR_RET(machine, LockMachine(a->session, LockType_Shared), RTEXITCODE_FAILURE);
    SessionType_T st;
    CHECK_ERROR_RET(a->session, COMGETTER(Type)(&st), RTEXITCODE_FAILURE);
    a->session->COMGETTER(Machine)(machine.asOutParam());

    try
    {
        bool fRunTime = (st == SessionType_Shared);

        if (fRunTime)
        {
            if (pszPassThrough)
                throw Utf8Str("Drive passthrough state cannot be changed while the VM is running\n");
            else if (pszBandwidthGroup)
                throw Utf8Str("Bandwidth group cannot be changed while the VM is running\n");
        }

        /* check if the storage controller is present */
        rc = machine->GetStorageControllerByName(Bstr(pszCtl).raw(),
                                                 storageCtl.asOutParam());
        if (FAILED(rc))
            throw Utf8StrFmt("Could not find a controller named '%s'\n", pszCtl);

        StorageBus_T storageBus = StorageBus_Null;
        CHECK_ERROR_RET(storageCtl, COMGETTER(Bus)(&storageBus), RTEXITCODE_FAILURE);
        ULONG maxPorts = 0;
        CHECK_ERROR_RET(systemProperties, GetMaxPortCountForStorageBus(storageBus, &maxPorts), RTEXITCODE_FAILURE);
        ULONG maxDevices = 0;
        CHECK_ERROR_RET(systemProperties, GetMaxDevicesPerPortForStorageBus(storageBus, &maxDevices), RTEXITCODE_FAILURE);

        if (port == ~0U)
        {
            if (maxPorts == 1)
                port = 0;
            else
                return errorSyntax(USAGE_STORAGEATTACH, "Port not specified");
        }
        if (device == ~0U)
        {
            if (maxDevices == 1)
                device = 0;
            else
                return errorSyntax(USAGE_STORAGEATTACH, "Device not specified");
        }

        /* for sata controller check if the port count is big enough
         * to accommodate the current port which is being assigned
         * else just increase the port count
         */
        {
            ULONG ulPortCount = 0;
            ULONG ulMaxPortCount = 0;

            CHECK_ERROR(storageCtl, COMGETTER(MaxPortCount)(&ulMaxPortCount));
            CHECK_ERROR(storageCtl, COMGETTER(PortCount)(&ulPortCount));

            if (   (ulPortCount != ulMaxPortCount)
                && (port >= ulPortCount)
                && (port < ulMaxPortCount))
                CHECK_ERROR(storageCtl, COMSETTER(PortCount)(port + 1));
        }

        StorageControllerType_T ctlType = StorageControllerType_Null;
        CHECK_ERROR(storageCtl, COMGETTER(ControllerType)(&ctlType));

        if (!RTStrICmp(pszMedium, "none"))
        {
            CHECK_ERROR(machine, DetachDevice(Bstr(pszCtl).raw(), port, device));
        }
        else if (!RTStrICmp(pszMedium, "emptydrive"))
        {
            if (fRunTime)
            {
                ComPtr<IMediumAttachment> mediumAttachment;
                DeviceType_T deviceType = DeviceType_Null;
                rc = machine->GetMediumAttachment(Bstr(pszCtl).raw(), port, device,
                                                  mediumAttachment.asOutParam());
                if (SUCCEEDED(rc))
                {
                    mediumAttachment->COMGETTER(Type)(&deviceType);

                    if (   (deviceType == DeviceType_DVD)
                        || (deviceType == DeviceType_Floppy))
                    {
                        /* just unmount the floppy/dvd */
                        CHECK_ERROR(machine, UnmountMedium(Bstr(pszCtl).raw(),
                                                           port,
                                                           device,
                                                           fForceUnmount));
                    }
                }
                else if (devTypeRequested == DeviceType_DVD)
                {
                    /*
                     * Try to attach an empty DVD drive as a hotplug operation.
                     * Main will complain if the controller doesn't support hotplugging.
                     */
                    CHECK_ERROR(machine, AttachDeviceWithoutMedium(Bstr(pszCtl).raw(), port, device,
                                                                   devTypeRequested));
                    deviceType = DeviceType_DVD; /* To avoid the error message below. */
                }

                if (   FAILED(rc)
                    || !(   deviceType == DeviceType_DVD
                         || deviceType == DeviceType_Floppy)
                   )
                    throw Utf8StrFmt("No DVD/Floppy Drive attached to the controller '%s'"
                                     "at the port: %u, device: %u", pszCtl, port, device);

            }
            else
            {
                DeviceType_T deviceType = DeviceType_Null;
                com::SafeArray <DeviceType_T> saDeviceTypes;
                ULONG driveCheck = 0;

                /* check if the device type is supported by the controller */
                CHECK_ERROR(systemProperties, GetDeviceTypesForStorageBus(storageBus, ComSafeArrayAsOutParam(saDeviceTypes)));
                for (size_t i = 0; i < saDeviceTypes.size(); ++ i)
                {
                    if (   (saDeviceTypes[i] == DeviceType_DVD)
                        || (saDeviceTypes[i] == DeviceType_Floppy))
                        driveCheck++;
                }

                if (!driveCheck)
                    throw Utf8StrFmt("The attachment is not supported by the storage controller '%s'", pszCtl);

                if (storageBus == StorageBus_Floppy)
                    deviceType = DeviceType_Floppy;
                else
                    deviceType = DeviceType_DVD;

                /* attach a empty floppy/dvd drive after removing previous attachment */
                machine->DetachDevice(Bstr(pszCtl).raw(), port, device);
                CHECK_ERROR(machine, AttachDeviceWithoutMedium(Bstr(pszCtl).raw(), port, device,
                                                            deviceType));
            }
        } // end if (!RTStrICmp(pszMedium, "emptydrive"))
        else
        {
            ComPtr<IMedium> pMedium2Mount;

            // not "none", not "emptydrive": then it must be a UUID or filename or hostdrive or iSCSI;
            // for all these we first need to know the type of drive we're attaching to
            {
                /*
                 * try to determine the type of the drive from the
                 * storage controller chipset, the attachment and
                 * the medium being attached
                 */
                if (ctlType == StorageControllerType_I82078)        // floppy controller
                    devTypeRequested = DeviceType_Floppy;
                else
                {
                    /*
                     * for SATA/SCSI/IDE it is hard to tell if it is a harddisk or
                     * a dvd being attached so lets check if the medium attachment
                     * and the medium, both are of same type. if yes then we are
                     * sure of its type and don't need the user to enter it manually
                     * else ask the user for the type.
                     */
                    ComPtr<IMediumAttachment> mediumAttachment;
                    rc = machine->GetMediumAttachment(Bstr(pszCtl).raw(), port,
                                                      device,
                                                      mediumAttachment.asOutParam());
                    if (SUCCEEDED(rc))
                    {
                        DeviceType_T deviceType;
                        mediumAttachment->COMGETTER(Type)(&deviceType);

                        if (pszMedium)
                        {
                            if (!RTStrICmp(pszMedium, "additions"))
                            {
                                ComPtr<ISystemProperties> pProperties;
                                CHECK_ERROR(a->virtualBox,
                                            COMGETTER(SystemProperties)(pProperties.asOutParam()));
                                CHECK_ERROR(pProperties, COMGETTER(DefaultAdditionsISO)(bstrIso.asOutParam()));
                                strIso = Utf8Str(bstrIso);
                                if (strIso.isEmpty())
                                    throw Utf8Str("Cannot find the Guest Additions ISO image\n");
                                pszMedium = strIso.c_str();
                                if (devTypeRequested == DeviceType_Null)
                                    devTypeRequested = DeviceType_DVD;
                            }
                            ComPtr<IMedium> pExistingMedium;
                            rc = openMedium(a, pszMedium, deviceType,
                                            AccessMode_ReadWrite,
                                            pExistingMedium,
                                            false /* fForceNewUuidOnOpen */,
                                            true /* fSilent */);
                            if (SUCCEEDED(rc) && pExistingMedium)
                            {
                                if (    (deviceType == DeviceType_DVD)
                                     || (deviceType == DeviceType_HardDisk)
                                   )
                                    devTypeRequested = deviceType;
                            }
                        }
                        else
                            devTypeRequested = deviceType;
                    }
                }
            }

            if (devTypeRequested == DeviceType_Null)        // still the initializer value?
                throw Utf8Str("Argument --type must be specified\n");

            /* check if the device type is supported by the controller */
            {
                com::SafeArray <DeviceType_T> saDeviceTypes;

                CHECK_ERROR(systemProperties, GetDeviceTypesForStorageBus(storageBus, ComSafeArrayAsOutParam(saDeviceTypes)));
                if (SUCCEEDED(rc))
                {
                    ULONG driveCheck = 0;
                    for (size_t i = 0; i < saDeviceTypes.size(); ++ i)
                        if (saDeviceTypes[i] == devTypeRequested)
                            driveCheck++;
                    if (!driveCheck)
                        throw Utf8StrFmt("The given attachment is not supported by the storage controller '%s'", pszCtl);
                }
                else
                    goto leave;
            }

            // find the medium given
            /* host drive? */
            if (!RTStrNICmp(pszMedium, RT_STR_TUPLE("host:")))
            {
                ComPtr<IHost> host;
                CHECK_ERROR(a->virtualBox, COMGETTER(Host)(host.asOutParam()));

                if (devTypeRequested == DeviceType_DVD)
                {
                    rc = host->FindHostDVDDrive(Bstr(pszMedium + 5).raw(),
                                                pMedium2Mount.asOutParam());
                    if (!pMedium2Mount)
                    {
                        /* 2nd try: try with the real name, important on Linux+libhal */
                        char szPathReal[RTPATH_MAX];
                        if (RT_FAILURE(RTPathReal(pszMedium + 5, szPathReal, sizeof(szPathReal))))
                            throw Utf8StrFmt("Invalid host DVD drive name \"%s\"", pszMedium + 5);
                        rc = host->FindHostDVDDrive(Bstr(szPathReal).raw(),
                                                    pMedium2Mount.asOutParam());
                        if (!pMedium2Mount)
                            throw Utf8StrFmt("Invalid host DVD drive name \"%s\"", pszMedium + 5);
                    }
                }
                else
                {
                    // floppy
                    rc = host->FindHostFloppyDrive(Bstr(pszMedium + 5).raw(),
                                                   pMedium2Mount.asOutParam());
                    if (!pMedium2Mount)
                        throw Utf8StrFmt("Invalid host floppy drive name \"%s\"", pszMedium + 5);
                }
            }
            else if (!RTStrICmp(pszMedium, "iSCSI"))
            {
                /* check for required options */
                if (bstrServer.isEmpty() || bstrTarget.isEmpty())
                    throw Utf8StrFmt("Parameters --server and --target are required for iSCSI media");

                /** @todo move the location stuff to Main, which can use pfnComposeName
                 * from the disk backends to construct the location properly. Also do
                 * not use slashes to separate the parts, as otherwise only the last
                 * element containing information will be shown. */
                Bstr bstrISCSIMedium;
                if (    bstrLun.isEmpty()
                     || (bstrLun == "0")
                     || (bstrLun == "enc0")
                   )
                    bstrISCSIMedium = BstrFmt("%ls|%ls", bstrServer.raw(), bstrTarget.raw());
                else
                    bstrISCSIMedium = BstrFmt("%ls|%ls|%ls", bstrServer.raw(), bstrTarget.raw(), bstrLun.raw());

                CHECK_ERROR(a->virtualBox, CreateMedium(Bstr("iSCSI").raw(),
                                                        bstrISCSIMedium.raw(),
                                                        AccessMode_ReadWrite,
                                                        DeviceType_HardDisk,
                                                        pMedium2Mount.asOutParam()));
                if (FAILED(rc)) goto leave;
                if (!bstrPort.isEmpty())
                    bstrServer = BstrFmt("%ls:%ls", bstrServer.raw(), bstrPort.raw());

                // set the other iSCSI parameters as properties
                com::SafeArray <BSTR> names;
                com::SafeArray <BSTR> values;
                Bstr("TargetAddress").detachTo(names.appendedRaw());
                bstrServer.detachTo(values.appendedRaw());
                Bstr("TargetName").detachTo(names.appendedRaw());
                bstrTarget.detachTo(values.appendedRaw());

                if (!bstrLun.isEmpty())
                {
                    Bstr("LUN").detachTo(names.appendedRaw());
                    bstrLun.detachTo(values.appendedRaw());
                }
                if (!bstrUsername.isEmpty())
                {
                    Bstr("InitiatorUsername").detachTo(names.appendedRaw());
                    bstrUsername.detachTo(values.appendedRaw());
                }
                if (!bstrPassword.isEmpty())
                {
                    Bstr("InitiatorSecret").detachTo(names.appendedRaw());
                    bstrPassword.detachTo(values.appendedRaw());
                }
                if (!bstrInitiator.isEmpty())
                {
                    Bstr("InitiatorName").detachTo(names.appendedRaw());
                    bstrInitiator.detachTo(values.appendedRaw());
                }

                /// @todo add --targetName and --targetPassword options

                if (fIntNet)
                {
                    Bstr("HostIPStack").detachTo(names.appendedRaw());
                    Bstr("0").detachTo(values.appendedRaw());
                }

                CHECK_ERROR(pMedium2Mount, SetProperties(ComSafeArrayAsInParam(names),
                                                         ComSafeArrayAsInParam(values)));
                if (FAILED(rc)) goto leave;
                Bstr guid;
                CHECK_ERROR(pMedium2Mount, COMGETTER(Id)(guid.asOutParam()));
                if (FAILED(rc)) goto leave;
                RTPrintf("iSCSI disk created. UUID: %s\n", Utf8Str(guid).c_str());
            }
            else
            {
                if (!pszMedium)
                {
                    ComPtr<IMediumAttachment> mediumAttachment;
                    rc = machine->GetMediumAttachment(Bstr(pszCtl).raw(), port,
                                                      device,
                                                      mediumAttachment.asOutParam());
                    if (FAILED(rc))
                        throw Utf8Str("Missing --medium argument");
                }
                else
                {
                    Bstr bstrMedium(pszMedium);
                    rc = openMedium(a, pszMedium, devTypeRequested,
                                    AccessMode_ReadWrite, pMedium2Mount,
                                    fSetNewUuid, false /* fSilent */);
                    if (FAILED(rc) || !pMedium2Mount)
                        throw Utf8StrFmt("Invalid UUID or filename \"%s\"", pszMedium);
                }
            }

            // set medium/parent medium UUID, if so desired
            if (pMedium2Mount && (fSetNewUuid || fSetNewParentUuid))
            {
                CHECK_ERROR(pMedium2Mount, SetIds(fSetNewUuid, bstrNewUuid.raw(),
                                                  fSetNewParentUuid, bstrNewParentUuid.raw()));
                if (FAILED(rc))
                    throw  Utf8Str("Failed to set the medium/parent medium UUID");
            }

            // set medium type, if so desired
            if (pMedium2Mount && fSetMediumType)
            {
                MediumType_T enmMediumTypeOld;
                CHECK_ERROR(pMedium2Mount, COMGETTER(Type)(&enmMediumTypeOld));
                if (SUCCEEDED(rc))
                {
                    if (enmMediumTypeOld != enmMediumType)
                    {
                        CHECK_ERROR(pMedium2Mount, COMSETTER(Type)(enmMediumType));
                        if (FAILED(rc))
                            throw  Utf8Str("Failed to set the medium type");
                    }
                }
            }

            if (pMedium2Mount && !bstrComment.isEmpty())
            {
                CHECK_ERROR(pMedium2Mount, COMSETTER(Description)(bstrComment.raw()));
            }

            if (pszMedium)
            {
                switch (devTypeRequested)
                {
                    case DeviceType_DVD:
                    case DeviceType_Floppy:
                    {
                        if (!fRunTime)
                        {
                            ComPtr<IMediumAttachment> mediumAttachment;
                            // check if there is a dvd/floppy drive at the given location, if not attach one first
                            rc = machine->GetMediumAttachment(Bstr(pszCtl).raw(),
                                                              port,
                                                              device,
                                                              mediumAttachment.asOutParam());
                            if (SUCCEEDED(rc))
                            {
                                DeviceType_T deviceType;
                                mediumAttachment->COMGETTER(Type)(&deviceType);
                                if (deviceType != devTypeRequested)
                                {
                                    machine->DetachDevice(Bstr(pszCtl).raw(), port, device);
                                    rc = machine->AttachDeviceWithoutMedium(Bstr(pszCtl).raw(),
                                                                            port,
                                                                            device,
                                                                            devTypeRequested);    // DeviceType_DVD or DeviceType_Floppy
                                }
                            }
                            else
                            {
                                rc = machine->AttachDeviceWithoutMedium(Bstr(pszCtl).raw(),
                                                                        port,
                                                                        device,
                                                                        devTypeRequested);    // DeviceType_DVD or DeviceType_Floppy
                            }
                        }

                        if (pMedium2Mount)
                        {
                            CHECK_ERROR(machine, MountMedium(Bstr(pszCtl).raw(),
                                                             port,
                                                             device,
                                                             pMedium2Mount,
                                                             fForceUnmount));
                        }
                        break;
                    } // end DeviceType_DVD or DeviceType_Floppy:

                    case DeviceType_HardDisk:
                    {
                        // if there is anything attached at the given location, remove it
                        machine->DetachDevice(Bstr(pszCtl).raw(), port, device);
                        CHECK_ERROR(machine, AttachDevice(Bstr(pszCtl).raw(),
                                                          port,
                                                          device,
                                                          DeviceType_HardDisk,
                                                          pMedium2Mount));
                        break;
                    }

                    default: break; /* Shut up MSC */
                }
            }
        }

        if (   pszPassThrough
            && (SUCCEEDED(rc)))
        {
            ComPtr<IMediumAttachment> mattach;
            CHECK_ERROR(machine, GetMediumAttachment(Bstr(pszCtl).raw(), port,
                                                     device, mattach.asOutParam()));

            if (SUCCEEDED(rc))
            {
                if (!RTStrICmp(pszPassThrough, "on"))
                {
                    CHECK_ERROR(machine, PassthroughDevice(Bstr(pszCtl).raw(),
                                                           port, device, TRUE));
                }
                else if (!RTStrICmp(pszPassThrough, "off"))
                {
                    CHECK_ERROR(machine, PassthroughDevice(Bstr(pszCtl).raw(),
                                                           port, device, FALSE));
                }
                else
                    throw Utf8StrFmt("Invalid --passthrough argument '%s'", pszPassThrough);
            }
            else
                throw Utf8StrFmt("Couldn't find the controller attachment for the controller '%s'\n", pszCtl);
        }

        if (   pszTempEject
            && (SUCCEEDED(rc)))
        {
            ComPtr<IMediumAttachment> mattach;
            CHECK_ERROR(machine, GetMediumAttachment(Bstr(pszCtl).raw(), port,
                                                     device, mattach.asOutParam()));

            if (SUCCEEDED(rc))
            {
                if (!RTStrICmp(pszTempEject, "on"))
                {
                    CHECK_ERROR(machine, TemporaryEjectDevice(Bstr(pszCtl).raw(),
                                                              port, device, TRUE));
                }
                else if (!RTStrICmp(pszTempEject, "off"))
                {
                    CHECK_ERROR(machine, TemporaryEjectDevice(Bstr(pszCtl).raw(),
                                                              port, device, FALSE));
                }
                else
                    throw Utf8StrFmt("Invalid --tempeject argument '%s'", pszTempEject);
            }
            else
                throw Utf8StrFmt("Couldn't find the controller attachment for the controller '%s'\n", pszCtl);
        }

        if (   pszNonRotational
            && (SUCCEEDED(rc)))
        {
            ComPtr<IMediumAttachment> mattach;
            CHECK_ERROR(machine, GetMediumAttachment(Bstr(pszCtl).raw(), port,
                                                     device, mattach.asOutParam()));

            if (SUCCEEDED(rc))
            {
                if (!RTStrICmp(pszNonRotational, "on"))
                {
                    CHECK_ERROR(machine, NonRotationalDevice(Bstr(pszCtl).raw(),
                                                             port, device, TRUE));
                }
                else if (!RTStrICmp(pszNonRotational, "off"))
                {
                    CHECK_ERROR(machine, NonRotationalDevice(Bstr(pszCtl).raw(),
                                                             port, device, FALSE));
                }
                else
                    throw Utf8StrFmt("Invalid --nonrotational argument '%s'", pszNonRotational);
            }
            else
                throw Utf8StrFmt("Couldn't find the controller attachment for the controller '%s'\n", pszCtl);
        }

        if (   pszDiscard
            && (SUCCEEDED(rc)))
        {
            ComPtr<IMediumAttachment> mattach;
            CHECK_ERROR(machine, GetMediumAttachment(Bstr(pszCtl).raw(), port,
                                                     device, mattach.asOutParam()));

            if (SUCCEEDED(rc))
            {
                if (!RTStrICmp(pszDiscard, "on"))
                {
                    CHECK_ERROR(machine, SetAutoDiscardForDevice(Bstr(pszCtl).raw(),
                                                                 port, device, TRUE));
                }
                else if (!RTStrICmp(pszDiscard, "off"))
                {
                    CHECK_ERROR(machine, SetAutoDiscardForDevice(Bstr(pszCtl).raw(),
                                                                 port, device, FALSE));
                }
                else
                    throw Utf8StrFmt("Invalid --discard argument '%s'", pszDiscard);
            }
            else
                throw Utf8StrFmt("Couldn't find the controller attachment for the controller '%s'\n", pszCtl);
        }

        if (   pszHotPluggable
            && (SUCCEEDED(rc)))
        {
            ComPtr<IMediumAttachment> mattach;
            CHECK_ERROR(machine, GetMediumAttachment(Bstr(pszCtl).raw(), port,
                                                     device, mattach.asOutParam()));

            if (SUCCEEDED(rc))
            {
                if (!RTStrICmp(pszHotPluggable, "on"))
                {
                    CHECK_ERROR(machine, SetHotPluggableForDevice(Bstr(pszCtl).raw(),
                                                                  port, device, TRUE));
                }
                else if (!RTStrICmp(pszHotPluggable, "off"))
                {
                    CHECK_ERROR(machine, SetHotPluggableForDevice(Bstr(pszCtl).raw(),
                                                                  port, device, FALSE));
                }
                else
                    throw Utf8StrFmt("Invalid --hotpluggable argument '%s'", pszHotPluggable);
            }
            else
                throw Utf8StrFmt("Couldn't find the controller attachment for the controller '%s'\n", pszCtl);
        }

        if (   pszBandwidthGroup
            && !fRunTime
            && SUCCEEDED(rc))
        {

            if (!RTStrICmp(pszBandwidthGroup, "none"))
            {
                /* Just remove the bandwidth gorup. */
                CHECK_ERROR(machine, SetNoBandwidthGroupForDevice(Bstr(pszCtl).raw(),
                                                                  port, device));
            }
            else
            {
                ComPtr<IBandwidthControl> bwCtrl;
                ComPtr<IBandwidthGroup> bwGroup;

                CHECK_ERROR(machine, COMGETTER(BandwidthControl)(bwCtrl.asOutParam()));

                if (SUCCEEDED(rc))
                {
                    CHECK_ERROR(bwCtrl, GetBandwidthGroup(Bstr(pszBandwidthGroup).raw(), bwGroup.asOutParam()));
                    if (SUCCEEDED(rc))
                    {
                        CHECK_ERROR(machine, SetBandwidthGroupForDevice(Bstr(pszCtl).raw(),
                                                                        port, device, bwGroup));
                    }
                }
            }
        }

        /* commit changes */
        if (SUCCEEDED(rc))
            CHECK_ERROR(machine, SaveSettings());
    }
    catch (const Utf8Str &strError)
    {
        errorArgument("%s", strError.c_str());
        rc = E_FAIL;
    }

    // machine must always be unlocked, even on errors
leave:
    a->session->UnlockMachine();

    return SUCCEEDED(rc) ? RTEXITCODE_SUCCESS : RTEXITCODE_FAILURE;
}


static const RTGETOPTDEF g_aStorageControllerOptions[] =
{
    { "--name",             'n', RTGETOPT_REQ_STRING },
    { "--add",              'a', RTGETOPT_REQ_STRING },
    { "--controller",       'c', RTGETOPT_REQ_STRING },
    { "--portcount",        'p', RTGETOPT_REQ_UINT32 },
    { "--remove",           'r', RTGETOPT_REQ_NOTHING },
    { "--rename",           'R', RTGETOPT_REQ_STRING },
    { "--hostiocache",      'i', RTGETOPT_REQ_STRING },
    { "--bootable",         'b', RTGETOPT_REQ_STRING },
};

RTEXITCODE handleStorageController(HandlerArg *a)
{
    int               c;
    const char       *pszCtl         = NULL;
    const char       *pszBusType     = NULL;
    const char       *pszCtlType     = NULL;
    const char       *pszHostIOCache = NULL;
    const char       *pszBootable    = NULL;
    const char       *pszCtlNewName  = NULL;
    ULONG             portcount      = ~0U;
    bool              fRemoveCtl     = false;
    ComPtr<IMachine>  machine;
    RTGETOPTUNION     ValueUnion;
    RTGETOPTSTATE     GetState;

    if (a->argc < 4)
        return errorSyntax(USAGE_STORAGECONTROLLER, "Too few parameters");

    RTGetOptInit (&GetState, a->argc, a->argv, g_aStorageControllerOptions,
                  RT_ELEMENTS(g_aStorageControllerOptions), 1, RTGETOPTINIT_FLAGS_NO_STD_OPTS);

    while ((c = RTGetOpt(&GetState, &ValueUnion)) != 0)
    {
        switch (c)
        {
            case 'n':   // controller name
                Assert(ValueUnion.psz);
                pszCtl = ValueUnion.psz;
                break;

            case 'a':   // controller bus type <ide/sata/scsi/floppy>
                Assert(ValueUnion.psz);
                pszBusType = ValueUnion.psz;
                break;

            case 'c':   // controller <lsilogic/buslogic/intelahci/piix3/piix4/ich6/i82078>
                Assert(ValueUnion.psz);
                pszCtlType = ValueUnion.psz;
                break;

            case 'p':   // portcount
                portcount = ValueUnion.u32;
                break;

            case 'r':   // remove controller
                fRemoveCtl = true;
                break;

            case 'R':   // rename controller
                Assert(ValueUnion.psz);
                pszCtlNewName = ValueUnion.psz;
                break;

            case 'i':
                pszHostIOCache = ValueUnion.psz;
                break;

            case 'b':
                pszBootable = ValueUnion.psz;
                break;

            default:
                return errorGetOpt(USAGE_STORAGECONTROLLER, c, &ValueUnion);
        }
    }

    HRESULT rc;

    /* try to find the given machine */
    CHECK_ERROR_RET(a->virtualBox, FindMachine(Bstr(a->argv[0]).raw(),
                                               machine.asOutParam()), RTEXITCODE_FAILURE);

    /* open a session for the VM */
    CHECK_ERROR_RET(machine, LockMachine(a->session, LockType_Write), RTEXITCODE_FAILURE);

    /* get the mutable session machine */
    a->session->COMGETTER(Machine)(machine.asOutParam());

    if (!pszCtl)
    {
        /* it's important to always close sessions */
        a->session->UnlockMachine();
        return errorSyntax(USAGE_STORAGECONTROLLER, "Storage controller name not specified\n");
    }

    if (fRemoveCtl)
    {
        CHECK_ERROR(machine, RemoveStorageController(Bstr(pszCtl).raw()));
    }
    else
    {
        if (pszBusType)
        {
            ComPtr<IStorageController> ctl;

            if (!RTStrICmp(pszBusType, "ide"))
            {
                CHECK_ERROR(machine, AddStorageController(Bstr(pszCtl).raw(),
                                                          StorageBus_IDE,
                                                          ctl.asOutParam()));
            }
            else if (!RTStrICmp(pszBusType, "sata"))
            {
                CHECK_ERROR(machine, AddStorageController(Bstr(pszCtl).raw(),
                                                          StorageBus_SATA,
                                                          ctl.asOutParam()));
            }
            else if (!RTStrICmp(pszBusType, "scsi"))
            {
                CHECK_ERROR(machine, AddStorageController(Bstr(pszCtl).raw(),
                                                          StorageBus_SCSI,
                                                          ctl.asOutParam()));
            }
            else if (!RTStrICmp(pszBusType, "floppy"))
            {
                CHECK_ERROR(machine, AddStorageController(Bstr(pszCtl).raw(),
                                                          StorageBus_Floppy,
                                                          ctl.asOutParam()));
            }
            else if (!RTStrICmp(pszBusType, "sas"))
            {
                CHECK_ERROR(machine, AddStorageController(Bstr(pszCtl).raw(),
                                                          StorageBus_SAS,
                                                          ctl.asOutParam()));
            }
            else if (!RTStrICmp(pszBusType, "usb"))
            {
                CHECK_ERROR(machine, AddStorageController(Bstr(pszCtl).raw(),
                                                          StorageBus_USB,
                                                          ctl.asOutParam()));
            }
            else if (!RTStrICmp(pszBusType, "pcie"))
            {
                CHECK_ERROR(machine, AddStorageController(Bstr(pszCtl).raw(),
                                                          StorageBus_PCIe,
                                                          ctl.asOutParam()));
            }
            else
            {
                errorArgument("Invalid --add argument '%s'", pszBusType);
                rc = E_FAIL;
            }
        }

        if (   pszCtlType
            && SUCCEEDED(rc))
        {
            ComPtr<IStorageController> ctl;

            CHECK_ERROR(machine, GetStorageControllerByName(Bstr(pszCtl).raw(),
                                                            ctl.asOutParam()));

            if (SUCCEEDED(rc))
            {
                if (!RTStrICmp(pszCtlType, "lsilogic"))
                {
                    CHECK_ERROR(ctl, COMSETTER(ControllerType)(StorageControllerType_LsiLogic));
                }
                else if (!RTStrICmp(pszCtlType, "buslogic"))
                {
                    CHECK_ERROR(ctl, COMSETTER(ControllerType)(StorageControllerType_BusLogic));
                }
                else if (!RTStrICmp(pszCtlType, "intelahci"))
                {
                    CHECK_ERROR(ctl, COMSETTER(ControllerType)(StorageControllerType_IntelAhci));
                }
                else if (!RTStrICmp(pszCtlType, "piix3"))
                {
                    CHECK_ERROR(ctl, COMSETTER(ControllerType)(StorageControllerType_PIIX3));
                }
                else if (!RTStrICmp(pszCtlType, "piix4"))
                {
                    CHECK_ERROR(ctl, COMSETTER(ControllerType)(StorageControllerType_PIIX4));
                }
                else if (!RTStrICmp(pszCtlType, "ich6"))
                {
                    CHECK_ERROR(ctl, COMSETTER(ControllerType)(StorageControllerType_ICH6));
                }
                else if (!RTStrICmp(pszCtlType, "i82078"))
                {
                    CHECK_ERROR(ctl, COMSETTER(ControllerType)(StorageControllerType_I82078));
                }
                else if (!RTStrICmp(pszCtlType, "lsilogicsas"))
                {
                    CHECK_ERROR(ctl, COMSETTER(ControllerType)(StorageControllerType_LsiLogicSas));
                }
                else if (!RTStrICmp(pszCtlType, "usb"))
                {
                    CHECK_ERROR(ctl, COMSETTER(ControllerType)(StorageControllerType_USB));
                }
                else if (!RTStrICmp(pszCtlType, "nvme"))
                {
                    CHECK_ERROR(ctl, COMSETTER(ControllerType)(StorageControllerType_NVMe));
                }
                else
                {
                    errorArgument("Invalid --type argument '%s'", pszCtlType);
                    rc = E_FAIL;
                }
            }
            else
            {
                errorArgument("Couldn't find the controller with the name: '%s'\n", pszCtl);
                rc = E_FAIL;
            }
        }

        if (   (portcount != ~0U)
            && SUCCEEDED(rc))
        {
            ComPtr<IStorageController> ctl;

            CHECK_ERROR(machine, GetStorageControllerByName(Bstr(pszCtl).raw(),
                                                            ctl.asOutParam()));

            if (SUCCEEDED(rc))
            {
                CHECK_ERROR(ctl, COMSETTER(PortCount)(portcount));
            }
            else
            {
                errorArgument("Couldn't find the controller with the name: '%s'\n", pszCtl);
                rc = E_FAIL;
            }
        }

        if (   pszHostIOCache
            && SUCCEEDED(rc))
        {
            ComPtr<IStorageController> ctl;

            CHECK_ERROR(machine, GetStorageControllerByName(Bstr(pszCtl).raw(),
                                                            ctl.asOutParam()));

            if (SUCCEEDED(rc))
            {
                if (!RTStrICmp(pszHostIOCache, "on"))
                {
                    CHECK_ERROR(ctl, COMSETTER(UseHostIOCache)(TRUE));
                }
                else if (!RTStrICmp(pszHostIOCache, "off"))
                {
                    CHECK_ERROR(ctl, COMSETTER(UseHostIOCache)(FALSE));
                }
                else
                {
                    errorArgument("Invalid --hostiocache argument '%s'", pszHostIOCache);
                    rc = E_FAIL;
                }
            }
            else
            {
                errorArgument("Couldn't find the controller with the name: '%s'\n", pszCtl);
                rc = E_FAIL;
            }
        }

        if (   pszBootable
            && SUCCEEDED(rc))
        {
            if (SUCCEEDED(rc))
            {
                if (!RTStrICmp(pszBootable, "on"))
                {
                    CHECK_ERROR(machine, SetStorageControllerBootable(Bstr(pszCtl).raw(), TRUE));
                }
                else if (!RTStrICmp(pszBootable, "off"))
                {
                    CHECK_ERROR(machine, SetStorageControllerBootable(Bstr(pszCtl).raw(), FALSE));
                }
                else
                {
                    errorArgument("Invalid --bootable argument '%s'", pszBootable);
                    rc = E_FAIL;
                }
            }
            else
            {
                errorArgument("Couldn't find the controller with the name: '%s'\n", pszCtl);
                rc = E_FAIL;
            }
        }

        if (   pszCtlNewName
            && SUCCEEDED(rc))
        {
            ComPtr<IStorageController> ctl;

            CHECK_ERROR(machine, GetStorageControllerByName(Bstr(pszCtl).raw(),
                                                            ctl.asOutParam()));

            if (SUCCEEDED(rc))
            {
                CHECK_ERROR(ctl, COMSETTER(Name)(Bstr(pszCtlNewName).raw()));
            }
            else
            {
                errorArgument("Couldn't find the controller with the name: '%s'\n", pszCtl);
                rc = E_FAIL;
            }
        }

    }

    /* commit changes */
    if (SUCCEEDED(rc))
        CHECK_ERROR(machine, SaveSettings());

    /* it's important to always close sessions */
    a->session->UnlockMachine();

    return SUCCEEDED(rc) ? RTEXITCODE_SUCCESS : RTEXITCODE_FAILURE;
}

#endif /* !VBOX_ONLY_DOCS */

