/* $Id$ */
/** @file
 * Recording utility code.
 */

/*
 * Copyright (C) 2012-2023 Oracle and/or its affiliates.
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

#include "Recording.h"
#include "RecordingUtils.h"

#include <iprt/asm.h>
#include <iprt/assert.h>
#include <iprt/critsect.h>
#include <iprt/path.h>
#include <iprt/semaphore.h>
#include <iprt/thread.h>
#include <iprt/time.h>

#ifdef DEBUG
#include <iprt/file.h>
#include <iprt/formats/bmp.h>
#endif

#include <VBox/log.h>


/**
 * Convert an image to YUV420p format.
 *
 * @return \c true on success, \c false on failure.
 * @param  aDstBuf              The destination image buffer.
 * @param  aDstWidth            Width (in pixel) of destination buffer.
 * @param  aDstHeight           Height (in pixel) of destination buffer.
 * @param  aSrcBuf              The source image buffer.
 * @param  aSrcWidth            Width (in pixel) of source buffer.
 * @param  aSrcHeight           Height (in pixel) of source buffer.
 */
template <class T>
inline bool recordingUtilsColorConvWriteYUV420p(uint8_t *aDstBuf, unsigned aDstWidth, unsigned aDstHeight,
                                                uint8_t *aSrcBuf, unsigned aSrcWidth, unsigned aSrcHeight)
{
    RT_NOREF(aDstWidth, aDstHeight);

    size_t image_size = aSrcWidth * aSrcHeight;
    size_t upos = image_size;
    size_t vpos = upos + upos / 4;
    size_t i = 0;

#define CALC_Y(r, g, b) \
    ((66 * r + 129 * g + 25 * b) >> 8) + 16
#define CALC_U(r, g, b) \
    ((-38 * r + -74 * g + 112 * b) >> 8) + 128
#define CALC_V(r, g, b) \
    ((112 * r + -94 * g + -18 * b) >> 8) + 128

    for (size_t line = 0; line < aSrcHeight; ++line)
    {
        if ((line % 2) == 0)
        {
            for (size_t x = 0; x < aSrcWidth; x += 2)
            {
                uint8_t b = aSrcBuf[4 * i];
                uint8_t g = aSrcBuf[4 * i + 1];
                uint8_t r = aSrcBuf[4 * i + 2];

                aDstBuf[i++] = CALC_Y(r, g, b);

                aDstBuf[upos++] = CALC_U(r, g, b);
                aDstBuf[vpos++] = CALC_V(r, g, b);

                b = aSrcBuf[4 * i];
                g = aSrcBuf[4 * i + 1];
                r = aSrcBuf[4 * i + 2];

                aDstBuf[i++] = CALC_Y(r, g, b);
            }
        }
        else
        {
            for (size_t x = 0; x < aSrcWidth; x++)
            {
                uint8_t b = aSrcBuf[4 * i];
                uint8_t g = aSrcBuf[4 * i + 1];
                uint8_t r = aSrcBuf[4 * i + 2];

                aDstBuf[i++] = CALC_Y(r, g, b);
            }
        }
    }

    return true;
}

/**
 * Convert an image to RGB24 format.
 *
 * @returns true on success, false on failure.
 * @param aWidth    Width of image.
 * @param aHeight   Height of image.
 * @param aDestBuf  An allocated memory buffer large enough to hold the
 *                  destination image (i.e. width * height * 12bits).
 * @param aSrcBuf   The source image as an array of bytes.
 */
template <class T>
inline bool RecordingUtilsColorConvWriteRGB24(unsigned aWidth, unsigned aHeight,
                                              uint8_t *aDestBuf, uint8_t *aSrcBuf)
{
    enum { PIX_SIZE = 3 };
    bool fRc = true;
    AssertReturn(0 == (aWidth & 1), false);
    AssertReturn(0 == (aHeight & 1), false);
    T iter(aWidth, aHeight, aSrcBuf);
    unsigned cPixels = aWidth * aHeight;
    for (unsigned i = 0; i < cPixels && fRc; ++i)
    {
        unsigned red, green, blue;
        fRc = iter.getRGB(&red, &green, &blue);
        if (fRc)
        {
            aDestBuf[i * PIX_SIZE    ] = red;
            aDestBuf[i * PIX_SIZE + 1] = green;
            aDestBuf[i * PIX_SIZE + 2] = blue;
        }
    }
    return fRc;
}

/**
 * Converts an entire RGB BGRA32 buffer to a YUV I420 buffer.
 *
 * @param   paDst               Pointer to destination buffer.
 * @param   uDstWidth           Width (X, in pixel) of destination buffer.
 * @param   uDstHeight          Height (Y, in pixel) of destination buffer.
 * @param   paSrc               Pointer to source buffer.
 * @param   uSrcWidth           Width (X, in pixel) of source buffer.
 * @param   uSrcHeight          Height (Y, in pixel) of source buffer.
 */
