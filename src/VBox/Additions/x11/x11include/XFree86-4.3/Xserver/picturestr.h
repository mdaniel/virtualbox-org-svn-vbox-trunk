/*
 * $XFree86: xc/programs/Xserver/render/picturestr.h,v 1.22 2002/11/23 02:38:15 keithp Exp $
 *
 * Copyright � 2000 SuSE, Inc.
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of SuSE not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission.  SuSE makes no representations about the
 * suitability of this software for any purpose.  It is provided "as is"
 * without express or implied warranty.
 *
 * SuSE DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL SuSE
 * BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN 
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Author:  Keith Packard, SuSE, Inc.
 */

#ifndef _PICTURESTR_H_
#define _PICTURESTR_H_

#include "glyphstr.h"
#include "scrnintstr.h"
#include "resource.h"

typedef struct _DirectFormat {
    CARD16	    red, redMask;
    CARD16	    green, greenMask;
    CARD16	    blue, blueMask;
    CARD16	    alpha, alphaMask;
} DirectFormatRec;

typedef struct _IndexFormat {
    VisualPtr	    pVisual;
    ColormapPtr	    pColormap;
    int		    nvalues;
    xIndexValue	    *pValues;
    void	    *devPrivate;
} IndexFormatRec;

typedef struct _PictFormat {
    CARD32	    id;
    CARD32	    format;	    /* except bpp */
    unsigned char   type;
    unsigned char   depth;
    DirectFormatRec direct;
    IndexFormatRec  index;
} PictFormatRec;

typedef struct _PictVector {
    xFixed	    vector[3];
} PictVector, *PictVectorPtr;

typedef struct _PictTransform {
    xFixed	    matrix[3][3];
} PictTransform, *PictTransformPtr;

typedef struct _Picture {
    DrawablePtr	    pDrawable;
    PictFormatPtr   pFormat;
    CARD32	    format;	    /* PICT_FORMAT */
    int		    refcnt;
    CARD32	    id;
    PicturePtr	    pNext;	    /* chain on same drawable */
    
    unsigned int    repeat : 1;
    unsigned int    graphicsExposures : 1;
    unsigned int    subWindowMode : 1;
    unsigned int    polyEdge : 1;
    unsigned int    polyMode : 1;
    unsigned int    freeCompClip : 1;
    unsigned int    clientClipType : 2;
    unsigned int    componentAlpha : 1;
    unsigned int    unused : 23;

    PicturePtr	    alphaMap;
    DDXPointRec	    alphaOrigin;

    DDXPointRec	    clipOrigin;
    pointer	    clientClip;

    Atom	    dither;

    unsigned long   stateChanges;
    unsigned long   serialNumber;

    RegionPtr	    pCompositeClip;
    
    DevUnion	    *devPrivates;
    
    PictTransform   *transform;

    int		    filter;
    xFixed	    *filter_params;
    int		    filter_nparams;
} PictureRec;

typedef struct {
    char	    *name;
    xFixed	    *params;
    int		    nparams;
    int		    id;
} PictFilterRec, *PictFilterPtr;

#define PictFilterNearest	0
#define PictFilterBilinear	1

#define PictFilterFast		2
#define PictFilterGood		3
#define PictFilterBest		4

typedef struct {
    char	    *alias;
    int		    alias_id;
    int		    filter_id;
} PictFilterAliasRec, *PictFilterAliasPtr;

typedef int	(*CreatePictureProcPtr)	    (PicturePtr pPicture);
typedef void	(*DestroyPictureProcPtr)    (PicturePtr pPicture);
typedef int	(*ChangePictureClipProcPtr) (PicturePtr	pPicture,
					     int	clipType,
					     pointer    value,
					     int	n);
typedef void	(*DestroyPictureClipProcPtr)(PicturePtr	pPicture);
    
typedef int	(*ChangePictureTransformProcPtr)    (PicturePtr	    pPicture,
						     PictTransform  *transform);

