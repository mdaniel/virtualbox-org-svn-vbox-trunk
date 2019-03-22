/* $Id$ */
/** @file
 * VBox Qt GUI - UISettingsDefs implementation
 */

/*
 * Copyright (C) 2011-2019 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/* GUI includes: */
#include "UISettingsDefs.h"


/* Using declarations: */
using namespace UISettingsDefs;

ConfigurationAccessLevel UISettingsDefs::configurationAccessLevel(KSessionState enmSessionState, KMachineState enmMachineState)
{
    /* Depending on passed arguments: */
    switch (enmMachineState)
    {
        case KMachineState_PoweredOff:
        case KMachineState_Teleported:
        case KMachineState_Aborted:    return enmSessionState == KSessionState_Unlocked ?
                                              ConfigurationAccessLevel_Full :
                                              ConfigurationAccessLevel_Partial_PoweredOff;
        case KMachineState_Saved:      return ConfigurationAccessLevel_Partial_Saved;
        case KMachineState_Running:
        case KMachineState_Paused:     return ConfigurationAccessLevel_Partial_Running;
        default: break;
    }
    /* Null by default: */
    return ConfigurationAccessLevel_Null;
}

