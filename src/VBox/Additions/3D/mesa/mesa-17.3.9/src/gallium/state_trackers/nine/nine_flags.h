

#ifndef _NINE_FLAGS_H_
#define _NINE_FLAGS_H_

#include "pipe/p_compiler.h"

/* Incoming 32 bits calls are 4-byte aligned.
 * We need to realign them to be able to use
 * SSE and to work with other libraries (llvm, etc)
 */
#define NINE_WINAPI WINAPI PIPE_ALIGN_STACK

#endif /* _NINE_FLAGS_H_ */