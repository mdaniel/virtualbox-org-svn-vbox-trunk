/*
 *  virtual page mapping and translated block handling
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

#include "config.h"
#ifndef VBOX
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/types.h>
#include <sys/mman.h>
#endif
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <inttypes.h>
#else /* VBOX */
# include <stdlib.h>
# include <stdio.h>
# include <iprt/alloc.h>
# include <iprt/string.h>
# include <iprt/param.h>
# include <VBox/vmm/pgm.h> /* PGM_DYNAMIC_RAM_ALLOC */
# include <VBox/err.h>
#endif /* VBOX */

#include "cpu.h"
#include "exec-all.h"
#include "qemu-common.h"
#include "tcg.h"
#ifndef VBOX
#include "hw/hw.h"
#include "hw/qdev.h"
#endif /* !VBOX */
#include "osdep.h"
#include "kvm.h"
#include "qemu-timer.h"
#if defined(CONFIG_USER_ONLY)
#include <qemu.h>
#include <signal.h>
#if defined(__FreeBSD__) || defined(__FreeBSD_kernel__)
#include <sys/param.h>
#if __FreeBSD_version >= 700104
#define HAVE_KINFO_GETVMMAP
#define sigqueue sigqueue_freebsd  /* avoid redefinition */
#include <sys/time.h>
#include <sys/proc.h>
#include <machine/profile.h>
#define _KERNEL
#include <sys/user.h>
#undef _KERNEL
#undef sigqueue
#include <libutil.h>
#endif
#endif
#endif

//#define DEBUG_TB_INVALIDATE
//#define DEBUG_FLUSH
//#define DEBUG_TLB
//#define DEBUG_UNASSIGNED

/* make various TB consistency checks */
//#define DEBUG_TB_CHECK
//#define DEBUG_TLB_CHECK

//#define DEBUG_IOPORT
//#define DEBUG_SUBPAGE

#if !defined(CONFIG_USER_ONLY)
/* TB consistency checks only implemented for usermode emulation.  */
#undef DEBUG_TB_CHECK
#endif

#define SMC_BITMAP_USE_THRESHOLD 10

static TranslationBlock *tbs;
static int code_gen_max_blocks;
TranslationBlock *tb_phys_hash[CODE_GEN_PHYS_HASH_SIZE];
static int nb_tbs;
/* any access to the tbs or the page table must use this lock */
spinlock_t tb_lock = SPIN_LOCK_UNLOCKED;

#ifndef VBOX
#if defined(__arm__) || defined(__sparc_v9__)
/* The prologue must be reachable with a direct jump. ARM and Sparc64
 have limited branch ranges (possibly also PPC) so place it in a
 section close to code segment. */
#define code_gen_section                                \
    __attribute__((__section__(".gen_code")))           \
    __attribute__((aligned (32)))
#elif defined(_WIN32)
/* Maximum alignment for Win32 is 16. */
#define code_gen_section                                \
    __attribute__((aligned (16)))
#else
#define code_gen_section                                \
    __attribute__((aligned (32)))
#endif

uint8_t code_gen_prologue[1024] code_gen_section;
#else /* VBOX */
extern uint8_t *code_gen_prologue;
#endif /* VBOX */
static uint8_t *code_gen_buffer;
static size_t code_gen_buffer_size;
/* threshold to flush the translated code buffer */
static size_t code_gen_buffer_max_size;
static uint8_t *code_gen_ptr;

#if !defined(CONFIG_USER_ONLY)
# ifndef VBOX
int phys_ram_fd;
static int in_migration;
# endif /* !VBOX */

RAMList ram_list = { .blocks = QLIST_HEAD_INITIALIZER(ram_list) };
#endif

CPUState *first_cpu;
/* current CPU in the current thread. It is only valid inside
   cpu_exec() */
CPUState *cpu_single_env;
/* 0 = Do not count executed instructions.
   1 = Precise instruction counting.
   2 = Adaptive rate instruction counting.  */
int use_icount = 0;
/* Current instruction counter.  While executing translated code this may
   include some instructions that have not yet been executed.  */
int64_t qemu_icount;

typedef struct PageDesc {
    /* list of TBs intersecting this ram page */
    TranslationBlock *first_tb;
    /* in order to optimize self modifying code, we count the number
       of lookups we do to a given page to use a bitmap */
    unsigned int code_write_count;
    uint8_t *code_bitmap;
#if defined(CONFIG_USER_ONLY)
    unsigned long flags;
#endif
} PageDesc;

/* In system mode we want L1_MAP to be based on ram offsets,
   while in user mode we want it to be based on virtual addresses.  */
#if !defined(CONFIG_USER_ONLY)
#if HOST_LONG_BITS < TARGET_PHYS_ADDR_SPACE_BITS
# define L1_MAP_ADDR_SPACE_BITS  HOST_LONG_BITS
#else
# define L1_MAP_ADDR_SPACE_BITS  TARGET_PHYS_ADDR_SPACE_BITS
#endif
#else
# define L1_MAP_ADDR_SPACE_BITS  TARGET_VIRT_ADDR_SPACE_BITS
#endif

/* Size of the L2 (and L3, etc) page tables.  */
#define L2_BITS 10
#define L2_SIZE (1 << L2_BITS)

/* The bits remaining after N lower levels of page tables.  */
#define P_L1_BITS_REM \
    ((TARGET_PHYS_ADDR_SPACE_BITS - TARGET_PAGE_BITS) % L2_BITS)
#define V_L1_BITS_REM \
    ((L1_MAP_ADDR_SPACE_BITS - TARGET_PAGE_BITS) % L2_BITS)

/* Size of the L1 page table.  Avoid silly small sizes.  */
#if P_L1_BITS_REM < 4
#define P_L1_BITS  (P_L1_BITS_REM + L2_BITS)
#else
#define P_L1_BITS  P_L1_BITS_REM
#endif

#if V_L1_BITS_REM < 4
#define V_L1_BITS  (V_L1_BITS_REM + L2_BITS)
#else
#define V_L1_BITS  V_L1_BITS_REM
#endif

#define P_L1_SIZE  ((target_phys_addr_t)1 << P_L1_BITS)
#define V_L1_SIZE  ((target_ulong)1 << V_L1_BITS)

#define P_L1_SHIFT (TARGET_PHYS_ADDR_SPACE_BITS - TARGET_PAGE_BITS - P_L1_BITS)
#define V_L1_SHIFT (L1_MAP_ADDR_SPACE_BITS - TARGET_PAGE_BITS - V_L1_BITS)

size_t qemu_real_host_page_size;
size_t qemu_host_page_bits;
size_t qemu_host_page_size;
uintptr_t qemu_host_page_mask;

/* This is a multi-level map on the virtual address space.
   The bottom level has pointers to PageDesc.  */
static void *l1_map[V_L1_SIZE];

#if !defined(CONFIG_USER_ONLY)
typedef struct PhysPageDesc {
    /* offset in host memory of the page + io_index in the low bits */
    ram_addr_t phys_offset;
    ram_addr_t region_offset;
} PhysPageDesc;

/* This is a multi-level map on the physical address space.
   The bottom level has pointers to PhysPageDesc.  */
static void *l1_phys_map[P_L1_SIZE];

static void io_mem_init(void);

/* io memory support */
CPUWriteMemoryFunc *io_mem_write[IO_MEM_NB_ENTRIES][4];
CPUReadMemoryFunc *io_mem_read[IO_MEM_NB_ENTRIES][4];
void *io_mem_opaque[IO_MEM_NB_ENTRIES];
static char io_mem_used[IO_MEM_NB_ENTRIES];
static int io_mem_watch;
#endif

#ifndef VBOX
/* log support */
#ifdef WIN32
static const char *logfilename = "qemu.log";
#else
static const char *logfilename = "/tmp/qemu.log";
#endif
#endif /* !VBOX */
FILE *logfile;
int loglevel;
#ifndef VBOX
static int log_append = 0;
#endif /* !VBOX */

/* statistics */
#ifndef VBOX
#if !defined(CONFIG_USER_ONLY)
static int tlb_flush_count;
#endif
static int tb_flush_count;
static int tb_phys_invalidate_count;
#else  /* VBOX - Resettable U32 stats, see VBoxRecompiler.c. */
uint32_t tlb_flush_count;
uint32_t tb_flush_count;
uint32_t tb_phys_invalidate_count;
#endif /* VBOX */

#ifndef VBOX
#ifdef _WIN32
static void map_exec(void *addr, size_t size)
{
    DWORD old_protect;
    VirtualProtect(addr, size,
                   PAGE_EXECUTE_READWRITE, &old_protect);

}
#else
static void map_exec(void *addr, size_t size)
{
    uintptr_t start, end, page_size;

    page_size = getpagesize();
    start = (uintptr_t)addr;
    start &= ~(page_size - 1);

    end = (uintptr_t)addr + size;
    end += page_size - 1;
    end &= ~(page_size - 1);

    mprotect((void *)start, end - start,
             PROT_READ | PROT_WRITE | PROT_EXEC);
}
#endif
#else /* VBOX */
static void map_exec(void *addr, size_t size)
{
    RTMemProtect(addr, size,
                 RTMEM_PROT_EXEC | RTMEM_PROT_READ | RTMEM_PROT_WRITE);
}
#endif /* VBOX */

static void page_init(void)
{
    /* NOTE: we can always suppose that qemu_host_page_size >=
       TARGET_PAGE_SIZE */
#ifdef VBOX
    RTMemProtect(code_gen_buffer, code_gen_buffer_size,
                 RTMEM_PROT_EXEC | RTMEM_PROT_READ | RTMEM_PROT_WRITE);
    qemu_real_host_page_size = PAGE_SIZE;
#else /* !VBOX */
#ifdef _WIN32
    {
        SYSTEM_INFO system_info;

        GetSystemInfo(&system_info);
        qemu_real_host_page_size = system_info.dwPageSize;
    }
#else
    qemu_real_host_page_size = getpagesize();
#endif
#endif /* !VBOX */
    if (qemu_host_page_size == 0)
        qemu_host_page_size = qemu_real_host_page_size;
    if (qemu_host_page_size < TARGET_PAGE_SIZE)
        qemu_host_page_size = TARGET_PAGE_SIZE;
    qemu_host_page_bits = 0;
    while ((1 << qemu_host_page_bits) < VBOX_ONLY((int))qemu_host_page_size)
        qemu_host_page_bits++;
    qemu_host_page_mask = ~(qemu_host_page_size - 1);

#ifndef VBOX /* We use other means to set reserved bit on our pages */
#if defined(CONFIG_BSD) && defined(CONFIG_USER_ONLY)
    {
#ifdef HAVE_KINFO_GETVMMAP
        struct kinfo_vmentry *freep;
        int i, cnt;

        freep = kinfo_getvmmap(getpid(), &cnt);
        if (freep) {
            mmap_lock();
            for (i = 0; i < cnt; i++) {
                uintptr_t startaddr, endaddr;

                startaddr = freep[i].kve_start;
                endaddr = freep[i].kve_end;
                if (h2g_valid(startaddr)) {
                    startaddr = h2g(startaddr) & TARGET_PAGE_MASK;

                    if (h2g_valid(endaddr)) {
                        endaddr = h2g(endaddr);
                        page_set_flags(startaddr, endaddr, PAGE_RESERVED);
                    } else {
#if TARGET_ABI_BITS <= L1_MAP_ADDR_SPACE_BITS
                        endaddr = ~0ul;
                        page_set_flags(startaddr, endaddr, PAGE_RESERVED);
#endif
                    }
                }
            }
            free(freep);
            mmap_unlock();
        }
#else
        FILE *f;

        last_brk = (uintptr_t)sbrk(0);

        f = fopen("/compat/linux/proc/self/maps", "r");
        if (f) {
            mmap_lock();

            do {
                uintptr_t startaddr, endaddr;
                int n;

                n = fscanf (f, "%lx-%lx %*[^\n]\n", &startaddr, &endaddr);

                if (n == 2 && h2g_valid(startaddr)) {
                    startaddr = h2g(startaddr) & TARGET_PAGE_MASK;

                    if (h2g_valid(endaddr)) {
                        endaddr = h2g(endaddr);
                    } else {
                        endaddr = ~0ul;
                    }
                    page_set_flags(startaddr, endaddr, PAGE_RESERVED);
                }
            } while (!feof(f));

            fclose(f);
            mmap_unlock();
        }
#endif
    }
#endif
#endif /* !VBOX */
}

static PageDesc *page_find_alloc(tb_page_addr_t index, int alloc)
{
    PageDesc *pd;
    void **lp;
    int i;

#if defined(CONFIG_USER_ONLY)
    /* We can't use qemu_malloc because it may recurse into a locked mutex. */
# define ALLOC(P, SIZE)                                 \
    do {                                                \
        P = mmap(NULL, SIZE, PROT_READ | PROT_WRITE,    \
                 MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);   \
    } while (0)
#else
# define ALLOC(P, SIZE) \
    do { P = qemu_mallocz(SIZE); } while (0)
#endif

    /* Level 1.  Always allocated.  */
    lp = l1_map + ((index >> V_L1_SHIFT) & (V_L1_SIZE - 1));

    /* Level 2..N-1.  */
    for (i = V_L1_SHIFT / L2_BITS - 1; i > 0; i--) {
        void **p = *lp;

        if (p == NULL) {
            if (!alloc) {
                return NULL;
            }
            ALLOC(p, sizeof(void *) * L2_SIZE);
            *lp = p;
        }

        lp = p + ((index >> (i * L2_BITS)) & (L2_SIZE - 1));
    }

    pd = *lp;
    if (pd == NULL) {
        if (!alloc) {
            return NULL;
        }
        ALLOC(pd, sizeof(PageDesc) * L2_SIZE);
        *lp = pd;
    }

#undef ALLOC

    return pd + (index & (L2_SIZE - 1));
}

static inline PageDesc *page_find(tb_page_addr_t index)
{
    return page_find_alloc(index, 0);
}

#if !defined(CONFIG_USER_ONLY)
static PhysPageDesc *phys_page_find_alloc(target_phys_addr_t index, int alloc)
{
    PhysPageDesc *pd;
    void **lp;
    int i;

    /* Level 1.  Always allocated.  */
    lp = l1_phys_map + ((index >> P_L1_SHIFT) & (P_L1_SIZE - 1));

    /* Level 2..N-1.  */
    for (i = P_L1_SHIFT / L2_BITS - 1; i > 0; i--) {
        void **p = *lp;
        if (p == NULL) {
            if (!alloc) {
                return NULL;
            }
            *lp = p = qemu_mallocz(sizeof(void *) * L2_SIZE);
        }
        lp = p + ((index >> (i * L2_BITS)) & (L2_SIZE - 1));
    }

    pd = *lp;
    if (pd == NULL) {
        int i;

        if (!alloc) {
            return NULL;
        }

        *lp = pd = qemu_malloc(sizeof(PhysPageDesc) * L2_SIZE);

        for (i = 0; i < L2_SIZE; i++) {
            pd[i].phys_offset = IO_MEM_UNASSIGNED;
            pd[i].region_offset = (index + i) << TARGET_PAGE_BITS;
        }
    }

    return pd + (index & (L2_SIZE - 1));
}

static inline PhysPageDesc *phys_page_find(target_phys_addr_t index)
{
    return phys_page_find_alloc(index, 0);
}

static void tlb_protect_code(ram_addr_t ram_addr);
static void tlb_unprotect_code_phys(CPUState *env, ram_addr_t ram_addr,
                                    target_ulong vaddr);
#define mmap_lock() do { } while(0)
#define mmap_unlock() do { } while(0)
#endif

#ifdef VBOX /*  We don't need such huge codegen buffer size, as execute
                most of the code in raw or hm mode. */
#define DEFAULT_CODE_GEN_BUFFER_SIZE (8 * 1024 * 1024)
#else  /* !VBOX */
#define DEFAULT_CODE_GEN_BUFFER_SIZE (32 * 1024 * 1024)
#endif /* !VBOX */

#if defined(CONFIG_USER_ONLY)
/* Currently it is not recommended to allocate big chunks of data in
   user mode. It will change when a dedicated libc will be used */
#define USE_STATIC_CODE_GEN_BUFFER
#endif

#if defined(VBOX) && defined(USE_STATIC_CODE_GEN_BUFFER)
# error "VBox allocates codegen buffer dynamically"
#endif

#ifdef USE_STATIC_CODE_GEN_BUFFER
static uint8_t static_code_gen_buffer[DEFAULT_CODE_GEN_BUFFER_SIZE]
               __attribute__((aligned (CODE_GEN_ALIGN)));
#endif

static void code_gen_alloc(uintptr_t tb_size)
{
#ifdef USE_STATIC_CODE_GEN_BUFFER
    code_gen_buffer = static_code_gen_buffer;
    code_gen_buffer_size = DEFAULT_CODE_GEN_BUFFER_SIZE;
    map_exec(code_gen_buffer, code_gen_buffer_size);
#else
# ifdef VBOX
    /* We cannot use phys_ram_size here, as it's 0 now,
     * it only gets initialized once RAM registration callback
     * (REMR3NotifyPhysRamRegister()) called.
     */
    code_gen_buffer_size = DEFAULT_CODE_GEN_BUFFER_SIZE;
# else  /* !VBOX */
    code_gen_buffer_size = tb_size;
    if (code_gen_buffer_size == 0) {
#if defined(CONFIG_USER_ONLY)
        /* in user mode, phys_ram_size is not meaningful */
        code_gen_buffer_size = DEFAULT_CODE_GEN_BUFFER_SIZE;
#else
        /* XXX: needs adjustments */
        code_gen_buffer_size = (uintptr_t)(ram_size / 4);
#endif
    }
    if (code_gen_buffer_size < MIN_CODE_GEN_BUFFER_SIZE)
        code_gen_buffer_size = MIN_CODE_GEN_BUFFER_SIZE;
# endif /* !VBOX */
    /* The code gen buffer location may have constraints depending on
       the host cpu and OS */
# ifdef VBOX
    code_gen_buffer = RTMemExecAlloc(code_gen_buffer_size);

    if (!code_gen_buffer) {
        LogRel(("REM: failed allocate codegen buffer %lld\n",
                code_gen_buffer_size));
        return;
    }
# else  /* !VBOX */
#if defined(__linux__)
    {
        int flags;
        void *start = NULL;

        flags = MAP_PRIVATE | MAP_ANONYMOUS;
#if defined(__x86_64__)
        flags |= MAP_32BIT;
        /* Cannot map more than that */
        if (code_gen_buffer_size > (800 * 1024 * 1024))
            code_gen_buffer_size = (800 * 1024 * 1024);
#elif defined(__sparc_v9__)
        // Map the buffer below 2G, so we can use direct calls and branches
        flags |= MAP_FIXED;
        start = (void *) 0x60000000UL;
        if (code_gen_buffer_size > (512 * 1024 * 1024))
            code_gen_buffer_size = (512 * 1024 * 1024);
#elif defined(__arm__)
        /* Map the buffer below 32M, so we can use direct calls and branches */
        flags |= MAP_FIXED;
        start = (void *) 0x01000000UL;
        if (code_gen_buffer_size > 16 * 1024 * 1024)
            code_gen_buffer_size = 16 * 1024 * 1024;
#elif defined(__s390x__)
        /* Map the buffer so that we can use direct calls and branches.  */
        /* We have a +- 4GB range on the branches; leave some slop.  */
        if (code_gen_buffer_size > (3ul * 1024 * 1024 * 1024)) {
            code_gen_buffer_size = 3ul * 1024 * 1024 * 1024;
        }
        start = (void *)0x90000000UL;
#endif
        code_gen_buffer = mmap(start, code_gen_buffer_size,
                               PROT_WRITE | PROT_READ | PROT_EXEC,
                               flags, -1, 0);
        if (code_gen_buffer == MAP_FAILED) {
            fprintf(stderr, "Could not allocate dynamic translator buffer\n");
            exit(1);
        }
    }
#elif defined(__FreeBSD__) || defined(__FreeBSD_kernel__) || defined(__DragonFly__)
    {
        int flags;
        void *addr = NULL;
        flags = MAP_PRIVATE | MAP_ANONYMOUS;
#if defined(__x86_64__)
        /* FreeBSD doesn't have MAP_32BIT, use MAP_FIXED and assume
         * 0x40000000 is free */
        flags |= MAP_FIXED;
        addr = (void *)0x40000000;
        /* Cannot map more than that */
        if (code_gen_buffer_size > (800 * 1024 * 1024))
            code_gen_buffer_size = (800 * 1024 * 1024);
#endif
        code_gen_buffer = mmap(addr, code_gen_buffer_size,
                               PROT_WRITE | PROT_READ | PROT_EXEC,
                               flags, -1, 0);
        if (code_gen_buffer == MAP_FAILED) {
            fprintf(stderr, "Could not allocate dynamic translator buffer\n");
            exit(1);
        }
    }
#else
    code_gen_buffer = qemu_malloc(code_gen_buffer_size);
    map_exec(code_gen_buffer, code_gen_buffer_size);
#endif
# endif /* !VBOX */
#endif /* !USE_STATIC_CODE_GEN_BUFFER */
#ifndef VBOX /** @todo r=bird: why are we different? */
    map_exec(code_gen_prologue, sizeof(code_gen_prologue));
#else
    map_exec(code_gen_prologue, _1K);
#endif
    code_gen_buffer_max_size = code_gen_buffer_size -
        (TCG_MAX_OP_SIZE * OPC_MAX_SIZE);
    code_gen_max_blocks = code_gen_buffer_size / CODE_GEN_AVG_BLOCK_SIZE;
    tbs = qemu_malloc(code_gen_max_blocks * sizeof(TranslationBlock));
}

