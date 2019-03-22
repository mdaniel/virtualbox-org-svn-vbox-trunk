/*
 * Copyright © 2016 VMware, Inc., Palo Alto, CA., USA
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
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. IN NO EVENT SHALL
 * THE COPYRIGHT HOLDERS, AUTHORS AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
 * USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

#include "util/u_math.h" /* for MAX2/MIN2 */
#include "util/u_debug.h"
#include "util/u_memory.h"
#include "util/u_string.h"
#include "pipe/p_defines.h"
#include "svga_msg.h"


#define MESSAGE_STATUS_SUCCESS  0x0001
#define MESSAGE_STATUS_DORECV   0x0002
#define MESSAGE_STATUS_CPT      0x0010
#define MESSAGE_STATUS_HB       0x0080

#define RPCI_PROTOCOL_NUM       0x49435052
#define GUESTMSG_FLAG_COOKIE    0x80000000

#define RETRIES                 3

#define VMW_HYPERVISOR_MAGIC    0x564D5868
#define VMW_HYPERVISOR_PORT     0x5658
#define VMW_HYPERVISOR_HB_PORT  0x5659

#define VMW_PORT_CMD_MSG        30
#define VMW_PORT_CMD_HB_MSG     0
#define VMW_PORT_CMD_OPEN_CHANNEL  (MSG_TYPE_OPEN << 16 | VMW_PORT_CMD_MSG)
#define VMW_PORT_CMD_CLOSE_CHANNEL (MSG_TYPE_CLOSE << 16 | VMW_PORT_CMD_MSG)
#define VMW_PORT_CMD_SENDSIZE   (MSG_TYPE_SENDSIZE << 16 | VMW_PORT_CMD_MSG)
#define VMW_PORT_CMD_RECVSIZE   (MSG_TYPE_RECVSIZE << 16 | VMW_PORT_CMD_MSG)
#define VMW_PORT_CMD_RECVSTATUS (MSG_TYPE_RECVSTATUS << 16 | VMW_PORT_CMD_MSG)

#define HIGH_WORD(X) ((X & 0xFFFF0000) >> 16)


#if defined(PIPE_CC_GCC) && (PIPE_CC_GCC_VERSION > 502)

/**
 * Hypervisor-specific bi-directional communication channel.  Should never
 * execute on bare metal hardware.  The caller must make sure to check for
 * supported hypervisor before using these macros.
 *
 * The last two parameters are both input and output and must be initialized.
 *
 * @cmd: [IN] Message Cmd
 * @in_bx: [IN] Message Len, through BX
 * @in_si: [IN] Input argument through SI, set to 0 if not used
 * @in_di: [IN] Input argument through DI, set ot 0 if not used
 * @port_num: [IN] port number + [channel id]
 * @magic: [IN] hypervisor magic value
 * @ax: [OUT] value of AX register
 * @bx: [OUT] e.g. status from an HB message status command
 * @cx: [OUT] e.g. status from a non-HB message status command
 * @dx: [OUT] e.g. channel id
 * @si:  [OUT]
 * @di:  [OUT]
 */
#define VMW_PORT(cmd, in_bx, in_si, in_di, \
         port_num, magic,                  \
         ax, bx, cx, dx, si, di)           \
({                                         \
   asm volatile ("inl %%dx, %%eax;" :      \
      "=a"(ax),                            \
      "=b"(bx),                            \
      "=c"(cx),                            \
      "=d"(dx),                            \
      "=S"(si),                            \
      "=D"(di) :                           \
      "a"(magic),                          \
      "b"(in_bx),                          \
      "c"(cmd),                            \
      "d"(port_num),                       \
      "S"(in_si),                          \
      "D"(in_di) :                         \
      "memory");                           \
})



/**
 * Hypervisor-specific bi-directional communication channel.  Should never
 * execute on bare metal hardware.  The caller must make sure to check for
 * supported hypervisor before using these macros.
 *
 * @cmd: [IN] Message Cmd
 * @in_cx: [IN] Message Len, through CX
 * @in_si: [IN] Input argument through SI, set to 0 if not used
 * @in_di: [IN] Input argument through DI, set to 0 if not used
 * @port_num: [IN] port number + [channel id]
 * @magic: [IN] hypervisor magic value
 * @bp:  [IN]
 * @ax: [OUT] value of AX register
 * @bx: [OUT] e.g. status from an HB message status command
 * @cx: [OUT] e.g. status from a non-HB message status command
 * @dx: [OUT] e.g. channel id
 * @si:  [OUT]
 * @di:  [OUT]
 */
#if defined(PIPE_ARCH_X86_64)

typedef uint64_t VMW_REG;

#define VMW_PORT_HB_OUT(cmd, in_cx, in_si, in_di, \
         port_num, magic, bp,                     \
         ax, bx, cx, dx, si, di)                  \
