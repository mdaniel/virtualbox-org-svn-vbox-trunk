/* $Id$ */
/** @file
 * VBoxNetAdp - Virtual Network Adapter Driver (Host), Linux Specific Code.
 */

/*
 * Copyright (C) 2009-2017 Oracle Corporation
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
#include "the-linux-kernel.h"
#include "version-generated.h"
#include "revision-generated.h"
#include "product-generated.h"
#include <linux/ethtool.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/miscdevice.h>

#define LOG_GROUP LOG_GROUP_NET_ADP_DRV
#include <VBox/log.h>
#include <VBox/err.h>
#include <iprt/process.h>
#include <iprt/initterm.h>
#include <iprt/mem.h>
#include <iprt/string.h>

/*
#include <iprt/assert.h>
#include <iprt/semaphore.h>
#include <iprt/spinlock.h>
#include <iprt/string.h>
#include <iprt/uuid.h>
#include <iprt/alloca.h>
*/

#define VBOXNETADP_OS_SPECFIC 1
#include "../VBoxNetAdpInternal.h"


/*********************************************************************************************************************************
*   Defined Constants And Macros                                                                                                 *
*********************************************************************************************************************************/
#define VBOXNETADP_LINUX_NAME      "vboxnet%d"
#define VBOXNETADP_CTL_DEV_NAME    "vboxnetctl"

#define VBOXNETADP_FROM_IFACE(iface) ((PVBOXNETADP) ifnet_softc(iface))


/*********************************************************************************************************************************
*   Internal Functions                                                                                                           *
*********************************************************************************************************************************/
static int  VBoxNetAdpLinuxInit(void);
static void VBoxNetAdpLinuxUnload(void);

static int VBoxNetAdpLinuxOpen(struct inode *pInode, struct file *pFilp);
static int VBoxNetAdpLinuxClose(struct inode *pInode, struct file *pFilp);
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 36)
static int VBoxNetAdpLinuxIOCtl(struct inode *pInode, struct file *pFilp,
                                unsigned int uCmd, unsigned long ulArg);
#else /* LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 36) */
static long VBoxNetAdpLinuxIOCtlUnlocked(struct file *pFilp,
                                         unsigned int uCmd, unsigned long ulArg);
#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 36) */

static void vboxNetAdpEthGetDrvinfo(struct net_device *dev, struct ethtool_drvinfo *info);
static int vboxNetAdpEthGetSettings(struct net_device *dev, struct ethtool_cmd *cmd);


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/
module_init(VBoxNetAdpLinuxInit);
module_exit(VBoxNetAdpLinuxUnload);

MODULE_AUTHOR(VBOX_VENDOR);
MODULE_DESCRIPTION(VBOX_PRODUCT " Network Adapter Driver");
MODULE_LICENSE("GPL");
#ifdef MODULE_VERSION
MODULE_VERSION(VBOX_VERSION_STRING " r" RT_XSTR(VBOX_SVN_REV) " (" RT_XSTR(INTNETTRUNKIFPORT_VERSION) ")");
#endif

/**
 * The (common) global data.
 */
static struct file_operations gFileOpsVBoxNetAdp =
{
    owner:      THIS_MODULE,
    open:       VBoxNetAdpLinuxOpen,
    release:    VBoxNetAdpLinuxClose,
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 36)
    ioctl:      VBoxNetAdpLinuxIOCtl,
#else /* LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 36) */
    unlocked_ioctl: VBoxNetAdpLinuxIOCtlUnlocked,
#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 36) */
};

/** The miscdevice structure. */
static struct miscdevice g_CtlDev =
{
    minor:      MISC_DYNAMIC_MINOR,
    name:       VBOXNETADP_CTL_DEV_NAME,
    fops:       &gFileOpsVBoxNetAdp,
# if LINUX_VERSION_CODE <= KERNEL_VERSION(2, 6, 17)
    devfs_name: VBOXNETADP_CTL_DEV_NAME
# endif
};

static const struct ethtool_ops gEthToolOpsVBoxNetAdp =
{
    .get_drvinfo        = vboxNetAdpEthGetDrvinfo,
    .get_settings       = vboxNetAdpEthGetSettings,
    .get_link           = ethtool_op_get_link,
};


struct VBoxNetAdpPriv
{
    struct net_device_stats Stats;
};

typedef struct VBoxNetAdpPriv VBOXNETADPPRIV;
typedef VBOXNETADPPRIV *PVBOXNETADPPRIV;

static int vboxNetAdpLinuxOpen(struct net_device *pNetDev)
{
    netif_start_queue(pNetDev);
    return 0;
}

