/* $Id$ */
/** @file
 * VirtualBox Guest Additions Driver for NetBSD.
 */

/*
 * Copyright (C) 2007-2017 Oracle Corporation
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


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/select.h>
#include <sys/conf.h>
#include <sys/kernel.h>
#include <sys/kmem.h>
#include <sys/module.h>
#include <sys/device.h>
#include <sys/bus.h>
#include <sys/poll.h>
#include <sys/proc.h>
#include <sys/stat.h>
#include <sys/selinfo.h>
#include <sys/queue.h>
#include <sys/lock.h>
#include <sys/types.h>
#include <sys/conf.h>
#include <sys/malloc.h>
#include <sys/uio.h>
#include <sys/file.h>
#include <sys/filedesc.h>
#include <sys/vfs_syscalls.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcidevs.h>

#include <dev/wscons/wsconsio.h>
#include <dev/wscons/wsmousevar.h>
#include <dev/wscons/tpcalibvar.h>

#ifdef PVM
#  undef PVM
#endif
#include "VBoxGuestInternal.h"
#include <VBox/log.h>
#include <iprt/assert.h>
#include <iprt/initterm.h>
#include <iprt/process.h>
#include <iprt/mem.h>
#include <iprt/asm.h>


/*********************************************************************************************************************************
*   Defined Constants And Macros                                                                                                 *
*********************************************************************************************************************************/
/** The module name. */
#define DEVICE_NAME  "vboxguest"


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/
typedef struct VBoxGuestDeviceState
{
    device_t sc_dev;
    pci_chipset_tag_t sc_pc;

    bus_space_tag_t sc_iot;
    bus_space_handle_t sc_ioh;
    bus_addr_t sc_iobase;
    bus_size_t sc_iosize;

    bus_space_tag_t sc_memt;
    bus_space_handle_t sc_memh;

    /** Size of the memory area. */
    bus_size_t         sc_memsize;

    /** IRQ resource handle. */
    pci_intr_handle_t  ih;
    /** Pointer to the IRQ handler. */
    void              *pfnIrqHandler;

    /** Controller features, limits and status. */
    u_int              vboxguest_state;

    device_t sc_wsmousedev;
    VMMDevReqMouseStatus *sc_vmmmousereq;
    PVBOXGUESTSESSION sc_session;
    struct tpcalib_softc sc_tpcalib;
} vboxguest_softc;


struct vboxguest_fdata
{
    vboxguest_softc *sc;
    PVBOXGUESTSESSION session;
};

#define VBOXGUEST_STATE_INITOK 1 << 0


/*********************************************************************************************************************************
*   Internal Functions                                                                                                           *
*********************************************************************************************************************************/
/*
 * Driver(9) autoconf machinery.
 */
static int VBoxGuestNetBSDMatch(device_t parent, cfdata_t match, void *aux);
static void VBoxGuestNetBSDAttach(device_t parent, device_t self, void *aux);
static void VBoxGuestNetBSDWsmAttach(vboxguest_softc *sc);
static int VBoxGuestNetBSDDetach(device_t self, int flags);

/*
 * IRQ related functions.
 */
static int  VBoxGuestNetBSDAddIRQ(vboxguest_softc *sc, struct pci_attach_args *pa);
static void VBoxGuestNetBSDRemoveIRQ(vboxguest_softc *sc);
static int  VBoxGuestNetBSDISR(void *pvState);

/*
 * Character device file handlers.
 */
static int VBoxGuestNetBSDOpen(dev_t device, int flags, int fmt, struct lwp *process);
static int VBoxGuestNetBSDClose(struct file *fp);
static int VBoxGuestNetBSDIOCtl(struct file *fp, u_long cmd, void *addr);
static int VBoxGuestNetBSDIOCtlSlow(struct vboxguest_fdata *fdata, u_long command, void *data);
static int VBoxGuestNetBSDPoll(struct file *fp, int events);

/*
 * wsmouse(4) accessops
 */
