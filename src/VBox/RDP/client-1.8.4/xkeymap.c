/* -*- c-basic-offset: 8 -*-
   rdesktop: A Remote Desktop Protocol client.
   User interface services - X keyboard mapping

   Copyright (C) Matthew Chapman <matthewc.unsw.edu.au> 1999-2008
   Copyright 2003-2008 Peter Astrand <astrand@cendio.se> for Cendio AB
   Copyright 2014 Henrik Andersson <hean01@cendio.se> for Cendio AB

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

#ifdef RDP2VNC
#include "vnc/x11stubs.h"
#else
#include <X11/Xlib.h>
#include <X11/keysym.h>
#endif

#include <ctype.h>
#include <limits.h>
#include <time.h>
#include <string.h>
#include <assert.h>
#include "rdesktop.h"
#include "scancodes.h"

#define KEYMAP_SIZE 0x7f+1
#define KEYMAP_MASK 0x7f
#define KEYMAP_MAX_LINE_LENGTH 80

extern Display *g_display;
extern Window g_wnd;
extern char g_keymapname[16];
extern unsigned int g_keylayout;
extern int g_keyboard_type;
extern int g_keyboard_subtype;
extern int g_keyboard_functionkeys;
extern int g_win_button_size;
extern RD_BOOL g_enable_compose;
extern RDP_VERSION g_rdp_version;
extern RD_BOOL g_numlock_sync;

static RD_BOOL keymap_loaded;
static key_translation_entry *keymap[KEYMAP_SIZE];
static KeySym keypress_keysyms[256];
static int min_keycode;
static uint16 remote_modifier_state = 0;
static uint16 saved_remote_modifier_state = 0;

static void update_modifier_state(uint8 scancode, RD_BOOL pressed);

/* Free key_translation structure, including linked list */
static void
free_key_translation(key_translation * ptr)
{
	key_translation *next;

	while (ptr)
	{
		next = ptr->next;
		xfree(ptr);
		ptr = next;
	}
}

/* Free the key_translation_entry for a given keysym and remove from the table */
static void
delete_key_translation_entry(KeySym keysym)
{
	uint32 hash;
	key_translation_entry *ptr;
	key_translation_entry *next;
	key_translation_entry *prev;
	key_translation_entry tmp;

	/* Faking a prev node allows us to keep the algorithm simple */
	hash = keysym & KEYMAP_MASK;
	ptr = keymap[hash];
	tmp.next = ptr;
	prev = &tmp;

	while (ptr)
	{
		next = ptr->next;
		if (ptr->keysym == keysym)
		{
			free_key_translation(ptr->tr);
			prev->next = next;
			xfree(ptr);
		}
		else
		{
			prev = ptr;
		}

		ptr = next;
	}

	/* Copy pointer back from our fake node */
	keymap[hash] = tmp.next;
}

/* Allocate and return a new entry in the translation table */
static key_translation_entry *
new_key_translation_entry(KeySym keysym)
{
	uint32 hash;
	key_translation_entry *entry;

	/* Clear out any existing entry */
	delete_key_translation_entry(keysym);

	/* Allocate the new one */
	entry = (key_translation_entry *) xmalloc(sizeof(key_translation_entry));
	memset(entry, 0, sizeof(key_translation_entry));
	entry->keysym = keysym;

	/* And insert it at head of list */
	hash = keysym & KEYMAP_MASK;
	entry->next = keymap[hash];
	keymap[hash] = entry;

	return entry;
}

/* Retrieve the key_translation_entry for a given keysym */
static key_translation_entry *
get_key_translation_entry(uint32 keysym)
{
	key_translation_entry *ptr;
	key_translation_entry *next;

	ptr = keymap[keysym & KEYMAP_MASK];

	while (ptr)
	{
		next = ptr->next;
		if (ptr->keysym == keysym)
			return ptr;

		ptr = next;
	}

	/* Not found */
	return NULL;
}

static void
add_to_keymap(char *keyname, uint8 scancode, uint16 modifiers, char *mapname)
{
	KeySym keysym;
	key_translation_entry *entry;

	keysym = XStringToKeysym(keyname);
	if (keysym == NoSymbol)
	{
		DEBUG_KBD(("Bad keysym \"%s\" in keymap %s (ignoring)\n", keyname, mapname));
		return;
	}

	DEBUG_KBD(("Adding translation, keysym=0x%x, scancode=0x%x, "
		   "modifiers=0x%x\n", (unsigned int) keysym, scancode, modifiers));

	/* Make a new entry in the table */
	entry = new_key_translation_entry(keysym);

	/* And add the new translation to it */
	entry->tr = (key_translation *) xmalloc(sizeof(key_translation));
	memset(entry->tr, 0, sizeof(key_translation));
	entry->tr->scancode = scancode;
	entry->tr->modifiers = modifiers;

	return;
}