typedef int	(*ChangePictureFilterProcPtr)	(PicturePtr	pPicture,
						 int		filter,
						 xFixed		*params,
						 int		nparams);

typedef void	(*DestroyPictureFilterProcPtr)	(PicturePtr pPicture);

typedef void	(*ChangePictureProcPtr)	    (PicturePtr pPicture,
					     Mask	mask);
typedef void	(*ValidatePictureProcPtr)    (PicturePtr pPicture,
					     Mask       mask);
typedef void	(*CompositeProcPtr)	    (CARD8	op,
					     PicturePtr pSrc,
					     PicturePtr pMask,
					     PicturePtr pDst,
					     INT16	xSrc,
					     INT16	ySrc,
					     INT16	xMask,
					     INT16	yMask,
					     INT16	xDst,
					     INT16	yDst,
					     CARD16	width,
					     CARD16	height);

typedef void	(*GlyphsProcPtr)	    (CARD8      op,
					     PicturePtr pSrc,
					     PicturePtr pDst,
					     PictFormatPtr  maskFormat,
					     INT16      xSrc,
					     INT16      ySrc,
					     int	nlists,
					     GlyphListPtr   lists,
					     GlyphPtr	*glyphs);

typedef void	(*CompositeRectsProcPtr)    (CARD8	    op,
					     PicturePtr	    pDst,
					     xRenderColor   *color,
					     int	    nRect,
					     xRectangle	    *rects);

typedef void	(*RasterizeTrapezoidProcPtr)(PicturePtr	    pMask,
					     xTrapezoid	    *trap,
					     int	    x_off,
					     int	    y_off);

typedef void	(*TrapezoidsProcPtr)	    (CARD8	    op,
					     PicturePtr	    pSrc,
					     PicturePtr	    pDst,
					     PictFormatPtr  maskFormat,
					     INT16	    xSrc,
					     INT16	    ySrc,
					     int	    ntrap,
					     xTrapezoid	    *traps);

typedef void	(*TrianglesProcPtr)	    (CARD8	    op,
					     PicturePtr	    pSrc,
					     PicturePtr	    pDst,
					     PictFormatPtr  maskFormat,
					     INT16	    xSrc,
					     INT16	    ySrc,
					     int	    ntri,
					     xTriangle	    *tris);

typedef void	(*TriStripProcPtr)	    (CARD8	    op,
					     PicturePtr	    pSrc,
					     PicturePtr	    pDst,
					     PictFormatPtr  maskFormat,
					     INT16	    xSrc,
					     INT16	    ySrc,
					     int	    npoint,
					     xPointFixed    *points);

typedef void	(*TriFanProcPtr)	    (CARD8	    op,
					     PicturePtr	    pSrc,
					     PicturePtr	    pDst,
					     PictFormatPtr  maskFormat,
					     INT16	    xSrc,
					     INT16	    ySrc,
					     int	    npoint,
					     xPointFixed    *points);

typedef Bool	(*InitIndexedProcPtr)	    (ScreenPtr	    pScreen,
					     PictFormatPtr  pFormat);

typedef void	(*CloseIndexedProcPtr)	    (ScreenPtr	    pScreen,
					     PictFormatPtr  pFormat);

typedef void	(*UpdateIndexedProcPtr)	    (ScreenPtr	    pScreen,
					     PictFormatPtr  pFormat,
					     int	    ndef,
					     xColorItem	    *pdef);

