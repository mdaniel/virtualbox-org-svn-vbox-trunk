/*
 * internal execution defines for qemu
 *
 *  Copyright (c) 2003 Fabrice Bellard
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

/*
 * Oracle LGPL Disclaimer: For the avoidance of doubt, except that if any license choice
 * other than GPL or LGPL is available it will apply instead, Oracle elects to use only
 * the Lesser General Public License version 2.1 (LGPLv2) at this time for any software where
 * a choice of LGPL license versions is made available with the language indicating
 * that LGPLv2 or any later version may be used, or where a choice of which version
 * of the LGPL is applied is otherwise unspecified.
 */

#ifndef _EXEC_ALL_H_
#define _EXEC_ALL_H_

#include "qemu-common.h"
#ifdef VBOX
# include <VBox/vmm/tm.h>
# include <VBox/vmm/pgm.h> /* PGM_DYNAMIC_RAM_ALLOC */
# include <VBox/vmm/em.h>  /* EMRemIsLockOwner */
# ifndef LOG_GROUP
#  define LOG_GROUP LOG_GROUP_REM
# endif
# include <VBox/log.h>
# include "REMInternal.h"
# include <VBox/vmm/vm.h>
#endif /* VBOX */

/* allow to see translation results - the slowdown should be negligible, so we leave it */
#ifndef VBOX
#define DEBUG_DISAS
#endif /* !VBOX */

/* Page tracking code uses ram addresses in system mode, and virtual
   addresses in userspace mode.  Define tb_page_addr_t to be an appropriate
   type.  */
#if defined(CONFIG_USER_ONLY)
typedef abi_ulong tb_page_addr_t;
#else
typedef ram_addr_t tb_page_addr_t;
#endif

/* is_jmp field values */
#define DISAS_NEXT    0 /* next instruction can be analyzed */
#define DISAS_JUMP    1 /* only pc was modified dynamically */
#define DISAS_UPDATE  2 /* cpu state was modified dynamically */
#define DISAS_TB_JUMP 3 /* only pc was modified statically */

typedef struct TranslationBlock TranslationBlock;

/* XXX: make safe guess about sizes */
#define MAX_OP_PER_INSTR 266

#if HOST_LONG_BITS == 32
#define MAX_OPC_PARAM_PER_ARG 2
#else
#define MAX_OPC_PARAM_PER_ARG 1
#endif
#define MAX_OPC_PARAM_IARGS 4
#define MAX_OPC_PARAM_OARGS 1
#define MAX_OPC_PARAM_ARGS (MAX_OPC_PARAM_IARGS + MAX_OPC_PARAM_OARGS)

/* A Call op needs up to 4 + 2N parameters on 32-bit archs,
 * and up to 4 + N parameters on 64-bit archs
 * (N = number of input arguments + output arguments).  */
#define MAX_OPC_PARAM (4 + (MAX_OPC_PARAM_PER_ARG * MAX_OPC_PARAM_ARGS))
#define OPC_BUF_SIZE 640
#define OPC_MAX_SIZE (OPC_BUF_SIZE - MAX_OP_PER_INSTR)

/* Maximum size a TCG op can expand to.  This is complicated because a
   single op may require several host instructions and register reloads.
   For now take a wild guess at 192 bytes, which should allow at least
   a couple of fixup instructions per argument.  */
#define TCG_MAX_OP_SIZE 192

#define OPPARAM_BUF_SIZE (OPC_BUF_SIZE * MAX_OPC_PARAM)

extern target_ulong gen_opc_pc[OPC_BUF_SIZE];
extern uint8_t gen_opc_instr_start[OPC_BUF_SIZE];
extern uint16_t gen_opc_icount[OPC_BUF_SIZE];

#include "qemu-log.h"

void gen_intermediate_code(CPUState *env, struct TranslationBlock *tb);
void gen_intermediate_code_pc(CPUState *env, struct TranslationBlock *tb);
void gen_pc_load(CPUState *env, struct TranslationBlock *tb,
                 uintptr_t searched_pc, int pc_pos, void *puc);

