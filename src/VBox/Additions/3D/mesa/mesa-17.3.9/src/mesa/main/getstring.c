/*
 * Mesa 3-D graphics library
 *
 * Copyright (C) 1999-2008  Brian Paul   All Rights Reserved.
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


#include <stdbool.h>
#include "glheader.h"
#include "context.h"
#include "debug_output.h"
#include "get.h"
#include "enums.h"
#include "extensions.h"
#include "mtypes.h"
#include "macros.h"
#include "version.h"

/**
 * Return the string for a glGetString(GL_SHADING_LANGUAGE_VERSION) query.
 */
static const GLubyte *
shading_language_version(struct gl_context *ctx)
{
   switch (ctx->API) {
   case API_OPENGL_COMPAT:
   case API_OPENGL_CORE:
      switch (ctx->Const.GLSLVersion) {
      case 120:
         return (const GLubyte *) "1.20";
      case 130:
         return (const GLubyte *) "1.30";
      case 140:
         return (const GLubyte *) "1.40";
      case 150:
         return (const GLubyte *) "1.50";
      case 330:
         return (const GLubyte *) "3.30";
      case 400:
         return (const GLubyte *) "4.00";
      case 410:
         return (const GLubyte *) "4.10";
      case 420:
         return (const GLubyte *) "4.20";
      case 430:
         return (const GLubyte *) "4.30";
      case 440:
         return (const GLubyte *) "4.40";
      case 450:
         return (const GLubyte *) "4.50";
      case 460:
         return (const GLubyte *) "4.60";
      default:
         _mesa_problem(ctx,
                       "Invalid GLSL version in shading_language_version()");
         return (const GLubyte *) 0;
      }
      break;

   case API_OPENGLES2:
      switch (ctx->Version) {
      case 20:
         return (const GLubyte *) "OpenGL ES GLSL ES 1.0.16";
      case 30:
         return (const GLubyte *) "OpenGL ES GLSL ES 3.00";
      case 31:
         return (const GLubyte *) "OpenGL ES GLSL ES 3.10";
      case 32:
         return (const GLubyte *) "OpenGL ES GLSL ES 3.20";
      default:
         _mesa_problem(ctx,
                       "Invalid OpenGL ES version in shading_language_version()");
         return (const GLubyte *) 0;
      }
   case API_OPENGLES:
      /* fall-through */

   default:
      _mesa_problem(ctx, "Unexpected API value in shading_language_version()");
      return (const GLubyte *) 0;
   }
}


/**
 * Query string-valued state.  The return value should _not_ be freed by
 * the caller.
 *
 * \param name  the state variable to query.
 *
 * \sa glGetString().
 *
 * Tries to get the string from dd_function_table::GetString, otherwise returns
 * the hardcoded strings.
 */
const GLubyte * GLAPIENTRY
_mesa_GetString( GLenum name )
{
   GET_CURRENT_CONTEXT(ctx);
   static const char *vendor = "Brian Paul";
   static const char *renderer = "Mesa";

   if (!ctx)
      return NULL;

   ASSERT_OUTSIDE_BEGIN_END_WITH_RETVAL(ctx, NULL);

   /* this is a required driver function */
   assert(ctx->Driver.GetString);
   {
      /* Give the driver the chance to handle this query */
      const GLubyte *str = ctx->Driver.GetString(ctx, name);
      if (str)
         return str;
   }

   switch (name) {
      case GL_VENDOR:
         return (const GLubyte *) vendor;
      case GL_RENDERER:
         return (const GLubyte *) renderer;
      case GL_VERSION:
         return (const GLubyte *) ctx->VersionString;
      case GL_EXTENSIONS:
         if (ctx->API == API_OPENGL_CORE) {
            _mesa_error(ctx, GL_INVALID_ENUM, "glGetString(GL_EXTENSIONS)");
            return (const GLubyte *) 0;
         }
         return (const GLubyte *) ctx->Extensions.String;
      case GL_SHADING_LANGUAGE_VERSION:
         if (ctx->API == API_OPENGLES)
            break;
	 return shading_language_version(ctx);
      case GL_PROGRAM_ERROR_STRING_ARB:
         if (ctx->API == API_OPENGL_COMPAT &&
             (ctx->Extensions.ARB_fragment_program ||
              ctx->Extensions.ARB_vertex_program)) {
            return (const GLubyte *) ctx->Program.ErrorString;
         }
         break;
      default:
         break;
   }

   _mesa_error( ctx, GL_INVALID_ENUM, "glGetString" );
   return (const GLubyte *) 0;
}


