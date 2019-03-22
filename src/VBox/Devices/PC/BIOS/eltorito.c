/*
 * Copyright (C) 2006-2016 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 * --------------------------------------------------------------------
 *
 * This code is based on:
 *
 *  ROM BIOS for use with Bochs/Plex86/QEMU emulation environment
 *
 *  Copyright (C) 2002  MandrakeSoft S.A.
 *
 *    MandrakeSoft S.A.
 *    43, rue d'Aboukir
 *    75002 Paris - France
 *    http://www.linux-mandrake.com/
 *    http://www.mandrakesoft.com/
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
 *
 */


#include <stdint.h>
#include <string.h>
#include "inlines.h"
#include "biosint.h"
#include "ebda.h"
#include "ata.h"

#if DEBUG_ELTORITO
#  define BX_DEBUG_INT13_ET(...)    BX_DEBUG(__VA_ARGS__)
#else
#  define BX_DEBUG_INT13_ET(...)
#endif

#if DEBUG_INT13_CD
#  define BX_DEBUG_INT13_CD(...)    BX_DEBUG(__VA_ARGS__)
#else
#  define BX_DEBUG_INT13_CD(...)
#endif

#if DEBUG_CD_BOOT
#  define BX_DEBUG_ELTORITO(...)    BX_DEBUG(__VA_ARGS__)
#else
#  define BX_DEBUG_ELTORITO(...)
#endif


/// @todo put in a header
#define AX      r.gr.u.r16.ax
#define BX      r.gr.u.r16.bx
#define CX      r.gr.u.r16.cx
#define DX      r.gr.u.r16.dx
#define SI      r.gr.u.r16.si
#define DI      r.gr.u.r16.di
#define BP      r.gr.u.r16.bp
#define ELDX    r.gr.u.r16.sp
#define DS      r.ds
#define ES      r.es
#define FLAGS   r.ra.flags.u.r16.flags

#pragma pack(1)

/* READ_10/WRITE_10 CDB padded to 12 bytes for ATAPI. */
typedef struct {
    uint16_t    command;    /* Command. */
    uint32_t    lba;        /* LBA, MSB first! */
    uint8_t     pad1;       /* Unused. */
    uint16_t    nsect;      /* Sector count, MSB first! */
    uint8_t     pad2[3];    /* Unused. */
} cdb_atapi;

#pragma pack()

ct_assert(sizeof(cdb_atapi) == 12);

/* Generic ATAPI/SCSI CD-ROM access routine signature. */
typedef uint16_t (* cd_pkt_func)(uint16_t device_id, uint8_t cmdlen, char __far *cmdbuf,
                                 uint16_t header, uint32_t length, uint8_t inout, char __far *buffer);

/* Pointers to HW specific CD-ROM access routines. */
cd_pkt_func     pktacc[DSKTYP_CNT] = {
    [DSK_TYPE_ATAPI]  = { ata_cmd_packet },
#ifdef VBOX_WITH_AHCI
    [DSK_TYPE_AHCI]   = { ahci_cmd_packet },
#endif
#ifdef VBOX_WITH_SCSI
    [DSK_TYPE_SCSI]   = { scsi_cmd_packet },
#endif
};

#if defined(VBOX_WITH_AHCI) || defined(VBOX_WITH_SCSI)
uint16_t dummy_soft_reset(uint16_t device_id)
{
    return 0;
}
#endif

/* Generic reset routine signature. */
typedef uint16_t (* cd_rst_func)(uint16_t device_id);

/* Pointers to HW specific CD-ROM reset routines. */
cd_rst_func     softrst[DSKTYP_CNT] = {
    [DSK_TYPE_ATAPI]  = { ata_soft_reset },
#ifdef VBOX_WITH_AHCI
    [DSK_TYPE_AHCI]   = { dummy_soft_reset },
#endif
#ifdef VBOX_WITH_SCSI
    [DSK_TYPE_SCSI]   = { dummy_soft_reset },
#endif
};


// ---------------------------------------------------------------------------
// Start of El-Torito boot functions
// ---------------------------------------------------------------------------

// !! TODO !! convert EBDA accesses to far pointers

extern  int     diskette_param_table;


void BIOSCALL cdemu_init(void)
{
    /// @todo a macro or a function for getting the EBDA segment
    uint16_t    ebda_seg = read_word(0x0040,0x000E);

    // the only important data is this one for now
    write_byte(ebda_seg,(uint16_t)&EbdaData->cdemu.active, 0x00);
}

uint8_t BIOSCALL cdemu_isactive(void)
{
    /// @todo a macro or a function for getting the EBDA segment
    uint16_t    ebda_seg = read_word(0x0040,0x000E);

    return read_byte(ebda_seg,(uint16_t)&EbdaData->cdemu.active);
}

