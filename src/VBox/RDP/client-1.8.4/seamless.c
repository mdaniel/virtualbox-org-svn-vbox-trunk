/* -*- c-basic-offset: 8 -*-
   rdesktop: A Remote Desktop Protocol client.
   Seamless Windows support
   Copyright 2005-2008 Peter Astrand <astrand@cendio.se> for Cendio AB
   Copyright 2007-2008 Pierre Ossman <ossman@cendio.se> for Cendio AB
   Copyright 2013-2014 Henrik Andersson  <hean01@cendio.se> for Cendio AB   

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

#include "rdesktop.h"
#include <stdarg.h>
#include <assert.h>

#ifdef WITH_DEBUG_SEAMLESS
#define DEBUG_SEAMLESS(args) printf args;
#else
#define DEBUG_SEAMLESS(args)
#endif

extern RD_BOOL g_seamless_rdp;
static VCHANNEL *seamless_channel;
static unsigned int seamless_serial;
static char *seamless_rest = NULL;
static char icon_buf[1024];

static char *
seamless_get_token(char **s)
{
	char *comma, *head;
	head = *s;

	if (!head)
		return NULL;

	comma = strchr(head, ',');
	if (comma)
	{
		*comma = '\0';
		*s = comma + 1;
	}
	else
	{
		*s = NULL;
	}

	return head;
}


static RD_BOOL
seamless_process_line(const char *line, void *data)
{
	char *p, *l;
	char *tok1, *tok2, *tok3, *tok4, *tok5, *tok6, *tok7, *tok8;
	unsigned long id, flags;
	char *endptr;

	l = xstrdup(line);
	p = l;

	DEBUG_SEAMLESS(("seamlessrdp got:%s\n", p));

	tok1 = seamless_get_token(&p);
	tok2 = seamless_get_token(&p);
	tok3 = seamless_get_token(&p);
	tok4 = seamless_get_token(&p);
	tok5 = seamless_get_token(&p);
	tok6 = seamless_get_token(&p);
	tok7 = seamless_get_token(&p);
	tok8 = seamless_get_token(&p);

	if (!strcmp("CREATE", tok1))
	{
		unsigned long group, parent;
		if (!tok6)
			return False;

		id = strtoul(tok3, &endptr, 0);
		if (*endptr)
			return False;

		group = strtoul(tok4, &endptr, 0);
		if (*endptr)
			return False;

		parent = strtoul(tok5, &endptr, 0);
		if (*endptr)
			return False;

		flags = strtoul(tok6, &endptr, 0);
		if (*endptr)
			return False;

		ui_seamless_create_window(id, group, parent, flags);
	}
	else if (!strcmp("DESTROY", tok1))
	{
		if (!tok4)
			return False;

		id = strtoul(tok3, &endptr, 0);
		if (*endptr)
			return False;

		flags = strtoul(tok4, &endptr, 0);
		if (*endptr)
			return False;

		ui_seamless_destroy_window(id, flags);

	}
	else if (!strcmp("DESTROYGRP", tok1))
	{
		if (!tok4)
			return False;

		id = strtoul(tok3, &endptr, 0);
		if (*endptr)
			return False;

		flags = strtoul(tok4, &endptr, 0);
		if (*endptr)
			return False;

		ui_seamless_destroy_group(id, flags);
	}
	else if (!strcmp("SETICON", tok1))
	{
		int chunk, width, height, len;
		char byte[3];

		if (!tok8)
			return False;

		id = strtoul(tok3, &endptr, 0);
		if (*endptr)
			return False;

		chunk = strtoul(tok4, &endptr, 0);
		if (*endptr)
			return False;

		width = strtoul(tok6, &endptr, 0);
		if (*endptr)
			return False;

		height = strtoul(tok7, &endptr, 0);
		if (*endptr)
			return False;

		byte[2] = '\0';
		len = 0;
		while (*tok8 != '\0')
		{
			byte[0] = *tok8;
			tok8++;
			if (*tok8 == '\0')
				return False;
			byte[1] = *tok8;
			tok8++;

			icon_buf[len] = strtol(byte, NULL, 16);
			len++;

			if ((size_t)len >= sizeof(icon_buf))
			{
				warning("seamless_process_line(), icon data would overrun icon_buf");
				break;
			}
		}

		ui_seamless_seticon(id, tok5, width, height, chunk, icon_buf, len);
	}
	else if (!strcmp("DELICON", tok1))
	{
		int width, height;

		if (!tok6)
			return False;

		id = strtoul(tok3, &endptr, 0);
		if (*endptr)
			return False;

		width = strtoul(tok5, &endptr, 0);
		if (*endptr)
			return False;

		height = strtoul(tok6, &endptr, 0);
		if (*endptr)
			return False;

		ui_seamless_delicon(id, tok4, width, height);
	}
	else if (!strcmp("POSITION", tok1))
	{
		int x, y, width, height;

		if (!tok8)
			return False;

		id = strtoul(tok3, &endptr, 0);
		if (*endptr)
			return False;

		x = strtol(tok4, &endptr, 0);
		if (*endptr)
			return False;
		y = strtol(tok5, &endptr, 0);
		if (*endptr)
			return False;

		width = strtol(tok6, &endptr, 0);
		if (*endptr)
			return False;
		height = strtol(tok7, &endptr, 0);
		if (*endptr)
			return False;

		flags = strtoul(tok8, &endptr, 0);
		if (*endptr)
			return False;

		ui_seamless_move_window(id, x, y, width, height, flags);
	}
	else if (!strcmp("ZCHANGE", tok1))
	{
		unsigned long behind;

		id = strtoul(tok3, &endptr, 0);
		if (*endptr)
			return False;

		behind = strtoul(tok4, &endptr, 0);
		if (*endptr)
			return False;

		flags = strtoul(tok5, &endptr, 0);
		if (*endptr)
			return False;

		ui_seamless_restack_window(id, behind, flags);
	}
	else if (!strcmp("TITLE", tok1))
	{
		if (!tok5)
			return False;

		id = strtoul(tok3, &endptr, 0);
		if (*endptr)
			return False;

		flags = strtoul(tok5, &endptr, 0);
		if (*endptr)
			return False;

		ui_seamless_settitle(id, tok4, flags);
	}
	else if (!strcmp("STATE", tok1))
	{
		unsigned int state;

		if (!tok5)
			return False;

		id = strtoul(tok3, &endptr, 0);
		if (*endptr)
			return False;

		state = strtoul(tok4, &endptr, 0);
		if (*endptr)
			return False;

		flags = strtoul(tok5, &endptr, 0);
		if (*endptr)
			return False;

		ui_seamless_setstate(id, state, flags);
	}
	else if (!strcmp("DEBUG", tok1))
	{
		DEBUG_SEAMLESS(("SeamlessRDP:%s\n", line));
	}
	else if (!strcmp("SYNCBEGIN", tok1))
	{
		if (!tok3)
			return False;

		flags = strtoul(tok3, &endptr, 0);
		if (*endptr)
			return False;

		ui_seamless_syncbegin(flags);
	}
	else if (!strcmp("SYNCEND", tok1))
	{
		if (!tok3)
			return False;

		flags = strtoul(tok3, &endptr, 0);
		if (*endptr)
			return False;

		/* do nothing, currently */
	}
	else if (!strcmp("HELLO", tok1))
	{
		if (!tok3)
			return False;

		flags = strtoul(tok3, &endptr, 0);
		if (*endptr)
			return False;

		ui_seamless_begin(! !(flags & SEAMLESSRDP_HELLO_HIDDEN));
	}
	else if (!strcmp("ACK", tok1))
	{
		unsigned int serial;

		serial = strtoul(tok3, &endptr, 0);
		if (*endptr)
			return False;

		ui_seamless_ack(serial);
	}
	else if (!strcmp("HIDE", tok1))
	{
		if (!tok3)
			return False;

		flags = strtoul(tok3, &endptr, 0);
		if (*endptr)
			return False;

		ui_seamless_hide_desktop();
	}
	else if (!strcmp("UNHIDE", tok1))
	{
		if (!tok3)
			return False;

		flags = strtoul(tok3, &endptr, 0);
		if (*endptr)
			return False;

		ui_seamless_unhide_desktop();
	}


	xfree(l);
	return True;
}


