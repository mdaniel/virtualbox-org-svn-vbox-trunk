/** @file
 * VBox Remote Desktop Extension (VRDE) - Video Input interface.
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

#ifndef ___VBox_RemoteDesktop_VRDEVideoIn_h
#define ___VBox_RemoteDesktop_VRDEVideoIn_h


/* Define VRDE_VIDEOIN_WITH_VRDEINTERFACE to include the server VRDE interface parts. */

#ifdef VRDE_VIDEOIN_WITH_VRDEINTERFACE
# include <VBox/RemoteDesktop/VRDE.h>
#endif /* VRDE_VIDEOIN_WITH_VRDEINTERFACE */

#ifdef AssertCompileSize
# define ASSERTSIZE(type, size) AssertCompileSize(type, size);
#else
# define ASSERTSIZE(type, size)
#endif /* AssertCompileSize */

#include <iprt/types.h>


/*
 * Interface for accessing a video camera device on the client.
 *
 * Async callbacks are used for providing feedback, reporting errors, etc.
 *
 * Initial version supports: Camera + Processing Unit + Streaming Control.
 *
 * There are 2 modes:
 * 1) The virtual WebCam is already attached to the guest.
 * 2) The virtual WebCam will be attached when the client has it.
 *
 * Initially the mode 1 is supported.
 *
 * Mode 1 details:
 * The WebCam has some fixed functionality, according to the descriptors,
 * which has been already read by the guest. So some of functions will
 * not work if the client does not support them.
 *
 * Mode 2 details:
 * Virtual WebCam descriptors are built from the client capabilities.
 *
 * Similarly to the smartcard, the server will inform the ConsoleVRDE that there is a WebCam.
 * ConsoleVRDE creates a VRDEVIDEOIN handle and forwards virtual WebCam requests to it.
 *
 * Interface with VBox.
 *
 *   Virtual WebCam         ConsoleVRDE                VRDE
 *
 *                                                 Negotiate <->
 *                                              <- VideoInDeviceNotify(Attached, DeviceId)
 *                       -> GetDeviceDesc
 *                                              <- DeviceDesc
 * 2                     <- CreateCamera
 * 2  CameraCreated ->
 *
 *    CameraRequest ->      Request ->
 *    Response <-        <- Response            <- Response
 *    Frame <-           <- Frame               <- Frame
 *                                              <- VideoInDeviceNotify(Detached, DeviceId)
 *
 * Unsupported requests fail.
 * The Device Description received from the client may be used to validate WebCam requests
 * in the ConsoleVRDE code, for example filter out unsupported requests.
 *
 */

/* All structures in this file are packed.
 * Everything is little-endian.
 */
#pragma pack(1)

/*
 * The interface supports generic video input descriptors, capabilities and controls:
 *   * Descriptors
 *     + Interface
 *       - Input, Camera Terminal
 *       - Processing Unit
 *     + Video Streaming
 *       - Input Header
 *       - Payload Format
 *       - Video Frame
 *       - Still Image Frame
 *   * Video Control requests
 *     + Interface
 *       - Power Mode
 *     + Unit and Terminal
 *       camera
 *       - Scanning Mode (interlaced, progressive)
 *       - Auto-Exposure Mode
 *       - Auto-Exposure Priority
 *       - Exposure Time Absolute, Relative
 *       - Focus Absolute, Relative, Auto
 *       - Iris Absolute, Relative
 *       - Zoom Absolute, Relative
 *       - PanTilt Absolute, Relative
 *       - Roll Absolute, Relative
 *       - Privacy
 *       processing
 *       - Backlight Compensation
 *       - Brightness
 *       - Contrast
 *       - Gain
 *       - Power Line Frequency
 *       - Hue Manual, Auto
 *       - Saturation
 *       - Sharpness
 *       - Gamma
 *       - White Balance Temperature Manual, Auto
 *       - White Balance Component Manual, Auto
 *       - Digital Multiplier
 *       - Digital Multiplier Limit
 *   * Video Streaming requests
 *     + Interface
 *       - Synch Delay
 *       - Still Image Trigger
 *       - Generate Key Frame
 *       - Update Frame Segment
 *       - Stream Error Code
 *
 *
 * Notes:
 *   * still capture uses a method similar to method 2, because the still frame will
 *     be send instead of video over the channel.
 *     Also the method 2 can be in principle emulated by both 1 and 3 on the client.
 *     However the client can initiate a still frame transfer, similar to hardware button trigger.
 *   * all control changes are async.
 *   * probe/commit are not used. The server can select a supported format/frame from the list.
 *   * no color matching. sRGB is the default.
 *   * most of constants are the same as in USB Video Class spec, but they are not the same and
 *     should be always converted.
 */

/*
 * The DEVICEDEC describes the device and provides a list of supported formats:
 *     VRDEVIDEOINDEVICEDESC
 *     VRDEVIDEOINFORMATDESC[0];
 *     VRDEVIDEOINFRAMEDESC[0..N-1]
 *     VRDEVIDEOINFORMATDESC[1];
 *     VRDEVIDEOINFRAMEDESC[0..M-1]
 *     ...
 */

typedef struct VRDEVIDEOINDEVICEDESC
{
    uint16_t u16ObjectiveFocalLengthMin;
    uint16_t u16ObjectiveFocalLengthMax;
    uint16_t u16OcularFocalLength;
    uint16_t u16MaxMultiplier;
    uint32_t fu32CameraControls;     /* VRDE_VIDEOIN_F_CT_CTRL_* */
    uint32_t fu32ProcessingControls; /* VRDE_VIDEOIN_F_PU_CTRL_* */
    uint8_t fu8DeviceCaps;           /* VRDE_VIDEOIN_F_DEV_CAP_* */
    uint8_t u8NumFormats;            /* Number of following VRDEVIDEOINFORMATDESC structures. */
    uint16_t cbExt;                  /* Size of the optional extended description. */
    /* An extended description may follow. */
    /* An array of VRDEVIDEOINFORMATDESC follows. */
} VRDEVIDEOINDEVICEDESC;

/* VRDEVIDEOINDEVICEDESC::fu32CameraControls */
#define VRDE_VIDEOIN_F_CT_CTRL_SCANNING_MODE            0x00000001 /* D0: Scanning Mode */
#define VRDE_VIDEOIN_F_CT_CTRL_AE_MODE                  0x00000002 /* D1: Auto-Exposure Mode */
#define VRDE_VIDEOIN_F_CT_CTRL_AE_PRIORITY              0x00000004 /* D2: Auto-Exposure Priority */
#define VRDE_VIDEOIN_F_CT_CTRL_EXPOSURE_TIME_ABSOLUTE   0x00000008 /* D3: Exposure Time (Absolute) */
#define VRDE_VIDEOIN_F_CT_CTRL_EXPOSURE_TIME_RELATIVE   0x00000010 /* D4: Exposure Time (Relative) */
#define VRDE_VIDEOIN_F_CT_CTRL_FOCUS_ABSOLUTE           0x00000020 /* D5: Focus (Absolute) */
#define VRDE_VIDEOIN_F_CT_CTRL_FOCUS_RELATIVE           0x00000040 /* D6: Focus (Relative) */
#define VRDE_VIDEOIN_F_CT_CTRL_IRIS_ABSOLUTE            0x00000080 /* D7: Iris (Absolute) */
#define VRDE_VIDEOIN_F_CT_CTRL_IRIS_RELATIVE            0x00000100 /* D8: Iris (Relative) */
#define VRDE_VIDEOIN_F_CT_CTRL_ZOOM_ABSOLUTE            0x00000200 /* D9: Zoom (Absolute) */
#define VRDE_VIDEOIN_F_CT_CTRL_ZOOM_RELATIVE            0x00000400 /* D10: Zoom (Relative) */
#define VRDE_VIDEOIN_F_CT_CTRL_PANTILT_ABSOLUTE         0x00000800 /* D11: PanTilt (Absolute) */
#define VRDE_VIDEOIN_F_CT_CTRL_PANTILT_RELATIVE         0x00001000 /* D12: PanTilt (Relative) */
#define VRDE_VIDEOIN_F_CT_CTRL_ROLL_ABSOLUTE            0x00002000 /* D13: Roll (Absolute) */
#define VRDE_VIDEOIN_F_CT_CTRL_ROLL_RELATIVE            0x00004000 /* D14: Roll (Relative) */
#define VRDE_VIDEOIN_F_CT_CTRL_RESERVED1                0x00008000 /* D15: Reserved */
#define VRDE_VIDEOIN_F_CT_CTRL_RESERVED2                0x00010000 /* D16: Reserved */
#define VRDE_VIDEOIN_F_CT_CTRL_FOCUS_AUTO               0x00020000 /* D17: Focus, Auto */
#define VRDE_VIDEOIN_F_CT_CTRL_PRIVACY                  0x00040000 /* D18: Privacy */

