/*
 * Copyright (c) 2013 Rob Clark <robdclark@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef INSTR_A3XX_H_
#define INSTR_A3XX_H_

#define PACKED __attribute__((__packed__))

#include <stdint.h>
#include <assert.h>

/* size of largest OPC field of all the instruction categories: */
#define NOPC_BITS 6

#define _OPC(cat, opc)   (((cat) << NOPC_BITS) | opc)

typedef enum {
	/* category 0: */
	OPC_NOP             = _OPC(0, 0),
	OPC_BR              = _OPC(0, 1),
	OPC_JUMP            = _OPC(0, 2),
	OPC_CALL            = _OPC(0, 3),
	OPC_RET             = _OPC(0, 4),
	OPC_KILL            = _OPC(0, 5),
	OPC_END             = _OPC(0, 6),
	OPC_EMIT            = _OPC(0, 7),
	OPC_CUT             = _OPC(0, 8),
	OPC_CHMASK          = _OPC(0, 9),
	OPC_CHSH            = _OPC(0, 10),
	OPC_FLOW_REV        = _OPC(0, 11),

	/* category 1: */
	OPC_MOV             = _OPC(1, 0),

	/* category 2: */
	OPC_ADD_F           = _OPC(2, 0),
	OPC_MIN_F           = _OPC(2, 1),
	OPC_MAX_F           = _OPC(2, 2),
	OPC_MUL_F           = _OPC(2, 3),
	OPC_SIGN_F          = _OPC(2, 4),
	OPC_CMPS_F          = _OPC(2, 5),
	OPC_ABSNEG_F        = _OPC(2, 6),
	OPC_CMPV_F          = _OPC(2, 7),
	/* 8 - invalid */
	OPC_FLOOR_F         = _OPC(2, 9),
	OPC_CEIL_F          = _OPC(2, 10),
	OPC_RNDNE_F         = _OPC(2, 11),
	OPC_RNDAZ_F         = _OPC(2, 12),
	OPC_TRUNC_F         = _OPC(2, 13),
	/* 14-15 - invalid */
	OPC_ADD_U           = _OPC(2, 16),
	OPC_ADD_S           = _OPC(2, 17),
	OPC_SUB_U           = _OPC(2, 18),
	OPC_SUB_S           = _OPC(2, 19),
	OPC_CMPS_U          = _OPC(2, 20),
	OPC_CMPS_S          = _OPC(2, 21),
	OPC_MIN_U           = _OPC(2, 22),
	OPC_MIN_S           = _OPC(2, 23),
	OPC_MAX_U           = _OPC(2, 24),
	OPC_MAX_S           = _OPC(2, 25),
	OPC_ABSNEG_S        = _OPC(2, 26),
	/* 27 - invalid */
	OPC_AND_B           = _OPC(2, 28),
	OPC_OR_B            = _OPC(2, 29),
	OPC_NOT_B           = _OPC(2, 30),
	OPC_XOR_B           = _OPC(2, 31),
	/* 32 - invalid */
	OPC_CMPV_U          = _OPC(2, 33),
	OPC_CMPV_S          = _OPC(2, 34),
	/* 35-47 - invalid */
	OPC_MUL_U           = _OPC(2, 48),
	OPC_MUL_S           = _OPC(2, 49),
	OPC_MULL_U          = _OPC(2, 50),
	OPC_BFREV_B         = _OPC(2, 51),
	OPC_CLZ_S           = _OPC(2, 52),
	OPC_CLZ_B           = _OPC(2, 53),
	OPC_SHL_B           = _OPC(2, 54),
	OPC_SHR_B           = _OPC(2, 55),
	OPC_ASHR_B          = _OPC(2, 56),
	OPC_BARY_F          = _OPC(2, 57),
	OPC_MGEN_B          = _OPC(2, 58),
	OPC_GETBIT_B        = _OPC(2, 59),
	OPC_SETRM           = _OPC(2, 60),
	OPC_CBITS_B         = _OPC(2, 61),
	OPC_SHB             = _OPC(2, 62),
	OPC_MSAD            = _OPC(2, 63),

	/* category 3: */
	OPC_MAD_U16         = _OPC(3, 0),
	OPC_MADSH_U16       = _OPC(3, 1),
	OPC_MAD_S16         = _OPC(3, 2),
	OPC_MADSH_M16       = _OPC(3, 3),   /* should this be .s16? */
	OPC_MAD_U24         = _OPC(3, 4),
	OPC_MAD_S24         = _OPC(3, 5),
	OPC_MAD_F16         = _OPC(3, 6),
	OPC_MAD_F32         = _OPC(3, 7),
	OPC_SEL_B16         = _OPC(3, 8),
	OPC_SEL_B32         = _OPC(3, 9),
	OPC_SEL_S16         = _OPC(3, 10),
	OPC_SEL_S32         = _OPC(3, 11),
	OPC_SEL_F16         = _OPC(3, 12),
	OPC_SEL_F32         = _OPC(3, 13),
	OPC_SAD_S16         = _OPC(3, 14),
	OPC_SAD_S32         = _OPC(3, 15),

	/* category 4: */
	OPC_RCP             = _OPC(4, 0),
	OPC_RSQ             = _OPC(4, 1),
	OPC_LOG2            = _OPC(4, 2),
	OPC_EXP2            = _OPC(4, 3),
	OPC_SIN             = _OPC(4, 4),
	OPC_COS             = _OPC(4, 5),
	OPC_SQRT            = _OPC(4, 6),
	// 7-63 - invalid

	/* category 5: */
	OPC_ISAM            = _OPC(5, 0),
	OPC_ISAML           = _OPC(5, 1),
	OPC_ISAMM           = _OPC(5, 2),
	OPC_SAM             = _OPC(5, 3),
	OPC_SAMB            = _OPC(5, 4),
	OPC_SAML            = _OPC(5, 5),
	OPC_SAMGQ           = _OPC(5, 6),
	OPC_GETLOD          = _OPC(5, 7),
	OPC_CONV            = _OPC(5, 8),
	OPC_CONVM           = _OPC(5, 9),
	OPC_GETSIZE         = _OPC(5, 10),
	OPC_GETBUF          = _OPC(5, 11),
	OPC_GETPOS          = _OPC(5, 12),
	OPC_GETINFO         = _OPC(5, 13),
	OPC_DSX             = _OPC(5, 14),
	OPC_DSY             = _OPC(5, 15),
	OPC_GATHER4R        = _OPC(5, 16),
	OPC_GATHER4G        = _OPC(5, 17),
	OPC_GATHER4B        = _OPC(5, 18),
	OPC_GATHER4A        = _OPC(5, 19),
	OPC_SAMGP0          = _OPC(5, 20),
	OPC_SAMGP1          = _OPC(5, 21),
	OPC_SAMGP2          = _OPC(5, 22),
	OPC_SAMGP3          = _OPC(5, 23),
	OPC_DSXPP_1         = _OPC(5, 24),
	OPC_DSYPP_1         = _OPC(5, 25),
	OPC_RGETPOS         = _OPC(5, 26),
	OPC_RGETINFO        = _OPC(5, 27),

	/* category 6: */
	OPC_LDG             = _OPC(6, 0),        /* load-global */
	OPC_LDL             = _OPC(6, 1),
	OPC_LDP             = _OPC(6, 2),
	OPC_STG             = _OPC(6, 3),        /* store-global */
	OPC_STL             = _OPC(6, 4),
	OPC_STP             = _OPC(6, 5),
	OPC_STI             = _OPC(6, 6),
	OPC_G2L             = _OPC(6, 7),
	OPC_L2G             = _OPC(6, 8),
	OPC_PREFETCH        = _OPC(6, 9),
	OPC_LDLW            = _OPC(6, 10),
	OPC_STLW            = _OPC(6, 11),
	OPC_RESFMT          = _OPC(6, 14),
	OPC_RESINFO         = _OPC(6, 15),
	OPC_ATOMIC_ADD      = _OPC(6, 16),
	OPC_ATOMIC_SUB      = _OPC(6, 17),
	OPC_ATOMIC_XCHG     = _OPC(6, 18),
	OPC_ATOMIC_INC      = _OPC(6, 19),
	OPC_ATOMIC_DEC      = _OPC(6, 20),
	OPC_ATOMIC_CMPXCHG  = _OPC(6, 21),
	OPC_ATOMIC_MIN      = _OPC(6, 22),
	OPC_ATOMIC_MAX      = _OPC(6, 23),
	OPC_ATOMIC_AND      = _OPC(6, 24),
	OPC_ATOMIC_OR       = _OPC(6, 25),
	OPC_ATOMIC_XOR      = _OPC(6, 26),
	OPC_LDGB            = _OPC(6, 27),
	OPC_STGB            = _OPC(6, 28),
	OPC_STIB            = _OPC(6, 29),
	OPC_LDC             = _OPC(6, 30),
	OPC_LDLV            = _OPC(6, 31),

	/* meta instructions (category -1): */
	/* placeholder instr to mark shader inputs: */
	OPC_META_INPUT      = _OPC(-1, 0),
	OPC_META_PHI        = _OPC(-1, 1),
	/* The "fan-in" and "fan-out" instructions are used for keeping
	 * track of instructions that write to multiple dst registers
	 * (fan-out) like texture sample instructions, or read multiple
	 * consecutive scalar registers (fan-in) (bary.f, texture samp)
	 */
	OPC_META_FO         = _OPC(-1, 2),
	OPC_META_FI         = _OPC(-1, 3),

} opc_t;