static int VBoxGuestNetBSDWsmEnable(void *cookie);
static void VBoxGuestNetBSDWsmDisable(void *cookie);
static int VBoxGuestNetBSDWsmIOCtl(void *cookie, u_long cmd, void *data, int flag, struct lwp *l);

static int VBoxGuestNetBSDSetMouseStatus(vboxguest_softc *sc, uint32_t fStatus);


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/
extern struct cfdriver vboxguest_cd; /* CFDRIVER_DECL */
extern struct cfattach vboxguest_ca; /* CFATTACH_DECL */

/*
 * The /dev/vboxguest character device entry points.
 */
static struct cdevsw g_VBoxGuestNetBSDChrDevSW =
{
    VBoxGuestNetBSDOpen,
    noclose,
    noread,
    nowrite,
    noioctl,
    nostop,
    notty,
    nopoll,
    nommap,
    nokqfilter,
};

static const struct fileops vboxguest_fileops = {
    .fo_read = fbadop_read,
    .fo_write = fbadop_write,
    .fo_ioctl = VBoxGuestNetBSDIOCtl,
    .fo_fcntl = fnullop_fcntl,
    .fo_poll = VBoxGuestNetBSDPoll,
    .fo_stat = fbadop_stat,
    .fo_close = VBoxGuestNetBSDClose,
    .fo_kqfilter = fnullop_kqfilter,
    .fo_restart = fnullop_restart
};


const struct wsmouse_accessops vboxguest_wsm_accessops = {
    VBoxGuestNetBSDWsmEnable,
    VBoxGuestNetBSDWsmIOCtl,
    VBoxGuestNetBSDWsmDisable,
};


static struct wsmouse_calibcoords vboxguest_wsm_default_calib = {
    .minx = VMMDEV_MOUSE_RANGE_MIN,
    .miny = VMMDEV_MOUSE_RANGE_MIN,
    .maxx = VMMDEV_MOUSE_RANGE_MAX,
    .maxy = VMMDEV_MOUSE_RANGE_MAX,
    .samplelen = WSMOUSE_CALIBCOORDS_RESET,
};

/** Device extention & session data association structure. */
static VBOXGUESTDEVEXT      g_DevExt;

static vboxguest_softc     *g_SC;

/** Reference counter */
static volatile uint32_t    cUsers;
/** selinfo structure used for polling. */
static struct selinfo       g_SelInfo;


CFATTACH_DECL_NEW(vboxguest, sizeof(vboxguest_softc),
    VBoxGuestNetBSDMatch, VBoxGuestNetBSDAttach, VBoxGuestNetBSDDetach, NULL);


static int VBoxGuestNetBSDMatch(device_t parent, cfdata_t match, void *aux)
{
    const struct pci_attach_args *pa = aux;

    if (RT_UNLIKELY(g_SC != NULL)) /* should not happen */
        return 0;

    if (   PCI_VENDOR(pa->pa_id) == VMMDEV_VENDORID
        && PCI_PRODUCT(pa->pa_id) == VMMDEV_DEVICEID)
    {
        return 1;
    }

    return 0;
}


