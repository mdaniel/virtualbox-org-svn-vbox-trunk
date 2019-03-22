/*
   rdesktop: A Remote Desktop Protocol client.
   Sound DSP routines
   Copyright (C) Michael Gernoth 2006-2008

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

/* Software volume control */
void rdpsnd_dsp_softvol_set(uint16 left, uint16 right);

/* Endian conversion */
void rdpsnd_dsp_swapbytes(unsigned char *buffer, unsigned int size, RD_WAVEFORMATEX * format);

/* Resample control */
RD_BOOL rdpsnd_dsp_resample_set(uint32 device_srate, uint16 device_bitspersample,
				uint16 device_channels);
RD_BOOL rdpsnd_dsp_resample_supported(RD_WAVEFORMATEX * pwfx);

STREAM rdpsnd_dsp_process(unsigned char *data, unsigned int size,
			  struct audio_driver *current_driver, RD_WAVEFORMATEX * format);
