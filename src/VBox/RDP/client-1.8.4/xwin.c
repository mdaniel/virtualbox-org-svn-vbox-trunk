/* -*- c-basic-offset: 8 -*-
   rdesktop: A Remote Desktop Protocol client.
   User interface services - X Window System
   Copyright (C) Matthew Chapman <matthewc.unsw.edu.au> 1999-2008
   Copyright 2007-2008 Pierre Ossman <ossman@cendio.se> for Cendio AB
   Copyright 2002-2011 Peter Astrand <astrand@cendio.se> for Cendio AB
   Copyright 2012-2013 Henrik Andersson <hean01@cendio.se> for Cendio AB

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

/*
 * Oracle GPL Disclaimer: For the avoidance of doubt, except that if any license choice
 * other than GPL or LGPL is available it will apply instead, Oracle elects to use only
 * the General Public License version 2 (GPLv2) at this time for any software where
 * a choice of GPL license versions is made available with the language indicating
 * that GPLv2 or any later version may be used, or where a choice of which version
 * of the GPL is applied is otherwise unspecified.
 */

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xproto.h>
#include <unistd.h>
#include <sys/time.h>
#include <time.h>
#include <errno.h>
#include <strings.h>
#include "rdesktop.h"
#include "xproto.h"
#ifdef HAVE_XRANDR
#include <X11/extensions/Xrandr.h>
#endif

extern int g_sizeopt;
extern int g_width;
extern int g_height;
extern int g_xpos;
extern int g_ypos;
extern int g_pos;
extern RD_BOOL g_sendmotion;
extern RD_BOOL g_fullscreen;
extern RD_BOOL g_grab_keyboard;
extern RD_BOOL g_hide_decorations;
extern RD_BOOL g_pending_resize;
extern char g_title[];
extern char g_seamless_spawn_cmd[];
/* Color depth of the RDP session.
   As of RDP 5.1, it may be 8, 15, 16 or 24. */
extern int g_server_depth;
extern int g_win_button_size;

Display *g_display;
Time g_last_gesturetime;
static int g_x_socket;
static Screen *g_screen;
Window g_wnd;

/* SeamlessRDP support */
typedef struct _seamless_group
{
	Window wnd;
	unsigned long id;
	unsigned int refcnt;
} seamless_group;
typedef struct _seamless_window
{
	Window wnd;
	unsigned long id;
	unsigned long behind;
	seamless_group *group;
	int xoffset, yoffset;
	int width, height;
	int state;		/* normal/minimized/maximized. */
	unsigned int desktop;
	struct timeval *position_timer;

	RD_BOOL outstanding_position;
	unsigned int outpos_serial;
	int outpos_xoffset, outpos_yoffset;
	int outpos_width, outpos_height;

	unsigned int icon_size;
	unsigned int icon_offset;
	char icon_buffer[32 * 32 * 4];

	struct _seamless_window *next;
} seamless_window;
static seamless_window *g_seamless_windows = NULL;
static unsigned long g_seamless_focused = 0;
static RD_BOOL g_seamless_started = False;	/* Server end is up and running */
static RD_BOOL g_seamless_active = False;	/* We are currently in seamless mode */
static RD_BOOL g_seamless_hidden = False;	/* Desktop is hidden on server */
static RD_BOOL g_seamless_broken_restack = False;	/* WM does not properly restack */
extern RD_BOOL g_seamless_rdp;
extern RD_BOOL g_seamless_persistent_mode;

extern uint32 g_embed_wnd;
RD_BOOL g_enable_compose = False;
RD_BOOL g_Unobscured;		/* used for screenblt */
static GC g_gc = NULL;
static GC g_create_bitmap_gc = NULL;
static GC g_create_glyph_gc = NULL;
static XRectangle g_clip_rectangle;
static Visual *g_visual;
/* Color depth of the X11 visual of our window (e.g. 24 for True Color R8G8B visual).
   This may be 32 for R8G8B8 visuals, and then the rest of the bits are undefined
   as far as we're concerned. */
static int g_depth;
/* Bits-per-Pixel of the pixmaps we'll be using to draw on our window.
   This may be larger than g_depth, in which case some of the bits would
   be kept solely for alignment (e.g. 32bpp pixmaps on a 24bpp visual). */
static int g_bpp;
static XIM g_IM;
static XIC g_IC;
static XModifierKeymap *g_mod_map;
/* Maps logical (xmodmap -pp) pointing device buttons (0-based) back
   to physical (1-based) indices. */
static unsigned char g_pointer_log_to_phys_map[32];
static Cursor g_current_cursor;
static RD_HCURSOR g_null_cursor = NULL;
static Atom g_protocol_atom, g_kill_atom;
extern Atom g_net_wm_state_atom;
extern Atom g_net_wm_desktop_atom;
static RD_BOOL g_focused;
static RD_BOOL g_mouse_in_wnd;
/* Indicates that:
   1) visual has 15, 16 or 24 depth and the same color channel masks
      as its RDP equivalent (implies X server is LE),
   2) host is LE
   This will trigger an optimization whose real value is questionable.
*/
static RD_BOOL g_compatible_arch;
/* Indicates whether RDP's bitmaps and our XImages have the same
   binary format. If so, we can avoid an expensive translation.
   Note that this can be true when g_compatible_arch is false,
   e.g.:
   
     RDP(LE) <-> host(BE) <-> X-Server(LE)
     
   ('host' is the machine running rdesktop; the host simply memcpy's
    so its endianess doesn't matter)
 */
static RD_BOOL g_no_translate_image = False;

/* endianness */
static RD_BOOL g_host_be;
static RD_BOOL g_xserver_be;
static int g_red_shift_r, g_blue_shift_r, g_green_shift_r;
static int g_red_shift_l, g_blue_shift_l, g_green_shift_l;

/* software backing store */
extern RD_BOOL g_ownbackstore;
static Pixmap g_backstore = 0;

/* Moving in single app mode */
static RD_BOOL g_moving_wnd;
static int g_move_x_offset = 0;
static int g_move_y_offset = 0;
static RD_BOOL g_using_full_workarea = False;

#ifdef WITH_RDPSND
extern RD_BOOL g_rdpsnd;
#endif

/* MWM decorations */
#define MWM_HINTS_DECORATIONS   (1L << 1)
#define PROP_MOTIF_WM_HINTS_ELEMENTS    5
typedef struct
{
	unsigned long flags;
	unsigned long functions;
	unsigned long decorations;
	long inputMode;
	unsigned long status;
}
PropMotifWmHints;

typedef struct
{
	uint32 red;
	uint32 green;
	uint32 blue;
}
PixelColour;

#define ON_ALL_SEAMLESS_WINDOWS(func, args) \
        do { \
                seamless_window *sw; \
                XRectangle rect; \
		if (!g_seamless_windows) break; \
                for (sw = g_seamless_windows; sw; sw = sw->next) { \
                    rect.x = g_clip_rectangle.x - sw->xoffset; \
                    rect.y = g_clip_rectangle.y - sw->yoffset; \
                    rect.width = g_clip_rectangle.width; \
                    rect.height = g_clip_rectangle.height; \
                    XSetClipRectangles(g_display, g_gc, 0, 0, &rect, 1, YXBanded); \
                    func args; \
                } \
                XSetClipRectangles(g_display, g_gc, 0, 0, &g_clip_rectangle, 1, YXBanded); \
        } while (0)

static void
seamless_XFillPolygon(Drawable d, XPoint * points, int npoints, int xoffset, int yoffset)
{
	points[0].x -= xoffset;
	points[0].y -= yoffset;
	XFillPolygon(g_display, d, g_gc, points, npoints, Complex, CoordModePrevious);
	points[0].x += xoffset;
	points[0].y += yoffset;
}

static void
seamless_XDrawLines(Drawable d, XPoint * points, int npoints, int xoffset, int yoffset)
{
	points[0].x -= xoffset;
	points[0].y -= yoffset;
	XDrawLines(g_display, d, g_gc, points, npoints, CoordModePrevious);
	points[0].x += xoffset;
	points[0].y += yoffset;
}

#define FILL_RECTANGLE(x,y,cx,cy)\
{ \
	XFillRectangle(g_display, g_wnd, g_gc, x, y, cx, cy); \
        ON_ALL_SEAMLESS_WINDOWS(XFillRectangle, (g_display, sw->wnd, g_gc, x-sw->xoffset, y-sw->yoffset, cx, cy)); \
	if (g_ownbackstore) \
		XFillRectangle(g_display, g_backstore, g_gc, x, y, cx, cy); \
}

#define FILL_RECTANGLE_BACKSTORE(x,y,cx,cy)\
{ \
	XFillRectangle(g_display, g_ownbackstore ? g_backstore : g_wnd, g_gc, x, y, cx, cy); \
}

#define FILL_POLYGON(p,np)\
{ \
	XFillPolygon(g_display, g_wnd, g_gc, p, np, Complex, CoordModePrevious); \
	if (g_ownbackstore) \
		XFillPolygon(g_display, g_backstore, g_gc, p, np, Complex, CoordModePrevious); \
	ON_ALL_SEAMLESS_WINDOWS(seamless_XFillPolygon, (sw->wnd, p, np, sw->xoffset, sw->yoffset)); \
}

#define DRAW_ELLIPSE(x,y,cx,cy,m)\
{ \
	switch (m) \
	{ \
		case 0:	/* Outline */ \
			XDrawArc(g_display, g_wnd, g_gc, x, y, cx, cy, 0, 360*64); \
                        ON_ALL_SEAMLESS_WINDOWS(XDrawArc, (g_display, sw->wnd, g_gc, x-sw->xoffset, y-sw->yoffset, cx, cy, 0, 360*64)); \
			if (g_ownbackstore) \
				XDrawArc(g_display, g_backstore, g_gc, x, y, cx, cy, 0, 360*64); \
			break; \
		case 1: /* Filled */ \
			XFillArc(g_display, g_wnd, g_gc, x, y, cx, cy, 0, 360*64); \
			ON_ALL_SEAMLESS_WINDOWS(XFillArc, (g_display, sw->wnd, g_gc, x-sw->xoffset, y-sw->yoffset, cx, cy, 0, 360*64)); \
			if (g_ownbackstore) \
				XFillArc(g_display, g_backstore, g_gc, x, y, cx, cy, 0, 360*64); \
			break; \
	} \
}

/* colour maps */
extern RD_BOOL g_owncolmap;
static Colormap g_xcolmap;
static uint32 *g_colmap = NULL;

#define TRANSLATE(col)		( g_server_depth != 8 ? translate_colour(col) : g_owncolmap ? col : g_colmap[col] )
#define SET_FOREGROUND(col)	XSetForeground(g_display, g_gc, TRANSLATE(col));
#define SET_BACKGROUND(col)	XSetBackground(g_display, g_gc, TRANSLATE(col));

static int rop2_map[] = {
	GXclear,		/* 0 */
	GXnor,			/* DPon */
	GXandInverted,		/* DPna */
	GXcopyInverted,		/* Pn */
	GXandReverse,		/* PDna */
	GXinvert,		/* Dn */
	GXxor,			/* DPx */
	GXnand,			/* DPan */
	GXand,			/* DPa */
	GXequiv,		/* DPxn */
	GXnoop,			/* D */
	GXorInverted,		/* DPno */
	GXcopy,			/* P */
	GXorReverse,		/* PDno */
	GXor,			/* DPo */
	GXset			/* 1 */
};

#define SET_FUNCTION(rop2)	{ if (rop2 != ROP2_COPY) XSetFunction(g_display, g_gc, rop2_map[rop2]); }
#define RESET_FUNCTION(rop2)	{ if (rop2 != ROP2_COPY) XSetFunction(g_display, g_gc, GXcopy); }

static seamless_window *
sw_get_window_by_id(unsigned long id)
{
	seamless_window *sw;
	for (sw = g_seamless_windows; sw; sw = sw->next)
	{
		if (sw->id == id)
			return sw;
	}
	return NULL;
}


static seamless_window *
sw_get_window_by_wnd(Window wnd)
{
	seamless_window *sw;
	for (sw = g_seamless_windows; sw; sw = sw->next)
	{
		if (sw->wnd == wnd)
			return sw;
	}
	return NULL;
}


static void
sw_remove_window(seamless_window * win)
{
	seamless_window *sw, **prevnext = &g_seamless_windows;
	for (sw = g_seamless_windows; sw; sw = sw->next)
	{
		if (sw == win)
		{
			*prevnext = sw->next;
			sw->group->refcnt--;
			if (sw->group->refcnt == 0)
			{
				XDestroyWindow(g_display, sw->group->wnd);
				xfree(sw->group);
			}
			xfree(sw->position_timer);
			xfree(sw);
			return;
		}
		prevnext = &sw->next;
	}
	return;
}


/* Move all windows except wnd to new desktop */
static void
sw_all_to_desktop(Window wnd, unsigned int desktop)
{
	seamless_window *sw;
	for (sw = g_seamless_windows; sw; sw = sw->next)
	{
		if (sw->wnd == wnd)
			continue;
		if (sw->desktop != desktop)
		{
			ewmh_move_to_desktop(sw->wnd, desktop);
			sw->desktop = desktop;
		}
	}
}


/* Send our position */
static void
sw_update_position(seamless_window * sw)
{
	XWindowAttributes wa;
	int x, y;
	Window child_return;
	unsigned int serial;

	XGetWindowAttributes(g_display, sw->wnd, &wa);
	XTranslateCoordinates(g_display, sw->wnd, wa.root,
			      -wa.border_width, -wa.border_width, &x, &y, &child_return);

	serial = seamless_send_position(sw->id, x, y, wa.width, wa.height, 0);

	sw->outstanding_position = True;
	sw->outpos_serial = serial;

	sw->outpos_xoffset = x;
	sw->outpos_yoffset = y;
	sw->outpos_width = wa.width;
	sw->outpos_height = wa.height;
}


/* Check if it's time to send our position */
static void
sw_check_timers()
{
	seamless_window *sw;
	struct timeval now;

	gettimeofday(&now, NULL);
	for (sw = g_seamless_windows; sw; sw = sw->next)
	{
		if (timerisset(sw->position_timer) && timercmp(sw->position_timer, &now, <))
		{
			timerclear(sw->position_timer);
			sw_update_position(sw);
		}
	}
}


static void
sw_restack_window(seamless_window * sw, unsigned long behind)
{
	seamless_window *sw_above;

	/* Remove window from stack */
	for (sw_above = g_seamless_windows; sw_above; sw_above = sw_above->next)
	{
		if (sw_above->behind == sw->id)
			break;
	}

	if (sw_above)
		sw_above->behind = sw->behind;

	/* And then add it at the new position */
	for (sw_above = g_seamless_windows; sw_above; sw_above = sw_above->next)
	{
		if (sw_above->behind == behind)
			break;
	}

	if (sw_above)
		sw_above->behind = sw->id;

	sw->behind = behind;
}


static void
sw_handle_restack(seamless_window * sw)
{
	Status status;
	Window root, parent, *children;
	unsigned int nchildren, i;
	seamless_window *sw_below;

	status = XQueryTree(g_display, RootWindowOfScreen(g_screen),
			    &root, &parent, &children, &nchildren);
	if (!status || !nchildren)
		return;

	sw_below = NULL;

	i = 0;
	while (children[i] != sw->wnd)
	{
		i++;
		if (i >= nchildren)
			goto end;
	}

	for (i++; i < nchildren; i++)
	{
		sw_below = sw_get_window_by_wnd(children[i]);
		if (sw_below)
			break;
	}

	if (!sw_below && !sw->behind)
		goto end;
	if (sw_below && (sw_below->id == sw->behind))
		goto end;

	if (sw_below)
	{
		seamless_send_zchange(sw->id, sw_below->id, 0);
		sw_restack_window(sw, sw_below->id);
	}
	else
	{
		seamless_send_zchange(sw->id, 0, 0);
		sw_restack_window(sw, 0);
	}

      end:
	XFree(children);
}


static seamless_group *
sw_find_group(unsigned long id, RD_BOOL dont_create)
{
	seamless_window *sw;
	seamless_group *sg;
	XSetWindowAttributes attribs;

	for (sw = g_seamless_windows; sw; sw = sw->next)
	{
		if (sw->group->id == id)
			return sw->group;
	}

	if (dont_create)
		return NULL;

	sg = xmalloc(sizeof(seamless_group));

	sg->wnd =
		XCreateWindow(g_display, RootWindowOfScreen(g_screen), -1, -1, 1, 1, 0,
			      CopyFromParent, CopyFromParent, CopyFromParent, 0, &attribs);

	sg->id = id;
	sg->refcnt = 0;

	return sg;
}


