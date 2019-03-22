/* $Id$ */
/** @file
 * JavaScript functions for the Graph Wizard.
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
 *
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL) only, as it comes in the "COPYING.CDDL" file of the
 * VirtualBox OSE distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 */


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
/** The previous width of the div element that we measure.  */
var g_cxPreviousWidth = 0;


/**
 * onload function that sets g_cxPreviousWidth to the width of @a sWidthSrcId.
 *
 * @returns true.
 * @param   sWidthSrcId     The ID of the element which width we should measure.
 */
function graphwizOnLoadRememberWidth(sWidthSrcId)
{
    var cx = getUnscaledElementWidthById(sWidthSrcId);
    if (cx)
    {
        g_cxPreviousWidth = cx;
    }
    return true;
}


/**
 * onresize callback function that scales the given graph width input field
 * value according to the resized element.
 *
 * @returns true.
 * @param   sWidthSrcId     The ID of the element which width we should measure
 *                          the resize effect on.
 * @param   sWidthInputId   The ID of the input field which values should be
 *                          scaled.
 *
 * @remarks Since we're likely to get several resize calls as part of one user
 *          resize operation, we're likely to suffer from some rounding
 *          artifacts.  So, should the user abort or undo the resizing, the
 *          width value is unlikely to be restored to the exact value it had
 *          prior to the resizing.
 */
function graphwizOnResizeRecalcWidth(sWidthSrcId, sWidthInputId)
{
    var cx = getUnscaledElementWidthById(sWidthSrcId);
    if (cx)
    {
        var oElement = document.getElementById(sWidthInputId);
        if (oElement && g_cxPreviousWidth)
        {
            var cxOld = oElement.value;
            if (isInteger(cxOld))
            {
                var fpRatio = cxOld / g_cxPreviousWidth;
                oElement.value = Math.round(cx * fpRatio);
            }
        }
        g_cxPreviousWidth = cx;
    }

    return true;
}

/**
 * Fills thegraph size (cx, cy) and dpi fields with default values.
 *
 * @returns false (for onclick).
 * @param   sWidthSrcId     The ID of the element which width we should measure.
 * @param   sWidthInputId   The ID of the graph width field (cx).
 * @param   sHeightInputId  The ID of the graph height field (cy).
 * @param   sDpiInputId     The ID of the graph DPI field.
 */
function graphwizSetDefaultSizeValues(sWidthSrcId, sWidthInputId, sHeightInputId, sDpiInputId)
{
    var cx            = getUnscaledElementWidthById(sWidthSrcId);
    var cDotsPerInch  = getDeviceXDotsPerInch();

    if (cx)
    {
        setInputFieldValue(sWidthInputId, cx);
        setInputFieldValue(sHeightInputId, Math.round(cx * 5 / 16)); /* See wuimain.py. */
    }

    if (cDotsPerInch)
    {
        setInputFieldValue(sDpiInputId, cDotsPerInch);
    }

    return false;
}