/* Must be called before using the QEMU cpus. 'tb_size' is the size
   (in bytes) allocated to the translation buffer. Zero means default
   size. */
void cpu_exec_init_all(uintptr_t tb_size)
{
    cpu_gen_init();
    code_gen_alloc(tb_size);
    code_gen_ptr = code_gen_buffer;
    page_init();
#if !defined(CONFIG_USER_ONLY)
    io_mem_init();
#endif
#if !defined(CONFIG_USER_ONLY) || !defined(CONFIG_USE_GUEST_BASE)
    /* There's no guest base to take into account, so go ahead and
       initialize the prologue now.  */
    tcg_prologue_init(&tcg_ctx);
#endif
}

#ifndef VBOX
#if defined(CPU_SAVE_VERSION) && !defined(CONFIG_USER_ONLY)

static int cpu_common_post_load(void *opaque, int version_id)
{
    CPUState *env = opaque;

    /* 0x01 was CPU_INTERRUPT_EXIT. This line can be removed when the
       version_id is increased. */
    env->interrupt_request &= ~0x01;
    tlb_flush(env, 1);

    return 0;
}

static const VMStateDescription vmstate_cpu_common = {
    .name = "cpu_common",
    .version_id = 1,
    .minimum_version_id = 1,
    .minimum_version_id_old = 1,
    .post_load = cpu_common_post_load,
    .fields      = (VMStateField []) {
        VMSTATE_UINT32(halted, CPUState),
        VMSTATE_UINT32(interrupt_request, CPUState),
        VMSTATE_END_OF_LIST()
    }
};
#endif

CPUState *qemu_get_cpu(int cpu)
{
    CPUState *env = first_cpu;

    while (env) {
        if (env->cpu_index == cpu)
            break;
        env = env->next_cpu;
    }

    return env;
}

#endif /* !VBOX */

void cpu_exec_init(CPUState *env)
{
    CPUState **penv;
    int cpu_index;

#if defined(CONFIG_USER_ONLY)
    cpu_list_lock();
#endif
    env->next_cpu = NULL;
    penv = &first_cpu;
    cpu_index = 0;
    while (*penv != NULL) {
        penv = &(*penv)->next_cpu;
        cpu_index++;
    }
    env->cpu_index = cpu_index;
    env->numa_node = 0;
    QTAILQ_INIT(&env->breakpoints);
    QTAILQ_INIT(&env->watchpoints);
    *penv = env;
#ifndef VBOX
#if defined(CONFIG_USER_ONLY)
    cpu_list_unlock();
#endif
#if defined(CPU_SAVE_VERSION) && !defined(CONFIG_USER_ONLY)
    vmstate_register(NULL, cpu_index, &vmstate_cpu_common, env);
    register_savevm(NULL, "cpu", cpu_index, CPU_SAVE_VERSION,
                    cpu_save, cpu_load, env);
#endif
#endif /* !VBOX */
}

static inline void invalidate_page_bitmap(PageDesc *p)
{
    if (p->code_bitmap) {
        qemu_free(p->code_bitmap);
        p->code_bitmap = NULL;
    }
    p->code_write_count = 0;
}

/* Set to NULL all the 'first_tb' fields in all PageDescs. */

static void page_flush_tb_1 (int level, void **lp)
{
    int i;

    if (*lp == NULL) {
        return;
    }
    if (level == 0) {
        PageDesc *pd = *lp;
        for (i = 0; i < L2_SIZE; ++i) {
            pd[i].first_tb = NULL;
            invalidate_page_bitmap(pd + i);
        }
    } else {
        void **pp = *lp;
        for (i = 0; i < L2_SIZE; ++i) {
            page_flush_tb_1 (level - 1, pp + i);
        }
    }
}

static void page_flush_tb(void)
{
    int i;
    for (i = 0; i < V_L1_SIZE; i++) {
        page_flush_tb_1(V_L1_SHIFT / L2_BITS - 1, l1_map + i);
    }
}

/* flush all the translation blocks */
/* XXX: tb_flush is currently not thread safe */
void tb_flush(CPUState *env1)
{
    CPUState *env;
#ifdef VBOX
    STAM_PROFILE_START(&env1->StatTbFlush, a);
#endif
#if defined(DEBUG_FLUSH)
    printf("qemu: flush code_size=%ld nb_tbs=%d avg_tb_size=%ld\n",
           (unsigned long)(code_gen_ptr - code_gen_buffer),
           nb_tbs, nb_tbs > 0 ?
           ((unsigned long)(code_gen_ptr - code_gen_buffer)) / nb_tbs : 0);
#endif
    if ((uintptr_t)(code_gen_ptr - code_gen_buffer) > code_gen_buffer_size)
        cpu_abort(env1, "Internal error: code buffer overflow\n");

    nb_tbs = 0;

    for(env = first_cpu; env != NULL; env = env->next_cpu) {
        memset (env->tb_jmp_cache, 0, TB_JMP_CACHE_SIZE * sizeof (void *));
    }

    memset (tb_phys_hash, 0, CODE_GEN_PHYS_HASH_SIZE * sizeof (void *));
    page_flush_tb();

    code_gen_ptr = code_gen_buffer;
    /* XXX: flush processor icache at this point if cache flush is
       expensive */
    tb_flush_count++;
#ifdef VBOX
    STAM_PROFILE_STOP(&env1->StatTbFlush, a);
#endif
}

#ifdef DEBUG_TB_CHECK

static void tb_invalidate_check(target_ulong address)
{
    TranslationBlock *tb;
    int i;
    address &= TARGET_PAGE_MASK;
    for(i = 0;i < CODE_GEN_PHYS_HASH_SIZE; i++) {
        for(tb = tb_phys_hash[i]; tb != NULL; tb = tb->phys_hash_next) {
            if (!(address + TARGET_PAGE_SIZE <= tb->pc ||
                  address >= tb->pc + tb->size)) {
                printf("ERROR invalidate: address=" TARGET_FMT_lx
                       " PC=%08lx size=%04x\n",
                       address, (long)tb->pc, tb->size);
            }
        }
    }
}

/* verify that all the pages have correct rights for code */
static void tb_page_check(void)
{
    TranslationBlock *tb;
    int i, flags1, flags2;

    for(i = 0;i < CODE_GEN_PHYS_HASH_SIZE; i++) {
        for(tb = tb_phys_hash[i]; tb != NULL; tb = tb->phys_hash_next) {
            flags1 = page_get_flags(tb->pc);
            flags2 = page_get_flags(tb->pc + tb->size - 1);
            if ((flags1 & PAGE_WRITE) || (flags2 & PAGE_WRITE)) {
                printf("ERROR page flags: PC=%08lx size=%04x f1=%x f2=%x\n",
                       (long)tb->pc, tb->size, flags1, flags2);
            }
        }
    }
}

#endif

/* invalidate one TB */
static inline void tb_remove(TranslationBlock **ptb, TranslationBlock *tb,
                             int next_offset)
{
    TranslationBlock *tb1;
    for(;;) {
        tb1 = *ptb;
        if (tb1 == tb) {
            *ptb = *(TranslationBlock **)((char *)tb1 + next_offset);
            break;
        }
        ptb = (TranslationBlock **)((char *)tb1 + next_offset);
    }
}

static inline void tb_page_remove(TranslationBlock **ptb, TranslationBlock *tb)
{
    TranslationBlock *tb1;
    unsigned int n1;

    for(;;) {
        tb1 = *ptb;
        n1 = (intptr_t)tb1 & 3;
        tb1 = (TranslationBlock *)((intptr_t)tb1 & ~3);
        if (tb1 == tb) {
            *ptb = tb1->page_next[n1];
            break;
        }
        ptb = &tb1->page_next[n1];
    }
}

static inline void tb_jmp_remove(TranslationBlock *tb, int n)
{
    TranslationBlock *tb1, **ptb;
    unsigned int n1;

    ptb = &tb->jmp_next[n];
    tb1 = *ptb;
    if (tb1) {
        /* find tb(n) in circular list */
        for(;;) {
            tb1 = *ptb;
            n1 = (intptr_t)tb1 & 3;
            tb1 = (TranslationBlock *)((intptr_t)tb1 & ~3);
            if (n1 == n && tb1 == tb)
                break;
            if (n1 == 2) {
                ptb = &tb1->jmp_first;
            } else {
                ptb = &tb1->jmp_next[n1];
            }
        }
        /* now we can suppress tb(n) from the list */
        *ptb = tb->jmp_next[n];

        tb->jmp_next[n] = NULL;
    }
}

/* reset the jump entry 'n' of a TB so that it is not chained to
   another TB */
static inline void tb_reset_jump(TranslationBlock *tb, int n)
{
    tb_set_jmp_target(tb, n, (uintptr_t)(tb->tc_ptr + tb->tb_next_offset[n]));
}

void tb_phys_invalidate(TranslationBlock *tb, tb_page_addr_t page_addr)
{
    CPUState *env;
    PageDesc *p;
    unsigned int h, n1;
    tb_page_addr_t phys_pc;
    TranslationBlock *tb1, *tb2;

    /* remove the TB from the hash list */
    phys_pc = tb->page_addr[0] + (tb->pc & ~TARGET_PAGE_MASK);
    h = tb_phys_hash_func(phys_pc);
    tb_remove(&tb_phys_hash[h], tb,
              offsetof(TranslationBlock, phys_hash_next));

    /* remove the TB from the page list */
    if (tb->page_addr[0] != page_addr) {
        p = page_find(tb->page_addr[0] >> TARGET_PAGE_BITS);
        tb_page_remove(&p->first_tb, tb);
        invalidate_page_bitmap(p);
    }
    if (tb->page_addr[1] != -1 && tb->page_addr[1] != page_addr) {
        p = page_find(tb->page_addr[1] >> TARGET_PAGE_BITS);
        tb_page_remove(&p->first_tb, tb);
        invalidate_page_bitmap(p);
    }

    tb_invalidated_flag = 1;

    /* remove the TB from the hash list */
    h = tb_jmp_cache_hash_func(tb->pc);
    for(env = first_cpu; env != NULL; env = env->next_cpu) {
        if (env->tb_jmp_cache[h] == tb)
            env->tb_jmp_cache[h] = NULL;
    }

    /* suppress this TB from the two jump lists */
    tb_jmp_remove(tb, 0);
    tb_jmp_remove(tb, 1);

    /* suppress any remaining jumps to this TB */
    tb1 = tb->jmp_first;
    for(;;) {
        n1 = (intptr_t)tb1 & 3;
        if (n1 == 2)
            break;
        tb1 = (TranslationBlock *)((intptr_t)tb1 & ~3);
        tb2 = tb1->jmp_next[n1];
        tb_reset_jump(tb1, n1);
        tb1->jmp_next[n1] = NULL;
        tb1 = tb2;
    }
    tb->jmp_first = (TranslationBlock *)((intptr_t)tb | 2); /* fail safe */

    tb_phys_invalidate_count++;
}

#ifdef VBOX

void tb_invalidate_virt(CPUState *env, uint32_t eip)
{
# if 1
    tb_flush(env);
# else
    uint8_t *cs_base, *pc;
    unsigned int flags, h, phys_pc;
    TranslationBlock *tb, **ptb;

    flags = env->hflags;
    flags |= (env->eflags & (IOPL_MASK | TF_MASK | VM_MASK));
    cs_base = env->segs[R_CS].base;
    pc = cs_base + eip;

    tb = tb_find(&ptb, (uintptr_t)pc, (uintptr_t)cs_base,
                 flags);

    if(tb)
    {
#  ifdef DEBUG
        printf("invalidating TB (%08X) at %08X\n", tb, eip);
#  endif
        tb_invalidate(tb);
        //Note: this will leak TBs, but the whole cache will be flushed
        //      when it happens too often
        tb->pc = 0;
        tb->cs_base = 0;
        tb->flags = 0;
    }
# endif
}

# ifdef VBOX_STRICT
/**
 * Gets the page offset.
 */
ram_addr_t get_phys_page_offset(target_ulong addr)
{
    PhysPageDesc *p = phys_page_find(addr >> TARGET_PAGE_BITS);
    return p ? p->phys_offset : 0;
}
# endif /* VBOX_STRICT */

#endif /* VBOX */

static inline void set_bits(uint8_t *tab, int start, int len)
{
    int end, mask, end1;

    end = start + len;
    tab += start >> 3;
    mask = 0xff << (start & 7);
    if ((start & ~7) == (end & ~7)) {
        if (start < end) {
            mask &= ~(0xff << (end & 7));
            *tab |= mask;
        }
    } else {
        *tab++ |= mask;
        start = (start + 8) & ~7;
        end1 = end & ~7;
        while (start < end1) {
            *tab++ = 0xff;
            start += 8;
        }
        if (start < end) {
            mask = ~(0xff << (end & 7));
            *tab |= mask;
        }
    }
}

static void build_page_bitmap(PageDesc *p)
{
    int n, tb_start, tb_end;
    TranslationBlock *tb;

    p->code_bitmap = qemu_mallocz(TARGET_PAGE_SIZE / 8);

    tb = p->first_tb;
    while (tb != NULL) {
        n = (intptr_t)tb & 3;
        tb = (TranslationBlock *)((intptr_t)tb & ~3);
        /* NOTE: this is subtle as a TB may span two physical pages */
        if (n == 0) {
            /* NOTE: tb_end may be after the end of the page, but
               it is not a problem */
            tb_start = tb->pc & ~TARGET_PAGE_MASK;
            tb_end = tb_start + tb->size;
            if (tb_end > TARGET_PAGE_SIZE)
                tb_end = TARGET_PAGE_SIZE;
        } else {
            tb_start = 0;
            tb_end = ((tb->pc + tb->size) & ~TARGET_PAGE_MASK);
        }
        set_bits(p->code_bitmap, tb_start, tb_end - tb_start);
        tb = tb->page_next[n];
    }
}

TranslationBlock *tb_gen_code(CPUState *env,
                              target_ulong pc, target_ulong cs_base,
                              int flags, int cflags)
{
    TranslationBlock *tb;
    uint8_t *tc_ptr;
    tb_page_addr_t phys_pc, phys_page2;
    target_ulong virt_page2;
    int code_gen_size;

    phys_pc = get_page_addr_code(env, pc);
    tb = tb_alloc(pc);
    if (!tb) {
        /* flush must be done */
        tb_flush(env);
        /* cannot fail at this point */
        tb = tb_alloc(pc);
        /* Don't forget to invalidate previous TB info.  */
        tb_invalidated_flag = 1;
    }
    tc_ptr = code_gen_ptr;
    tb->tc_ptr = tc_ptr;
    tb->cs_base = cs_base;
    tb->flags = flags;
    tb->cflags = cflags;
    cpu_gen_code(env, tb, &code_gen_size);
    code_gen_ptr = (void *)(((uintptr_t)code_gen_ptr + code_gen_size + CODE_GEN_ALIGN - 1) & ~(CODE_GEN_ALIGN - 1));

    /* check next page if needed */
    virt_page2 = (pc + tb->size - 1) & TARGET_PAGE_MASK;
    phys_page2 = -1;
    if ((pc & TARGET_PAGE_MASK) != virt_page2) {
        phys_page2 = get_page_addr_code(env, virt_page2);
    }
    tb_link_page(tb, phys_pc, phys_page2);
    return tb;
}

/* invalidate all TBs which intersect with the target physical page
   starting in range [start;end[. NOTE: start and end must refer to
   the same physical page. 'is_cpu_write_access' should be true if called
   from a real cpu write access: the virtual CPU will exit the current
   TB if code is modified inside this TB. */