static int vboxNetAdpLinuxStop(struct net_device *pNetDev)
{
    netif_stop_queue(pNetDev);
    return 0;
}

static int vboxNetAdpLinuxXmit(struct sk_buff *pSkb, struct net_device *pNetDev)
{
    PVBOXNETADPPRIV pPriv = netdev_priv(pNetDev);

    /* Update the stats. */
    pPriv->Stats.tx_packets++;
    pPriv->Stats.tx_bytes += pSkb->len;
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 31)
    /* Update transmission time stamp. */
    pNetDev->trans_start = jiffies;
#endif
    /* Nothing else to do, just free the sk_buff. */
    dev_kfree_skb(pSkb);
    return 0;
}

struct net_device_stats *vboxNetAdpLinuxGetStats(struct net_device *pNetDev)
{
    PVBOXNETADPPRIV pPriv = netdev_priv(pNetDev);
    return &pPriv->Stats;
}


/* ethtool_ops::get_drvinfo */
static void vboxNetAdpEthGetDrvinfo(struct net_device *pNetDev, struct ethtool_drvinfo *info)
{
    PVBOXNETADPPRIV pPriv = netdev_priv(pNetDev);
    NOREF(pPriv);

    RTStrPrintf(info->driver, sizeof(info->driver),
                "%s", VBOXNETADP_NAME);

    /*
     * Would be nice to include VBOX_SVN_REV, but it's not available
     * here.  Use file's svn revision via svn keyword?
     */
    RTStrPrintf(info->version, sizeof(info->version),
                "%s", VBOX_VERSION_STRING);

    RTStrPrintf(info->fw_version, sizeof(info->fw_version),
                "0x%08X", INTNETTRUNKIFPORT_VERSION);

    RTStrPrintf(info->bus_info, sizeof(info->driver),
                "N/A");
}


/* ethtool_ops::get_settings */
static int vboxNetAdpEthGetSettings(struct net_device *pNetDev, struct ethtool_cmd *cmd)
{
    cmd->supported      = 0;
    cmd->advertising    = 0;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 27)
    ethtool_cmd_speed_set(cmd, SPEED_10);
#else
    cmd->speed          = SPEED_10;
#endif
    cmd->duplex         = DUPLEX_FULL;
    cmd->port           = PORT_TP;
    cmd->phy_address    = 0;
    cmd->transceiver    = XCVR_INTERNAL;
    cmd->autoneg        = AUTONEG_DISABLE;
    cmd->maxtxpkt       = 0;
    cmd->maxrxpkt       = 0;
    return 0;
}


#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 29)
static const struct net_device_ops vboxNetAdpNetdevOps = {
    .ndo_open               = vboxNetAdpLinuxOpen,
    .ndo_stop               = vboxNetAdpLinuxStop,
    .ndo_start_xmit         = vboxNetAdpLinuxXmit,
    .ndo_get_stats          = vboxNetAdpLinuxGetStats
};
#endif

static void vboxNetAdpNetDevInit(struct net_device *pNetDev)
{
    PVBOXNETADPPRIV pPriv;

    ether_setup(pNetDev);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 29)
    pNetDev->netdev_ops = &vboxNetAdpNetdevOps;
#else /* LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 29) */
    pNetDev->open = vboxNetAdpLinuxOpen;
    pNetDev->stop = vboxNetAdpLinuxStop;
    pNetDev->hard_start_xmit = vboxNetAdpLinuxXmit;
    pNetDev->get_stats = vboxNetAdpLinuxGetStats;
#endif /* LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 29) */

    pNetDev->ethtool_ops = &gEthToolOpsVBoxNetAdp;

    pPriv = netdev_priv(pNetDev);
    memset(pPriv, 0, sizeof(*pPriv));
}