static void
mwm_hide_decorations(Window wnd)
{
	PropMotifWmHints motif_hints;
	Atom hintsatom;

	/* setup the property */
	motif_hints.flags = MWM_HINTS_DECORATIONS;
	motif_hints.decorations = 0;

	/* get the atom for the property */
	hintsatom = XInternAtom(g_display, "_MOTIF_WM_HINTS", False);
	if (!hintsatom)
	{
		warning("Failed to get atom _MOTIF_WM_HINTS: probably your window manager does not support MWM hints\n");
		return;
	}

	XChangeProperty(g_display, wnd, hintsatom, hintsatom, 32, PropModeReplace,
			(unsigned char *) &motif_hints, PROP_MOTIF_WM_HINTS_ELEMENTS);

}

typedef struct _sw_configurenotify_context
{
	Window window;
	unsigned long serial;
} sw_configurenotify_context;

/* Predicate procedure for sw_wait_configurenotify */
static Bool
sw_configurenotify_p(Display * display, XEvent * xevent, XPointer arg)
{
	sw_configurenotify_context *context = (sw_configurenotify_context *) arg;
	if (xevent->xany.type == ConfigureNotify
	    && xevent->xconfigure.window == context->window
	    && xevent->xany.serial >= context->serial)
		return True;

	return False;
}

/* Wait for a ConfigureNotify, with a equal or larger serial, on the
   specified window. The event will be removed from the queue. We
   could use XMaskEvent(StructureNotifyMask), but we would then risk
   throwing away crucial events like DestroyNotify. 

   After a ConfigureWindow, according to ICCCM section 4.1.5, we
   should recieve a ConfigureNotify, either a real or synthetic
   one. This indicates that the configure has been "completed".
   However, some WMs such as several versions of Metacity fails to
   send synthetic events. See bug
   http://bugzilla.gnome.org/show_bug.cgi?id=322840. We need to use a
   timeout to avoid a hang. Tk uses the same approach. */
static void
sw_wait_configurenotify(Window wnd, unsigned long serial)
{
	XEvent xevent;
	sw_configurenotify_context context;
	struct timeval now;
	struct timeval future;
	RD_BOOL got = False;

	context.window = wnd;
	context.serial = serial;

	gettimeofday(&future, NULL);
	future.tv_usec += 500000;

	do
	{
		if (XCheckIfEvent(g_display, &xevent, sw_configurenotify_p, (XPointer) & context))
		{
			got = True;
			break;
		}
		usleep(100000);
		gettimeofday(&now, NULL);
	}
	while (timercmp(&now, &future, <));

	if (!got)
	{
		warning("Broken Window Manager: Timeout while waiting for ConfigureNotify\n");
	}
}

/* Get the toplevel window, in case of reparenting */
static Window
sw_get_toplevel(Window wnd)
{
	Window root, parent;
	Window *child_list;
	unsigned int num_children;

	while (1)
	{
		XQueryTree(g_display, wnd, &root, &parent, &child_list, &num_children);
		if (root == parent)
		{
			break;
		}
		else if (!parent)
		{
			warning("Internal error: sw_get_toplevel called with root window\n");
		}

		wnd = parent;
	}

	return wnd;
}


/* Check if wnd is already behind a window wrt stacking order */
static RD_BOOL
sw_window_is_behind(Window wnd, Window behind)
{
	Window dummy1, dummy2;
	Window *child_list;
	unsigned int num_children;
	unsigned int i;
	RD_BOOL found_behind = False;
	RD_BOOL found_wnd = False;

	wnd = sw_get_toplevel(wnd);
	behind = sw_get_toplevel(behind);

	XQueryTree(g_display, RootWindowOfScreen(g_screen), &dummy1, &dummy2, &child_list,
		   &num_children);

	for (i = num_children; i > 0; i--)
	{
		if (child_list[i-1] == behind)
		{
			found_behind = True;
		}
		else if (child_list[i-1] == wnd)
		{
			found_wnd = True;
			break;
		}
	}

	if (child_list)
		XFree(child_list);

	if (!found_wnd)
	{
		warning("sw_window_is_behind: Unable to find window 0x%lx\n", wnd);

		if (!found_behind)
		{
			warning("sw_window_is_behind: Unable to find behind window 0x%lx\n",
				behind);
		}
	}

	return found_behind;
}


/* Test if the window manager correctly handles window restacking. In
   particular, we are testing if it's possible to place a window
   between two other windows. Many WMs such as Metacity can only stack
   windows on the top or bottom. The window creation should mostly
   match ui_seamless_create_window. */
static void
seamless_restack_test()
{
	/* The goal is to have the middle window between top and
	   bottom.  The middle window is initially at the top,
	   though. */
	Window wnds[3];		/* top, middle and bottom */
	int i;
	XEvent xevent;
	XWindowChanges values;
	unsigned long restack_serial;

	for (i = 0; i < 3; i++)
	{
		char name[64];
		wnds[i] =
			XCreateSimpleWindow(g_display, RootWindowOfScreen(g_screen), 0, 0, 20, 20,
					    0, 0, 0);
		snprintf(name, sizeof(name), "SeamlessRDP restack test - window %d", i);
		XStoreName(g_display, wnds[i], name);
		ewmh_set_wm_name(wnds[i], name);

		/* Hide decorations. Often this means that no
		   reparenting will be done, which makes the restack
		   easier. Besides, we want to mimic our other
		   seamless windows as much as possible. We must still
		   handle the case with reparenting, though. */
		mwm_hide_decorations(wnds[i]);

		/* Prevent windows from appearing in task bar */
		XSetTransientForHint(g_display, wnds[i], RootWindowOfScreen(g_screen));
		ewmh_set_window_popup(wnds[i]);

		/* We need to catch MapNotify/ConfigureNotify */
		XSelectInput(g_display, wnds[i], StructureNotifyMask);
	}

	/* Map Windows. Currently, we assume that XMapRaised places
	   the window on the top of the stack. Should be fairly safe;
	   the window is configured before it's mapped. */
	XMapRaised(g_display, wnds[2]);	/* bottom */
	do
	{
		XWindowEvent(g_display, wnds[2], StructureNotifyMask, &xevent);
	}
	while (xevent.type != MapNotify);
	XMapRaised(g_display, wnds[0]);	/* top */
	do
	{
		XWindowEvent(g_display, wnds[0], StructureNotifyMask, &xevent);
	}
	while (xevent.type != MapNotify);
	XMapRaised(g_display, wnds[1]);	/* middle */
	do
	{
		XWindowEvent(g_display, wnds[1], StructureNotifyMask, &xevent);
	}
	while (xevent.type != MapNotify);

	/* The stacking order should now be 1 - 0 - 2 */
	if (!sw_window_is_behind(wnds[0], wnds[1]) || !sw_window_is_behind(wnds[2], wnds[1]))
	{
		/* Ok, technically a WM is allowed to stack windows arbitrarily, but... */
		warning("Broken Window Manager: Unable to test window restacking\n");
		g_seamless_broken_restack = True;
		for (i = 0; i < 3; i++)
			XDestroyWindow(g_display, wnds[i]);
		return;
	}

	/* Restack, using XReconfigureWMWindow, which should correctly
	   handle reparented windows as well as nonreparenting WMs. */
	values.stack_mode = Below;
	values.sibling = wnds[0];
	restack_serial = XNextRequest(g_display);
	XReconfigureWMWindow(g_display, wnds[1], DefaultScreen(g_display), CWStackMode | CWSibling,
			     &values);
	sw_wait_configurenotify(wnds[1], restack_serial);

	/* Now verify that middle is behind top but not behind
	   bottom */
	if (!sw_window_is_behind(wnds[1], wnds[0]))
	{
		warning("Broken Window Manager: doesn't handle restack (restack request was ignored)\n");
		g_seamless_broken_restack = True;
	}
	else if (sw_window_is_behind(wnds[1], wnds[2]))
	{
		warning("Broken Window Manager: doesn't handle restack (window was moved to bottom)\n");
		g_seamless_broken_restack = True;
	}

	/* Destroy windows */
	for (i = 0; i < 3; i++)
	{
		XDestroyWindow(g_display, wnds[i]);
		do
		{
			XWindowEvent(g_display, wnds[i], StructureNotifyMask, &xevent);
		}
		while (xevent.type != DestroyNotify);
	}
}

#define SPLITCOLOUR15(colour, rv) \
{ \
	rv.red = ((colour >> 7) & 0xf8) | ((colour >> 12) & 0x7); \
	rv.green = ((colour >> 2) & 0xf8) | ((colour >> 8) & 0x7); \
	rv.blue = ((colour << 3) & 0xf8) | ((colour >> 2) & 0x7); \
}

#define SPLITCOLOUR16(colour, rv) \
{ \
	rv.red = ((colour >> 8) & 0xf8) | ((colour >> 13) & 0x7); \
	rv.green = ((colour >> 3) & 0xfc) | ((colour >> 9) & 0x3); \
	rv.blue = ((colour << 3) & 0xf8) | ((colour >> 2) & 0x7); \
} \

#define SPLITCOLOUR24(colour, rv) \
{ \
	rv.blue = (colour & 0xff0000) >> 16; \
	rv.green = (colour & 0x00ff00) >> 8; \
	rv.red = (colour & 0x0000ff); \
}

#define MAKECOLOUR(pc) \
	((pc.red >> g_red_shift_r) << g_red_shift_l) \
		| ((pc.green >> g_green_shift_r) << g_green_shift_l) \
		| ((pc.blue >> g_blue_shift_r) << g_blue_shift_l) \

#define BSWAP16(x) { x = (((x & 0xff) << 8) | (x >> 8)); }
#define BSWAP24(x) { x = (((x & 0xff) << 16) | (x >> 16) | (x & 0xff00)); }
#define BSWAP32(x) { x = (((x & 0xff00ff) << 8) | ((x >> 8) & 0xff00ff)); \
			x = (x << 16) | (x >> 16); }

/* The following macros output the same octet sequences
   on both BE and LE hosts: */

#define BOUT16(o, x) { *(o++) = x >> 8; *(o++) = x; }
#define BOUT24(o, x) { *(o++) = x >> 16; *(o++) = x >> 8; *(o++) = x; }
#define BOUT32(o, x) { *(o++) = x >> 24; *(o++) = x >> 16; *(o++) = x >> 8; *(o++) = x; }
#define LOUT16(o, x) { *(o++) = x; *(o++) = x >> 8; }
#define LOUT24(o, x) { *(o++) = x; *(o++) = x >> 8; *(o++) = x >> 16; }
#define LOUT32(o, x) { *(o++) = x; *(o++) = x >> 8; *(o++) = x >> 16; *(o++) = x >> 24; }

static uint32
translate_colour(uint32 colour)
{
	PixelColour pc;
	switch (g_server_depth)
	{
		case 15:
			SPLITCOLOUR15(colour, pc);
			break;
		case 16:
			SPLITCOLOUR16(colour, pc);
			break;
		case 24:
		case 32:
			SPLITCOLOUR24(colour, pc);
			break;
		default:
			/* Avoid warning */
			pc.red = 0;
			pc.green = 0;
			pc.blue = 0;
			break;
	}
	return MAKECOLOUR(pc);
}

/* indent is confused by UNROLL8 */
/* *INDENT-OFF* */

/* repeat and unroll, similar to bitmap.c */
/* potentialy any of the following translate */
/* functions can use repeat but just doing */
/* the most common ones */

#define UNROLL8(stm) { stm stm stm stm stm stm stm stm }
/* 2 byte output repeat */
#define REPEAT2(stm) \
{ \
	while (out <= end - 8 * 2) \
		UNROLL8(stm) \
	while (out < end) \
		{ stm } \
}
/* 3 byte output repeat */
#define REPEAT3(stm) \
{ \
	while (out <= end - 8 * 3) \
		UNROLL8(stm) \
	while (out < end) \
		{ stm } \
}
/* 4 byte output repeat */
#define REPEAT4(stm) \
{ \
	while (out <= end - 8 * 4) \
		UNROLL8(stm) \
	while (out < end) \
		{ stm } \
}
/* *INDENT-ON* */

static void
translate8to8(const uint8 * data, uint8 * out, uint8 * end)
{
	while (out < end)
		*(out++) = (uint8) g_colmap[*(data++)];
}

static void
translate8to16(const uint8 * data, uint8 * out, uint8 * end)
{
	uint16 value;

	if (g_compatible_arch)
	{
		/* *INDENT-OFF* */
		REPEAT2
		(
			*((uint16 *) out) = g_colmap[*(data++)];
			out += 2;
		)
		/* *INDENT-ON* */
	}
	else if (g_xserver_be)
	{
		while (out < end)
		{
			value = (uint16) g_colmap[*(data++)];
			BOUT16(out, value);
		}
	}
	else
	{
		while (out < end)
		{
			value = (uint16) g_colmap[*(data++)];
			LOUT16(out, value);
		}
	}
}

/* little endian - conversion happens when colourmap is built */
static void
translate8to24(const uint8 * data, uint8 * out, uint8 * end)
{
	uint32 value;

	if (g_compatible_arch)
	{
		while (out < end)
		{
			value = g_colmap[*(data++)];
			BOUT24(out, value);
		}
	}
	else
	{
		while (out < end)
		{
			value = g_colmap[*(data++)];
			LOUT24(out, value);
		}
	}
}

static void
translate8to32(const uint8 * data, uint8 * out, uint8 * end)
{
	uint32 value;

	if (g_compatible_arch)
	{
		/* *INDENT-OFF* */
		REPEAT4
		(
			*((uint32 *) out) = g_colmap[*(data++)];
			out += 4;
		)
		/* *INDENT-ON* */
	}
	else if (g_xserver_be)
	{
		while (out < end)
		{
			value = g_colmap[*(data++)];
			BOUT32(out, value);
		}
	}
	else
	{
		while (out < end)
		{
			value = g_colmap[*(data++)];
			LOUT32(out, value);
		}
	}
}

static void
translate15to16(const uint16 * data, uint8 * out, uint8 * end)
{
	uint16 pixel;
	uint16 value;
	PixelColour pc;

	if (g_xserver_be)
	{
		while (out < end)
		{
			pixel = *(data++);
			if (g_host_be)
			{
				BSWAP16(pixel);
			}
			SPLITCOLOUR15(pixel, pc);
			value = MAKECOLOUR(pc);
			BOUT16(out, value);
		}
	}
	else
	{
		while (out < end)
		{
			pixel = *(data++);
			if (g_host_be)
			{
				BSWAP16(pixel);
			}
			SPLITCOLOUR15(pixel, pc);
			value = MAKECOLOUR(pc);
			LOUT16(out, value);
		}
	}
}

static void
translate15to24(const uint16 * data, uint8 * out, uint8 * end)
{
	uint32 value;
	uint16 pixel;
	PixelColour pc;

	if (g_compatible_arch)
	{
		/* *INDENT-OFF* */
		REPEAT3
		(
			pixel = *(data++);
			SPLITCOLOUR15(pixel, pc);
			*(out++) = pc.blue;
			*(out++) = pc.green;
			*(out++) = pc.red;
		)
		/* *INDENT-ON* */
	}
	else if (g_xserver_be)
	{
		while (out < end)
		{
			pixel = *(data++);
			if (g_host_be)
			{
				BSWAP16(pixel);
			}
			SPLITCOLOUR15(pixel, pc);
			value = MAKECOLOUR(pc);
			BOUT24(out, value);
		}
	}
	else
	{
		while (out < end)
		{
			pixel = *(data++);
			if (g_host_be)
			{
				BSWAP16(pixel);
			}
			SPLITCOLOUR15(pixel, pc);
			value = MAKECOLOUR(pc);
			LOUT24(out, value);
		}
	}
}

static void
translate15to32(const uint16 * data, uint8 * out, uint8 * end)
{
	uint16 pixel;
	uint32 value;
	PixelColour pc;

	if (g_compatible_arch)
	{
		/* *INDENT-OFF* */
		REPEAT4
		(
			pixel = *(data++);
			SPLITCOLOUR15(pixel, pc);
			*(out++) = pc.blue;
			*(out++) = pc.green;
			*(out++) = pc.red;
			*(out++) = 0;
		)
		/* *INDENT-ON* */
	}
	else if (g_xserver_be)
	{
		while (out < end)
		{
			pixel = *(data++);
			if (g_host_be)
			{
				BSWAP16(pixel);
			}
			SPLITCOLOUR15(pixel, pc);
			value = MAKECOLOUR(pc);
			BOUT32(out, value);
		}
	}
	else
	{
		while (out < end)
		{
			pixel = *(data++);
			if (g_host_be)
			{
				BSWAP16(pixel);
			}
			SPLITCOLOUR15(pixel, pc);
			value = MAKECOLOUR(pc);
			LOUT32(out, value);
		}
	}
}