static void
add_sequence(char *rest, char *mapname)
{
	KeySym keysym;
	KeySym seq_keysym;
	key_translation_entry *entry;
	key_translation *tr, **prev_next;
	size_t chars;
	char keyname[KEYMAP_MAX_LINE_LENGTH];

	/* Skip over whitespace after the sequence keyword */
	chars = strspn(rest, " \t");
	rest += chars;

	/* Fetch the keysym name */
	chars = strcspn(rest, " \t\0");
	STRNCPY(keyname, rest, chars + 1);
	rest += chars;

	keysym = XStringToKeysym(keyname);
	if (keysym == NoSymbol)
	{
		DEBUG_KBD(("Bad keysym \"%s\" in keymap %s (ignoring line)\n", keyname, mapname));
		return;
	}

	DEBUG_KBD(("Adding sequence for keysym (0x%lx, %s) -> ", keysym, keyname));

	entry = new_key_translation_entry(keysym);
	prev_next = &(entry->tr);

	while (*rest)
	{
		/* Skip whitespace */
		chars = strspn(rest, " \t");
		rest += chars;

		/* Fetch the keysym name */
		chars = strcspn(rest, " \t\0");
		STRNCPY(keyname, rest, chars + 1);
		rest += chars;

		/* Handle trailing whitespace */
		if (*keyname == 0)
			break;

		seq_keysym = XStringToKeysym(keyname);
		if (seq_keysym == NoSymbol)
		{
			DEBUG_KBD(("Bad keysym \"%s\" in keymap %s (ignoring line)\n", keyname,
				   mapname));
			delete_key_translation_entry(keysym);
			return;
		}

		/* Allocate space for key_translation structure */
		tr = (key_translation *) xmalloc(sizeof(key_translation));
		memset(tr, 0, sizeof(key_translation));

		/* Do this straight away so the key_translation won't get orphaned on error */
		if (!entry->tr)
			entry->tr = tr;

		*prev_next = tr;
		prev_next = &tr->next;
		tr->seq_keysym = seq_keysym;

		DEBUG_KBD(("0x%x, ", (unsigned int) seq_keysym));
	}
	DEBUG_KBD(("\n"));
}

RD_BOOL
xkeymap_from_locale(const char *locale)
{
	char *str, *ptr;
	FILE *fp;

	/* Create a working copy */
	str = xstrdup(locale);

	/* Truncate at dot and at */
	ptr = strrchr(str, '.');
	if (ptr)
		*ptr = '\0';
	ptr = strrchr(str, '@');
	if (ptr)
		*ptr = '\0';

	/* Replace _ with - */
	ptr = strrchr(str, '_');
	if (ptr)
		*ptr = '-';

	/* Convert to lowercase */
	ptr = str;
	while (*ptr)
	{
		*ptr = tolower((int) *ptr);
		ptr++;
	}

	/* Try to open this keymap (da-dk) */
	fp = xkeymap_open(str);
	if (fp == NULL)
	{
		/* Truncate at dash */
		ptr = strrchr(str, '-');
		if (ptr)
			*ptr = '\0';

		/* Try the short name (da) */
		fp = xkeymap_open(str);
	}

	if (fp)
	{
		fclose(fp);
		STRNCPY(g_keymapname, str, sizeof(g_keymapname));
		xfree(str);
		return True;
	}

	xfree(str);
	return False;
}


/* Joins two path components. The result should be freed with
   xfree(). */
static char *
pathjoin(const char *a, const char *b)
{
	char *result;
	result = xmalloc(PATH_MAX * 2 + 1);

	if (b[0] == '/')
	{
		strncpy(result, b, PATH_MAX);
	}
	else
	{
		strncpy(result, a, PATH_MAX);
		strcat(result, "/");
		strncat(result, b, PATH_MAX);
	}
	return result;
}

/* Try to open a keymap with fopen() */
FILE *
xkeymap_open(const char *filename)
{
	char *path1, *path2;
	char *home;
	FILE *fp;

	/* Try ~/.rdesktop/keymaps */
	home = getenv("HOME");
	if (home)
	{
		path1 = pathjoin(home, ".rdesktop/keymaps");
		path2 = pathjoin(path1, filename);
		xfree(path1);
		fp = fopen(path2, "r");
		xfree(path2);
		if (fp)
			return fp;
	}

	/* Try KEYMAP_PATH */
	path1 = pathjoin(KEYMAP_PATH, filename);
	fp = fopen(path1, "r");
	xfree(path1);
	if (fp)
		return fp;

	/* Try current directory, in case we are running from the source
	   tree */
	path1 = pathjoin("keymaps", filename);
	fp = fopen(path1, "r");
	xfree(path1);
	if (fp)
		return fp;

	return NULL;
}

