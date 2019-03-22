/* Copyright (c) 2001, Stanford University
 * All rights reserved
 *
 * See the file LICENSE.txt for information on redistributing this software.
 */

#include "state.h"
#include "state/cr_statetypes.h"
#include "state/cr_statefuncs.h"
#include "state_internals.h"
#include "cr_mem.h"

#if !defined(IN_GUEST)
#include "cr_unpack.h"
#endif

void
crStateOcclusionInit(CRContext *ctx)
{
	CROcclusionState *o = &ctx->occlusion;

	o->objects = crAllocHashtable();
	o->currentQueryObject = 0;
}


void
crStateOcclusionDestroy(CRContext *ctx)
{
	CROcclusionState *o = &(ctx->occlusion);
	crFreeHashtable(o->objects, crFree);
}


static CROcclusionObject *
NewQueryObject(GLenum target, GLuint id)
{
	CROcclusionObject *q = (CROcclusionObject *) crAlloc(sizeof(CROcclusionObject));
   if (q) {
      q->target = target;
      q->name = id;
      q->passedCounter = 0;
      q->active = GL_FALSE;
   }
   return q;
}


void STATE_APIENTRY
crStateDeleteQueriesARB(GLsizei n, const GLuint *ids)
{
	CRContext *g = GetCurrentContext();
	CROcclusionState *o = &(g->occlusion);
	/*CRStateBits *sb = GetCurrentBits();*/
	/*CROcclusionBits *bb = &(sb->occlusion);*/
	int i;

	FLUSH();

	if (g->current.inBeginEnd) {
		crStateError(__LINE__, __FILE__, GL_INVALID_OPERATION,
								 "glDeleteQueriesARB called in Begin/End");
		return;
	}

    if (n <= 0 || n >= (GLsizei)(INT32_MAX / sizeof(GLuint)))
    {
        crStateError(__LINE__, __FILE__, GL_INVALID_OPERATION,
                     "glDeleteQueriesARB: parameter 'n' is out of range");
        return;
    }

#if !defined(IN_GUEST)
    if (!DATA_POINTER_CHECK(n * sizeof(GLuint)))
    {
        crError("glDeleteQueriesARB: parameter 'n' is out of range");
        return;
    }
#endif

    for (i = 0; i < n; i++) {
		if (ids[i]) {
			CROcclusionObject *q = (CROcclusionObject *)
				crHashtableSearch(o->objects, ids[i]);
			if (q) {
				crHashtableDelete(o->objects, ids[i], crFree);
			}
		}
	}
}


void STATE_APIENTRY
crStateGenQueriesARB(GLsizei n, GLuint * queries)
{
	CRContext *g = GetCurrentContext();
	CROcclusionState *o = &(g->occlusion);
	GLint start;

	FLUSH();

	if (g->current.inBeginEnd) {
		crStateError(__LINE__, __FILE__, GL_INVALID_OPERATION,
								 "glGenQueriesARB called in Begin/End");
		return;
	}

	if (n < 0) {
		crStateError(__LINE__, __FILE__, GL_INVALID_VALUE,
								 "glGenQueriesARB(n < 0)");
		return;
	}

	start = crHashtableAllocKeys(o->objects, n);
	if (start) {
		GLint i;
		for (i = 0; i < n; i++)
			queries[i] = (GLuint) (start + i);
	}
	else {
		crStateError(__LINE__, __FILE__, GL_OUT_OF_MEMORY, "glGenQueriesARB");
	}
}


GLboolean STATE_APIENTRY
crStateIsQueryARB(GLuint id)
{
	CRContext *g = GetCurrentContext();
	CROcclusionState *o = &(g->occlusion);

	FLUSH();

	if (g->current.inBeginEnd) {
		crStateError(__LINE__, __FILE__, GL_INVALID_OPERATION,
								 "glIsQueryARB called in begin/end");
		return GL_FALSE;
	}

	if (id && crHashtableIsKeyUsed(o->objects, id))
		return GL_TRUE;
	else
		return GL_FALSE;
}


void STATE_APIENTRY
crStateGetQueryivARB(GLenum target, GLenum pname, GLint *params)
{
	CRContext *g = GetCurrentContext();
	CROcclusionState *o = &(g->occlusion);
	(void)target;

	FLUSH();

	if (g->current.inBeginEnd) {
		crStateError(__LINE__, __FILE__, GL_INVALID_OPERATION,
								 "glGetGetQueryivARB called in begin/end");
		return;
	}

	switch (pname) {
	case GL_QUERY_COUNTER_BITS_ARB:
		 *params = 8 * sizeof(GLuint);
		 break;
	case GL_CURRENT_QUERY_ARB:
		 *params = o->currentQueryObject;
		 break;
	default:
		crStateError(__LINE__, __FILE__, GL_INVALID_ENUM,
								 "glGetGetQueryivARB(pname)");
		return;
	}
}