/* VRDEVIDEOINDEVICEDESC::fu32ProcessingControls */
#define VRDE_VIDEOIN_F_PU_CTRL_BRIGHTNESS                0x00000001 /* D0: Brightness */
#define VRDE_VIDEOIN_F_PU_CTRL_CONTRAST                  0x00000002 /* D1: Contrast */
#define VRDE_VIDEOIN_F_PU_CTRL_HUE                       0x00000004 /* D2: Hue */
#define VRDE_VIDEOIN_F_PU_CTRL_SATURATION                0x00000008 /* D3: Saturation */
#define VRDE_VIDEOIN_F_PU_CTRL_SHARPNESS                 0x00000010 /* D4: Sharpness */
#define VRDE_VIDEOIN_F_PU_CTRL_GAMMA                     0x00000020 /* D5: Gamma */
#define VRDE_VIDEOIN_F_PU_CTRL_WHITE_BALANCE_TEMPERATURE 0x00000040 /* D6: White Balance Temperature */
#define VRDE_VIDEOIN_F_PU_CTRL_WHITE_BALANCE_COMPONENT   0x00000080 /* D7: White Balance Component */
#define VRDE_VIDEOIN_F_PU_CTRL_BACKLIGHT_COMPENSATION    0x00000100 /* D8: Backlight Compensation */
#define VRDE_VIDEOIN_F_PU_CTRL_GAIN                      0x00000200 /* D9: Gain */
#define VRDE_VIDEOIN_F_PU_CTRL_POWER_LINE_FREQUENCY      0x00000400 /* D10: Power Line Frequency */
#define VRDE_VIDEOIN_F_PU_CTRL_HUE_AUTO                  0x00000800 /* D11: Hue, Auto */
#define VRDE_VIDEOIN_F_PU_CTRL_WHITE_BALANCE_TEMPERATURE_AUTO 0x00001000 /* D12: White Balance Temperature, Auto */
#define VRDE_VIDEOIN_F_PU_CTRL_WHITE_BALANCE_COMPONENT_AUTO   0x00002000 /* D13: White Balance Component, Auto */
#define VRDE_VIDEOIN_F_PU_CTRL_DIGITAL_MULTIPLIER        0x00004000 /* D14: Digital Multiplier */
#define VRDE_VIDEOIN_F_PU_CTRL_DIGITAL_MULTIPLIER_LIMIT  0x00008000 /* D15: Digital Multiplier Limit */

/* VRDEVIDEOINDEVICEDESC::fu8DeviceCaps */
#define VRDE_VIDEOIN_F_DEV_CAP_DYNAMICCHANGE   0x01 /* Whether dynamic format change is supported. */
#define VRDE_VIDEOIN_F_DEV_CAP_TRIGGER         0x02 /* Whether hardware triggering is supported. */
#define VRDE_VIDEOIN_F_DEV_CAP_TRIGGER_USAGE   0x04 /* 0 - still image, 1 - generic button event.*/

/* VRDEVIDEOINDEVICEDESC extended description. */
typedef struct VRDEVIDEOINDEVICEEXT
{
    uint32_t fu32Fields;
    /* One or more VRDEVIDEOINDEVICEFIELD follow. */
} VRDEVIDEOINDEVICEEXT;

typedef struct VRDEVIDEOINDEVICEFIELDHDR
{
    uint16_t cbField;  /* Number of bytes reserved for this field. */
} VRDEVIDEOINDEVICEFIELDHDR;

/* VRDEVIDEOINDEVICEDESC::fu32Fields */
#define VRDE_VIDEOIN_F_DEV_EXT_NAME   0x00000001 /* Utf8 device name. */
#define VRDE_VIDEOIN_F_DEV_EXT_SERIAL 0x00000002 /* Utf8 device serial number. */

/* The video format descriptor. */
typedef struct VRDEVIDEOINFORMATDESC
{
    uint16_t cbFormat;     /* Size of the structure including cbFormat and format specific data. */
    uint8_t u8FormatId;    /* The unique identifier of the format on the client. */
    uint8_t u8FormatType;  /* MJPEG etc. VRDE_VIDEOIN_FORMAT_* */
    uint8_t u8FormatFlags; /* VRDE_VIDEOIN_F_FMT_* */
    uint8_t u8NumFrames;   /* Number of following VRDEVIDEOINFRAMEDESC structures. */
    uint16_t u16Reserved;  /* Must be set to 0. */
    /* Other format specific data may follow. */
    /* An array of VRDEVIDEOINFRAMEDESC follows. */
} VRDEVIDEOINFORMATDESC;

/* VRDEVIDEOINFORMATDESC::u8FormatType */
#define VRDE_VIDEOIN_FORMAT_UNCOMPRESSED 0x04
#define VRDE_VIDEOIN_FORMAT_MJPEG        0x06
#define VRDE_VIDEOIN_FORMAT_MPEG2TS      0x0A
#define VRDE_VIDEOIN_FORMAT_DV           0x0C
#define VRDE_VIDEOIN_FORMAT_FRAME_BASED  0x10
#define VRDE_VIDEOIN_FORMAT_STREAM_BASED 0x12

/* VRDEVIDEOINFORMATDESC::u8FormatFlags. */
#define VRDE_VIDEOIN_F_FMT_GENERATEKEYFRAME   0x01 /* Supports Generate Key Frame */
#define VRDE_VIDEOIN_F_FMT_UPDATEFRAMESEGMENT 0x02 /* Supports Update Frame Segment */
#define VRDE_VIDEOIN_F_FMT_COPYPROTECT        0x04 /* If duplication should be restricted. */
#define VRDE_VIDEOIN_F_FMT_COMPQUALITY        0x08 /* If the format supports an adjustable compression quality. */

typedef struct VRDEVIDEOINFRAMEDESC
{
    uint16_t cbFrame;      /* Size of the structure including cbFrame and frame specific data. */
    uint8_t  u8FrameId;    /* The unique identifier of the frame for the corresponding format on the client. */
    uint8_t  u8FrameFlags;
    uint16_t u16Width;
    uint16_t u16Height;
    uint32_t u32NumFrameIntervals; /* The number of supported frame intervals. */
    uint32_t u32MinFrameInterval;  /* Shortest frame interval supported (at highest frame rate), in 100ns units. */
    uint32_t u32MaxFrameInterval;  /* Longest frame interval supported (at lowest frame rate), in 100ns units. */
    /* Supported frame intervals (in 100ns units) follow if VRDE_VIDEOIN_F_FRM_DISCRETE_INTERVALS is set.
     * uint32_t au32FrameIntervals[u32NumFrameIntervals];
     */
    /* Supported min and max bitrate in bits per second follow if VRDE_VIDEOIN_F_FRM_BITRATE is set.
     * uint32_t u32MinBitRate;
     * uint32_t u32MaxBitRate;
     */
    /* Other frame specific data may follow. */
} VRDEVIDEOINFRAMEDESC;