int vboxNetAdpOsCreate(PVBOXNETADP pThis, PCRTMAC pMACAddress)
{
    int rc = VINF_SUCCESS;
    struct net_device *pNetDev;

    /* No need for private data. */
    pNetDev = alloc_netdev(sizeof(VBOXNETADPPRIV),
                           pThis->szName[0] ? pThis->szName : VBOXNETADP_LINUX_NAME,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 17, 0)
                           NET_NAME_UNKNOWN,
#endif
                           vboxNetAdpNetDevInit);
    if (pNetDev)
    {
        int err;

        if (pNetDev->dev_addr)
        {
            memcpy(pNetDev->dev_addr, pMACAddress, ETH_ALEN);
            Log2(("vboxNetAdpOsCreate: pNetDev->dev_addr = %.6Rhxd\n", pNetDev->dev_addr));

            /*
             * We treat presence of VBoxNetFlt filter as our "carrier",
             * see vboxNetFltSetLinkState().
             *
             * operstates.txt: "On device allocation, networking core
             * sets the flags equivalent to netif_carrier_ok() and
             * !netif_dormant()" - so turn carrier off here.
             */
            netif_carrier_off(pNetDev);

            err = register_netdev(pNetDev);
            if (!err)
            {
                strncpy(pThis->szName, pNetDev->name, sizeof(pThis->szName));
                pThis->szName[sizeof(pThis->szName) - 1] = '\0';
                pThis->u.s.pNetDev = pNetDev;
                Log2(("vboxNetAdpOsCreate: pThis=%p pThis->szName = %p\n", pThis, pThis->szName));
                return VINF_SUCCESS;
            }
        }
        else
        {
            LogRel(("VBoxNetAdp: failed to set MAC address (dev->dev_addr == NULL)\n"));
            err = EFAULT;
        }
        free_netdev(pNetDev);
        rc = RTErrConvertFromErrno(err);
    }
    return rc;
}

void vboxNetAdpOsDestroy(PVBOXNETADP pThis)
{
    struct net_device *pNetDev = pThis->u.s.pNetDev;
    AssertPtr(pThis->u.s.pNetDev);

    pThis->u.s.pNetDev = NULL;
    unregister_netdev(pNetDev);
    free_netdev(pNetDev);
}

/**
 * Device open. Called on open /dev/vboxnetctl
 *
 * @param   pInode      Pointer to inode info structure.
 * @param   pFilp       Associated file pointer.
 */
static int VBoxNetAdpLinuxOpen(struct inode *pInode, struct file *pFilp)
{
    Log(("VBoxNetAdpLinuxOpen: pid=%d/%d %s\n", RTProcSelf(), current->pid, current->comm));

#ifdef VBOX_WITH_HARDENING
    /*
     * Only root is allowed to access the device, enforce it!
     */
    if (!capable(CAP_SYS_ADMIN))
    {
        Log(("VBoxNetAdpLinuxOpen: admin privileges required!\n"));
        return -EPERM;
    }
#endif

    return 0;
}


/**
 * Close device.
 *
 * @param   pInode      Pointer to inode info structure.
 * @param   pFilp       Associated file pointer.
 */
static int VBoxNetAdpLinuxClose(struct inode *pInode, struct file *pFilp)
{
    Log(("VBoxNetAdpLinuxClose: pid=%d/%d %s\n",
         RTProcSelf(), current->pid, current->comm));
    pFilp->private_data = NULL;
    return 0;
}

/**
 * Device I/O Control entry point.
 *
 * @param   pFilp       Associated file pointer.
 * @param   uCmd        The function specified to ioctl().
 * @param   ulArg       The argument specified to ioctl().
 */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 36)
static int VBoxNetAdpLinuxIOCtl(struct inode *pInode, struct file *pFilp,
                                unsigned int uCmd, unsigned long ulArg)
#else /* LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 36) */
static long VBoxNetAdpLinuxIOCtlUnlocked(struct file *pFilp,
                                         unsigned int uCmd, unsigned long ulArg)