static void
translate16to16(const uint16 * data, uint8 * out, uint8 * end)
{
	uint16 pixel;
	uint16 value;
	PixelColour pc;

	if (g_xserver_be)
	{
		if (g_host_be)
		{
			while (out < end)
			{
				pixel = *(data++);
				BSWAP16(pixel);
				SPLITCOLOUR16(pixel, pc);
				value = MAKECOLOUR(pc);
				BOUT16(out, value);
			}
		}
		else
		{
			while (out < end)
			{
				pixel = *(data++);
				SPLITCOLOUR16(pixel, pc);
				value = MAKECOLOUR(pc);
				BOUT16(out, value);
			}
		}
	}
	else
	{
		if (g_host_be)
		{
			while (out < end)
			{
				pixel = *(data++);
				BSWAP16(pixel);
				SPLITCOLOUR16(pixel, pc);
				value = MAKECOLOUR(pc);
				LOUT16(out, value);
			}
		}
		else
		{
			while (out < end)
			{
				pixel = *(data++);
				SPLITCOLOUR16(pixel, pc);
				value = MAKECOLOUR(pc);
				LOUT16(out, value);
			}
		}
	}
}

static void
translate16to24(const uint16 * data, uint8 * out, uint8 * end)
{
	uint32 value;
	uint16 pixel;
	PixelColour pc;

	if (g_compatible_arch)
	{
		/* *INDENT-OFF* */
		REPEAT3
		(
			pixel = *(data++);
			SPLITCOLOUR16(pixel, pc);
			*(out++) = pc.blue;
			*(out++) = pc.green;
			*(out++) = pc.red;
		)
		/* *INDENT-ON* */
	}
	else if (g_xserver_be)
	{
		if (g_host_be)
		{
			while (out < end)
			{
				pixel = *(data++);
				BSWAP16(pixel);
				SPLITCOLOUR16(pixel, pc);
				value = MAKECOLOUR(pc);
				BOUT24(out, value);
			}
		}
		else
		{
			while (out < end)
			{
				pixel = *(data++);
				SPLITCOLOUR16(pixel, pc);
				value = MAKECOLOUR(pc);
				BOUT24(out, value);
			}
		}
	}
	else
	{
		if (g_host_be)
		{
			while (out < end)
			{
				pixel = *(data++);
				BSWAP16(pixel);
				SPLITCOLOUR16(pixel, pc);
				value = MAKECOLOUR(pc);
				LOUT24(out, value);
			}
		}
		else
		{
			while (out < end)
			{
				pixel = *(data++);
				SPLITCOLOUR16(pixel, pc);
				value = MAKECOLOUR(pc);
				LOUT24(out, value);
			}
		}
	}
}

static void
translate16to32(const uint16 * data, uint8 * out, uint8 * end)
{
	uint16 pixel;
	uint32 value;
	PixelColour pc;

	if (g_compatible_arch)
	{
		/* *INDENT-OFF* */
		REPEAT4
		(
			pixel = *(data++);
			SPLITCOLOUR16(pixel, pc);
			*(out++) = pc.blue;
			*(out++) = pc.green;
			*(out++) = pc.red;
			*(out++) = 0;
		)
		/* *INDENT-ON* */
	}
	else if (g_xserver_be)
	{
		if (g_host_be)
		{
			while (out < end)
			{
				pixel = *(data++);
				BSWAP16(pixel);
				SPLITCOLOUR16(pixel, pc);
				value = MAKECOLOUR(pc);
				BOUT32(out, value);
			}
		}
		else
		{
			while (out < end)
			{
				pixel = *(data++);
				SPLITCOLOUR16(pixel, pc);
				value = MAKECOLOUR(pc);
				BOUT32(out, value);
			}
		}
	}
	else
	{
		if (g_host_be)
		{
			while (out < end)
			{
				pixel = *(data++);
				BSWAP16(pixel);
				SPLITCOLOUR16(pixel, pc);
				value = MAKECOLOUR(pc);
				LOUT32(out, value);
			}
		}
		else
		{
			while (out < end)
			{
				pixel = *(data++);
				SPLITCOLOUR16(pixel, pc);
				value = MAKECOLOUR(pc);
				LOUT32(out, value);
			}
		}
	}
}

static void
translate24to16(const uint8 * data, uint8 * out, uint8 * end)
{
	uint32 pixel = 0;
	uint16 value;
	PixelColour pc;

	while (out < end)
	{
		pixel = *(data++) << 16;
		pixel |= *(data++) << 8;
		pixel |= *(data++);
		SPLITCOLOUR24(pixel, pc);
		value = MAKECOLOUR(pc);
		if (g_xserver_be)
		{
			BOUT16(out, value);
		}
		else
		{
			LOUT16(out, value);
		}
	}
}

static void
translate24to24(const uint8 * data, uint8 * out, uint8 * end)
{
	uint32 pixel;
	uint32 value;
	PixelColour pc;

	if (g_xserver_be)
	{
		while (out < end)
		{
			pixel = *(data++) << 16;
			pixel |= *(data++) << 8;
			pixel |= *(data++);
			SPLITCOLOUR24(pixel, pc);
			value = MAKECOLOUR(pc);
			BOUT24(out, value);
		}
	}
	else
	{
		while (out < end)
		{
			pixel = *(data++) << 16;
			pixel |= *(data++) << 8;
			pixel |= *(data++);
			SPLITCOLOUR24(pixel, pc);
			value = MAKECOLOUR(pc);
			LOUT24(out, value);
		}
	}
}

static void
translate24to32(const uint8 * data, uint8 * out, uint8 * end)
{
	uint32 pixel;
	uint32 value;
	PixelColour pc;

	if (g_compatible_arch)
	{
		/* *INDENT-OFF* */
#ifdef NEED_ALIGN
		REPEAT4
		(
			*(out++) = *(data++);
			*(out++) = *(data++);
			*(out++) = *(data++);
			*(out++) = 0;
		)
#else
		REPEAT4
		(
		 /* Only read 3 bytes. Reading 4 bytes means reading beyond buffer. */
		 *((uint32 *) out) = *((uint16 *) data) + (*((uint8 *) data + 2) << 16);
		 out += 4;
		 data += 3;
		)
#endif
		/* *INDENT-ON* */
	}
	else if (g_xserver_be)
	{
		while (out < end)
		{
			pixel = *(data++) << 16;
			pixel |= *(data++) << 8;
			pixel |= *(data++);
			SPLITCOLOUR24(pixel, pc);
			value = MAKECOLOUR(pc);
			BOUT32(out, value);
		}
	}
	else
	{
		while (out < end)
		{
			pixel = *(data++) << 16;
			pixel |= *(data++) << 8;
			pixel |= *(data++);
			SPLITCOLOUR24(pixel, pc);
			value = MAKECOLOUR(pc);
			LOUT32(out, value);
		}
	}
}

static uint8 *
translate_image(int width, int height, uint8 * data)
{
	int size;
	uint8 *out;
	uint8 *end;

	/*
	   If RDP depth and X Visual depths match,
	   and arch(endian) matches, no need to translate:
	   just return data.
	   Note: select_visual should've already ensured g_no_translate
	   is only set for compatible depths, but the RDP depth might've
	   changed during connection negotiations.
	 */

	/* todo */
	if (g_server_depth == 32 && g_depth == 24)
	{
		return data;
	}

	if (g_no_translate_image)
	{
		if ((g_depth == 15 && g_server_depth == 15) ||
		    (g_depth == 16 && g_server_depth == 16) ||
		    (g_depth == 24 && g_server_depth == 24))
			return data;
	}

	size = width * height * (g_bpp / 8);
	out = (uint8 *) xmalloc(size);
	end = out + size;

	switch (g_server_depth)
	{
		case 24:
			switch (g_bpp)
			{
				case 32:
					translate24to32(data, out, end);
					break;
				case 24:
					translate24to24(data, out, end);
					break;
				case 16:
					translate24to16(data, out, end);
					break;
			}
			break;
		case 16:
			switch (g_bpp)
			{
				case 32:
					translate16to32((uint16 *) data, out, end);
					break;
				case 24:
					translate16to24((uint16 *) data, out, end);
					break;
				case 16:
					translate16to16((uint16 *) data, out, end);
					break;
			}
			break;
		case 15:
			switch (g_bpp)
			{
				case 32:
					translate15to32((uint16 *) data, out, end);
					break;
				case 24:
					translate15to24((uint16 *) data, out, end);
					break;
				case 16:
					translate15to16((uint16 *) data, out, end);
					break;
			}
			break;
		case 8:
			switch (g_bpp)
			{
				case 8:
					translate8to8(data, out, end);
					break;
				case 16:
					translate8to16(data, out, end);
					break;
				case 24:
					translate8to24(data, out, end);
					break;
				case 32:
					translate8to32(data, out, end);
					break;
			}
			break;
	}
	return out;
}

static void
xwin_refresh_pointer_map(void)
{
	unsigned char phys_to_log_map[sizeof(g_pointer_log_to_phys_map)];
	int i, pointer_buttons;

	pointer_buttons = XGetPointerMapping(g_display, phys_to_log_map, sizeof(phys_to_log_map));
	if (pointer_buttons > sizeof(phys_to_log_map))
		pointer_buttons = sizeof(phys_to_log_map);

	/* if multiple physical buttons map to the same logical button, then
	 * use the lower numbered physical one */
	for (i = pointer_buttons - 1; i >= 0; i--)
	{
		/* a user could specify arbitrary values for the logical button
		 * number, ignore any that are abnormally large */
		if (phys_to_log_map[i] > sizeof(g_pointer_log_to_phys_map))
			continue;
		g_pointer_log_to_phys_map[phys_to_log_map[i] - 1] = i + 1;
	}
}

RD_BOOL
get_key_state(unsigned int state, uint32 keysym)
{
	int modifierpos, key, keysymMask = 0;
	int offset;

	KeyCode keycode = XKeysymToKeycode(g_display, keysym);

	if (keycode == NoSymbol)
		return False;

	for (modifierpos = 0; modifierpos < 8; modifierpos++)
	{
		offset = g_mod_map->max_keypermod * modifierpos;

		for (key = 0; key < g_mod_map->max_keypermod; key++)
		{
			if (g_mod_map->modifiermap[offset + key] == keycode)
				keysymMask |= 1 << modifierpos;
		}
	}

	return (state & keysymMask) ? True : False;
}

static void
calculate_shifts(uint32 mask, int *shift_r, int *shift_l)
{
	*shift_l = ffs(mask) - 1;
	mask >>= *shift_l;
	*shift_r = 8 - ffs(mask & ~(mask >> 1));
}

/* Given a mask of a colour channel (e.g. XVisualInfo.red_mask),
   calculates the bits-per-pixel of this channel (a.k.a. colour weight).
 */
static unsigned
calculate_mask_weight(uint32 mask)
{
	unsigned weight = 0;
	do
	{
		weight += (mask & 1);
	}
	while (mask >>= 1);
	return weight;
}

static RD_BOOL
select_visual(int screen_num)
{
	XPixmapFormatValues *pfm;
	int pixmap_formats_count, visuals_count;
	XVisualInfo *vmatches = NULL;
	XVisualInfo template;
	int i;
	unsigned red_weight, blue_weight, green_weight;

	red_weight = blue_weight = green_weight = 0;

	if (g_server_depth == -1)
	{
		g_server_depth = DisplayPlanes(g_display, DefaultScreen(g_display));
	}

	pfm = XListPixmapFormats(g_display, &pixmap_formats_count);
	if (pfm == NULL)
	{
		error("Unable to get list of pixmap formats from display.\n");
		XCloseDisplay(g_display);
		return False;
	}

	/* Search for best TrueColor visual */
	template.class = TrueColor;
	template.screen = screen_num;
	vmatches =
		XGetVisualInfo(g_display, VisualClassMask | VisualScreenMask, &template,
			       &visuals_count);
	g_visual = NULL;
	g_no_translate_image = False;
	g_compatible_arch = False;
	if (vmatches != NULL)
	{
		for (i = 0; i < visuals_count; ++i)
		{
			XVisualInfo *visual_info = &vmatches[i];
			RD_BOOL can_translate_to_bpp = False;
			int j;

			/* Try to find a no-translation visual that'll
			   allow us to use RDP bitmaps directly as ZPixmaps. */
			if (!g_xserver_be && (((visual_info->depth == 15) &&
					       /* R5G5B5 */
					       (visual_info->red_mask == 0x7c00) &&
					       (visual_info->green_mask == 0x3e0) &&
					       (visual_info->blue_mask == 0x1f)) ||
					      ((visual_info->depth == 16) &&
					       /* R5G6B5 */
					       (visual_info->red_mask == 0xf800) &&
					       (visual_info->green_mask == 0x7e0) &&
					       (visual_info->blue_mask == 0x1f)) ||
					      ((visual_info->depth == 24) &&
					       /* R8G8B8 */
					       (visual_info->red_mask == 0xff0000) &&
					       (visual_info->green_mask == 0xff00) &&
					       (visual_info->blue_mask == 0xff))))
			{
				g_visual = visual_info->visual;
				g_depth = visual_info->depth;
				g_compatible_arch = !g_host_be;
				g_no_translate_image = (visual_info->depth == g_server_depth);
				if (g_no_translate_image)
					/* We found the best visual */
					break;
			}
			else
			{
				g_compatible_arch = False;
			}

			if (visual_info->depth > 24)
			{
				/* Avoid 32-bit visuals and likes like the plague.
				   They're either untested or proven to work bad
				   (e.g. nvidia's Composite 32-bit visual).
				   Most implementation offer a 24-bit visual anyway. */
				continue;
			}

			/* Only care for visuals, for whose BPPs (not depths!)
			   we have a translateXtoY function. */
			for (j = 0; j < pixmap_formats_count; ++j)
			{
				if (pfm[j].depth == visual_info->depth)
				{
					if ((pfm[j].bits_per_pixel == 16) ||
					    (pfm[j].bits_per_pixel == 24) ||
					    (pfm[j].bits_per_pixel == 32))
					{
						can_translate_to_bpp = True;
					}
					break;
				}
			}

			/* Prefer formats which have the most colour depth.
			   We're being truly aristocratic here, minding each
			   weight on its own. */
			if (can_translate_to_bpp)
			{
				unsigned vis_red_weight =
					calculate_mask_weight(visual_info->red_mask);
				unsigned vis_green_weight =
					calculate_mask_weight(visual_info->green_mask);
				unsigned vis_blue_weight =
					calculate_mask_weight(visual_info->blue_mask);
				if ((vis_red_weight >= red_weight)
				    && (vis_green_weight >= green_weight)
				    && (vis_blue_weight >= blue_weight))
				{
					red_weight = vis_red_weight;
					green_weight = vis_green_weight;
					blue_weight = vis_blue_weight;
					g_visual = visual_info->visual;
					g_depth = visual_info->depth;
				}
			}
		}
		XFree(vmatches);
	}

	if (g_visual != NULL)
	{
		g_owncolmap = False;
		calculate_shifts(g_visual->red_mask, &g_red_shift_r, &g_red_shift_l);
		calculate_shifts(g_visual->green_mask, &g_green_shift_r, &g_green_shift_l);
		calculate_shifts(g_visual->blue_mask, &g_blue_shift_r, &g_blue_shift_l);
	}
	else
	{
		template.class = PseudoColor;
		template.depth = 8;
		template.colormap_size = 256;
		vmatches =
			XGetVisualInfo(g_display,
				       VisualClassMask | VisualDepthMask | VisualColormapSizeMask,
				       &template, &visuals_count);
		if (vmatches == NULL)
		{
			error("No usable TrueColor or PseudoColor visuals on this display.\n");
			XCloseDisplay(g_display);
			XFree(pfm);
			return False;
		}

		/* we use a colourmap, so the default visual should do */
		g_owncolmap = True;
		g_visual = vmatches[0].visual;
		g_depth = vmatches[0].depth;
	}

	g_bpp = 0;
	for (i = 0; i < pixmap_formats_count; ++i)
	{
		XPixmapFormatValues *pf = &pfm[i];
		if (pf->depth == g_depth)
		{
			g_bpp = pf->bits_per_pixel;

			if (g_no_translate_image)
			{
				switch (g_server_depth)
				{
					case 15:
					case 16:
						if (g_bpp != 16)
							g_no_translate_image = False;
						break;
					case 24:
						/* Yes, this will force image translation
						   on most modern servers which use 32 bits
						   for R8G8B8. */
						if (g_bpp != 24)
							g_no_translate_image = False;
						break;
					default:
						g_no_translate_image = False;
						break;
				}
			}

			/* Pixmap formats list is a depth-to-bpp mapping --
			   there's just a single entry for every depth,
			   so we can safely break here */
			break;
		}
	}
	XFree(pfm);
	pfm = NULL;
	return True;
}

