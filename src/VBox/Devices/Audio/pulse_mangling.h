/** @file
 *
 * Mangle libpulse symbols. This is necessary on hosts which don't
 * support the -fvisibility gcc switch.
 */

/*
 * Copyright (C) 2013-2017 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef AUDIO_PULSE_MANGLING_H
#define AUDIO_PULSE_MANGLING_H

#define PULSE_MANGLER(symbol) VBox_##symbol

#define pa_stream_connect_playback              PULSE_MANGLER(pa_stream_connect_playback)
#define pa_stream_connect_record                PULSE_MANGLER(pa_stream_connect_record)
#define pa_stream_disconnect                    PULSE_MANGLER(pa_stream_disconnect)
#define pa_stream_get_sample_spec               PULSE_MANGLER(pa_stream_get_sample_spec)
#define pa_stream_set_latency_update_callback   PULSE_MANGLER(pa_stream_set_latency_update_callback)
#define pa_stream_write                         PULSE_MANGLER(pa_stream_write)
#define pa_stream_unref                         PULSE_MANGLER(pa_stream_unref)
#define pa_stream_get_state                     PULSE_MANGLER(pa_stream_get_state)
#define pa_stream_get_latency                   PULSE_MANGLER(pa_stream_get_latency)
#define pa_stream_get_timing_info               PULSE_MANGLER(pa_stream_get_timing_info)
#define pa_stream_set_buffer_attr               PULSE_MANGLER(pa_stream_set_buffer_attr)
#define pa_stream_set_state_callback            PULSE_MANGLER(pa_stream_set_state_callback)
#define pa_stream_set_underflow_callback        PULSE_MANGLER(pa_stream_set_underflow_callback)
#define pa_stream_set_write_callback            PULSE_MANGLER(pa_stream_set_write_callback)
#define pa_stream_flush                         PULSE_MANGLER(pa_stream_flush)
#define pa_stream_drain                         PULSE_MANGLER(pa_stream_drain)
#define pa_stream_trigger                       PULSE_MANGLER(pa_stream_trigger)
#define pa_stream_new                           PULSE_MANGLER(pa_stream_new)
#define pa_stream_get_buffer_attr               PULSE_MANGLER(pa_stream_get_buffer_attr)
#define pa_stream_peek                          PULSE_MANGLER(pa_stream_peek)
#define pa_stream_cork                          PULSE_MANGLER(pa_stream_cork)
#define pa_stream_drop                          PULSE_MANGLER(pa_stream_drop)
#define pa_stream_writable_size                 PULSE_MANGLER(pa_stream_writable_size)
#define pa_context_connect                      PULSE_MANGLER(pa_context_connect)
#define pa_context_disconnect                   PULSE_MANGLER(pa_context_disconnect)
#define pa_context_get_server_info              PULSE_MANGLER(pa_context_get_server_info)
#define pa_context_get_sink_info_by_name        PULSE_MANGLER(pa_context_get_sink_info_by_name)
#define pa_context_get_source_info_by_name      PULSE_MANGLER(pa_context_get_source_info_by_name)
#define pa_context_get_state                    PULSE_MANGLER(pa_context_get_state)
#define pa_context_unref                        PULSE_MANGLER(pa_context_unref)
#define pa_context_errno                        PULSE_MANGLER(pa_context_errno)
#define pa_context_new                          PULSE_MANGLER(pa_context_new)
#define pa_context_set_state_callback           PULSE_MANGLER(pa_context_set_state_callback)
#define pa_threaded_mainloop_stop               PULSE_MANGLER(pa_threaded_mainloop_stop)
#define pa_threaded_mainloop_get_api            PULSE_MANGLER(pa_threaded_mainloop_get_api)
#define pa_threaded_mainloop_free               PULSE_MANGLER(pa_threaded_mainloop_free)
#define pa_threaded_mainloop_signal             PULSE_MANGLER(pa_threaded_mainloop_signal)
#define pa_threaded_mainloop_unlock             PULSE_MANGLER(pa_threaded_mainloop_unlock)
#define pa_threaded_mainloop_new                PULSE_MANGLER(pa_threaded_mainloop_new)
#define pa_threaded_mainloop_wait               PULSE_MANGLER(pa_threaded_mainloop_wait)
#define pa_threaded_mainloop_start              PULSE_MANGLER(pa_threaded_mainloop_start)
#define pa_threaded_mainloop_lock               PULSE_MANGLER(pa_threaded_mainloop_lock)
#define pa_bytes_per_second                     PULSE_MANGLER(pa_bytes_per_second)
#define pa_bytes_to_usec                        PULSE_MANGLER(pa_bytes_to_usec)
#define pa_usec_to_bytes                        PULSE_MANGLER(pa_usec_to_bytes)
#define pa_rtclock_now                          PULSE_MANGLER(pa_rtclock_now)
#define pa_frame_size                           PULSE_MANGLER(pa_frame_size)
#define pa_sample_format_to_string              PULSE_MANGLER(pa_sample_format_to_string)
#define pa_sample_spec_valid                    PULSE_MANGLER(pa_sample_spec_valid)
#define pa_channel_map_init_auto                PULSE_MANGLER(pa_channel_map_init_auto)
#define pa_operation_unref                      PULSE_MANGLER(pa_operation_unref)
#define pa_operation_get_state                  PULSE_MANGLER(pa_operation_get_state)
#define pa_operation_cancel                     PULSE_MANGLER(pa_operation_cancel)
#define pa_strerror                             PULSE_MANGLER(pa_strerror)
#define pa_stream_readable_size                 PULSE_MANGLER(pa_stream_readable_size)

#endif /* !AUDIO_PULSE_MANGLING_H */