void tb_invalidate_phys_page_range(tb_page_addr_t start, tb_page_addr_t end,
                                   int is_cpu_write_access)
{
    TranslationBlock *tb, *tb_next, *saved_tb;
    CPUState *env = cpu_single_env;
    tb_page_addr_t tb_start, tb_end;
    PageDesc *p;
    int n;
#ifdef TARGET_HAS_PRECISE_SMC
    int current_tb_not_found = is_cpu_write_access;
    TranslationBlock *current_tb = NULL;
    int current_tb_modified = 0;
    target_ulong current_pc = 0;
    target_ulong current_cs_base = 0;
    int current_flags = 0;
#endif /* TARGET_HAS_PRECISE_SMC */

    p = page_find(start >> TARGET_PAGE_BITS);
    if (!p)
        return;
    if (!p->code_bitmap &&
        ++p->code_write_count >= SMC_BITMAP_USE_THRESHOLD &&
        is_cpu_write_access) {
        /* build code bitmap */
        build_page_bitmap(p);
    }

    /* we remove all the TBs in the range [start, end[ */
    /* XXX: see if in some cases it could be faster to invalidate all the code */
    tb = p->first_tb;
    while (tb != NULL) {
        n = (intptr_t)tb & 3;
        tb = (TranslationBlock *)((intptr_t)tb & ~3);
        tb_next = tb->page_next[n];
        /* NOTE: this is subtle as a TB may span two physical pages */
        if (n == 0) {
            /* NOTE: tb_end may be after the end of the page, but
               it is not a problem */
            tb_start = tb->page_addr[0] + (tb->pc & ~TARGET_PAGE_MASK);
            tb_end = tb_start + tb->size;
        } else {
            tb_start = tb->page_addr[1];
            tb_end = tb_start + ((tb->pc + tb->size) & ~TARGET_PAGE_MASK);
        }
        if (!(tb_end <= start || tb_start >= end)) {
#ifdef TARGET_HAS_PRECISE_SMC
            if (current_tb_not_found) {
                current_tb_not_found = 0;
                current_tb = NULL;
                if (env->mem_io_pc) {
                    /* now we have a real cpu fault */
                    current_tb = tb_find_pc(env->mem_io_pc);
                }
            }
            if (current_tb == tb &&
                (current_tb->cflags & CF_COUNT_MASK) != 1) {
                /* If we are modifying the current TB, we must stop
                its execution. We could be more precise by checking
                that the modification is after the current PC, but it
                would require a specialized function to partially
                restore the CPU state */

                current_tb_modified = 1;
                cpu_restore_state(current_tb, env,
                                  env->mem_io_pc, NULL);
                cpu_get_tb_cpu_state(env, &current_pc, &current_cs_base,
                                     &current_flags);
            }
#endif /* TARGET_HAS_PRECISE_SMC */
            /* we need to do that to handle the case where a signal
               occurs while doing tb_phys_invalidate() */
            saved_tb = NULL;
            if (env) {
                saved_tb = env->current_tb;
                env->current_tb = NULL;
            }
            tb_phys_invalidate(tb, -1);
            if (env) {
                env->current_tb = saved_tb;
                if (env->interrupt_request && env->current_tb)
                    cpu_interrupt(env, env->interrupt_request);
            }
        }
        tb = tb_next;
    }
#if !defined(CONFIG_USER_ONLY)
    /* if no code remaining, no need to continue to use slow writes */
    if (!p->first_tb) {
        invalidate_page_bitmap(p);
        if (is_cpu_write_access) {
            tlb_unprotect_code_phys(env, start, env->mem_io_vaddr);
        }
    }
#endif
#ifdef TARGET_HAS_PRECISE_SMC
    if (current_tb_modified) {
        /* we generate a block containing just the instruction
           modifying the memory. It will ensure that it cannot modify
           itself */
        env->current_tb = NULL;
        tb_gen_code(env, current_pc, current_cs_base, current_flags, 1);
        cpu_resume_from_signal(env, NULL);
    }
#endif
}

/* len must be <= 8 and start must be a multiple of len */
static inline void tb_invalidate_phys_page_fast(tb_page_addr_t start, int len)
{
    PageDesc *p;
    int offset, b;
#if 0
    if (1) {
        qemu_log("modifying code at 0x%x size=%d EIP=%x PC=%08x\n",
                  cpu_single_env->mem_io_vaddr, len,
                  cpu_single_env->eip,
                  cpu_single_env->eip + (intptr_t)cpu_single_env->segs[R_CS].base);
    }
#endif
    p = page_find(start >> TARGET_PAGE_BITS);
    if (!p)
        return;
    if (p->code_bitmap) {
        offset = start & ~TARGET_PAGE_MASK;
        b = p->code_bitmap[offset >> 3] >> (offset & 7);
        if (b & ((1 << len) - 1))
            goto do_invalidate;
    } else {
    do_invalidate:
        tb_invalidate_phys_page_range(start, start + len, 1);
    }
}

#if !defined(CONFIG_SOFTMMU)
static void tb_invalidate_phys_page(tb_page_addr_t addr,
                                    uintptr_t pc, void *puc)
{
    TranslationBlock *tb;
    PageDesc *p;
    int n;
#ifdef TARGET_HAS_PRECISE_SMC
    TranslationBlock *current_tb = NULL;
    CPUState *env = cpu_single_env;
    int current_tb_modified = 0;
    target_ulong current_pc = 0;
    target_ulong current_cs_base = 0;
    int current_flags = 0;
#endif

    addr &= TARGET_PAGE_MASK;
    p = page_find(addr >> TARGET_PAGE_BITS);
    if (!p)
        return;
    tb = p->first_tb;
#ifdef TARGET_HAS_PRECISE_SMC
    if (tb && pc != 0) {
        current_tb = tb_find_pc(pc);
    }
#endif
    while (tb != NULL) {
        n = (intptr_t)tb & 3;
        tb = (TranslationBlock *)((intptr_t)tb & ~3);
#ifdef TARGET_HAS_PRECISE_SMC
        if (current_tb == tb &&
            (current_tb->cflags & CF_COUNT_MASK) != 1) {
                /* If we are modifying the current TB, we must stop
                   its execution. We could be more precise by checking
                   that the modification is after the current PC, but it
                   would require a specialized function to partially
                   restore the CPU state */

            current_tb_modified = 1;
            cpu_restore_state(current_tb, env, pc, puc);
            cpu_get_tb_cpu_state(env, &current_pc, &current_cs_base,
                                 &current_flags);
        }
#endif /* TARGET_HAS_PRECISE_SMC */
        tb_phys_invalidate(tb, addr);
        tb = tb->page_next[n];
    }
    p->first_tb = NULL;
#ifdef TARGET_HAS_PRECISE_SMC
    if (current_tb_modified) {
        /* we generate a block containing just the instruction
           modifying the memory. It will ensure that it cannot modify
           itself */
        env->current_tb = NULL;
        tb_gen_code(env, current_pc, current_cs_base, current_flags, 1);
        cpu_resume_from_signal(env, puc);
    }
#endif
}
#endif

/* add the tb in the target page and protect it if necessary */
static inline void tb_alloc_page(TranslationBlock *tb,
                                 unsigned int n, tb_page_addr_t page_addr)
{
    PageDesc *p;
    TranslationBlock *last_first_tb;

    tb->page_addr[n] = page_addr;
    p = page_find_alloc(page_addr >> TARGET_PAGE_BITS, 1);
    tb->page_next[n] = p->first_tb;
    last_first_tb = p->first_tb;
    p->first_tb = (TranslationBlock *)((intptr_t)tb | n);
    invalidate_page_bitmap(p);

#if defined(TARGET_HAS_SMC) || 1

#if defined(CONFIG_USER_ONLY)
    if (p->flags & PAGE_WRITE) {
        target_ulong addr;
        PageDesc *p2;
        int prot;

        /* force the host page as non writable (writes will have a
           page fault + mprotect overhead) */
        page_addr &= qemu_host_page_mask;
        prot = 0;
        for(addr = page_addr; addr < page_addr + qemu_host_page_size;
            addr += TARGET_PAGE_SIZE) {

            p2 = page_find (addr >> TARGET_PAGE_BITS);
            if (!p2)
                continue;
            prot |= p2->flags;
            p2->flags &= ~PAGE_WRITE;
          }
        mprotect(g2h(page_addr), qemu_host_page_size,
                 (prot & PAGE_BITS) & ~PAGE_WRITE);
#ifdef DEBUG_TB_INVALIDATE
        printf("protecting code page: 0x" TARGET_FMT_lx "\n",
               page_addr);
#endif
    }
#else
    /* if some code is already present, then the pages are already
       protected. So we handle the case where only the first TB is
       allocated in a physical page */
    if (!last_first_tb) {
        tlb_protect_code(page_addr);
    }
#endif

#endif /* TARGET_HAS_SMC */
}

/* Allocate a new translation block. Flush the translation buffer if
   too many translation blocks or too much generated code. */
TranslationBlock *tb_alloc(target_ulong pc)
{
    TranslationBlock *tb;

    if (nb_tbs >= code_gen_max_blocks ||
        (code_gen_ptr - code_gen_buffer) >= VBOX_ONLY((uintptr_t))code_gen_buffer_max_size)
        return NULL;
    tb = &tbs[nb_tbs++];
    tb->pc = pc;
    tb->cflags = 0;
    return tb;
}

void tb_free(TranslationBlock *tb)
{
    /* In practice this is mostly used for single use temporary TB
       Ignore the hard cases and just back up if this TB happens to
       be the last one generated.  */
    if (nb_tbs > 0 && tb == &tbs[nb_tbs - 1]) {
        code_gen_ptr = tb->tc_ptr;
        nb_tbs--;
    }
}

/* add a new TB and link it to the physical page tables. phys_page2 is
   (-1) to indicate that only one page contains the TB. */
void tb_link_page(TranslationBlock *tb,
                  tb_page_addr_t phys_pc, tb_page_addr_t phys_page2)
{
    unsigned int h;
    TranslationBlock **ptb;

    /* Grab the mmap lock to stop another thread invalidating this TB
       before we are done.  */
    mmap_lock();
    /* add in the physical hash table */
    h = tb_phys_hash_func(phys_pc);
    ptb = &tb_phys_hash[h];
    tb->phys_hash_next = *ptb;
    *ptb = tb;

    /* add in the page list */
    tb_alloc_page(tb, 0, phys_pc & TARGET_PAGE_MASK);
    if (phys_page2 != -1)
        tb_alloc_page(tb, 1, phys_page2);
    else
        tb->page_addr[1] = -1;

    tb->jmp_first = (TranslationBlock *)((intptr_t)tb | 2);
    tb->jmp_next[0] = NULL;
    tb->jmp_next[1] = NULL;

    /* init original jump addresses */
    if (tb->tb_next_offset[0] != 0xffff)
        tb_reset_jump(tb, 0);
    if (tb->tb_next_offset[1] != 0xffff)
        tb_reset_jump(tb, 1);

#ifdef DEBUG_TB_CHECK
    tb_page_check();
#endif
    mmap_unlock();
}

/* find the TB 'tb' such that tb[0].tc_ptr <= tc_ptr <
   tb[1].tc_ptr. Return NULL if not found */
TranslationBlock *tb_find_pc(uintptr_t tc_ptr)
{
    int m_min, m_max, m;
    uintptr_t v;
    TranslationBlock *tb;

    if (nb_tbs <= 0)
        return NULL;
    if (tc_ptr < (uintptr_t)code_gen_buffer ||
        tc_ptr >= (uintptr_t)code_gen_ptr)
        return NULL;
    /* binary search (cf Knuth) */
    m_min = 0;
    m_max = nb_tbs - 1;
    while (m_min <= m_max) {
        m = (m_min + m_max) >> 1;
        tb = &tbs[m];
        v = (uintptr_t)tb->tc_ptr;
        if (v == tc_ptr)
            return tb;
        else if (tc_ptr < v) {
            m_max = m - 1;
        } else {
            m_min = m + 1;
        }
    }
    return &tbs[m_max];
}

static void tb_reset_jump_recursive(TranslationBlock *tb);

static inline void tb_reset_jump_recursive2(TranslationBlock *tb, int n)
{
    TranslationBlock *tb1, *tb_next, **ptb;
    unsigned int n1;

    tb1 = tb->jmp_next[n];
    if (tb1 != NULL) {
        /* find head of list */
        for(;;) {
            n1 = (intptr_t)tb1 & 3;
            tb1 = (TranslationBlock *)((intptr_t)tb1 & ~3);
            if (n1 == 2)
                break;
            tb1 = tb1->jmp_next[n1];
        }
        /* we are now sure now that tb jumps to tb1 */
        tb_next = tb1;

        /* remove tb from the jmp_first list */
        ptb = &tb_next->jmp_first;
        for(;;) {
            tb1 = *ptb;
            n1 = (intptr_t)tb1 & 3;
            tb1 = (TranslationBlock *)((intptr_t)tb1 & ~3);
            if (n1 == n && tb1 == tb)
                break;
            ptb = &tb1->jmp_next[n1];
        }
        *ptb = tb->jmp_next[n];
        tb->jmp_next[n] = NULL;

        /* suppress the jump to next tb in generated code */
        tb_reset_jump(tb, n);

        /* suppress jumps in the tb on which we could have jumped */
        tb_reset_jump_recursive(tb_next);
    }
}

static void tb_reset_jump_recursive(TranslationBlock *tb)
{
    tb_reset_jump_recursive2(tb, 0);
    tb_reset_jump_recursive2(tb, 1);
}

#if defined(TARGET_HAS_ICE)
#if defined(CONFIG_USER_ONLY)
static void breakpoint_invalidate(CPUState *env, target_ulong pc)
{
    tb_invalidate_phys_page_range(pc, pc + 1, 0);
}
#else
static void breakpoint_invalidate(CPUState *env, target_ulong pc)
{
    target_phys_addr_t addr;
    target_ulong pd;
    ram_addr_t ram_addr;
    PhysPageDesc *p;

    addr = cpu_get_phys_page_debug(env, pc);
    p = phys_page_find(addr >> TARGET_PAGE_BITS);
    if (!p) {
        pd = IO_MEM_UNASSIGNED;
    } else {
        pd = p->phys_offset;
    }
    ram_addr = (pd & TARGET_PAGE_MASK) | (pc & ~TARGET_PAGE_MASK);
    tb_invalidate_phys_page_range(ram_addr, ram_addr + 1, 0);
}
#endif
#endif /* TARGET_HAS_ICE */

#if defined(CONFIG_USER_ONLY)
void cpu_watchpoint_remove_all(CPUState *env, int mask)

{
}

int cpu_watchpoint_insert(CPUState *env, target_ulong addr, target_ulong len,
                          int flags, CPUWatchpoint **watchpoint)
{
    return -ENOSYS;
}
#else
/* Add a watchpoint.  */
int cpu_watchpoint_insert(CPUState *env, target_ulong addr, target_ulong len,
                          int flags, CPUWatchpoint **watchpoint)
{
    target_ulong len_mask = ~(len - 1);
    CPUWatchpoint *wp;

    /* sanity checks: allow power-of-2 lengths, deny unaligned watchpoints */
    if ((len != 1 && len != 2 && len != 4 && len != 8) || (addr & ~len_mask)) {
        fprintf(stderr, "qemu: tried to set invalid watchpoint at "
                TARGET_FMT_lx ", len=" TARGET_FMT_lu "\n", addr, len);
#ifndef VBOX
        return -EINVAL;
#else
        return VERR_INVALID_PARAMETER;
#endif
    }
    wp = qemu_malloc(sizeof(*wp));

    wp->vaddr = addr;
    wp->len_mask = len_mask;
    wp->flags = flags;

    /* keep all GDB-injected watchpoints in front */
    if (flags & BP_GDB)
        QTAILQ_INSERT_HEAD(&env->watchpoints, wp, entry);
    else
        QTAILQ_INSERT_TAIL(&env->watchpoints, wp, entry);

    tlb_flush_page(env, addr);

    if (watchpoint)
        *watchpoint = wp;
    return 0;
}

/* Remove a specific watchpoint.  */
int cpu_watchpoint_remove(CPUState *env, target_ulong addr, target_ulong len,
                          int flags)
{
    target_ulong len_mask = ~(len - 1);
    CPUWatchpoint *wp;

    QTAILQ_FOREACH(wp, &env->watchpoints, entry) {
        if (addr == wp->vaddr && len_mask == wp->len_mask
                && flags == (wp->flags & ~BP_WATCHPOINT_HIT)) {
            cpu_watchpoint_remove_by_ref(env, wp);
            return 0;
        }
    }
#ifndef VBOX
    return -ENOENT;
#else
    return VERR_NOT_FOUND;
#endif
}

/* Remove a specific watchpoint by reference.  */
void cpu_watchpoint_remove_by_ref(CPUState *env, CPUWatchpoint *watchpoint)
{
    QTAILQ_REMOVE(&env->watchpoints, watchpoint, entry);

    tlb_flush_page(env, watchpoint->vaddr);

    qemu_free(watchpoint);
}

/* Remove all matching watchpoints.  */
void cpu_watchpoint_remove_all(CPUState *env, int mask)
{
    CPUWatchpoint *wp, *next;

    QTAILQ_FOREACH_SAFE(wp, &env->watchpoints, entry, next) {
        if (wp->flags & mask)
            cpu_watchpoint_remove_by_ref(env, wp);
    }
}
#endif

/* Add a breakpoint.  */
int cpu_breakpoint_insert(CPUState *env, target_ulong pc, int flags,
                          CPUBreakpoint **breakpoint)
{
#if defined(TARGET_HAS_ICE)
    CPUBreakpoint *bp;

    bp = qemu_malloc(sizeof(*bp));

    bp->pc = pc;
    bp->flags = flags;

    /* keep all GDB-injected breakpoints in front */
    if (flags & BP_GDB)
        QTAILQ_INSERT_HEAD(&env->breakpoints, bp, entry);
    else
        QTAILQ_INSERT_TAIL(&env->breakpoints, bp, entry);

    breakpoint_invalidate(env, pc);

    if (breakpoint)
        *breakpoint = bp;
    return 0;
#else
    return -ENOSYS;
#endif
}

/* Remove a specific breakpoint.  */
int cpu_breakpoint_remove(CPUState *env, target_ulong pc, int flags)
{
#if defined(TARGET_HAS_ICE)
    CPUBreakpoint *bp;

    QTAILQ_FOREACH(bp, &env->breakpoints, entry) {
        if (bp->pc == pc && bp->flags == flags) {
            cpu_breakpoint_remove_by_ref(env, bp);
            return 0;
        }
    }
# ifndef VBOX
    return -ENOENT;
# else
    return VERR_NOT_FOUND;
# endif
#else
    return -ENOSYS;
#endif
}

/* Remove a specific breakpoint by reference.  */
void cpu_breakpoint_remove_by_ref(CPUState *env, CPUBreakpoint *breakpoint)
{
#if defined(TARGET_HAS_ICE)
    QTAILQ_REMOVE(&env->breakpoints, breakpoint, entry);

    breakpoint_invalidate(env, breakpoint->pc);

    qemu_free(breakpoint);
#endif
}

/* Remove all matching breakpoints. */
void cpu_breakpoint_remove_all(CPUState *env, int mask)
{
#if defined(TARGET_HAS_ICE)
    CPUBreakpoint *bp, *next;

    QTAILQ_FOREACH_SAFE(bp, &env->breakpoints, entry, next) {
        if (bp->flags & mask)
            cpu_breakpoint_remove_by_ref(env, bp);
    }
#endif
}

/* enable or disable single step mode. EXCP_DEBUG is returned by the
   CPU loop after each instruction */
void cpu_single_step(CPUState *env, int enabled)
{
#if defined(TARGET_HAS_ICE)
    if (env->singlestep_enabled != enabled) {
        env->singlestep_enabled = enabled;
        if (kvm_enabled())
            kvm_update_guest_debug(env, 0);
        else {
            /* must flush all the translated code to avoid inconsistencies */
            /* XXX: only flush what is necessary */
            tb_flush(env);
        }
    }
#endif
}

#ifndef VBOX

/* enable or disable low levels log */
void cpu_set_log(int log_flags)
{
    loglevel = log_flags;
    if (loglevel && !logfile) {
        logfile = fopen(logfilename, log_append ? "a" : "w");
        if (!logfile) {
            perror(logfilename);
            _exit(1);
        }
#if !defined(CONFIG_SOFTMMU)
        /* must avoid mmap() usage of glibc by setting a buffer "by hand" */
        {
            static char logfile_buf[4096];
            setvbuf(logfile, logfile_buf, _IOLBF, sizeof(logfile_buf));
        }
#elif !defined(_WIN32)
        /* Win32 doesn't support line-buffering and requires size >= 2 */
        setvbuf(logfile, NULL, _IOLBF, 0);
#endif
        log_append = 1;
    }
    if (!loglevel && logfile) {
        fclose(logfile);
        logfile = NULL;
    }
}

void cpu_set_log_filename(const char *filename)
{
    logfilename = strdup(filename);
    if (logfile) {
        fclose(logfile);
        logfile = NULL;
    }
    cpu_set_log(loglevel);
}

#endif /* !VBOX */