void STATE_APIENTRY
crStateGetQueryObjectivARB(GLuint id, GLenum pname, GLint *params)
{
	CRContext *g = GetCurrentContext();
	CROcclusionState *o = &(g->occlusion);
	CROcclusionObject *q;

	FLUSH();

	if (g->current.inBeginEnd) {
		crStateError(__LINE__, __FILE__, GL_INVALID_OPERATION,
								 "glGetGetQueryObjectivARB called in begin/end");
		return;
	}

	q = (CROcclusionObject *) crHashtableSearch(o->objects, id);
	if (!q || q->active) {
		crStateError(__LINE__, __FILE__, GL_INVALID_OPERATION,
								"glGetQueryObjectivARB");
		return;
	}

	switch (pname) {
	case GL_QUERY_RESULT_ARB:
		*params = q->passedCounter;
		break;
	case GL_QUERY_RESULT_AVAILABLE_ARB:
		*params = GL_TRUE;
		break;
	default:
		crStateError(__LINE__, __FILE__, GL_INVALID_ENUM,
								 "glGetQueryObjectivARB(pname)");
		return;
   }
}


void STATE_APIENTRY
crStateGetQueryObjectuivARB(GLuint id, GLenum pname, GLuint *params)
{
	CRContext *g = GetCurrentContext();
	CROcclusionState *o = &(g->occlusion);
	CROcclusionObject *q;

	FLUSH();

	if (g->current.inBeginEnd) {
		crStateError(__LINE__, __FILE__, GL_INVALID_OPERATION,
								 "glGetGetQueryObjectuivARB called in begin/end");
		return;
	}

	q = (CROcclusionObject *) crHashtableSearch(o->objects, id);
	if (!q || q->active) {
		crStateError(__LINE__, __FILE__, GL_INVALID_OPERATION,
								"glGetQueryObjectuivARB");
		return;
	}

	switch (pname) {
	case GL_QUERY_RESULT_ARB:
		*params = q->passedCounter;
		break;
	case GL_QUERY_RESULT_AVAILABLE_ARB:
		/* XXX revisit when we have a hardware implementation! */
		*params = GL_TRUE;
		break;
	default:
		crStateError(__LINE__, __FILE__, GL_INVALID_ENUM,
								 "glGetQueryObjectuivARB(pname)");
		return;
   }
}


void STATE_APIENTRY
crStateBeginQueryARB(GLenum target, GLuint id)
{
	CRContext *g = GetCurrentContext();
	CROcclusionState *o = &(g->occlusion);
	CROcclusionObject *q;

	FLUSH();

	if (g->current.inBeginEnd) {
		crStateError(__LINE__, __FILE__, GL_INVALID_OPERATION,
								 "glGetGetQueryObjectuivARB called in begin/end");
		return;
	}

	if (target != GL_SAMPLES_PASSED_ARB) {
		crStateError(__LINE__, __FILE__, GL_INVALID_ENUM,
								 "glBeginQueryARB(target)");
		return;
	}

	if (o->currentQueryObject) {
		crStateError(__LINE__, __FILE__, GL_INVALID_OPERATION,
								 "glBeginQueryARB(target)");
		return;
	}

	q = (CROcclusionObject *) crHashtableSearch(o->objects, id);
	if (q && q->active) {
		crStateError(__LINE__, __FILE__, GL_INVALID_OPERATION, "glBeginQueryARB");
		return;
	}
	else if (!q) {
		q = NewQueryObject(target, id);
		if (!q) {
			crStateError(__LINE__, __FILE__, GL_OUT_OF_MEMORY, "glBeginQueryARB");
			return;
		}
		crHashtableAdd(o->objects, id, q);
	}
	
	q->active = GL_TRUE;
	q->passedCounter = 0;
	q->active = GL_TRUE;
	q->passedCounter = 0;
	o->currentQueryObject = id;
}


void STATE_APIENTRY
crStateEndQueryARB(GLenum target)
{
	CRContext *g = GetCurrentContext();
	CROcclusionState *o = &(g->occlusion);
	CROcclusionObject *q;

	FLUSH();

	if (g->current.inBeginEnd) {
		crStateError(__LINE__, __FILE__, GL_INVALID_OPERATION,
								 "glGetGetQueryObjectuivARB called in begin/end");
		return;
	}

	if (target != GL_SAMPLES_PASSED_ARB) {
		crStateError(__LINE__, __FILE__, GL_INVALID_ENUM, "glEndQueryARB(target)");
		return;
	}

	q = (CROcclusionObject *) crHashtableSearch(o->objects, o->currentQueryObject);
	if (!q || !q->active) {
		crStateError(__LINE__, __FILE__, GL_INVALID_OPERATION,
								 "glEndQueryARB with glBeginQueryARB");
		return;
	}

	q->passedCounter = 0;
	q->active = GL_FALSE;
	o->currentQueryObject = 0;
}


void crStateOcclusionDiff(CROcclusionBits *bb, CRbitvalue *bitID,
													CRContext *fromCtx, CRContext *toCtx)
{
	/* Apparently, no occlusion state differencing needed */
	(void)bb; (void)bitID; (void)fromCtx; (void)toCtx;
}


/*
 * XXX this function might need some testing/fixing.
 */
void crStateOcclusionSwitch(CROcclusionBits *bb, CRbitvalue *bitID, 
														CRContext *fromCtx, CRContext *toCtx)
{
	/* Apparently, no occlusion state switching needed */
	/* Note: we better not do a switch while we're inside a glBeginQuery/
	 * glEndQuery sequence.
	 */
	(void)bb; (void)bitID; (void)fromCtx; (void)toCtx;
	CRASSERT(!fromCtx->occlusion.currentQueryObject);
}