#define opc_cat(opc) ((int)((opc) >> NOPC_BITS))
#define opc_op(opc)  ((unsigned)((opc) & ((1 << NOPC_BITS) - 1)))

typedef enum {
	TYPE_F16 = 0,
	TYPE_F32 = 1,
	TYPE_U16 = 2,
	TYPE_U32 = 3,
	TYPE_S16 = 4,
	TYPE_S32 = 5,
	TYPE_U8  = 6,
	TYPE_S8  = 7,  // XXX I assume?
} type_t;

static inline uint32_t type_size(type_t type)
{
	switch (type) {
	case TYPE_F32:
	case TYPE_U32:
	case TYPE_S32:
		return 32;
	case TYPE_F16:
	case TYPE_U16:
	case TYPE_S16:
		return 16;
	case TYPE_U8:
	case TYPE_S8:
		return 8;
	default:
		assert(0); /* invalid type */
		return 0;
	}
}

static inline int type_float(type_t type)
{
	return (type == TYPE_F32) || (type == TYPE_F16);
}

static inline int type_uint(type_t type)
{
	return (type == TYPE_U32) || (type == TYPE_U16) || (type == TYPE_U8);
}

static inline int type_sint(type_t type)
{
	return (type == TYPE_S32) || (type == TYPE_S16) || (type == TYPE_S8);
}