static void cpu_unlink_tb(CPUState *env)
{
    /* FIXME: TB unchaining isn't SMP safe.  For now just ignore the
       problem and hope the cpu will stop of its own accord.  For userspace
       emulation this often isn't actually as bad as it sounds.  Often
       signals are used primarily to interrupt blocking syscalls.  */
    TranslationBlock *tb;
    static spinlock_t interrupt_lock = SPIN_LOCK_UNLOCKED;

    spin_lock(&interrupt_lock);
    tb = env->current_tb;
    /* if the cpu is currently executing code, we must unlink it and
       all the potentially executing TB */
    if (tb) {
        env->current_tb = NULL;
        tb_reset_jump_recursive(tb);
    }
    spin_unlock(&interrupt_lock);
}

/* mask must never be zero, except for A20 change call */
void cpu_interrupt(CPUState *env, int mask)
{
    int old_mask;

    old_mask = env->interrupt_request;
#ifndef VBOX
    env->interrupt_request |= mask;
#else  /* VBOX */
    VM_ASSERT_EMT(env->pVM);
    ASMAtomicOrS32((int32_t volatile *)&env->interrupt_request, mask);
#endif /* VBOX */

#ifndef VBOX
#ifndef CONFIG_USER_ONLY
    /*
     * If called from iothread context, wake the target cpu in
     * case its halted.
     */
    if (!qemu_cpu_self(env)) {
        qemu_cpu_kick(env);
        return;
    }
#endif
#endif /* !VBOX */

    if (use_icount) {
        env->icount_decr.u16.high = 0xffff;
#ifndef CONFIG_USER_ONLY
        if (!can_do_io(env)
            && (mask & ~old_mask) != 0) {
            cpu_abort(env, "Raised interrupt while not in I/O function");
        }
#endif
    } else {
        cpu_unlink_tb(env);
    }
}

void cpu_reset_interrupt(CPUState *env, int mask)
{
#ifdef VBOX
    /*
     * Note: the current implementation can be executed by another thread without problems; make sure this remains true
     *       for future changes!
     */
    ASMAtomicAndS32((int32_t volatile *)&env->interrupt_request, ~mask);
#else /* !VBOX */
    env->interrupt_request &= ~mask;
#endif /* !VBOX */
}

void cpu_exit(CPUState *env)
{
    env->exit_request = 1;
    cpu_unlink_tb(env);
}

#ifndef VBOX
const CPULogItem cpu_log_items[] = {
    { CPU_LOG_TB_OUT_ASM, "out_asm",
      "show generated host assembly code for each compiled TB" },
    { CPU_LOG_TB_IN_ASM, "in_asm",
      "show target assembly code for each compiled TB" },
    { CPU_LOG_TB_OP, "op",
      "show micro ops for each compiled TB" },
    { CPU_LOG_TB_OP_OPT, "op_opt",
      "show micro ops "
#ifdef TARGET_I386
      "before eflags optimization and "
#endif
      "after liveness analysis" },
    { CPU_LOG_INT, "int",
      "show interrupts/exceptions in short format" },
    { CPU_LOG_EXEC, "exec",
      "show trace before each executed TB (lots of logs)" },
    { CPU_LOG_TB_CPU, "cpu",
      "show CPU state before block translation" },
#ifdef TARGET_I386
    { CPU_LOG_PCALL, "pcall",
      "show protected mode far calls/returns/exceptions" },
    { CPU_LOG_RESET, "cpu_reset",
      "show CPU state before CPU resets" },
#endif
#ifdef DEBUG_IOPORT
    { CPU_LOG_IOPORT, "ioport",
      "show all i/o ports accesses" },
#endif
    { 0, NULL, NULL },
};

#ifndef CONFIG_USER_ONLY
static QLIST_HEAD(memory_client_list, CPUPhysMemoryClient) memory_client_list
    = QLIST_HEAD_INITIALIZER(memory_client_list);

static void cpu_notify_set_memory(target_phys_addr_t start_addr,
				  ram_addr_t size,
				  ram_addr_t phys_offset)
{
    CPUPhysMemoryClient *client;
    QLIST_FOREACH(client, &memory_client_list, list) {
        client->set_memory(client, start_addr, size, phys_offset);
    }
}

static int cpu_notify_sync_dirty_bitmap(target_phys_addr_t start,
					target_phys_addr_t end)
{
    CPUPhysMemoryClient *client;
    QLIST_FOREACH(client, &memory_client_list, list) {
        int r = client->sync_dirty_bitmap(client, start, end);
        if (r < 0)
            return r;
    }
    return 0;
}

static int cpu_notify_migration_log(int enable)
{
    CPUPhysMemoryClient *client;
    QLIST_FOREACH(client, &memory_client_list, list) {
        int r = client->migration_log(client, enable);
        if (r < 0)
            return r;
    }
    return 0;
}

static void phys_page_for_each_1(CPUPhysMemoryClient *client,
                                 int level, void **lp)
{
    int i;

    if (*lp == NULL) {
        return;
    }
    if (level == 0) {
        PhysPageDesc *pd = *lp;
        for (i = 0; i < L2_SIZE; ++i) {
            if (pd[i].phys_offset != IO_MEM_UNASSIGNED) {
                client->set_memory(client, pd[i].region_offset,
                                   TARGET_PAGE_SIZE, pd[i].phys_offset);
            }
        }
    } else {
        void **pp = *lp;
        for (i = 0; i < L2_SIZE; ++i) {
            phys_page_for_each_1(client, level - 1, pp + i);
        }
    }
}

static void phys_page_for_each(CPUPhysMemoryClient *client)
{
    int i;
    for (i = 0; i < P_L1_SIZE; ++i) {
        phys_page_for_each_1(client, P_L1_SHIFT / L2_BITS - 1,
                             l1_phys_map + 1);
    }
}

void cpu_register_phys_memory_client(CPUPhysMemoryClient *client)
{
    QLIST_INSERT_HEAD(&memory_client_list, client, list);
    phys_page_for_each(client);
}

void cpu_unregister_phys_memory_client(CPUPhysMemoryClient *client)
{
    QLIST_REMOVE(client, list);
}
#endif

static int cmp1(const char *s1, int n, const char *s2)
{
    if (strlen(s2) != n)
        return 0;
    return memcmp(s1, s2, n) == 0;
}

/* takes a comma separated list of log masks. Return 0 if error. */
int cpu_str_to_log_mask(const char *str)
{
    const CPULogItem *item;
    int mask;
    const char *p, *p1;

    p = str;
    mask = 0;
    for(;;) {
        p1 = strchr(p, ',');
        if (!p1)
            p1 = p + strlen(p);
	if(cmp1(p,p1-p,"all")) {
		for(item = cpu_log_items; item->mask != 0; item++) {
			mask |= item->mask;
		}
	} else {
        for(item = cpu_log_items; item->mask != 0; item++) {
            if (cmp1(p, p1 - p, item->name))
                goto found;
        }
        return 0;
	}
    found:
        mask |= item->mask;
        if (*p1 != ',')
            break;
        p = p1 + 1;
    }
    return mask;
}

void cpu_abort(CPUState *env, const char *fmt, ...)
{
    va_list ap;
    va_list ap2;

    va_start(ap, fmt);
    va_copy(ap2, ap);
    fprintf(stderr, "qemu: fatal: ");
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
#ifdef TARGET_I386
    cpu_dump_state(env, stderr, fprintf, X86_DUMP_FPU | X86_DUMP_CCOP);
#else
    cpu_dump_state(env, stderr, fprintf, 0);
#endif
    if (qemu_log_enabled()) {
        qemu_log("qemu: fatal: ");
        qemu_log_vprintf(fmt, ap2);
        qemu_log("\n");
#ifdef TARGET_I386
        log_cpu_state(env, X86_DUMP_FPU | X86_DUMP_CCOP);
#else
        log_cpu_state(env, 0);
#endif
        qemu_log_flush();
        qemu_log_close();
    }
    va_end(ap2);
    va_end(ap);
#if defined(CONFIG_USER_ONLY)
    {
        struct sigaction act;
        sigfillset(&act.sa_mask);
        act.sa_handler = SIG_DFL;
        sigaction(SIGABRT, &act, NULL);
    }
#endif
    abort();
}

CPUState *cpu_copy(CPUState *env)
{
    CPUState *new_env = cpu_init(env->cpu_model_str);
    CPUState *next_cpu = new_env->next_cpu;
    int cpu_index = new_env->cpu_index;
#if defined(TARGET_HAS_ICE)
    CPUBreakpoint *bp;
    CPUWatchpoint *wp;
#endif

    memcpy(new_env, env, sizeof(CPUState));

    /* Preserve chaining and index. */
    new_env->next_cpu = next_cpu;
    new_env->cpu_index = cpu_index;

    /* Clone all break/watchpoints.
       Note: Once we support ptrace with hw-debug register access, make sure
       BP_CPU break/watchpoints are handled correctly on clone. */
    QTAILQ_INIT(&env->breakpoints);
    QTAILQ_INIT(&env->watchpoints);
#if defined(TARGET_HAS_ICE)
    QTAILQ_FOREACH(bp, &env->breakpoints, entry) {
        cpu_breakpoint_insert(new_env, bp->pc, bp->flags, NULL);
    }
    QTAILQ_FOREACH(wp, &env->watchpoints, entry) {
        cpu_watchpoint_insert(new_env, wp->vaddr, (~wp->len_mask) + 1,
                              wp->flags, NULL);
    }
#endif

    return new_env;
}

#endif /* !VBOX */
#if !defined(CONFIG_USER_ONLY)

static inline void tlb_flush_jmp_cache(CPUState *env, target_ulong addr)
{
    unsigned int i;

    /* Discard jump cache entries for any tb which might potentially
       overlap the flushed page.  */
    i = tb_jmp_cache_hash_page(addr - TARGET_PAGE_SIZE);
    memset (&env->tb_jmp_cache[i], 0,
	    TB_JMP_PAGE_SIZE * sizeof(TranslationBlock *));

    i = tb_jmp_cache_hash_page(addr);
    memset (&env->tb_jmp_cache[i], 0,
	    TB_JMP_PAGE_SIZE * sizeof(TranslationBlock *));
#ifdef VBOX

    /* inform raw mode about TLB page flush */
    remR3FlushPage(env, addr);
#endif /* VBOX */
}

static CPUTLBEntry s_cputlb_empty_entry = {
    .addr_read  = -1,
    .addr_write = -1,
    .addr_code  = -1,
    .addend     = -1,
};

/* NOTE: if flush_global is true, also flush global entries (not
   implemented yet) */
void tlb_flush(CPUState *env, int flush_global)
{
    int i;

#ifdef VBOX
    Assert(EMRemIsLockOwner(env->pVM));
    ASMAtomicAndS32((int32_t volatile *)&env->interrupt_request, ~CPU_INTERRUPT_EXTERNAL_FLUSH_TLB);
#endif

#if defined(DEBUG_TLB)
    printf("tlb_flush:\n");
#endif
    /* must reset current TB so that interrupts cannot modify the
       links while we are modifying them */
    env->current_tb = NULL;

    for(i = 0; i < CPU_TLB_SIZE; i++) {
        int mmu_idx;
        for (mmu_idx = 0; mmu_idx < NB_MMU_MODES; mmu_idx++) {
            env->tlb_table[mmu_idx][i] = s_cputlb_empty_entry;
        }
    }

    memset (env->tb_jmp_cache, 0, TB_JMP_CACHE_SIZE * sizeof (void *));

    env->tlb_flush_addr = -1;
    env->tlb_flush_mask = 0;
    tlb_flush_count++;
#ifdef VBOX

    /* inform raw mode about TLB flush */
    remR3FlushTLB(env, flush_global);
#endif /* VBOX */
}

static inline void tlb_flush_entry(CPUTLBEntry *tlb_entry, target_ulong addr)
{
    if (addr == (tlb_entry->addr_read &
                 (TARGET_PAGE_MASK | TLB_INVALID_MASK)) ||
        addr == (tlb_entry->addr_write &
                 (TARGET_PAGE_MASK | TLB_INVALID_MASK)) ||
        addr == (tlb_entry->addr_code &
                 (TARGET_PAGE_MASK | TLB_INVALID_MASK))) {
        *tlb_entry = s_cputlb_empty_entry;
    }
}

void tlb_flush_page(CPUState *env, target_ulong addr)
{
    int i;
    int mmu_idx;

    Assert(EMRemIsLockOwner(env->pVM));
#if defined(DEBUG_TLB)
    printf("tlb_flush_page: " TARGET_FMT_lx "\n", addr);
#endif
    /* Check if we need to flush due to large pages.  */
    if ((addr & env->tlb_flush_mask) == env->tlb_flush_addr) {
#if defined(DEBUG_TLB)
        printf("tlb_flush_page: forced full flush ("
               TARGET_FMT_lx "/" TARGET_FMT_lx ")\n",
               env->tlb_flush_addr, env->tlb_flush_mask);
#endif
        tlb_flush(env, 1);
        return;
    }
    /* must reset current TB so that interrupts cannot modify the
       links while we are modifying them */
    env->current_tb = NULL;

    addr &= TARGET_PAGE_MASK;
    i = (addr >> TARGET_PAGE_BITS) & (CPU_TLB_SIZE - 1);
    for (mmu_idx = 0; mmu_idx < NB_MMU_MODES; mmu_idx++)
        tlb_flush_entry(&env->tlb_table[mmu_idx][i], addr);

    tlb_flush_jmp_cache(env, addr);
}

/* update the TLBs so that writes to code in the virtual page 'addr'
   can be detected */
static void tlb_protect_code(ram_addr_t ram_addr)
{
    cpu_physical_memory_reset_dirty(ram_addr,
                                    ram_addr + TARGET_PAGE_SIZE,
                                    CODE_DIRTY_FLAG);
#if defined(VBOX) && defined(REM_MONITOR_CODE_PAGES)
    /** @todo Retest this? This function has changed... */
    remR3ProtectCode(cpu_single_env, ram_addr);
#endif /* VBOX */
}

/* update the TLB so that writes in physical page 'phys_addr' are no longer
   tested for self modifying code */
static void tlb_unprotect_code_phys(CPUState *env, ram_addr_t ram_addr,
                                    target_ulong vaddr)
{
    cpu_physical_memory_set_dirty_flags(ram_addr, CODE_DIRTY_FLAG);
}

static inline void tlb_reset_dirty_range(CPUTLBEntry *tlb_entry,
                                         uintptr_t start, uintptr_t length)
{
    uintptr_t addr;
#ifdef VBOX

    if (start & 3)
        return;
#endif /* VBOX */
    if ((tlb_entry->addr_write & ~TARGET_PAGE_MASK) == IO_MEM_RAM) {
        addr = (tlb_entry->addr_write & TARGET_PAGE_MASK) + tlb_entry->addend;
        if ((addr - start) < length) {
            tlb_entry->addr_write = (tlb_entry->addr_write & TARGET_PAGE_MASK) | TLB_NOTDIRTY;
        }
    }
}

/* Note: start and end must be within the same ram block.  */
void cpu_physical_memory_reset_dirty(ram_addr_t start, ram_addr_t end,
                                     int dirty_flags)
{
    CPUState *env;
    uintptr_t length, start1;
    int i;

    start &= TARGET_PAGE_MASK;
    end = TARGET_PAGE_ALIGN(end);

    length = end - start;
    if (length == 0)
        return;
    cpu_physical_memory_mask_dirty_range(start, length, dirty_flags);

    /* we modify the TLB cache so that the dirty bit will be set again
       when accessing the range */
#if defined(VBOX) && defined(REM_PHYS_ADDR_IN_TLB)
    start1 = start;
#elif !defined(VBOX)
    start1 = (uintptr_t)qemu_get_ram_ptr(start);
    /* Chek that we don't span multiple blocks - this breaks the
       address comparisons below.  */
    if ((uintptr_t)qemu_get_ram_ptr(end - 1) - start1
            != (end - 1) - start) {
        abort();
    }
#else
    start1 = (uintptr_t)remR3TlbGCPhys2Ptr(first_cpu, start, 1 /*fWritable*/); /** @todo page replacing (sharing or read only) may cause trouble, fix interface/whatever. */
#endif

    for(env = first_cpu; env != NULL; env = env->next_cpu) {
        int mmu_idx;
        for (mmu_idx = 0; mmu_idx < NB_MMU_MODES; mmu_idx++) {
            for(i = 0; i < CPU_TLB_SIZE; i++)
                tlb_reset_dirty_range(&env->tlb_table[mmu_idx][i],
                                      start1, length);
        }
    }
}

#ifndef VBOX

int cpu_physical_memory_set_dirty_tracking(int enable)
{
    int ret = 0;
    in_migration = enable;
    ret = cpu_notify_migration_log(!!enable);
    return ret;
}

int cpu_physical_memory_get_dirty_tracking(void)
{
    return in_migration;
}

#endif /* !VBOX */

int cpu_physical_sync_dirty_bitmap(target_phys_addr_t start_addr,
                                   target_phys_addr_t end_addr)
{
#ifndef VBOX
    int ret;

    ret = cpu_notify_sync_dirty_bitmap(start_addr, end_addr);
    return ret;
#else  /* VBOX */
    return 0;
#endif /* VBOX */
}

#if defined(VBOX) && !defined(REM_PHYS_ADDR_IN_TLB)
DECLINLINE(void) tlb_update_dirty(CPUTLBEntry *tlb_entry, target_phys_addr_t phys_addend)
#else
static inline void tlb_update_dirty(CPUTLBEntry *tlb_entry)
#endif
{
    ram_addr_t ram_addr;
#ifndef VBOX
    void *p;
#endif

    if ((tlb_entry->addr_write & ~TARGET_PAGE_MASK) == IO_MEM_RAM) {
#if defined(VBOX) && defined(REM_PHYS_ADDR_IN_TLB)
        ram_addr = (tlb_entry->addr_write & TARGET_PAGE_MASK) + tlb_entry->addend;
#elif !defined(VBOX)
        p = (void *)(uintptr_t)((tlb_entry->addr_write & TARGET_PAGE_MASK)
            + tlb_entry->addend);
        ram_addr = qemu_ram_addr_from_host(p);
#else
        Assert(phys_addend != -1);
        ram_addr = (tlb_entry->addr_write & TARGET_PAGE_MASK) + phys_addend;
#endif
        if (!cpu_physical_memory_is_dirty(ram_addr)) {
            tlb_entry->addr_write |= TLB_NOTDIRTY;
        }
    }
}

/* update the TLB according to the current state of the dirty bits */
void cpu_tlb_update_dirty(CPUState *env)
{
    int i;
    int mmu_idx;
    for (mmu_idx = 0; mmu_idx < NB_MMU_MODES; mmu_idx++) {
        for(i = 0; i < CPU_TLB_SIZE; i++)
#if defined(VBOX) && !defined(REM_PHYS_ADDR_IN_TLB)
            tlb_update_dirty(&env->tlb_table[mmu_idx][i], env->phys_addends[mmu_idx][i]);
#else
            tlb_update_dirty(&env->tlb_table[mmu_idx][i]);
#endif
    }
}

static inline void tlb_set_dirty1(CPUTLBEntry *tlb_entry, target_ulong vaddr)
{
    if (tlb_entry->addr_write == (vaddr | TLB_NOTDIRTY))
        tlb_entry->addr_write = vaddr;
}

/* update the TLB corresponding to virtual page vaddr
   so that it is no longer dirty */
static inline void tlb_set_dirty(CPUState *env, target_ulong vaddr)
{
    int i;
    int mmu_idx;

    vaddr &= TARGET_PAGE_MASK;
    i = (vaddr >> TARGET_PAGE_BITS) & (CPU_TLB_SIZE - 1);
    for (mmu_idx = 0; mmu_idx < NB_MMU_MODES; mmu_idx++)
        tlb_set_dirty1(&env->tlb_table[mmu_idx][i], vaddr);
}