void RecordingUtilsConvBGRA32ToYUVI420(uint8_t *paDst, uint32_t uDstWidth, uint32_t uDstHeight,
                                       uint8_t *paSrc, uint32_t uSrcWidth, uint32_t uSrcHeight)
{
    RT_NOREF(uDstWidth, uDstHeight);

    size_t const image_size = uSrcWidth * uSrcHeight;
    size_t upos = image_size;
    size_t vpos = upos + upos / 4;
    size_t i = 0;

#define CALC_Y(r, g, b) \
    ((66 * r + 129 * g + 25 * b) >> 8) + 16
#define CALC_U(r, g, b) \
    ((-38 * r + -74 * g + 112 * b) >> 8) + 128
#define CALC_V(r, g, b) \
    ((112 * r + -94 * g + -18 * b) >> 8) + 128

    for (size_t y = 0; y < uSrcHeight; y++)
    {
        if ((y % 2) == 0)
        {
            for (size_t x = 0; x < uSrcWidth; x += 2)
            {
                uint8_t b = paSrc[4 * i];
                uint8_t g = paSrc[4 * i + 1];
                uint8_t r = paSrc[4 * i + 2];

                paDst[i++] = CALC_Y(r, g, b);

                paDst[upos++] = CALC_U(r, g, b);
                paDst[vpos++] = CALC_V(r, g, b);

                b = paSrc[4 * i];
                g = paSrc[4 * i + 1];
                r = paSrc[4 * i + 2];

                paDst[i++] = CALC_Y(r, g, b);
            }
        }
        else
        {
            for (size_t x = 0; x < uSrcWidth; x++)
            {
                uint8_t const b = paSrc[4 * i];
                uint8_t const g = paSrc[4 * i + 1];
                uint8_t const r = paSrc[4 * i + 2];

                paDst[i++] = CALC_Y(r, g, b);
            }
        }
    }

#undef CALC_Y
#undef CALC_U
#undef CALC_V
}

/**
 * Converts a part of a RGB BGRA32 buffer to a YUV I420 buffer.
 *
 * @param   paDst               Pointer to destination buffer.
 * @param   uDstX               X destination position (in pixel).
 * @param   uDstY               Y destination position (in pixel).
 * @param   uDstWidth           Width (X, in pixel) of destination buffer.
 * @param   uDstHeight          Height (Y, in pixel) of destination buffer.
 * @param   paSrc               Pointer to source buffer.
 * @param   uSrcX               X source position (in pixel).
 * @param   uSrcY               Y source position (in pixel).
 * @param   uSrcWidth           Width (X, in pixel) of source buffer.
 * @param   uSrcHeight          Height (Y, in pixel) of source buffer.
 * @param   uSrcStride          Stride (in bytes) of source buffer.
 * @param   uSrcBPP             Bits per pixel of source buffer.
 */
void RecordingUtilsConvBGRA32ToYUVI420Ex(uint8_t *paDst, uint32_t uDstX, uint32_t uDstY, uint32_t uDstWidth, uint32_t uDstHeight,
                                         uint8_t *paSrc, uint32_t uSrcX, uint32_t uSrcY, uint32_t uSrcWidth, uint32_t uSrcHeight,
                                         uint32_t uSrcStride, uint8_t uSrcBPP)
{
    Assert(uDstX < uDstWidth);
    Assert(uDstX + uSrcWidth <= uDstWidth);
    Assert(uDstY < uDstHeight);
    Assert(uDstY + uSrcHeight <= uDstHeight);
    Assert(uSrcBPP % 8 == 0);

    RT_NOREF(uSrcHeight, uDstHeight);

#define CALC_Y(r, g, b) \
    (66 * r + 129 * g + 25 * b) >> 8
#define CALC_U(r, g, b) \
    ((-38 * r + -74 * g + 112 * b) >> 8) + 128
#define CALC_V(r, g, b) \
    ((112 * r + -94 * g + -18 * b) >> 8) + 128

    uint32_t uDstXCur = uDstX;

    const unsigned uSrcBytesPerPixel = uSrcBPP / 8;

    size_t offSrc = (uSrcY * uSrcStride) + (uSrcX * uSrcBytesPerPixel);

    for (size_t y = 0; y < uSrcHeight; y++)
    {
        for (size_t x = 0; x < uSrcWidth; x++)
        {
            size_t const offBGR = offSrc + (x * uSrcBytesPerPixel);

            uint8_t const b = paSrc[offBGR];
            uint8_t const g = paSrc[offBGR + 1];
            uint8_t const r = paSrc[offBGR + 2];

            size_t const offY  = uDstY * uDstWidth + uDstXCur;
            size_t const offUV = (uDstY / 2) * (uDstWidth / 2) + (uDstXCur / 2) + uDstWidth * uDstHeight;

            paDst[offY]                               = CALC_Y(r, g, b);
            paDst[offUV]                              = CALC_U(r, g, b);
            paDst[offUV + uDstWidth * uDstHeight / 4] = CALC_V(r, g, b);

            uDstXCur++;
        }

        offSrc += uSrcStride;

        uDstXCur = uDstX;
        uDstY++;
    }

#undef CALC_Y
#undef CALC_U
#undef CALC_V
}