typedef union PACKED {
	/* normal gpr or const src register: */
	struct PACKED {
		uint32_t comp  : 2;
		uint32_t num   : 10;
	};
	/* for immediate val: */
	int32_t  iim_val   : 11;
	/* to make compiler happy: */
	uint32_t dummy32;
	uint32_t dummy10   : 10;
	int32_t  idummy10  : 10;
	uint32_t dummy11   : 11;
	uint32_t dummy12   : 12;
	uint32_t dummy13   : 13;
	uint32_t dummy8    : 8;
} reg_t;

/* special registers: */
#define REG_A0 61       /* address register */
#define REG_P0 62       /* predicate register */

static inline int reg_special(reg_t reg)
{
	return (reg.num == REG_A0) || (reg.num == REG_P0);
}

typedef struct PACKED {
	/* dword0: */
	union PACKED {
		struct PACKED {
			int16_t  immed    : 16;
			uint32_t dummy1   : 16;
		} a3xx;
		struct PACKED {
			int32_t  immed    : 20;
			uint32_t dummy1   : 12;
		} a4xx;
		struct PACKED {
			uint32_t immed    : 32;
		} a5xx;
	};

	/* dword1: */
	uint32_t dummy2   : 8;
	uint32_t repeat   : 3;
	uint32_t dummy3   : 1;
	uint32_t ss       : 1;
	uint32_t dummy4   : 7;
	uint32_t inv      : 1;
	uint32_t comp     : 2;
	uint32_t opc      : 4;
	uint32_t jmp_tgt  : 1;
	uint32_t sync     : 1;
	uint32_t opc_cat  : 3;
} instr_cat0_t;