static RD_BOOL
xkeymap_read(char *mapname)
{
	FILE *fp;
	char line[KEYMAP_MAX_LINE_LENGTH];
	unsigned int line_num = 0;
	unsigned int line_length = 0;
	char *keyname, *p;
	char *line_rest;
	uint8 scancode;
	uint16 modifiers;

	fp = xkeymap_open(mapname);
	if (fp == NULL)
	{
		error("Failed to open keymap %s\n", mapname);
		return False;
	}

	/* FIXME: More tolerant on white space */
	while (fgets(line, sizeof(line), fp) != NULL)
	{
		line_num++;

		/* Replace the \n with \0 */
		p = strchr(line, '\n');
		if (p != NULL)
			*p = 0;

		line_length = strlen(line);

		/* Completely empty line */
		if (strspn(line, " \t\n\r\f\v") == line_length)
		{
			continue;
		}

		/* Include */
		if (str_startswith(line, "include "))
		{
			if (!xkeymap_read(line + sizeof("include ") - 1))
				return False;
			continue;
		}

		/* map */
		if (str_startswith(line, "map "))
		{
			g_keylayout = strtoul(line + sizeof("map ") - 1, NULL, 16);
			DEBUG_KBD(("Keylayout 0x%x\n", g_keylayout));
			continue;
		}

		/* compose */
		if (str_startswith(line, "enable_compose"))
		{
			DEBUG_KBD(("Enabling compose handling\n"));
			g_enable_compose = True;
			continue;
		}

		/* sequence */
		if (str_startswith(line, "sequence"))
		{
			add_sequence(line + sizeof("sequence") - 1, mapname);
			continue;
		}

		/* keyboard_type */
		if (str_startswith(line, "keyboard_type "))
		{
			g_keyboard_type = strtol(line + sizeof("keyboard_type ") - 1, NULL, 16);
			DEBUG_KBD(("keyboard_type 0x%x\n", g_keyboard_type));
			continue;
		}

		/* keyboard_subtype */
		if (str_startswith(line, "keyboard_subtype "))
		{
			g_keyboard_subtype =
				strtol(line + sizeof("keyboard_subtype ") - 1, NULL, 16);
			DEBUG_KBD(("keyboard_subtype 0x%x\n", g_keyboard_subtype));
			continue;
		}

		/* keyboard_functionkeys */
		if (str_startswith(line, "keyboard_functionkeys "))
		{
			g_keyboard_functionkeys =
				strtol(line + sizeof("keyboard_functionkeys ") - 1, NULL, 16);
			DEBUG_KBD(("keyboard_functionkeys 0x%x\n", g_keyboard_functionkeys));
			continue;
		}

		/* Comment */
		if (line[0] == '#')
		{
			continue;
		}

		/* Normal line */
		keyname = line;
		p = strchr(line, ' ');
		if (p == NULL)
		{
			error("Bad line %d in keymap %s\n", line_num, mapname);
			continue;
		}
		else
		{
			*p = 0;
		}

		/* scancode */
		p++;
		scancode = strtol(p, &line_rest, 16);

		/* flags */
		/* FIXME: Should allow case-insensitive flag names. 
		   Fix by using lex+yacc... */
		modifiers = 0;
		if (strstr(line_rest, "altgr"))
		{
			MASK_ADD_BITS(modifiers, MapAltGrMask);
		}

		if (strstr(line_rest, "shift"))
		{
			MASK_ADD_BITS(modifiers, MapLeftShiftMask);
		}

		if (strstr(line_rest, "numlock"))
		{
			MASK_ADD_BITS(modifiers, MapNumLockMask);
		}

		if (strstr(line_rest, "localstate"))
		{
			MASK_ADD_BITS(modifiers, MapLocalStateMask);
		}

		if (strstr(line_rest, "inhibit"))
		{
			MASK_ADD_BITS(modifiers, MapInhibitMask);
		}

		add_to_keymap(keyname, scancode, modifiers, mapname);

		if (strstr(line_rest, "addupper"))
		{
			/* Automatically add uppercase key, with same modifiers 
			   plus shift */
			for (p = keyname; *p; p++)
				*p = toupper((int) *p);
			MASK_ADD_BITS(modifiers, MapLeftShiftMask);
			add_to_keymap(keyname, scancode, modifiers, mapname);
		}
	}

	fclose(fp);
	return True;
}