uint8_t BIOSCALL cdemu_emulated_drive(void)
{
    /// @todo a macro or a function for getting the EBDA segment
    uint16_t    ebda_seg = read_word(0x0040,0x000E);

    return read_byte(ebda_seg,(uint16_t)&EbdaData->cdemu.emulated_drive);
}

// ---------------------------------------------------------------------------
// Start of int13 for eltorito functions
// ---------------------------------------------------------------------------

void BIOSCALL int13_eltorito(disk_regs_t r)
{
    /// @todo a macro or a function for getting the EBDA segment
    uint16_t        ebda_seg=read_word(0x0040,0x000E);
    cdemu_t __far   *cdemu;

    cdemu = ebda_seg :> &EbdaData->cdemu;


    BX_DEBUG_INT13_ET("%s: AX=%04x BX=%04x CX=%04x DX=%04x ES=%04x\n", __func__, AX, BX, CX, DX, ES);
    // BX_DEBUG_INT13_ET("%s: SS=%04x DS=%04x ES=%04x DI=%04x SI=%04x\n", __func__, get_SS(), DS, ES, DI, SI);

    switch (GET_AH()) {

    // FIXME ElTorito Various. Not implemented in many real BIOSes.
    case 0x4a: // ElTorito - Initiate disk emu
    case 0x4c: // ElTorito - Initiate disk emu and boot
    case 0x4d: // ElTorito - Return Boot catalog
        BX_INFO("%s: call with AX=%04x not implemented.\n", __func__, AX);
        goto int13_fail;
        break;

    case 0x4b: // ElTorito - Terminate disk emu
        // FIXME ElTorito Hardcoded
        /// @todo maybe our cdemu struct should match El Torito to allow memcpy()?
        write_byte(DS,SI+0x00,0x13);
        write_byte(DS,SI+0x01,cdemu->media);
        write_byte(DS,SI+0x02,cdemu->emulated_drive);
        write_byte(DS,SI+0x03,cdemu->controller_index);
        write_dword(DS,SI+0x04,cdemu->ilba);
        write_word(DS,SI+0x08,cdemu->device_spec);
        write_word(DS,SI+0x0a,cdemu->buffer_segment);
        write_word(DS,SI+0x0c,cdemu->load_segment);
        write_word(DS,SI+0x0e,cdemu->sector_count);
        write_byte(DS,SI+0x10,cdemu->vdevice.cylinders);
        write_byte(DS,SI+0x11,cdemu->vdevice.spt);
        write_byte(DS,SI+0x12,cdemu->vdevice.heads);

        // If we have to terminate emulation
        if(GET_AL() == 0x00) {
            // FIXME ElTorito Various. Should be handled accordingly to spec
            cdemu->active = 0;  // bye bye
        }

        goto int13_success;
        break;

    default:
          BX_INFO("%s: unsupported AH=%02x\n", __func__, GET_AH());
          goto int13_fail;
          break;
    }

int13_fail:
    SET_AH(0x01); // defaults to invalid function in AH or invalid parameter
    SET_DISK_RET_STATUS(GET_AH());
    SET_CF();     // error occurred
    return;

int13_success:
    SET_AH(0x00); // no error
    SET_DISK_RET_STATUS(0x00);
    CLEAR_CF();   // no error
    return;
}

// ---------------------------------------------------------------------------
// End of int13 for eltorito functions
// ---------------------------------------------------------------------------

/* Utility routine to check if a device is a CD-ROM. */
/// @todo this function is kinda useless as the ATAPI type check is obsolete.
static uint16_t device_is_cdrom(uint8_t device)
{
    bio_dsk_t __far *bios_dsk;

    bios_dsk = read_word(0x0040, 0x000E) :> &EbdaData->bdisk;

    if (device >= BX_MAX_STORAGE_DEVICES)
        return 0;

//    if (bios_dsk->devices[device].type != DSK_TYPE_ATAPI)
//        return 0;

    if (bios_dsk->devices[device].device != DSK_DEVICE_CDROM)
        return 0;

    return 1;
}