static void VBoxGuestNetBSDAttach(device_t parent, device_t self, void *aux)
{
    int rc = VINF_SUCCESS;
    int iResId = 0;
    vboxguest_softc *sc;
    struct pci_attach_args *pa = aux;
    bus_space_tag_t iot, memt;
    bus_space_handle_t ioh, memh;
    bus_dma_segment_t seg;
    int ioh_valid, memh_valid;

    KASSERT(g_SC == NULL);

    cUsers = 0;

    aprint_normal(": VirtualBox Guest\n");

    sc = device_private(self);
    sc->sc_dev = self;

    /*
     * Initialize IPRT R0 driver, which internally calls OS-specific r0 init.
     */
    rc = RTR0Init(0);
    if (RT_FAILURE(rc))
    {
        LogFunc(("RTR0Init failed.\n"));
        aprint_error_dev(sc->sc_dev, "RTR0Init failed\n");
        return;
    }

    sc->sc_pc = pa->pa_pc;

    /*
     * Allocate I/O port resource.
     */
    ioh_valid = (pci_mapreg_map(pa, PCI_BAR0,
                                PCI_MAPREG_TYPE_IO, 0,
                                &sc->sc_iot, &sc->sc_ioh,
                                &sc->sc_iobase, &sc->sc_iosize) == 0);

    if (ioh_valid)
    {

        /*
         * Map the MMIO region.
         */
        memh_valid = (pci_mapreg_map(pa, PCI_BAR1,
                                     PCI_MAPREG_TYPE_MEM, BUS_SPACE_MAP_LINEAR,
                                     &sc->sc_memt, &sc->sc_memh,
                                     NULL, &sc->sc_memsize) == 0);
        if (memh_valid)
        {
            /*
             * Call the common device extension initializer.
             */
            rc = VGDrvCommonInitDevExt(&g_DevExt, sc->sc_iobase,
                                       bus_space_vaddr(sc->sc_memt, sc->sc_memh),
                                       sc->sc_memsize,
#if ARCH_BITS == 64
                                       VBOXOSTYPE_NetBSD_x64,
#else
                                       VBOXOSTYPE_NetBSD,
#endif
                                       VMMDEV_EVENT_MOUSE_POSITION_CHANGED);
            if (RT_SUCCESS(rc))
            {
                /*
                 * Add IRQ of VMMDev.
                 */
                rc = VBoxGuestNetBSDAddIRQ(sc, pa);
                if (RT_SUCCESS(rc))
                {
                    sc->vboxguest_state |= VBOXGUEST_STATE_INITOK;

                    /*
                     * Read host configuration.
                     */
                    VGDrvCommonProcessOptionsFromHost(&g_DevExt);

                    /*
                     * Attach wsmouse.
                     */
                    VBoxGuestNetBSDWsmAttach(sc);

                    g_SC = sc;
                    return;
                }
                VGDrvCommonDeleteDevExt(&g_DevExt);
            }
            else
            {
                aprint_error_dev(sc->sc_dev, "init failed\n");
            }
            bus_space_unmap(sc->sc_memt, sc->sc_memh, sc->sc_memsize);
        }
        else
        {
            aprint_error_dev(sc->sc_dev, "MMIO mapping failed\n");
        }
        bus_space_unmap(sc->sc_iot, sc->sc_ioh, sc->sc_iosize);
    }
    else
    {
        aprint_error_dev(sc->sc_dev, "IO mapping failed\n");
    }

    RTR0Term();
    return;
}


/**
 * Sets IRQ for VMMDev.
 *
 * @returns NetBSD error code.
 * @param   sc  Pointer to the state info structure.
 * @param   pa  Pointer to the PCI attach arguments.
 */
static int VBoxGuestNetBSDAddIRQ(vboxguest_softc *sc, struct pci_attach_args *pa)
{
    int iResId = 0;
    int rc = 0;
    const char *intrstr;
#if __NetBSD_Prereq__(6, 99, 39)
    char intstrbuf[100];
#endif

    LogFlow((DEVICE_NAME ": %s\n", __func__));

    if (pci_intr_map(pa, &sc->ih))
    {
        aprint_error_dev(sc->sc_dev, "couldn't map interrupt.\n");
        return VERR_DEV_IO_ERROR;
    }

    intrstr = pci_intr_string(sc->sc_pc, sc->ih
#if __NetBSD_Prereq__(6, 99, 39)
                              , intstrbuf, sizeof(intstrbuf)
#endif
                              );
    aprint_normal_dev(sc->sc_dev, "interrupting at %s\n", intrstr);

    sc->pfnIrqHandler = pci_intr_establish(sc->sc_pc, sc->ih, IPL_BIO, VBoxGuestNetBSDISR, sc);
    if (sc->pfnIrqHandler == NULL)
    {
        aprint_error_dev(sc->sc_dev, "couldn't establish interrupt\n");
        return VERR_DEV_IO_ERROR;
    }

    return VINF_SUCCESS;
}