/* VRDEVIDEOINFRAMEDESC::u8FrameFlags. */
#define VRDE_VIDEOIN_F_FRM_STILL              0x01 /* If still images are supported for this frame. */
#define VRDE_VIDEOIN_F_FRM_DISCRETE_INTERVALS 0x02 /* If the discrete intervals list is included. */
#define VRDE_VIDEOIN_F_FRM_BITRATE            0x04 /* If the bitrate fields are included. */
#define VRDE_VIDEOIN_F_FRM_SIZE_OF_FIELDS     0x08 /* If the all optional fields start with 16 bit field size. */

/*
 * Controls.
 *
 * The same structures are used for both SET and GET requests.
 * Requests are async. A callback is invoked, when the client returns a reply.
 * A control change notification also uses these structures.
 *
 * If a control request can not be fulfilled, then VRDE_VIDEOIN_CTRLHDR_F_FAIL
 * will be set and u8Status contains the error code. This replaces the VC_REQUEST_ERROR_CODE_CONTROL.
 *
 * If the client receives an unsupported control, then the client must ignore it.
 * That is the control request must not affect the client in any way.
 * The client may send a VRDEVIDEOINCTRLHDR response for the unsupported control with:
 *   u16ControlSelector = the received value;
 *   u16RequestType = the received value;
 *   u16ParmSize = 0;
 *   u8Flags = VRDE_VIDEOIN_CTRLHDR_F_FAIL;
 *   u8Status = VRDE_VIDEOIN_CTRLHDR_STATUS_INVALIDCONTROL;
 */

typedef struct VRDEVIDEOINCTRLHDR
{
    uint16_t u16ControlSelector; /* VRDE_VIDEOIN_CTRLSEL_* */
    uint16_t u16RequestType;     /* VRDE_VIDEOIN_CTRLREQ_* */
    uint16_t u16ParmSize;        /* The size of the control specific parameters. */
    uint8_t u8Flags;             /* VRDE_VIDEOIN_CTRLHDR_F_* */
    uint8_t u8Status;            /* VRDE_VIDEOIN_CTRLHDR_STATUS_* */
    /* Control specific data follows. */
} VRDEVIDEOINCTRLHDR;

/* Control request types: VRDEVIDEOINCTRLHDR::u16RequestType. */
#define VRDE_VIDEOIN_CTRLREQ_UNDEFINED 0x00
#define VRDE_VIDEOIN_CTRLREQ_SET_CUR   0x01
#define VRDE_VIDEOIN_CTRLREQ_GET_CUR   0x81
#define VRDE_VIDEOIN_CTRLREQ_GET_MIN   0x82
#define VRDE_VIDEOIN_CTRLREQ_GET_MAX   0x83
#define VRDE_VIDEOIN_CTRLREQ_GET_RES   0x84
#define VRDE_VIDEOIN_CTRLREQ_GET_LEN   0x85
#define VRDE_VIDEOIN_CTRLREQ_GET_INFO  0x86
#define VRDE_VIDEOIN_CTRLREQ_GET_DEF   0x87

/* VRDEVIDEOINCTRLHDR::u8Flags */
#define VRDE_VIDEOIN_CTRLHDR_F_NOTIFY 0x01 /* Control change notification, the attribute is derived from u16RequestType and F_FAIL. */
#define VRDE_VIDEOIN_CTRLHDR_F_FAIL   0x02 /* The operation failed. Error code is in u8Status. */

/* VRDEVIDEOINCTRLHDR::u8Status if the VRDE_VIDEOIN_CTRLHDR_F_FAIL is set. */
#define VRDE_VIDEOIN_CTRLHDR_STATUS_SUCCESS        0x00 /**/
#define VRDE_VIDEOIN_CTRLHDR_STATUS_NOTREADY       0x01 /* Not ready */
#define VRDE_VIDEOIN_CTRLHDR_STATUS_WRONGSTATE     0x02 /* Wrong state */
#define VRDE_VIDEOIN_CTRLHDR_STATUS_POWER          0x03 /* Power */
#define VRDE_VIDEOIN_CTRLHDR_STATUS_OUTOFRANGE     0x04 /* Out of range */
#define VRDE_VIDEOIN_CTRLHDR_STATUS_INVALIDUNIT    0x05 /* Invalid unit */
#define VRDE_VIDEOIN_CTRLHDR_STATUS_INVALIDCONTROL 0x06 /* Invalid control */
#define VRDE_VIDEOIN_CTRLHDR_STATUS_INVALIDREQUEST 0x07 /* Invalid request */
#define VRDE_VIDEOIN_CTRLHDR_STATUS_UNKNOWN        0xFF /* Unknown */

/* Control selectors. 16 bit. High byte is the category. Low byte is the identifier.*/
#ifdef RT_MAKE_U16
#define VRDE_VIDEOIN_CTRLSEL_MAKE(Lo, Hi) RT_MAKE_U16(Lo, Hi)
#else
#define VRDE_VIDEOIN_CTRLSEL_MAKE(Lo, Hi) ((uint16_t)( (uint16_t)((uint8_t)(Hi)) << 8 | (uint8_t)(Lo) ))
#endif

#define VRDE_VIDEOIN_CTRLSEL_VC(a) VRDE_VIDEOIN_CTRLSEL_MAKE(a, 0x01)
#define VRDE_VIDEOIN_CTRLSEL_CT(a) VRDE_VIDEOIN_CTRLSEL_MAKE(a, 0x02)
#define VRDE_VIDEOIN_CTRLSEL_PU(a) VRDE_VIDEOIN_CTRLSEL_MAKE(a, 0x03)
#define VRDE_VIDEOIN_CTRLSEL_VS(a) VRDE_VIDEOIN_CTRLSEL_MAKE(a, 0x04)
#define VRDE_VIDEOIN_CTRLSEL_HW(a) VRDE_VIDEOIN_CTRLSEL_MAKE(a, 0x05)
#define VRDE_VIDEOIN_CTRLSEL_PROT(a) VRDE_VIDEOIN_CTRLSEL_MAKE(a, 0x06)

#define VRDE_VIDEOIN_CTRLSEL_VC_VIDEO_POWER_MODE_CONTROL   VRDE_VIDEOIN_CTRLSEL_VC(0x01)