// ---------------------------------------------------------------------------
// End of ATA/ATAPI generic functions
// ---------------------------------------------------------------------------
static const char isotag[]="CD001";
static const char eltorito[]="EL TORITO SPECIFICATION";
//
// Returns ah: emulated drive, al: error code
//
uint16_t cdrom_boot(void)
{
    /// @todo a macro or a function for getting the EBDA segment
    uint16_t            ebda_seg=read_word(0x0040,0x000E);
    uint8_t             buffer[2048];
    cdb_atapi           atapicmd;
    uint32_t            lba;
    uint16_t            boot_segment, nbsectors, i, error;
    uint8_t             device;
    uint8_t             read_try;
    cdemu_t __far       *cdemu;
    bio_dsk_t __far     *bios_dsk;

    cdemu    = ebda_seg :> &EbdaData->cdemu;
    bios_dsk = ebda_seg :> &EbdaData->bdisk;

    /* Find the first CD-ROM. */
    for (device = 0; device < BX_MAX_STORAGE_DEVICES; ++device) {
        if (device_is_cdrom(device))
            break;
    }

    /* Fail if not found. */
    if (device >= BX_MAX_STORAGE_DEVICES)
        return 2;

    /* Read the Boot Record Volume Descriptor (BRVD). */
    _fmemset(&atapicmd, 0, sizeof(atapicmd));
    atapicmd.command = 0x28;    // READ 10 command
    atapicmd.lba     = swap_32(0x11);
    atapicmd.nsect   = swap_16(1);

    bios_dsk->drqp.nsect   = 1;
    bios_dsk->drqp.sect_sz = 2048;

    for (read_try = 0; read_try <= 4; ++read_try)
    {
        error = pktacc[bios_dsk->devices[device].type](device, 12, (char __far *)&atapicmd, 0, 2048L, ATA_DATA_IN, &buffer);
        if (!error)
            break;
    }
    if (error)
        return 3;

    /* Check for a valid BRVD. */
    if (buffer[0] != 0)
        return 4;
    /// @todo what's wrong with memcmp()?
    for (i = 0; i < 5; ++i) {
        if (buffer[1+i] != isotag[i])
            return 5;
    }
    for (i = 0; i < 23; ++i)
        if (buffer[7+i] != eltorito[i])
            return 6;

    // ok, now we calculate the Boot catalog address
    lba = *((uint32_t *)&buffer[0x47]);
    BX_DEBUG_ELTORITO("BRVD at LBA %lx\n", lba);

    /* Now we read the Boot Catalog. */
    atapicmd.command = 0x28;    // READ 10 command
    atapicmd.lba     = swap_32(lba);
    atapicmd.nsect   = swap_16(1);

#if 0   // Not necessary as long as previous values are reused
    bios_dsk->drqp.nsect   = 1;
    bios_dsk->drqp.sect_sz = 512;
#endif

    error = pktacc[bios_dsk->devices[device].type](device, 12, (char __far *)&atapicmd, 0, 2048L, ATA_DATA_IN, &buffer);
    if (error != 0)
        return 7;

    /// @todo Define a struct for the Boot Catalog, the hardcoded offsets are so dumb...

    /* Check if the Boot Catalog looks valid. */
    if (buffer[0x00] != 0x01)
        return 8;   // Header
    if (buffer[0x01] != 0x00)
        return 9;   // Platform
    if (buffer[0x1E] != 0x55)
        return 10;  // key 1
    if (buffer[0x1F] != 0xAA)
        return 10;  // key 2

    // Initial/Default Entry
    if (buffer[0x20] != 0x88)
        return 11; // Bootable

    cdemu->media = buffer[0x21];
    if (buffer[0x21] == 0) {
        // FIXME ElTorito Hardcoded. cdrom is hardcoded as device 0xE0.
        // Win2000 cd boot needs to know it booted from cd
        cdemu->emulated_drive = 0xE0;
    }
    else if (buffer[0x21] < 4)
        cdemu->emulated_drive = 0x00;
    else
        cdemu->emulated_drive = 0x80;

    cdemu->controller_index = device / 2;
    cdemu->device_spec      = device % 2;

    boot_segment  = *((uint16_t *)&buffer[0x22]);
    if (boot_segment == 0)
        boot_segment = 0x07C0;

    cdemu->load_segment   = boot_segment;
    cdemu->buffer_segment = 0x0000;

    nbsectors = ((uint16_t *)buffer)[0x26 / 2];
    cdemu->sector_count = nbsectors;

    /* Sanity check the sector count. In incorrectly mastered CDs, it might
     * be zero. If it's more than 512K, reject it as well.
     */
    if (nbsectors == 0 || nbsectors > 1024)
        return 12;

    lba = *((uint32_t *)&buffer[0x28]);
    cdemu->ilba = lba;

    BX_DEBUG_ELTORITO("Emulate drive %02x, type %02x, LBA %lu\n",
                      cdemu->emulated_drive, cdemu->media, cdemu->ilba);

    /* Read the disk image's boot sector into memory. */
    atapicmd.command = 0x28;    // READ 10 command
    atapicmd.lba     = swap_32(lba);
    atapicmd.nsect   = swap_16(1 + (nbsectors - 1) / 4);

    bios_dsk->drqp.nsect   = 1 + (nbsectors - 1) / 4;
    bios_dsk->drqp.sect_sz = 512;

    bios_dsk->drqp.skip_a = (2048 - nbsectors * 512) % 2048;

    error = pktacc[bios_dsk->devices[device].type](device, 12, (char __far *)&atapicmd, 0, nbsectors*512L, ATA_DATA_IN, MK_FP(boot_segment,0));

    bios_dsk->drqp.skip_a = 0;

    if (error != 0)
        return 13;

    BX_DEBUG_ELTORITO("Emulate drive %02x, type %02x, LBA %lu\n",
                      cdemu->emulated_drive, cdemu->media, cdemu->ilba);
    /* Set up emulated drive geometry based on the media type. */
    switch (cdemu->media) {
    case 0x01:  /* 1.2M floppy */
        cdemu->vdevice.spt       = 15;
        cdemu->vdevice.cylinders = 80;
        cdemu->vdevice.heads     = 2;
        break;
    case 0x02:  /* 1.44M floppy */
        cdemu->vdevice.spt       = 18;
        cdemu->vdevice.cylinders = 80;
        cdemu->vdevice.heads     = 2;
        break;
    case 0x03:  /* 2.88M floppy */
        cdemu->vdevice.spt       = 36;
        cdemu->vdevice.cylinders = 80;
        cdemu->vdevice.heads     = 2;
        break;
    case 0x04:  /* Hard disk */
        cdemu->vdevice.spt       = read_byte(boot_segment,446+6)&0x3f;
        cdemu->vdevice.cylinders = ((read_byte(boot_segment,446+6)&~0x3f)<<2) + read_byte(boot_segment,446+7) + 1;
        cdemu->vdevice.heads     = read_byte(boot_segment,446+5) + 1;
        break;
    }
    BX_DEBUG_ELTORITO("VCHS=%u/%u/%u\n", cdemu->vdevice.cylinders,
                      cdemu->vdevice.heads, cdemu->vdevice.spt);

    if (cdemu->media != 0) {
        /* Increase BIOS installed number of drives (floppy or fixed). */
        if (cdemu->emulated_drive == 0x00)
            write_byte(0x40,0x10,read_byte(0x40,0x10)|0x41);
        else
            write_byte(ebda_seg,(uint16_t)&EbdaData->bdisk.hdcount, read_byte(ebda_seg, (uint16_t)&EbdaData->bdisk.hdcount) + 1);
    }

    // everything is ok, so from now on, the emulation is active
    if (cdemu->media != 0)
        cdemu->active = 0x01;

    // return the boot drive + no error
    return (cdemu->emulated_drive*0x100)+0;
}

