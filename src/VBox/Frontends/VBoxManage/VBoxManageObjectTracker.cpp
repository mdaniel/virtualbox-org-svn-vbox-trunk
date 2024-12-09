/* $Id$ */
/** @file
 * VBoxManageObjectTracker - The object tracker related commands.
 */

/*
 * Copyright (C) 2006-2024 Oracle and/or its affiliates.
 *
 * This file is part of VirtualBox base platform packages, as
 * available from https://www.virtualbox.org.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, in version 3 of the
 * License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <https://www.gnu.org/licenses>.
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "VBoxManage.h"

#include <map>
#include <vector>
#include <iprt/time.h>

using namespace com;
using namespace std;

enum supIfaces_T
{
    None = 0,
    kProgress = 1,
    kSession,
    kMedium,
    kMachine,
};

std::map <com::Utf8Str, supIfaces_T> mapInterfaceNameToEnum = {
    {"IProgress", kProgress},
    {"ISession", kSession},
    {"IMedium", kMedium},
    {"IMachine", kMachine}
};

static void printProgressObjectInfo(const ComPtr<IProgress>& pObj);
static void printSessionObjectInfo(const ComPtr<ISession>& pObj);
static void printMediumObjectInfo(const ComPtr<IMedium>& pObj);
static void printMachineObjectInfo(const ComPtr<IMachine>& pObj);

static void makeTimeStr(char *s, int cb, int64_t millies)
{
    RTTIME t;
    RTTIMESPEC ts;

    RTTimeSpecSetMilli(&ts, millies);

    RTTimeExplode(&t, &ts);

    RTStrPrintf(s, cb, "%04d/%02d/%02d %02d:%02d:%02d UTC",
                        t.i32Year, t.u8Month, t.u8MonthDay,
                        t.u8Hour, t.u8Minute, t.u8Second);
}

static Utf8Str trackedObjectStateToStr(TrackedObjectState_T aState)
{
    Utf8Str strState("None");
    switch (aState)
    {
        case TrackedObjectState_Alive:
            strState = "Alive";
            break;
        case TrackedObjectState_Deleted:
            strState = "Deleted";
            break;
        case TrackedObjectState_Invalid:
            strState = "Invalid";
            break;
        case TrackedObjectState_None:
        default:
            strState = "None";
            break;
    }

    return strState;
}

struct TrackedObjInfo_T
{
    ComPtr<IUnknown> pIUnknown;
    TrackedObjectState_T enmState;
    LONG64 creationTime;
    LONG64 deletionTime;
};

struct TrackedObjInfoShow
{
    supIfaces_T m_iface;
    Utf8Str m_id;
    TrackedObjInfo_T m_objInfo;

    TrackedObjInfoShow(Utf8Str aId, const TrackedObjInfo_T& aObjInfo, supIfaces_T aIface):
        m_iface(aIface),
        m_id(aId)
    {
        m_objInfo = aObjInfo;
    }

    TrackedObjInfoShow(supIfaces_T aIface): m_iface(aIface) {}

    void setObjId(const Utf8Str& aId)
    {
        m_id = aId;
    }

    void setObjInfo(const TrackedObjInfo_T& aObjInfo)
    {
        m_objInfo = aObjInfo;
    }

    void show() const
    {
        RTPrintf(("\nTracked object id: %s\n"), m_id.c_str());

        Utf8Str strState = trackedObjectStateToStr(m_objInfo.enmState);
        RTPrintf(("  State                   %s\n"), strState.c_str());

        char szTimeValue[128];
        makeTimeStr(szTimeValue, sizeof(szTimeValue), m_objInfo.creationTime);
        RTPrintf(("  Creation time           %s\n"), szTimeValue);

        if (m_objInfo.deletionTime != 0)
        {
            makeTimeStr(szTimeValue, sizeof(szTimeValue), m_objInfo.deletionTime);
            RTPrintf(("  Deletion time           %s\n"), szTimeValue);
        }

        switch (m_iface)
        {
            case kProgress:
            {
                if (m_objInfo.enmState != TrackedObjectState_Invalid)
                {
                    ComPtr<IProgress> pObj;
                    m_objInfo.pIUnknown->QueryInterface(IID_IProgress, (void **)pObj.asOutParam());
                    if (pObj.isNotNull())
                        printProgressObjectInfo(pObj);
                }
                break;
            }
            case kSession:
            {
                if (m_objInfo.enmState != TrackedObjectState_Invalid)
                {
                    ComPtr<ISession> pObj;
                    m_objInfo.pIUnknown->QueryInterface(IID_ISession, (void **)pObj.asOutParam());
                    if (pObj.isNotNull())
                        printSessionObjectInfo(pObj);
                }
                break;
            }
            case kMedium:
            {
                if (m_objInfo.enmState != TrackedObjectState_Invalid)
                {
                    ComPtr<IMedium> pObj;
                    m_objInfo.pIUnknown->QueryInterface(IID_IMedium, (void **)pObj.asOutParam());
                    if (pObj.isNotNull())
                        printMediumObjectInfo(pObj);
                }
                break;
            }
            case kMachine:
            {
                if (m_objInfo.enmState != TrackedObjectState_Invalid)
                {
                    ComPtr<IMachine> pObj;
                    m_objInfo.pIUnknown->QueryInterface(IID_IMachine, (void **)pObj.asOutParam());
                    if(pObj.isNotNull())
                        printMachineObjectInfo(pObj);
                }
                break;
            }
            /* Impossible situation but to support the default case */
            default:
                RTPrintf("Interface isn't supported by object tracker at moment");
        }
    }
};