#define VRDE_VIDEOIN_CTRLSEL_CT_UNDEFINED                 VRDE_VIDEOIN_CTRLSEL_CT(0x00)
#define VRDE_VIDEOIN_CTRLSEL_CT_SCANNING_MODE             VRDE_VIDEOIN_CTRLSEL_CT(0x01)
#define VRDE_VIDEOIN_CTRLSEL_CT_AE_MODE                   VRDE_VIDEOIN_CTRLSEL_CT(0x02)
#define VRDE_VIDEOIN_CTRLSEL_CT_AE_PRIORITY               VRDE_VIDEOIN_CTRLSEL_CT(0x03)
#define VRDE_VIDEOIN_CTRLSEL_CT_EXPOSURE_TIME_ABSOLUTE    VRDE_VIDEOIN_CTRLSEL_CT(0x04)
#define VRDE_VIDEOIN_CTRLSEL_CT_EXPOSURE_TIME_RELATIVE    VRDE_VIDEOIN_CTRLSEL_CT(0x05)
#define VRDE_VIDEOIN_CTRLSEL_CT_FOCUS_ABSOLUTE            VRDE_VIDEOIN_CTRLSEL_CT(0x06)
#define VRDE_VIDEOIN_CTRLSEL_CT_FOCUS_RELATIVE            VRDE_VIDEOIN_CTRLSEL_CT(0x07)
#define VRDE_VIDEOIN_CTRLSEL_CT_FOCUS_AUTO                VRDE_VIDEOIN_CTRLSEL_CT(0x08)
#define VRDE_VIDEOIN_CTRLSEL_CT_IRIS_ABSOLUTE             VRDE_VIDEOIN_CTRLSEL_CT(0x09)
#define VRDE_VIDEOIN_CTRLSEL_CT_IRIS_RELATIVE             VRDE_VIDEOIN_CTRLSEL_CT(0x0A)
#define VRDE_VIDEOIN_CTRLSEL_CT_ZOOM_ABSOLUTE             VRDE_VIDEOIN_CTRLSEL_CT(0x0B)
#define VRDE_VIDEOIN_CTRLSEL_CT_ZOOM_RELATIVE             VRDE_VIDEOIN_CTRLSEL_CT(0x0C)
#define VRDE_VIDEOIN_CTRLSEL_CT_PANTILT_ABSOLUTE          VRDE_VIDEOIN_CTRLSEL_CT(0x0D)
#define VRDE_VIDEOIN_CTRLSEL_CT_PANTILT_RELATIVE          VRDE_VIDEOIN_CTRLSEL_CT(0x0E)
#define VRDE_VIDEOIN_CTRLSEL_CT_ROLL_ABSOLUTE             VRDE_VIDEOIN_CTRLSEL_CT(0x0F)
#define VRDE_VIDEOIN_CTRLSEL_CT_ROLL_RELATIVE             VRDE_VIDEOIN_CTRLSEL_CT(0x10)
#define VRDE_VIDEOIN_CTRLSEL_CT_PRIVACY                   VRDE_VIDEOIN_CTRLSEL_CT(0x11)

#define VRDE_VIDEOIN_CTRLSEL_PU_UNDEFINED                 VRDE_VIDEOIN_CTRLSEL_PU(0x00)
#define VRDE_VIDEOIN_CTRLSEL_PU_BACKLIGHT_COMPENSATION    VRDE_VIDEOIN_CTRLSEL_PU(0x01)
#define VRDE_VIDEOIN_CTRLSEL_PU_BRIGHTNESS                VRDE_VIDEOIN_CTRLSEL_PU(0x02)
#define VRDE_VIDEOIN_CTRLSEL_PU_CONTRAST                  VRDE_VIDEOIN_CTRLSEL_PU(0x03)
#define VRDE_VIDEOIN_CTRLSEL_PU_GAIN                      VRDE_VIDEOIN_CTRLSEL_PU(0x04)
#define VRDE_VIDEOIN_CTRLSEL_PU_POWER_LINE_FREQUENCY      VRDE_VIDEOIN_CTRLSEL_PU(0x05)
#define VRDE_VIDEOIN_CTRLSEL_PU_HUE                       VRDE_VIDEOIN_CTRLSEL_PU(0x06)
#define VRDE_VIDEOIN_CTRLSEL_PU_SATURATION                VRDE_VIDEOIN_CTRLSEL_PU(0x07)
#define VRDE_VIDEOIN_CTRLSEL_PU_SHARPNESS                 VRDE_VIDEOIN_CTRLSEL_PU(0x08)
#define VRDE_VIDEOIN_CTRLSEL_PU_GAMMA                     VRDE_VIDEOIN_CTRLSEL_PU(0x09)
#define VRDE_VIDEOIN_CTRLSEL_PU_WHITE_BALANCE_TEMPERATURE VRDE_VIDEOIN_CTRLSEL_PU(0x0A)
#define VRDE_VIDEOIN_CTRLSEL_PU_WHITE_BALANCE_TEMPERATURE_AUTO VRDE_VIDEOIN_CTRLSEL_PU(0x0B)
#define VRDE_VIDEOIN_CTRLSEL_PU_WHITE_BALANCE_COMPONENT   VRDE_VIDEOIN_CTRLSEL_PU(0x0C)
#define VRDE_VIDEOIN_CTRLSEL_PU_WHITE_BALANCE_COMPONENT_AUTO VRDE_VIDEOIN_CTRLSEL_PU(0x0D)
#define VRDE_VIDEOIN_CTRLSEL_PU_DIGITAL_MULTIPLIER        VRDE_VIDEOIN_CTRLSEL_PU(0x0E)
#define VRDE_VIDEOIN_CTRLSEL_PU_DIGITAL_MULTIPLIER_LIMIT  VRDE_VIDEOIN_CTRLSEL_PU(0x0F)
#define VRDE_VIDEOIN_CTRLSEL_PU_HUE_AUTO                  VRDE_VIDEOIN_CTRLSEL_PU(0x10)
#define VRDE_VIDEOIN_CTRLSEL_PU_ANALOG_VIDEO_STANDARD     VRDE_VIDEOIN_CTRLSEL_PU(0x11)
#define VRDE_VIDEOIN_CTRLSEL_PU_ANALOG_LOCK_STATUS        VRDE_VIDEOIN_CTRLSEL_PU(0x12)

#define VRDE_VIDEOIN_CTRLSEL_VS_UNDEFINED                 VRDE_VIDEOIN_CTRLSEL_VS(0x00)
#define VRDE_VIDEOIN_CTRLSEL_VS_SETUP                     VRDE_VIDEOIN_CTRLSEL_VS(0x01)
#define VRDE_VIDEOIN_CTRLSEL_VS_OFF                       VRDE_VIDEOIN_CTRLSEL_VS(0x02)
#define VRDE_VIDEOIN_CTRLSEL_VS_ON                        VRDE_VIDEOIN_CTRLSEL_VS(0x03)
#define VRDE_VIDEOIN_CTRLSEL_VS_STILL_IMAGE_TRIGGER       VRDE_VIDEOIN_CTRLSEL_VS(0x05)
#define VRDE_VIDEOIN_CTRLSEL_VS_STREAM_ERROR_CODE         VRDE_VIDEOIN_CTRLSEL_VS(0x06)
#define VRDE_VIDEOIN_CTRLSEL_VS_GENERATE_KEY_FRAME        VRDE_VIDEOIN_CTRLSEL_VS(0x07)
#define VRDE_VIDEOIN_CTRLSEL_VS_UPDATE_FRAME_SEGMENT      VRDE_VIDEOIN_CTRLSEL_VS(0x08)
#define VRDE_VIDEOIN_CTRLSEL_VS_SYNCH_DELAY               VRDE_VIDEOIN_CTRLSEL_VS(0x09)

#define VRDE_VIDEOIN_CTRLSEL_HW_BUTTON                    VRDE_VIDEOIN_CTRLSEL_HW(0x01)

#define VRDE_VIDEOIN_CTRLSEL_PROT_PING                    VRDE_VIDEOIN_CTRLSEL_PROT(0x01)
#define VRDE_VIDEOIN_CTRLSEL_PROT_SAMPLING                VRDE_VIDEOIN_CTRLSEL_PROT(0x02)
#define VRDE_VIDEOIN_CTRLSEL_PROT_FRAMES                  VRDE_VIDEOIN_CTRLSEL_PROT(0x03)

typedef struct VRDEVIDEOINCTRL_VIDEO_POWER_MODE
{
    VRDEVIDEOINCTRLHDR hdr;
    uint8_t u8DevicePowerMode;
} VRDEVIDEOINCTRL_VIDEO_POWER_MODE;