// ---------------------------------------------------------------------------
// End of El-Torito boot functions
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// Start of int13 when emulating a device from the cd
// ---------------------------------------------------------------------------

void BIOSCALL int13_cdemu(disk_regs_t r)
{
    /// @todo a macro or a function for getting the EBDA segment
    uint16_t            ebda_seg=read_word(0x0040,0x000E);
    uint8_t             device, status;
    uint16_t            vheads, vspt, vcylinders;
    uint16_t            head, sector, cylinder, nbsectors;
    uint32_t            vlba, ilba, slba, elba;
    uint16_t            before, segment, offset;
    cdb_atapi           atapicmd;
    cdemu_t __far       *cdemu;
    bio_dsk_t __far     *bios_dsk;

    cdemu    = ebda_seg :> &EbdaData->cdemu;
    bios_dsk = ebda_seg :> &EbdaData->bdisk;

    BX_DEBUG_INT13_ET("%s: AX=%04x BX=%04x CX=%04x DX=%04x ES=%04x\n", __func__, AX, BX, CX, DX, ES);

    /* at this point, we are emulating a floppy/harddisk */

    // Recompute the device number
    device  = cdemu->controller_index * 2;
    device += cdemu->device_spec;

    SET_DISK_RET_STATUS(0x00);

    /* basic checks : emulation should be active, dl should equal the emulated drive */
    if (!cdemu->active || (cdemu->emulated_drive != GET_DL())) {
        BX_INFO("%s: function %02x, emulation not active for DL= %02x\n", __func__, GET_AH(), GET_DL());
        goto int13_fail;
    }

    switch (GET_AH()) {

    case 0x00: /* disk controller reset */
        if (pktacc[bios_dsk->devices[device].type])
        {
            status = softrst[bios_dsk->devices[device].type](device);
        }
        goto int13_success;
        break;
    // all those functions return SUCCESS
    case 0x09: /* initialize drive parameters */
    case 0x0c: /* seek to specified cylinder */
    case 0x0d: /* alternate disk reset */  // FIXME ElTorito Various. should really reset ?
    case 0x10: /* check drive ready */     // FIXME ElTorito Various. should check if ready ?
    case 0x11: /* recalibrate */
    case 0x14: /* controller internal diagnostic */
    case 0x16: /* detect disk change */
        goto int13_success;
        break;

    // all those functions return disk write-protected
    case 0x03: /* write disk sectors */
    case 0x05: /* format disk track */
        SET_AH(0x03);
        goto int13_fail_noah;
        break;

    case 0x01: /* read disk status */
        status=read_byte(0x0040, 0x0074);
        SET_AH(status);
        SET_DISK_RET_STATUS(0);

        /* set CF if error status read */
        if (status)
            goto int13_fail_nostatus;
        else
            goto int13_success_noah;
        break;

    case 0x02: // read disk sectors
    case 0x04: // verify disk sectors
        vspt       = cdemu->vdevice.spt;
        vcylinders = cdemu->vdevice.cylinders;
        vheads     = cdemu->vdevice.heads;
        ilba       = cdemu->ilba;

        sector    = GET_CL() & 0x003f;
        cylinder  = (GET_CL() & 0x00c0) << 2 | GET_CH();
        head      = GET_DH();
        nbsectors = GET_AL();
        segment   = ES;
        offset    = BX;

        BX_DEBUG_INT13_ET("%s: read to %04x:%04x @ VCHS %u/%u/%u (%u sectors)\n", __func__,
                          ES, BX, cylinder, head, sector, nbsectors);

        // no sector to read ?
        if(nbsectors==0)
            goto int13_success;

        // sanity checks sco openserver needs this!
        if ((sector   >  vspt)
          || (cylinder >= vcylinders)
          || (head     >= vheads)) {
            goto int13_fail;
        }

        // After validating the input, verify does nothing
        if (GET_AH() == 0x04)
            goto int13_success;

        segment = ES+(BX / 16);
        offset  = BX % 16;

        // calculate the virtual lba inside the image
        vlba=((((uint32_t)cylinder*(uint32_t)vheads)+(uint32_t)head)*(uint32_t)vspt)+((uint32_t)(sector-1));

        // In advance so we don't lose the count
        SET_AL(nbsectors);

        // start lba on cd
        slba   = (uint32_t)vlba / 4;
        before = (uint32_t)vlba % 4;

        // end lba on cd
        elba = (uint32_t)(vlba + nbsectors - 1) / 4;

        _fmemset(&atapicmd, 0, sizeof(atapicmd));
        atapicmd.command = 0x28;    // READ 10 command
        atapicmd.lba     = swap_32(ilba + slba);
        atapicmd.nsect   = swap_16(elba - slba + 1);

        bios_dsk->drqp.nsect   = nbsectors;
        bios_dsk->drqp.sect_sz = 512;

        bios_dsk->drqp.skip_b = before * 512;
        bios_dsk->drqp.skip_a = ((4 - nbsectors % 4 - before) * 512) % 2048;

        status = pktacc[bios_dsk->devices[device].type](device, 12, (char __far *)&atapicmd, before*512, nbsectors*512L, ATA_DATA_IN, MK_FP(segment,offset));

        bios_dsk->drqp.skip_b = 0;
        bios_dsk->drqp.skip_a = 0;

        if (status != 0) {
            BX_INFO("%s: function %02x, error %02x !\n", __func__, GET_AH(), status);
            SET_AH(0x02);
            SET_AL(0);
            goto int13_fail_noah;
        }

        goto int13_success;
        break;

    case 0x08: /* read disk drive parameters */
        vspt       = cdemu->vdevice.spt;
        vcylinders = cdemu->vdevice.cylinders - 1;
        vheads     = cdemu->vdevice.heads - 1;

        SET_AL( 0x00 );
        SET_BL( 0x00 );
        SET_CH( vcylinders & 0xff );
        SET_CL((( vcylinders >> 2) & 0xc0) | ( vspt  & 0x3f ));
        SET_DH( vheads );
        SET_DL( 0x02 );   // FIXME ElTorito Various. should send the real count of drives 1 or 2
                          // FIXME ElTorito Harddisk. should send the HD count

        switch (cdemu->media) {
        case 0x01: SET_BL( 0x02 ); break;   /* 1.2 MB  */
        case 0x02: SET_BL( 0x04 ); break;   /* 1.44 MB */
        case 0x03: SET_BL( 0x05 ); break;   /* 2.88 MB */
        }

        /* Only set the DPT pointer for emulated floppies. */
        if (cdemu->media < 4) {
            DI = (uint16_t)&diskette_param_table;   /// @todo should this depend on emulated medium?
            ES = 0xF000;                            /// @todo how to make this relocatable?
        }
        goto int13_success;
        break;

    case 0x15: /* read disk drive size */
        // FIXME ElTorito Harddisk. What geometry to send ?
        SET_AH(0x03);
        goto int13_success_noah;
        break;

    // all those functions return unimplemented
    case 0x0a: /* read disk sectors with ECC */
    case 0x0b: /* write disk sectors with ECC */
    case 0x18: /* set media type for format */
    case 0x41: // IBM/MS installation check
      // FIXME ElTorito Harddisk. Darwin would like to use EDD
    case 0x42: // IBM/MS extended read
    case 0x43: // IBM/MS extended write
    case 0x44: // IBM/MS verify sectors
    case 0x45: // IBM/MS lock/unlock drive
    case 0x46: // IBM/MS eject media
    case 0x47: // IBM/MS extended seek
    case 0x48: // IBM/MS get drive parameters
    case 0x49: // IBM/MS extended media change
    case 0x4e: // ? - set hardware configuration
    case 0x50: // ? - send packet command
    default:
        BX_INFO("%s: function AH=%02x unsupported, returns fail\n", __func__, GET_AH());
        goto int13_fail;
        break;
    }

int13_fail:
    SET_AH(0x01); // defaults to invalid function in AH or invalid parameter
int13_fail_noah:
    SET_DISK_RET_STATUS(GET_AH());
int13_fail_nostatus:
    SET_CF();     // error occurred
    return;

int13_success:
    SET_AH(0x00); // no error
int13_success_noah:
    SET_DISK_RET_STATUS(0x00);
    CLEAR_CF();   // no error
    return;
}