/**
 * Crops (centers) or centers coordinates of a given source.
 *
 * @returns VBox status code.
 * @retval  VWRN_RECORDING_ENCODING_SKIPPED if the source is not visible.
 * @param   pCodecParms         Video codec parameters to use for cropping / centering.
 * @param   sx                  Input / output: X location (in pixel) of the source.
 * @param   sy                  Input / output: Y location (in pixel) of the source.
 * @param   sw                  Input / output: Width (in pixel) of the source.
 * @param   sh                  Input / output: Height (in pixel) of the source.
 * @param   dx                  Input / output: X location (in pixel) of the destination.
 * @param   dy                  Input / output: Y location (in pixel) of the destination.
 *
 * @note    Used when no other scaling algorithm is being selected / available. See testcase.
 */
int RecordingUtilsCoordsCropCenter(PRECORDINGCODECPARMS pCodecParms,
                                   int32_t *sx, int32_t *sy, int32_t *sw, int32_t *sh, int32_t *dx, int32_t *dy)
{
    int vrc = VINF_SUCCESS;

    Log3Func(("Original: sx=%RI32 sy=%RI32 sw=%RI32 sh=%RI32 dx=%RI32 dy=%RI32\n",
              *sx, *sy, *sw, *sh, *dx, *dy));

    /*
     * Do centered cropping or centering.
     */

    int32_t const uOriginX = (int32_t)pCodecParms->u.Video.Scaling.u.Crop.m_iOriginX;
    int32_t const uOriginY = (int32_t)pCodecParms->u.Video.Scaling.u.Crop.m_iOriginY;

    /* Apply cropping / centering values. */
    *dx += uOriginX;
    *dy += uOriginY;

    if (*dx < 0)
    {
        *dx  = 0;
        *sx += RT_ABS(uOriginX);
        *sw -= RT_ABS(uOriginX);
    }

    if (*dy < 0)
    {
        *dy  = 0;
        *sy += RT_ABS(uOriginY);
        *sh -= RT_ABS(uOriginY);
    }

    Log3Func(("Crop0: sx=%RI32 sy=%RI32 sw=%RI32 sh=%RI32 dx=%RI32 dy=%RI32\n",
              *sx, *sy, *sw, *sh, *dx, *dy));

    if (*sw > (int32_t)pCodecParms->u.Video.uWidth)
        *sw = (int32_t)pCodecParms->u.Video.uWidth;

    if (*sh > (int32_t)pCodecParms->u.Video.uHeight)
        *sh = pCodecParms->u.Video.uHeight;

    if (*dx + *sw >= (int32_t)pCodecParms->u.Video.uWidth)
        *sw = pCodecParms->u.Video.uWidth - *dx;

    if (*dy + *sh >= (int32_t)pCodecParms->u.Video.uHeight)
        *sh = pCodecParms->u.Video.uHeight - *dy;

    if (   *dx + *sw < 1
        || *dy + *sh < 1
        || *dx > (int32_t)pCodecParms->u.Video.uWidth
        || *dy > (int32_t)pCodecParms->u.Video.uHeight
        || *sw < 1
        || *sh < 1)
    {
        vrc = VWRN_RECORDING_ENCODING_SKIPPED; /* Not visible, skip encoding. */
    }

    Log3Func(("Crop1: sx=%RI32 sy=%RI32 sw=%RI32 sh=%RI32 dx=%RI32 dy=%RI32 -> vrc=%Rrc\n",
              *sx, *sy, *sw, *sh, *dx, *dy, vrc));

    return vrc;
}

#ifdef DEBUG
/**
 * Dumps image data to a bitmap (BMP) file.
 *
 * @returns VBox status code.
 * @param   pu8RGBBuf           Pointer to actual RGB image data.
 * @param   cbRGBBuf            Size (in bytes) of \a pu8RGBBuf.
 * @param   pszPath             Absolute path to dump file to. Must exist.
 *                              Specify NULL to use the system's temp directory as output directory.
 *                              Existing files will be overwritten.
 * @param   pszWhat             Hint of what to dump. Optional and can be NULL.
 * @param   uWidth              Width (in pixel) to write.
 * @param   uHeight             Height (in pixel) to write.
 * @param   uBytsPerLine        Bytes per line.
 * @param   uBPP                Bits in pixel.
 */