/* Our TLB does not support large pages, so remember the area covered by
   large pages and trigger a full TLB flush if these are invalidated.  */
static void tlb_add_large_page(CPUState *env, target_ulong vaddr,
                               target_ulong size)
{
    target_ulong mask = ~(size - 1);

    if (env->tlb_flush_addr == (target_ulong)-1) {
        env->tlb_flush_addr = vaddr & mask;
        env->tlb_flush_mask = mask;
        return;
    }
    /* Extend the existing region to include the new page.
       This is a compromise between unnecessary flushes and the cost
       of maintaining a full variable size TLB.  */
    mask &= env->tlb_flush_mask;
    while (((env->tlb_flush_addr ^ vaddr) & mask) != 0) {
        mask <<= 1;
    }
    env->tlb_flush_addr &= mask;
    env->tlb_flush_mask = mask;
}

/* Add a new TLB entry. At most one entry for a given virtual address
   is permitted. Only a single TARGET_PAGE_SIZE region is mapped, the
   supplied size is only used by tlb_flush_page.  */
void tlb_set_page(CPUState *env, target_ulong vaddr,
                  target_phys_addr_t paddr, int prot,
                  int mmu_idx, target_ulong size)
{
    PhysPageDesc *p;
    ram_addr_t pd;
    unsigned int index;
    target_ulong address;
    target_ulong code_address;
    uintptr_t addend;
    CPUTLBEntry *te;
    CPUWatchpoint *wp;
    target_phys_addr_t iotlb;
#if defined(VBOX) && !defined(REM_PHYS_ADDR_IN_TLB)
    int read_mods = 0, write_mods = 0, code_mods = 0;
#endif

    assert(size >= TARGET_PAGE_SIZE);
    if (size != TARGET_PAGE_SIZE) {
        tlb_add_large_page(env, vaddr, size);
    }
    p = phys_page_find(paddr >> TARGET_PAGE_BITS);
    if (!p) {
        pd = IO_MEM_UNASSIGNED;
    } else {
        pd = p->phys_offset;
    }
#if defined(DEBUG_TLB)
    printf("tlb_set_page: vaddr=" TARGET_FMT_lx " paddr=0x%08x prot=%x idx=%d size=" TARGET_FMT_lx " pd=0x%08lx\n",
           vaddr, (int)paddr, prot, mmu_idx, size, (long)pd);
#endif

    address = vaddr;
    if ((pd & ~TARGET_PAGE_MASK) > IO_MEM_ROM && !(pd & IO_MEM_ROMD)) {
        /* IO memory case (romd handled later) */
        address |= TLB_MMIO;
    }
#if defined(VBOX) && defined(REM_PHYS_ADDR_IN_TLB)
    addend = pd & TARGET_PAGE_MASK;
#elif !defined(VBOX)
    addend = (uintptr_t)qemu_get_ram_ptr(pd & TARGET_PAGE_MASK);
#else
    /** @todo this is racing the phys_page_find call above since it may register
     *        a new chunk of memory...  */
    addend = (uintptr_t)remR3TlbGCPhys2Ptr(env, pd & TARGET_PAGE_MASK, !!(prot & PAGE_WRITE));
#endif

    if ((pd & ~TARGET_PAGE_MASK) <= IO_MEM_ROM) {
        /* Normal RAM.  */
        iotlb = pd & TARGET_PAGE_MASK;
        if ((pd & ~TARGET_PAGE_MASK) == IO_MEM_RAM)
            iotlb |= IO_MEM_NOTDIRTY;
        else
            iotlb |= IO_MEM_ROM;
    } else {
        /* IO handlers are currently passed a physical address.
           It would be nice to pass an offset from the base address
           of that region.  This would avoid having to special case RAM,
           and avoid full address decoding in every device.
           We can't use the high bits of pd for this because
           IO_MEM_ROMD uses these as a ram address.  */
        iotlb = (pd & ~TARGET_PAGE_MASK);
        if (p) {
            iotlb += p->region_offset;
        } else {
            iotlb += paddr;
        }
    }

    code_address = address;
#if defined(VBOX) && !defined(REM_PHYS_ADDR_IN_TLB)

    if (addend & 0x3)
    {
        if (addend & 0x2)
        {
            /* catch write */
            if ((pd & ~TARGET_PAGE_MASK) <= IO_MEM_ROM)
                write_mods |= TLB_MMIO;
        }
        else if (addend & 0x1)
        {
            /* catch all */
            if ((pd & ~TARGET_PAGE_MASK) <= IO_MEM_ROM)
            {
                read_mods |= TLB_MMIO;
                write_mods |= TLB_MMIO;
                code_mods |= TLB_MMIO;
            }
        }
        if ((iotlb & ~TARGET_PAGE_MASK) == 0)
            iotlb = env->pVM->rem.s.iHandlerMemType  + paddr;
        addend &= ~(target_ulong)0x3;
    }

#endif
    /* Make accesses to pages with watchpoints go via the
       watchpoint trap routines.  */
    QTAILQ_FOREACH(wp, &env->watchpoints, entry) {
        if (vaddr == (wp->vaddr & TARGET_PAGE_MASK)) {
            /* Avoid trapping reads of pages with a write breakpoint. */
            if ((prot & PAGE_WRITE) || (wp->flags & BP_MEM_READ)) {
                iotlb = io_mem_watch + paddr;
                address |= TLB_MMIO;
                break;
            }
        }
    }

    index = (vaddr >> TARGET_PAGE_BITS) & (CPU_TLB_SIZE - 1);
    env->iotlb[mmu_idx][index] = iotlb - vaddr;
    te = &env->tlb_table[mmu_idx][index];
    te->addend = addend - vaddr;
    if (prot & PAGE_READ) {
        te->addr_read = address;
    } else {
        te->addr_read = -1;
    }

    if (prot & PAGE_EXEC) {
        te->addr_code = code_address;
    } else {
        te->addr_code = -1;
    }
    if (prot & PAGE_WRITE) {
        if ((pd & ~TARGET_PAGE_MASK) == IO_MEM_ROM ||
            (pd & IO_MEM_ROMD)) {
            /* Write access calls the I/O callback.  */
            te->addr_write = address | TLB_MMIO;
        } else if ((pd & ~TARGET_PAGE_MASK) == IO_MEM_RAM &&
                   !cpu_physical_memory_is_dirty(pd)) {
            te->addr_write = address | TLB_NOTDIRTY;
        } else {
            te->addr_write = address;
        }
    } else {
        te->addr_write = -1;
    }

#if defined(VBOX) && !defined(REM_PHYS_ADDR_IN_TLB)
    if (prot & PAGE_READ)
        te->addr_read |= read_mods;
    if (prot & PAGE_EXEC)
        te->addr_code |= code_mods;
    if (prot & PAGE_WRITE)
        te->addr_write |= write_mods;

    env->phys_addends[mmu_idx][index] = (pd & TARGET_PAGE_MASK)- vaddr;
#endif

#ifdef VBOX
    /* inform raw mode about TLB page change */
    remR3FlushPage(env, vaddr);
#endif
}

#else

void tlb_flush(CPUState *env, int flush_global)
{
}

void tlb_flush_page(CPUState *env, target_ulong addr)
{
}

/*
 * Walks guest process memory "regions" one by one
 * and calls callback function 'fn' for each region.
 */

struct walk_memory_regions_data
{
    walk_memory_regions_fn fn;
    void *priv;
    uintptr_t start;
    int prot;
};

static int walk_memory_regions_end(struct walk_memory_regions_data *data,
                                   abi_ulong end, int new_prot)
{
    if (data->start != -1ul) {
        int rc = data->fn(data->priv, data->start, end, data->prot);
        if (rc != 0) {
            return rc;
        }
    }

    data->start = (new_prot ? end : -1ul);
    data->prot = new_prot;

    return 0;
}

static int walk_memory_regions_1(struct walk_memory_regions_data *data,
                                 abi_ulong base, int level, void **lp)
{
    abi_ulong pa;
    int i, rc;

    if (*lp == NULL) {
        return walk_memory_regions_end(data, base, 0);
    }

    if (level == 0) {
        PageDesc *pd = *lp;
        for (i = 0; i < L2_SIZE; ++i) {
            int prot = pd[i].flags;

            pa = base | (i << TARGET_PAGE_BITS);
            if (prot != data->prot) {
                rc = walk_memory_regions_end(data, pa, prot);
                if (rc != 0) {
                    return rc;
                }
            }
        }
    } else {
        void **pp = *lp;
        for (i = 0; i < L2_SIZE; ++i) {
            pa = base | ((abi_ulong)i <<
                (TARGET_PAGE_BITS + L2_BITS * level));
            rc = walk_memory_regions_1(data, pa, level - 1, pp + i);
            if (rc != 0) {
                return rc;
            }
        }
    }

    return 0;
}

int walk_memory_regions(void *priv, walk_memory_regions_fn fn)
{
    struct walk_memory_regions_data data;
    target_ulong i;

    data.fn = fn;
    data.priv = priv;
    data.start = -1ul;
    data.prot = 0;

    for (i = 0; i < V_L1_SIZE; i++) {
        int rc = walk_memory_regions_1(&data, (abi_ulong)i << V_L1_SHIFT,
                                       V_L1_SHIFT / L2_BITS - 1, l1_map + i);
        if (rc != 0) {
            return rc;
        }
    }

    return walk_memory_regions_end(&data, 0, 0);
}

static int dump_region(void *priv, abi_ulong start,
    abi_ulong end, unsigned long prot)
{
    FILE *f = (FILE *)priv;

    (void) fprintf(f, TARGET_ABI_FMT_lx"-"TARGET_ABI_FMT_lx
        " "TARGET_ABI_FMT_lx" %c%c%c\n",
        start, end, end - start,
        ((prot & PAGE_READ) ? 'r' : '-'),
        ((prot & PAGE_WRITE) ? 'w' : '-'),
        ((prot & PAGE_EXEC) ? 'x' : '-'));

    return (0);
}

/* dump memory mappings */
void page_dump(FILE *f)
{
    (void) fprintf(f, "%-8s %-8s %-8s %s\n",
            "start", "end", "size", "prot");
    walk_memory_regions(f, dump_region);
}

int page_get_flags(target_ulong address)
{
    PageDesc *p;

    p = page_find(address >> TARGET_PAGE_BITS);
    if (!p)
        return 0;
    return p->flags;
}

/* Modify the flags of a page and invalidate the code if necessary.
   The flag PAGE_WRITE_ORG is positioned automatically depending
   on PAGE_WRITE.  The mmap_lock should already be held.  */
void page_set_flags(target_ulong start, target_ulong end, int flags)
{
    target_ulong addr, len;

    /* This function should never be called with addresses outside the
       guest address space.  If this assert fires, it probably indicates
       a missing call to h2g_valid.  */
#if TARGET_ABI_BITS > L1_MAP_ADDR_SPACE_BITS
    assert(end < ((abi_ulong)1 << L1_MAP_ADDR_SPACE_BITS));
#endif
    assert(start < end);

    start = start & TARGET_PAGE_MASK;
    end = TARGET_PAGE_ALIGN(end);

    if (flags & PAGE_WRITE) {
        flags |= PAGE_WRITE_ORG;
    }

#ifdef VBOX
    AssertMsgFailed(("We shouldn't be here, and if we should, we must have an env to do the proper locking!\n"));
#endif
    for (addr = start, len = end - start;
         len != 0;
         len -= TARGET_PAGE_SIZE, addr += TARGET_PAGE_SIZE) {
        PageDesc *p = page_find_alloc(addr >> TARGET_PAGE_BITS, 1);

        /* If the write protection bit is set, then we invalidate
           the code inside.  */
        if (!(p->flags & PAGE_WRITE) &&
            (flags & PAGE_WRITE) &&
            p->first_tb) {
            tb_invalidate_phys_page(addr, 0, NULL);
        }
        p->flags = flags;
    }
}

int page_check_range(target_ulong start, target_ulong len, int flags)
{
    PageDesc *p;
    target_ulong end;
    target_ulong addr;

    /* This function should never be called with addresses outside the
       guest address space.  If this assert fires, it probably indicates
       a missing call to h2g_valid.  */
#if TARGET_ABI_BITS > L1_MAP_ADDR_SPACE_BITS
    assert(start < ((abi_ulong)1 << L1_MAP_ADDR_SPACE_BITS));
#endif

    if (len == 0) {
        return 0;
    }
    if (start + len - 1 < start) {
        /* We've wrapped around.  */
        return -1;
    }

    end = TARGET_PAGE_ALIGN(start+len); /* must do before we loose bits in the next step */
    start = start & TARGET_PAGE_MASK;

    for (addr = start, len = end - start;
         len != 0;
         len -= TARGET_PAGE_SIZE, addr += TARGET_PAGE_SIZE) {
        p = page_find(addr >> TARGET_PAGE_BITS);
        if( !p )
            return -1;
        if( !(p->flags & PAGE_VALID) )
            return -1;

        if ((flags & PAGE_READ) && !(p->flags & PAGE_READ))
            return -1;
        if (flags & PAGE_WRITE) {
            if (!(p->flags & PAGE_WRITE_ORG))
                return -1;
            /* unprotect the page if it was put read-only because it
               contains translated code */
            if (!(p->flags & PAGE_WRITE)) {
                if (!page_unprotect(addr, 0, NULL))
                    return -1;
            }
            return 0;
        }
    }
    return 0;
}

/* called from signal handler: invalidate the code and unprotect the
   page. Return TRUE if the fault was successfully handled. */
int page_unprotect(target_ulong address, uintptr_t pc, void *puc)
{
    unsigned int prot;
    PageDesc *p;
    target_ulong host_start, host_end, addr;

    /* Technically this isn't safe inside a signal handler.  However we
       know this only ever happens in a synchronous SEGV handler, so in
       practice it seems to be ok.  */
    mmap_lock();

    p = page_find(address >> TARGET_PAGE_BITS);
    if (!p) {
        mmap_unlock();
        return 0;
    }

    /* if the page was really writable, then we change its
       protection back to writable */
    if ((p->flags & PAGE_WRITE_ORG) && !(p->flags & PAGE_WRITE)) {
        host_start = address & qemu_host_page_mask;
        host_end = host_start + qemu_host_page_size;

        prot = 0;
        for (addr = host_start ; addr < host_end ; addr += TARGET_PAGE_SIZE) {
            p = page_find(addr >> TARGET_PAGE_BITS);
            p->flags |= PAGE_WRITE;
            prot |= p->flags;

            /* and since the content will be modified, we must invalidate
               the corresponding translated code. */
            tb_invalidate_phys_page(addr, pc, puc);
#ifdef DEBUG_TB_CHECK
            tb_invalidate_check(addr);
#endif
        }
        mprotect((void *)g2h(host_start), qemu_host_page_size,
                 prot & PAGE_BITS);

        mmap_unlock();
        return 1;
    }
    mmap_unlock();
    return 0;
}

static inline void tlb_set_dirty(CPUState *env,
                                 uintptr_t addr, target_ulong vaddr)
{
}
#endif /* defined(CONFIG_USER_ONLY) */

#if !defined(CONFIG_USER_ONLY)

#define SUBPAGE_IDX(addr) ((addr) & ~TARGET_PAGE_MASK)
typedef struct subpage_t {
    target_phys_addr_t base;
    ram_addr_t sub_io_index[TARGET_PAGE_SIZE];
    ram_addr_t region_offset[TARGET_PAGE_SIZE];
} subpage_t;

static int subpage_register (subpage_t *mmio, uint32_t start, uint32_t end,
                             ram_addr_t memory, ram_addr_t region_offset);
static subpage_t *subpage_init (target_phys_addr_t base, ram_addr_t *phys,
                                ram_addr_t orig_memory,
                                ram_addr_t region_offset);
#define CHECK_SUBPAGE(addr, start_addr, start_addr2, end_addr, end_addr2, \
                      need_subpage)                                     \
    do {                                                                \
        if (addr > start_addr)                                          \
            start_addr2 = 0;                                            \
        else {                                                          \
            start_addr2 = start_addr & ~TARGET_PAGE_MASK;               \
            if (start_addr2 > 0)                                        \
                need_subpage = 1;                                       \
        }                                                               \
                                                                        \
        if ((start_addr + orig_size) - addr >= TARGET_PAGE_SIZE)        \
            end_addr2 = TARGET_PAGE_SIZE - 1;                           \
        else {                                                          \
            end_addr2 = (start_addr + orig_size - 1) & ~TARGET_PAGE_MASK; \
            if (end_addr2 < TARGET_PAGE_SIZE - 1)                       \
                need_subpage = 1;                                       \
        }                                                               \
    } while (0)

/* register physical memory.
   For RAM, 'size' must be a multiple of the target page size.
   If (phys_offset & ~TARGET_PAGE_MASK) != 0, then it is an
   io memory page.  The address used when calling the IO function is
   the offset from the start of the region, plus region_offset.  Both
   start_addr and region_offset are rounded down to a page boundary
   before calculating this offset.  This should not be a problem unless
   the low bits of start_addr and region_offset differ.  */
void cpu_register_physical_memory_offset(target_phys_addr_t start_addr,
                                         ram_addr_t size,
                                         ram_addr_t phys_offset,
                                         ram_addr_t region_offset)
{
    target_phys_addr_t addr, end_addr;
    PhysPageDesc *p;
    CPUState *env;
    ram_addr_t orig_size = size;
    subpage_t *subpage;

#ifndef VBOX
    cpu_notify_set_memory(start_addr, size, phys_offset);
#endif /* !VBOX */

    if (phys_offset == IO_MEM_UNASSIGNED) {
        region_offset = start_addr;
    }
    region_offset &= TARGET_PAGE_MASK;
    size = (size + TARGET_PAGE_SIZE - 1) & TARGET_PAGE_MASK;
    end_addr = start_addr + (target_phys_addr_t)size;
    for(addr = start_addr; addr != end_addr; addr += TARGET_PAGE_SIZE) {
        p = phys_page_find(addr >> TARGET_PAGE_BITS);
        if (p && p->phys_offset != IO_MEM_UNASSIGNED) {
            ram_addr_t orig_memory = p->phys_offset;
            target_phys_addr_t start_addr2, end_addr2;
            int need_subpage = 0;

            CHECK_SUBPAGE(addr, start_addr, start_addr2, end_addr, end_addr2,
                          need_subpage);
            if (need_subpage) {
                if (!(orig_memory & IO_MEM_SUBPAGE)) {
                    subpage = subpage_init((addr & TARGET_PAGE_MASK),
                                           &p->phys_offset, orig_memory,
                                           p->region_offset);
                } else {
                    subpage = io_mem_opaque[(orig_memory & ~TARGET_PAGE_MASK)
                                            >> IO_MEM_SHIFT];
                }
                subpage_register(subpage, start_addr2, end_addr2, phys_offset,
                                 region_offset);
                p->region_offset = 0;
            } else {
                p->phys_offset = phys_offset;
                if ((phys_offset & ~TARGET_PAGE_MASK) <= IO_MEM_ROM ||
                    (phys_offset & IO_MEM_ROMD))
                    phys_offset += TARGET_PAGE_SIZE;
            }
        } else {
            p = phys_page_find_alloc(addr >> TARGET_PAGE_BITS, 1);
            p->phys_offset = phys_offset;
            p->region_offset = region_offset;
            if ((phys_offset & ~TARGET_PAGE_MASK) <= IO_MEM_ROM ||
                (phys_offset & IO_MEM_ROMD)) {
                phys_offset += TARGET_PAGE_SIZE;
            } else {
                target_phys_addr_t start_addr2, end_addr2;
                int need_subpage = 0;

                CHECK_SUBPAGE(addr, start_addr, start_addr2, end_addr,
                              end_addr2, need_subpage);

                if (need_subpage) {
                    subpage = subpage_init((addr & TARGET_PAGE_MASK),
                                           &p->phys_offset, IO_MEM_UNASSIGNED,
                                           addr & TARGET_PAGE_MASK);
                    subpage_register(subpage, start_addr2, end_addr2,
                                     phys_offset, region_offset);
                    p->region_offset = 0;
                }
            }
        }
        region_offset += TARGET_PAGE_SIZE;
    }

    /* since each CPU stores ram addresses in its TLB cache, we must
       reset the modified entries */
#ifndef VBOX
    /* XXX: slow ! */
    for(env = first_cpu; env != NULL; env = env->next_cpu) {
        tlb_flush(env, 1);
    }
#else
    /* We have one thread per CPU, so, one of the other EMTs might be executing
       code right now and flushing the TLB may crash it. */
    env = first_cpu;
    if (EMRemIsLockOwner(env->pVM))
        tlb_flush(env, 1);
    else
        ASMAtomicOrS32((int32_t volatile *)&env->interrupt_request,
                       CPU_INTERRUPT_EXTERNAL_FLUSH_TLB);
#endif
}