static XErrorHandler g_old_error_handler;
static RD_BOOL g_error_expected = False;

/* Check if the X11 window corresponding to a seamless window with
   specified id exists. */
RD_BOOL
sw_window_exists(unsigned long id)
{
	seamless_window *sw;
	char *name;
	Status sts = 0;

	sw = sw_get_window_by_id(id);
	if (!sw)
		return False;

	g_error_expected = True;
	sts = XFetchName(g_display, sw->wnd, &name);
	g_error_expected = False;
	if (sts)
	{
		XFree(name);
	}

	return sts;
}

static int
error_handler(Display * dpy, XErrorEvent * eev)
{
	if (g_error_expected)
		return 0;

	return g_old_error_handler(dpy, eev);
}

/* Initialize the UI. This is done once per process. */
RD_BOOL
ui_init(void)
{
	int screen_num;

	g_display = XOpenDisplay(NULL);
	if (g_display == NULL)
	{
		error("Failed to open display: %s\n", XDisplayName(NULL));
		return False;
	}

	{
		uint16 endianess_test = 1;
		g_host_be = !(RD_BOOL) (*(uint8 *) (&endianess_test));
	}

	g_old_error_handler = XSetErrorHandler(error_handler);
	g_xserver_be = (ImageByteOrder(g_display) == MSBFirst);
	screen_num = DefaultScreen(g_display);
	g_x_socket = ConnectionNumber(g_display);
	g_screen = ScreenOfDisplay(g_display, screen_num);
	g_depth = DefaultDepthOfScreen(g_screen);

	if (!select_visual(screen_num))
		return False;

	if (g_no_translate_image)
	{
		DEBUG(("Performance optimization possible: avoiding image translation (colour depth conversion).\n"));
	}

	if (g_server_depth > g_bpp)
	{
		warning("Remote desktop colour depth %d higher than display colour depth %d.\n",
			g_server_depth, g_bpp);
	}

	DEBUG(("RDP depth: %d, display depth: %d, display bpp: %d, X server BE: %d, host BE: %d\n",
	       g_server_depth, g_depth, g_bpp, g_xserver_be, g_host_be));

	if (!g_owncolmap)
	{
		g_xcolmap =
			XCreateColormap(g_display, RootWindowOfScreen(g_screen), g_visual,
					AllocNone);
		if (g_depth <= 8)
			warning("Display colour depth is %d bit: you may want to use -C for a private colourmap.\n", g_depth);
	}

	if ((!g_ownbackstore) && (DoesBackingStore(g_screen) != Always))
	{
		warning("External BackingStore not available. Using internal.\n");
		g_ownbackstore = True;
	}

	g_mod_map = XGetModifierMapping(g_display);
	xwin_refresh_pointer_map();

	xkeymap_init();

	if (g_enable_compose)
		g_IM = XOpenIM(g_display, NULL, NULL, NULL);

	xclip_init();
	ewmh_init();
	if (g_seamless_rdp)
	{
		seamless_init();
	}

	DEBUG_RDP5(("server bpp %d client bpp %d depth %d\n", g_server_depth, g_bpp, g_depth));

	return True;
}


/* 
   Initialize connection specific data, such as session size. 
 */
void
ui_init_connection(void)
{
	/*
	 * Determine desktop size
	 */
	if (g_fullscreen)
	{
		g_width = WidthOfScreen(g_screen);
		g_height = HeightOfScreen(g_screen);
		g_using_full_workarea = True;
	}
	else if (g_sizeopt < 0)
	{
		/* Percent of screen */
		if (-g_sizeopt >= 100)
			g_using_full_workarea = True;
		g_height = HeightOfScreen(g_screen) * (-g_sizeopt) / 100;
		g_width = WidthOfScreen(g_screen) * (-g_sizeopt) / 100;
	}
	else if (g_sizeopt == 1)
	{
		/* Fetch geometry from _NET_WORKAREA */
		uint32 x, y, cx, cy;
		if (get_current_workarea(&x, &y, &cx, &cy) == 0)
		{
			g_width = cx;
			g_height = cy;
			g_using_full_workarea = True;
		}
		else
		{
			warning("Failed to get workarea: probably your window manager does not support extended hints\n");
			g_width = WidthOfScreen(g_screen);
			g_height = HeightOfScreen(g_screen);
		}
	}

	/* make sure width is a multiple of 4 */
	g_width = (g_width + 3) & ~3;
}


void
ui_deinit(void)
{
	xclip_deinit();

	if (g_IM != NULL)
		XCloseIM(g_IM);

	if (g_null_cursor != NULL)
		ui_destroy_cursor(g_null_cursor);

	XFreeModifiermap(g_mod_map);

	XFreeGC(g_display, g_gc);
	XCloseDisplay(g_display);
	g_display = NULL;
}


static void
get_window_attribs(XSetWindowAttributes * attribs)
{
	attribs->background_pixel = BlackPixelOfScreen(g_screen);
	attribs->border_pixel = WhitePixelOfScreen(g_screen);
	attribs->backing_store = g_ownbackstore ? NotUseful : Always;
	attribs->override_redirect = g_fullscreen;
	attribs->colormap = g_xcolmap;
}

static void
get_input_mask(long *input_mask)
{
	*input_mask = KeyPressMask | KeyReleaseMask | ButtonPressMask | ButtonReleaseMask |
		VisibilityChangeMask | FocusChangeMask | StructureNotifyMask;

	if (g_sendmotion)
		*input_mask |= PointerMotionMask;
	if (g_ownbackstore)
		*input_mask |= ExposureMask;
	if (g_fullscreen || g_grab_keyboard)
		*input_mask |= EnterWindowMask;
	if (g_grab_keyboard)
		*input_mask |= LeaveWindowMask;
}

RD_BOOL
ui_create_window(void)
{
	uint8 null_pointer_mask[1] = { 0x80 };
	uint8 null_pointer_data[24] = { 0x00 };

	XSetWindowAttributes attribs;
	XClassHint *classhints;
	XSizeHints *sizehints;
	int wndwidth, wndheight;
	long input_mask, ic_input_mask;
	XEvent xevent;

	wndwidth = g_fullscreen ? WidthOfScreen(g_screen) : g_width;
	wndheight = g_fullscreen ? HeightOfScreen(g_screen) : g_height;

	/* Handle -x-y portion of geometry string */
	if (g_xpos < 0 || (g_xpos == 0 && (g_pos & 2)))
		g_xpos = WidthOfScreen(g_screen) + g_xpos - g_width;
	if (g_ypos < 0 || (g_ypos == 0 && (g_pos & 4)))
		g_ypos = HeightOfScreen(g_screen) + g_ypos - g_height;

	get_window_attribs(&attribs);

	g_wnd = XCreateWindow(g_display, RootWindowOfScreen(g_screen), g_xpos, g_ypos, wndwidth,
			      wndheight, 0, g_depth, InputOutput, g_visual,
			      CWBackPixel | CWBackingStore | CWOverrideRedirect | CWColormap |
			      CWBorderPixel, &attribs);

	if (g_gc == NULL)
	{
		g_gc = XCreateGC(g_display, g_wnd, 0, NULL);
		ui_reset_clip();
	}

	if (g_create_bitmap_gc == NULL)
		g_create_bitmap_gc = XCreateGC(g_display, g_wnd, 0, NULL);

	if ((g_ownbackstore) && (g_backstore == 0))
	{
		g_backstore = XCreatePixmap(g_display, g_wnd, g_width, g_height, g_depth);

		/* clear to prevent rubbish being exposed at startup */
		XSetForeground(g_display, g_gc, BlackPixelOfScreen(g_screen));
		XFillRectangle(g_display, g_backstore, g_gc, 0, 0, g_width, g_height);
	}

	XStoreName(g_display, g_wnd, g_title);
	ewmh_set_wm_name(g_wnd, g_title);

	if (g_hide_decorations)
		mwm_hide_decorations(g_wnd);

	classhints = XAllocClassHint();
	if (classhints != NULL)
	{
		classhints->res_name = classhints->res_class = "rdesktop";
		XSetClassHint(g_display, g_wnd, classhints);
		XFree(classhints);
	}

	sizehints = XAllocSizeHints();
	if (sizehints)
	{
		sizehints->flags = PMinSize | PMaxSize;
		if (g_pos)
			sizehints->flags |= PPosition;
		sizehints->min_width = sizehints->max_width = g_width;
		sizehints->min_height = sizehints->max_height = g_height;
		XSetWMNormalHints(g_display, g_wnd, sizehints);
		XFree(sizehints);
	}

	if (g_embed_wnd)
	{
		XReparentWindow(g_display, g_wnd, (Window) g_embed_wnd, 0, 0);
	}

	get_input_mask(&input_mask);

	if (g_IM != NULL)
	{
		g_IC = XCreateIC(g_IM, XNInputStyle, (XIMPreeditNothing | XIMStatusNothing),
				 XNClientWindow, g_wnd, XNFocusWindow, g_wnd, NULL);

		if ((g_IC != NULL)
		    && (XGetICValues(g_IC, XNFilterEvents, &ic_input_mask, NULL) == NULL))
			input_mask |= ic_input_mask;
	}

	XSelectInput(g_display, g_wnd, input_mask);
#ifdef HAVE_XRANDR
	XSelectInput(g_display, RootWindowOfScreen(g_screen), StructureNotifyMask);
#endif
	XMapWindow(g_display, g_wnd);

	/* wait for VisibilityNotify */
	do
	{
		XMaskEvent(g_display, VisibilityChangeMask, &xevent);
	}
	while (xevent.type != VisibilityNotify);
	g_Unobscured = xevent.xvisibility.state == VisibilityUnobscured;

	g_focused = False;
	g_mouse_in_wnd = False;

	/* handle the WM_DELETE_WINDOW protocol */
	g_protocol_atom = XInternAtom(g_display, "WM_PROTOCOLS", True);
	g_kill_atom = XInternAtom(g_display, "WM_DELETE_WINDOW", True);
	XSetWMProtocols(g_display, g_wnd, &g_kill_atom, 1);

	/* create invisible 1x1 cursor to be used as null cursor */
	if (g_null_cursor == NULL)
		g_null_cursor =
			ui_create_cursor(0, 0, 1, 1, null_pointer_mask, null_pointer_data, 24);

	if (g_seamless_rdp)
	{
		seamless_reset_state();
		seamless_restack_test();
	}

	return True;
}

void
ui_resize_window()
{
	XSizeHints *sizehints;
	Pixmap bs;

	sizehints = XAllocSizeHints();
	if (sizehints)
	{
		sizehints->flags = PMinSize | PMaxSize;
		sizehints->min_width = sizehints->max_width = g_width;
		sizehints->min_height = sizehints->max_height = g_height;
		XSetWMNormalHints(g_display, g_wnd, sizehints);
		XFree(sizehints);
	}

	if (!(g_fullscreen || g_embed_wnd))
	{
		XResizeWindow(g_display, g_wnd, g_width, g_height);
	}

	/* create new backstore pixmap */
	if (g_backstore != 0)
	{
		bs = XCreatePixmap(g_display, g_wnd, g_width, g_height, g_depth);
		XSetForeground(g_display, g_gc, BlackPixelOfScreen(g_screen));
		XFillRectangle(g_display, bs, g_gc, 0, 0, g_width, g_height);
		XCopyArea(g_display, g_backstore, bs, g_gc, 0, 0, g_width, g_height, 0, 0);
		XFreePixmap(g_display, g_backstore);
		g_backstore = bs;
	}
}

RD_BOOL
ui_have_window()
{
	return g_wnd ? True : False;
}

void
ui_destroy_window(void)
{
	if (g_IC != NULL)
		XDestroyIC(g_IC);

	XDestroyWindow(g_display, g_wnd);
	g_wnd = 0;

	if (g_backstore)
	{
		XFreePixmap(g_display, g_backstore);
		g_backstore = 0;
	}
}

void
xwin_toggle_fullscreen(void)
{
	Pixmap contents = 0;

	if (g_seamless_active)
		/* Turn off SeamlessRDP mode */
		ui_seamless_toggle();

	if (!g_ownbackstore)
	{
		/* need to save contents of window */
		contents = XCreatePixmap(g_display, g_wnd, g_width, g_height, g_depth);
		XCopyArea(g_display, g_wnd, contents, g_gc, 0, 0, g_width, g_height, 0, 0);
	}

	ui_destroy_window();
	g_fullscreen = !g_fullscreen;
	ui_create_window();

	XDefineCursor(g_display, g_wnd, g_current_cursor);

	if (!g_ownbackstore)
	{
		XCopyArea(g_display, contents, g_wnd, g_gc, 0, 0, g_width, g_height, 0, 0);
		XFreePixmap(g_display, contents);
	}
}

static void
handle_button_event(XEvent xevent, RD_BOOL down)
{
	uint16 button, flags = 0;
	g_last_gesturetime = xevent.xbutton.time;
	/* Reverse the pointer button mapping, e.g. in the case of
	   "left-handed mouse mode"; the RDP session expects to
	   receive physical buttons (true in mstsc as well) and
	   logical button behavior depends on the remote desktop's own
	   mouse settings */
	xevent.xbutton.button = g_pointer_log_to_phys_map[xevent.xbutton.button - 1];
	button = xkeymap_translate_button(xevent.xbutton.button);
	if (button == 0)
		return;

	if (down)
		flags = MOUSE_FLAG_DOWN;

	/* Stop moving window when button is released, regardless of cursor position */
	if (g_moving_wnd && (xevent.type == ButtonRelease))
		g_moving_wnd = False;

	/* If win_button_size is nonzero, enable single app mode */
	if (xevent.xbutton.y < g_win_button_size)
	{
		/*  Check from right to left: */
		if (xevent.xbutton.x >= g_width - g_win_button_size)
		{
			/* The close button, continue */
			;
		}
		else if (xevent.xbutton.x >= g_width - g_win_button_size * 2)
		{
			/* The maximize/restore button. Do not send to
			   server.  It might be a good idea to change the
			   cursor or give some other visible indication
			   that rdesktop inhibited this click */
			if (xevent.type == ButtonPress)
				return;
		}
		else if (xevent.xbutton.x >= g_width - g_win_button_size * 3)
		{
			/* The minimize button. Iconify window. */
			if (xevent.type == ButtonRelease)
			{
				/* Release the mouse button outside the minimize button, to prevent the
				   actual minimazation to happen */
				rdp_send_input(time(NULL), RDP_INPUT_MOUSE, button, 1, 1);
				XIconifyWindow(g_display, g_wnd, DefaultScreen(g_display));
				return;
			}
		}
		else if (xevent.xbutton.x <= g_win_button_size)
		{
			/* The system menu. Ignore. */
			if (xevent.type == ButtonPress)
				return;
		}
		else
		{
			/* The title bar. */
			if (xevent.type == ButtonPress)
			{
				if (!g_fullscreen && g_hide_decorations && !g_using_full_workarea)
				{
					g_moving_wnd = True;
					g_move_x_offset = xevent.xbutton.x;
					g_move_y_offset = xevent.xbutton.y;
				}
				return;
			}
		}
	}

	/* Ignore mouse scroll button release event which will be handled as an additional
	 * scrolldown event on the Windows side.
	 */
	if (!down && (button == MOUSE_FLAG_BUTTON4 || button == MOUSE_FLAG_BUTTON5))
	{
		return;
	}

	if (xevent.xmotion.window == g_wnd)
	{
		rdp_send_input(time(NULL), RDP_INPUT_MOUSE,
			       flags | button, xevent.xbutton.x, xevent.xbutton.y);
	}
	else
	{
		/* SeamlessRDP */
		rdp_send_input(time(NULL), RDP_INPUT_MOUSE,
			       flags | button, xevent.xbutton.x_root, xevent.xbutton.y_root);
	}
}


