/* $XFree86: xc/programs/Xserver/hw/xfree86/os-support/xf86OSmouse.h,v 1.20 2002/12/17 20:55:23 dawes Exp $ */

/*
 * Copyright (c) 1997-1999 by The XFree86 Project, Inc.
 */

/* Public interface to OS-specific mouse support. */

#ifndef _XF86OSMOUSE_H_
#define _XF86OSMOUSE_H_

#include "xf86Xinput.h"

/* Mouse interface classes */
#define MSE_NONE	0x00
#define MSE_SERIAL	0x01		/* serial port */
#define MSE_BUS		0x02		/* old bus mouse */
#define MSE_PS2		0x04		/* standard read-only PS/2 */
#define MSE_XPS2	0x08		/* extended PS/2 */
#define MSE_AUTO	0x10		/* auto-detect (PnP) */
#define MSE_MISC	0x20		/* The OS layer will identify the
					 * specific protocol names that are
					 * supported for this class. */

struct _MouseDevRec;

typedef int (*GetInterfaceTypesProc)(void);
typedef const char **(*BuiltinNamesProc)(void);
typedef Bool (*CheckProtocolProc)(const char *protocol);
typedef Bool (*BuiltinPreInitProc)(InputInfoPtr pInfo, const char *protocol,
				   int flags);
typedef const char *(*DefaultProtocolProc)(void);
typedef const char *(*SetupAutoProc)(InputInfoPtr pInfo, int *protoPara);
typedef void (*SetResProc)(InputInfoPtr pInfo, const char* protocol, int rate,
			   int res);
typedef void (*checkMovementsProc)(InputInfoPtr,int, int);
typedef void (*autoProbeProc)(InputInfoPtr, Bool, Bool);
typedef Bool (*collectDataProc)(struct _MouseDevRec *, unsigned char);
typedef Bool (*dataGoodProc)(struct _MouseDevRec *);

/*
 * OSMouseInfoRec is used to pass information from the OSMouse layer to the
 * OS-independent mouse driver.
 */
typedef struct {
	GetInterfaceTypesProc	SupportedInterfaces;
	BuiltinNamesProc	BuiltinNames;
	CheckProtocolProc	CheckProtocol;
	BuiltinPreInitProc	PreInit;
	DefaultProtocolProc	DefaultProtocol;
	SetupAutoProc		SetupAuto;
	SetResProc		SetPS2Res;
	SetResProc		SetBMRes;
	SetResProc		SetMiscRes;
} OSMouseInfoRec, *OSMouseInfoPtr;

/*
 * SupportedInterfaces: Returns the mouse interface types that the OS support.
 *		If MSE_MISC is returned, then the BuiltinNames and
 *		CheckProtocol should be set.
 *
 * BuiltinNames: Returns the names of the protocols that are fully handled
 *		in the OS-specific code.  These are names that don't appear
 *		directly in the main "mouse" driver.
 *
 * CheckProtocol: Checks if the protocol name given is supported by the
 *		OS.  It should return TRUE for both "builtin" protocols and
 *		protocols of type MSE_MISC that are supported by the OS.
 *
 * PreInit:	The PreInit function for protocols that are builtin.  This
 *		function is passed the protocol name.
 *
 * DefaultProtocol: Returns the name of a default protocol that should be used
 *		for the OS when none has been supplied in the config file.
 *		This should only be set when there is a reasonable default.
 *
 * SetupAuto:	This function can be used to do OS-specific protocol
 *		auto-detection.  It returns the name of the detected protocol,
 *		or NULL when detection fails.  It may also adjust one or more
 *		of the "protoPara" values for the detected protocol by setting
 *		then to something other than -1.
 *
 * SetPS2Res:	Set the resolution and sample rate for MSE_PS2 and MSE_XPS2
 *		protocol types.
 *
 * SetBMRes:	Set the resolution and sample rate for MSE_BM protocol types.
 *
 * SetMiscRes:	Set the resolution and sample rate for MSE_MISC protocol types.
 */

extern OSMouseInfoPtr xf86OSMouseInit(int flags);

/*
 * Mouse device record.  This is shared by the mouse driver and the OSMouse
 * layer.
 */

typedef void (*PostMseEventProc)(InputInfoPtr pInfo, int buttons,
			      int dx, int dy, int dz, int dw);
typedef void (*MouseCommonOptProc)(InputInfoPtr pInfo);

typedef struct _MouseDevRec {
    PtrCtrlProcPtr	Ctrl;
    PostMseEventProc	PostEvent;
    MouseCommonOptProc	CommonOptions;
    DeviceIntPtr	device;
    const char *	mseDevice;
    const char *	protocol;
    int			protocolID;
    int                 oldProtocolID; /* hack */
    int			class;
    int			mseModel;
    int			baudRate;
    int			oldBaudRate;
    int			sampleRate;
    int			lastButtons;
    int			threshold;	/* acceleration */
    int			num;
    int			den;
    int			buttons;	/* # of buttons */
    int			emulateState;	/* automata state for 2 button mode */
    Bool		emulate3Buttons;
    Bool		emulate3ButtonsSoft;
    int			emulate3Timeout;/* Timeout for 3 button emulation */
    Bool		chordMiddle;
    Bool                flipXY;
    int                 invX;
    int                 invY;
    int			mouseFlags;	/* Flags to Clear after opening
					 * mouse dev */
    int			truebuttons;	/* (not used)
					 * Arg to maintain before
					 * emulate3buttons timer callback */
    int			resolution;
    int			negativeZ;	/* button mask */
    int			positiveZ;	/* button mask */
    int			negativeW;	/* button mask */
    int			positiveW;	/* button mask */
    pointer		buffer;		/* usually an XISBuffer* */
    int			protoBufTail;
    unsigned char	protoBuf[8];
    unsigned char	protoPara[8];
    unsigned char	inSync;		/* driver in sync with datastream */
    pointer		mousePriv;	/* private area */
    InputInfoPtr	pInfo;
    int			origProtocolID;
    const char *	origProtocol;
    Bool		emulate3Pending;/* timer waiting */
    CARD32		emulate3Expires;/* time to fire emulation code */
    Bool		emulateWheel;
    int			wheelInertia;
    int			wheelButtonMask;
    int			negativeX;	/* Button values.  Unlike the Z and */
    int			positiveX;	/* W equivalents, these are button  */
    int			negativeY;	/* values rather than button masks. */
    int			positiveY;
    int			wheelYDistance;
    int			wheelXDistance;
    Bool		autoProbe;
    checkMovementsProc  checkMovements;
    autoProbeProc	autoProbeMouse;
    collectDataProc	collectData;
    dataGoodProc	dataGood;
    int			angleOffset;
    pointer		pDragLock;	/* drag lock area */
} MouseDevRec, *MouseDevPtr;

/* Z axis mapping */
#define MSE_NOZMAP	0
#define MSE_MAPTOX	-1
#define MSE_MAPTOY	-2
#define MSE_MAPTOZ	-3
#define MSE_MAPTOW	-4

/* Generalize for other axes. */
#define MSE_NOAXISMAP	MSE_NOZMAP

#define MSE_MAXBUTTONS	12
#define MSE_DFLTBUTTONS	 3

#endif /* _XF86OSMOUSE_H_ */