typedef struct PACKED {
	/* dword0: */
	union PACKED {
		/* for normal src register: */
		struct PACKED {
			uint32_t src : 11;
			/* at least low bit of pad must be zero or it will
			 * look like a address relative src
			 */
			uint32_t pad : 21;
		};
		/* for address relative: */
		struct PACKED {
			int32_t  off : 10;
			uint32_t src_rel_c : 1;
			uint32_t src_rel : 1;
			uint32_t unknown : 20;
		};
		/* for immediate: */
		int32_t  iim_val;
		uint32_t uim_val;
		float    fim_val;
	};

	/* dword1: */
	uint32_t dst        : 8;
	uint32_t repeat     : 3;
	uint32_t src_r      : 1;
	uint32_t ss         : 1;
	uint32_t ul         : 1;
	uint32_t dst_type   : 3;
	uint32_t dst_rel    : 1;
	uint32_t src_type   : 3;
	uint32_t src_c      : 1;
	uint32_t src_im     : 1;
	uint32_t even       : 1;
	uint32_t pos_inf    : 1;
	uint32_t must_be_0  : 2;
	uint32_t jmp_tgt    : 1;
	uint32_t sync       : 1;
	uint32_t opc_cat    : 3;
} instr_cat1_t;

typedef struct PACKED {
	/* dword0: */
	union PACKED {
		struct PACKED {
			uint32_t src1         : 11;
			uint32_t must_be_zero1: 2;
			uint32_t src1_im      : 1;   /* immediate */
			uint32_t src1_neg     : 1;   /* negate */
			uint32_t src1_abs     : 1;   /* absolute value */
		};
		struct PACKED {
			uint32_t src1         : 10;
			uint32_t src1_c       : 1;   /* relative-const */
			uint32_t src1_rel     : 1;   /* relative address */
			uint32_t must_be_zero : 1;
			uint32_t dummy        : 3;
		} rel1;
		struct PACKED {
			uint32_t src1         : 12;
			uint32_t src1_c       : 1;   /* const */
			uint32_t dummy        : 3;
		} c1;
	};

	union PACKED {
		struct PACKED {
			uint32_t src2         : 11;
			uint32_t must_be_zero2: 2;
			uint32_t src2_im      : 1;   /* immediate */
			uint32_t src2_neg     : 1;   /* negate */
			uint32_t src2_abs     : 1;   /* absolute value */
		};
		struct PACKED {
			uint32_t src2         : 10;
			uint32_t src2_c       : 1;   /* relative-const */
			uint32_t src2_rel     : 1;   /* relative address */
			uint32_t must_be_zero : 1;
			uint32_t dummy        : 3;
		} rel2;
		struct PACKED {
			uint32_t src2         : 12;
			uint32_t src2_c       : 1;   /* const */
			uint32_t dummy        : 3;
		} c2;
	};

	/* dword1: */
	uint32_t dst      : 8;
	uint32_t repeat   : 3;
	uint32_t src1_r   : 1;
	uint32_t ss       : 1;
	uint32_t ul       : 1;   /* dunno */
	uint32_t dst_half : 1;   /* or widen/narrow.. ie. dst hrN <-> rN */
	uint32_t ei       : 1;
	uint32_t cond     : 3;
	uint32_t src2_r   : 1;
	uint32_t full     : 1;   /* not half */
	uint32_t opc      : 6;
	uint32_t jmp_tgt  : 1;
	uint32_t sync     : 1;
	uint32_t opc_cat  : 3;
} instr_cat2_t;