/*
 * Optionally attach wsmouse(4) device as a child.
 */
static void VBoxGuestNetBSDWsmAttach(vboxguest_softc *sc)
{
    struct wsmousedev_attach_args am = { &vboxguest_wsm_accessops, sc };

    PVBOXGUESTSESSION session = NULL;
    VMMDevReqMouseStatus *req = NULL;
    int rc;

    rc = VGDrvCommonCreateKernelSession(&g_DevExt, &session);
    if (RT_FAILURE(rc))
        goto fail;

    rc = VbglR0GRAlloc((VMMDevRequestHeader **)&req, sizeof(*req),
                       VMMDevReq_GetMouseStatus);
    if (RT_FAILURE(rc))
        goto fail;

    sc->sc_wsmousedev = config_found_ia(sc->sc_dev, "wsmousedev", &am, wsmousedevprint);
    if (sc->sc_wsmousedev == NULL)
        goto fail;

    sc->sc_session = session;
    sc->sc_vmmmousereq = req;

    tpcalib_init(&sc->sc_tpcalib);
    tpcalib_ioctl(&sc->sc_tpcalib, WSMOUSEIO_SCALIBCOORDS,
                  &vboxguest_wsm_default_calib, 0, 0);
    return;

  fail:
    if (session != NULL)
        VGDrvCommonCloseSession(&g_DevExt, session);
    if (req != NULL)
        VbglR0GRFree((VMMDevRequestHeader *)req);
}


static int VBoxGuestNetBSDDetach(device_t self, int flags)
{
    vboxguest_softc *sc;
    sc = device_private(self);

    LogFlow((DEVICE_NAME ": %s\n", __func__));

    if (cUsers > 0)
        return EBUSY;

    if ((sc->vboxguest_state & VBOXGUEST_STATE_INITOK) == 0)
        return 0;

    /*
     * Reverse what we did in VBoxGuestNetBSDAttach.
     */
    if (sc->sc_vmmmousereq != NULL)
        VbglR0GRFree((VMMDevRequestHeader *)sc->sc_vmmmousereq);

    VBoxGuestNetBSDRemoveIRQ(sc);

    VGDrvCommonDeleteDevExt(&g_DevExt);

    bus_space_unmap(sc->sc_memt, sc->sc_memh, sc->sc_memsize);
    bus_space_unmap(sc->sc_iot, sc->sc_ioh, sc->sc_iosize);

    RTR0Term();

    return config_detach_children(self, flags);
}


/**
 * Removes IRQ for VMMDev.
 *
 * @param   sc      Opaque pointer to the state info structure.
 */
static void VBoxGuestNetBSDRemoveIRQ(vboxguest_softc *sc)
{
    LogFlow((DEVICE_NAME ": %s\n", __func__));

    if (sc->pfnIrqHandler)
    {
        pci_intr_disestablish(sc->sc_pc, sc->pfnIrqHandler);
    }
}


/**
 * Interrupt service routine.
 *
 * @returns Whether the interrupt was from VMMDev.
 * @param   pvState Opaque pointer to the device state.
 */
static int VBoxGuestNetBSDISR(void *pvState)
{
    LogFlow((DEVICE_NAME ": %s: pvState=%p\n", __func__, pvState));

    bool fOurIRQ = VGDrvCommonISR(&g_DevExt);

    return fOurIRQ ? 1 : 0;
}


/*
 * Called by VGDrvCommonISR() if mouse position changed
 */
