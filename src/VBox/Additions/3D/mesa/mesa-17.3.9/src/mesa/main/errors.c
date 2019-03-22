/**
 * \file errors.c
 * Mesa debugging and error handling functions.
 */

/*
 * Mesa 3-D graphics library
 *
 * Copyright (C) 1999-2007  Brian Paul   All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */


#include <stdarg.h>
#include <stdio.h>
#include "errors.h"
#include "enums.h"
#include "imports.h"
#include "context.h"
#include "debug_output.h"


static FILE *LogFile = NULL;


static void
output_if_debug(const char *prefixString, const char *outputString,
                GLboolean newline)
{
   static int debug = -1;

   /* Init the local 'debug' var once.
    * Note: the _mesa_init_debug() function should have been called
    * by now so MESA_DEBUG_FLAGS will be initialized.
    */
   if (debug == -1) {
      /* If MESA_LOG_FILE env var is set, log Mesa errors, warnings,
       * etc to the named file.  Otherwise, output to stderr.
       */
      const char *logFile = getenv("MESA_LOG_FILE");
      if (logFile)
         LogFile = fopen(logFile, "w");
      if (!LogFile)
         LogFile = stderr;
#ifdef DEBUG
      /* in debug builds, print messages unless MESA_DEBUG="silent" */
      if (MESA_DEBUG_FLAGS & DEBUG_SILENT)
         debug = 0;
      else
         debug = 1;
#else
      /* in release builds, be silent unless MESA_DEBUG is set */
      debug = getenv("MESA_DEBUG") != NULL;
#endif
   }

   /* Now only print the string if we're required to do so. */
   if (debug) {
      if (prefixString)
         fprintf(LogFile, "%s: %s", prefixString, outputString);
      else
         fprintf(LogFile, "%s", outputString);
      if (newline)
         fprintf(LogFile, "\n");
      fflush(LogFile);

#if defined(_WIN32)
      /* stderr from windows applications without console is not usually 
       * visible, so communicate with the debugger instead */ 
      {
         char buf[4096];
         _mesa_snprintf(buf, sizeof(buf), "%s: %s%s", prefixString, outputString, newline ? "\n" : "");
         OutputDebugStringA(buf);
      }
#endif
   }
}


/**
 * Return the file handle to use for debug/logging.  Defaults to stderr
 * unless MESA_LOG_FILE is defined.
 */
FILE *
_mesa_get_log_file(void)
{
   assert(LogFile);
   return LogFile;
}


/**
 * When a new type of error is recorded, print a message describing
 * previous errors which were accumulated.
 */
static void
flush_delayed_errors( struct gl_context *ctx )
{
   char s[MAX_DEBUG_MESSAGE_LENGTH];

   if (ctx->ErrorDebugCount) {
      _mesa_snprintf(s, MAX_DEBUG_MESSAGE_LENGTH, "%d similar %s errors",
                     ctx->ErrorDebugCount,
                     _mesa_enum_to_string(ctx->ErrorValue));

      output_if_debug("Mesa", s, GL_TRUE);

      ctx->ErrorDebugCount = 0;
   }
}


/**
 * Report a warning (a recoverable error condition) to stderr if
 * either DEBUG is defined or the MESA_DEBUG env var is set.
 *
 * \param ctx GL context.
 * \param fmtString printf()-like format string.
 */
void
_mesa_warning( struct gl_context *ctx, const char *fmtString, ... )
{
   char str[MAX_DEBUG_MESSAGE_LENGTH];
   va_list args;
   va_start( args, fmtString );
   (void) _mesa_vsnprintf( str, MAX_DEBUG_MESSAGE_LENGTH, fmtString, args );
   va_end( args );

   if (ctx)
      flush_delayed_errors( ctx );

   output_if_debug("Mesa warning", str, GL_TRUE);
}


/**
 * Report an internal implementation problem.
 * Prints the message to stderr via fprintf().
 *
 * \param ctx GL context.
 * \param fmtString problem description string.
 */
void
_mesa_problem( const struct gl_context *ctx, const char *fmtString, ... )
{
   va_list args;
   char str[MAX_DEBUG_MESSAGE_LENGTH];
   static int numCalls = 0;

   (void) ctx;

   if (numCalls < 50) {
      numCalls++;

      va_start( args, fmtString );
      _mesa_vsnprintf( str, MAX_DEBUG_MESSAGE_LENGTH, fmtString, args );
      va_end( args );
      fprintf(stderr, "Mesa " PACKAGE_VERSION " implementation error: %s\n",
              str);
      fprintf(stderr, "Please report at " PACKAGE_BUGREPORT "\n");
   }
}


static GLboolean
should_output(struct gl_context *ctx, GLenum error, const char *fmtString)
{
   static GLint debug = -1;

   /* Check debug environment variable only once:
    */
   if (debug == -1) {
      const char *debugEnv = getenv("MESA_DEBUG");

#ifdef DEBUG
      if (debugEnv && strstr(debugEnv, "silent"))
         debug = GL_FALSE;
      else
         debug = GL_TRUE;
#else
      if (debugEnv)
         debug = GL_TRUE;
      else
         debug = GL_FALSE;
#endif
   }

   if (debug) {
      if (ctx->ErrorValue != error ||
          ctx->ErrorDebugFmtString != fmtString) {
         flush_delayed_errors( ctx );
         ctx->ErrorDebugFmtString = fmtString;
         ctx->ErrorDebugCount = 0;
         return GL_TRUE;
      }
      ctx->ErrorDebugCount++;
   }
   return GL_FALSE;
}