typedef struct PACKED {
	/* dword0: */
	union PACKED {
		struct PACKED {
			uint32_t src1         : 11;
			uint32_t must_be_zero1: 2;
			uint32_t src2_c       : 1;
			uint32_t src1_neg     : 1;
			uint32_t src2_r       : 1;
		};
		struct PACKED {
			uint32_t src1         : 10;
			uint32_t src1_c       : 1;
			uint32_t src1_rel     : 1;
			uint32_t must_be_zero : 1;
			uint32_t dummy        : 3;
		} rel1;
		struct PACKED {
			uint32_t src1         : 12;
			uint32_t src1_c       : 1;
			uint32_t dummy        : 3;
		} c1;
	};

	union PACKED {
		struct PACKED {
			uint32_t src3         : 11;
			uint32_t must_be_zero2: 2;
			uint32_t src3_r       : 1;
			uint32_t src2_neg     : 1;
			uint32_t src3_neg     : 1;
		};
		struct PACKED {
			uint32_t src3         : 10;
			uint32_t src3_c       : 1;
			uint32_t src3_rel     : 1;
			uint32_t must_be_zero : 1;
			uint32_t dummy        : 3;
		} rel2;
		struct PACKED {
			uint32_t src3         : 12;
			uint32_t src3_c       : 1;
			uint32_t dummy        : 3;
		} c2;
	};

	/* dword1: */
	uint32_t dst      : 8;
	uint32_t repeat   : 3;
	uint32_t src1_r   : 1;
	uint32_t ss       : 1;
	uint32_t ul       : 1;
	uint32_t dst_half : 1;   /* or widen/narrow.. ie. dst hrN <-> rN */
	uint32_t src2     : 8;
	uint32_t opc      : 4;
	uint32_t jmp_tgt  : 1;
	uint32_t sync     : 1;
	uint32_t opc_cat  : 3;
} instr_cat3_t;

static inline bool instr_cat3_full(instr_cat3_t *cat3)
{
	switch (_OPC(3, cat3->opc)) {
	case OPC_MAD_F16:
	case OPC_MAD_U16:
	case OPC_MAD_S16:
	case OPC_SEL_B16:
	case OPC_SEL_S16:
	case OPC_SEL_F16:
	case OPC_SAD_S16:
	case OPC_SAD_S32:  // really??
		return false;
	default:
		return true;
	}
}

typedef struct PACKED {
	/* dword0: */
	union PACKED {
		struct PACKED {
			uint32_t src          : 11;
			uint32_t must_be_zero1: 2;
			uint32_t src_im       : 1;   /* immediate */
			uint32_t src_neg      : 1;   /* negate */
			uint32_t src_abs      : 1;   /* absolute value */
		};
		struct PACKED {
			uint32_t src          : 10;
			uint32_t src_c        : 1;   /* relative-const */
			uint32_t src_rel      : 1;   /* relative address */
			uint32_t must_be_zero : 1;
			uint32_t dummy        : 3;
		} rel;
		struct PACKED {
			uint32_t src          : 12;
			uint32_t src_c        : 1;   /* const */
			uint32_t dummy        : 3;
		} c;
	};
	uint32_t dummy1   : 16;  /* seem to be ignored */

	/* dword1: */
	uint32_t dst      : 8;
	uint32_t repeat   : 3;
	uint32_t src_r    : 1;
	uint32_t ss       : 1;
	uint32_t ul       : 1;
	uint32_t dst_half : 1;   /* or widen/narrow.. ie. dst hrN <-> rN */
	uint32_t dummy2   : 5;   /* seem to be ignored */
	uint32_t full     : 1;   /* not half */
	uint32_t opc      : 6;
	uint32_t jmp_tgt  : 1;
	uint32_t sync     : 1;
	uint32_t opc_cat  : 3;
} instr_cat4_t;