({                                                \
   asm volatile ("push %%rbp;"                    \
      "movq %12, %%rbp;"                          \
      "rep outsb;"                                \
      "pop %%rbp;" :                              \
      "=a"(ax),                                   \
      "=b"(bx),                                   \
      "=c"(cx),                                   \
      "=d"(dx),                                   \
      "=S"(si),                                   \
      "=D"(di) :                                  \
      "a"(magic),                                 \
      "b"(cmd),                                   \
      "c"(in_cx),                                 \
      "d"(port_num),                              \
      "S"(in_si),                                 \
      "D"(in_di),                                 \
      "r"(bp) :                                   \
      "memory", "cc");                            \
})

#define VMW_PORT_HB_IN(cmd, in_cx, in_si, in_di,  \
         port_num, magic, bp,                     \
         ax, bx, cx, dx, si, di)                  \
({                                                \
   asm volatile ("push %%rbp;"                    \
      "movq %12, %%rbp;"                          \
      "rep insb;"                                 \
      "pop %%rbp" :                               \
      "=a"(ax),                                   \
      "=b"(bx),                                   \
      "=c"(cx),                                   \
      "=d"(dx),                                   \
      "=S"(si),                                   \
      "=D"(di) :                                  \
      "a"(magic),                                 \
      "b"(cmd),                                   \
      "c"(in_cx),                                 \
      "d"(port_num),                              \
      "S"(in_si),                                 \
      "D"(in_di),                                 \
      "r"(bp) :                                   \
      "memory", "cc");                            \
})

#else

typedef uint32_t VMW_REG;

/* In the 32-bit version of this macro, we use "m" because there is no
 * more register left for bp
 */
#define VMW_PORT_HB_OUT(cmd, in_cx, in_si, in_di, \
         port_num, magic, bp,                     \
         ax, bx, cx, dx, si, di)                  \
({                                                \
   asm volatile ("push %%ebp;"                    \
      "mov %12, %%ebp;"                           \
      "rep outsb;"                                \
      "pop %%ebp;" :                              \
      "=a"(ax),                                   \
      "=b"(bx),                                   \
      "=c"(cx),                                   \
      "=d"(dx),                                   \
      "=S"(si),                                   \
      "=D"(di) :                                  \
      "a"(magic),                                 \
      "b"(cmd),                                   \
      "c"(in_cx),                                 \
      "d"(port_num),                              \
      "S"(in_si),                                 \
      "D"(in_di),                                 \
      "m"(bp) :                                   \
      "memory", "cc");                            \
})


#define VMW_PORT_HB_IN(cmd, in_cx, in_si, in_di,  \
         port_num, magic, bp,                     \
         ax, bx, cx, dx, si, di)                  \
({                                                \
   asm volatile ("push %%ebp;"                    \
      "mov %12, %%ebp;"                           \
      "rep insb;"                                 \
      "pop %%ebp" :                               \
      "=a"(ax),                                   \
      "=b"(bx),                                   \
      "=c"(cx),                                   \
      "=d"(dx),                                   \
      "=S"(si),                                   \
      "=D"(di) :                                  \
      "a"(magic),                                 \
      "b"(cmd),                                   \
      "c"(in_cx),                                 \
      "d"(port_num),                              \
      "S"(in_si),                                 \
      "D"(in_di),                                 \
      "m"(bp) :                                   \
      "memory", "cc");                            \
})

#endif

#else

#define MSG_NOT_IMPLEMENTED 1

/* not implemented */

typedef uint32_t VMW_REG;


#define VMW_PORT(cmd, in_bx, in_si, in_di, \
         port_num, magic,                  \
         ax, bx, cx, dx, si, di)           \
         (void) in_bx;                     \
         (void) ax; (void) bx; (void) cx;  \
         (void) dx; (void) si; (void) di;

#define VMW_PORT_HB_OUT(cmd, in_cx, in_si, in_di, \
         port_num, magic, bp,                     \
         ax, bx, cx, dx, si, di)                  \
         (void) in_cx; (void) bp;                 \
         (void) ax; (void) bx; (void) cx;         \
         (void) dx; (void) si; (void) di;
			

#define VMW_PORT_HB_IN(cmd, in_cx, in_si, in_di,  \
         port_num, magic, bp,                     \
         ax, bx, cx, dx, si, di)                  \
         (void) bp;                               \
         (void) ax; (void) bx; (void) cx;         \
         (void) dx; (void) si; (void) di;

#endif /* #if PIPE_CC_GCC */


enum rpc_msg_type {
   MSG_TYPE_OPEN,
   MSG_TYPE_SENDSIZE,
   MSG_TYPE_SENDPAYLOAD,
   MSG_TYPE_RECVSIZE,
   MSG_TYPE_RECVPAYLOAD,
   MSG_TYPE_RECVSTATUS,
   MSG_TYPE_CLOSE,
};

struct rpc_channel {
   uint16_t channel_id;
   uint32_t cookie_high;
   uint32_t cookie_low;
};