/* Process events in Xlib queue
   Returns 0 after user quit, 1 otherwise */
static int
xwin_process_events(void)
{
	XEvent xevent;
	KeySym keysym;
	uint32 ev_time;
	char str[256];
	Status status;
	int events = 0;
	seamless_window *sw;

	while ((XPending(g_display) > 0) && events++ < 20)
	{
		XNextEvent(g_display, &xevent);

		if (!g_wnd)
			/* Ignore events between ui_destroy_window and ui_create_window */
			continue;

		/* Also ignore root window events except ConfigureNotify */
		if (xevent.type != ConfigureNotify
		    && xevent.xany.window == DefaultRootWindow(g_display))
			continue;

		if ((g_IC != NULL) && (XFilterEvent(&xevent, None) == True))
		{
			DEBUG_KBD(("Filtering event\n"));
			continue;
		}

		switch (xevent.type)
		{
			case VisibilityNotify:
				if (xevent.xvisibility.window == g_wnd)
					g_Unobscured =
						xevent.xvisibility.state == VisibilityUnobscured;

				break;
			case ClientMessage:
				/* the window manager told us to quit */
				if ((xevent.xclient.message_type == g_protocol_atom)
				    && ((Atom) xevent.xclient.data.l[0] == g_kill_atom))
				{
					/* When killing a seamless window, close the window on the
					   serverside instead of terminating rdesktop */
					sw = sw_get_window_by_wnd(xevent.xclient.window);
					if (!sw)
						/* Otherwise, quit */
						return 0;
					/* send seamless destroy process message */
					seamless_send_destroy(sw->id);
				}
				break;

			case KeyPress:
				g_last_gesturetime = xevent.xkey.time;
				if (g_IC != NULL)
					/* Multi_key compatible version */
				{
					XmbLookupString(g_IC,
							&xevent.xkey, str, sizeof(str), &keysym,
							&status);
					if (!((status == XLookupKeySym) || (status == XLookupBoth)))
					{
						error("XmbLookupString failed with status 0x%x\n",
						      status);
						break;
					}
				}
				else
				{
					/* Plain old XLookupString */
					DEBUG_KBD(("\nNo input context, using XLookupString\n"));
					XLookupString((XKeyEvent *) & xevent,
						      str, sizeof(str), &keysym, NULL);
				}

				DEBUG_KBD(("KeyPress for keysym (0x%lx, %s)\n", keysym,
					   get_ksname(keysym)));

				set_keypress_keysym(xevent.xkey.keycode, keysym);
				ev_time = time(NULL);
				if (handle_special_keys(keysym, xevent.xkey.state, ev_time, True))
					break;

				xkeymap_send_keys(keysym, xevent.xkey.keycode, xevent.xkey.state,
						  ev_time, True, 0);
				break;

			case KeyRelease:
				g_last_gesturetime = xevent.xkey.time;
				XLookupString((XKeyEvent *) & xevent, str,
					      sizeof(str), &keysym, NULL);

				DEBUG_KBD(("\nKeyRelease for keysym (0x%lx, %s)\n", keysym,
					   get_ksname(keysym)));

				keysym = reset_keypress_keysym(xevent.xkey.keycode, keysym);
				ev_time = time(NULL);
				if (handle_special_keys(keysym, xevent.xkey.state, ev_time, False))
					break;

				xkeymap_send_keys(keysym, xevent.xkey.keycode, xevent.xkey.state,
						  ev_time, False, 0);
				break;

			case ButtonPress:
				handle_button_event(xevent, True);
				break;

			case ButtonRelease:
				handle_button_event(xevent, False);
				break;

			case MotionNotify:
				if (g_moving_wnd)
				{
					XMoveWindow(g_display, g_wnd,
						    xevent.xmotion.x_root - g_move_x_offset,
						    xevent.xmotion.y_root - g_move_y_offset);
					break;
				}

				if (g_fullscreen && !g_focused)
					XSetInputFocus(g_display, g_wnd, RevertToPointerRoot,
						       CurrentTime);

				if (xevent.xmotion.window == g_wnd)
				{
					rdp_send_input(time(NULL), RDP_INPUT_MOUSE, MOUSE_FLAG_MOVE,
						       xevent.xmotion.x, xevent.xmotion.y);
				}
				else
				{
					/* SeamlessRDP */
					rdp_send_input(time(NULL), RDP_INPUT_MOUSE, MOUSE_FLAG_MOVE,
						       xevent.xmotion.x_root,
						       xevent.xmotion.y_root);
				}
				break;

			case FocusIn:
				if (xevent.xfocus.mode == NotifyGrab)
					break;
				g_focused = True;
				reset_modifier_keys();
				if (g_grab_keyboard && g_mouse_in_wnd)
					XGrabKeyboard(g_display, g_wnd, True,
						      GrabModeAsync, GrabModeAsync, CurrentTime);

				sw = sw_get_window_by_wnd(xevent.xfocus.window);
				if (!sw)
					break;

				/* Menu windows are real X11 windows,
				   with focus. When such a window is
				   destroyed, focus is reverted to the
				   main application window, which
				   would cause us to send FOCUS. This
				   breaks window switching in, say,
				   Seamonkey. We shouldn't need to
				   send FOCUS: Windows should also
				   revert focus to some other window
				   when the menu window is
				   destroyed. So, we only send FOCUS
				   if the previous focus window still
				   exists. */
				if (sw->id != g_seamless_focused)
				{

					if (sw_window_exists(g_seamless_focused))
						seamless_send_focus(sw->id, 0);
					g_seamless_focused = sw->id;
				}
				break;

			case FocusOut:
				if (xevent.xfocus.mode == NotifyUngrab)
					break;
				g_focused = False;
				if (xevent.xfocus.mode == NotifyWhileGrabbed)
					XUngrabKeyboard(g_display, CurrentTime);
				break;

			case EnterNotify:
				/* we only register for this event when in fullscreen mode */
				/* or grab_keyboard */
				g_mouse_in_wnd = True;
				if (g_fullscreen)
				{
					XSetInputFocus(g_display, g_wnd, RevertToPointerRoot,
						       CurrentTime);
					break;
				}
				if (g_focused)
					XGrabKeyboard(g_display, g_wnd, True,
						      GrabModeAsync, GrabModeAsync, CurrentTime);
				break;

			case LeaveNotify:
				/* we only register for this event when grab_keyboard */
				g_mouse_in_wnd = False;
				XUngrabKeyboard(g_display, CurrentTime);
				/* VirtualBox code begin */
				if (g_fullscreen)
				{
					/* If mouse pointer is outside the fullscreen client window,
					 * release it to let the client on the other screen to continue
					 * with the mouse processing.
					 */
					XUngrabPointer(g_display, CurrentTime);
				}
				/* VirtualBox code end */
				break;

			case Expose:
				if (xevent.xexpose.window == g_wnd)
				{
					XCopyArea(g_display, g_backstore, xevent.xexpose.window,
						  g_gc,
						  xevent.xexpose.x, xevent.xexpose.y,
						  xevent.xexpose.width, xevent.xexpose.height,
						  xevent.xexpose.x, xevent.xexpose.y);
				}
				else
				{
					sw = sw_get_window_by_wnd(xevent.xexpose.window);
					if (!sw)
						break;
					XCopyArea(g_display, g_backstore,
						  xevent.xexpose.window, g_gc,
						  xevent.xexpose.x + sw->xoffset,
						  xevent.xexpose.y + sw->yoffset,
						  xevent.xexpose.width,
						  xevent.xexpose.height, xevent.xexpose.x,
						  xevent.xexpose.y);
				}

				break;

			case MappingNotify:
				/* Refresh keyboard mapping if it has changed. This is important for
				   Xvnc, since it allocates keycodes dynamically */
				if (xevent.xmapping.request == MappingKeyboard
				    || xevent.xmapping.request == MappingModifier)
					XRefreshKeyboardMapping(&xevent.xmapping);

				if (xevent.xmapping.request == MappingModifier)
				{
					XFreeModifiermap(g_mod_map);
					g_mod_map = XGetModifierMapping(g_display);
				}

				if (xevent.xmapping.request == MappingPointer)
				{
					xwin_refresh_pointer_map();
				}

				break;

				/* clipboard stuff */
			case SelectionNotify:
				xclip_handle_SelectionNotify(&xevent.xselection);
				break;
			case SelectionRequest:
				xclip_handle_SelectionRequest(&xevent.xselectionrequest);
				break;
			case SelectionClear:
				xclip_handle_SelectionClear();
				break;
			case PropertyNotify:
				xclip_handle_PropertyNotify(&xevent.xproperty);
				if (xevent.xproperty.window == g_wnd)
					break;
				if (xevent.xproperty.window == DefaultRootWindow(g_display))
					break;

				/* seamless */
				sw = sw_get_window_by_wnd(xevent.xproperty.window);
				if (!sw)
					break;

				if ((xevent.xproperty.atom == g_net_wm_state_atom)
				    && (xevent.xproperty.state == PropertyNewValue))
				{
					sw->state = ewmh_get_window_state(sw->wnd);
					seamless_send_state(sw->id, sw->state, 0);
				}

				if ((xevent.xproperty.atom == g_net_wm_desktop_atom)
				    && (xevent.xproperty.state == PropertyNewValue))
				{
					sw->desktop = ewmh_get_window_desktop(sw->wnd);
					sw_all_to_desktop(sw->wnd, sw->desktop);
				}

				break;
			case MapNotify:
				if (!g_seamless_active)
					rdp_send_client_window_status(1);
				break;
			case UnmapNotify:
				if (!g_seamless_active)
					rdp_send_client_window_status(0);
				break;
			case ConfigureNotify:
#ifdef HAVE_XRANDR
				if ((g_sizeopt || g_fullscreen)
				    && xevent.xconfigure.window == DefaultRootWindow(g_display))
				{
					if (xevent.xconfigure.width != WidthOfScreen(g_screen)
					    || xevent.xconfigure.height != HeightOfScreen(g_screen))
					{
						XRRUpdateConfiguration(&xevent);
						XSync(g_display, False);
						g_pending_resize = True;
					}

				}
#endif
				if (!g_seamless_active)
					break;

				sw = sw_get_window_by_wnd(xevent.xconfigure.window);
				if (!sw)
					break;

				gettimeofday(sw->position_timer, NULL);
				if (sw->position_timer->tv_usec + SEAMLESSRDP_POSITION_TIMER >=
				    1000000)
				{
					sw->position_timer->tv_usec +=
						SEAMLESSRDP_POSITION_TIMER - 1000000;
					sw->position_timer->tv_sec += 1;
				}
				else
				{
					sw->position_timer->tv_usec += SEAMLESSRDP_POSITION_TIMER;
				}

				sw_handle_restack(sw);
				break;
		}
	}
	/* Keep going */
	return 1;
}

/* Returns 0 after user quit, 1 otherwise */
int
ui_select(int rdp_socket)
{
	int n;
	fd_set rfds, wfds;
	struct timeval tv;
	RD_BOOL s_timeout = False;

	while (True)
	{
		n = (rdp_socket > g_x_socket) ? rdp_socket : g_x_socket;
		/* Process any events already waiting */
		if (!xwin_process_events())
			/* User quit */
			return 0;

		if (g_seamless_active)
			sw_check_timers();

		FD_ZERO(&rfds);
		FD_ZERO(&wfds);
		FD_SET(rdp_socket, &rfds);
		FD_SET(g_x_socket, &rfds);

		/* default timeout */
		tv.tv_sec = 60;
		tv.tv_usec = 0;

#ifdef WITH_RDPSND
		rdpsnd_add_fds(&n, &rfds, &wfds, &tv);
#endif

#ifdef WITH_RDPUSB
		rdpusb_add_fds (&n, &rfds, &wfds);
#endif

		/* add redirection handles */
		rdpdr_add_fds(&n, &rfds, &wfds, &tv, &s_timeout);
		seamless_select_timeout(&tv);

		/* add ctrl slaves handles */
		ctrl_add_fds(&n, &rfds);

		n++;

		switch (select(n, &rfds, &wfds, NULL, &tv))
		{
			case -1:
				error("select: %s\n", strerror(errno));

			case 0:
#ifdef WITH_RDPSND
				rdpsnd_check_fds(&rfds, &wfds);
#endif

				/* Abort serial read calls */
				if (s_timeout)
					rdpdr_check_fds(&rfds, &wfds, (RD_BOOL) True);
				continue;
		}

#ifdef WITH_RDPUSB
		rdpusb_check_fds (&rfds, &wfds);
#endif

#ifdef WITH_RDPSND
		rdpsnd_check_fds(&rfds, &wfds);
#endif

		rdpdr_check_fds(&rfds, &wfds, (RD_BOOL) False);

		ctrl_check_fds(&rfds, &wfds);

		if (FD_ISSET(rdp_socket, &rfds))
			return 1;

	}
}

void
ui_move_pointer(int x, int y)
{
	XWarpPointer(g_display, g_wnd, g_wnd, 0, 0, 0, 0, x, y);
}

RD_HBITMAP
ui_create_bitmap(int width, int height, uint8 * data)
{
	XImage *image;
	Pixmap bitmap;
	uint8 *tdata;
	int bitmap_pad;

	if (g_server_depth == 8)
	{
		bitmap_pad = 8;
	}
	else
	{
		bitmap_pad = g_bpp;

		if (g_bpp == 24)
			bitmap_pad = 32;
	}

	tdata = (g_owncolmap ? data : translate_image(width, height, data));
	bitmap = XCreatePixmap(g_display, g_wnd, width, height, g_depth);
	image = XCreateImage(g_display, g_visual, g_depth, ZPixmap, 0,
			     (char *) tdata, width, height, bitmap_pad, 0);

	XPutImage(g_display, bitmap, g_create_bitmap_gc, image, 0, 0, 0, 0, width, height);

	XFree(image);
	if (tdata != data)
		xfree(tdata);
	return (RD_HBITMAP) bitmap;
}

void
ui_paint_bitmap(int x, int y, int cx, int cy, int width, int height, uint8 * data)
{
	XImage *image;
	uint8 *tdata;
	int bitmap_pad;

	if (g_server_depth == 8)
	{
		bitmap_pad = 8;
	}
	else
	{
		bitmap_pad = g_bpp;

		if (g_bpp == 24)
			bitmap_pad = 32;
	}

	tdata = (g_owncolmap ? data : translate_image(width, height, data));
	image = XCreateImage(g_display, g_visual, g_depth, ZPixmap, 0,
			     (char *) tdata, width, height, bitmap_pad, 0);

	if (g_ownbackstore)
	{
		XPutImage(g_display, g_backstore, g_gc, image, 0, 0, x, y, cx, cy);
		XCopyArea(g_display, g_backstore, g_wnd, g_gc, x, y, cx, cy, x, y);
		ON_ALL_SEAMLESS_WINDOWS(XCopyArea,
					(g_display, g_backstore, sw->wnd, g_gc, x, y, cx, cy,
					 x - sw->xoffset, y - sw->yoffset));
	}
	else
	{
		XPutImage(g_display, g_wnd, g_gc, image, 0, 0, x, y, cx, cy);
		ON_ALL_SEAMLESS_WINDOWS(XCopyArea,
					(g_display, g_wnd, sw->wnd, g_gc, x, y, cx, cy,
					 x - sw->xoffset, y - sw->yoffset));
	}

	XFree(image);
	if (tdata != data)
		xfree(tdata);
}

void
ui_destroy_bitmap(RD_HBITMAP bmp)
{
	XFreePixmap(g_display, (Pixmap) bmp);
}

RD_HGLYPH
ui_create_glyph(int width, int height, uint8 * data)
{
	XImage *image;
	Pixmap bitmap;
	int scanline;

	scanline = (width + 7) / 8;

	bitmap = XCreatePixmap(g_display, g_wnd, width, height, 1);
	if (g_create_glyph_gc == 0)
		g_create_glyph_gc = XCreateGC(g_display, bitmap, 0, NULL);

	image = XCreateImage(g_display, g_visual, 1, ZPixmap, 0, (char *) data,
			     width, height, 8, scanline);
	image->byte_order = MSBFirst;
	image->bitmap_bit_order = MSBFirst;
	XInitImage(image);

	XPutImage(g_display, bitmap, g_create_glyph_gc, image, 0, 0, 0, 0, width, height);

	XFree(image);
	return (RD_HGLYPH) bitmap;
}