#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 36) */
{
    VBOXNETADPREQ Req;
    PVBOXNETADP pAdp;
    int rc;
    char *pszName = NULL;

    Log(("VBoxNetAdpLinuxIOCtl: param len %#x; uCmd=%#x; add=%#x\n", _IOC_SIZE(uCmd), uCmd, VBOXNETADP_CTL_ADD));
    if (RT_UNLIKELY(_IOC_SIZE(uCmd) != sizeof(Req))) /* paranoia */
    {
        Log(("VBoxNetAdpLinuxIOCtl: bad ioctl sizeof(Req)=%#x _IOC_SIZE=%#x; uCmd=%#x.\n", sizeof(Req), _IOC_SIZE(uCmd), uCmd));
        return -EINVAL;
    }

    switch (uCmd)
    {
        case VBOXNETADP_CTL_ADD:
            Log(("VBoxNetAdpLinuxIOCtl: _IOC_DIR(uCmd)=%#x; IOC_OUT=%#x\n", _IOC_DIR(uCmd), IOC_OUT));
            if (RT_UNLIKELY(copy_from_user(&Req, (void *)ulArg, sizeof(Req))))
            {
                Log(("VBoxNetAdpLinuxIOCtl: copy_from_user(,%#lx,) failed; uCmd=%#x.\n", ulArg, uCmd));
                return -EFAULT;
            }
            Log(("VBoxNetAdpLinuxIOCtl: Add %s\n", Req.szName));

            if (Req.szName[0])
            {
                pAdp = vboxNetAdpFindByName(Req.szName);
                if (pAdp)
                {
                    Log(("VBoxNetAdpLinuxIOCtl: '%s' already exists\n", Req.szName));
                    return -EINVAL;
                }
                pszName = Req.szName;
            }
            rc = vboxNetAdpCreate(&pAdp, pszName);
            if (RT_FAILURE(rc))
            {
                Log(("VBoxNetAdpLinuxIOCtl: vboxNetAdpCreate -> %Rrc\n", rc));
                return -(rc == VERR_OUT_OF_RESOURCES ? ENOMEM : EINVAL);
            }

            Assert(strlen(pAdp->szName) < sizeof(Req.szName));
            strncpy(Req.szName, pAdp->szName, sizeof(Req.szName) - 1);
            Req.szName[sizeof(Req.szName) - 1] = '\0';

            if (RT_UNLIKELY(copy_to_user((void *)ulArg, &Req, sizeof(Req))))
            {
                /* this is really bad! */
                /** @todo remove the adapter again? */
                printk(KERN_ERR "VBoxNetAdpLinuxIOCtl: copy_to_user(%#lx,,%#zx); uCmd=%#x!\n", ulArg, sizeof(Req), uCmd);
                return -EFAULT;
            }
            Log(("VBoxNetAdpLinuxIOCtl: Successfully added '%s'\n", Req.szName));
            break;

        case VBOXNETADP_CTL_REMOVE:
            if (RT_UNLIKELY(copy_from_user(&Req, (void *)ulArg, sizeof(Req))))
            {
                Log(("VBoxNetAdpLinuxIOCtl: copy_from_user(,%#lx,) failed; uCmd=%#x.\n", ulArg, uCmd));
                return -EFAULT;
            }
            Log(("VBoxNetAdpLinuxIOCtl: Remove %s\n", Req.szName));

            pAdp = vboxNetAdpFindByName(Req.szName);
            if (!pAdp)
            {
                Log(("VBoxNetAdpLinuxIOCtl: '%s' not found\n", Req.szName));
                return -EINVAL;
            }

            rc = vboxNetAdpDestroy(pAdp);
            if (RT_FAILURE(rc))
            {
                Log(("VBoxNetAdpLinuxIOCtl: vboxNetAdpDestroy('%s') -> %Rrc\n", Req.szName, rc));
                return -EINVAL;
            }
            Log(("VBoxNetAdpLinuxIOCtl: Successfully removed '%s'\n", Req.szName));
            break;

        default:
            printk(KERN_ERR "VBoxNetAdpLinuxIOCtl: unknown command %x.\n", uCmd);
            return -EINVAL;
    }

    return 0;
}

int  vboxNetAdpOsInit(PVBOXNETADP pThis)
{
    /*
     * Init linux-specific members.
     */
    pThis->u.s.pNetDev = NULL;

    return VINF_SUCCESS;
}



/**
 * Initialize module.
 *
 * @returns appropriate status code.
 */
static int __init VBoxNetAdpLinuxInit(void)
{
    int rc;
    /*
     * Initialize IPRT.
     */
    rc = RTR0Init(0);
    if (RT_SUCCESS(rc))
    {
        Log(("VBoxNetAdpLinuxInit\n"));

        rc = vboxNetAdpInit();
        if (RT_SUCCESS(rc))
        {
            rc = misc_register(&g_CtlDev);
            if (rc)
            {
                printk(KERN_ERR "VBoxNetAdp: Can't register " VBOXNETADP_CTL_DEV_NAME " device! rc=%d\n", rc);
                return rc;
            }
            LogRel(("VBoxNetAdp: Successfully started.\n"));
            return 0;
        }
        else
            LogRel(("VBoxNetAdp: failed to register vboxnet0 device (rc=%d)\n", rc));
    }
    else
        LogRel(("VBoxNetAdp: failed to initialize IPRT (rc=%d)\n", rc));

    return -RTErrConvertToErrno(rc);
}


/**
 * Unload the module.
 *
 * @todo We have to prevent this if we're busy!
 */
static void __exit VBoxNetAdpLinuxUnload(void)
{
    Log(("VBoxNetAdpLinuxUnload\n"));

    /*
     * Undo the work done during start (in reverse order).
     */

    vboxNetAdpShutdown();
    /* Remove control device */
    misc_deregister(&g_CtlDev);

    RTR0Term();

    Log(("VBoxNetAdpLinuxUnload - done\n"));
}