static void printProgressObjectInfo(const ComPtr<IProgress>& pObj)
{
    if (pObj.isNotNull())
    {
        RTPrintf(("Progress:\n"));
        Bstr bStrAttr;
        pObj->COMGETTER(Id)(bStrAttr.asOutParam());
        RTPrintf(("  Id                       %s\n"), Utf8Str(bStrAttr).c_str());

        pObj->COMGETTER(Description)(bStrAttr.asOutParam());
        RTPrintf(("  Description              %s\n"), Utf8Str(bStrAttr).c_str());

        BOOL fBoolAttr;
        pObj->COMGETTER(Completed)(&fBoolAttr);
        RTPrintf(("  Completed                %s\n"), fBoolAttr ? "True" : "False");

        pObj->COMGETTER(Canceled)(&fBoolAttr);
        RTPrintf(("  Canceled                 %s\n"), fBoolAttr ? "True" : "False");
    }
}

static void printSessionObjectInfo(const ComPtr<ISession>& pObj)
{
    if (pObj.isNotNull())
    {
        RTPrintf(("Session:\n"));
        Bstr bStrAttr;
        pObj->COMGETTER(Name)(bStrAttr.asOutParam());
        RTPrintf(("  Name               %s\n"), Utf8Str(bStrAttr).c_str());

        SessionState_T enmState;
        pObj->COMGETTER(State)(&enmState);
        RTPrintf(("  State              %u\n"), enmState);

        SessionType_T enmType;
        pObj->COMGETTER(Type)(&enmType);
        RTPrintf(("  Type               %u\n"), enmType);
    }
}