typedef struct PACKED {
	/* dword0: */
	union PACKED {
		/* normal case: */
		struct PACKED {
			uint32_t full     : 1;   /* not half */
			uint32_t src1     : 8;
			uint32_t src2     : 8;
			uint32_t dummy1   : 4;   /* seem to be ignored */
			uint32_t samp     : 4;
			uint32_t tex      : 7;
		} norm;
		/* s2en case: */
		struct PACKED {
			uint32_t full     : 1;   /* not half */
			uint32_t src1     : 8;
			uint32_t src2     : 11;
			uint32_t dummy1   : 1;
			uint32_t src3     : 8;
			uint32_t dummy2   : 3;
		} s2en;
		/* same in either case: */
		// XXX I think, confirm this
		struct PACKED {
			uint32_t full     : 1;   /* not half */
			uint32_t src1     : 8;
			uint32_t pad      : 23;
		};
	};

	/* dword1: */
	uint32_t dst      : 8;
	uint32_t wrmask   : 4;   /* write-mask */
	uint32_t type     : 3;
	uint32_t dummy2   : 1;   /* seems to be ignored */
	uint32_t is_3d    : 1;

	uint32_t is_a     : 1;
	uint32_t is_s     : 1;
	uint32_t is_s2en  : 1;
	uint32_t is_o     : 1;
	uint32_t is_p     : 1;

	uint32_t opc      : 5;
	uint32_t jmp_tgt  : 1;
	uint32_t sync     : 1;
	uint32_t opc_cat  : 3;
} instr_cat5_t;

/* dword0 encoding for src_off: [src1 + off], src2: */
typedef struct PACKED {
	/* dword0: */
	uint32_t mustbe1  : 1;
	int32_t  off      : 13;
	uint32_t src1     : 8;
	uint32_t src1_im  : 1;
	uint32_t src2_im  : 1;
	uint32_t src2     : 8;

	/* dword1: */
	uint32_t dword1;
} instr_cat6a_t;

/* dword0 encoding for !src_off: [src1], src2 */
typedef struct PACKED {
	/* dword0: */
	uint32_t mustbe0  : 1;
	uint32_t src1     : 13;
	uint32_t ignore0  : 8;
	uint32_t src1_im  : 1;
	uint32_t src2_im  : 1;
	uint32_t src2     : 8;

	/* dword1: */
	uint32_t dword1;
} instr_cat6b_t;

/* dword1 encoding for dst_off: */
typedef struct PACKED {
	/* dword0: */
	uint32_t dword0;

	/* note: there is some weird stuff going on where sometimes
	 * cat6->a.off is involved.. but that seems like a bug in
	 * the blob, since it is used even if !cat6->src_off
	 * It would make sense for there to be some more bits to
	 * bring us to 11 bits worth of offset, but not sure..
	 */
	int32_t off       : 8;
	uint32_t mustbe1  : 1;
	uint32_t dst      : 8;
	uint32_t pad1     : 15;
} instr_cat6c_t;

/* dword1 encoding for !dst_off: */
typedef struct PACKED {
	/* dword0: */
	uint32_t dword0;

	uint32_t dst      : 8;
	uint32_t mustbe0  : 1;
	uint32_t idx      : 8;
	uint32_t pad0     : 15;
} instr_cat6d_t;

/* ldgb and atomics.. atomics use 3rd src and pad0=1, pad3=3.  For
 * ldgb pad0=0, pad3=2
 */
typedef struct PACKED {
	/* dword0: */
	uint32_t pad0     : 1;
	uint32_t src3     : 8;
	uint32_t d        : 2;
	uint32_t typed    : 1;
	uint32_t type_size : 2;
	uint32_t src1     : 8;
	uint32_t src1_im  : 1;
	uint32_t src2_im  : 1;
	uint32_t src2     : 8;

	/* dword1: */
	uint32_t dst      : 8;
	uint32_t mustbe0  : 1;
	uint32_t src_ssbo : 8;
	uint32_t pad2     : 3;  // type
	uint32_t pad3     : 2;
	uint32_t pad4     : 10; // opc/jmp_tgt/sync/opc_cat
} instr_cat6ldgb_t;

/* stgb, pad0=0, pad3=2
 */