static RD_BOOL
seamless_line_handler(const char *line, void *data)
{
	if (!seamless_process_line(line, data))
	{
		warning("SeamlessRDP: Invalid request:%s\n", line);
	}
	return True;
}


static void
seamless_process(STREAM s)
{
	unsigned int pkglen;
	char *buf;
	struct stream packet = *s;

	if (!s_check(s))
	{
		rdp_protocol_error("seamless_process(), stream is in unstable state", &packet);
	}

	pkglen = s->end - s->p;
	/* str_handle_lines requires null terminated strings */
	buf = xmalloc(pkglen + 1);
	STRNCPY(buf, (char *) s->p, pkglen + 1);
#if 0
	printf("seamless recv:\n");
	hexdump(s->p, pkglen);
#endif

	str_handle_lines(buf, &seamless_rest, seamless_line_handler, NULL);

	xfree(buf);
}


RD_BOOL
seamless_init(void)
{
	if (!g_seamless_rdp)
		return False;

	seamless_serial = 0;

	seamless_channel =
		channel_register("seamrdp", CHANNEL_OPTION_INITIALIZED | CHANNEL_OPTION_ENCRYPT_RDP,
				 seamless_process);
	return (seamless_channel != NULL);
}

void
seamless_reset_state(void)
{
	if (seamless_rest != NULL)
	{
		xfree(seamless_rest);
		seamless_rest = NULL;
	}
}