void VGDrvNativeISRMousePollEvent(PVBOXGUESTDEVEXT pDevExt)
{
    vboxguest_softc *sc = g_SC;

    LogFlow((DEVICE_NAME ": %s\n", __func__));

    /*
     * Wake up poll waiters.
     */
    selnotify(&g_SelInfo, 0, 0);

    if (sc->sc_vmmmousereq != NULL) {
        int x, y;
        int rc;

        sc->sc_vmmmousereq->mouseFeatures = 0;
        sc->sc_vmmmousereq->pointerXPos = 0;
        sc->sc_vmmmousereq->pointerYPos = 0;

        rc = VbglR0GRPerform(&sc->sc_vmmmousereq->header);
        if (RT_FAILURE(rc))
            return;

        tpcalib_trans(&sc->sc_tpcalib,
                      sc->sc_vmmmousereq->pointerXPos,
                      sc->sc_vmmmousereq->pointerYPos,
                      &x, &y);

        wsmouse_input(sc->sc_wsmousedev,
                      0,    /* buttons */
                      x, y,
                      0, 0, /* z, w */
                      WSMOUSE_INPUT_ABSOLUTE_X | WSMOUSE_INPUT_ABSOLUTE_Y);
    }
}


bool VGDrvNativeProcessOption(PVBOXGUESTDEVEXT pDevExt, const char *pszName, const char *pszValue)
{
    RT_NOREF(pDevExt); RT_NOREF(pszName); RT_NOREF(pszValue);
    return false;
}


static int VBoxGuestNetBSDSetMouseStatus(vboxguest_softc *sc, uint32_t fStatus)
{
    VBGLIOCSETMOUSESTATUS Req;
    int rc;

    VBGLREQHDR_INIT(&Req.Hdr, SET_MOUSE_STATUS);
    Req.u.In.fStatus = fStatus;
    rc = VGDrvCommonIoCtl(VBGL_IOCTL_SET_MOUSE_STATUS,
                          &g_DevExt,
                          sc->sc_session,
                          &Req.Hdr, sizeof(Req));
    if (RT_SUCCESS(rc))
        rc = Req.Hdr.rc;

    return rc;
}


static int
VBoxGuestNetBSDWsmEnable(void *cookie)
{
    vboxguest_softc *sc = cookie;
    int rc;

    rc = VBoxGuestNetBSDSetMouseStatus(sc, VMMDEV_MOUSE_GUEST_CAN_ABSOLUTE
                                         | VMMDEV_MOUSE_NEW_PROTOCOL);
    if (RT_FAILURE(rc))
        return RTErrConvertToErrno(rc);

    return 0;
}


static void
VBoxGuestNetBSDWsmDisable(void *cookie)
{
    vboxguest_softc *sc = cookie;
    VBoxGuestNetBSDSetMouseStatus(sc, 0);
}


static int
VBoxGuestNetBSDWsmIOCtl(void *cookie, u_long cmd, void *data, int flag, struct lwp *l)
{
    vboxguest_softc *sc = cookie;

    switch (cmd) {
    case WSMOUSEIO_GTYPE:
        *(u_int *)data = WSMOUSE_TYPE_TPANEL;
        break;

    case WSMOUSEIO_SCALIBCOORDS:
    case WSMOUSEIO_GCALIBCOORDS:
        return tpcalib_ioctl(&sc->sc_tpcalib, cmd, data, flag, l);

    default:
        return EPASSTHROUGH;
    }
    return 0;
}


/**
 * File open handler
 *
 */