typedef struct _PictureScreen {
    int				totalPictureSize;
    unsigned int		*PicturePrivateSizes;
    int				PicturePrivateLen;

    PictFormatPtr		formats;
    PictFormatPtr		fallback;
    int				nformats;
    
    CreatePictureProcPtr	CreatePicture;
    DestroyPictureProcPtr	DestroyPicture;
    ChangePictureClipProcPtr	ChangePictureClip;
    DestroyPictureClipProcPtr	DestroyPictureClip;
    
    ChangePictureProcPtr	ChangePicture;
    ValidatePictureProcPtr	ValidatePicture;

    CompositeProcPtr		Composite;
    GlyphsProcPtr		Glyphs;
    CompositeRectsProcPtr	CompositeRects;

    DestroyWindowProcPtr	DestroyWindow;
    CloseScreenProcPtr		CloseScreen;

    StoreColorsProcPtr		StoreColors;

    InitIndexedProcPtr		InitIndexed;
    CloseIndexedProcPtr		CloseIndexed;
    UpdateIndexedProcPtr	UpdateIndexed;

    int				subpixel;
    
    PictFilterPtr		filters;
    int				nfilters;
    PictFilterAliasPtr		filterAliases;
    int				nfilterAliases;

    ChangePictureTransformProcPtr   ChangePictureTransform;
    ChangePictureFilterProcPtr	ChangePictureFilter;
    DestroyPictureFilterProcPtr	DestroyPictureFilter;
    
    TrapezoidsProcPtr		Trapezoids;
    TrianglesProcPtr		Triangles;
    TriStripProcPtr		TriStrip;
    TriFanProcPtr		TriFan;

    RasterizeTrapezoidProcPtr	RasterizeTrapezoid;
} PictureScreenRec, *PictureScreenPtr;

extern int		PictureScreenPrivateIndex;
extern int		PictureWindowPrivateIndex;
extern RESTYPE		PictureType;
extern RESTYPE		PictFormatType;
extern RESTYPE		GlyphSetType;

#define GetPictureScreen(s) ((PictureScreenPtr) ((s)->devPrivates[PictureScreenPrivateIndex].ptr))
#define GetPictureScreenIfSet(s) ((PictureScreenPrivateIndex != -1) ? GetPictureScreen(s) : NULL)
#define SetPictureScreen(s,p) ((s)->devPrivates[PictureScreenPrivateIndex].ptr = (pointer) (p))
#define GetPictureWindow(w) ((PicturePtr) ((w)->devPrivates[PictureWindowPrivateIndex].ptr))
#define SetPictureWindow(w,p) ((w)->devPrivates[PictureWindowPrivateIndex].ptr = (pointer) (p))

#define VERIFY_PICTURE(pPicture, pid, client, mode, err) {\
    pPicture = SecurityLookupIDByType(client, pid, PictureType, mode);\
    if (!pPicture) { \
	client->errorValue = pid; \
	return err; \
    } \
}

#define VERIFY_ALPHA(pPicture, pid, client, mode, err) {\
    if (pid == None) \
	pPicture = 0; \
    else { \
	VERIFY_PICTURE(pPicture, pid, client, mode, err); \
    } \
} \

Bool
PictureDestroyWindow (WindowPtr pWindow);

Bool
PictureCloseScreen (int Index, ScreenPtr pScreen);

void
PictureStoreColors (ColormapPtr pColormap, int ndef, xColorItem *pdef);

Bool
PictureInitIndexedFormats (ScreenPtr pScreen);

Bool
PictureSetSubpixelOrder (ScreenPtr pScreen, int subpixel);

int
PictureGetSubpixelOrder (ScreenPtr pScreen);

PictFormatPtr
PictureCreateDefaultFormats (ScreenPtr pScreen, int *nformatp);

PictFormatPtr
PictureMatchVisual (ScreenPtr pScreen, int depth, VisualPtr pVisual);

PictFormatPtr
PictureMatchFormat (ScreenPtr pScreen, int depth, CARD32 format);
    
Bool
PictureInit (ScreenPtr pScreen, PictFormatPtr formats, int nformats);

int
PictureGetFilterId (char *filter, int len, Bool makeit);

char *
PictureGetFilterName (int id);

int
PictureAddFilter (ScreenPtr pScreen, char *filter, xFixed *params, int nparams);

Bool
PictureSetFilterAlias (ScreenPtr pScreen, char *filter, char *alias);

Bool
PictureSetDefaultFilters (ScreenPtr pScreen);
    
void
PictureResetFilters (ScreenPtr pScreen);