static void printMediumObjectInfo(const ComPtr<IMedium>& pObj)
{
    if (pObj.isNotNull())
    {
        RTPrintf(("Medium:\n"));
        Bstr bStrAttr;
        pObj->COMGETTER(Id)(bStrAttr.asOutParam());
        RTPrintf(("  Medium Id               %s\n"), Utf8Str(bStrAttr).c_str());

        pObj->COMGETTER(Name)(bStrAttr.asOutParam());
        RTPrintf(("  Name                    %s\n"), Utf8Str(bStrAttr).c_str());

        pObj->COMGETTER(Description)(bStrAttr.asOutParam());
        RTPrintf(("  Description             %s\n"), Utf8Str(bStrAttr).c_str());

        pObj->COMGETTER(Location)(bStrAttr.asOutParam());
        RTPrintf(("  Location                %s\n"), Utf8Str(bStrAttr).c_str());

        pObj->COMGETTER(Format)(bStrAttr.asOutParam());
        RTPrintf(("  Format                  %s\n"), Utf8Str(bStrAttr).c_str());

        pObj->COMGETTER(LastAccessError)(bStrAttr.asOutParam());
        RTPrintf(("  LastAccessError         %s\n"), Utf8Str(bStrAttr).c_str());

        MediumState_T enmState;
        pObj->RefreshState(&enmState);
        /* check for accessibility */
        const char *pszState = "unknown";
        switch (enmState)
        {
            case MediumState_NotCreated:
                pszState = "not created";
                break;
            case MediumState_Created:
                pszState = "created";
                break;
            case MediumState_LockedRead:
                pszState = "locked read";
                break;
            case MediumState_LockedWrite:
                pszState = "locked write";
                break;
            case MediumState_Inaccessible:
                pszState = "inaccessible";
                break;
            case MediumState_Creating:
                pszState = "creating";
                break;
            case MediumState_Deleting:
                pszState = "deleting";
                break;
#ifdef VBOX_WITH_XPCOM_CPP_ENUM_HACK
            case MediumState_32BitHack: break; /* Shut up compiler warnings. */
#endif
        }
        RTPrintf(("  State                   %s\n"), pszState);

        BOOL fBoolAttr;
        pObj->COMGETTER(HostDrive)(&fBoolAttr);
        RTPrintf(("  HostDrive               %s\n"), fBoolAttr ? "True" : "False");

        LONG64 nLongIntAttr;
        pObj->COMGETTER(LogicalSize)(&nLongIntAttr);
        RTPrintf(("  Logical Size (MB)       %ld\n"), nLongIntAttr / _1M);

        pObj->COMGETTER(Size)(&nLongIntAttr);
        RTPrintf(("  Real Size (MB)          %ld\n"), nLongIntAttr / _1M);
    }
}

static void printMachineObjectInfo(const ComPtr<IMachine>& pObj)
{
    if (pObj.isNotNull())
    {
        RTPrintf(("Machine:\n"));

        Bstr bStrAttr;
        pObj->COMGETTER(Name)(bStrAttr.asOutParam());
        RTPrintf(("  Name                    %s\n"), Utf8Str(bStrAttr).c_str());

        BOOL fBoolAttr;
        pObj->COMGETTER(Accessible)(&fBoolAttr);
        RTPrintf(("  Accessible              %s\n"), fBoolAttr ? "True" : "False");

        pObj->COMGETTER(Id)(bStrAttr.asOutParam());
        RTPrintf(("  Machine Id              %s\n"), Utf8Str(bStrAttr).c_str());

        pObj->COMGETTER(Description)(bStrAttr.asOutParam());
        RTPrintf(("  Description             %s\n"), Utf8Str(bStrAttr).c_str());

        pObj->COMGETTER(OSTypeId)(bStrAttr.asOutParam());
        RTPrintf(("  OSTypeId                %s\n"), Utf8Str(bStrAttr).c_str());

        pObj->COMGETTER(HardwareVersion)(bStrAttr.asOutParam());
        RTPrintf(("  HardwareVersion         %s\n"), Utf8Str(bStrAttr).c_str());

        pObj->COMGETTER(HardwareUUID)(bStrAttr.asOutParam());
        RTPrintf(("  HardwareUUID            %s\n"), Utf8Str(bStrAttr).c_str());

        pObj->COMGETTER(SnapshotFolder)(bStrAttr.asOutParam());
        RTPrintf(("  SnapshotFolder          %s\n"), Utf8Str(bStrAttr).c_str());

        pObj->COMGETTER(SettingsFilePath)(bStrAttr.asOutParam());
        RTPrintf(("  SettingsFilePath        %s\n"), Utf8Str(bStrAttr).c_str());

        pObj->COMGETTER(StateFilePath)(bStrAttr.asOutParam());
        RTPrintf(("  StateFilePath           %s\n"), Utf8Str(bStrAttr).c_str());

        pObj->COMGETTER(LogFolder)(bStrAttr.asOutParam());
        RTPrintf(("  LogFolder               %s\n"), Utf8Str(bStrAttr).c_str());

        ULONG nIntAttr;
        pObj->COMGETTER(CPUCount)(&nIntAttr);
        RTPrintf(("  CPUCount                %ld\n"), nIntAttr);

        pObj->COMGETTER(MemorySize)(&nIntAttr);
        RTPrintf(("  Memory Size (MB)        %ld\n"), nIntAttr);

        MachineState_T enmState;
        pObj->COMGETTER(State)(&enmState);
        const char *pszState = machineStateToName(enmState, true);
        RTPrintf(("  State                   %s\n"), pszState);
    }
}