typedef struct VRDEVIDEOINCTRL_CT_SCANNING_MODE
{
    VRDEVIDEOINCTRLHDR hdr;
    uint8_t u8ScanningMode;
} VRDEVIDEOINCTRL_CT_SCANNING_MODE;

typedef struct VRDEVIDEOINCTRL_CT_AE_MODE
{
    VRDEVIDEOINCTRLHDR hdr;
    uint8_t u8AutoExposureMode;
} VRDEVIDEOINCTRL_CT_AE_MODE;

typedef struct VRDEVIDEOINCTRL_CT_AE_PRIORITY
{
    VRDEVIDEOINCTRLHDR hdr;
    uint8_t u8AutoExposurePriority;
} VRDEVIDEOINCTRL_CT_AE_PRIORITY;

typedef struct VRDEVIDEOINCTRL_CT_EXPOSURE_TIME_ABSOLUTE
{
    VRDEVIDEOINCTRLHDR hdr;
    uint32_t u32ExposureTimeAbsolute;
} VRDEVIDEOINCTRL_CT_EXPOSURE_TIME_ABSOLUTE;

typedef struct VRDEVIDEOINCTRL_CT_EXPOSURE_TIME_RELATIVE
{
    VRDEVIDEOINCTRLHDR hdr;
    uint8_t u8ExposureTimeRelative;
} VRDEVIDEOINCTRL_CT_EXPOSURE_TIME_RELATIVE;

typedef struct VRDEVIDEOINCTRL_CT_FOCUS_ABSOLUTE
{
    VRDEVIDEOINCTRLHDR hdr;
    uint16_t u16FocusAbsolute;
} VRDEVIDEOINCTRL_CT_FOCUS_ABSOLUTE;

typedef struct VRDEVIDEOINCTRL_CT_FOCUS_RELATIVE
{
    VRDEVIDEOINCTRLHDR hdr;
    uint8_t u8FocusRelative;
    uint8_t u8Speed;
} VRDEVIDEOINCTRL_CT_FOCUS_RELATIVE;

typedef struct VRDEVIDEOINCTRL_CT_FOCUS_AUTO
{
    VRDEVIDEOINCTRLHDR hdr;
    uint8_t u8FocusAuto;
} VRDEVIDEOINCTRL_CT_FOCUS_AUTO;

typedef struct VRDEVIDEOINCTRL_CT_IRIS_ABSOLUTE
{
    VRDEVIDEOINCTRLHDR hdr;
    uint16_t u16IrisAbsolute;
} VRDEVIDEOINCTRL_CT_IRIS_ABSOLUTE;

typedef struct VRDEVIDEOINCTRL_CT_IRIS_RELATIVE
{
    VRDEVIDEOINCTRLHDR hdr;
    uint8_t u8IrisRelative;
} VRDEVIDEOINCTRL_CT_IRIS_RELATIVE;

typedef struct VRDEVIDEOINCTRL_CT_ZOOM_ABSOLUTE
{
    VRDEVIDEOINCTRLHDR hdr;
    uint16_t u16ZoomAbsolute;
} VRDEVIDEOINCTRL_CT_ZOOM_ABSOLUTE;

typedef struct VRDEVIDEOINCTRL_CT_ZOOM_RELATIVE
{
    VRDEVIDEOINCTRLHDR hdr;
    uint8_t u8Zoom;
    uint8_t u8DigitalZoom;
    uint8_t u8Speed;
} VRDEVIDEOINCTRL_CT_ZOOM_RELATIVE;

typedef struct VRDEVIDEOINCTRL_CT_PANTILT_ABSOLUTE
{
    VRDEVIDEOINCTRLHDR hdr;
    uint32_t u32PanAbsolute;
    uint32_t u32TiltAbsolute;
} VRDEVIDEOINCTRL_CT_PANTILT_ABSOLUTE;

typedef struct VRDEVIDEOINCTRL_CT_PANTILT_RELATIVE
{
    VRDEVIDEOINCTRLHDR hdr;
    uint8_t u8PanRelative;
    uint8_t u8PanSpeed;
    uint8_t u8TiltRelative;
    uint8_t u8TiltSpeed;
} VRDEVIDEOINCTRL_CT_PANTILT_RELATIVE;

typedef struct VRDEVIDEOINCTRL_CT_ROLL_ABSOLUTE
{
    VRDEVIDEOINCTRLHDR hdr;
    uint16_t u16RollAbsolute;
} VRDEVIDEOINCTRL_CT_ROLL_ABSOLUTE;

typedef struct VRDEVIDEOINCTRL_CT_ROLL_RELATIVE
{
    VRDEVIDEOINCTRLHDR hdr;
    uint8_t u8RollRelative;
    uint8_t u8Speed;
} VRDEVIDEOINCTRL_CT_ROLL_RELATIVE;

typedef struct VRDEVIDEOINCTRL_CT_PRIVACY_MODE
{
    VRDEVIDEOINCTRLHDR hdr;
    uint8_t u8Privacy;
} VRDEVIDEOINCTRL_CT_PRIVACY_MODE;

typedef struct VRDEVIDEOINCTRL_PU_BACKLIGHT_COMPENSATION
{
    VRDEVIDEOINCTRLHDR hdr;
    uint16_t u16BacklightCompensation;
} VRDEVIDEOINCTRL_PU_BACKLIGHT_COMPENSATION;

typedef struct VRDEVIDEOINCTRL_PU_BRIGHTNESS
{
    VRDEVIDEOINCTRLHDR hdr;
    uint16_t u16Brightness;
} VRDEVIDEOINCTRL_PU_BRIGHTNESS;

typedef struct VRDEVIDEOINCTRL_PU_CONTRAST
{
    VRDEVIDEOINCTRLHDR hdr;
    uint16_t u16Contrast;
} VRDEVIDEOINCTRL_PU_CONTRAST;

typedef struct VRDEVIDEOINCTRL_PU_GAIN
{
    VRDEVIDEOINCTRLHDR hdr;
    uint16_t u16Gain;
} VRDEVIDEOINCTRL_PU_GAIN;

typedef struct VRDEVIDEOINCTRL_PU_POWER_LINE_FREQUENCY
{
    VRDEVIDEOINCTRLHDR hdr;
    uint16_t u16PowerLineFrequency;
} VRDEVIDEOINCTRL_PU_POWER_LINE_FREQUENCY;

typedef struct VRDEVIDEOINCTRL_PU_HUE
{
    VRDEVIDEOINCTRLHDR hdr;
    uint16_t u16Hue;
} VRDEVIDEOINCTRL_PU_HUE;

typedef struct VRDEVIDEOINCTRL_PU_HUE_AUTO
{
    VRDEVIDEOINCTRLHDR hdr;
    uint8_t u8HueAuto;
} VRDEVIDEOINCTRL_PU_HUE_AUTO;

typedef struct VRDEVIDEOINCTRL_PU_SATURATION
{
    VRDEVIDEOINCTRLHDR hdr;
    uint16_t u16Saturation;
} VRDEVIDEOINCTRL_PU_SATURATION;

typedef struct VRDEVIDEOINCTRL_PU_SHARPNESS
{
    VRDEVIDEOINCTRLHDR hdr;
    uint16_t u16Sharpness;
} VRDEVIDEOINCTRL_PU_SHARPNESS;

typedef struct VRDEVIDEOINCTRL_PU_GAMMA
{
    VRDEVIDEOINCTRLHDR hdr;
    uint16_t u16Gamma;
} VRDEVIDEOINCTRL_PU_GAMMA;

