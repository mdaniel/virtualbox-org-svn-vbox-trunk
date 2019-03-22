/**************************************************************************
 *
 * Copyright 2012 Francisco Jerez
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sub license, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.
 * IN NO EVENT SHALL VMWARE AND/OR ITS SUPPLIERS BE LIABLE FOR
 * ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 **************************************************************************/

/**
 * \file Library that provides device enumeration and creation of
 * winsys/pipe_screen instances.
 */

#ifndef PIPE_LOADER_H
#define PIPE_LOADER_H

#include "pipe/p_compiler.h"
#include "state_tracker/drm_driver.h"
#include "util/xmlconfig.h"

#ifdef __cplusplus
extern "C" {
#endif

struct pipe_screen;
struct drisw_loader_funcs;

enum pipe_loader_device_type {
   PIPE_LOADER_DEVICE_SOFTWARE,
   PIPE_LOADER_DEVICE_PCI,
   PIPE_LOADER_DEVICE_PLATFORM,
   NUM_PIPE_LOADER_DEVICE_TYPES
};

/**
 * A device known to the pipe loader.
 */
struct pipe_loader_device {
   enum pipe_loader_device_type type;

   union {
      struct {
         int vendor_id;
         int chip_id;
      } pci;
   } u; /**< Discriminated by \a type */

   char *driver_name;
   const struct pipe_loader_ops *ops;

   driOptionCache option_cache;
   driOptionCache option_info;
};

/**
 * Get a list of known devices.
 *
 * \param devs Array that will be filled with pointers to the devices
 *             available in the system.
 * \param ndev Maximum number of devices to return.
 * \return Number of devices available in the system.
 */
int
pipe_loader_probe(struct pipe_loader_device **devs, int ndev);

/**
 * Create a pipe_screen for the specified device.
 *
 * \param dev Device the screen will be created for.
 */
struct pipe_screen *
pipe_loader_create_screen(struct pipe_loader_device *dev);

/**
 * Query the configuration parameters for the specified device.
 *
 * \param dev Device that will be queried.
 * \param conf The drm_conf id of the option to be queried.
 */
const struct drm_conf_ret *
pipe_loader_configuration(struct pipe_loader_device *dev,
                          enum drm_conf conf);

/**
 * Ensure that dev->option_cache is initialized appropriately for the driver.
 *
 * This function can be called multiple times.
 *
 * \param dev Device for which options should be loaded.
 */
void
pipe_loader_load_options(struct pipe_loader_device *dev);

/**
 * Get the driinfo XML string used by the given driver.
 *
 * The returned string is heap-allocated.
 */
char *
pipe_loader_get_driinfo_xml(const char *driver_name);

/**
 * Release resources allocated for a list of devices.
 *
 * Should be called when the specified devices are no longer in use to
 * release any resources allocated by pipe_loader_probe.
 *
 * \param devs Devices to release.
 * \param ndev Number of devices to release.
 */
void
pipe_loader_release(struct pipe_loader_device **devs, int ndev);

/**
 * Initialize sw dri device give the drisw_loader_funcs.
 *
 * This function is platform-specific.
 *
 * \sa pipe_loader_probe
 */
bool
pipe_loader_sw_probe_dri(struct pipe_loader_device **devs,
                         struct drisw_loader_funcs *drisw_lf);

/**
 * Initialize a kms backed sw device given an fd.
 *
 * This function is platform-specific.
 *
 * \sa pipe_loader_probe
 */
bool
pipe_loader_sw_probe_kms(struct pipe_loader_device **devs, int fd);

/**
 * Initialize a null sw device.
 *
 * This function is platform-specific.
 *
 * \sa pipe_loader_probe
 */
bool
pipe_loader_sw_probe_null(struct pipe_loader_device **devs);

/**
 * Get a list of known software devices.
 *
 * This function is platform-specific.
 *
 * \sa pipe_loader_probe
 */
int
pipe_loader_sw_probe(struct pipe_loader_device **devs, int ndev);

/**
 * Get a software device wrapped atop another device.
 *
 * This function is platform-specific.
 *
 * \sa pipe_loader_probe
 */
boolean
pipe_loader_sw_probe_wrapped(struct pipe_loader_device **dev,
                             struct pipe_screen *screen);

/**
 * Get a list of known DRM devices.
 *
 * This function is platform-specific.
 *
 * \sa pipe_loader_probe
 */
int
pipe_loader_drm_probe(struct pipe_loader_device **devs, int ndev);

/**
 * Initialize a DRM device in an already opened fd.
 *
 * This function is platform-specific.
 *
 * \sa pipe_loader_probe
 */
bool
pipe_loader_drm_probe_fd(struct pipe_loader_device **dev, int fd);

/**
 * Get the driinfo XML used for the DRM driver of the given name, if any.
 *
 * The returned string is heap-allocated.
 */
char *
pipe_loader_drm_get_driinfo_xml(const char *driver_name);

extern const char gallium_driinfo_xml[];

#ifdef __cplusplus
}
#endif

#endif /* PIPE_LOADER_H */