typedef struct PACKED {
	/* dword0: */
	uint32_t mustbe1  : 1;  // ???
	uint32_t src1     : 8;
	uint32_t d        : 2;
	uint32_t typed    : 1;
	uint32_t type_size : 2;
	uint32_t pad0     : 9;
	uint32_t src2_im  : 1;
	uint32_t src2     : 8;

	/* dword1: */
	uint32_t src3     : 8;
	uint32_t src3_im  : 1;
	uint32_t dst_ssbo : 8;
	uint32_t pad2     : 3;  // type
	uint32_t pad3     : 2;
	uint32_t pad4     : 10; // opc/jmp_tgt/sync/opc_cat
} instr_cat6stgb_t;

typedef union PACKED {
	instr_cat6a_t a;
	instr_cat6b_t b;
	instr_cat6c_t c;
	instr_cat6d_t d;
	instr_cat6ldgb_t ldgb;
	instr_cat6stgb_t stgb;
	struct PACKED {
		/* dword0: */
		uint32_t src_off  : 1;
		uint32_t pad1     : 31;

		/* dword1: */
		uint32_t pad2     : 8;
		uint32_t dst_off  : 1;
		uint32_t pad3     : 8;
		uint32_t type     : 3;
		uint32_t g        : 1;  /* or in some cases it means dst immed */
		uint32_t pad4     : 1;
		uint32_t opc      : 5;
		uint32_t jmp_tgt  : 1;
		uint32_t sync     : 1;
		uint32_t opc_cat  : 3;
	};
} instr_cat6_t;

typedef union PACKED {
	instr_cat0_t cat0;
	instr_cat1_t cat1;
	instr_cat2_t cat2;
	instr_cat3_t cat3;
	instr_cat4_t cat4;
	instr_cat5_t cat5;
	instr_cat6_t cat6;
	struct PACKED {
		/* dword0: */
		uint64_t pad1     : 40;
		uint32_t repeat   : 3;  /* cat0-cat4 */
		uint32_t pad2     : 1;
		uint32_t ss       : 1;  /* cat1-cat4 (cat0??) */
		uint32_t ul       : 1;  /* cat2-cat4 (and cat1 in blob.. which may be bug??) */
		uint32_t pad3     : 13;
		uint32_t jmp_tgt  : 1;
		uint32_t sync     : 1;
		uint32_t opc_cat  : 3;

	};
} instr_t;

static inline uint32_t instr_opc(instr_t *instr)
{
	switch (instr->opc_cat) {
	case 0:  return instr->cat0.opc;
	case 1:  return 0;
	case 2:  return instr->cat2.opc;
	case 3:  return instr->cat3.opc;
	case 4:  return instr->cat4.opc;
	case 5:  return instr->cat5.opc;
	case 6:  return instr->cat6.opc;
	default: return 0;
	}
}

static inline bool is_mad(opc_t opc)
{
	switch (opc) {
	case OPC_MAD_U16:
	case OPC_MAD_S16:
	case OPC_MAD_U24:
	case OPC_MAD_S24:
	case OPC_MAD_F16:
	case OPC_MAD_F32:
		return true;
	default:
		return false;
	}
}

static inline bool is_madsh(opc_t opc)
{
	switch (opc) {
	case OPC_MADSH_U16:
	case OPC_MADSH_M16:
		return true;
	default:
		return false;
	}
}

static inline bool is_atomic(opc_t opc)
{
	switch (opc) {
	case OPC_ATOMIC_ADD:
	case OPC_ATOMIC_SUB:
	case OPC_ATOMIC_XCHG:
	case OPC_ATOMIC_INC:
	case OPC_ATOMIC_DEC:
	case OPC_ATOMIC_CMPXCHG:
	case OPC_ATOMIC_MIN:
	case OPC_ATOMIC_MAX:
	case OPC_ATOMIC_AND:
	case OPC_ATOMIC_OR:
	case OPC_ATOMIC_XOR:
		return true;
	default:
		return false;
	}
}

#endif /* INSTR_A3XX_H_ */