/**
 * GL3
 */
const GLubyte * GLAPIENTRY
_mesa_GetStringi(GLenum name, GLuint index)
{
   GET_CURRENT_CONTEXT(ctx);

   if (!ctx)
      return NULL;

   ASSERT_OUTSIDE_BEGIN_END_WITH_RETVAL(ctx, NULL);

   switch (name) {
   case GL_EXTENSIONS:
      if (index >= _mesa_get_extension_count(ctx)) {
         _mesa_error(ctx, GL_INVALID_VALUE, "glGetStringi(index=%u)", index);
         return (const GLubyte *) 0;
      }
      return _mesa_get_enabled_extension(ctx, index);
   case GL_SHADING_LANGUAGE_VERSION:
      {
         char *version;
         int num;
         if (!_mesa_is_desktop_gl(ctx) || ctx->Version < 43) {
            _mesa_error(ctx, GL_INVALID_ENUM,
                        "glGetStringi(GL_SHADING_LANGUAGE_VERSION): "
                        "supported only in GL4.3 and later");
            return (const GLubyte *) 0;
         }
         num = _mesa_get_shading_language_version(ctx, index, &version);
         if (index >= num) {
            _mesa_error(ctx, GL_INVALID_VALUE,
                        "glGetStringi(GL_SHADING_LANGUAGE_VERSION, index=%d)",
                        index);
            return (const GLubyte *) 0;
         }
         return (const GLubyte *) version;
      }
   default:
      _mesa_error(ctx, GL_INVALID_ENUM, "glGetStringi");
      return (const GLubyte *) 0;
   }
}



/**
 * Return pointer-valued state, such as a vertex array pointer.
 *
 * \param pname  names state to be queried
 * \param params  returns the pointer value
 *
 * \sa glGetPointerv().
 *
 * Tries to get the specified pointer via dd_function_table::GetPointerv,
 * otherwise gets the specified pointer from the current context.
 */
