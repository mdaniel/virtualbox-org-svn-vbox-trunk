
#ifndef VBOX_INCLUDED_SRC_Graphics_BIOS_vgabios_h
#define VBOX_INCLUDED_SRC_Graphics_BIOS_vgabios_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* Types */
//typedef unsigned char  Bit8u;
//typedef unsigned short Bit16u;
//typedef unsigned long  Bit32u;
typedef unsigned short Boolean;

/* Defines */

#define SET_AL(val8) AX = ((AX & 0xff00) | (val8))
#define SET_BL(val8) BX = ((BX & 0xff00) | (val8))
#define SET_CL(val8) CX = ((CX & 0xff00) | (val8))
#define SET_DL(val8) DX = ((DX & 0xff00) | (val8))
#define SET_AH(val8) AX = ((AX & 0x00ff) | ((val8) << 8))
#define SET_BH(val8) BX = ((BX & 0x00ff) | ((val8) << 8))
#define SET_CH(val8) CX = ((CX & 0x00ff) | ((val8) << 8))
#define SET_DH(val8) DX = ((DX & 0x00ff) | ((val8) << 8))

#define GET_AL() ( AX & 0x00ff )
#define GET_BL() ( BX & 0x00ff )
#define GET_CL() ( CX & 0x00ff )
#define GET_DL() ( DX & 0x00ff )
#define GET_AH() ( AX >> 8 )
#define GET_BH() ( BX >> 8 )
#define GET_CH() ( CX >> 8 )
#define GET_DH() ( DX >> 8 )

#define SET_CF()     FLAGS |= 0x0001
#define CLEAR_CF()   FLAGS &= 0xfffe
#define GET_CF()     (FLAGS & 0x0001)

#define SET_ZF()     FLAGS |= 0x0040
#define CLEAR_ZF()   FLAGS &= 0xffbf
#define GET_ZF()     (FLAGS & 0x0040)

#define SCROLL_DOWN 0
#define SCROLL_UP   1
#define NO_ATTR     2
#define WITH_ATTR   3

#define SCREEN_SIZE(x,y) (((x*y*2)|0x00ff)+1)
#define SCREEN_MEM_START(x,y,p) ((((x*y*2)|0x00ff)+1)*p)
#define SCREEN_IO_START(x,y,p) ((((x*y)|0x00ff)+1)*p)

/* Macro for stack-based pointers. */
#define STACK_BASED _based(_segname("_STACK"))

/* Output. */
extern void __cdecl printf(char *s, ...);

/* VGA BIOS routines called by VBE. */
extern void biosfn_set_video_mode(uint8_t mode);
extern uint16_t biosfn_read_video_state_size2(uint16_t state);
extern uint16_t biosfn_save_video_state(uint16_t CX, uint16_t ES, uint16_t BX);
extern uint16_t biosfn_restore_video_state(uint16_t CX, uint16_t ES, uint16_t BX);

/* Allow stand-alone compilation. */
#ifndef VBOX_VERSION_STRING
#include <VBox/version.h>
#endif

#endif /* !VBOX_INCLUDED_SRC_Graphics_BIOS_vgabios_h */