// ---------------------------------------------------------------------------
// Start of int13 for cdrom
// ---------------------------------------------------------------------------

void BIOSCALL int13_cdrom(uint16_t EHBX, disk_regs_t r)
{
    uint16_t            ebda_seg = read_word(0x0040,0x000E);
    uint8_t             device, status, locks;
    cdb_atapi           atapicmd;
    uint32_t            lba;
    uint16_t            count, segment, offset, size;
    bio_dsk_t __far     *bios_dsk;
    int13ext_t __far    *i13x;
    dpt_t __far         *dpt;

    bios_dsk = ebda_seg :> &EbdaData->bdisk;

    BX_DEBUG_INT13_CD("%s: AX=%04x BX=%04x CX=%04x DX=%04x ES=%04x\n", __func__, AX, BX, CX, DX, ES);

    SET_DISK_RET_STATUS(0x00);

    /* basic check : device should be 0xE0+ */
    if( (GET_ELDL() < 0xE0) || (GET_ELDL() >= 0xE0 + BX_MAX_STORAGE_DEVICES) ) {
        BX_DEBUG("%s: function %02x, ELDL out of range %02x\n", __func__, GET_AH(), GET_ELDL());
        goto int13_fail;
    }

    // Get the ata channel
    device = bios_dsk->cdidmap[GET_ELDL()-0xE0];

    /* basic check : device has to be valid  */
    if (device >= BX_MAX_STORAGE_DEVICES) {
        BX_DEBUG("%s: function %02x, unmapped device for ELDL=%02x\n", __func__, GET_AH(), GET_ELDL());
        goto int13_fail;
    }

    switch (GET_AH()) {

    // all those functions return SUCCESS
    case 0x00: /* disk controller reset */
    case 0x09: /* initialize drive parameters */
    case 0x0c: /* seek to specified cylinder */
    case 0x0d: /* alternate disk reset */
    case 0x10: /* check drive ready */
    case 0x11: /* recalibrate */
    case 0x14: /* controller internal diagnostic */
    case 0x16: /* detect disk change */
        goto int13_success;
        break;

    // all those functions return disk write-protected
    case 0x03: /* write disk sectors */
    case 0x05: /* format disk track */
    case 0x43: // IBM/MS extended write
        SET_AH(0x03);
        goto int13_fail_noah;
        break;

    case 0x01: /* read disk status */
        status = read_byte(0x0040, 0x0074);
        SET_AH(status);
        SET_DISK_RET_STATUS(0);

        /* set CF if error status read */
        if (status)
            goto int13_fail_nostatus;
        else
            goto int13_success_noah;
        break;

    case 0x15: /* read disk drive size */
        SET_AH(0x02);
        goto int13_fail_noah;
        break;

    case 0x41: // IBM/MS installation check
        BX = 0xaa55;    // install check
        SET_AH(0x30);   // EDD 2.1
        CX = 0x0007;    // ext disk access, removable and edd
        goto int13_success_noah;
        break;

    case 0x42: // IBM/MS extended read
    case 0x44: // IBM/MS verify sectors
    case 0x47: // IBM/MS extended seek

        /* Load the I13X struct pointer. */
        i13x = MK_FP(DS, SI);

        count   = i13x->count;
        segment = i13x->segment;
        offset  = i13x->offset;

        // Can't use 64 bits lba
        lba = i13x->lba2;
        if (lba != 0L) {
            BX_PANIC("%s: function %02x. Can't use 64bits lba\n", __func__, GET_AH());
            goto int13_fail;
        }

        // Get 32 bits lba
        lba = i13x->lba1;

        // If verify or seek
        if (( GET_AH() == 0x44 ) || ( GET_AH() == 0x47 ))
            goto int13_success;

        BX_DEBUG_INT13_CD("%s: read %u sectors @ LBA %lu to %04X:%04X\n",
                          __func__, count, lba, segment, offset);

        _fmemset(&atapicmd, 0, sizeof(atapicmd));
        atapicmd.command = 0x28;    // READ 10 command
        atapicmd.lba     = swap_32(lba);
        atapicmd.nsect   = swap_16(count);

        bios_dsk->drqp.nsect   = count;
        bios_dsk->drqp.sect_sz = 2048;

        status = pktacc[bios_dsk->devices[device].type](device, 12, (char __far *)&atapicmd, 0, count*2048L, ATA_DATA_IN, MK_FP(segment,offset));

        count = (uint16_t)(bios_dsk->drqp.trsfbytes >> 11);
        i13x->count = count;

        if (status != 0) {
            BX_INFO("%s: function %02x, status %02x !\n", __func__, GET_AH(), status);
            SET_AH(0x0c);
            goto int13_fail_noah;
        }

        goto int13_success;
        break;

    case 0x45: // IBM/MS lock/unlock drive
        if (GET_AL() > 2)
            goto int13_fail;

        locks = bios_dsk->devices[device].lock;

        switch (GET_AL()) {
        case 0 :  // lock
            if (locks == 0xff) {
                SET_AH(0xb4);
                SET_AL(1);
                goto int13_fail_noah;
            }
            bios_dsk->devices[device].lock = ++locks;
            SET_AL(1);
            break;
        case 1 :  // unlock
            if (locks == 0x00) {
                SET_AH(0xb0);
                SET_AL(0);
                goto int13_fail_noah;
            }
            bios_dsk->devices[device].lock = --locks;
            SET_AL(locks==0?0:1);
            break;
        case 2 :  // status
            SET_AL(locks==0?0:1);
            break;
        }
        goto int13_success;
        break;

    case 0x46: // IBM/MS eject media
        locks = bios_dsk->devices[device].lock;

        if (locks != 0) {
            SET_AH(0xb1); // media locked
            goto int13_fail_noah;
        }
        // FIXME should handle 0x31 no media in device
        // FIXME should handle 0xb5 valid request failed

#if 0 /// @todo implement!
        // Call removable media eject
        ASM_START
        push bp
        mov  bp, sp

        mov ah, #0x52
        int #0x15
        mov _int13_cdrom.status + 2[bp], ah
        jnc int13_cdrom_rme_end
        mov _int13_cdrom.status, #1
int13_cdrom_rme_end:
        pop bp
        ASM_END
#endif

        if (status != 0) {
            SET_AH(0xb1); // media locked
            goto int13_fail_noah;
        }

        goto int13_success;
        break;

    /// @todo Part of this should be merged with analogous code in disk.c
    case 0x48: // IBM/MS get drive parameters
        dpt = DS :> (dpt_t *)SI;
        size = dpt->size;

        // Buffer is too small
        if (size < 0x1a)
            goto int13_fail;

        // EDD 1.x
        if (size >= 0x1a) {
            uint16_t   blksize;

            blksize = bios_dsk->devices[device].blksize;

            dpt->size      = 0x1a;
            dpt->infos     = 0x74;  /* Removable, media change, lockable, max values */
            dpt->cylinders = 0xffffffff;
            dpt->heads     = 0xffffffff;
            dpt->spt       = 0xffffffff;
            dpt->blksize   = blksize;
            dpt->sector_count1 = 0xffffffff;  // FIXME should be Bit64
            dpt->sector_count2 = 0xffffffff;
        }

        // EDD 2.x
        if(size >= 0x1e) {
            uint8_t     channel, irq, mode, checksum, i;
            uint16_t    iobase1, iobase2, options;

            dpt->size = 0x1e;
            dpt->dpte_segment = ebda_seg;
            dpt->dpte_offset  = (uint16_t)&EbdaData->bdisk.dpte;

            // Fill in dpte
            channel = device / 2;
            iobase1 = bios_dsk->channels[channel].iobase1;
            iobase2 = bios_dsk->channels[channel].iobase2;
            irq     = bios_dsk->channels[channel].irq;
            mode    = bios_dsk->devices[device].mode;

            // FIXME atapi device
            options  = (1<<4); // lba translation
            options |= (1<<5); // removable device
            options |= (1<<6); // atapi device
#if VBOX_BIOS_CPU >= 80386
            options |= (mode==ATA_MODE_PIO32?1:0<<7);
#endif

            bios_dsk->dpte.iobase1  = iobase1;
            bios_dsk->dpte.iobase2  = iobase2;
            bios_dsk->dpte.prefix   = (0xe | (device % 2))<<4;
            bios_dsk->dpte.unused   = 0xcb;
            bios_dsk->dpte.irq      = irq;
            bios_dsk->dpte.blkcount = 1 ;
            bios_dsk->dpte.dma      = 0;
            bios_dsk->dpte.pio      = 0;
            bios_dsk->dpte.options  = options;
            bios_dsk->dpte.reserved = 0;
            bios_dsk->dpte.revision = 0x11;

            checksum = 0;
            for (i = 0; i < 15; ++i)
                checksum += read_byte(ebda_seg, (uint16_t)&EbdaData->bdisk.dpte + i);
            checksum = -checksum;
            bios_dsk->dpte.checksum = checksum;
        }

        // EDD 3.x
        if(size >= 0x42) {
            uint8_t     channel, iface, checksum, i;
            uint16_t    iobase1;

            channel = device / 2;
            iface   = bios_dsk->channels[channel].iface;
            iobase1 = bios_dsk->channels[channel].iobase1;

            dpt->size       = 0x42;
            dpt->key        = 0xbedd;
            dpt->dpi_length = 0x24;
            dpt->reserved1  = 0;
            dpt->reserved2  = 0;

            if (iface == ATA_IFACE_ISA) {
                dpt->host_bus[0] = 'I';
                dpt->host_bus[1] = 'S';
                dpt->host_bus[2] = 'A';
                dpt->host_bus[3] = ' ';
            }
            else {
                // FIXME PCI
            }
            dpt->iface_type[0] = 'A';
            dpt->iface_type[1] = 'T';
            dpt->iface_type[2] = 'A';
            dpt->iface_type[3] = ' ';
            dpt->iface_type[4] = ' ';
            dpt->iface_type[5] = ' ';
            dpt->iface_type[6] = ' ';
            dpt->iface_type[7] = ' ';

            if (iface == ATA_IFACE_ISA) {
                ((uint16_t __far *)dpt->iface_path)[0] = iobase1;
                ((uint16_t __far *)dpt->iface_path)[1] = 0;
                ((uint32_t __far *)dpt->iface_path)[1] = 0;
            }
            else {
                // FIXME PCI
            }
            ((uint16_t __far *)dpt->device_path)[0] = device & 1;
            ((uint16_t __far *)dpt->device_path)[1] = 0;
            ((uint32_t __far *)dpt->device_path)[1] = 0;

            checksum = 0;
            for (i = 30; i < 64; ++i)
                checksum += ((uint8_t __far *)dpt)[i];
            checksum = -checksum;
            dpt->checksum = checksum;
        }

        goto int13_success;
        break;

    case 0x49: // IBM/MS extended media change
        // always send changed ??
        SET_AH(06);
        goto int13_fail_nostatus;
        break;

    case 0x4e: // // IBM/MS set hardware configuration
        // DMA, prefetch, PIO maximum not supported
        switch (GET_AL()) {
        case 0x01:
        case 0x03:
        case 0x04:
        case 0x06:
            goto int13_success;
            break;
        default :
            goto int13_fail;
        }
        break;

    // all those functions return unimplemented
    case 0x02: /* read sectors */
    case 0x04: /* verify sectors */
    case 0x08: /* read disk drive parameters */
    case 0x0a: /* read disk sectors with ECC */
    case 0x0b: /* write disk sectors with ECC */
    case 0x18: /* set media type for format */
    case 0x50: // ? - send packet command
    default:
        BX_INFO("%s: unsupported AH=%02x\n", __func__, GET_AH());
        goto int13_fail;
        break;
    }

int13_fail:
    SET_AH(0x01); // defaults to invalid function in AH or invalid parameter
int13_fail_noah:
    SET_DISK_RET_STATUS(GET_AH());
int13_fail_nostatus:
    SET_CF();     // error occurred
    return;

int13_success:
    SET_AH(0x00); // no error
int13_success_noah:
    SET_DISK_RET_STATUS(0x00);
    CLEAR_CF();   // no error
    return;
}

// ---------------------------------------------------------------------------
// End of int13 for cdrom
// ---------------------------------------------------------------------------