void cpu_gen_init(void);
int cpu_gen_code(CPUState *env, struct TranslationBlock *tb,
                 int *gen_code_size_ptr);
int cpu_restore_state(struct TranslationBlock *tb,
                      CPUState *env, uintptr_t searched_pc,
                      void *puc);
void cpu_resume_from_signal(CPUState *env1, void *puc);
void cpu_io_recompile(CPUState *env, void *retaddr);
TranslationBlock *tb_gen_code(CPUState *env,
                              target_ulong pc, target_ulong cs_base, int flags,
                              int cflags);
void cpu_exec_init(CPUState *env);
void QEMU_NORETURN cpu_loop_exit(void);
int page_unprotect(target_ulong address, uintptr_t pc, void *puc);
void tb_invalidate_phys_page_range(tb_page_addr_t start, tb_page_addr_t end,
                                   int is_cpu_write_access);
void tb_invalidate_page_range(target_ulong start, target_ulong end);
void tlb_flush_page(CPUState *env, target_ulong addr);
void tlb_flush(CPUState *env, int flush_global);
#if !defined(CONFIG_USER_ONLY)
void tlb_set_page(CPUState *env, target_ulong vaddr,
                  target_phys_addr_t paddr, int prot,
                  int mmu_idx, target_ulong size);
#endif

#define CODE_GEN_ALIGN           16 /* must be >= of the size of a icache line */

#define CODE_GEN_PHYS_HASH_BITS     15
#define CODE_GEN_PHYS_HASH_SIZE     (1 << CODE_GEN_PHYS_HASH_BITS)

#define MIN_CODE_GEN_BUFFER_SIZE     (1024 * 1024)

/* estimated block size for TB allocation */
/* XXX: use a per code average code fragment size and modulate it
   according to the host CPU */
#if defined(CONFIG_SOFTMMU)
#define CODE_GEN_AVG_BLOCK_SIZE 128
#else
#define CODE_GEN_AVG_BLOCK_SIZE 64
#endif

#if defined(_ARCH_PPC) || defined(__x86_64__) || defined(__arm__) || defined(__i386__)
#define USE_DIRECT_JUMP
#endif

#ifdef VBOX /* bird: not safe in next step because of threading & cpu_interrupt. */
# undef USE_DIRECT_JUMP
#endif /* VBOX */

struct TranslationBlock {
    target_ulong pc;   /* simulated PC corresponding to this block (EIP + CS base) */
    target_ulong cs_base; /* CS base for this block */
    uint64_t flags; /* flags defining in which context the code was generated */
    uint16_t size;      /* size of target code for this block (1 <=
                           size <= TARGET_PAGE_SIZE) */
    uint16_t cflags;    /* compile flags */
#define CF_COUNT_MASK  0x7fff
#define CF_LAST_IO     0x8000 /* Last insn may be an IO access.  */
#ifdef VBOX
# define CF_RAW_MODE   0x0010 /* block was generated in raw mode */
#endif

    uint8_t *tc_ptr;    /* pointer to the translated code */
    /* next matching tb for physical address. */
    struct TranslationBlock *phys_hash_next;
    /* first and second physical page containing code. The lower bit
       of the pointer tells the index in page_next[] */
    struct TranslationBlock *page_next[2];
    tb_page_addr_t page_addr[2];

    /* the following data are used to directly call another TB from
       the code of this one. */
    uint16_t tb_next_offset[2]; /* offset of original jump target */
#ifdef USE_DIRECT_JUMP
    uint16_t tb_jmp_offset[2]; /* offset of jump instruction */
#else
    uintptr_t tb_next[2]; /* address of jump generated code */
#endif
    /* list of TBs jumping to this one. This is a circular list using
       the two least significant bits of the pointers to tell what is
       the next pointer: 0 = jmp_next[0], 1 = jmp_next[1], 2 =
       jmp_first */
    struct TranslationBlock *jmp_next[2];
    struct TranslationBlock *jmp_first;
    uint32_t icount;
};