PictFilterPtr
PictureFindFilter (ScreenPtr pScreen, char *name, int len);

int
SetPictureFilter (PicturePtr pPicture, char *name, int len, xFixed *params, int nparams);

Bool
PictureFinishInit (void);

void
SetPictureToDefaults (PicturePtr pPicture);
    
PicturePtr
AllocatePicture (ScreenPtr  pScreen);

#if 0
Bool
miPictureInit (ScreenPtr pScreen, PictFormatPtr formats, int nformats);
#endif


PicturePtr
CreatePicture (Picture		pid,
	       DrawablePtr	pDrawable,
	       PictFormatPtr	pFormat,
	       Mask		mask,
	       XID		*list,
	       ClientPtr	client,
	       int		*error);

int
ChangePicture (PicturePtr	pPicture,
	       Mask		vmask,
	       XID		*vlist,
	       DevUnion		*ulist,
	       ClientPtr	client);

int
SetPictureClipRects (PicturePtr	pPicture,
		     int	xOrigin,
		     int	yOrigin,
		     int	nRect,
		     xRectangle	*rects);

int
SetPictureTransform (PicturePtr	    pPicture,
		     PictTransform  *transform);
		     
void
ValidatePicture(PicturePtr pPicture);

int
FreePicture (pointer	pPicture,
	     XID	pid);

int
FreePictFormat (pointer	pPictFormat,
		XID     pid);

void
CompositePicture (CARD8		op,
		  PicturePtr	pSrc,
		  PicturePtr	pMask,
		  PicturePtr	pDst,
		  INT16		xSrc,
		  INT16		ySrc,
		  INT16		xMask,
		  INT16		yMask,
		  INT16		xDst,
		  INT16		yDst,
		  CARD16	width,
		  CARD16	height);

void
CompositeGlyphs (CARD8		op,
		 PicturePtr	pSrc,
		 PicturePtr	pDst,
		 PictFormatPtr	maskFormat,
		 INT16		xSrc,
		 INT16		ySrc,
		 int		nlist,
		 GlyphListPtr	lists,
		 GlyphPtr	*glyphs);

void
CompositeRects (CARD8		op,
		PicturePtr	pDst,
		xRenderColor	*color,
		int		nRect,
		xRectangle      *rects);

void
CompositeTrapezoids (CARD8	    op,
		     PicturePtr	    pSrc,
		     PicturePtr	    pDst,
		     PictFormatPtr  maskFormat,
		     INT16	    xSrc,
		     INT16	    ySrc,
		     int	    ntrap,
		     xTrapezoid	    *traps);

void
CompositeTriangles (CARD8	    op,
		    PicturePtr	    pSrc,
		    PicturePtr	    pDst,
		    PictFormatPtr   maskFormat,
		    INT16	    xSrc,
		    INT16	    ySrc,
		    int		    ntriangles,
		    xTriangle	    *triangles);

void
CompositeTriStrip (CARD8	    op,
		   PicturePtr	    pSrc,
		   PicturePtr	    pDst,
		   PictFormatPtr    maskFormat,
		   INT16	    xSrc,
		   INT16	    ySrc,
		   int		    npoints,
		   xPointFixed	    *points);

void
CompositeTriFan (CARD8		op,
		 PicturePtr	pSrc,
		 PicturePtr	pDst,
		 PictFormatPtr	maskFormat,
		 INT16		xSrc,
		 INT16		ySrc,
		 int		npoints,
		 xPointFixed	*points);

Bool
PictureTransformPoint (PictTransformPtr transform,
		       PictVectorPtr	vector);

void RenderExtensionInit (void);

Bool
AnimCurInit (ScreenPtr pScreen);

int
AnimCursorCreate (CursorPtr *cursors, CARD32 *deltas, int ncursor, CursorPtr *ppCursor);

#ifdef PANORAMIX
void PanoramiXRenderInit (void);
void PanoramiXRenderReset (void);
#endif

#endif /* _PICTURESTR_H_ */