int RecordingUtilsDbgDumpImageData(const uint8_t *pu8RGBBuf, size_t cbRGBBuf, const char *pszPath, const char *pszWhat,
                                   uint32_t uWidth, uint32_t uHeight, uint32_t uBytesPerLine, uint8_t uBPP)
{
    RT_NOREF(cbRGBBuf);

    const uint8_t  uBytesPerPixel = uBPP / 8 /* Bits */;
    const size_t   cbData         = uWidth * uHeight * uBytesPerPixel;

    if (!cbData) /* No data to write? Bail out early. */
        return VINF_SUCCESS;

    BMPFILEHDR fileHdr;
    RT_ZERO(fileHdr);

    BMPWIN3XINFOHDR coreHdr;
    RT_ZERO(coreHdr);

    fileHdr.uType      = BMP_HDR_MAGIC;
    fileHdr.cbFileSize = (uint32_t)(sizeof(BMPFILEHDR) + sizeof(BMPWIN3XINFOHDR) + cbData);
    fileHdr.offBits    = (uint32_t)(sizeof(BMPFILEHDR) + sizeof(BMPWIN3XINFOHDR));

    coreHdr.cbSize         = sizeof(BMPWIN3XINFOHDR);
    coreHdr.uWidth         = uWidth;
    coreHdr.uHeight        = uHeight;
    coreHdr.cPlanes        = 1;
    coreHdr.cBits          = uBPP;
    coreHdr.uXPelsPerMeter = 5000;
    coreHdr.uYPelsPerMeter = 5000;

    static uint64_t s_iCount = 0;

    /* Hardcoded path for now. */
    char szPath[RTPATH_MAX];
    if (!pszPath)
    {
        int rc2 = RTPathTemp(szPath, sizeof(szPath));
        if (RT_FAILURE(rc2))
            return rc2;
    }

    char szFileName[RTPATH_MAX];
    if (RTStrPrintf2(szFileName, sizeof(szFileName), "%s/RecDump-%04RU64-%s-w%RU16h%RU16.bmp",
                     pszPath ? pszPath : szPath, s_iCount, pszWhat ? pszWhat : "Frame", uWidth, uHeight) <= 0)
        return VERR_BUFFER_OVERFLOW;

    s_iCount++;

    RTFILE fh;
    int vrc = RTFileOpen(&fh, szFileName,
                         RTFILE_O_CREATE_REPLACE | RTFILE_O_WRITE | RTFILE_O_DENY_NONE);
    if (RT_SUCCESS(vrc))
    {
        RTFileWrite(fh, &fileHdr, sizeof(fileHdr), NULL);
        RTFileWrite(fh, &coreHdr, sizeof(coreHdr), NULL);

        /* Bitmaps (DIBs) are stored upside-down (thanks, OS/2), so work from the bottom up. */
        size_t offSrc = cbRGBBuf - uBytesPerLine;

        /* Do the copy. */
        for (unsigned int i = 0; i < uHeight; i++)
        {
            RTFileWrite(fh, pu8RGBBuf + offSrc, uBytesPerLine, NULL);
            offSrc -= uBytesPerLine;
        }

        RTFileClose(fh);
    }

    return vrc;
}

/**
 * Dumps a video recording frame to a bitmap (BMP) file, extended version.
 *
 * @returns VBox status code.
 * @param   pFrame              Video frame to dump.
 * @param   pszPath             Output directory.
 * @param   pszWhat             Hint of what to dump. Optional and can be NULL.
 */
int RecordingUtilsDbgDumpVideoFrameEx(const PRECORDINGVIDEOFRAME pFrame, const char *pszPath, const char *pszWhat)
{
    return RecordingUtilsDbgDumpImageData(pFrame->pau8Buf, pFrame->cbBuf,
                                          pszPath, pszWhat,
                                          pFrame->Info.uWidth, pFrame->Info.uHeight, pFrame->Info.uBytesPerLine, pFrame->Info.uBPP);
}

/**
 * Dumps a video recording frame to a bitmap (BMP) file.
 *
 * @returns VBox status code.
 * @param   pFrame              Video frame to dump.
 * @param   pszWhat             Hint of what to dump. Optional and can be NULL.
 */
int RecordingUtilsDbgDumpVideoFrame(const PRECORDINGVIDEOFRAME pFrame, const char *pszWhat)
{
    return RecordingUtilsDbgDumpVideoFrameEx(pFrame, NULL /* Use temp directory */, pszWhat);
}
#endif /* DEBUG */