typedef struct VRDEVIDEOINCTRL_PU_WHITE_BALANCE_TEMPERATURE
{
    VRDEVIDEOINCTRLHDR hdr;
    uint16_t u16WhiteBalanceTemperature;
} VRDEVIDEOINCTRL_PU_WHITE_BALANCE_TEMPERATURE;

typedef struct VRDEVIDEOINCTRL_PU_WHITE_BALANCE_TEMPERATURE_AUTO
{
    VRDEVIDEOINCTRLHDR hdr;
    uint8_t u8WhiteBalanceTemperatureAuto;
} VRDEVIDEOINCTRL_PU_WHITE_BALANCE_TEMPERATURE_AUTO;

typedef struct VRDEVIDEOINCTRL_PU_WHITE_BALANCE_COMPONENT
{
    VRDEVIDEOINCTRLHDR hdr;
    uint16_t u16WhiteBalanceBlue;
    uint16_t u16WhiteBalanceRed;
} VRDEVIDEOINCTRL_PU_WHITE_BALANCE_COMPONENT;

typedef struct VRDEVIDEOINCTRL_PU_WHITE_BALANCE_COMPONENT_AUTO
{
    VRDEVIDEOINCTRLHDR hdr;
    uint8_t u8WhiteBalanceComponentAuto;
} VRDEVIDEOINCTRL_PU_WHITE_BALANCE_COMPONENT_AUTO;

typedef struct VRDEVIDEOINCTRL_PU_DIGITAL_MULTIPLIER
{
    VRDEVIDEOINCTRLHDR hdr;
    uint16_t u16MultiplierStep;
} VRDEVIDEOINCTRL_PU_DIGITAL_MULTIPLIER;

typedef struct VRDEVIDEOINCTRL_PU_DIGITAL_MULTIPLIER_LIMIT
{
    VRDEVIDEOINCTRLHDR hdr;
    uint16_t u16MultiplierLimit;
} VRDEVIDEOINCTRL_PU_DIGITAL_MULTIPLIER_LIMIT;

typedef struct VRDEVIDEOINCTRL_PU_ANALOG_VIDEO_STANDARD
{
    VRDEVIDEOINCTRLHDR hdr;
    uint8_t u8VideoStandard;
} VRDEVIDEOINCTRL_PU_ANALOG_VIDEO_STANDARD;

typedef struct VRDEVIDEOINCTRL_PU_ANALOG_LOCK_STATUS
{
    VRDEVIDEOINCTRLHDR hdr;
    uint8_t u8Status;
} VRDEVIDEOINCTRL_PU_ANALOG_LOCK_STATUS;

/* Set streaming parameters. The actual streaming will be enabled by VS_ON. */
#define VRDEVIDEOINCTRL_F_VS_SETUP_FID 0x01
#define VRDEVIDEOINCTRL_F_VS_SETUP_EOF 0x02

typedef struct VRDEVIDEOINCTRL_VS_SETUP
{
    VRDEVIDEOINCTRLHDR hdr;
    uint8_t u8FormatId;             /* The format id on the client: VRDEVIDEOINFORMATDESC::u8FormatId. */
    uint8_t u8FramingInfo;          /* VRDEVIDEOINCTRL_F_VS_SETUP_*. Set by the client. */
    uint16_t u16Width;
    uint16_t u16Height;
    uint32_t u32FrameInterval;      /* Frame interval in 100 ns units, 0 means a still image capture.
                                     * The client may choose a different interval if this value is
                                     * not supported.
                                     */
    uint16_t u16CompQuality;        /* 0 .. 10000 = 0 .. 100%.
                                     * Applicable if the format has VRDE_VIDEOIN_F_FMT_COMPQUALITY,
                                     * otherwise this field is ignored.
                                     */
    uint16_t u16Delay;              /* Latency in ms from video data capture to presentation on the channel.
                                     * Set by the client, read by the server.
                                     */
    uint32_t u32ClockFrequency;     /* @todo just all clocks in 100ns units? */
} VRDEVIDEOINCTRL_VS_SETUP;

/* Stop sending video frames. */
typedef struct VRDEVIDEOINCTRL_VS_OFF
{
    VRDEVIDEOINCTRLHDR hdr;
} VRDEVIDEOINCTRL_VS_OFF;

/* Start sending video frames with parameters set by VS_SETUP. */
typedef struct VRDEVIDEOINCTRL_VS_ON
{
    VRDEVIDEOINCTRLHDR hdr;
} VRDEVIDEOINCTRL_VS_ON;

typedef struct VRDEVIDEOINCTRL_VS_STILL_IMAGE_TRIGGER
{
    VRDEVIDEOINCTRLHDR hdr;
    uint8_t u8Trigger;
} VRDEVIDEOINCTRL_VS_STILL_IMAGE_TRIGGER;

typedef struct VRDEVIDEOINCTRL_VS_STREAM_ERROR_CODE
{
    VRDEVIDEOINCTRLHDR hdr;
    uint8_t u8StreamErrorCode;
} VRDEVIDEOINCTRL_VS_STREAM_ERROR_CODE;

typedef struct VRDEVIDEOINCTRL_VS_GENERATE_KEY_FRAME
{
    VRDEVIDEOINCTRLHDR hdr;
    uint8_t u8GenerateKeyFrame;
} VRDEVIDEOINCTRL_VS_GENERATE_KEY_FRAME;

typedef struct VRDEVIDEOINCTRL_VS_UPDATE_FRAME_SEGMENT
{
    VRDEVIDEOINCTRLHDR hdr;
    uint8_t u8StartFrameSegment;
    uint8_t u8EndFrameSegment;
} VRDEVIDEOINCTRL_VS_UPDATE_FRAME_SEGMENT;

typedef struct VRDEVIDEOINCTRL_VS_SYNCH_DELAY
{
    VRDEVIDEOINCTRLHDR hdr;
    uint16_t u16Delay;
} VRDEVIDEOINCTRL_VS_SYNCH_DELAY;

/* A hardware button was pressed/released on the device. */
typedef struct VRDEVIDEOINCTRL_HW_BUTTON
{
    VRDEVIDEOINCTRLHDR hdr;
    uint8_t u8Pressed;
} VRDEVIDEOINCTRL_HW_BUTTON;

typedef struct VRDEVIDEOINCTRL_PROT_PING
{
    VRDEVIDEOINCTRLHDR hdr;
    uint32_t u32Timestamp;      /* Set in the request and the same value must be send back in the response. */
} VRDEVIDEOINCTRL_PROT_PING;

typedef struct VRDEVIDEOINCTRL_PROT_SAMPLING
{
    VRDEVIDEOINCTRLHDR hdr;
    uint32_t fu32SampleStart;   /* Which parameters must be sampled VRDEVIDEOINCTRL_F_PROT_SAMPLING_*. */
    uint32_t fu32SampleStop;    /* Which parameters to disable VRDEVIDEOINCTRL_F_PROT_SAMPLING_*.
                                 * If both Start and Stop is set, then restart the sampling.
                                 */
    uint32_t u32PeriodMS;       /* Sampling period in milliseconds. Applies to all samples in fu32SampleStart.
                                 * Not mandatory, the actual sampling period may be different.
                                 */
} VRDEVIDEOINCTRL_PROT_SAMPLING;

#define VRDEVIDEOINCTRL_F_PROT_SAMPLING_FRAMES_SOURCE     0x00000001 /* Periodic VRDEVIDEOINCTRL_PROT_FRAMES samples */
#define VRDEVIDEOINCTRL_F_PROT_SAMPLING_FRAMES_CLIENT_OUT 0x00000002 /* Periodic VRDEVIDEOINCTRL_PROT_FRAMES samples */