void
ui_destroy_glyph(RD_HGLYPH glyph)
{
	XFreePixmap(g_display, (Pixmap) glyph);
}

/* convert next pixel to 32 bpp */
static int
get_next_xor_pixel(uint8 * xormask, int bpp, int *k)
{
	int rv = 0;
	PixelColour pc;
	uint8 *s8;
	uint16 *s16;

	switch (bpp)
	{
		case 1:
			s8 = xormask + (*k) / 8;
			rv = (*s8) & (0x80 >> ((*k) % 8));
			rv = rv ? 0xffffff : 0;
			(*k) += 1;
			break;
		case 8:
			s8 = xormask + *k;
			/* should use colour map */
			rv = s8[0];
			rv = rv ? 0xffffff : 0;
			(*k) += 1;
			break;
		case 15:
			s16 = (uint16 *) xormask;
			SPLITCOLOUR15(s16[*k], pc);
			rv = (pc.red << 16) | (pc.green << 8) | pc.blue;
			(*k) += 1;
			break;
		case 16:
			s16 = (uint16 *) xormask;
			SPLITCOLOUR16(s16[*k], pc);
			rv = (pc.red << 16) | (pc.green << 8) | pc.blue;
			(*k) += 1;
			break;
		case 24:
			s8 = xormask + *k;
			rv = (s8[0] << 16) | (s8[1] << 8) | s8[2];
			(*k) += 3;
			break;
		case 32:
			s8 = xormask + *k;
			rv = (s8[1] << 16) | (s8[2] << 8) | s8[3];
			(*k) += 4;
			break;
		default:
			error("unknown bpp in get_next_xor_pixel %d\n", bpp);
			break;
	}
	return rv;
}

RD_HCURSOR
ui_create_cursor(unsigned int x, unsigned int y, int width, int height,
		 uint8 * andmask, uint8 * xormask, int bpp)
{
	RD_HGLYPH maskglyph, cursorglyph;
	XColor bg, fg;
	Cursor xcursor;
	uint8 *cursor, *pcursor;
	uint8 *mask, *pmask;
	uint8 nextbit;
	int scanline, offset, delta;
	int i, j, k;

	k = 0;
	scanline = (width + 7) / 8;
	offset = scanline * height;

	cursor = (uint8 *) xmalloc(offset);
	memset(cursor, 0, offset);

	mask = (uint8 *) xmalloc(offset);
	memset(mask, 0, offset);
	if (bpp == 1)
	{
		offset = 0;
		delta = scanline;
	}
	else
	{
		offset = scanline * height - scanline;
		delta = -scanline;
	}
	/* approximate AND and XOR masks with a monochrome X pointer */
	for (i = 0; i < height; i++)
	{
		pcursor = &cursor[offset];
		pmask = &mask[offset];

		for (j = 0; j < scanline; j++)
		{
			for (nextbit = 0x80; nextbit != 0; nextbit >>= 1)
			{
				if (get_next_xor_pixel(xormask, bpp, &k))
				{
					*pcursor |= (~(*andmask) & nextbit);
					*pmask |= nextbit;
				}
				else
				{
					*pcursor |= ((*andmask) & nextbit);
					*pmask |= (~(*andmask) & nextbit);
				}
			}

			andmask++;
			pcursor++;
			pmask++;
		}
		offset += delta;
	}

	fg.red = fg.blue = fg.green = 0xffff;
	bg.red = bg.blue = bg.green = 0x0000;
	fg.flags = bg.flags = DoRed | DoBlue | DoGreen;

	cursorglyph = ui_create_glyph(width, height, cursor);
	maskglyph = ui_create_glyph(width, height, mask);

	xcursor =
		XCreatePixmapCursor(g_display, (Pixmap) cursorglyph,
				    (Pixmap) maskglyph, &fg, &bg, x, y);

	ui_destroy_glyph(maskglyph);
	ui_destroy_glyph(cursorglyph);
	xfree(mask);
	xfree(cursor);
	return (RD_HCURSOR) xcursor;
}

void
ui_set_cursor(RD_HCURSOR cursor)
{
	g_current_cursor = (Cursor) cursor;
	XDefineCursor(g_display, g_wnd, g_current_cursor);
	ON_ALL_SEAMLESS_WINDOWS(XDefineCursor, (g_display, sw->wnd, g_current_cursor));
}

void
ui_destroy_cursor(RD_HCURSOR cursor)
{
	XFreeCursor(g_display, (Cursor) cursor);
}

void
ui_set_null_cursor(void)
{
	ui_set_cursor(g_null_cursor);
}

#define MAKE_XCOLOR(xc,c) \
		(xc)->red   = ((c)->red   << 8) | (c)->red; \
		(xc)->green = ((c)->green << 8) | (c)->green; \
		(xc)->blue  = ((c)->blue  << 8) | (c)->blue; \
		(xc)->flags = DoRed | DoGreen | DoBlue;


RD_HCOLOURMAP
ui_create_colourmap(COLOURMAP * colours)
{
	COLOURENTRY *entry;
	int i, ncolours = colours->ncolours;
	if (!g_owncolmap)
	{
		uint32 *map = (uint32 *) xmalloc(sizeof(*g_colmap) * ncolours);
		XColor xentry;
		XColor xc_cache[256];
		uint32 colour;
		int colLookup = 256;
		for (i = 0; i < ncolours; i++)
		{
			entry = &colours->colours[i];
			MAKE_XCOLOR(&xentry, entry);

			if (XAllocColor(g_display, g_xcolmap, &xentry) == 0)
			{
				/* Allocation failed, find closest match. */
				int j = 256;
				int nMinDist = 3 * 256 * 256;
				long nDist = nMinDist;

				/* only get the colors once */
				while (colLookup--)
				{
					xc_cache[colLookup].pixel = colLookup;
					xc_cache[colLookup].red = xc_cache[colLookup].green =
						xc_cache[colLookup].blue = 0;
					xc_cache[colLookup].flags = 0;
					XQueryColor(g_display,
						    DefaultColormap(g_display,
								    DefaultScreen(g_display)),
						    &xc_cache[colLookup]);
				}
				colLookup = 0;

				/* approximate the pixel */
				while (j--)
				{
					if (xc_cache[j].flags)
					{
						nDist = ((long) (xc_cache[j].red >> 8) -
							 (long) (xentry.red >> 8)) *
							((long) (xc_cache[j].red >> 8) -
							 (long) (xentry.red >> 8)) +
							((long) (xc_cache[j].green >> 8) -
							 (long) (xentry.green >> 8)) *
							((long) (xc_cache[j].green >> 8) -
							 (long) (xentry.green >> 8)) +
							((long) (xc_cache[j].blue >> 8) -
							 (long) (xentry.blue >> 8)) *
							((long) (xc_cache[j].blue >> 8) -
							 (long) (xentry.blue >> 8));
					}
					if (nDist < nMinDist)
					{
						nMinDist = nDist;
						xentry.pixel = j;
					}
				}
			}
			colour = xentry.pixel;

			/* update our cache */
			if (xentry.pixel < 256)
			{
				xc_cache[xentry.pixel].red = xentry.red;
				xc_cache[xentry.pixel].green = xentry.green;
				xc_cache[xentry.pixel].blue = xentry.blue;

			}

			map[i] = colour;
		}
		return map;
	}
	else
	{
		XColor *xcolours, *xentry;
		Colormap map;

		xcolours = (XColor *) xmalloc(sizeof(XColor) * ncolours);
		for (i = 0; i < ncolours; i++)
		{
			entry = &colours->colours[i];
			xentry = &xcolours[i];
			xentry->pixel = i;
			MAKE_XCOLOR(xentry, entry);
		}

		map = XCreateColormap(g_display, g_wnd, g_visual, AllocAll);
		XStoreColors(g_display, map, xcolours, ncolours);

		xfree(xcolours);
		return (RD_HCOLOURMAP) map;
	}
}

void
ui_destroy_colourmap(RD_HCOLOURMAP map)
{
	if (!g_owncolmap)
		xfree(map);
	else
		XFreeColormap(g_display, (Colormap) map);
}

void
ui_set_colourmap(RD_HCOLOURMAP map)
{
	if (!g_owncolmap)
	{
		if (g_colmap)
			xfree(g_colmap);

		g_colmap = (uint32 *) map;
	}
	else
	{
		XSetWindowColormap(g_display, g_wnd, (Colormap) map);
		ON_ALL_SEAMLESS_WINDOWS(XSetWindowColormap, (g_display, sw->wnd, (Colormap) map));
	}
}

void
ui_set_clip(int x, int y, int cx, int cy)
{
	g_clip_rectangle.x = x;
	g_clip_rectangle.y = y;
	g_clip_rectangle.width = cx;
	g_clip_rectangle.height = cy;
	XSetClipRectangles(g_display, g_gc, 0, 0, &g_clip_rectangle, 1, YXBanded);
}

void
ui_reset_clip(void)
{
	g_clip_rectangle.x = 0;
	g_clip_rectangle.y = 0;
	g_clip_rectangle.width = g_width;
	g_clip_rectangle.height = g_height;
	XSetClipRectangles(g_display, g_gc, 0, 0, &g_clip_rectangle, 1, YXBanded);
}

void
ui_bell(void)
{
	XBell(g_display, 0);
}

void
ui_destblt(uint8 opcode,
	   /* dest */ int x, int y, int cx, int cy)
{
	SET_FUNCTION(opcode);
	FILL_RECTANGLE(x, y, cx, cy);
	RESET_FUNCTION(opcode);
}

static uint8 hatch_patterns[] = {
	0x00, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00, 0x00,	/* 0 - bsHorizontal */
	0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08,	/* 1 - bsVertical */
	0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01,	/* 2 - bsFDiagonal */
	0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80,	/* 3 - bsBDiagonal */
	0x08, 0x08, 0x08, 0xff, 0x08, 0x08, 0x08, 0x08,	/* 4 - bsCross */
	0x81, 0x42, 0x24, 0x18, 0x18, 0x24, 0x42, 0x81	/* 5 - bsDiagCross */
};

void
ui_patblt(uint8 opcode,
	  /* dest */ int x, int y, int cx, int cy,
	  /* brush */ BRUSH * brush, int bgcolour, int fgcolour)
{
	Pixmap fill;
	uint8 i, ipattern[8];

	SET_FUNCTION(opcode);

	switch (brush->style)
	{
		case 0:	/* Solid */
			SET_FOREGROUND(fgcolour);
			FILL_RECTANGLE_BACKSTORE(x, y, cx, cy);
			break;

		case 2:	/* Hatch */
			fill = (Pixmap) ui_create_glyph(8, 8,
							hatch_patterns + brush->pattern[0] * 8);
			SET_FOREGROUND(fgcolour);
			SET_BACKGROUND(bgcolour);
			XSetFillStyle(g_display, g_gc, FillOpaqueStippled);
			XSetStipple(g_display, g_gc, fill);
			XSetTSOrigin(g_display, g_gc, brush->xorigin, brush->yorigin);
			FILL_RECTANGLE_BACKSTORE(x, y, cx, cy);
			XSetFillStyle(g_display, g_gc, FillSolid);
			XSetTSOrigin(g_display, g_gc, 0, 0);
			ui_destroy_glyph((RD_HGLYPH) fill);
			break;

		case 3:	/* Pattern */
			if (brush->bd == 0)	/* rdp4 brush */
			{
				for (i = 0; i != 8; i++)
					ipattern[7 - i] = brush->pattern[i];
				fill = (Pixmap) ui_create_glyph(8, 8, ipattern);
				SET_FOREGROUND(bgcolour);
				SET_BACKGROUND(fgcolour);
				XSetFillStyle(g_display, g_gc, FillOpaqueStippled);
				XSetStipple(g_display, g_gc, fill);
				XSetTSOrigin(g_display, g_gc, brush->xorigin, brush->yorigin);
				FILL_RECTANGLE_BACKSTORE(x, y, cx, cy);
				XSetFillStyle(g_display, g_gc, FillSolid);
				XSetTSOrigin(g_display, g_gc, 0, 0);
				ui_destroy_glyph((RD_HGLYPH) fill);
			}
			else if (brush->bd->colour_code > 1)	/* > 1 bpp */
			{
				fill = (Pixmap) ui_create_bitmap(8, 8, brush->bd->data);
				XSetFillStyle(g_display, g_gc, FillTiled);
				XSetTile(g_display, g_gc, fill);
				XSetTSOrigin(g_display, g_gc, brush->xorigin, brush->yorigin);
				FILL_RECTANGLE_BACKSTORE(x, y, cx, cy);
				XSetFillStyle(g_display, g_gc, FillSolid);
				XSetTSOrigin(g_display, g_gc, 0, 0);
				ui_destroy_bitmap((RD_HBITMAP) fill);
			}
			else
			{
				fill = (Pixmap) ui_create_glyph(8, 8, brush->bd->data);
				SET_FOREGROUND(bgcolour);
				SET_BACKGROUND(fgcolour);
				XSetFillStyle(g_display, g_gc, FillOpaqueStippled);
				XSetStipple(g_display, g_gc, fill);
				XSetTSOrigin(g_display, g_gc, brush->xorigin, brush->yorigin);
				FILL_RECTANGLE_BACKSTORE(x, y, cx, cy);
				XSetFillStyle(g_display, g_gc, FillSolid);
				XSetTSOrigin(g_display, g_gc, 0, 0);
				ui_destroy_glyph((RD_HGLYPH) fill);
			}
			break;

		default:
			unimpl("brush %d\n", brush->style);
	}

	RESET_FUNCTION(opcode);

	if (g_ownbackstore)
		XCopyArea(g_display, g_backstore, g_wnd, g_gc, x, y, cx, cy, x, y);
	ON_ALL_SEAMLESS_WINDOWS(XCopyArea,
				(g_display, g_ownbackstore ? g_backstore : g_wnd, sw->wnd, g_gc,
				 x, y, cx, cy, x - sw->xoffset, y - sw->yoffset));
}

void
ui_screenblt(uint8 opcode,
	     /* dest */ int x, int y, int cx, int cy,
	     /* src */ int srcx, int srcy)
{
	SET_FUNCTION(opcode);
	if (g_ownbackstore)
	{
		XCopyArea(g_display, g_Unobscured ? g_wnd : g_backstore,
			  g_wnd, g_gc, srcx, srcy, cx, cy, x, y);
		XCopyArea(g_display, g_backstore, g_backstore, g_gc, srcx, srcy, cx, cy, x, y);
	}
	else
	{
		XCopyArea(g_display, g_wnd, g_wnd, g_gc, srcx, srcy, cx, cy, x, y);
	}

	ON_ALL_SEAMLESS_WINDOWS(XCopyArea,
				(g_display, g_ownbackstore ? g_backstore : g_wnd,
				 sw->wnd, g_gc, x, y, cx, cy, x - sw->xoffset, y - sw->yoffset));

	RESET_FUNCTION(opcode);
}

void
ui_memblt(uint8 opcode,
	  /* dest */ int x, int y, int cx, int cy,
	  /* src */ RD_HBITMAP src, int srcx, int srcy)
{
	SET_FUNCTION(opcode);
	XCopyArea(g_display, (Pixmap) src, g_wnd, g_gc, srcx, srcy, cx, cy, x, y);
	ON_ALL_SEAMLESS_WINDOWS(XCopyArea,
				(g_display, (Pixmap) src, sw->wnd, g_gc,
				 srcx, srcy, cx, cy, x - sw->xoffset, y - sw->yoffset));
	if (g_ownbackstore)
		XCopyArea(g_display, (Pixmap) src, g_backstore, g_gc, srcx, srcy, cx, cy, x, y);
	RESET_FUNCTION(opcode);
}

void
ui_triblt(uint8 opcode,
	  /* dest */ int x, int y, int cx, int cy,
	  /* src */ RD_HBITMAP src, int srcx, int srcy,
	  /* brush */ BRUSH * brush, int bgcolour, int fgcolour)
{
	/* This is potentially difficult to do in general. Until someone
	   comes up with a more efficient way of doing it I am using cases. */

	switch (opcode)
	{
		case 0x69:	/* PDSxxn */
			ui_memblt(ROP2_XOR, x, y, cx, cy, src, srcx, srcy);
			ui_patblt(ROP2_NXOR, x, y, cx, cy, brush, bgcolour, fgcolour);
			break;

		case 0xb8:	/* PSDPxax */
			ui_patblt(ROP2_XOR, x, y, cx, cy, brush, bgcolour, fgcolour);
			ui_memblt(ROP2_AND, x, y, cx, cy, src, srcx, srcy);
			ui_patblt(ROP2_XOR, x, y, cx, cy, brush, bgcolour, fgcolour);
			break;

		case 0xc0:	/* PSa */
			ui_memblt(ROP2_COPY, x, y, cx, cy, src, srcx, srcy);
			ui_patblt(ROP2_AND, x, y, cx, cy, brush, bgcolour, fgcolour);
			break;

		default:
			unimpl("triblt 0x%x\n", opcode);
			ui_memblt(ROP2_COPY, x, y, cx, cy, src, srcx, srcy);
	}
}