/* Before connecting and creating UI */
void
xkeymap_init(void)
{
	unsigned int max_keycode;

	if (strcmp(g_keymapname, "none"))
	{
		if (xkeymap_read(g_keymapname))
			keymap_loaded = True;
	}

	XDisplayKeycodes(g_display, &min_keycode, (int *) &max_keycode);
}

static void
send_winkey(uint32 ev_time, RD_BOOL pressed, RD_BOOL leftkey)
{
	uint8 winkey;

	if (leftkey)
		winkey = SCANCODE_CHAR_LWIN;
	else
		winkey = SCANCODE_CHAR_RWIN;

	if (pressed)
	{
		if (g_rdp_version >= RDP_V5)
		{
			rdp_send_scancode(ev_time, RDP_KEYPRESS, winkey);
		}
		else
		{
			/* RDP4 doesn't support winkey. Fake with Ctrl-Esc */
			rdp_send_scancode(ev_time, RDP_KEYPRESS, SCANCODE_CHAR_LCTRL);
			rdp_send_scancode(ev_time, RDP_KEYPRESS, SCANCODE_CHAR_ESC);
		}
	}
	else
	{
		/* key released */
		if (g_rdp_version >= RDP_V5)
		{
			rdp_send_scancode(ev_time, RDP_KEYRELEASE, winkey);
		}
		else
		{
			rdp_send_scancode(ev_time, RDP_KEYRELEASE, SCANCODE_CHAR_ESC);
			rdp_send_scancode(ev_time, RDP_KEYRELEASE, SCANCODE_CHAR_LCTRL);
		}
	}
}

static void
reset_winkey(uint32 ev_time)
{
	if (g_rdp_version >= RDP_V5)
	{
		/* For some reason, it seems to suffice to release
		 *either* the left or right winkey. */
		rdp_send_scancode(ev_time, RDP_KEYRELEASE, SCANCODE_CHAR_LWIN);
	}
}


void
set_keypress_keysym(unsigned int keycode, KeySym keysym)
{
	if (keycode < 8 || keycode > 255)
		return;
	keypress_keysyms[keycode] = keysym;
}


KeySym
reset_keypress_keysym(unsigned int keycode, KeySym keysym)
{
	KeySym ks;
	if (keycode < 8 || keycode > 255)
		return keysym;
	ks = keypress_keysyms[keycode];
	if (ks != 0)
	{
		keypress_keysyms[keycode] = 0;
	}
	else
	{
		ks = keysym;
	}

	return ks;
}