static int VBoxGuestNetBSDOpen(dev_t device, int flags, int fmt, struct lwp *pLwp)
{
    vboxguest_softc *sc;
    struct vboxguest_fdata *fdata;
    file_t *fp;
    int fd, error;

    LogFlow((DEVICE_NAME ": %s\n", __func__));

    if ((sc = device_lookup_private(&vboxguest_cd, minor(device))) == NULL)
    {
        printf("device_lookup_private failed\n");
        return (ENXIO);
    }

    if ((sc->vboxguest_state & VBOXGUEST_STATE_INITOK) == 0)
    {
        aprint_error_dev(sc->sc_dev, "device not configured\n");
        return (ENXIO);
    }

    fdata = kmem_alloc(sizeof(*fdata), KM_SLEEP);
    if (fdata != NULL)
    {
        fdata->sc = sc;

        error = fd_allocfile(&fp, &fd);
        if (error == 0)
        {
            /*
             * Create a new session.
             */
            int rc;
            struct kauth_cred *pCred = pLwp->l_cred;
            uint32_t fRequestor = VMMDEV_REQUESTOR_USERMODE | VMMDEV_REQUESTOR_TRUST_NOT_GIVEN;
            if (pCred && kauth_cred_geteuid(pCred) == 0)
                fRequestor |= VMMDEV_REQUESTOR_USR_ROOT;
            else
                fRequestor |= VMMDEV_REQUESTOR_USR_USER;

            if (pCred && kauth_cred_ismember_gid(pCred, 0))
                fRequestor |= VMMDEV_REQUESTOR_GRP_WHEEL;
            fRequestor |= VMMDEV_REQUESTOR_NO_USER_DEVICE; /** @todo implement /dev/vboxuser
            if (!fUnrestricted)
                fRequestor |= VMMDEV_REQUESTOR_USER_DEVICE; */
            fRequestor |= VMMDEV_REQUESTOR_CON_DONT_KNOW; /** @todo can we find out if pLwp is on the console? */
            rc = VGDrvCommonCreateUserSession(&g_DevExt, fRequestor, &fdata->session);
            if (RT_SUCCESS(rc))
            {
                ASMAtomicIncU32(&cUsers);
                return fd_clone(fp, fd, flags, &vboxguest_fileops, fdata);
            }

            aprint_error_dev(sc->sc_dev, "VBox session creation failed\n");
            closef(fp); /* ??? */
            error = RTErrConvertToErrno(rc);
        }
        kmem_free(fdata, sizeof(*fdata));
    }
    else
        error = NOMEM;
    return error;

}

/**
 * File close handler
 *
 */
static int VBoxGuestNetBSDClose(struct file *fp)
{
    struct vboxguest_fdata *fdata = fp->f_data;
    vboxguest_softc *sc = fdata->sc;

    LogFlow((DEVICE_NAME ": %s\n", __func__));

    VGDrvCommonCloseSession(&g_DevExt, fdata->session);
    ASMAtomicDecU32(&cUsers);

    kmem_free(fdata, sizeof(*fdata));

    return 0;
}

/**
 * IOCTL handler
 *
 */
static int VBoxGuestNetBSDIOCtl(struct file *fp, u_long command, void *data)
{
    struct vboxguest_fdata *fdata = fp->f_data;

    if (VBGL_IOCTL_IS_FAST(command))
        return VGDrvCommonIoCtlFast(command, &g_DevExt, fdata->session);

    return VBoxGuestNetBSDIOCtlSlow(fdata, command, data);
}