void printTrackedObjectInfo(supIfaces_T aIface, const map < Bstr, TrackedObjInfo_T >& aObjMap)
{
    TrackedObjInfoShow trackedObjInfoShow(aIface);
    for (const pair< const Bstr, TrackedObjInfo_T >& item : aObjMap)
    {
        trackedObjInfoShow.setObjId(Utf8Str(item.first));
        trackedObjInfoShow.setObjInfo(item.second);
        trackedObjInfoShow.show();
    }

}

static RTEXITCODE handleObjInfo(HandlerArg *a, int iFirst)
{
    HRESULT hrc = S_OK;

    static const RTGETOPTDEF s_aOptions[] =
    {
        { "--id",        'i', RTGETOPT_REQ_STRING },
        { "--ifacename", 'f', RTGETOPT_REQ_STRING },
        { "help",        'h', RTGETOPT_REQ_NOTHING },
        { "--help",      'h', RTGETOPT_REQ_NOTHING }
    };
    RTGETOPTSTATE GetState;
    RTGETOPTUNION ValueUnion;
    int vrc = RTGetOptInit(&GetState, a->argc, a->argv, s_aOptions, RT_ELEMENTS(s_aOptions), iFirst, 0);
    AssertRCReturn(vrc, RTEXITCODE_FAILURE);
    if (a->argc == iFirst)
    {
        RTPrintf(("Empty command parameter list, show help.\n"));
        printHelp(g_pStdOut);
        return RTEXITCODE_SUCCESS;
    }

    ComPtr<IVirtualBox> pVirtualBox = a->virtualBox;

    Utf8Str strObjUuid;
    Utf8Str strIfaceName;

    int c;
    while ((c = RTGetOpt(&GetState, &ValueUnion)) != 0)
    {
        switch (c)
        {
            case 'i':
            {
                if (strObjUuid.isNotEmpty())
                    return errorArgument("Duplicate parameter: --id");

                strObjUuid = ValueUnion.psz;
                if (strObjUuid.isEmpty())
                    return errorArgument("Empty parameter: --id");

                break;
            }
            case 'f':
            {
                if (strIfaceName.isNotEmpty())
                    return errorArgument("Duplicate parameter: --ifacename");

                strIfaceName = ValueUnion.psz;
                if (strIfaceName.isEmpty())
                    return errorArgument("Empty parameter: --ifacename");

                break;
            }
            case 'h':
                printHelp(g_pStdOut);
                return RTEXITCODE_SUCCESS;
            case VINF_GETOPT_NOT_OPTION:
                return errorUnknownSubcommand(ValueUnion.psz);

            default:
                return errorGetOpt(c, &ValueUnion);
        }
    }

    map <com::Utf8Str, supIfaces_T>::const_iterator cIt = mapInterfaceNameToEnum.find(strIfaceName);
    if (cIt != mapInterfaceNameToEnum.end())
    {
        supIfaces_T foundIface = cIt->second;
        com::SafeArray<BSTR> ObjIDsList;
        hrc = pVirtualBox->GetTrackedObjectIds(Bstr(cIt->first).raw(), ComSafeArrayAsOutParam(ObjIDsList));
        if (SUCCEEDED(hrc))
        {
            map < Bstr, TrackedObjInfo_T > lObjInfoMap;

            if (strObjUuid.isNotEmpty())
            {
                for (size_t i = 0; i < ObjIDsList.size(); ++i)
                {
                    Bstr bstrObjId = ObjIDsList[i];
                    if (bstrObjId.equals(strObjUuid.c_str()))
                    {
                        TrackedObjInfo_T objInfo;
                        hrc = pVirtualBox->GetTrackedObject(bstrObjId.raw(),
                                                            objInfo.pIUnknown.asOutParam(),
                                                            &objInfo.enmState,
                                                            &objInfo.creationTime,
                                                            &objInfo.deletionTime);
                        lObjInfoMap[bstrObjId] = objInfo;
                        break;
                    }
                }
            }
            else
            {
                for (size_t i = 0; i < ObjIDsList.size(); ++i)
                {
                    Bstr bstrObjId = ObjIDsList[i];
                    TrackedObjInfo_T objInfo;
                    hrc = pVirtualBox->GetTrackedObject(bstrObjId.raw(),
                                                        objInfo.pIUnknown.asOutParam(),
                                                        &objInfo.enmState,
                                                        &objInfo.creationTime,
                                                        &objInfo.deletionTime);
                    if (SUCCEEDED(hrc))
                        lObjInfoMap[bstrObjId] = objInfo;
                    else
                        RTPrintf(("VirtualBox::getTrackedObject() returned the error 0x%LX\n"), hrc);
                }
            }

            if (!lObjInfoMap.empty())
                printTrackedObjectInfo(foundIface, lObjInfoMap);
            else
                RTPrintf(("Object with Id %s wasn't found\n"), strObjUuid.c_str());
        }
        else
            RTPrintf(("VirtualBox::getTrackedObjectIds() returned the error 0x%LX\n"), hrc);
    }
    else
        RTPrintf(("Interface %s isn't supported at present\n"), strIfaceName.c_str());

    return SUCCEEDED(hrc) ? RTEXITCODE_SUCCESS : RTEXITCODE_FAILURE;
}