/* Handle special key combinations */
RD_BOOL
handle_special_keys(uint32 keysym, unsigned int state, uint32 ev_time, RD_BOOL pressed)
{
	switch (keysym)
	{
		case XK_Return:
			if ((get_key_state(state, XK_Alt_L) || get_key_state(state, XK_Alt_R))
			    && (get_key_state(state, XK_Control_L)
				|| get_key_state(state, XK_Control_R)))
			{
				/* Ctrl-Alt-Enter: toggle full screen */
				if (pressed)
					xwin_toggle_fullscreen();
				return True;
			}
			break;

		case XK_Break:
#ifdef RDESKTOP_KBD_CODE
			/* Send Break sequence E0 46 E0 C6 */
			if (pressed)
			{
				rdp_send_scancode(ev_time, RDP_KEYPRESS,
						  (SCANCODE_EXTENDED | 0x46));
				rdp_send_scancode(ev_time, RDP_KEYPRESS,
						  (SCANCODE_EXTENDED | 0xc6));
			}
			/* No release sequence */
			return True;
#else
			/* Send Break sequence E0 46 E0 C6 */
			if (pressed)
			{
				/* VirtualBox code begin */
				rdp_send_input(ev_time, RDP_INPUT_SCANCODE, KBD_FLAG_EXT, 0x46, 0);
				rdp_send_input(ev_time, RDP_INPUT_SCANCODE, KBD_FLAG_EXT | KBD_FLAG_UP, 0x46, 0);
				/* VirtualBox code end */
			}
			/* No release sequence */
			return True;
#endif /* RDESKTOP_KBD_CODE */
			break;

		case XK_Pause:
#ifdef RDESKTOP_KBD_CODE
			/* According to MS Keyboard Scan Code
			   Specification, pressing Pause should result
			   in E1 1D 45 E1 9D C5. I'm not exactly sure
			   of how this is supposed to be sent via
			   RDP. The code below seems to work, but with
			   the side effect that Left Ctrl stays
			   down. Therefore, we release it when Pause
			   is released. */
			if (pressed)
			{
				rdp_send_input(ev_time, RDP_INPUT_SCANCODE, RDP_KEYPRESS, 0xe1, 0);
				rdp_send_input(ev_time, RDP_INPUT_SCANCODE, RDP_KEYPRESS, 0x1d, 0);
				rdp_send_input(ev_time, RDP_INPUT_SCANCODE, RDP_KEYPRESS, 0x45, 0);
				rdp_send_input(ev_time, RDP_INPUT_SCANCODE, RDP_KEYPRESS, 0xe1, 0);
				rdp_send_input(ev_time, RDP_INPUT_SCANCODE, RDP_KEYPRESS, 0x9d, 0);
				rdp_send_input(ev_time, RDP_INPUT_SCANCODE, RDP_KEYPRESS, 0xc5, 0);
			}
			else
			{
				/* Release Left Ctrl */
				rdp_send_input(ev_time, RDP_INPUT_SCANCODE, RDP_KEYRELEASE,
					       0x1d, 0);
			}
			return True;
#else
			/* Send Break sequence E1 1D 45 E1 9D C5 */
			if (pressed)
			{
				/* VirtualBox code begin */
				rdp_send_input(ev_time, RDP_INPUT_SCANCODE, KBD_FLAG_EXT2, 0x1d, 0);
				rdp_send_input(ev_time, RDP_INPUT_SCANCODE, RDP_KEYPRESS,  0x45, 0);
				rdp_send_input(ev_time, RDP_INPUT_SCANCODE, KBD_FLAG_EXT2 | KBD_FLAG_UP, 0x1d, 0);
				rdp_send_input(ev_time, RDP_INPUT_SCANCODE, KBD_FLAG_UP,  0x45, 0);
				/* VirtualBox code end */
			}
			/* No release sequence */
			return True;
#endif /* RDESKTOP_KBD_CODE */
			break;

		case XK_Meta_L:	/* Windows keys */
		case XK_Super_L:
		case XK_Hyper_L:
			send_winkey(ev_time, pressed, True);
			return True;
			break;

		case XK_Meta_R:
		case XK_Super_R:
		case XK_Hyper_R:
			send_winkey(ev_time, pressed, False);
			return True;
			break;

		case XK_space:
			/* Prevent access to the Windows system menu in single app mode */
			if (g_win_button_size
			    && (get_key_state(state, XK_Alt_L) || get_key_state(state, XK_Alt_R)))
				return True;
			break;

		case XK_Num_Lock:
			/* Synchronize on key release */
			if (g_numlock_sync && !pressed)
				rdp_send_input(0, RDP_INPUT_SYNCHRONIZE, 0,
					       ui_get_numlock_state(read_keyboard_state()), 0);

			/* Inhibit */
			return True;
			break;
		case XK_Overlay1_Enable:
			/* Toggle SeamlessRDP */
			if (pressed)
				ui_seamless_toggle();
			break;
#ifdef WITH_BIRD_VD_HACKS
		case XK_Left:
		case XK_Right:
		{
			/* Check for typical virtual desktop switching hotkeys:
			       Ctrl-Alt-Left and Ctrl-Alt-Right.
			   Needs to be pressed twice to have any effect... */
			extern RD_BOOL g_keep_virtual_desktop_shortcuts;
			extern RD_BOOL g_fullscreen;
			if (   g_keep_virtual_desktop_shortcuts
			    && (   (   get_key_state(state, XK_Alt_L)
			            || get_key_state(state, XK_Alt_R))
			        && (   get_key_state(state, XK_Control_L)
			            || get_key_state(state, XK_Control_R))
			       )
			   )
			{
				if (g_fullscreen)
					xwin_toggle_fullscreen();
				XUngrabKeyboard(g_display, CurrentTime);
				return True;
			}
			break;
		}
#endif
	}
	return False;
}