typedef struct VRDEVIDEOINCTRL_PROT_FRAMES
{
    VRDEVIDEOINCTRLHDR hdr;     /* Note: the message should be sent as VRDE_VIDEOIN_FN_CONTROL_NOTIFY. */
    uint32_t u32Sample;         /* Which sample is this, one of VRDEVIDEOINCTRL_F_PROT_SAMPLING_*. */
    uint32_t u32TimestampMS;    /* When the period started, milliseconds since the start of sampling. */
    uint32_t u32PeriodMS;       /* Actual period during which the frames were counted in milliseconds.
                                 * This may be different from VRDEVIDEOINCTRL_PROT_SAMPLING::u32PeriodMS.
                                 */
    uint32_t u32FramesCount;    /* How many frames per u32PeriodMS milliseconds. */
} VRDEVIDEOINCTRL_PROT_FRAMES;


/*
 * Payload transfers. How frames are sent to the server:
 * the client send a PAYLOAD packet, which has the already set format.
 * The server enables the transfers by sending VRDEVIDEOINCTRL_VS_ON.
 */

/* Payload header */
typedef struct VRDEVIDEOINPAYLOADHDR
{
    uint8_t u8HeaderLength;       /* Entire header. */
    uint8_t u8HeaderInfo;         /* VRDE_VIDEOIN_PAYLOAD_F_* */
    uint32_t u32PresentationTime; /* @todo define this */
    uint32_t u32SourceTimeClock;  /* @todo At the moment when the frame was sent to the channel.
                                   * Allows the server to measure clock drift.
                                   */
    uint16_t u16Reserved;         /* @todo */
} VRDEVIDEOINPAYLOADHDR;

/* VRDEVIDEOINPAYLOADHDR::u8HeaderInfo */
#define VRDE_VIDEOIN_PAYLOAD_F_FID     0x01 /* Frame ID */
#define VRDE_VIDEOIN_PAYLOAD_F_EOF     0x02 /* End of Frame */
#define VRDE_VIDEOIN_PAYLOAD_F_PTS     0x04 /* Presentation Time */
#define VRDE_VIDEOIN_PAYLOAD_F_SCR     0x08 /* Source Clock Reference */
#define VRDE_VIDEOIN_PAYLOAD_F_RES     0x10 /* Reserved */
#define VRDE_VIDEOIN_PAYLOAD_F_STI     0x20 /* Still Image */
#define VRDE_VIDEOIN_PAYLOAD_F_ERR     0x40 /* Error */
#define VRDE_VIDEOIN_PAYLOAD_F_EOH     0x80 /* End of header */


/*
 * The network channel specification.
 */

/*
 * The protocol uses a dynamic RDP channel.
 * Everything is little-endian.
 */

/* The dynamic RDP channel name. */
#define VRDE_VIDEOIN_CHANNEL "RVIDEOIN"

/* Major functions. */
#define VRDE_VIDEOIN_FN_NEGOTIATE  0x0000 /* Version and capabilities check. */
#define VRDE_VIDEOIN_FN_NOTIFY     0x0001 /* Device attach/detach from the client. */
#define VRDE_VIDEOIN_FN_DEVICEDESC 0x0002 /* Query device description. */
#define VRDE_VIDEOIN_FN_CONTROL    0x0003 /* Control the device and start/stop video input.
                                           * This function is used for sending a request and
                                           * the corresponding response.
                                           */
#define VRDE_VIDEOIN_FN_CONTROL_NOTIFY 0x0004 /* The client reports a control change, etc.
                                               * This function indicated that the message is
                                               * not a response to a CONTROL request.
                                               */
#define VRDE_VIDEOIN_FN_FRAME      0x0005 /* Frame from the client. */

/* Status codes. */
#define VRDE_VIDEOIN_STATUS_SUCCESS       0 /* Function completed successfully. */
#define VRDE_VIDEOIN_STATUS_FAILED        1 /* Failed for some reason. */

typedef struct VRDEVIDEOINMSGHDR
{
    uint32_t u32Length;     /* The length of the message in bytes, including the header. */
    uint32_t u32DeviceId;   /* The client's device id. */
    uint32_t u32MessageId;  /* Unique id assigned by the server. The client must send a reply with the same id.
                             * If the client initiates a request, then this must be set to 0, because there is
                             * currently no client requests, which would require a response from the server.
                             */
    uint16_t u16FunctionId; /* VRDE_VIDEOIN_FN_* */
    uint16_t u16Status;     /* The result of a request. VRDE_VIDEOIN_STATUS_*. */
} VRDEVIDEOINMSGHDR;
ASSERTSIZE(VRDEVIDEOINMSGHDR, 16)

/*
 * VRDE_VIDEOIN_FN_NEGOTIATE
 *
 * Sent by the server when the channel is established and the client replies with its capabilities.
 */
#define VRDE_VIDEOIN_NEGOTIATE_VERSION 1

/* VRDEVIDEOINMSG_NEGOTIATE::fu32Capabilities */
#define VRDE_VIDEOIN_NEGOTIATE_CAP_VOID 0x00000000
#define VRDE_VIDEOIN_NEGOTIATE_CAP_PROT 0x00000001 /* Supports VRDE_VIDEOIN_CTRLSEL_PROT_* controls. */

typedef struct VRDEVIDEOINMSG_NEGOTIATE
{
    VRDEVIDEOINMSGHDR hdr;
    uint32_t u32Version;       /* VRDE_VIDEOIN_NEGOTIATE_VERSION */
    uint32_t fu32Capabilities; /* VRDE_VIDEOIN_NEGOTIATE_CAP_* */
} VRDEVIDEOINMSG_NEGOTIATE;

/*
 * VRDE_VIDEOIN_FN_NOTIFY
 *
 * Sent by the client when a webcam is attached or detached.
 * The client must send the ATTACH notification for each webcam, which is
 * already connected to the client when the VIDEOIN channel is established.
 */
#define VRDE_VIDEOIN_NOTIFY_EVENT_ATTACH 0
#define VRDE_VIDEOIN_NOTIFY_EVENT_DETACH 1
#define VRDE_VIDEOIN_NOTIFY_EVENT_NEGOTIATE 2 /* Negotiate again with the client. */

typedef struct VRDEVIDEOINMSG_NOTIFY
{
    VRDEVIDEOINMSGHDR hdr;
    uint32_t u32NotifyEvent; /* VRDE_VIDEOIN_NOTIFY_EVENT_* */
    /* Event specific data may follow. The underlying protocol provides the length of the message. */
} VRDEVIDEOINMSG_NOTIFY;

/*
 * VRDE_VIDEOIN_FN_DEVICEDESC
 *
 * The server queries the description of a device.
 */
typedef struct VRDEVIDEOINMSG_DEVICEDESC_REQ
{
    VRDEVIDEOINMSGHDR hdr;
} VRDEVIDEOINMSG_DEVICEDESC_REQ;

typedef struct VRDEVIDEOINMSG_DEVICEDESC_RSP
{
    VRDEVIDEOINMSGHDR hdr;
    VRDEVIDEOINDEVICEDESC Device;
    /*
     * VRDEVIDEOINFORMATDESC[0]
     * VRDEVIDEOINFRAMEDESC[0]
     * ...
     * VRDEVIDEOINFRAMEDESC[n]
     * VRDEVIDEOINFORMATDESC[1]
     * VRDEVIDEOINFRAMEDESC[0]
     * ...
     * VRDEVIDEOINFRAMEDESC[m]
     * ...
     */
} VRDEVIDEOINMSG_DEVICEDESC_RSP;

/*
 * VRDE_VIDEOIN_FN_CONTROL
 * VRDE_VIDEOIN_FN_CONTROL_NOTIFY
 *
 * Either sent by the server or by the client as a notification/response.
 * If sent by the client as a notification, then hdr.u32MessageId must be 0.
 */