/* XXX: temporary until new memory mapping API */
ram_addr_t cpu_get_physical_page_desc(target_phys_addr_t addr)
{
    PhysPageDesc *p;

    p = phys_page_find(addr >> TARGET_PAGE_BITS);
    if (!p)
        return IO_MEM_UNASSIGNED;
    return p->phys_offset;
}

#ifndef VBOX

void qemu_register_coalesced_mmio(target_phys_addr_t addr, ram_addr_t size)
{
    if (kvm_enabled())
        kvm_coalesce_mmio_region(addr, size);
}

void qemu_unregister_coalesced_mmio(target_phys_addr_t addr, ram_addr_t size)
{
    if (kvm_enabled())
        kvm_uncoalesce_mmio_region(addr, size);
}

void qemu_flush_coalesced_mmio_buffer(void)
{
    if (kvm_enabled())
        kvm_flush_coalesced_mmio_buffer();
}

#if defined(__linux__) && !defined(TARGET_S390X)

#include <sys/vfs.h>

#define HUGETLBFS_MAGIC       0x958458f6

static size_t gethugepagesize(const char *path)
{
    struct statfs fs;
    int ret;

    do {
	    ret = statfs(path, &fs);
    } while (ret != 0 && errno == EINTR);

    if (ret != 0) {
	    perror(path);
	    return 0;
    }

    if (fs.f_type != HUGETLBFS_MAGIC)
	    fprintf(stderr, "Warning: path not on HugeTLBFS: %s\n", path);

    return (size_t)fs.f_bsize;
}