void
ui_line(uint8 opcode,
	/* dest */ int startx, int starty, int endx, int endy,
	/* pen */ PEN * pen)
{
	SET_FUNCTION(opcode);
	SET_FOREGROUND(pen->colour);
	XDrawLine(g_display, g_wnd, g_gc, startx, starty, endx, endy);
	ON_ALL_SEAMLESS_WINDOWS(XDrawLine, (g_display, sw->wnd, g_gc,
					    startx - sw->xoffset, starty - sw->yoffset,
					    endx - sw->xoffset, endy - sw->yoffset));
	if (g_ownbackstore)
		XDrawLine(g_display, g_backstore, g_gc, startx, starty, endx, endy);
	RESET_FUNCTION(opcode);
}

void
ui_rect(
	       /* dest */ int x, int y, int cx, int cy,
	       /* brush */ int colour)
{
	SET_FOREGROUND(colour);
	FILL_RECTANGLE(x, y, cx, cy);
}

void
ui_polygon(uint8 opcode,
	   /* mode */ uint8 fillmode,
	   /* dest */ RD_POINT * point, int npoints,
	   /* brush */ BRUSH * brush, int bgcolour, int fgcolour)
{
	uint8 style, i, ipattern[8];
	Pixmap fill;

	SET_FUNCTION(opcode);

	switch (fillmode)
	{
		case ALTERNATE:
			XSetFillRule(g_display, g_gc, EvenOddRule);
			break;
		case WINDING:
			XSetFillRule(g_display, g_gc, WindingRule);
			break;
		default:
			unimpl("fill mode %d\n", fillmode);
	}

	if (brush)
		style = brush->style;
	else
		style = 0;

	switch (style)
	{
		case 0:	/* Solid */
			SET_FOREGROUND(fgcolour);
			FILL_POLYGON((XPoint *) point, npoints);
			break;

		case 2:	/* Hatch */
			fill = (Pixmap) ui_create_glyph(8, 8,
							hatch_patterns + brush->pattern[0] * 8);
			SET_FOREGROUND(fgcolour);
			SET_BACKGROUND(bgcolour);
			XSetFillStyle(g_display, g_gc, FillOpaqueStippled);
			XSetStipple(g_display, g_gc, fill);
			XSetTSOrigin(g_display, g_gc, brush->xorigin, brush->yorigin);
			FILL_POLYGON((XPoint *) point, npoints);
			XSetFillStyle(g_display, g_gc, FillSolid);
			XSetTSOrigin(g_display, g_gc, 0, 0);
			ui_destroy_glyph((RD_HGLYPH) fill);
			break;

		case 3:	/* Pattern */
			if (brush->bd == 0)	/* rdp4 brush */
			{
				for (i = 0; i != 8; i++)
					ipattern[7 - i] = brush->pattern[i];
				fill = (Pixmap) ui_create_glyph(8, 8, ipattern);
				SET_FOREGROUND(bgcolour);
				SET_BACKGROUND(fgcolour);
				XSetFillStyle(g_display, g_gc, FillOpaqueStippled);
				XSetStipple(g_display, g_gc, fill);
				XSetTSOrigin(g_display, g_gc, brush->xorigin, brush->yorigin);
				FILL_POLYGON((XPoint *) point, npoints);
				XSetFillStyle(g_display, g_gc, FillSolid);
				XSetTSOrigin(g_display, g_gc, 0, 0);
				ui_destroy_glyph((RD_HGLYPH) fill);
			}
			else if (brush->bd->colour_code > 1)	/* > 1 bpp */
			{
				fill = (Pixmap) ui_create_bitmap(8, 8, brush->bd->data);
				XSetFillStyle(g_display, g_gc, FillTiled);
				XSetTile(g_display, g_gc, fill);
				XSetTSOrigin(g_display, g_gc, brush->xorigin, brush->yorigin);
				FILL_POLYGON((XPoint *) point, npoints);
				XSetFillStyle(g_display, g_gc, FillSolid);
				XSetTSOrigin(g_display, g_gc, 0, 0);
				ui_destroy_bitmap((RD_HBITMAP) fill);
			}
			else
			{
				fill = (Pixmap) ui_create_glyph(8, 8, brush->bd->data);
				SET_FOREGROUND(bgcolour);
				SET_BACKGROUND(fgcolour);
				XSetFillStyle(g_display, g_gc, FillOpaqueStippled);
				XSetStipple(g_display, g_gc, fill);
				XSetTSOrigin(g_display, g_gc, brush->xorigin, brush->yorigin);
				FILL_POLYGON((XPoint *) point, npoints);
				XSetFillStyle(g_display, g_gc, FillSolid);
				XSetTSOrigin(g_display, g_gc, 0, 0);
				ui_destroy_glyph((RD_HGLYPH) fill);
			}
			break;

		default:
			unimpl("brush %d\n", brush->style);
	}

	RESET_FUNCTION(opcode);
}

void
ui_polyline(uint8 opcode,
	    /* dest */ RD_POINT * points, int npoints,
	    /* pen */ PEN * pen)
{
	/* TODO: set join style */
	SET_FUNCTION(opcode);
	SET_FOREGROUND(pen->colour);
	XDrawLines(g_display, g_wnd, g_gc, (XPoint *) points, npoints, CoordModePrevious);
	if (g_ownbackstore)
		XDrawLines(g_display, g_backstore, g_gc, (XPoint *) points, npoints,
			   CoordModePrevious);

	ON_ALL_SEAMLESS_WINDOWS(seamless_XDrawLines,
				(sw->wnd, (XPoint *) points, npoints, sw->xoffset, sw->yoffset));

	RESET_FUNCTION(opcode);
}

void
ui_ellipse(uint8 opcode,
	   /* mode */ uint8 fillmode,
	   /* dest */ int x, int y, int cx, int cy,
	   /* brush */ BRUSH * brush, int bgcolour, int fgcolour)
{
	uint8 style, i, ipattern[8];
	Pixmap fill;

	SET_FUNCTION(opcode);

	if (brush)
		style = brush->style;
	else
		style = 0;

	switch (style)
	{
		case 0:	/* Solid */
			SET_FOREGROUND(fgcolour);
			DRAW_ELLIPSE(x, y, cx, cy, fillmode);
			break;

		case 2:	/* Hatch */
			fill = (Pixmap) ui_create_glyph(8, 8,
							hatch_patterns + brush->pattern[0] * 8);
			SET_FOREGROUND(fgcolour);
			SET_BACKGROUND(bgcolour);
			XSetFillStyle(g_display, g_gc, FillOpaqueStippled);
			XSetStipple(g_display, g_gc, fill);
			XSetTSOrigin(g_display, g_gc, brush->xorigin, brush->yorigin);
			DRAW_ELLIPSE(x, y, cx, cy, fillmode);
			XSetFillStyle(g_display, g_gc, FillSolid);
			XSetTSOrigin(g_display, g_gc, 0, 0);
			ui_destroy_glyph((RD_HGLYPH) fill);
			break;

		case 3:	/* Pattern */
			if (brush->bd == 0)	/* rdp4 brush */
			{
				for (i = 0; i != 8; i++)
					ipattern[7 - i] = brush->pattern[i];
				fill = (Pixmap) ui_create_glyph(8, 8, ipattern);
				SET_FOREGROUND(bgcolour);
				SET_BACKGROUND(fgcolour);
				XSetFillStyle(g_display, g_gc, FillOpaqueStippled);
				XSetStipple(g_display, g_gc, fill);
				XSetTSOrigin(g_display, g_gc, brush->xorigin, brush->yorigin);
				DRAW_ELLIPSE(x, y, cx, cy, fillmode);
				XSetFillStyle(g_display, g_gc, FillSolid);
				XSetTSOrigin(g_display, g_gc, 0, 0);
				ui_destroy_glyph((RD_HGLYPH) fill);
			}
			else if (brush->bd->colour_code > 1)	/* > 1 bpp */
			{
				fill = (Pixmap) ui_create_bitmap(8, 8, brush->bd->data);
				XSetFillStyle(g_display, g_gc, FillTiled);
				XSetTile(g_display, g_gc, fill);
				XSetTSOrigin(g_display, g_gc, brush->xorigin, brush->yorigin);
				DRAW_ELLIPSE(x, y, cx, cy, fillmode);
				XSetFillStyle(g_display, g_gc, FillSolid);
				XSetTSOrigin(g_display, g_gc, 0, 0);
				ui_destroy_bitmap((RD_HBITMAP) fill);
			}
			else
			{
				fill = (Pixmap) ui_create_glyph(8, 8, brush->bd->data);
				SET_FOREGROUND(bgcolour);
				SET_BACKGROUND(fgcolour);
				XSetFillStyle(g_display, g_gc, FillOpaqueStippled);
				XSetStipple(g_display, g_gc, fill);
				XSetTSOrigin(g_display, g_gc, brush->xorigin, brush->yorigin);
				DRAW_ELLIPSE(x, y, cx, cy, fillmode);
				XSetFillStyle(g_display, g_gc, FillSolid);
				XSetTSOrigin(g_display, g_gc, 0, 0);
				ui_destroy_glyph((RD_HGLYPH) fill);
			}
			break;

		default:
			unimpl("brush %d\n", brush->style);
	}

	RESET_FUNCTION(opcode);
}

/* warning, this function only draws on wnd or backstore, not both */
void
ui_draw_glyph(int mixmode,
	      /* dest */ int x, int y, int cx, int cy,
	      /* src */ RD_HGLYPH glyph, int srcx, int srcy,
	      int bgcolour, int fgcolour)
{
	SET_FOREGROUND(fgcolour);
	SET_BACKGROUND(bgcolour);

	XSetFillStyle(g_display, g_gc,
		      (mixmode == MIX_TRANSPARENT) ? FillStippled : FillOpaqueStippled);
	XSetStipple(g_display, g_gc, (Pixmap) glyph);
	XSetTSOrigin(g_display, g_gc, x, y);

	FILL_RECTANGLE_BACKSTORE(x, y, cx, cy);

	XSetFillStyle(g_display, g_gc, FillSolid);
}

#define DO_GLYPH(ttext,idx) \
{\
  glyph = cache_get_font (font, ttext[idx]);\
  if (!(flags & TEXT2_IMPLICIT_X))\
  {\
    xyoffset = ttext[++idx];\
    if ((xyoffset & 0x80))\
    {\
      if (flags & TEXT2_VERTICAL)\
        y += ttext[idx+1] | (ttext[idx+2] << 8);\
      else\
        x += ttext[idx+1] | (ttext[idx+2] << 8);\
      idx += 2;\
    }\
    else\
    {\
      if (flags & TEXT2_VERTICAL)\
        y += xyoffset;\
      else\
        x += xyoffset;\
    }\
  }\
  if (glyph != NULL)\
  {\
    x1 = x + glyph->offset;\
    y1 = y + glyph->baseline;\
    XSetStipple(g_display, g_gc, (Pixmap) glyph->pixmap);\
    XSetTSOrigin(g_display, g_gc, x1, y1);\
    FILL_RECTANGLE_BACKSTORE(x1, y1, glyph->width, glyph->height);\
    if (flags & TEXT2_IMPLICIT_X)\
      x += glyph->width;\
  }\
}

void
ui_draw_text(uint8 font, uint8 flags, uint8 opcode, int mixmode, int x, int y,
	     int clipx, int clipy, int clipcx, int clipcy,
	     int boxx, int boxy, int boxcx, int boxcy, BRUSH * brush,
	     int bgcolour, int fgcolour, uint8 * text, uint8 length)
{
	/* TODO: use brush appropriately */

	FONTGLYPH *glyph;
	int i, j, xyoffset, x1, y1;
	DATABLOB *entry;

	SET_FOREGROUND(bgcolour);

	/* Sometimes, the boxcx value is something really large, like
	   32691. This makes XCopyArea fail with Xvnc. The code below
	   is a quick fix. */
	if (boxx + boxcx > g_width)
		boxcx = g_width - boxx;

	if (boxcx > 1)
	{
		FILL_RECTANGLE_BACKSTORE(boxx, boxy, boxcx, boxcy);
	}
	else if (mixmode == MIX_OPAQUE)
	{
		FILL_RECTANGLE_BACKSTORE(clipx, clipy, clipcx, clipcy);
	}

	SET_FOREGROUND(fgcolour);
	SET_BACKGROUND(bgcolour);
	XSetFillStyle(g_display, g_gc, FillStippled);

	/* Paint text, character by character */
	for (i = 0; i < length;)
	{
		switch (text[i])
		{
			case 0xff:
				/* At least two bytes needs to follow */
				if (i + 3 > length)
				{
					warning("Skipping short 0xff command:");
					for (j = 0; j < length; j++)
						fprintf(stderr, "%02x ", text[j]);
					fprintf(stderr, "\n");
					i = length = 0;
					break;
				}
				cache_put_text(text[i + 1], text, text[i + 2]);
				i += 3;
				length -= i;
				/* this will move pointer from start to first character after FF command */
				text = &(text[i]);
				i = 0;
				break;

			case 0xfe:
				/* At least one byte needs to follow */
				if (i + 2 > length)
				{
					warning("Skipping short 0xfe command:");
					for (j = 0; j < length; j++)
						fprintf(stderr, "%02x ", text[j]);
					fprintf(stderr, "\n");
					i = length = 0;
					break;
				}
				entry = cache_get_text(text[i + 1]);
				if (entry->data != NULL)
				{
					if ((((uint8 *) (entry->data))[1] == 0)
					    && (!(flags & TEXT2_IMPLICIT_X)) && (i + 2 < length))
					{
						if (flags & TEXT2_VERTICAL)
							y += text[i + 2];
						else
							x += text[i + 2];
					}
					for (j = 0; j < entry->size; j++)
						DO_GLYPH(((uint8 *) (entry->data)), j);
				}
				if (i + 2 < length)
					i += 3;
				else
					i += 2;
				length -= i;
				/* this will move pointer from start to first character after FE command */
				text = &(text[i]);
				i = 0;
				break;

			default:
				DO_GLYPH(text, i);
				i++;
				break;
		}
	}

	XSetFillStyle(g_display, g_gc, FillSolid);

	if (g_ownbackstore)
	{
		if (boxcx > 1)
		{
			XCopyArea(g_display, g_backstore, g_wnd, g_gc, boxx,
				  boxy, boxcx, boxcy, boxx, boxy);
			ON_ALL_SEAMLESS_WINDOWS(XCopyArea,
						(g_display, g_backstore, sw->wnd, g_gc,
						 boxx, boxy,
						 boxcx, boxcy,
						 boxx - sw->xoffset, boxy - sw->yoffset));
		}
		else
		{
			XCopyArea(g_display, g_backstore, g_wnd, g_gc, clipx,
				  clipy, clipcx, clipcy, clipx, clipy);
			ON_ALL_SEAMLESS_WINDOWS(XCopyArea,
						(g_display, g_backstore, sw->wnd, g_gc,
						 clipx, clipy,
						 clipcx, clipcy, clipx - sw->xoffset,
						 clipy - sw->yoffset));
		}
	}
}

void
ui_desktop_save(uint32 offset, int x, int y, int cx, int cy)
{
	Pixmap pix;
	XImage *image;

	if (g_ownbackstore)
	{
		image = XGetImage(g_display, g_backstore, x, y, cx, cy, AllPlanes, ZPixmap);
		exit_if_null(image);
	}
	else
	{
		pix = XCreatePixmap(g_display, g_wnd, cx, cy, g_depth);
		XCopyArea(g_display, g_wnd, pix, g_gc, x, y, cx, cy, 0, 0);
		image = XGetImage(g_display, pix, 0, 0, cx, cy, AllPlanes, ZPixmap);
		exit_if_null(image);
		XFreePixmap(g_display, pix);
	}

	offset *= g_bpp / 8;
	cache_put_desktop(offset, cx, cy, image->bytes_per_line, g_bpp / 8, (uint8 *) image->data);

	XDestroyImage(image);
}