key_translation
xkeymap_translate_key(uint32 keysym, unsigned int keycode, unsigned int state)
{
	key_translation tr = { 0, 0, 0, 0 };
	key_translation *ptr;
	key_translation_entry *entry;

	entry = get_key_translation_entry(keysym);
	ptr = entry ? entry->tr : NULL;

	if (ptr)
	{
		tr = *ptr;
		if (tr.seq_keysym == 0)	/* Normal scancode translation */
		{
			if (MASK_HAS_BITS(tr.modifiers, MapInhibitMask))
			{
				DEBUG_KBD(("Inhibiting key\n"));
				tr.scancode = 0;
				return tr;
			}

			if (MASK_HAS_BITS(tr.modifiers, MapLocalStateMask))
			{
				/* The modifiers to send for this key should be obtained
				   from the local state. Currently, only shift is implemented. */
				if (MASK_HAS_BITS(state, ShiftMask))
				{
					tr.modifiers = MapLeftShiftMask;
				}
			}

			/* Windows interprets CapsLock+Ctrl+key
			   differently from Shift+Ctrl+key. Since we
			   are simulating CapsLock with Shifts, things
			   like Ctrl+f with CapsLock on breaks. To
			   solve this, we are releasing Shift if Ctrl
			   is on, but only if Shift isn't physically pressed. */
			if (MASK_HAS_BITS(tr.modifiers, MapShiftMask)
			    && MASK_HAS_BITS(remote_modifier_state, MapCtrlMask)
			    && !MASK_HAS_BITS(state, ShiftMask))
			{
				DEBUG_KBD(("Non-physical Shift + Ctrl pressed, releasing Shift\n"));
				MASK_REMOVE_BITS(tr.modifiers, MapShiftMask);
			}

			DEBUG_KBD(("Found scancode translation, scancode=0x%x, modifiers=0x%x\n",
				   tr.scancode, tr.modifiers));
		}
	}
	else
	{
		if (keymap_loaded)
			warning("No translation for (keysym 0x%lx, %s)\n", keysym,
				get_ksname(keysym));

		/* not in keymap, try to interpret the raw scancode */
		if (((int) keycode >= min_keycode) && (keycode <= 0x60))
		{
			tr.scancode = keycode - min_keycode;

			/* The modifiers to send for this key should be
			   obtained from the local state. Currently, only
			   shift is implemented. */
			if (MASK_HAS_BITS(state, ShiftMask))
			{
				tr.modifiers = MapLeftShiftMask;
			}

			DEBUG_KBD(("Sending guessed scancode 0x%x\n", tr.scancode));
		}
		else
		{
			DEBUG_KBD(("No good guess for keycode 0x%x found\n", keycode));
		}
	}

	return tr;
}

static RD_BOOL
is_modifier(uint8 scancode)
{
	switch (scancode)
	{
		case SCANCODE_CHAR_LSHIFT:
		case SCANCODE_CHAR_RSHIFT:
		case SCANCODE_CHAR_LCTRL:
		case SCANCODE_CHAR_RCTRL:
		case SCANCODE_CHAR_LALT:
		case SCANCODE_CHAR_RALT:
		case SCANCODE_CHAR_LWIN:
		case SCANCODE_CHAR_RWIN:
		case SCANCODE_CHAR_NUMLOCK:
			return True;
		default:
			break;
	}
	return False;
}


void
xkeymap_send_keys(uint32 keysym, unsigned int keycode, unsigned int state, uint32 ev_time,
		  RD_BOOL pressed, uint8 nesting)
{
	key_translation tr, *ptr;
	tr = xkeymap_translate_key(keysym, keycode, state);

	if (tr.seq_keysym == 0)
	{
		/* Scancode translation */
		if (tr.scancode == 0)
			return;

		save_remote_modifiers(tr.scancode);
		ensure_remote_modifiers(ev_time, tr);
		rdp_send_scancode(ev_time, pressed ? RDP_KEYPRESS : RDP_KEYRELEASE, tr.scancode);
		restore_remote_modifiers(ev_time, tr.scancode);
		return;
	}

	/* Sequence, only on key down */
	if (pressed)
	{
		ptr = &tr;
		do
		{
			DEBUG_KBD(("Handling sequence element, keysym=0x%x\n",
				   (unsigned int) ptr->seq_keysym));

			if (nesting++ > 32)
			{
				error("Sequence nesting too deep\n");
				return;
			}

			xkeymap_send_keys(ptr->seq_keysym, keycode, state, ev_time, True, nesting);
			xkeymap_send_keys(ptr->seq_keysym, keycode, state, ev_time, False, nesting);
			ptr = ptr->next;
		}
		while (ptr);
	}
}

uint16
xkeymap_translate_button(unsigned int button)
{
	switch (button)
	{
		case Button1:	/* left */
			return MOUSE_FLAG_BUTTON1;
		case Button2:	/* middle */
			return MOUSE_FLAG_BUTTON3;
		case Button3:	/* right */
			return MOUSE_FLAG_BUTTON2;
		case Button4:	/* wheel up */
			return MOUSE_FLAG_BUTTON4;
		case Button5:	/* wheel down */
			return MOUSE_FLAG_BUTTON5;
	}

	return 0;
}

char *
get_ksname(uint32 keysym)
{
	char *ksname = NULL;

	if (keysym == NoSymbol)
		ksname = "NoSymbol";
	else if (!(ksname = XKeysymToString(keysym)))
		ksname = "(no name)";

	return ksname;
}

void
save_remote_modifiers(uint8 scancode)
{
	if (is_modifier(scancode))
		return;

	saved_remote_modifier_state = remote_modifier_state;
}