static int VBoxGuestNetBSDIOCtlSlow(struct vboxguest_fdata *fdata, u_long command, void *data)
{
    vboxguest_softc *sc = fdata->sc;
    size_t cbReq = IOCPARM_LEN(command);
    PVBGLREQHDR pHdr = NULL;
    void *pvUser = NULL;
    int err, rc;

    LogFlow(("%s: command=%#lx data=%p\n", __func__, command, data));

    /*
     * Buffered request?
     */
    if ((command & IOC_DIRMASK) == IOC_INOUT)
    {
        /* will be validated by VGDrvCommonIoCtl() */
        pHdr = (PVBGLREQHDR)data;
    }

    /*
     * Big unbuffered request?  "data" is the userland pointer.
     */
    else if ((command & IOC_DIRMASK) == IOC_VOID && cbReq != 0)
    {
        /*
         * Read the header, validate it and figure out how much that
         * needs to be buffered.
         */
        VBGLREQHDR Hdr;

        if (RT_UNLIKELY(cbReq < sizeof(Hdr)))
            return ENOTTY;

        pvUser = data;
        err = copyin(pvUser, &Hdr, sizeof(Hdr));
        if (RT_UNLIKELY(err != 0))
            return err;

        if (RT_UNLIKELY(Hdr.uVersion != VBGLREQHDR_VERSION))
            return ENOTTY;

        if (cbReq > 16 * _1M)
            return EINVAL;

        if (Hdr.cbOut == 0)
            Hdr.cbOut = Hdr.cbIn;

        if (RT_UNLIKELY(   Hdr.cbIn  < sizeof(Hdr) || Hdr.cbIn  > cbReq
                        || Hdr.cbOut < sizeof(Hdr) || Hdr.cbOut > cbReq))
            return EINVAL;

        /*
         * Allocate buffer and copy in the data.
         */
        cbReq = RT_MAX(Hdr.cbIn, Hdr.cbOut);

        pHdr = (PVBGLREQHDR)RTMemTmpAlloc(cbReq);
        if (RT_UNLIKELY(pHdr == NULL))
        {
            LogRel(("%s: command=%#lx data=%p: unable to allocate %zu bytes\n",
                    __func__, command, data, cbReq));
            return ENOMEM;
        }

        err = copyin(pvUser, pHdr, Hdr.cbIn);
        if (err != 0)
        {
            RTMemTmpFree(pHdr);
            return err;
        }

        if (Hdr.cbIn < cbReq)
            memset((uint8_t *)pHdr + Hdr.cbIn, '\0', cbReq - Hdr.cbIn);
    }

    /*
     * Process the IOCtl.
     */
    rc = VGDrvCommonIoCtl(command, &g_DevExt, fdata->session, pHdr, cbReq);
    if (RT_SUCCESS(rc))
    {
        err = 0;

        /*
         * If unbuffered, copy back the result before returning.
         */
        if (pvUser != NULL)
        {
            size_t cbOut = pHdr->cbOut;
            if (cbOut > cbReq)
            {
                LogRel(("%s: command=%#lx data=%p: too much output: %zu > %zu\n",
                        __func__, command, data, cbOut, cbReq));
                cbOut = cbReq;
            }

            err = copyout(pHdr, pvUser, cbOut);
            RTMemTmpFree(pHdr);
        }
    }
    else
    {
        LogRel(("%s: command=%#lx data=%p: error %Rrc\n",
                __func__, command, data, rc));

        if (pvUser != NULL)
            RTMemTmpFree(pHdr);

        err = RTErrConvertToErrno(rc);
    }

    return err;
}

static int VBoxGuestNetBSDPoll(struct file *fp, int events)
{
    struct vboxguest_fdata *fdata = fp->f_data;
    vboxguest_softc *sc = fdata->sc;

    int rc = 0;
    int events_processed;

    uint32_t u32CurSeq;

    LogFlow((DEVICE_NAME ": %s\n", __func__));

    u32CurSeq = ASMAtomicUoReadU32(&g_DevExt.u32MousePosChangedSeq);
    if (fdata->session->u32MousePosChangedSeq != u32CurSeq)
    {
        events_processed = events & (POLLIN | POLLRDNORM);
        fdata->session->u32MousePosChangedSeq = u32CurSeq;
    }
    else
    {
        events_processed = 0;

        selrecord(curlwp, &g_SelInfo);
    }

    return events_processed;
}


/**
 * @note This code is duplicated on other platforms with variations, so please
 *       keep them all up to date when making changes!
 */
int VBOXCALL VBoxGuestIDC(void *pvSession, uintptr_t uReq, PVBGLREQHDR pReqHdr, size_t cbReq)
{
    /*
     * Simple request validation (common code does the rest).
     */
    int rc;
    if (   RT_VALID_PTR(pReqHdr)
        && cbReq >= sizeof(*pReqHdr))
    {
        /*
         * All requests except the connect one requires a valid session.
         */
        PVBOXGUESTSESSION pSession = (PVBOXGUESTSESSION)pvSession;
        if (pSession)
        {
            if (   RT_VALID_PTR(pSession)
                && pSession->pDevExt == &g_DevExt)
                rc = VGDrvCommonIoCtl(uReq, &g_DevExt, pSession, pReqHdr, cbReq);
            else
                rc = VERR_INVALID_HANDLE;
        }
        else if (uReq == VBGL_IOCTL_IDC_CONNECT)
        {
            rc = VGDrvCommonCreateKernelSession(&g_DevExt, &pSession);
            if (RT_SUCCESS(rc))
            {
                rc = VGDrvCommonIoCtl(uReq, &g_DevExt, pSession, pReqHdr, cbReq);
                if (RT_FAILURE(rc))
                    VGDrvCommonCloseSession(&g_DevExt, pSession);
            }
        }
        else
            rc = VERR_INVALID_HANDLE;
    }
    else
        rc = VERR_INVALID_POINTER;
    return rc;
}