/**
 * svga_open_channel
 *
 * @channel: RPC channel
 * @protocol:
 *
 * Returns: PIPE_OK on success, PIPE_ERROR otherwise
 */
static enum pipe_error
svga_open_channel(struct rpc_channel *channel, unsigned protocol)
{
   VMW_REG ax = 0, bx = 0, cx = 0, dx = 0, si = 0, di = 0;

   VMW_PORT(VMW_PORT_CMD_OPEN_CHANNEL,
      (protocol | GUESTMSG_FLAG_COOKIE), si, di,
      VMW_HYPERVISOR_PORT,
      VMW_HYPERVISOR_MAGIC,
      ax, bx, cx, dx, si, di);

   if ((HIGH_WORD(cx) & MESSAGE_STATUS_SUCCESS) == 0)
      return PIPE_ERROR;

   channel->channel_id = HIGH_WORD(dx);
   channel->cookie_high = si;
   channel->cookie_low = di;

   return PIPE_OK;
}



/**
 * svga_close_channel
 *
 * @channel: RPC channel
 *
 * Returns: PIPE_OK on success, PIPE_ERROR otherwises
 */
static enum pipe_error
svga_close_channel(struct rpc_channel *channel)
{
   VMW_REG ax = 0, bx = 0, cx = 0, dx = 0, si, di;

   /* Set up additional parameters */
   si = channel->cookie_high;
   di = channel->cookie_low;

   VMW_PORT(VMW_PORT_CMD_CLOSE_CHANNEL,
      0, si, di,
      (VMW_HYPERVISOR_PORT | (channel->channel_id << 16)),
      VMW_HYPERVISOR_MAGIC,
      ax, bx, cx, dx, si, di);

   if ((HIGH_WORD(cx) & MESSAGE_STATUS_SUCCESS) == 0)
      return PIPE_ERROR;

   return PIPE_OK;
}



/**
 * svga_send_msg: Sends a message to the host
 *
 * @channel: RPC channel
 * @logmsg: NULL terminated string
 *
 * Returns: PIPE_OK on success
 */
static enum pipe_error
svga_send_msg(struct rpc_channel *channel, const char *msg)
{
   VMW_REG ax = 0, bx = 0, cx = 0, dx = 0, si, di, bp;
   size_t msg_len = strlen(msg);
   int retries = 0;


   while (retries < RETRIES) {
      retries++;

      /* Set up additional parameters */
      si = channel->cookie_high;
      di = channel->cookie_low;

      VMW_PORT(VMW_PORT_CMD_SENDSIZE,
         msg_len, si, di,
         VMW_HYPERVISOR_PORT | (channel->channel_id << 16),
         VMW_HYPERVISOR_MAGIC,
         ax, bx, cx, dx, si, di);

      if ((HIGH_WORD(cx) & MESSAGE_STATUS_SUCCESS) == 0 ||
          (HIGH_WORD(cx) & MESSAGE_STATUS_HB) == 0) {
         /* Expected success + high-bandwidth. Give up. */
         return PIPE_ERROR;
      }

      /* Send msg */
      si = (uintptr_t) msg;
      di = channel->cookie_low;
      bp = channel->cookie_high;

      VMW_PORT_HB_OUT(
         (MESSAGE_STATUS_SUCCESS << 16) | VMW_PORT_CMD_HB_MSG,
         msg_len, si, di,
         VMW_HYPERVISOR_HB_PORT | (channel->channel_id << 16),
         VMW_HYPERVISOR_MAGIC, bp,
         ax, bx, cx, dx, si, di);

      if ((HIGH_WORD(bx) & MESSAGE_STATUS_SUCCESS) != 0) {
         return PIPE_OK;
      } else if ((HIGH_WORD(bx) & MESSAGE_STATUS_CPT) != 0) {
         /* A checkpoint occurred. Retry. */
         continue;
      } else {
         break;
      }
   }

   return PIPE_ERROR;
}



/**
 * svga_host_log: Sends a log message to the host
 *
 * @log: NULL terminated string
 *
 * Returns: PIPE_OK on success
 */
enum pipe_error
svga_host_log(const char *log)
{
   struct rpc_channel channel;
   char *msg;
   int msg_len;
   enum pipe_error ret = PIPE_OK;

#ifdef MSG_NOT_IMPLEMENTED
   return ret;
#endif

   if (!log)
      return ret;

   msg_len = strlen(log) + strlen("log ") + 1;
   msg = CALLOC(1, msg_len);
   if (msg == NULL) {
      debug_printf("Cannot allocate memory for log message\n");
      return PIPE_ERROR_OUT_OF_MEMORY;
   }

   util_sprintf(msg, "log %s", log);

   if (svga_open_channel(&channel, RPCI_PROTOCOL_NUM) ||
       svga_send_msg(&channel, msg) ||
       svga_close_channel(&channel)) {
      debug_printf("Failed to send log\n");

      ret = PIPE_ERROR;
   }

   FREE(msg);

   return ret;
}