static RTEXITCODE handleSupIfaceList(HandlerArg *a, int iFirst)
{
    HRESULT hrc = S_OK;

    static const RTGETOPTDEF s_aOptions[] =
    {
        { "help",        'h', RTGETOPT_REQ_NOTHING },
        { "--help",      'h', RTGETOPT_REQ_NOTHING }
    };
    RTGETOPTSTATE GetState;
    RTGETOPTUNION ValueUnion;
    int vrc = RTGetOptInit(&GetState, a->argc, a->argv, s_aOptions, RT_ELEMENTS(s_aOptions), iFirst, 0);
    AssertRCReturn(vrc, RTEXITCODE_FAILURE);

    int c;
    while ((c = RTGetOpt(&GetState, &ValueUnion)) != 0)
    {
        switch (c)
        {
            case 'h':
                printHelp(g_pStdOut);
                return RTEXITCODE_SUCCESS;
            case VINF_GETOPT_NOT_OPTION:
                return errorUnknownSubcommand(ValueUnion.psz);

            default:
                return errorGetOpt(c, &ValueUnion);
        }
    }

    RTPrintf(("Supported interfaces:\n"));
    for (pair<const com::Utf8Str, supIfaces_T>& item : mapInterfaceNameToEnum)
        RTPrintf(("  %s\n"), item.first.c_str());

    return SUCCEEDED(hrc) ? RTEXITCODE_SUCCESS : RTEXITCODE_FAILURE;
}