void
restore_remote_modifiers(uint32 ev_time, uint8 scancode)
{
	key_translation dummy = { };

	if (is_modifier(scancode))
		return;

	dummy.scancode = 0;
	dummy.modifiers = saved_remote_modifier_state;
	ensure_remote_modifiers(ev_time, dummy);
}

void
ensure_remote_modifiers(uint32 ev_time, key_translation tr)
{
	/* If this key is a modifier, do nothing */
	if (is_modifier(tr.scancode))
		return;

	if (!g_numlock_sync)
	{
		/* NumLock */
		if (MASK_HAS_BITS(tr.modifiers, MapNumLockMask)
		    != MASK_HAS_BITS(remote_modifier_state, MapNumLockMask))
		{
			/* The remote modifier state is not correct */
			uint16 new_remote_state;

			if (MASK_HAS_BITS(tr.modifiers, MapNumLockMask))
			{
				DEBUG_KBD(("Remote NumLock state is incorrect, activating NumLock.\n"));
				new_remote_state = KBD_FLAG_NUMLOCK;
				remote_modifier_state = MapNumLockMask;
			}
			else
			{
				DEBUG_KBD(("Remote NumLock state is incorrect, deactivating NumLock.\n"));
				new_remote_state = 0;
				remote_modifier_state = 0;
			}

			rdp_send_input(0, RDP_INPUT_SYNCHRONIZE, 0, new_remote_state, 0);
		}
	}


	/* Shift. Left shift and right shift are treated as equal; either is fine. */
	if (MASK_HAS_BITS(tr.modifiers, MapShiftMask)
	    != MASK_HAS_BITS(remote_modifier_state, MapShiftMask))
	{
		/* The remote modifier state is not correct */
		if (MASK_HAS_BITS(tr.modifiers, MapLeftShiftMask))
		{
			/* Needs left shift. Send down. */
			rdp_send_scancode(ev_time, RDP_KEYPRESS, SCANCODE_CHAR_LSHIFT);
		}
		else if (MASK_HAS_BITS(tr.modifiers, MapRightShiftMask))
		{
			/* Needs right shift. Send down. */
			rdp_send_scancode(ev_time, RDP_KEYPRESS, SCANCODE_CHAR_RSHIFT);
		}
		else
		{
			/* Should not use this modifier. Send up for shift currently pressed. */
			if (MASK_HAS_BITS(remote_modifier_state, MapLeftShiftMask))
				/* Left shift is down */
				rdp_send_scancode(ev_time, RDP_KEYRELEASE, SCANCODE_CHAR_LSHIFT);
			else
				/* Right shift is down */
				rdp_send_scancode(ev_time, RDP_KEYRELEASE, SCANCODE_CHAR_RSHIFT);
		}
	}

	/* AltGr */
	if (MASK_HAS_BITS(tr.modifiers, MapAltGrMask)
	    != MASK_HAS_BITS(remote_modifier_state, MapAltGrMask))
	{
		/* The remote modifier state is not correct */
		if (MASK_HAS_BITS(tr.modifiers, MapAltGrMask))
		{
			/* Needs this modifier. Send down. */
			rdp_send_scancode(ev_time, RDP_KEYPRESS, SCANCODE_CHAR_RALT);
		}
		else
		{
			/* Should not use this modifier. Send up. */
			rdp_send_scancode(ev_time, RDP_KEYRELEASE, SCANCODE_CHAR_RALT);
		}
	}


}


unsigned int
read_keyboard_state()
{
#ifdef RDP2VNC
	return 0;
#else
	unsigned int state;
	Window wdummy;
	int dummy;

	XQueryPointer(g_display, g_wnd, &wdummy, &wdummy, &dummy, &dummy, &dummy, &dummy, &state);
	return state;
#endif
}


uint16
ui_get_numlock_state(unsigned int state)
{
	uint16 numlock_state = 0;

	if (get_key_state(state, XK_Num_Lock))
		numlock_state = KBD_FLAG_NUMLOCK;

	return numlock_state;
}