void GLAPIENTRY
_mesa_GetPointerv( GLenum pname, GLvoid **params )
{
   GET_CURRENT_CONTEXT(ctx);
   const GLuint clientUnit = ctx->Array.ActiveTexture;
   const char *callerstr;

   if (_mesa_is_desktop_gl(ctx))
      callerstr = "glGetPointerv";
   else
      callerstr = "glGetPointervKHR";

   if (!params)
      return;

   if (MESA_VERBOSE & VERBOSE_API)
      _mesa_debug(ctx, "%s %s\n", callerstr, _mesa_enum_to_string(pname));

   switch (pname) {
      case GL_VERTEX_ARRAY_POINTER:
         if (ctx->API != API_OPENGL_COMPAT && ctx->API != API_OPENGLES)
            goto invalid_pname;
         *params = (GLvoid *) ctx->Array.VAO->VertexAttrib[VERT_ATTRIB_POS].Ptr;
         break;
      case GL_NORMAL_ARRAY_POINTER:
         if (ctx->API != API_OPENGL_COMPAT && ctx->API != API_OPENGLES)
            goto invalid_pname;
         *params = (GLvoid *) ctx->Array.VAO->VertexAttrib[VERT_ATTRIB_NORMAL].Ptr;
         break;
      case GL_COLOR_ARRAY_POINTER:
         if (ctx->API != API_OPENGL_COMPAT && ctx->API != API_OPENGLES)
            goto invalid_pname;
         *params = (GLvoid *) ctx->Array.VAO->VertexAttrib[VERT_ATTRIB_COLOR0].Ptr;
         break;
      case GL_SECONDARY_COLOR_ARRAY_POINTER_EXT:
         if (ctx->API != API_OPENGL_COMPAT)
            goto invalid_pname;
         *params = (GLvoid *) ctx->Array.VAO->VertexAttrib[VERT_ATTRIB_COLOR1].Ptr;
         break;
      case GL_FOG_COORDINATE_ARRAY_POINTER_EXT:
         if (ctx->API != API_OPENGL_COMPAT)
            goto invalid_pname;
         *params = (GLvoid *) ctx->Array.VAO->VertexAttrib[VERT_ATTRIB_FOG].Ptr;
         break;
      case GL_INDEX_ARRAY_POINTER:
         if (ctx->API != API_OPENGL_COMPAT)
            goto invalid_pname;
         *params = (GLvoid *) ctx->Array.VAO->VertexAttrib[VERT_ATTRIB_COLOR_INDEX].Ptr;
         break;
      case GL_TEXTURE_COORD_ARRAY_POINTER:
         if (ctx->API != API_OPENGL_COMPAT && ctx->API != API_OPENGLES)
            goto invalid_pname;
         *params = (GLvoid *) ctx->Array.VAO->VertexAttrib[VERT_ATTRIB_TEX(clientUnit)].Ptr;
         break;
      case GL_EDGE_FLAG_ARRAY_POINTER:
         if (ctx->API != API_OPENGL_COMPAT)
            goto invalid_pname;
         *params = (GLvoid *) ctx->Array.VAO->VertexAttrib[VERT_ATTRIB_EDGEFLAG].Ptr;
         break;
      case GL_FEEDBACK_BUFFER_POINTER:
         if (ctx->API != API_OPENGL_COMPAT)
            goto invalid_pname;
         *params = ctx->Feedback.Buffer;
         break;
      case GL_SELECTION_BUFFER_POINTER:
         if (ctx->API != API_OPENGL_COMPAT)
            goto invalid_pname;
         *params = ctx->Select.Buffer;
         break;
      case GL_POINT_SIZE_ARRAY_POINTER_OES:
         if (ctx->API != API_OPENGLES)
            goto invalid_pname;
         *params = (GLvoid *) ctx->Array.VAO->VertexAttrib[VERT_ATTRIB_POINT_SIZE].Ptr;
         break;
      case GL_DEBUG_CALLBACK_FUNCTION_ARB:
      case GL_DEBUG_CALLBACK_USER_PARAM_ARB:
         *params = _mesa_get_debug_state_ptr(ctx, pname);
         break;
      default:
         goto invalid_pname;
   }

   return;

invalid_pname:
   _mesa_error( ctx, GL_INVALID_ENUM, "%s", callerstr);
   return;
}


/**
 * Returns the current GL error code, or GL_NO_ERROR.
 * \return current error code
 *
 * Returns __struct gl_contextRec::ErrorValue.
 */
GLenum GLAPIENTRY
_mesa_GetError( void )
{
   GET_CURRENT_CONTEXT(ctx);
   GLenum e = ctx->ErrorValue;
   ASSERT_OUTSIDE_BEGIN_END_WITH_RETVAL(ctx, 0);

   /* From Issue (3) of the KHR_no_error spec:
    *
    *    "Should glGetError() always return NO_ERROR or have undefined
    *    results?
    *
    *    RESOLVED: It should for all errors except OUT_OF_MEMORY."
    */
   if (_mesa_is_no_error_enabled(ctx) && e != GL_OUT_OF_MEMORY) {
      e = GL_NO_ERROR;
   }

   if (MESA_VERBOSE & VERBOSE_API)
      _mesa_debug(ctx, "glGetError <-- %s\n", _mesa_enum_to_string(e));

   ctx->ErrorValue = (GLenum) GL_NO_ERROR;
   ctx->ErrorDebugCount = 0;
   return e;
}