MODULE(MODULE_CLASS_DRIVER, vboxguest, "pci");

/*
 * XXX: See netbsd/vboxguest.ioconf for the details.
*/
#if 0
#include "ioconf.c"
#else

static const struct cfiattrdata wsmousedevcf_iattrdata = {
    "wsmousedev", 1, {
        { "mux", "0", 0 },
    }
};

/* device vboxguest: wsmousedev */
static const struct cfiattrdata * const vboxguest_attrs[] = { &wsmousedevcf_iattrdata, NULL };
CFDRIVER_DECL(vboxguest, DV_DULL, vboxguest_attrs);

static struct cfdriver * const cfdriver_ioconf_vboxguest[] = {
    &vboxguest_cd, NULL
};


static const struct cfparent vboxguest_pspec = {
    "pci", "pci", DVUNIT_ANY
};
static int vboxguest_loc[] = { -1, -1 };


static const struct cfparent wsmousedev_pspec = {
    "wsmousedev", "vboxguest", DVUNIT_ANY
};
static int wsmousedev_loc[] = { 0 };


static struct cfdata cfdata_ioconf_vboxguest[] = {
    /*  vboxguest0 at pci? dev ? function ? */
    {
        .cf_name = "vboxguest",
        .cf_atname = "vboxguest",
        .cf_unit = 0,           /* Only unit 0 is ever used  */
        .cf_fstate = FSTATE_NOTFOUND,
        .cf_loc = vboxguest_loc,
        .cf_flags = 0,
        .cf_pspec = &vboxguest_pspec,
    },

    /* wsmouse* at vboxguest? */
    { "wsmouse", "wsmouse", 0, FSTATE_STAR, wsmousedev_loc, 0, &wsmousedev_pspec },

    { NULL, NULL, 0, 0, NULL, 0, NULL }
};

static struct cfattach * const vboxguest_cfattachinit[] = {
    &vboxguest_ca, NULL
};

static const struct cfattachinit cfattach_ioconf_vboxguest[] = {
    { "vboxguest", vboxguest_cfattachinit },
    { NULL, NULL }
};
#endif


static int
vboxguest_modcmd(modcmd_t cmd, void *opaque)
{
    devmajor_t bmajor, cmajor;
    register_t retval;
    int error;

    LogFlow((DEVICE_NAME ": %s\n", __func__));

    switch (cmd)
    {
        case MODULE_CMD_INIT:
            error = config_init_component(cfdriver_ioconf_vboxguest,
                                          cfattach_ioconf_vboxguest,
                                          cfdata_ioconf_vboxguest);
            if (error)
                break;

            bmajor = cmajor = NODEVMAJOR;
            error = devsw_attach("vboxguest",
                                 NULL, &bmajor,
                                 &g_VBoxGuestNetBSDChrDevSW, &cmajor);
            if (error)
            {
                if (error == EEXIST)
                    error = 0; /* maybe built-in ... improve eventually */
                else
                    break;
            }

            error = do_sys_mknod(curlwp, "/dev/vboxguest",
                                 0666|S_IFCHR, makedev(cmajor, 0),
                                 &retval, UIO_SYSSPACE);
            if (error == EEXIST)
                error = 0;
            break;

        case MODULE_CMD_FINI:
            error = config_fini_component(cfdriver_ioconf_vboxguest,
                                          cfattach_ioconf_vboxguest,
                                          cfdata_ioconf_vboxguest);
            if (error)
                break;

            devsw_detach(NULL, &g_VBoxGuestNetBSDChrDevSW);
            break;

        default:
            return ENOTTY;
    }
    return error;
}