typedef struct VRDEVIDEOINMSG_CONTROL
{
    VRDEVIDEOINMSGHDR hdr;
    VRDEVIDEOINCTRLHDR Control;
    /* Control specific data may follow. */
} VRDEVIDEOINMSG_CONTROL;

/*
 * VRDE_VIDEOIN_FN_FRAME
 *
 * The client sends a video/still frame in the already specified format.
 * hdr.u32MessageId must be 0.
 */
typedef struct VRDEVIDEOINMSG_FRAME
{
    VRDEVIDEOINMSGHDR hdr;
    VRDEVIDEOINPAYLOADHDR Payload;
    /* The frame data follow. */
} VRDEVIDEOINMSG_FRAME;


#ifdef VRDE_VIDEOIN_WITH_VRDEINTERFACE
/*
 * The application interface between VirtualBox and the VRDE server.
 */

#define VRDE_VIDEOIN_INTERFACE_NAME "VIDEOIN"

typedef struct VRDEVIDEOINDEVICEHANDLE
{
    uint32_t u32ClientId;
    uint32_t u32DeviceId;
} VRDEVIDEOINDEVICEHANDLE;

/* The VRDE server video input interface entry points. Interface version 1. */
typedef struct VRDEVIDEOININTERFACE
{
    /* The header. */
    VRDEINTERFACEHDR header;

    /* Tell the server that this device will be used and associate a context with the device.
     *
     * @param hServer The VRDE server instance.
     * @param pDeviceHandle The device reported by ATTACH notification.
     * @param pvDeviceCtx The caller context associated with the pDeviceHandle.
     *
     * @return IPRT status code.
     */
    DECLR3CALLBACKMEMBER(int, VRDEVideoInDeviceAttach, (HVRDESERVER hServer,
                                                        const VRDEVIDEOINDEVICEHANDLE *pDeviceHandle,
                                                        void *pvDeviceCtx));

    /* This device will be not be used anymore. The device context must not be used by the server too.
     *
     * @param hServer The VRDE server instance.
     * @param pDeviceHandle The device reported by ATTACH notification.
     *
     * @return IPRT status code.
     */
    DECLR3CALLBACKMEMBER(int, VRDEVideoInDeviceDetach, (HVRDESERVER hServer,
                                                        const VRDEVIDEOINDEVICEHANDLE *pDeviceHandle));

    /* Get a device description.
     *
     * @param hServer The VRDE server instance.
     * @param pvUser  The callers context of this request.
     * @param pDeviceHandle The device reported by ATTACH notification.
     *
     * @return IPRT status code.
     */
    DECLR3CALLBACKMEMBER(int, VRDEVideoInGetDeviceDesc, (HVRDESERVER hServer,
                                                         void *pvUser,
                                                         const VRDEVIDEOINDEVICEHANDLE *pDeviceHandle));

    /* Submit a set/get control request.
     *
     * @param hServer The VRDE server instance.
     * @param pvUser  The callers context of this request.
     * @param pDeviceHandle The device reported by ATTACH notification.
     * @param pReq  The request.
     * @param cbReq Size of the request.
     *
     * @return IPRT status code.
     */
    DECLR3CALLBACKMEMBER(int, VRDEVideoInControl, (HVRDESERVER hServer,
                                                   void *pvUser,
                                                   const VRDEVIDEOINDEVICEHANDLE *pDeviceHandle,
                                                   const VRDEVIDEOINCTRLHDR *pReq,
                                                   uint32_t cbReq));

} VRDEVIDEOININTERFACE;


/*
 * Notifications.
 * Data structures: pvData of VRDEVIDEOINCALLBACKS::VRDECallbackVideoInNotify.
 */
typedef struct VRDEVIDEOINNOTIFYATTACH
{
    VRDEVIDEOINDEVICEHANDLE deviceHandle;
    uint32_t u32Version;       /* VRDE_VIDEOIN_NEGOTIATE_VERSION */
    uint32_t fu32Capabilities; /* VRDE_VIDEOIN_NEGOTIATE_CAP_* */
} VRDEVIDEOINNOTIFYATTACH;

typedef struct VRDEVIDEOINNOTIFYDETACH
{
    VRDEVIDEOINDEVICEHANDLE deviceHandle;
} VRDEVIDEOINNOTIFYDETACH;

/* Notification codes, */
#define VRDE_VIDEOIN_NOTIFY_ID_ATTACH 0
#define VRDE_VIDEOIN_NOTIFY_ID_DETACH 1


/* Video input interface callbacks. */
typedef struct VRDEVIDEOINCALLBACKS
{
    /* The header. */
    VRDEINTERFACEHDR header;

    /* Notifications.
     *
     * @param pvCallback The callbacks context specified in VRDEGetInterface.
     * @param u32EventId The notification identifier: VRDE_VIDEOIN_NOTIFY_*.
     * @param pvData     The notification specific data.
     * @param cbData     The size of buffer pointed by pvData.
     */
    DECLR3CALLBACKMEMBER(void, VRDECallbackVideoInNotify,(void *pvCallback,
                                                          uint32_t u32Id,
                                                          const void *pvData,
                                                          uint32_t cbData));

    /* Device description received from the client.
     *
     * @param pvCallback The callbacks context specified in VRDEGetInterface.
     * @param rcRequest  The result code of the request.
     * @param pDeviceCtx The device context associated with the device in VRDEVideoInGetDeviceDesc.
     * @param pvUser     The pvUser parameter of VRDEVideoInGetDeviceDesc.
     * @param pDeviceDesc The device description.
     * @param cbDeviceDesc The size of buffer pointed by pDevice.
     */
    DECLR3CALLBACKMEMBER(void, VRDECallbackVideoInDeviceDesc,(void *pvCallback,
                                                              int rcRequest,
                                                              void *pDeviceCtx,
                                                              void *pvUser,
                                                              const VRDEVIDEOINDEVICEDESC *pDeviceDesc,
                                                              uint32_t cbDeviceDesc));

    /* Control response or notification.
     *
     * @param pvCallback The callbacks context specified in VRDEGetInterface.
     * @param rcRequest  The result code of the request.
     * @param pDeviceCtx The device context associated with the device in VRDEVideoInGetDeviceDesc.
     * @param pvUser     The pvUser parameter of VRDEVideoInControl. NULL if this is a notification.
     * @param pControl   The control information.
     * @param cbControl  The size of buffer pointed by pControl.
     */
    DECLR3CALLBACKMEMBER(void, VRDECallbackVideoInControl,(void *pvCallback,
                                                           int rcRequest,
                                                           void *pDeviceCtx,
                                                           void *pvUser,
                                                           const VRDEVIDEOINCTRLHDR *pControl,
                                                           uint32_t cbControl));

    /* Frame which was received from the client.
     *
     * @param pvCallback The callbacks context specified in VRDEGetInterface.
     * @param rcRequest  The result code of the request.
     * @param pDeviceCtx The device context associated with the device in VRDEVideoInGetDeviceDesc.
     * @param pFrame     The frame data.
     * @param cbFrame    The size of buffer pointed by pFrame.
     */
    DECLR3CALLBACKMEMBER(void, VRDECallbackVideoInFrame,(void *pvCallback,
                                                         int rcRequest,
                                                         void *pDeviceCtx,
                                                         const VRDEVIDEOINPAYLOADHDR *pFrame,
                                                         uint32_t cbFrame));

} VRDEVIDEOINCALLBACKS;
#endif /* VRDE_VIDEOIN_WITH_VRDEINTERFACE */

#pragma pack()

#endif