void
ui_desktop_restore(uint32 offset, int x, int y, int cx, int cy)
{
	XImage *image;
	uint8 *data;

	offset *= g_bpp / 8;
	data = cache_get_desktop(offset, cx, cy, g_bpp / 8);
	if (data == NULL)
		return;

	image = XCreateImage(g_display, g_visual, g_depth, ZPixmap, 0,
			     (char *) data, cx, cy, g_bpp, 0);

	if (g_ownbackstore)
	{
		XPutImage(g_display, g_backstore, g_gc, image, 0, 0, x, y, cx, cy);
		XCopyArea(g_display, g_backstore, g_wnd, g_gc, x, y, cx, cy, x, y);
		ON_ALL_SEAMLESS_WINDOWS(XCopyArea,
					(g_display, g_backstore, sw->wnd, g_gc,
					 x, y, cx, cy, x - sw->xoffset, y - sw->yoffset));
	}
	else
	{
		XPutImage(g_display, g_wnd, g_gc, image, 0, 0, x, y, cx, cy);
		ON_ALL_SEAMLESS_WINDOWS(XCopyArea,
					(g_display, g_wnd, sw->wnd, g_gc, x, y, cx, cy,
					 x - sw->xoffset, y - sw->yoffset));
	}

	XFree(image);
}

/* these do nothing here but are used in uiports */
void
ui_begin_update(void)
{
}

void
ui_end_update(void)
{
	XFlush(g_display);
}


void
ui_seamless_begin(RD_BOOL hidden)
{
	if (!g_seamless_rdp)
		return;

	if (g_seamless_started)
		return;

	g_seamless_started = True;
	g_seamless_hidden = hidden;

	if (!hidden)
		ui_seamless_toggle();

	if (g_seamless_spawn_cmd[0])
	{
		seamless_send_spawn(g_seamless_spawn_cmd);
		g_seamless_spawn_cmd[0] = 0;
	}

	seamless_send_persistent(g_seamless_persistent_mode);
}


void
ui_seamless_end()
{
	/* Destroy all seamless windows */
	while (g_seamless_windows)
	{
		XDestroyWindow(g_display, g_seamless_windows->wnd);
		sw_remove_window(g_seamless_windows);
	}

	g_seamless_started = False;
	g_seamless_active = False;
	g_seamless_hidden = False;
}


void
ui_seamless_hide_desktop()
{
	if (!g_seamless_rdp)
		return;

	if (!g_seamless_started)
		return;

	if (g_seamless_active)
		ui_seamless_toggle();

	g_seamless_hidden = True;
}


void
ui_seamless_unhide_desktop()
{
	if (!g_seamless_rdp)
		return;

	if (!g_seamless_started)
		return;

	g_seamless_hidden = False;

	ui_seamless_toggle();
}


void
ui_seamless_toggle()
{
	if (!g_seamless_rdp)
		return;

	if (!g_seamless_started)
		return;

	if (g_seamless_hidden)
		return;

	if (g_seamless_active)
	{
		/* Deactivate */
		while (g_seamless_windows)
		{
			XDestroyWindow(g_display, g_seamless_windows->wnd);
			sw_remove_window(g_seamless_windows);
		}
		XMapWindow(g_display, g_wnd);
	}
	else
	{
		/* Activate */
		XUnmapWindow(g_display, g_wnd);
		seamless_send_sync();
	}

	g_seamless_active = !g_seamless_active;
}


void
ui_seamless_create_window(unsigned long id, unsigned long group, unsigned long parent,
			  unsigned long flags)
{
	Window wnd;
	XSetWindowAttributes attribs;
	XClassHint *classhints;
	XSizeHints *sizehints;
	XWMHints *wmhints;
	long input_mask;
	seamless_window *sw, *sw_parent;

	if (!g_seamless_active)
		return;

	/* Ignore CREATEs for existing windows */
	sw = sw_get_window_by_id(id);
	if (sw)
		return;

	get_window_attribs(&attribs);
	wnd = XCreateWindow(g_display, RootWindowOfScreen(g_screen), -1, -1, 1, 1, 0, g_depth,
			    InputOutput, g_visual,
			    CWBackPixel | CWBackingStore | CWColormap | CWBorderPixel, &attribs);

	XStoreName(g_display, wnd, "SeamlessRDP");
	ewmh_set_wm_name(wnd, "SeamlessRDP");

	mwm_hide_decorations(wnd);

	classhints = XAllocClassHint();
	if (classhints != NULL)
	{
		classhints->res_name = "rdesktop";
		classhints->res_class = "SeamlessRDP";
		XSetClassHint(g_display, wnd, classhints);
		XFree(classhints);
	}

	/* WM_NORMAL_HINTS */
	sizehints = XAllocSizeHints();
	if (sizehints != NULL)
	{
		sizehints->flags = USPosition;
		XSetWMNormalHints(g_display, wnd, sizehints);
		XFree(sizehints);
	}

	/* Parent-less transient windows */
	if (parent == 0xFFFFFFFF)
	{
		XSetTransientForHint(g_display, wnd, RootWindowOfScreen(g_screen));
		/* Some buggy wm:s (kwin) do not handle the above, so fake it
		   using some other hints. */
		ewmh_set_window_popup(wnd);
	}
	/* Normal transient windows */
	else if (parent != 0x00000000)
	{
		sw_parent = sw_get_window_by_id(parent);
		if (sw_parent)
			XSetTransientForHint(g_display, wnd, sw_parent->wnd);
		else
			warning("ui_seamless_create_window: No parent window 0x%lx\n", parent);
	}

	if (flags & SEAMLESSRDP_CREATE_MODAL)
	{
		/* We do this to support buggy wm:s (*cough* metacity *cough*)
		   somewhat at least */
		if (parent == 0x00000000)
			XSetTransientForHint(g_display, wnd, RootWindowOfScreen(g_screen));
		ewmh_set_window_modal(wnd);
	}

	if (flags & SEAMLESSRDP_CREATE_TOPMOST)
	{
		/* Make window always-on-top */
		ewmh_set_window_above(wnd);
	}

	/* FIXME: Support for Input Context:s */

	get_input_mask(&input_mask);
	input_mask |= PropertyChangeMask;

	XSelectInput(g_display, wnd, input_mask);

	/* handle the WM_DELETE_WINDOW protocol. */
	XSetWMProtocols(g_display, wnd, &g_kill_atom, 1);

	sw = xmalloc(sizeof(seamless_window));

	memset(sw, 0, sizeof(seamless_window));

	sw->wnd = wnd;
	sw->id = id;
	sw->group = sw_find_group(group, False);
	sw->group->refcnt++;
	sw->state = SEAMLESSRDP_NOTYETMAPPED;
	sw->desktop = 0;
	sw->position_timer = xmalloc(sizeof(struct timeval));
	timerclear(sw->position_timer);

	sw->outstanding_position = False;
	sw->outpos_serial = 0;
	sw->outpos_xoffset = sw->outpos_yoffset = 0;
	sw->outpos_width = sw->outpos_height = 0;

	sw->next = g_seamless_windows;
	g_seamless_windows = sw;

	/* WM_HINTS */
	wmhints = XAllocWMHints();
	if (wmhints)
	{
		wmhints->flags = WindowGroupHint;
		wmhints->window_group = sw->group->wnd;
		XSetWMHints(g_display, sw->wnd, wmhints);
		XFree(wmhints);
	}
}


void
ui_seamless_destroy_window(unsigned long id, unsigned long flags)
{
	seamless_window *sw;

	if (!g_seamless_active)
		return;

	sw = sw_get_window_by_id(id);
	if (!sw)
	{
		warning("ui_seamless_destroy_window: No information for window 0x%lx\n", id);
		return;
	}

	XDestroyWindow(g_display, sw->wnd);
	sw_remove_window(sw);
}


void
ui_seamless_destroy_group(unsigned long id, unsigned long flags)
{
	seamless_window *sw, *sw_next;

	if (!g_seamless_active)
		return;

	for (sw = g_seamless_windows; sw; sw = sw_next)
	{
		sw_next = sw->next;

		if (sw->group->id == id)
		{
			XDestroyWindow(g_display, sw->wnd);
			sw_remove_window(sw);
		}
	}
}


void
ui_seamless_seticon(unsigned long id, const char *format, int width, int height, int chunk,
		    const char *data, int chunk_len)
{
	seamless_window *sw;

	if (!g_seamless_active)
		return;

	sw = sw_get_window_by_id(id);
	if (!sw)
	{
		warning("ui_seamless_seticon: No information for window 0x%lx\n", id);
		return;
	}

	if (chunk == 0)
	{
		if (sw->icon_size)
			warning("ui_seamless_seticon: New icon started before previous completed\n");

		if (strcmp(format, "RGBA") != 0)
		{
			warning("ui_seamless_seticon: Uknown icon format \"%s\"\n", format);
			return;
		}

		sw->icon_size = width * height * 4;
		if (sw->icon_size > 32 * 32 * 4)
		{
			warning("ui_seamless_seticon: Icon too large (%d bytes)\n", sw->icon_size);
			sw->icon_size = 0;
			return;
		}

		sw->icon_offset = 0;
	}
	else
	{
		if (!sw->icon_size)
			return;
	}

	if (chunk_len > (sw->icon_size - sw->icon_offset))
	{
		warning("ui_seamless_seticon: Too large chunk received (%d bytes > %d bytes)\n",
			chunk_len, sw->icon_size - sw->icon_offset);
		sw->icon_size = 0;
		return;
	}

	memcpy(sw->icon_buffer + sw->icon_offset, data, chunk_len);
	sw->icon_offset += chunk_len;

	if (sw->icon_offset == sw->icon_size)
	{
		ewmh_set_icon(sw->wnd, width, height, sw->icon_buffer);
		sw->icon_size = 0;
	}
}


void
ui_seamless_delicon(unsigned long id, const char *format, int width, int height)
{
	seamless_window *sw;

	if (!g_seamless_active)
		return;

	sw = sw_get_window_by_id(id);
	if (!sw)
	{
		warning("ui_seamless_seticon: No information for window 0x%lx\n", id);
		return;
	}

	if (strcmp(format, "RGBA") != 0)
	{
		warning("ui_seamless_seticon: Uknown icon format \"%s\"\n", format);
		return;
	}

	ewmh_del_icon(sw->wnd, width, height);
}


void
ui_seamless_move_window(unsigned long id, int x, int y, int width, int height, unsigned long flags)
{
	seamless_window *sw;

	if (!g_seamless_active)
		return;

	sw = sw_get_window_by_id(id);
	if (!sw)
	{
		warning("ui_seamless_move_window: No information for window 0x%lx\n", id);
		return;
	}

	/* We ignore server updates until it has handled our request. */
	if (sw->outstanding_position)
		return;

	if (!width || !height)
		/* X11 windows must be at least 1x1 */
		return;

	/* If we move the window in a maximized state, then KDE won't
	   accept restoration */
	switch (sw->state)
	{
		case SEAMLESSRDP_MINIMIZED:
		case SEAMLESSRDP_MAXIMIZED:
			sw_update_position(sw);
			return;
	}

	sw->xoffset = x;
	sw->yoffset = y;
	sw->width = width;
	sw->height = height;

	/* FIXME: Perhaps use ewmh_net_moveresize_window instead */
	XMoveResizeWindow(g_display, sw->wnd, sw->xoffset, sw->yoffset, sw->width, sw->height);
}


void
ui_seamless_restack_window(unsigned long id, unsigned long behind, unsigned long flags)
{
	seamless_window *sw;
	XWindowChanges values;
	unsigned long restack_serial;
	unsigned int value_mask;

	if (!g_seamless_active)
		return;

	sw = sw_get_window_by_id(id);
	if (!sw)
	{
		warning("ui_seamless_restack_window: No information for window 0x%lx\n", id);
		return;
	}

	if (behind)
	{
		seamless_window *sw_behind;

		sw_behind = sw_get_window_by_id(behind);
		if (!sw_behind)
		{
			warning("ui_seamless_restack_window: No information for behind window 0x%lx\n", behind);
			return;
		}

		values.stack_mode = Below;
		value_mask = CWStackMode | CWSibling;
		values.sibling = sw_behind->wnd;

		/* Avoid that topmost windows references non-topmost
		   windows, and vice versa. */
		if (ewmh_is_window_above(sw->wnd))
		{
			if (!ewmh_is_window_above(sw_behind->wnd))
			{
				/* Disallow, move to bottom of the
				   topmost stack. */
				values.stack_mode = Below;
				value_mask = CWStackMode;	/* Not sibling */
			}
		}
		else
		{
			if (ewmh_is_window_above(sw_behind->wnd))
			{
				/* Move to top of non-topmost
				   stack. */
				values.stack_mode = Above;
				value_mask = CWStackMode;	/* Not sibling */
			}
		}
	}
	else
	{
		values.stack_mode = Above;
		value_mask = CWStackMode;
	}

	restack_serial = XNextRequest(g_display);
	XReconfigureWMWindow(g_display, sw->wnd, DefaultScreen(g_display), value_mask, &values);
	sw_wait_configurenotify(sw->wnd, restack_serial);

	sw_restack_window(sw, behind);

	if (flags & SEAMLESSRDP_CREATE_TOPMOST)
	{
		/* Make window always-on-top */
		ewmh_set_window_above(sw->wnd);
	}
}


void
ui_seamless_settitle(unsigned long id, const char *title, unsigned long flags)
{
	seamless_window *sw;

	if (!g_seamless_active)
		return;

	sw = sw_get_window_by_id(id);
	if (!sw)
	{
		warning("ui_seamless_settitle: No information for window 0x%lx\n", id);
		return;
	}

	/* FIXME: Might want to convert the name for non-EWMH WMs */
	XStoreName(g_display, sw->wnd, title);
	ewmh_set_wm_name(sw->wnd, title);
}


void
ui_seamless_setstate(unsigned long id, unsigned int state, unsigned long flags)
{
	seamless_window *sw;

	if (!g_seamless_active)
		return;

	sw = sw_get_window_by_id(id);
	if (!sw)
	{
		warning("ui_seamless_setstate: No information for window 0x%lx\n", id);
		return;
	}

	switch (state)
	{
		case SEAMLESSRDP_NORMAL:
		case SEAMLESSRDP_MAXIMIZED:
			ewmh_change_state(sw->wnd, state);
			XMapWindow(g_display, sw->wnd);
			break;
		case SEAMLESSRDP_MINIMIZED:
			/* EWMH says: "if an Application asks to toggle _NET_WM_STATE_HIDDEN
			   the Window Manager should probably just ignore the request, since
			   _NET_WM_STATE_HIDDEN is a function of some other aspect of the window
			   such as minimization, rather than an independent state." Besides,
			   XIconifyWindow is easier. */
			if (sw->state == SEAMLESSRDP_NOTYETMAPPED)
			{
				XWMHints *hints;
				hints = XGetWMHints(g_display, sw->wnd);
				if (hints)
				{
					hints->flags |= StateHint;
					hints->initial_state = IconicState;
					XSetWMHints(g_display, sw->wnd, hints);
					XFree(hints);
				}
				XMapWindow(g_display, sw->wnd);
			}
			else
				XIconifyWindow(g_display, sw->wnd, DefaultScreen(g_display));
			break;
		default:
			warning("SeamlessRDP: Invalid state %d\n", state);
			break;
	}

	sw->state = state;
}


void
ui_seamless_syncbegin(unsigned long flags)
{
	if (!g_seamless_active)
		return;

	/* Destroy all seamless windows */
	while (g_seamless_windows)
	{
		XDestroyWindow(g_display, g_seamless_windows->wnd);
		sw_remove_window(g_seamless_windows);
	}
}


void
ui_seamless_ack(unsigned int serial)
{
	seamless_window *sw;
	for (sw = g_seamless_windows; sw; sw = sw->next)
	{
		if (sw->outstanding_position && (sw->outpos_serial == serial))
		{
			sw->xoffset = sw->outpos_xoffset;
			sw->yoffset = sw->outpos_yoffset;
			sw->width = sw->outpos_width;
			sw->height = sw->outpos_height;
			sw->outstanding_position = False;

			/* Do a complete redraw of the window as part of the
			   completion of the move. This is to remove any
			   artifacts caused by our lack of synchronization. */
			XCopyArea(g_display, g_backstore,
				  sw->wnd, g_gc,
				  sw->xoffset, sw->yoffset, sw->width, sw->height, 0, 0);

			break;
		}
	}
}