static RTEXITCODE handleObjectList(HandlerArg *a, int iFirst)
{
    HRESULT hrc = S_OK;

    static const RTGETOPTDEF s_aOptions[] =
    {
        { "--ifacename", 'f', RTGETOPT_REQ_STRING },
        { "help",        'h', RTGETOPT_REQ_NOTHING },
        { "--help",      'h', RTGETOPT_REQ_NOTHING }
    };
    RTGETOPTSTATE GetState;
    RTGETOPTUNION ValueUnion;
    int vrc = RTGetOptInit(&GetState, a->argc, a->argv, s_aOptions, RT_ELEMENTS(s_aOptions), iFirst, 0);
    AssertRCReturn(vrc, RTEXITCODE_FAILURE);
    if (a->argc == iFirst)
    {
        RTPrintf(("Empty command parameter list, show help.\n"));
        printHelp(g_pStdOut);
        return RTEXITCODE_SUCCESS;
    }

    ComPtr<IVirtualBox> pVirtualBox = a->virtualBox;
    Utf8Str strIfaceName;

    int c;
    while ((c = RTGetOpt(&GetState, &ValueUnion)) != 0)
    {
        switch (c)
        {
            case 'f':
            {
                if (strIfaceName.isNotEmpty())
                    return errorArgument("Duplicate parameter: --ifacename");

                strIfaceName = ValueUnion.psz;
                if (strIfaceName.isEmpty())
                    return errorArgument("Empty parameter: --ifacename");

                break;
            }
            case 'h':
                printHelp(g_pStdOut);
                return RTEXITCODE_SUCCESS;
            case VINF_GETOPT_NOT_OPTION:
                return errorUnknownSubcommand(ValueUnion.psz);

            default:
                return errorGetOpt(c, &ValueUnion);
        }
    }

    map <com::Utf8Str, supIfaces_T>::const_iterator cIt = mapInterfaceNameToEnum.find(strIfaceName);
    if (cIt != mapInterfaceNameToEnum.end())
    {
        com::SafeArray<BSTR> ObjIDsList;
        hrc = pVirtualBox->GetTrackedObjectIds(Bstr(strIfaceName).raw(), ComSafeArrayAsOutParam(ObjIDsList));
        if (SUCCEEDED(hrc))
        {
            RTPrintf(("Tracked objects IIDs:\n"));
            for (size_t i = 0; i < ObjIDsList.size(); ++i)
            {
                Bstr bstrObjId = ObjIDsList[i];
                RTPrintf(("  %s\n"), Utf8Str(bstrObjId).c_str());
            }
        }
    }

    return SUCCEEDED(hrc) ? RTEXITCODE_SUCCESS : RTEXITCODE_FAILURE;
}

RTEXITCODE handleTrackedObjects(HandlerArg *a)
{
    enum
    {
        kObjectTracker_Ifaces = 1000,
        kObjectTracker_ObjList,
        kObjectTracker_ObjInfo,
    };

    static const RTGETOPTDEF s_aOptions[] =
    {
        /* common options */
        { "ifaces",  kObjectTracker_Ifaces,            RTGETOPT_REQ_NOTHING },
        { "objlist", kObjectTracker_ObjList,           RTGETOPT_REQ_NOTHING },
        { "objinfo", kObjectTracker_ObjInfo,           RTGETOPT_REQ_NOTHING },
    };

    if (a->argc < 1)
        return errorNoSubcommand();

    RTGETOPTSTATE GetState;
    int vrc = RTGetOptInit(&GetState, a->argc, a->argv, s_aOptions, RT_ELEMENTS(s_aOptions), 0, 0);
    AssertRCReturn(vrc, RTEXITCODE_FAILURE);

    int c;
    RTGETOPTUNION ValueUnion;
    while ((c = RTGetOpt(&GetState, &ValueUnion)) != 0)
    {
        switch (c)
        {
            /* Sub-commands: */
            case kObjectTracker_Ifaces:
                setCurrentSubcommand(HELP_SCOPE_OBJTRACKER_IFACES);
                return handleSupIfaceList(a, GetState.iNext);
            case kObjectTracker_ObjList:
                setCurrentSubcommand(HELP_SCOPE_OBJTRACKER_OBJLIST);
                return handleObjectList(a, GetState.iNext);
            case kObjectTracker_ObjInfo:
                setCurrentSubcommand(HELP_SCOPE_OBJTRACKER_OBJINFO);
                return handleObjInfo(a, GetState.iNext);
            case VINF_GETOPT_NOT_OPTION:
                return errorUnknownSubcommand(ValueUnion.psz);
            default:
                return errorGetOpt(c, &ValueUnion);
        }
    }

    return errorNoSubcommand();
}
