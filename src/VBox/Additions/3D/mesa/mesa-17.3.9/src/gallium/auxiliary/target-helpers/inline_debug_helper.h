
#ifndef INLINE_DEBUG_HELPER_H
#define INLINE_DEBUG_HELPER_H

#include "pipe/p_compiler.h"
#include "util/u_debug.h"
#include "util/u_tests.h"


/* Helper function to wrap a screen with
 * one or more debug driver: rbug, trace.
 */

#ifdef GALLIUM_DDEBUG
#include "ddebug/dd_public.h"
#endif

#ifdef GALLIUM_TRACE
#include "trace/tr_public.h"
#endif

#ifdef GALLIUM_RBUG
#include "rbug/rbug_public.h"
#endif

#ifdef GALLIUM_NOOP
#include "noop/noop_public.h"
#endif

/*
 * TODO: Audit the following *screen_create() - all of
 * them should return the original screen on failuire.
 */
static inline struct pipe_screen *
debug_screen_wrap(struct pipe_screen *screen)
{
#if defined(GALLIUM_DDEBUG)
   screen = ddebug_screen_create(screen);
#endif

#if defined(GALLIUM_RBUG)
   screen = rbug_screen_create(screen);
#endif

#if defined(GALLIUM_TRACE)
   screen = trace_screen_create(screen);
#endif

#if defined(GALLIUM_NOOP)
   screen = noop_screen_create(screen);
#endif

   if (debug_get_bool_option("GALLIUM_TESTS", FALSE))
      util_run_tests(screen);

   return screen;
}

#endif