static inline unsigned int tb_jmp_cache_hash_page(target_ulong pc)
{
    target_ulong tmp;
    tmp = pc ^ (pc >> (TARGET_PAGE_BITS - TB_JMP_PAGE_BITS));
    return (tmp >> (TARGET_PAGE_BITS - TB_JMP_PAGE_BITS)) & TB_JMP_PAGE_MASK;
}

static inline unsigned int tb_jmp_cache_hash_func(target_ulong pc)
{
    target_ulong tmp;
    tmp = pc ^ (pc >> (TARGET_PAGE_BITS - TB_JMP_PAGE_BITS));
    return (((tmp >> (TARGET_PAGE_BITS - TB_JMP_PAGE_BITS)) & TB_JMP_PAGE_MASK)
	    | (tmp & TB_JMP_ADDR_MASK));
}

static inline unsigned int tb_phys_hash_func(tb_page_addr_t pc)
{
    return pc & (CODE_GEN_PHYS_HASH_SIZE - 1);
}

TranslationBlock *tb_alloc(target_ulong pc);
void tb_free(TranslationBlock *tb);
void tb_flush(CPUState *env);
void tb_link_page(TranslationBlock *tb,
                  tb_page_addr_t phys_pc, tb_page_addr_t phys_page2);
void tb_phys_invalidate(TranslationBlock *tb, tb_page_addr_t page_addr);

extern TranslationBlock *tb_phys_hash[CODE_GEN_PHYS_HASH_SIZE];

#if defined(USE_DIRECT_JUMP)

#if defined(_ARCH_PPC)
extern void ppc_tb_set_jmp_target(uintptr_t jmp_addr, uintptr_t addr);
#define tb_set_jmp_target1 ppc_tb_set_jmp_target
#elif defined(__i386__) || defined(__x86_64__)
static inline void tb_set_jmp_target1(uintptr_t jmp_addr, uintptr_t addr)
{
    /* patch the branch destination */
    *(uint32_t *)jmp_addr = addr - (jmp_addr + 4);
    /* no need to flush icache explicitly */
}
#elif defined(__arm__)
static inline void tb_set_jmp_target1(uintptr_t jmp_addr, uintptr_t addr)
{
#if QEMU_GNUC_PREREQ(4, 1)
    void __clear_cache(char *beg, char *end);
#else
    register unsigned long _beg __asm ("a1");
    register unsigned long _end __asm ("a2");
    register unsigned long _flg __asm ("a3");
#endif

    /* we could use a ldr pc, [pc, #-4] kind of branch and avoid the flush */
    *(uint32_t *)jmp_addr =
        (*(uint32_t *)jmp_addr & ~0xffffff)
        | (((addr - (jmp_addr + 8)) >> 2) & 0xffffff);

#if QEMU_GNUC_PREREQ(4, 1)
    __clear_cache((char *) jmp_addr, (char *) jmp_addr + 4);
#else
    /* flush icache */
    _beg = jmp_addr;
    _end = jmp_addr + 4;
    _flg = 0;
    __asm __volatile__ ("swi 0x9f0002" : : "r" (_beg), "r" (_end), "r" (_flg));
#endif
}
#endif

static inline void tb_set_jmp_target(TranslationBlock *tb,
                                     int n, uintptr_t addr)
{
    uintptr_t offset;

    offset = tb->tb_jmp_offset[n];
    tb_set_jmp_target1((uintptr_t)(tb->tc_ptr + offset), addr);
}

#else

/* set the jump target */
static inline void tb_set_jmp_target(TranslationBlock *tb,
                                     int n, uintptr_t addr)
{
    tb->tb_next[n] = addr;
}

#endif

static inline void tb_add_jump(TranslationBlock *tb, int n,
                               TranslationBlock *tb_next)
{
    /* NOTE: this test is only needed for thread safety */
    if (!tb->jmp_next[n]) {
        /* patch the native jump address */
        tb_set_jmp_target(tb, n, (uintptr_t)tb_next->tc_ptr);

        /* add in TB jmp circular list */
        tb->jmp_next[n] = tb_next->jmp_first;
        tb_next->jmp_first = (TranslationBlock *)((intptr_t)(tb) | (n));
    }
}