static void *file_ram_alloc(RAMBlock *block,
                            ram_addr_t memory,
                            const char *path)
{
    char *filename;
    void *area;
    int fd;
#ifdef MAP_POPULATE
    int flags;
#endif
    size_t hpagesize;

    hpagesize = gethugepagesize(path);
    if (!hpagesize) {
	return NULL;
    }

    if (memory < hpagesize) {
        return NULL;
    }

    if (kvm_enabled() && !kvm_has_sync_mmu()) {
        fprintf(stderr, "host lacks kvm mmu notifiers, -mem-path unsupported\n");
        return NULL;
    }

    if (asprintf(&filename, "%s/qemu_back_mem.XXXXXX", path) == -1) {
	return NULL;
    }

    fd = mkstemp(filename);
    if (fd < 0) {
	perror("unable to create backing store for hugepages");
	free(filename);
	return NULL;
    }
    unlink(filename);
    free(filename);

    memory = (memory+hpagesize-1) & ~(hpagesize-1);

    /*
     * ftruncate is not supported by hugetlbfs in older
     * hosts, so don't bother bailing out on errors.
     * If anything goes wrong with it under other filesystems,
     * mmap will fail.
     */
    if (ftruncate(fd, memory))
	perror("ftruncate");

#ifdef MAP_POPULATE
    /* NB: MAP_POPULATE won't exhaustively alloc all phys pages in the case
     * MAP_PRIVATE is requested.  For mem_prealloc we mmap as MAP_SHARED
     * to sidestep this quirk.
     */
    flags = mem_prealloc ? MAP_POPULATE | MAP_SHARED : MAP_PRIVATE;
    area = mmap(0, memory, PROT_READ | PROT_WRITE, flags, fd, 0);
#else
    area = mmap(0, memory, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
#endif
    if (area == MAP_FAILED) {
	perror("file_ram_alloc: can't mmap RAM pages");
	close(fd);
	return (NULL);
    }
    block->fd = fd;
    return area;
}
#endif

static ram_addr_t find_ram_offset(ram_addr_t size)
{
    RAMBlock *block, *next_block;
    ram_addr_t offset = 0, mingap = ULONG_MAX;

    if (QLIST_EMPTY(&ram_list.blocks))
        return 0;

    QLIST_FOREACH(block, &ram_list.blocks, next) {
        ram_addr_t end, next = ULONG_MAX;

        end = block->offset + block->length;

        QLIST_FOREACH(next_block, &ram_list.blocks, next) {
            if (next_block->offset >= end) {
                next = MIN(next, next_block->offset);
            }
        }
        if (next - end >= size && next - end < mingap) {
            offset =  end;
            mingap = next - end;
        }
    }
    return offset;
}

static ram_addr_t last_ram_offset(void)
{
    RAMBlock *block;
    ram_addr_t last = 0;

    QLIST_FOREACH(block, &ram_list.blocks, next)
        last = MAX(last, block->offset + block->length);

    return last;
}

ram_addr_t qemu_ram_alloc_from_ptr(DeviceState *dev, const char *name,
                        ram_addr_t size, void *host)
{
    RAMBlock *new_block, *block;

    size = TARGET_PAGE_ALIGN(size);
    new_block = qemu_mallocz(sizeof(*new_block));

    if (dev && dev->parent_bus && dev->parent_bus->info->get_dev_path) {
        char *id = dev->parent_bus->info->get_dev_path(dev);
        if (id) {
            snprintf(new_block->idstr, sizeof(new_block->idstr), "%s/", id);
            qemu_free(id);
        }
    }
    pstrcat(new_block->idstr, sizeof(new_block->idstr), name);

    QLIST_FOREACH(block, &ram_list.blocks, next) {
        if (!strcmp(block->idstr, new_block->idstr)) {
            fprintf(stderr, "RAMBlock \"%s\" already registered, abort!\n",
                    new_block->idstr);
            abort();
        }
    }

    new_block->host = host;

    new_block->offset = find_ram_offset(size);
    new_block->length = size;

    QLIST_INSERT_HEAD(&ram_list.blocks, new_block, next);

    ram_list.phys_dirty = qemu_realloc(ram_list.phys_dirty,
                                       last_ram_offset() >> TARGET_PAGE_BITS);
    memset(ram_list.phys_dirty + (new_block->offset >> TARGET_PAGE_BITS),
           0xff, size >> TARGET_PAGE_BITS);

    if (kvm_enabled())
        kvm_setup_guest_memory(new_block->host, size);

    return new_block->offset;
}

ram_addr_t qemu_ram_alloc(DeviceState *dev, const char *name, ram_addr_t size)
{
    RAMBlock *new_block, *block;

    size = TARGET_PAGE_ALIGN(size);
    new_block = qemu_mallocz(sizeof(*new_block));

    if (dev && dev->parent_bus && dev->parent_bus->info->get_dev_path) {
        char *id = dev->parent_bus->info->get_dev_path(dev);
        if (id) {
            snprintf(new_block->idstr, sizeof(new_block->idstr), "%s/", id);
            qemu_free(id);
        }
    }
    pstrcat(new_block->idstr, sizeof(new_block->idstr), name);

    QLIST_FOREACH(block, &ram_list.blocks, next) {
        if (!strcmp(block->idstr, new_block->idstr)) {
            fprintf(stderr, "RAMBlock \"%s\" already registered, abort!\n",
                    new_block->idstr);
            abort();
        }
    }

    if (mem_path) {
#if defined (__linux__) && !defined(TARGET_S390X)
        new_block->host = file_ram_alloc(new_block, size, mem_path);
        if (!new_block->host) {
            new_block->host = qemu_vmalloc(size);
#ifdef MADV_MERGEABLE
            madvise(new_block->host, size, MADV_MERGEABLE);
#endif
        }
#else
        fprintf(stderr, "-mem-path option unsupported\n");
        exit(1);
#endif
    } else {
#if defined(TARGET_S390X) && defined(CONFIG_KVM)
        /* XXX S390 KVM requires the topmost vma of the RAM to be < 256GB */
        new_block->host = mmap((void*)0x1000000, size,
                                PROT_EXEC|PROT_READ|PROT_WRITE,
                                MAP_SHARED | MAP_ANONYMOUS, -1, 0);
#else
        new_block->host = qemu_vmalloc(size);
#endif
#ifdef MADV_MERGEABLE
        madvise(new_block->host, size, MADV_MERGEABLE);
#endif
    }
    new_block->offset = find_ram_offset(size);
    new_block->length = size;

    QLIST_INSERT_HEAD(&ram_list.blocks, new_block, next);

    ram_list.phys_dirty = qemu_realloc(ram_list.phys_dirty,
                                       last_ram_offset() >> TARGET_PAGE_BITS);
    memset(ram_list.phys_dirty + (new_block->offset >> TARGET_PAGE_BITS),
           0xff, size >> TARGET_PAGE_BITS);

    if (kvm_enabled())
        kvm_setup_guest_memory(new_block->host, size);

    return new_block->offset;
}

void qemu_ram_free(ram_addr_t addr)
{
    RAMBlock *block;

    QLIST_FOREACH(block, &ram_list.blocks, next) {
        if (addr == block->offset) {
            QLIST_REMOVE(block, next);
            if (mem_path) {
#if defined (__linux__) && !defined(TARGET_S390X)
                if (block->fd) {
                    munmap(block->host, block->length);
                    close(block->fd);
                } else {
                    qemu_vfree(block->host);
                }
#endif
            } else {
#if defined(TARGET_S390X) && defined(CONFIG_KVM)
                munmap(block->host, block->length);
#else
                qemu_vfree(block->host);
#endif
            }
            qemu_free(block);
            return;
        }
    }

}

/* Return a host pointer to ram allocated with qemu_ram_alloc.
   With the exception of the softmmu code in this file, this should
   only be used for local memory (e.g. video ram) that the device owns,
   and knows it isn't going to access beyond the end of the block.

   It should not be used for general purpose DMA.
   Use cpu_physical_memory_map/cpu_physical_memory_rw instead.
 */
void *qemu_get_ram_ptr(ram_addr_t addr)
{
    RAMBlock *block;

    QLIST_FOREACH(block, &ram_list.blocks, next) {
        if (addr - block->offset < block->length) {
            QLIST_REMOVE(block, next);
            QLIST_INSERT_HEAD(&ram_list.blocks, block, next);
            return block->host + (addr - block->offset);
        }
    }

    fprintf(stderr, "Bad ram offset %" PRIx64 "\n", (uint64_t)addr);
    abort();

    return NULL;
}

/* Some of the softmmu routines need to translate from a host pointer
   (typically a TLB entry) back to a ram offset.  */
ram_addr_t qemu_ram_addr_from_host(void *ptr)
{
    RAMBlock *block;
    uint8_t *host = ptr;

    QLIST_FOREACH(block, &ram_list.blocks, next) {
        if (host - block->host < block->length) {
            return block->offset + (host - block->host);
        }
    }

    fprintf(stderr, "Bad ram pointer %p\n", ptr);
    abort();

    return 0;
}

#endif /* !VBOX */

static uint32_t unassigned_mem_readb(void *opaque, target_phys_addr_t addr)
{
#ifdef DEBUG_UNASSIGNED
    printf("Unassigned mem read " TARGET_FMT_plx "\n", addr);
#endif
#if defined(TARGET_SPARC) || defined(TARGET_MICROBLAZE)
    do_unassigned_access(addr, 0, 0, 0, 1);
#endif
    return 0;
}

static uint32_t unassigned_mem_readw(void *opaque, target_phys_addr_t addr)
{
#ifdef DEBUG_UNASSIGNED
    printf("Unassigned mem read " TARGET_FMT_plx "\n", addr);
#endif
#if defined(TARGET_SPARC) || defined(TARGET_MICROBLAZE)
    do_unassigned_access(addr, 0, 0, 0, 2);
#endif
    return 0;
}

static uint32_t unassigned_mem_readl(void *opaque, target_phys_addr_t addr)
{
#ifdef DEBUG_UNASSIGNED
    printf("Unassigned mem read " TARGET_FMT_plx "\n", addr);
#endif
#if defined(TARGET_SPARC) || defined(TARGET_MICROBLAZE)
    do_unassigned_access(addr, 0, 0, 0, 4);
#endif
    return 0;
}

static void unassigned_mem_writeb(void *opaque, target_phys_addr_t addr, uint32_t val)
{
#ifdef DEBUG_UNASSIGNED
    printf("Unassigned mem write " TARGET_FMT_plx " = 0x%x\n", addr, val);
#endif
#if defined(TARGET_SPARC) || defined(TARGET_MICROBLAZE)
    do_unassigned_access(addr, 1, 0, 0, 1);
#endif
}

static void unassigned_mem_writew(void *opaque, target_phys_addr_t addr, uint32_t val)
{
#ifdef DEBUG_UNASSIGNED
    printf("Unassigned mem write " TARGET_FMT_plx " = 0x%x\n", addr, val);
#endif
#if defined(TARGET_SPARC) || defined(TARGET_MICROBLAZE)
    do_unassigned_access(addr, 1, 0, 0, 2);
#endif
}

static void unassigned_mem_writel(void *opaque, target_phys_addr_t addr, uint32_t val)
{
#ifdef DEBUG_UNASSIGNED
    printf("Unassigned mem write " TARGET_FMT_plx " = 0x%x\n", addr, val);
#endif
#if defined(TARGET_SPARC) || defined(TARGET_MICROBLAZE)
    do_unassigned_access(addr, 1, 0, 0, 4);
#endif
}

static CPUReadMemoryFunc * const unassigned_mem_read[3] = {
    unassigned_mem_readb,
    unassigned_mem_readw,
    unassigned_mem_readl,
};

static CPUWriteMemoryFunc * const unassigned_mem_write[3] = {
    unassigned_mem_writeb,
    unassigned_mem_writew,
    unassigned_mem_writel,
};

static void notdirty_mem_writeb(void *opaque, target_phys_addr_t ram_addr,
                                uint32_t val)
{
    int dirty_flags;
    dirty_flags = cpu_physical_memory_get_dirty_flags(ram_addr);
    if (!(dirty_flags & CODE_DIRTY_FLAG)) {
#if !defined(CONFIG_USER_ONLY)
        tb_invalidate_phys_page_fast(ram_addr, 1);
        dirty_flags = cpu_physical_memory_get_dirty_flags(ram_addr);
#endif
    }
#if defined(VBOX) && !defined(REM_PHYS_ADDR_IN_TLB)
    remR3PhysWriteU8(ram_addr, val);
#else
    stb_p(qemu_get_ram_ptr(ram_addr), val);
#endif
    dirty_flags |= (0xff & ~CODE_DIRTY_FLAG);
    cpu_physical_memory_set_dirty_flags(ram_addr, dirty_flags);
    /* we remove the notdirty callback only if the code has been
       flushed */
    if (dirty_flags == 0xff)
        tlb_set_dirty(cpu_single_env, cpu_single_env->mem_io_vaddr);
}

static void notdirty_mem_writew(void *opaque, target_phys_addr_t ram_addr,
                                uint32_t val)
{
    int dirty_flags;
    dirty_flags = cpu_physical_memory_get_dirty_flags(ram_addr);
    if (!(dirty_flags & CODE_DIRTY_FLAG)) {
#if !defined(CONFIG_USER_ONLY)
        tb_invalidate_phys_page_fast(ram_addr, 2);
        dirty_flags = cpu_physical_memory_get_dirty_flags(ram_addr);
#endif
    }
#if defined(VBOX) && !defined(REM_PHYS_ADDR_IN_TLB)
    remR3PhysWriteU16(ram_addr, val);
#else
    stw_p(qemu_get_ram_ptr(ram_addr), val);
#endif
    dirty_flags |= (0xff & ~CODE_DIRTY_FLAG);
    cpu_physical_memory_set_dirty_flags(ram_addr, dirty_flags);
    /* we remove the notdirty callback only if the code has been
       flushed */
    if (dirty_flags == 0xff)
        tlb_set_dirty(cpu_single_env, cpu_single_env->mem_io_vaddr);
}

static void notdirty_mem_writel(void *opaque, target_phys_addr_t ram_addr,
                                uint32_t val)
{
    int dirty_flags;
    dirty_flags = cpu_physical_memory_get_dirty_flags(ram_addr);
    if (!(dirty_flags & CODE_DIRTY_FLAG)) {
#if !defined(CONFIG_USER_ONLY)
        tb_invalidate_phys_page_fast(ram_addr, 4);
        dirty_flags = cpu_physical_memory_get_dirty_flags(ram_addr);
#endif
    }
#if defined(VBOX) && !defined(REM_PHYS_ADDR_IN_TLB)
    remR3PhysWriteU32(ram_addr, val);
#else
    stl_p(qemu_get_ram_ptr(ram_addr), val);
#endif
    dirty_flags |= (0xff & ~CODE_DIRTY_FLAG);
    cpu_physical_memory_set_dirty_flags(ram_addr, dirty_flags);
    /* we remove the notdirty callback only if the code has been
       flushed */
    if (dirty_flags == 0xff)
        tlb_set_dirty(cpu_single_env, cpu_single_env->mem_io_vaddr);
}

static CPUReadMemoryFunc * const error_mem_read[3] = {
    NULL, /* never used */
    NULL, /* never used */
    NULL, /* never used */
};

static CPUWriteMemoryFunc * const notdirty_mem_write[3] = {
    notdirty_mem_writeb,
    notdirty_mem_writew,
    notdirty_mem_writel,
};

/* Generate a debug exception if a watchpoint has been hit.  */
static void check_watchpoint(int offset, int len_mask, int flags)
{
    CPUState *env = cpu_single_env;
    target_ulong pc, cs_base;
    TranslationBlock *tb;
    target_ulong vaddr;
    CPUWatchpoint *wp;
    int cpu_flags;

    if (env->watchpoint_hit) {
        /* We re-entered the check after replacing the TB. Now raise
         * the debug interrupt so that is will trigger after the
         * current instruction. */
        cpu_interrupt(env, CPU_INTERRUPT_DEBUG);
        return;
    }
    vaddr = (env->mem_io_vaddr & TARGET_PAGE_MASK) + offset;
    QTAILQ_FOREACH(wp, &env->watchpoints, entry) {
        if ((vaddr == (wp->vaddr & len_mask) ||
             (vaddr & wp->len_mask) == wp->vaddr) && (wp->flags & flags)) {
            wp->flags |= BP_WATCHPOINT_HIT;
            if (!env->watchpoint_hit) {
                env->watchpoint_hit = wp;
                tb = tb_find_pc(env->mem_io_pc);
                if (!tb) {
                    cpu_abort(env, "check_watchpoint: could not find TB for "
                              "pc=%p", (void *)env->mem_io_pc);
                }
                cpu_restore_state(tb, env, env->mem_io_pc, NULL);
                tb_phys_invalidate(tb, -1);
                if (wp->flags & BP_STOP_BEFORE_ACCESS) {
                    env->exception_index = EXCP_DEBUG;
                } else {
                    cpu_get_tb_cpu_state(env, &pc, &cs_base, &cpu_flags);
                    tb_gen_code(env, pc, cs_base, cpu_flags, 1);
                }
                cpu_resume_from_signal(env, NULL);
            }
        } else {
            wp->flags &= ~BP_WATCHPOINT_HIT;
        }
    }
}

/* Watchpoint access routines.  Watchpoints are inserted using TLB tricks,
   so these check for a hit then pass through to the normal out-of-line
   phys routines.  */
static uint32_t watch_mem_readb(void *opaque, target_phys_addr_t addr)
{
    check_watchpoint(addr & ~TARGET_PAGE_MASK, ~0x0, BP_MEM_READ);
    return ldub_phys(addr);
}

static uint32_t watch_mem_readw(void *opaque, target_phys_addr_t addr)
{
    check_watchpoint(addr & ~TARGET_PAGE_MASK, ~0x1, BP_MEM_READ);
    return lduw_phys(addr);
}

static uint32_t watch_mem_readl(void *opaque, target_phys_addr_t addr)
{
    check_watchpoint(addr & ~TARGET_PAGE_MASK, ~0x3, BP_MEM_READ);
    return ldl_phys(addr);
}

static void watch_mem_writeb(void *opaque, target_phys_addr_t addr,
                             uint32_t val)
{
    check_watchpoint(addr & ~TARGET_PAGE_MASK, ~0x0, BP_MEM_WRITE);
    stb_phys(addr, val);
}

static void watch_mem_writew(void *opaque, target_phys_addr_t addr,
                             uint32_t val)
{
    check_watchpoint(addr & ~TARGET_PAGE_MASK, ~0x1, BP_MEM_WRITE);
    stw_phys(addr, val);
}

static void watch_mem_writel(void *opaque, target_phys_addr_t addr,
                             uint32_t val)
{
    check_watchpoint(addr & ~TARGET_PAGE_MASK, ~0x3, BP_MEM_WRITE);
    stl_phys(addr, val);
}

static CPUReadMemoryFunc * const watch_mem_read[3] = {
    watch_mem_readb,
    watch_mem_readw,
    watch_mem_readl,
};

static CPUWriteMemoryFunc * const watch_mem_write[3] = {
    watch_mem_writeb,
    watch_mem_writew,
    watch_mem_writel,
};

static inline uint32_t subpage_readlen (subpage_t *mmio,
                                        target_phys_addr_t addr,
                                        unsigned int len)
{
    unsigned int idx = SUBPAGE_IDX(addr);
#if defined(DEBUG_SUBPAGE)
    printf("%s: subpage %p len %d addr " TARGET_FMT_plx " idx %d\n", __func__,
           mmio, len, addr, idx);
#endif

    addr += mmio->region_offset[idx];
    idx = mmio->sub_io_index[idx];
    return io_mem_read[idx][len](io_mem_opaque[idx], addr);
}

static inline void subpage_writelen (subpage_t *mmio, target_phys_addr_t addr,
                                     uint32_t value, unsigned int len)
{
    unsigned int idx = SUBPAGE_IDX(addr);
#if defined(DEBUG_SUBPAGE)
    printf("%s: subpage %p len %d addr " TARGET_FMT_plx " idx %d value %08x\n",
           __func__, mmio, len, addr, idx, value);
#endif

    addr += mmio->region_offset[idx];
    idx = mmio->sub_io_index[idx];
    io_mem_write[idx][len](io_mem_opaque[idx], addr, value);
}

static uint32_t subpage_readb (void *opaque, target_phys_addr_t addr)
{
    return subpage_readlen(opaque, addr, 0);
}

static void subpage_writeb (void *opaque, target_phys_addr_t addr,
                            uint32_t value)
{
    subpage_writelen(opaque, addr, value, 0);
}

static uint32_t subpage_readw (void *opaque, target_phys_addr_t addr)
{
    return subpage_readlen(opaque, addr, 1);
}

static void subpage_writew (void *opaque, target_phys_addr_t addr,
                            uint32_t value)
{
    subpage_writelen(opaque, addr, value, 1);
}

static uint32_t subpage_readl (void *opaque, target_phys_addr_t addr)
{
    return subpage_readlen(opaque, addr, 2);
}

static void subpage_writel (void *opaque, target_phys_addr_t addr,
                            uint32_t value)
{
    subpage_writelen(opaque, addr, value, 2);
}

static CPUReadMemoryFunc * const subpage_read[] = {
    &subpage_readb,
    &subpage_readw,
    &subpage_readl,
};

static CPUWriteMemoryFunc * const subpage_write[] = {
    &subpage_writeb,
    &subpage_writew,
    &subpage_writel,
};

static int subpage_register (subpage_t *mmio, uint32_t start, uint32_t end,
                             ram_addr_t memory, ram_addr_t region_offset)
{
    int idx, eidx;

    if (start >= TARGET_PAGE_SIZE || end >= TARGET_PAGE_SIZE)
        return -1;
    idx = SUBPAGE_IDX(start);
    eidx = SUBPAGE_IDX(end);
#if defined(DEBUG_SUBPAGE)
    printf("%s: %p start %08x end %08x idx %08x eidx %08x mem %ld\n", __func__,
           mmio, start, end, idx, eidx, memory);
#endif
    memory = (memory >> IO_MEM_SHIFT) & (IO_MEM_NB_ENTRIES - 1);
    for (; idx <= eidx; idx++) {
        mmio->sub_io_index[idx] = memory;
        mmio->region_offset[idx] = region_offset;
    }

    return 0;
}

static subpage_t *subpage_init (target_phys_addr_t base, ram_addr_t *phys,
                                ram_addr_t orig_memory,
                                ram_addr_t region_offset)
{
    subpage_t *mmio;
    int subpage_memory;

    mmio = qemu_mallocz(sizeof(subpage_t));

    mmio->base = base;
    subpage_memory = cpu_register_io_memory(subpage_read, subpage_write, mmio);
#if defined(DEBUG_SUBPAGE)
    printf("%s: %p base " TARGET_FMT_plx " len %08x %d\n", __func__,
           mmio, base, TARGET_PAGE_SIZE, subpage_memory);
#endif
    *phys = subpage_memory | IO_MEM_SUBPAGE;
    subpage_register(mmio, 0, TARGET_PAGE_SIZE-1, orig_memory, region_offset);

    return mmio;
}

static int get_free_io_mem_idx(void)
{
    int i;

    for (i = 0; i<IO_MEM_NB_ENTRIES; i++)
        if (!io_mem_used[i]) {
            io_mem_used[i] = 1;
            return i;
        }
    fprintf(stderr, "RAN out out io_mem_idx, max %d !\n", IO_MEM_NB_ENTRIES);
    return -1;
}

/* mem_read and mem_write are arrays of functions containing the
   function to access byte (index 0), word (index 1) and dword (index
   2). Functions can be omitted with a NULL function pointer.
   If io_index is non zero, the corresponding io zone is
   modified. If it is zero, a new io zone is allocated. The return
   value can be used with cpu_register_physical_memory(). (-1) is
   returned if error. */
static int cpu_register_io_memory_fixed(int io_index,
                                        CPUReadMemoryFunc * const *mem_read,
                                        CPUWriteMemoryFunc * const *mem_write,
                                        void *opaque)
{
    int i;

    if (io_index <= 0) {
        io_index = get_free_io_mem_idx();
        if (io_index == -1)
            return io_index;
    } else {
        io_index >>= IO_MEM_SHIFT;
        if (io_index >= IO_MEM_NB_ENTRIES)
            return -1;
    }

    for (i = 0; i < 3; ++i) {
        io_mem_read[io_index][i]
            = (mem_read[i] ? mem_read[i] : unassigned_mem_read[i]);
    }
    for (i = 0; i < 3; ++i) {
        io_mem_write[io_index][i]
            = (mem_write[i] ? mem_write[i] : unassigned_mem_write[i]);
    }
    io_mem_opaque[io_index] = opaque;

    return (io_index << IO_MEM_SHIFT);
}

int cpu_register_io_memory(CPUReadMemoryFunc * const *mem_read,
                           CPUWriteMemoryFunc * const *mem_write,
                           void *opaque)
{
    return cpu_register_io_memory_fixed(0, mem_read, mem_write, opaque);
}

void cpu_unregister_io_memory(int io_table_address)
{
    int i;
    int io_index = io_table_address >> IO_MEM_SHIFT;

    for (i=0;i < 3; i++) {
        io_mem_read[io_index][i] = unassigned_mem_read[i];
        io_mem_write[io_index][i] = unassigned_mem_write[i];
    }
    io_mem_opaque[io_index] = NULL;
    io_mem_used[io_index] = 0;
}

static void io_mem_init(void)
{
    int i;

    cpu_register_io_memory_fixed(IO_MEM_ROM, error_mem_read, unassigned_mem_write, NULL);
    cpu_register_io_memory_fixed(IO_MEM_UNASSIGNED, unassigned_mem_read, unassigned_mem_write, NULL);
    cpu_register_io_memory_fixed(IO_MEM_NOTDIRTY, error_mem_read, notdirty_mem_write, NULL);
    for (i=0; i<5; i++)
        io_mem_used[i] = 1;

    io_mem_watch = cpu_register_io_memory(watch_mem_read,
                                          watch_mem_write, NULL);
}

#endif /* !defined(CONFIG_USER_ONLY) */

/* physical memory access (slow version, mainly for debug) */
#if defined(CONFIG_USER_ONLY)
int cpu_memory_rw_debug(CPUState *env, target_ulong addr,
                        uint8_t *buf, int len, int is_write)
{
    int l, flags;
    target_ulong page;
    void * p;

    while (len > 0) {
        page = addr & TARGET_PAGE_MASK;
        l = (page + TARGET_PAGE_SIZE) - addr;
        if (l > len)
            l = len;
        flags = page_get_flags(page);
        if (!(flags & PAGE_VALID))
            return -1;
        if (is_write) {
            if (!(flags & PAGE_WRITE))
                return -1;
            /* XXX: this code should not depend on lock_user */
            if (!(p = lock_user(VERIFY_WRITE, addr, l, 0)))
                return -1;
            memcpy(p, buf, l);
            unlock_user(p, addr, l);
        } else {
            if (!(flags & PAGE_READ))
                return -1;
            /* XXX: this code should not depend on lock_user */
            if (!(p = lock_user(VERIFY_READ, addr, l, 1)))
                return -1;
            memcpy(buf, p, l);
            unlock_user(p, addr, 0);
        }
        len -= l;
        buf += l;
        addr += l;
    }
    return 0;
}

#else
void cpu_physical_memory_rw(target_phys_addr_t addr, uint8_t *buf,
                            int len, int is_write)
{
    int l, io_index;
    uint8_t *ptr;
    uint32_t val;
    target_phys_addr_t page;
    ram_addr_t pd;
    PhysPageDesc *p;

    while (len > 0) {
        page = addr & TARGET_PAGE_MASK;
        l = (page + TARGET_PAGE_SIZE) - addr;
        if (l > len)
            l = len;
        p = phys_page_find(page >> TARGET_PAGE_BITS);
        if (!p) {
            pd = IO_MEM_UNASSIGNED;
        } else {
            pd = p->phys_offset;
        }

        if (is_write) {
            if ((pd & ~TARGET_PAGE_MASK) != IO_MEM_RAM) {
                target_phys_addr_t addr1 = addr;
                io_index = (pd >> IO_MEM_SHIFT) & (IO_MEM_NB_ENTRIES - 1);
                if (p)
                    addr1 = (addr & ~TARGET_PAGE_MASK) + p->region_offset;
                /* XXX: could force cpu_single_env to NULL to avoid
                   potential bugs */
                if (l >= 4 && ((addr1 & 3) == 0)) {
                    /* 32 bit write access */
#if !defined(VBOX) || !defined(REM_PHYS_ADDR_IN_TLB)
                    val = ldl_p(buf);
#else
                    val = *(const uint32_t *)buf;
#endif
                    io_mem_write[io_index][2](io_mem_opaque[io_index], addr1, val);
                    l = 4;
                } else if (l >= 2 && ((addr1 & 1) == 0)) {
                    /* 16 bit write access */
#if !defined(VBOX) || !defined(REM_PHYS_ADDR_IN_TLB)
                    val = lduw_p(buf);
#else
                    val = *(const uint16_t *)buf;
#endif
                    io_mem_write[io_index][1](io_mem_opaque[io_index], addr1, val);
                    l = 2;
                } else {
                    /* 8 bit write access */
#if !defined(VBOX) || !defined(REM_PHYS_ADDR_IN_TLB)
                    val = ldub_p(buf);
#else
                    val = *(const uint8_t *)buf;
#endif
                    io_mem_write[io_index][0](io_mem_opaque[io_index], addr1, val);
                    l = 1;
                }
            } else {
                ram_addr_t addr1;
                addr1 = (pd & TARGET_PAGE_MASK) + (addr & ~TARGET_PAGE_MASK);
                /* RAM case */
#ifdef VBOX
                remR3PhysWrite(addr1, buf, l); NOREF(ptr);
#else
                ptr = qemu_get_ram_ptr(addr1);
                memcpy(ptr, buf, l);
#endif
                if (!cpu_physical_memory_is_dirty(addr1)) {
                    /* invalidate code */
                    tb_invalidate_phys_page_range(addr1, addr1 + l, 0);
                    /* set dirty bit */
                    cpu_physical_memory_set_dirty_flags(
                        addr1, (0xff & ~CODE_DIRTY_FLAG));
                }
            }
        } else {
            if ((pd & ~TARGET_PAGE_MASK) > IO_MEM_ROM &&
                !(pd & IO_MEM_ROMD)) {
                target_phys_addr_t addr1 = addr;
                /* I/O case */
                io_index = (pd >> IO_MEM_SHIFT) & (IO_MEM_NB_ENTRIES - 1);
                if (p)
                    addr1 = (addr & ~TARGET_PAGE_MASK) + p->region_offset;
                if (l >= 4 && ((addr1 & 3) == 0)) {
                    /* 32 bit read access */
                    val = io_mem_read[io_index][2](io_mem_opaque[io_index], addr1);
#if !defined(VBOX) || !defined(REM_PHYS_ADDR_IN_TLB)
                    stl_p(buf, val);
#else
                    *(uint32_t *)buf = val;
#endif
                    l = 4;
                } else if (l >= 2 && ((addr1 & 1) == 0)) {
                    /* 16 bit read access */
                    val = io_mem_read[io_index][1](io_mem_opaque[io_index], addr1);
#if !defined(VBOX) || !defined(REM_PHYS_ADDR_IN_TLB)
                    stw_p(buf, val);
#else
                    *(uint16_t *)buf = val;
#endif
                    l = 2;
                } else {
                    /* 8 bit read access */
                    val = io_mem_read[io_index][0](io_mem_opaque[io_index], addr1);
#if !defined(VBOX) || !defined(REM_PHYS_ADDR_IN_TLB)
                    stb_p(buf, val);
#else
                    *(uint8_t *)buf = val;
#endif
                    l = 1;
                }
            } else {
                /* RAM case */
#ifdef VBOX
                remR3PhysRead((pd & TARGET_PAGE_MASK) + (addr & ~TARGET_PAGE_MASK), buf, l); NOREF(ptr);
#else
                ptr = qemu_get_ram_ptr(pd & TARGET_PAGE_MASK) +
                    (addr & ~TARGET_PAGE_MASK);
                memcpy(buf, ptr, l);
#endif
            }
        }
        len -= l;
        buf += l;
        addr += l;
    }
}

#ifndef VBOX

/* used for ROM loading : can write in RAM and ROM */
void cpu_physical_memory_write_rom(target_phys_addr_t addr,
                                   const uint8_t *buf, int len)
{
    int l;
    uint8_t *ptr;
    target_phys_addr_t page;
    ram_addr_t pd;
    PhysPageDesc *p;

    while (len > 0) {
        page = addr & TARGET_PAGE_MASK;
        l = (page + TARGET_PAGE_SIZE) - addr;
        if (l > len)
            l = len;
        p = phys_page_find(page >> TARGET_PAGE_BITS);
        if (!p) {
            pd = IO_MEM_UNASSIGNED;
        } else {
            pd = p->phys_offset;
        }

        if ((pd & ~TARGET_PAGE_MASK) != IO_MEM_RAM &&
            (pd & ~TARGET_PAGE_MASK) != IO_MEM_ROM &&
            !(pd & IO_MEM_ROMD)) {
            /* do nothing */
        } else {
            ram_addr_t addr1;
            addr1 = (pd & TARGET_PAGE_MASK) + (addr & ~TARGET_PAGE_MASK);
            /* ROM/RAM case */
            ptr = qemu_get_ram_ptr(addr1);
            memcpy(ptr, buf, l);
        }
        len -= l;
        buf += l;
        addr += l;
    }
}

typedef struct {
    void *buffer;
    target_phys_addr_t addr;
    target_phys_addr_t len;
} BounceBuffer;

static BounceBuffer bounce;

typedef struct MapClient {
    void *opaque;
    void (*callback)(void *opaque);
    QLIST_ENTRY(MapClient) link;
} MapClient;

static QLIST_HEAD(map_client_list, MapClient) map_client_list
    = QLIST_HEAD_INITIALIZER(map_client_list);

void *cpu_register_map_client(void *opaque, void (*callback)(void *opaque))
{
    MapClient *client = qemu_malloc(sizeof(*client));

    client->opaque = opaque;
    client->callback = callback;
    QLIST_INSERT_HEAD(&map_client_list, client, link);
    return client;
}

void cpu_unregister_map_client(void *_client)
{
    MapClient *client = (MapClient *)_client;

    QLIST_REMOVE(client, link);
    qemu_free(client);
}

static void cpu_notify_map_clients(void)
{
    MapClient *client;

    while (!QLIST_EMPTY(&map_client_list)) {
        client = QLIST_FIRST(&map_client_list);
        client->callback(client->opaque);
        cpu_unregister_map_client(client);
    }
}

/* Map a physical memory region into a host virtual address.
 * May map a subset of the requested range, given by and returned in *plen.
 * May return NULL if resources needed to perform the mapping are exhausted.
 * Use only for reads OR writes - not for read-modify-write operations.
 * Use cpu_register_map_client() to know when retrying the map operation is
 * likely to succeed.
 */
void *cpu_physical_memory_map(target_phys_addr_t addr,
                              target_phys_addr_t *plen,
                              int is_write)
{
    target_phys_addr_t len = *plen;
    target_phys_addr_t done = 0;
    int l;
    uint8_t *ret = NULL;
    uint8_t *ptr;
    target_phys_addr_t page;
    ram_addr_t pd;
    PhysPageDesc *p;
    ram_addr_t addr1;

    while (len > 0) {
        page = addr & TARGET_PAGE_MASK;
        l = (page + TARGET_PAGE_SIZE) - addr;
        if (l > len)
            l = len;
        p = phys_page_find(page >> TARGET_PAGE_BITS);
        if (!p) {
            pd = IO_MEM_UNASSIGNED;
        } else {
            pd = p->phys_offset;
        }

        if ((pd & ~TARGET_PAGE_MASK) != IO_MEM_RAM) {
            if (done || bounce.buffer) {
                break;
            }
            bounce.buffer = qemu_memalign(TARGET_PAGE_SIZE, TARGET_PAGE_SIZE);
            bounce.addr = addr;
            bounce.len = l;
            if (!is_write) {
                cpu_physical_memory_rw(addr, bounce.buffer, l, 0);
            }
            ptr = bounce.buffer;
        } else {
            addr1 = (pd & TARGET_PAGE_MASK) + (addr & ~TARGET_PAGE_MASK);
            ptr = qemu_get_ram_ptr(addr1);
        }
        if (!done) {
            ret = ptr;
        } else if (ret + done != ptr) {
            break;
        }

        len -= l;
        addr += l;
        done += l;
    }
    *plen = done;
    return ret;
}

/* Unmaps a memory region previously mapped by cpu_physical_memory_map().
 * Will also mark the memory as dirty if is_write == 1.  access_len gives
 * the amount of memory that was actually read or written by the caller.
 */
void cpu_physical_memory_unmap(void *buffer, target_phys_addr_t len,
                               int is_write, target_phys_addr_t access_len)
{
    if (buffer != bounce.buffer) {
        if (is_write) {
            ram_addr_t addr1 = qemu_ram_addr_from_host(buffer);
            while (access_len) {
                unsigned l;
                l = TARGET_PAGE_SIZE;
                if (l > access_len)
                    l = access_len;
                if (!cpu_physical_memory_is_dirty(addr1)) {
                    /* invalidate code */
                    tb_invalidate_phys_page_range(addr1, addr1 + l, 0);
                    /* set dirty bit */
                    cpu_physical_memory_set_dirty_flags(
                        addr1, (0xff & ~CODE_DIRTY_FLAG));
                }
                addr1 += l;
                access_len -= l;
            }
        }
        return;
    }
    if (is_write) {
        cpu_physical_memory_write(bounce.addr, bounce.buffer, access_len);
    }
    qemu_vfree(bounce.buffer);
    bounce.buffer = NULL;
    cpu_notify_map_clients();
}

#endif /* !VBOX */

/* warning: addr must be aligned */
uint32_t ldl_phys(target_phys_addr_t addr)
{
    int io_index;
    uint8_t *ptr;
    uint32_t val;
    ram_addr_t pd;
    PhysPageDesc *p;

    p = phys_page_find(addr >> TARGET_PAGE_BITS);
    if (!p) {
        pd = IO_MEM_UNASSIGNED;
    } else {
        pd = p->phys_offset;
    }

    if ((pd & ~TARGET_PAGE_MASK) > IO_MEM_ROM &&
        !(pd & IO_MEM_ROMD)) {
        /* I/O case */
        io_index = (pd >> IO_MEM_SHIFT) & (IO_MEM_NB_ENTRIES - 1);
        if (p)
            addr = (addr & ~TARGET_PAGE_MASK) + p->region_offset;
        val = io_mem_read[io_index][2](io_mem_opaque[io_index], addr);
    } else {
        /* RAM case */
#ifndef VBOX
        ptr = qemu_get_ram_ptr(pd & TARGET_PAGE_MASK) +
            (addr & ~TARGET_PAGE_MASK);
        val = ldl_p(ptr);
#else
        val = remR3PhysReadU32((pd & TARGET_PAGE_MASK) + (addr & ~TARGET_PAGE_MASK)); NOREF(ptr);
#endif
    }
    return val;
}

/* warning: addr must be aligned */
uint64_t ldq_phys(target_phys_addr_t addr)
{
    int io_index;
    uint8_t *ptr;
    uint64_t val;
    ram_addr_t pd;
    PhysPageDesc *p;

    p = phys_page_find(addr >> TARGET_PAGE_BITS);
    if (!p) {
        pd = IO_MEM_UNASSIGNED;
    } else {
        pd = p->phys_offset;
    }

    if ((pd & ~TARGET_PAGE_MASK) > IO_MEM_ROM &&
        !(pd & IO_MEM_ROMD)) {
        /* I/O case */
        io_index = (pd >> IO_MEM_SHIFT) & (IO_MEM_NB_ENTRIES - 1);
        if (p)
            addr = (addr & ~TARGET_PAGE_MASK) + p->region_offset;
#ifdef TARGET_WORDS_BIGENDIAN
        val = (uint64_t)io_mem_read[io_index][2](io_mem_opaque[io_index], addr) << 32;
        val |= io_mem_read[io_index][2](io_mem_opaque[io_index], addr + 4);
#else
        val = io_mem_read[io_index][2](io_mem_opaque[io_index], addr);
        val |= (uint64_t)io_mem_read[io_index][2](io_mem_opaque[io_index], addr + 4) << 32;
#endif
    } else {
        /* RAM case */
#ifndef VBOX
        ptr = qemu_get_ram_ptr(pd & TARGET_PAGE_MASK) +
            (addr & ~TARGET_PAGE_MASK);
        val = ldq_p(ptr);
#else
        val = remR3PhysReadU64((pd & TARGET_PAGE_MASK) + (addr & ~TARGET_PAGE_MASK)); NOREF(ptr);
#endif
    }
    return val;
}

/* XXX: optimize */
uint32_t ldub_phys(target_phys_addr_t addr)
{
    uint8_t val;
    cpu_physical_memory_read(addr, &val, 1);
    return val;
}

/* warning: addr must be aligned */
uint32_t lduw_phys(target_phys_addr_t addr)
{
    int io_index;
#ifndef VBOX
    uint8_t *ptr;
#endif
    uint64_t val;
    ram_addr_t pd;
    PhysPageDesc *p;

    p = phys_page_find(addr >> TARGET_PAGE_BITS);
    if (!p) {
        pd = IO_MEM_UNASSIGNED;
    } else {
        pd = p->phys_offset;
    }

    if ((pd & ~TARGET_PAGE_MASK) > IO_MEM_ROM &&
        !(pd & IO_MEM_ROMD)) {
        /* I/O case */
        io_index = (pd >> IO_MEM_SHIFT) & (IO_MEM_NB_ENTRIES - 1);
        if (p)
            addr = (addr & ~TARGET_PAGE_MASK) + p->region_offset;
        val = io_mem_read[io_index][1](io_mem_opaque[io_index], addr);
    } else {
        /* RAM case */
#ifndef VBOX
        ptr = qemu_get_ram_ptr(pd & TARGET_PAGE_MASK) +
            (addr & ~TARGET_PAGE_MASK);
        val = lduw_p(ptr);
#else
        val = remR3PhysReadU16((pd & TARGET_PAGE_MASK) | (addr & ~TARGET_PAGE_MASK));
#endif
    }
    return val;
}

/* warning: addr must be aligned. The ram page is not masked as dirty
   and the code inside is not invalidated. It is useful if the dirty
   bits are used to track modified PTEs */
void stl_phys_notdirty(target_phys_addr_t addr, uint32_t val)
{
    int io_index;
    uint8_t *ptr;
    ram_addr_t pd;
    PhysPageDesc *p;

    p = phys_page_find(addr >> TARGET_PAGE_BITS);
    if (!p) {
        pd = IO_MEM_UNASSIGNED;
    } else {
        pd = p->phys_offset;
    }

    if ((pd & ~TARGET_PAGE_MASK) != IO_MEM_RAM) {
        io_index = (pd >> IO_MEM_SHIFT) & (IO_MEM_NB_ENTRIES - 1);
        if (p)
            addr = (addr & ~TARGET_PAGE_MASK) + p->region_offset;
        io_mem_write[io_index][2](io_mem_opaque[io_index], addr, val);
    } else {
#ifndef VBOX
        ram_addr_t addr1 = (pd & TARGET_PAGE_MASK) + (addr & ~TARGET_PAGE_MASK);
        ptr = qemu_get_ram_ptr(addr1);
        stl_p(ptr, val);
#else
        remR3PhysWriteU32((pd & TARGET_PAGE_MASK) + (addr & ~TARGET_PAGE_MASK), val); NOREF(ptr);
#endif

#ifndef VBOX
        if (unlikely(in_migration)) {
            if (!cpu_physical_memory_is_dirty(addr1)) {
                /* invalidate code */
                tb_invalidate_phys_page_range(addr1, addr1 + 4, 0);
                /* set dirty bit */
                cpu_physical_memory_set_dirty_flags(
                    addr1, (0xff & ~CODE_DIRTY_FLAG));
            }
        }
#endif /* !VBOX */
    }
}

void stq_phys_notdirty(target_phys_addr_t addr, uint64_t val)
{
    int io_index;
    uint8_t *ptr;
    ram_addr_t pd;
    PhysPageDesc *p;

    p = phys_page_find(addr >> TARGET_PAGE_BITS);
    if (!p) {
        pd = IO_MEM_UNASSIGNED;
    } else {
        pd = p->phys_offset;
    }

    if ((pd & ~TARGET_PAGE_MASK) != IO_MEM_RAM) {
        io_index = (pd >> IO_MEM_SHIFT) & (IO_MEM_NB_ENTRIES - 1);
        if (p)
            addr = (addr & ~TARGET_PAGE_MASK) + p->region_offset;
#ifdef TARGET_WORDS_BIGENDIAN
        io_mem_write[io_index][2](io_mem_opaque[io_index], addr, val >> 32);
        io_mem_write[io_index][2](io_mem_opaque[io_index], addr + 4, val);
#else
        io_mem_write[io_index][2](io_mem_opaque[io_index], addr, val);
        io_mem_write[io_index][2](io_mem_opaque[io_index], addr + 4, val >> 32);
#endif
    } else {
#ifndef VBOX
        ptr = qemu_get_ram_ptr(pd & TARGET_PAGE_MASK) +
            (addr & ~TARGET_PAGE_MASK);
        stq_p(ptr, val);
#else
        remR3PhysWriteU64((pd & TARGET_PAGE_MASK) + (addr & ~TARGET_PAGE_MASK), val); NOREF(ptr);
#endif
    }
}

/* warning: addr must be aligned */
void stl_phys(target_phys_addr_t addr, uint32_t val)
{
    int io_index;
    uint8_t *ptr;
    ram_addr_t pd;
    PhysPageDesc *p;

    p = phys_page_find(addr >> TARGET_PAGE_BITS);
    if (!p) {
        pd = IO_MEM_UNASSIGNED;
    } else {
        pd = p->phys_offset;
    }

    if ((pd & ~TARGET_PAGE_MASK) != IO_MEM_RAM) {
        io_index = (pd >> IO_MEM_SHIFT) & (IO_MEM_NB_ENTRIES - 1);
        if (p)
            addr = (addr & ~TARGET_PAGE_MASK) + p->region_offset;
        io_mem_write[io_index][2](io_mem_opaque[io_index], addr, val);
    } else {
        ram_addr_t addr1;
        addr1 = (pd & TARGET_PAGE_MASK) + (addr & ~TARGET_PAGE_MASK);
        /* RAM case */
#ifndef VBOX
        ptr = qemu_get_ram_ptr(addr1);
        stl_p(ptr, val);
#else
        remR3PhysWriteU32((pd & TARGET_PAGE_MASK) + (addr & ~TARGET_PAGE_MASK), val); NOREF(ptr);
#endif
        if (!cpu_physical_memory_is_dirty(addr1)) {
            /* invalidate code */
            tb_invalidate_phys_page_range(addr1, addr1 + 4, 0);
            /* set dirty bit */
            cpu_physical_memory_set_dirty_flags(addr1,
                (0xff & ~CODE_DIRTY_FLAG));
        }
    }
}

/* XXX: optimize */
void stb_phys(target_phys_addr_t addr, uint32_t val)
{
    uint8_t v = val;
    cpu_physical_memory_write(addr, &v, 1);
}

/* warning: addr must be aligned */
void stw_phys(target_phys_addr_t addr, uint32_t val)
{
    int io_index;
    uint8_t *ptr;
    ram_addr_t pd;
    PhysPageDesc *p;

    p = phys_page_find(addr >> TARGET_PAGE_BITS);
    if (!p) {
        pd = IO_MEM_UNASSIGNED;
    } else {
        pd = p->phys_offset;
    }

    if ((pd & ~TARGET_PAGE_MASK) != IO_MEM_RAM) {
        io_index = (pd >> IO_MEM_SHIFT) & (IO_MEM_NB_ENTRIES - 1);
        if (p)
            addr = (addr & ~TARGET_PAGE_MASK) + p->region_offset;
        io_mem_write[io_index][1](io_mem_opaque[io_index], addr, val);
    } else {
        ram_addr_t addr1;
        addr1 = (pd & TARGET_PAGE_MASK) + (addr & ~TARGET_PAGE_MASK);
        /* RAM case */
#ifndef VBOX
        ptr = qemu_get_ram_ptr(addr1);
        stw_p(ptr, val);
#else
        remR3PhysWriteU16(addr1, val); NOREF(ptr);
#endif
        if (!cpu_physical_memory_is_dirty(addr1)) {
            /* invalidate code */
            tb_invalidate_phys_page_range(addr1, addr1 + 2, 0);
            /* set dirty bit */
            cpu_physical_memory_set_dirty_flags(addr1,
                (0xff & ~CODE_DIRTY_FLAG));
        }
    }
}

/* XXX: optimize */
void stq_phys(target_phys_addr_t addr, uint64_t val)
{
    val = tswap64(val);
    cpu_physical_memory_write(addr, (const uint8_t *)&val, 8);
}

#ifndef VBOX
/* virtual memory access for debug (includes writing to ROM) */
int cpu_memory_rw_debug(CPUState *env, target_ulong addr,
                        uint8_t *buf, int len, int is_write)
{
    int l;
    target_phys_addr_t phys_addr;
    target_ulong page;

    while (len > 0) {
        page = addr & TARGET_PAGE_MASK;
        phys_addr = cpu_get_phys_page_debug(env, page);
        /* if no physical page mapped, return an error */
        if (phys_addr == -1)
            return -1;
        l = (page + TARGET_PAGE_SIZE) - addr;
        if (l > len)
            l = len;
        phys_addr += (addr & ~TARGET_PAGE_MASK);
        if (is_write)
            cpu_physical_memory_write_rom(phys_addr, buf, l);
        else
            cpu_physical_memory_rw(phys_addr, buf, l, is_write);
        len -= l;
        buf += l;
        addr += l;
    }
    return 0;
}
#endif /* !VBOX */
#endif

/* in deterministic execution mode, instructions doing device I/Os
   must be at the end of the TB */
void cpu_io_recompile(CPUState *env, void *retaddr)
{
    TranslationBlock *tb;
    uint32_t n, cflags;
    target_ulong pc, cs_base;
    uint64_t flags;

    tb = tb_find_pc((uintptr_t)retaddr);
    if (!tb) {
        cpu_abort(env, "cpu_io_recompile: could not find TB for pc=%p",
                  retaddr);
    }
    n = env->icount_decr.u16.low + tb->icount;
    cpu_restore_state(tb, env, (uintptr_t)retaddr, NULL);
    /* Calculate how many instructions had been executed before the fault
       occurred.  */
    n = n - env->icount_decr.u16.low;
    /* Generate a new TB ending on the I/O insn.  */
    n++;
    /* On MIPS and SH, delay slot instructions can only be restarted if
       they were already the first instruction in the TB.  If this is not
       the first instruction in a TB then re-execute the preceding
       branch.  */
#if defined(TARGET_MIPS)
    if ((env->hflags & MIPS_HFLAG_BMASK) != 0 && n > 1) {
        env->active_tc.PC -= 4;
        env->icount_decr.u16.low++;
        env->hflags &= ~MIPS_HFLAG_BMASK;
    }
#elif defined(TARGET_SH4)
    if ((env->flags & ((DELAY_SLOT | DELAY_SLOT_CONDITIONAL))) != 0
            && n > 1) {
        env->pc -= 2;
        env->icount_decr.u16.low++;
        env->flags &= ~(DELAY_SLOT | DELAY_SLOT_CONDITIONAL);
    }
#endif
    /* This should never happen.  */
    if (n > CF_COUNT_MASK)
        cpu_abort(env, "TB too big during recompile");

    cflags = n | CF_LAST_IO;
    pc = tb->pc;
    cs_base = tb->cs_base;
    flags = tb->flags;
    tb_phys_invalidate(tb, -1);
    /* FIXME: In theory this could raise an exception.  In practice
       we have already translated the block once so it's probably ok.  */
    tb_gen_code(env, pc, cs_base, flags, cflags);
    /** @todo If env->pc != tb->pc (i.e. the faulting instruction was not
       the first in the TB) then we end up generating a whole new TB and
       repeating the fault, which is horribly inefficient.
       Better would be to execute just this insn uncached, or generate a
       second new TB.  */
    cpu_resume_from_signal(env, NULL);
}

#if !defined(CONFIG_USER_ONLY)

#ifndef VBOX
void dump_exec_info(FILE *f,
                    int (*cpu_fprintf)(FILE *f, const char *fmt, ...))
{
    int i, target_code_size, max_target_code_size;
    int direct_jmp_count, direct_jmp2_count, cross_page;
    TranslationBlock *tb;

    target_code_size = 0;
    max_target_code_size = 0;
    cross_page = 0;
    direct_jmp_count = 0;
    direct_jmp2_count = 0;
    for(i = 0; i < nb_tbs; i++) {
        tb = &tbs[i];
        target_code_size += tb->size;
        if (tb->size > max_target_code_size)
            max_target_code_size = tb->size;
        if (tb->page_addr[1] != -1)
            cross_page++;
        if (tb->tb_next_offset[0] != 0xffff) {
            direct_jmp_count++;
            if (tb->tb_next_offset[1] != 0xffff) {
                direct_jmp2_count++;
            }
        }
    }
    /* XXX: avoid using doubles ? */
    cpu_fprintf(f, "Translation buffer state:\n");
    cpu_fprintf(f, "gen code size       %ld/%ld\n",
                code_gen_ptr - code_gen_buffer, code_gen_buffer_max_size);
    cpu_fprintf(f, "TB count            %d/%d\n",
                nb_tbs, code_gen_max_blocks);
    cpu_fprintf(f, "TB avg target size  %d max=%d bytes\n",
                nb_tbs ? target_code_size / nb_tbs : 0,
                max_target_code_size);
    cpu_fprintf(f, "TB avg host size    %d bytes (expansion ratio: %0.1f)\n",
                nb_tbs ? (code_gen_ptr - code_gen_buffer) / nb_tbs : 0,
                target_code_size ? (double) (code_gen_ptr - code_gen_buffer) / target_code_size : 0);
    cpu_fprintf(f, "cross page TB count %d (%d%%)\n",
            cross_page,
            nb_tbs ? (cross_page * 100) / nb_tbs : 0);
    cpu_fprintf(f, "direct jump count   %d (%d%%) (2 jumps=%d %d%%)\n",
                direct_jmp_count,
                nb_tbs ? (direct_jmp_count * 100) / nb_tbs : 0,
                direct_jmp2_count,
                nb_tbs ? (direct_jmp2_count * 100) / nb_tbs : 0);
    cpu_fprintf(f, "\nStatistics:\n");
    cpu_fprintf(f, "TB flush count      %d\n", tb_flush_count);
    cpu_fprintf(f, "TB invalidate count %d\n", tb_phys_invalidate_count);
    cpu_fprintf(f, "TLB flush count     %d\n", tlb_flush_count);
    tcg_dump_info(f, cpu_fprintf);
}
#endif /* !VBOX */

#define MMUSUFFIX _cmmu
#define GETPC() NULL
#define env cpu_single_env
#define SOFTMMU_CODE_ACCESS

#define SHIFT 0
#include "softmmu_template.h"

#define SHIFT 1
#include "softmmu_template.h"

#define SHIFT 2
#include "softmmu_template.h"

#define SHIFT 3
#include "softmmu_template.h"

#undef env

#endif