static unsigned int
seamless_send(const char *command, const char *format, ...)
{
	STREAM s;
	size_t len;
	va_list argp;
	char *escaped, buf[1025];

	len = snprintf(buf, sizeof(buf) - 1, "%s,%u,", command, seamless_serial);

	assert(len < (sizeof(buf) - 1));

	va_start(argp, format);
	len += vsnprintf(buf + len, sizeof(buf) - len - 1, format, argp);
	va_end(argp);

	assert(len < (sizeof(buf) - 1));

	escaped = utils_string_escape(buf);
	len = snprintf(buf, sizeof(buf), "%s", escaped);
	free(escaped);
	assert(len < (sizeof(buf) - 1));

	buf[len] = '\n';
	buf[len + 1] = '\0';

	len++;

	s = channel_init(seamless_channel, len);
	out_uint8p(s, buf, len) s_mark_end(s);

	DEBUG_SEAMLESS(("seamlessrdp sending:%s", buf));

#if 0
	printf("seamless send:\n");
	hexdump(s->channel_hdr + 8, s->end - s->channel_hdr - 8);
#endif

	channel_send(s, seamless_channel);

	return seamless_serial++;
}


unsigned int
seamless_send_sync()
{
	if (!g_seamless_rdp)
		return (unsigned int) -1;

	return seamless_send("SYNC", "");
}


unsigned int
seamless_send_state(unsigned long id, unsigned int state, unsigned long flags)
{
	if (!g_seamless_rdp)
		return (unsigned int) -1;

	return seamless_send("STATE", "0x%08lx,0x%x,0x%lx", id, state, flags);
}


unsigned int
seamless_send_position(unsigned long id, int x, int y, int width, int height, unsigned long flags)
{
	return seamless_send("POSITION", "0x%08lx,%d,%d,%d,%d,0x%lx", id, x, y, width, height,
			     flags);
}


/* Update select timeout */
void
seamless_select_timeout(struct timeval *tv)
{
	struct timeval ourtimeout = { 0, SEAMLESSRDP_POSITION_TIMER };

	if (g_seamless_rdp)
	{
		if (timercmp(&ourtimeout, tv, <))
		{
			tv->tv_sec = ourtimeout.tv_sec;
			tv->tv_usec = ourtimeout.tv_usec;
		}
	}
}


unsigned int
seamless_send_zchange(unsigned long id, unsigned long below, unsigned long flags)
{
	if (!g_seamless_rdp)
		return (unsigned int) -1;

	return seamless_send("ZCHANGE", "0x%08lx,0x%08lx,0x%lx", id, below, flags);
}



unsigned int
seamless_send_focus(unsigned long id, unsigned long flags)
{
	if (!g_seamless_rdp)
		return (unsigned int) -1;

	return seamless_send("FOCUS", "0x%08lx,0x%lx", id, flags);
}

/* Send client-to-server message to destroy process on the server. */
unsigned int
seamless_send_destroy(unsigned long id)
{
	return seamless_send("DESTROY", "0x%08lx", id);
}

unsigned int
seamless_send_spawn(char *cmdline)
{
	unsigned int res;
	if (!g_seamless_rdp)
		return (unsigned int) -1;

	res = seamless_send("SPAWN", cmdline);

	return res;
}

unsigned int
seamless_send_persistent(RD_BOOL enable)
{
	unsigned int res;
	if (!g_seamless_rdp)
		return (unsigned int) -1;
	printf("%s persistent seamless mode.\n", enable?"Enable":"Disable");
	res = seamless_send("PERSISTENT", "%d", enable);
	
	return res;
}