void
_mesa_gl_vdebug(struct gl_context *ctx,
                GLuint *id,
                enum mesa_debug_source source,
                enum mesa_debug_type type,
                enum mesa_debug_severity severity,
                const char *fmtString,
                va_list args)
{
   char s[MAX_DEBUG_MESSAGE_LENGTH];
   int len;

   _mesa_debug_get_id(id);

   len = _mesa_vsnprintf(s, MAX_DEBUG_MESSAGE_LENGTH, fmtString, args);

   _mesa_log_msg(ctx, source, type, *id, severity, len, s);
}


void
_mesa_gl_debug(struct gl_context *ctx,
               GLuint *id,
               enum mesa_debug_source source,
               enum mesa_debug_type type,
               enum mesa_debug_severity severity,
               const char *fmtString, ...)
{
   va_list args;
   va_start(args, fmtString);
   _mesa_gl_vdebug(ctx, id, source, type, severity, fmtString, args);
   va_end(args);
}


/**
 * Record an OpenGL state error.  These usually occur when the user
 * passes invalid parameters to a GL function.
 *
 * If debugging is enabled (either at compile-time via the DEBUG macro, or
 * run-time via the MESA_DEBUG environment variable), report the error with
 * _mesa_debug().
 *
 * \param ctx the GL context.
 * \param error the error value.
 * \param fmtString printf() style format string, followed by optional args
 */
void
_mesa_error( struct gl_context *ctx, GLenum error, const char *fmtString, ... )
{
   GLboolean do_output, do_log;
   /* Ideally this would be set up by the caller, so that we had proper IDs
    * per different message.
    */
   static GLuint error_msg_id = 0;

   _mesa_debug_get_id(&error_msg_id);

   do_output = should_output(ctx, error, fmtString);

   mtx_lock(&ctx->DebugMutex);
   if (ctx->Debug) {
      do_log = _mesa_debug_is_message_enabled(ctx->Debug,
                                              MESA_DEBUG_SOURCE_API,
                                              MESA_DEBUG_TYPE_ERROR,
                                              error_msg_id,
                                              MESA_DEBUG_SEVERITY_HIGH);
   }
   else {
      do_log = GL_FALSE;
   }
   mtx_unlock(&ctx->DebugMutex);

   if (do_output || do_log) {
      char s[MAX_DEBUG_MESSAGE_LENGTH], s2[MAX_DEBUG_MESSAGE_LENGTH];
      int len;
      va_list args;

      va_start(args, fmtString);
      len = _mesa_vsnprintf(s, MAX_DEBUG_MESSAGE_LENGTH, fmtString, args);
      va_end(args);

      if (len >= MAX_DEBUG_MESSAGE_LENGTH) {
         /* Too long error message. Whoever calls _mesa_error should use
          * shorter strings.
          */
         assert(0);
         return;
      }

      len = _mesa_snprintf(s2, MAX_DEBUG_MESSAGE_LENGTH, "%s in %s",
                           _mesa_enum_to_string(error), s);
      if (len >= MAX_DEBUG_MESSAGE_LENGTH) {
         /* Same as above. */
         assert(0);
         return;
      }

      /* Print the error to stderr if needed. */
      if (do_output) {
         output_if_debug("Mesa: User error", s2, GL_TRUE);
      }

      /* Log the error via ARB_debug_output if needed.*/
      if (do_log) {
         _mesa_log_msg(ctx, MESA_DEBUG_SOURCE_API, MESA_DEBUG_TYPE_ERROR,
                       error_msg_id, MESA_DEBUG_SEVERITY_HIGH, len, s2);
      }
   }

   /* Set the GL context error state for glGetError. */
   _mesa_record_error(ctx, error);
}

void
_mesa_error_no_memory(const char *caller)
{
   GET_CURRENT_CONTEXT(ctx);
   _mesa_error(ctx, GL_OUT_OF_MEMORY, "out of memory in %s", caller);
}

/**
 * Report debug information.  Print error message to stderr via fprintf().
 * No-op if DEBUG mode not enabled.
 *
 * \param ctx GL context.
 * \param fmtString printf()-style format string, followed by optional args.
 */
void
_mesa_debug( const struct gl_context *ctx, const char *fmtString, ... )
{
#ifdef DEBUG
   char s[MAX_DEBUG_MESSAGE_LENGTH];
   va_list args;
   va_start(args, fmtString);
   _mesa_vsnprintf(s, MAX_DEBUG_MESSAGE_LENGTH, fmtString, args);
   va_end(args);
   output_if_debug("Mesa", s, GL_FALSE);
#endif /* DEBUG */
   (void) ctx;
   (void) fmtString;
}


void
_mesa_log(const char *fmtString, ...)
{
   char s[MAX_DEBUG_MESSAGE_LENGTH];
   va_list args;
   va_start(args, fmtString);
   _mesa_vsnprintf(s, MAX_DEBUG_MESSAGE_LENGTH, fmtString, args);
   va_end(args);
   output_if_debug("", s, GL_FALSE);
}


/**
 * Report debug information from the shader compiler via GL_ARB_debug_output.
 *
 * \param ctx GL context.
 * \param type The namespace to which this message belongs.
 * \param id The message ID within the given namespace.
 * \param msg The message to output. Must be null-terminated.
 */
void
_mesa_shader_debug(struct gl_context *ctx, GLenum type, GLuint *id,
                   const char *msg)
{
   enum mesa_debug_source source = MESA_DEBUG_SOURCE_SHADER_COMPILER;
   enum mesa_debug_severity severity = MESA_DEBUG_SEVERITY_HIGH;
   int len;

   _mesa_debug_get_id(id);

   len = strlen(msg);

   /* Truncate the message if necessary. */
   if (len >= MAX_DEBUG_MESSAGE_LENGTH)
      len = MAX_DEBUG_MESSAGE_LENGTH - 1;

   _mesa_log_msg(ctx, source, type, *id, severity, len, msg);
}