void
reset_modifier_keys()
{
	unsigned int state = read_keyboard_state();

	/* reset keys */
	uint32 ev_time;
	ev_time = time(NULL);

	if (MASK_HAS_BITS(remote_modifier_state, MapLeftShiftMask)
	    && !get_key_state(state, XK_Shift_L))
		rdp_send_scancode(ev_time, RDP_KEYRELEASE, SCANCODE_CHAR_LSHIFT);

	if (MASK_HAS_BITS(remote_modifier_state, MapRightShiftMask)
	    && !get_key_state(state, XK_Shift_R))
		rdp_send_scancode(ev_time, RDP_KEYRELEASE, SCANCODE_CHAR_RSHIFT);

	if (MASK_HAS_BITS(remote_modifier_state, MapLeftCtrlMask)
	    && !get_key_state(state, XK_Control_L))
		rdp_send_scancode(ev_time, RDP_KEYRELEASE, SCANCODE_CHAR_LCTRL);

	if (MASK_HAS_BITS(remote_modifier_state, MapRightCtrlMask)
	    && !get_key_state(state, XK_Control_R))
		rdp_send_scancode(ev_time, RDP_KEYRELEASE, SCANCODE_CHAR_RCTRL);

	if (MASK_HAS_BITS(remote_modifier_state, MapLeftAltMask) && !get_key_state(state, XK_Alt_L))
		rdp_send_scancode(ev_time, RDP_KEYRELEASE, SCANCODE_CHAR_LALT);

	if (MASK_HAS_BITS(remote_modifier_state, MapRightAltMask) &&
	    !get_key_state(state, XK_Alt_R) && !get_key_state(state, XK_Mode_switch)
	    && !get_key_state(state, XK_ISO_Level3_Shift))
		rdp_send_scancode(ev_time, RDP_KEYRELEASE, SCANCODE_CHAR_RALT);

	reset_winkey(ev_time);

	if (g_numlock_sync)
		rdp_send_input(ev_time, RDP_INPUT_SYNCHRONIZE, 0, ui_get_numlock_state(state), 0);
}


static void
update_modifier_state(uint8 scancode, RD_BOOL pressed)
{
#ifdef WITH_DEBUG_KBD
	uint16 old_modifier_state;

	old_modifier_state = remote_modifier_state;
#endif

	switch (scancode)
	{
		case SCANCODE_CHAR_LSHIFT:
			MASK_CHANGE_BIT(remote_modifier_state, MapLeftShiftMask, pressed);
			break;
		case SCANCODE_CHAR_RSHIFT:
			MASK_CHANGE_BIT(remote_modifier_state, MapRightShiftMask, pressed);
			break;
		case SCANCODE_CHAR_LCTRL:
			MASK_CHANGE_BIT(remote_modifier_state, MapLeftCtrlMask, pressed);
			break;
		case SCANCODE_CHAR_RCTRL:
			MASK_CHANGE_BIT(remote_modifier_state, MapRightCtrlMask, pressed);
			break;
		case SCANCODE_CHAR_LALT:
			MASK_CHANGE_BIT(remote_modifier_state, MapLeftAltMask, pressed);
			break;
		case SCANCODE_CHAR_RALT:
			MASK_CHANGE_BIT(remote_modifier_state, MapRightAltMask, pressed);
			break;
		case SCANCODE_CHAR_LWIN:
			MASK_CHANGE_BIT(remote_modifier_state, MapLeftWinMask, pressed);
			break;
		case SCANCODE_CHAR_RWIN:
			MASK_CHANGE_BIT(remote_modifier_state, MapRightWinMask, pressed);
			break;
		case SCANCODE_CHAR_NUMLOCK:
			/* KeyReleases for NumLocks are sent immediately. Toggle the
			   modifier state only on Keypress */
			if (pressed && !g_numlock_sync)
			{
				RD_BOOL newNumLockState;
				newNumLockState =
					(MASK_HAS_BITS
					 (remote_modifier_state, MapNumLockMask) == False);
				MASK_CHANGE_BIT(remote_modifier_state,
						MapNumLockMask, newNumLockState);
			}
	}

#ifdef WITH_DEBUG_KBD
	if (old_modifier_state != remote_modifier_state)
	{
		DEBUG_KBD(("Before updating modifier_state:0x%x, pressed=0x%x\n",
			   old_modifier_state, pressed));
		DEBUG_KBD(("After updating modifier_state:0x%x\n", remote_modifier_state));
	}
#endif

}

/* Send keyboard input */
void
rdp_send_scancode(uint32 time, uint16 flags, uint8 scancode)
{
	update_modifier_state(scancode, !(flags & RDP_KEYRELEASE));

	if (scancode & SCANCODE_EXTENDED)
	{
		DEBUG_KBD(("Sending extended scancode=0x%x, flags=0x%x\n",
			   scancode & ~SCANCODE_EXTENDED, flags));
		rdp_send_input(time, RDP_INPUT_SCANCODE, flags | KBD_FLAG_EXT,
			       scancode & ~SCANCODE_EXTENDED, 0);
	}
	else
	{
		DEBUG_KBD(("Sending scancode=0x%x, flags=0x%x\n", scancode, flags));
		rdp_send_input(time, RDP_INPUT_SCANCODE, flags, scancode, 0);
	}
}
