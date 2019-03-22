/* -*- c-basic-offset: 8 -*-
   rdesktop: A Remote Desktop Protocol client.
   Copyright (C) Matthew Chapman <matthewc.unsw.edu.au> 1999-2008

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

#define	MAX_PARALLEL_DEVICES		1

#define FILE_DEVICE_PARALLEL		0x22

#define IOCTL_PAR_QUERY_RAW_DEVICE_ID	0x0c

#include "rdesktop.h"
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <errno.h>

#if defined(__linux__)
#include <linux/lp.h>
#endif

extern RDPDR_DEVICE g_rdpdr_device[];


/* Enumeration of devices from rdesktop.c        */
/* returns numer of units found and initialized. */
/* optarg looks like ':LPT1=/dev/lp0'            */
/* when it arrives to this function.             */
int
parallel_enum_devices(uint32 * id, char *optarg)
{
	PARALLEL_DEVICE *ppar_info;

	char *pos = optarg;
	char *pos2;
	int count = 0;

	/* skip the first colon */
	optarg++;
	while ((pos = next_arg(optarg, ',')) && *id < RDPDR_MAX_DEVICES)
	{
		ppar_info = (PARALLEL_DEVICE *) xmalloc(sizeof(PARALLEL_DEVICE));

		pos2 = next_arg(optarg, '=');
		strcpy(g_rdpdr_device[*id].name, optarg);

		toupper_str(g_rdpdr_device[*id].name);

		g_rdpdr_device[*id].local_path = xmalloc(strlen(pos2) + 1);
		strcpy(g_rdpdr_device[*id].local_path, pos2);
		printf("PARALLEL %s to %s\n", optarg, pos2);

		/* set device type */
		g_rdpdr_device[*id].device_type = DEVICE_TYPE_PARALLEL;
		g_rdpdr_device[*id].pdevice_data = (void *) ppar_info;
		g_rdpdr_device[*id].handle = 0;
		count++;
		(*id)++;

		optarg = pos;
	}
	return count;
}

static RD_NTSTATUS
parallel_create(uint32 device_id, uint32 access, uint32 share_mode, uint32 disposition,
		uint32 flags, char *filename, RD_NTHANDLE * handle)
{
	int parallel_fd;

	parallel_fd = open(g_rdpdr_device[device_id].local_path, O_RDWR);
	if (parallel_fd == -1)
	{
		perror("open");
		return RD_STATUS_ACCESS_DENIED;
	}

	/* all read and writes should be non blocking */
	if (fcntl(parallel_fd, F_SETFL, O_NONBLOCK) == -1)
		perror("fcntl");

#if defined(LPABORT)
	/* Retry on errors */
	ioctl(parallel_fd, LPABORT, (int) 1);
#endif

	g_rdpdr_device[device_id].handle = parallel_fd;

	*handle = parallel_fd;

	return RD_STATUS_SUCCESS;
}

static RD_NTSTATUS
parallel_close(RD_NTHANDLE handle)
{
	int i = get_device_index(handle);
	if (i >= 0)
		g_rdpdr_device[i].handle = 0;
	close(handle);
	return RD_STATUS_SUCCESS;
}

static RD_NTSTATUS
parallel_read(RD_NTHANDLE handle, uint8 * data, uint32 length, uint32 offset, uint32 * result)
{
	*result = read(handle, data, length);
	return RD_STATUS_SUCCESS;
}

static RD_NTSTATUS
parallel_write(RD_NTHANDLE handle, uint8 * data, uint32 length, uint32 offset, uint32 * result)
{
	int rc = RD_STATUS_SUCCESS;

	int n = write(handle, data, length);
	if (n < 0)
	{
#if defined(LPGETSTATUS)
		int status;
#endif

		*result = 0;
		switch (errno)
		{
			case EAGAIN:
				rc = RD_STATUS_DEVICE_OFF_LINE;
			case ENOSPC:
				rc = RD_STATUS_DEVICE_PAPER_EMPTY;
			case EIO:
				rc = RD_STATUS_DEVICE_OFF_LINE;
			default:
				rc = RD_STATUS_DEVICE_POWERED_OFF;
		}
#if defined(LPGETSTATUS)
		if (ioctl(handle, LPGETSTATUS, &status) == 0)
		{
			/* coming soon: take care for the printer status */
			printf("parallel_write: status = %d, errno = %d\n", status, errno);
		}
#endif
	}
	*result = n;
	return rc;
}

static RD_NTSTATUS
parallel_device_control(RD_NTHANDLE handle, uint32 request, STREAM in, STREAM out)
{
	if ((request >> 16) != FILE_DEVICE_PARALLEL)
		return RD_STATUS_INVALID_PARAMETER;

	/* extract operation */
	request >>= 2;
	request &= 0xfff;

	printf("PARALLEL IOCTL %d: ", request);

	switch (request)
	{
		case IOCTL_PAR_QUERY_RAW_DEVICE_ID:

		default:

			printf("\n");
			unimpl("UNKNOWN IOCTL %d\n", request);
	}
	return RD_STATUS_SUCCESS;
}

DEVICE_FNS parallel_fns = {
	parallel_create,
	parallel_close,
	parallel_read,
	parallel_write,
	parallel_device_control
};