TranslationBlock *tb_find_pc(uintptr_t pc_ptr);

#include "qemu-lock.h"

extern spinlock_t tb_lock;

extern int tb_invalidated_flag;

#if !defined(CONFIG_USER_ONLY)

extern CPUWriteMemoryFunc *io_mem_write[IO_MEM_NB_ENTRIES][4];
extern CPUReadMemoryFunc *io_mem_read[IO_MEM_NB_ENTRIES][4];
extern void *io_mem_opaque[IO_MEM_NB_ENTRIES];

void tlb_fill(target_ulong addr, int is_write, int mmu_idx,
              void *retaddr);

#include "softmmu_defs.h"

#define ACCESS_TYPE (NB_MMU_MODES + 1)
#define MEMSUFFIX _code
#define env cpu_single_env

#define DATA_SIZE 1
#include "softmmu_header.h"

#define DATA_SIZE 2
#include "softmmu_header.h"

#define DATA_SIZE 4
#include "softmmu_header.h"

#define DATA_SIZE 8
#include "softmmu_header.h"

#undef ACCESS_TYPE
#undef MEMSUFFIX
#undef env

#endif

#if defined(CONFIG_USER_ONLY)
static inline tb_page_addr_t get_page_addr_code(CPUState *env1, target_ulong addr)
{
    return addr;
}
#else
# ifdef VBOX
target_ulong remR3PhysGetPhysicalAddressCode(CPUState *env, target_ulong addr, CPUTLBEntry *pTLBEntry, target_phys_addr_t ioTLBEntry);
# endif
/* NOTE: this function can trigger an exception */
/* NOTE2: the returned address is not exactly the physical address: it
   is the offset relative to phys_ram_base */
static inline tb_page_addr_t get_page_addr_code(CPUState *env1, target_ulong addr)
{
    int mmu_idx, page_index, pd;
# ifndef VBOX
    void *p;
# endif

    page_index = (addr >> TARGET_PAGE_BITS) & (CPU_TLB_SIZE - 1);
    mmu_idx = cpu_mmu_index(env1);
    if (unlikely(env1->tlb_table[mmu_idx][page_index].addr_code !=
                 (addr & TARGET_PAGE_MASK))) {
        ldub_code(addr);
    }
    pd = env1->tlb_table[mmu_idx][page_index].addr_code & ~TARGET_PAGE_MASK;
    if (pd > IO_MEM_ROM && !(pd & IO_MEM_ROMD)) {
# ifdef VBOX
        /* deal with non-MMIO access handlers. */
        return remR3PhysGetPhysicalAddressCode(env1, addr,
                                               &env1->tlb_table[mmu_idx][page_index],
                                               env1->iotlb[mmu_idx][page_index]);
# elif defined(TARGET_SPARC) || defined(TARGET_MIPS)
        do_unassigned_access(addr, 0, 1, 0, 4);
#else
        cpu_abort(env1, "Trying to execute code outside RAM or ROM at 0x" TARGET_FMT_lx "\n", addr);
#endif
    }
# if defined(VBOX) && defined(REM_PHYS_ADDR_IN_TLB)
    return addr + env1->tlb_table[mmu_idx][page_index].addend;
# elif defined(VBOX)
    Assert(env1->phys_addends[mmu_idx][page_index] != -1);
    return addr + env1->phys_addends[mmu_idx][page_index];
# else
    p = (void *)(uintptr_t)addr
        + env1->tlb_table[mmu_idx][page_index].addend;
    return qemu_ram_addr_from_host(p);
# endif
}
#endif

typedef void (CPUDebugExcpHandler)(CPUState *env);

CPUDebugExcpHandler *cpu_set_debug_excp_handler(CPUDebugExcpHandler *handler);

#ifndef VBOX
/* vl.c */
extern int singlestep;

/* cpu-exec.c */
extern volatile sig_atomic_t exit_request;
#endif /*!VBOX*/


#endif
