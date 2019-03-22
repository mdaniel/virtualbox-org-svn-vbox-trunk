/*
 * Copyright 2011 Joakim Sindholt <opensource@zhasha.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * on the rights to use, copy, modify, merge, publish, distribute, sub
 * license, and/or sell copies of the Software, and to permit persons to whom
 * the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHOR(S) AND/OR THEIR SUPPLIERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
 * USE OR OTHER DEALINGS IN THE SOFTWARE. */

#include "swapchain9.h"
#include "surface9.h"
#include "device9.h"

#include "nine_helpers.h"
#include "nine_pipe.h"
#include "nine_dump.h"

#include "util/u_inlines.h"
#include "util/u_surface.h"
#include "hud/hud_context.h"
#include "state_tracker/drm_driver.h"

#include "threadpool.h"

#define DBG_CHANNEL DBG_SWAPCHAIN

#define UNTESTED(n) DBG("UNTESTED point %d. Please tell if it worked\n", n)

HRESULT
NineSwapChain9_ctor( struct NineSwapChain9 *This,
                     struct NineUnknownParams *pParams,
                     BOOL implicit,
                     ID3DPresent *pPresent,
                     D3DPRESENT_PARAMETERS *pPresentationParameters,
                     struct d3dadapter9_context *pCTX,
                     HWND hFocusWindow,
                     D3DDISPLAYMODEEX *mode )
{
    HRESULT hr;

    DBG("This=%p pDevice=%p pPresent=%p pCTX=%p hFocusWindow=%p\n",
        This, pParams->device, pPresent, pCTX, hFocusWindow);

    hr = NineUnknown_ctor(&This->base, pParams);
    if (FAILED(hr))
        return hr;

    This->screen = NineDevice9_GetScreen(This->base.device);
    This->implicit = implicit;
    This->actx = pCTX;
    This->present = pPresent;
    This->mode = NULL;

    ID3DPresent_AddRef(pPresent);
    if (!This->actx->thread_submit &&
        This->base.device->minor_version_num > 2) {
        D3DPRESENT_PARAMETERS2 params2;

        memset(&params2, 0, sizeof(D3DPRESENT_PARAMETERS2));
        params2.AllowDISCARDDelayedRelease = This->actx->discard_delayed_release;
        params2.TearFreeDISCARD = This->actx->tearfree_discard;
        ID3DPresent_SetPresentParameters2(pPresent, &params2);
    }

    if (!pPresentationParameters->hDeviceWindow)
        pPresentationParameters->hDeviceWindow = hFocusWindow;

    This->rendering_done = FALSE;
    This->pool = NULL;
    return NineSwapChain9_Resize(This, pPresentationParameters, mode);
}

static D3DWindowBuffer *
D3DWindowBuffer_create(struct NineSwapChain9 *This,
                       struct pipe_resource *resource,
                       int depth,
                       int for_frontbuffer_reading)
{
    D3DWindowBuffer *ret;
    struct pipe_context *pipe = nine_context_get_pipe_acquire(This->base.device);
    struct winsys_handle whandle;
    int stride, dmaBufFd;
    HRESULT hr;

    memset(&whandle, 0, sizeof(whandle));
    whandle.type = DRM_API_HANDLE_TYPE_FD;
    This->screen->resource_get_handle(This->screen, pipe, resource,
                                      &whandle,
                                      for_frontbuffer_reading ?
                                          PIPE_HANDLE_USAGE_WRITE :
                                          PIPE_HANDLE_USAGE_EXPLICIT_FLUSH |
                                          PIPE_HANDLE_USAGE_READ);
    nine_context_get_pipe_release(This->base.device);
    stride = whandle.stride;
    dmaBufFd = whandle.handle;
    hr = ID3DPresent_NewD3DWindowBufferFromDmaBuf(This->present,
                                                  dmaBufFd,
                                                  resource->width0,
                                                  resource->height0,
                                                  stride,
                                                  depth,
                                                  32,
                                                  &ret);
    assert (SUCCEEDED(hr));

    if (FAILED(hr)) {
        ERR("Failed to create new D3DWindowBufferFromDmaBuf\n");
        return NULL;
    }
    return ret;
}

static int
NineSwapChain9_GetBackBufferCountForParams( struct NineSwapChain9 *This,
                                            D3DPRESENT_PARAMETERS *pParams );

HRESULT
NineSwapChain9_Resize( struct NineSwapChain9 *This,
                       D3DPRESENT_PARAMETERS *pParams,
                       D3DDISPLAYMODEEX *mode )
{
    struct NineDevice9 *pDevice = This->base.device;
    D3DSURFACE_DESC desc;
    HRESULT hr;
    struct pipe_resource *resource, tmplt;
    enum pipe_format pf;
    BOOL has_present_buffers = FALSE;
    int depth;
    unsigned i, oldBufferCount, newBufferCount;
    D3DMULTISAMPLE_TYPE multisample_type;

    DBG("This=%p pParams=%p\n", This, pParams);
    user_assert(pParams != NULL, E_POINTER);
    user_assert(pParams->SwapEffect, D3DERR_INVALIDCALL);
    user_assert((pParams->SwapEffect != D3DSWAPEFFECT_COPY) ||
                (pParams->BackBufferCount <= 1), D3DERR_INVALIDCALL);
    user_assert(pDevice->ex || pParams->BackBufferCount <=
                D3DPRESENT_BACK_BUFFERS_MAX, D3DERR_INVALIDCALL);
    user_assert(!pDevice->ex || pParams->BackBufferCount <=
                D3DPRESENT_BACK_BUFFERS_MAX_EX, D3DERR_INVALIDCALL);
    user_assert(pDevice->ex ||
                (pParams->SwapEffect == D3DSWAPEFFECT_FLIP) ||
                (pParams->SwapEffect == D3DSWAPEFFECT_COPY) ||
                (pParams->SwapEffect == D3DSWAPEFFECT_DISCARD), D3DERR_INVALIDCALL);

    DBG("pParams(%p):\n"
        "BackBufferWidth: %u\n"
        "BackBufferHeight: %u\n"
        "BackBufferFormat: %s\n"
        "BackBufferCount: %u\n"
        "MultiSampleType: %u\n"
        "MultiSampleQuality: %u\n"
        "SwapEffect: %u\n"
        "hDeviceWindow: %p\n"
        "Windowed: %i\n"
        "EnableAutoDepthStencil: %i\n"
        "AutoDepthStencilFormat: %s\n"
        "Flags: %s\n"
        "FullScreen_RefreshRateInHz: %u\n"
        "PresentationInterval: %x\n", pParams,
        pParams->BackBufferWidth, pParams->BackBufferHeight,
        d3dformat_to_string(pParams->BackBufferFormat),
        pParams->BackBufferCount,
        pParams->MultiSampleType, pParams->MultiSampleQuality,
        pParams->SwapEffect, pParams->hDeviceWindow, pParams->Windowed,
        pParams->EnableAutoDepthStencil,
        d3dformat_to_string(pParams->AutoDepthStencilFormat),
        nine_D3DPRESENTFLAG_to_str(pParams->Flags),
        pParams->FullScreen_RefreshRateInHz,
        pParams->PresentationInterval);

    if (pParams->BackBufferCount == 0) {
        pParams->BackBufferCount = 1;
    }

    if (pParams->BackBufferFormat == D3DFMT_UNKNOWN) {
        pParams->BackBufferFormat = D3DFMT_A8R8G8B8;
    }

    This->desired_fences = This->actx->throttling ? This->actx->throttling_value + 1 : 0;
    /* +1 because we add the fence of the current buffer before popping an old one */
    if (This->desired_fences > DRI_SWAP_FENCES_MAX)
        This->desired_fences = DRI_SWAP_FENCES_MAX;

    if (This->actx->vblank_mode == 0)
        pParams->PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
    else if (This->actx->vblank_mode == 3)
        pParams->PresentationInterval = D3DPRESENT_INTERVAL_ONE;

    if (mode && This->mode) {
        *(This->mode) = *mode;
    } else if (mode) {
        This->mode = malloc(sizeof(D3DDISPLAYMODEEX));
        memcpy(This->mode, mode, sizeof(D3DDISPLAYMODEEX));
    } else {
        free(This->mode);
        This->mode = NULL;
    }

    /* Note: It is the role of the backend to fill if necessary
     * BackBufferWidth and BackBufferHeight */
    hr = ID3DPresent_SetPresentParameters(This->present, pParams, This->mode);
    if (hr != D3D_OK)
        return hr;

    oldBufferCount = This->num_back_buffers;
    newBufferCount = NineSwapChain9_GetBackBufferCountForParams(This, pParams);

    multisample_type = pParams->MultiSampleType;

    /* Map MultiSampleQuality to MultiSampleType */
    hr = d3dmultisample_type_check(This->screen, pParams->BackBufferFormat,
                                   &multisample_type,
                                   pParams->MultiSampleQuality,
                                   NULL);
    if (FAILED(hr)) {
        return hr;
    }

    pf = d3d9_to_pipe_format_checked(This->screen, pParams->BackBufferFormat,
                                     PIPE_TEXTURE_2D, multisample_type,
                                     PIPE_BIND_RENDER_TARGET, FALSE, FALSE);

    if (This->actx->linear_framebuffer ||
        (pf != PIPE_FORMAT_B8G8R8X8_UNORM &&
        pf != PIPE_FORMAT_B8G8R8A8_UNORM) ||
        pParams->SwapEffect != D3DSWAPEFFECT_DISCARD ||
        multisample_type >= 2 ||
        (This->actx->ref && This->actx->ref == This->screen))
        has_present_buffers = TRUE;

    /* Note: the buffer depth has to match the window depth.
     * In practice, ARGB buffers can be used with windows
     * of depth 24. Windows of depth 32 are extremely rare.
     * So even if the buffer is ARGB, say it is depth 24.
     * It is common practice, for example that's how
     * glamor implements depth 24.
     * TODO: handle windows with other depths. Not possible in the short term.
     * For example 16 bits.*/
    depth = 24;

    memset(&tmplt, 0, sizeof(tmplt));
    tmplt.target = PIPE_TEXTURE_2D;
    tmplt.width0 = pParams->BackBufferWidth;
    tmplt.height0 = pParams->BackBufferHeight;
    tmplt.depth0 = 1;
    tmplt.last_level = 0;
    tmplt.array_size = 1;
    tmplt.usage = PIPE_USAGE_DEFAULT;
    tmplt.flags = 0;

    desc.Type = D3DRTYPE_SURFACE;
    desc.Pool = D3DPOOL_DEFAULT;
    desc.MultiSampleType = pParams->MultiSampleType;
    desc.MultiSampleQuality = pParams->MultiSampleQuality;
    desc.Width = pParams->BackBufferWidth;
    desc.Height = pParams->BackBufferHeight;

    for (i = 0; i < oldBufferCount; i++) {
        if (This->tasks[i])
            _mesa_threadpool_wait_for_task(This->pool, &(This->tasks[i]));
    }
    memset(This->tasks, 0, sizeof(This->tasks));

    if (This->pool) {
        _mesa_threadpool_destroy(This, This->pool);
        This->pool = NULL;
    }
    This->enable_threadpool = This->actx->thread_submit && (pParams->SwapEffect != D3DSWAPEFFECT_COPY);
    if (This->enable_threadpool)
        This->pool = _mesa_threadpool_create(This);
    if (!This->pool)
        This->enable_threadpool = FALSE;

    for (i = 0; i < oldBufferCount; i++) {
        ID3DPresent_DestroyD3DWindowBuffer(This->present, This->present_handles[i]);
        This->present_handles[i] = NULL;
        if (This->present_buffers[i])
            pipe_resource_reference(&(This->present_buffers[i]), NULL);
    }

    if (newBufferCount != oldBufferCount) {
        for (i = newBufferCount; i < oldBufferCount;
             ++i)
            NineUnknown_Detach(NineUnknown(This->buffers[i]));

        for (i = oldBufferCount; i < newBufferCount; ++i) {
            This->buffers[i] = NULL;
            This->present_handles[i] = NULL;
        }
    }
    This->num_back_buffers = newBufferCount;

    for (i = 0; i < newBufferCount; ++i) {
        tmplt.bind = PIPE_BIND_SAMPLER_VIEW | PIPE_BIND_RENDER_TARGET;
        tmplt.nr_samples = multisample_type;
        if (!has_present_buffers)
            tmplt.bind |= NINE_BIND_PRESENTBUFFER_FLAGS;
        tmplt.format = d3d9_to_pipe_format_checked(This->screen,
                                                   pParams->BackBufferFormat,
                                                   PIPE_TEXTURE_2D,
                                                   tmplt.nr_samples,
                                                   tmplt.bind, FALSE, FALSE);
        if (tmplt.format == PIPE_FORMAT_NONE)
            return D3DERR_INVALIDCALL;
        resource = This->screen->resource_create(This->screen, &tmplt);
        if (!resource) {
            DBG("Failed to create pipe_resource.\n");
            return D3DERR_OUTOFVIDEOMEMORY;
        }
        if (pParams->Flags & D3DPRESENTFLAG_LOCKABLE_BACKBUFFER)
            resource->flags |= NINE_RESOURCE_FLAG_LOCKABLE;
        if (This->buffers[i]) {
            NineSurface9_SetMultiSampleType(This->buffers[i], desc.MultiSampleType);
            NineSurface9_SetResourceResize(This->buffers[i], resource);
            if (has_present_buffers)
                pipe_resource_reference(&resource, NULL);
        } else {
            desc.Format = pParams->BackBufferFormat;
            desc.Usage = D3DUSAGE_RENDERTARGET;
            hr = NineSurface9_new(pDevice, NineUnknown(This), resource, NULL, 0,
                                  0, 0, &desc, &This->buffers[i]);
            if (has_present_buffers)
                pipe_resource_reference(&resource, NULL);
            if (FAILED(hr)) {
                DBG("Failed to create RT surface.\n");
                return hr;
            }
            This->buffers[i]->base.base.forward = FALSE;
        }
        if (has_present_buffers) {
            tmplt.format = PIPE_FORMAT_B8G8R8X8_UNORM;
            tmplt.bind = NINE_BIND_PRESENTBUFFER_FLAGS;
            tmplt.nr_samples = 0;
            if (This->actx->linear_framebuffer)
                tmplt.bind |= PIPE_BIND_LINEAR;
            if (pParams->SwapEffect != D3DSWAPEFFECT_DISCARD)
                tmplt.bind |= PIPE_BIND_RENDER_TARGET;
            resource = This->screen->resource_create(This->screen, &tmplt);
            pipe_resource_reference(&(This->present_buffers[i]), resource);
        }
        This->present_handles[i] = D3DWindowBuffer_create(This, resource, depth, false);
        pipe_resource_reference(&resource, NULL);
        if (!This->present_handles[i]) {
            return D3DERR_DRIVERINTERNALERROR;
        }
    }
    if (pParams->EnableAutoDepthStencil) {
        tmplt.bind = d3d9_get_pipe_depth_format_bindings(pParams->AutoDepthStencilFormat);
        tmplt.nr_samples = multisample_type;
        tmplt.format = d3d9_to_pipe_format_checked(This->screen,
                                                   pParams->AutoDepthStencilFormat,
                                                   PIPE_TEXTURE_2D,
                                                   tmplt.nr_samples,
                                                   tmplt.bind,
                                                   FALSE, FALSE);

        if (tmplt.format == PIPE_FORMAT_NONE)
            return D3DERR_INVALIDCALL;

        if (This->zsbuf) {
            resource = This->screen->resource_create(This->screen, &tmplt);
            if (!resource) {
                DBG("Failed to create pipe_resource for depth buffer.\n");
                return D3DERR_OUTOFVIDEOMEMORY;
            }

            NineSurface9_SetMultiSampleType(This->zsbuf, desc.MultiSampleType);
            NineSurface9_SetResourceResize(This->zsbuf, resource);
            pipe_resource_reference(&resource, NULL);
        } else {
            hr = NineDevice9_CreateDepthStencilSurface(pDevice,
                                                       pParams->BackBufferWidth,
                                                       pParams->BackBufferHeight,
                                                       pParams->AutoDepthStencilFormat,
                                                       pParams->MultiSampleType,
                                                       pParams->MultiSampleQuality,
                                                       0,
                                                       (IDirect3DSurface9 **)&This->zsbuf,
                                                       NULL);
            if (FAILED(hr)) {
                DBG("Failed to create ZS surface.\n");
                return hr;
            }
            NineUnknown_ConvertRefToBind(NineUnknown(This->zsbuf));
        }
    }

    This->params = *pParams;

    return D3D_OK;
}

/* Throttling: code adapted from the dri state tracker */

/**
 * swap_fences_pop_front - pull a fence from the throttle queue
 *
 * If the throttle queue is filled to the desired number of fences,
 * pull fences off the queue until the number is less than the desired
 * number of fences, and return the last fence pulled.
 */
static struct pipe_fence_handle *
swap_fences_pop_front(struct NineSwapChain9 *This)
{
    struct pipe_screen *screen = This->screen;
    struct pipe_fence_handle *fence = NULL;

    if (This->desired_fences == 0)
        return NULL;

    if (This->cur_fences >= This->desired_fences) {
        screen->fence_reference(screen, &fence, This->swap_fences[This->tail]);
        screen->fence_reference(screen, &This->swap_fences[This->tail++], NULL);
        This->tail &= DRI_SWAP_FENCES_MASK;
        --This->cur_fences;
    }
    return fence;
}


/**
 * swap_fences_see_front - same than swap_fences_pop_front without
 * pulling
 *
 */

static struct pipe_fence_handle *
swap_fences_see_front(struct NineSwapChain9 *This)
{
    struct pipe_screen *screen = This->screen;
    struct pipe_fence_handle *fence = NULL;

    if (This->desired_fences == 0)
        return NULL;

    if (This->cur_fences >= This->desired_fences) {
        screen->fence_reference(screen, &fence, This->swap_fences[This->tail]);
    }
    return fence;
}


/**
 * swap_fences_push_back - push a fence onto the throttle queue at the back
 *
 * push a fence onto the throttle queue and pull fences of the queue
 * so that the desired number of fences are on the queue.
 */
static void
swap_fences_push_back(struct NineSwapChain9 *This,
                      struct pipe_fence_handle *fence)
{
    struct pipe_screen *screen = This->screen;

    if (!fence || This->desired_fences == 0)
        return;

    while(This->cur_fences == This->desired_fences)
        swap_fences_pop_front(This);

    This->cur_fences++;
    screen->fence_reference(screen, &This->swap_fences[This->head++],
                            fence);
    This->head &= DRI_SWAP_FENCES_MASK;
}


/**
 * swap_fences_unref - empty the throttle queue
 *
 * pulls fences of the throttle queue until it is empty.
 */
static void
swap_fences_unref(struct NineSwapChain9 *This)
{
    struct pipe_screen *screen = This->screen;

    while(This->cur_fences) {
        screen->fence_reference(screen, &This->swap_fences[This->tail++], NULL);
        This->tail &= DRI_SWAP_FENCES_MASK;
        --This->cur_fences;
    }
}

void
NineSwapChain9_dtor( struct NineSwapChain9 *This )
{
    unsigned i;

    DBG("This=%p\n", This);

    if (This->pool)
        _mesa_threadpool_destroy(This, This->pool);

    for (i = 0; i < This->num_back_buffers; i++) {
        if (This->buffers[i])
            NineUnknown_Detach(NineUnknown(This->buffers[i]));
        if (This->present_handles[i])
            ID3DPresent_DestroyD3DWindowBuffer(This->present, This->present_handles[i]);
        if (This->present_buffers[i])
            pipe_resource_reference(&(This->present_buffers[i]), NULL);
    }
    if (This->zsbuf)
        NineUnknown_Unbind(NineUnknown(This->zsbuf));

    if (This->present)
        ID3DPresent_Release(This->present);

    swap_fences_unref(This);
    NineUnknown_dtor(&This->base);
}

static void
create_present_buffer( struct NineSwapChain9 *This,
                       unsigned int width, unsigned int height,
                       struct pipe_resource **resource,
                       D3DWindowBuffer **present_handle)
{
    struct pipe_resource tmplt;

    memset(&tmplt, 0, sizeof(tmplt));
    tmplt.target = PIPE_TEXTURE_2D;
    tmplt.width0 = width;
    tmplt.height0 = height;
    tmplt.depth0 = 1;
    tmplt.last_level = 0;
    tmplt.array_size = 1;
    tmplt.usage = PIPE_USAGE_DEFAULT;
    tmplt.flags = 0;
    tmplt.format = PIPE_FORMAT_B8G8R8X8_UNORM;
    tmplt.bind = NINE_BIND_BACKBUFFER_FLAGS |
                 NINE_BIND_PRESENTBUFFER_FLAGS;
    tmplt.nr_samples = 0;
    if (This->actx->linear_framebuffer)
        tmplt.bind |= PIPE_BIND_LINEAR;
    *resource = This->screen->resource_create(This->screen, &tmplt);

    *present_handle = D3DWindowBuffer_create(This, *resource, 24, true);

    if (!*present_handle) {
        pipe_resource_reference(resource, NULL);
    }
}

static void
handle_draw_cursor_and_hud( struct NineSwapChain9 *This, struct pipe_resource *resource)
{
    struct NineDevice9 *device = This->base.device;
    struct pipe_blit_info blit;
    struct pipe_context *pipe;

    if (device->cursor.software && device->cursor.visible && device->cursor.w) {
        memset(&blit, 0, sizeof(blit));
        blit.src.resource = device->cursor.image;
        blit.src.level = 0;
        blit.src.format = device->cursor.image->format;
        blit.src.box.x = 0;
        blit.src.box.y = 0;
        blit.src.box.z = 0;
        blit.src.box.depth = 1;
        blit.src.box.width = device->cursor.w;
        blit.src.box.height = device->cursor.h;

        blit.dst.resource = resource;
        blit.dst.level = 0;
        blit.dst.format = resource->format;
        blit.dst.box.z = 0;
        blit.dst.box.depth = 1;

        blit.mask = PIPE_MASK_RGBA;
        blit.filter = PIPE_TEX_FILTER_NEAREST;
        blit.scissor_enable = FALSE;

        /* NOTE: blit messes up when box.x + box.width < 0, fix driver
         * NOTE2: device->cursor.pos contains coordinates relative to the screen.
         * This happens to be also the position of the cursor when we are fullscreen.
         * We don't use sw cursor for Windowed mode */
        blit.dst.box.x = MAX2(device->cursor.pos.x, 0) - device->cursor.hotspot.x;
        blit.dst.box.y = MAX2(device->cursor.pos.y, 0) - device->cursor.hotspot.y;
        blit.dst.box.width = blit.src.box.width;
        blit.dst.box.height = blit.src.box.height;

        DBG("Blitting cursor(%ux%u) to (%i,%i).\n",
            blit.src.box.width, blit.src.box.height,
            blit.dst.box.x, blit.dst.box.y);

        blit.alpha_blend = TRUE;
        pipe = NineDevice9_GetPipe(This->base.device);
        pipe->blit(pipe, &blit);
    }

    if (device->hud && resource) {
        /* Implicit use of context pipe */
        (void)NineDevice9_GetPipe(This->base.device);
        hud_draw(device->hud, resource); /* XXX: no offset */
        /* HUD doesn't clobber stipple */
        nine_state_restore_non_cso(device);
    }
}

struct end_present_struct {
    struct pipe_screen *screen;
    struct pipe_fence_handle *fence_to_wait;
    ID3DPresent *present;
    D3DWindowBuffer *present_handle;
    HWND hDestWindowOverride;
};

static void work_present(void *data)
{
    struct end_present_struct *work = data;
    if (work->fence_to_wait) {
        (void) work->screen->fence_finish(work->screen, NULL, work->fence_to_wait, PIPE_TIMEOUT_INFINITE);
        work->screen->fence_reference(work->screen, &(work->fence_to_wait), NULL);
    }
    ID3DPresent_PresentBuffer(work->present, work->present_handle, work->hDestWindowOverride, NULL, NULL, NULL, 0);
    free(work);
}

static void pend_present(struct NineSwapChain9 *This,
                         HWND hDestWindowOverride)
{
    struct end_present_struct *work = calloc(1, sizeof(struct end_present_struct));

    work->screen = This->screen;
    work->fence_to_wait = swap_fences_pop_front(This);
    work->present = This->present;
    work->present_handle = This->present_handles[0];
    work->hDestWindowOverride = hDestWindowOverride;
    This->tasks[0] = _mesa_threadpool_queue_task(This->pool, work_present, work);

    return;
}

static inline HRESULT
present( struct NineSwapChain9 *This,
         const RECT *pSourceRect,
         const RECT *pDestRect,
         HWND hDestWindowOverride,
         const RGNDATA *pDirtyRegion,
         DWORD dwFlags )
{
    struct pipe_context *pipe;
    struct pipe_resource *resource;
    struct pipe_fence_handle *fence;
    HRESULT hr;
    struct pipe_blit_info blit;

    DBG("present: This=%p pSourceRect=%p pDestRect=%p "
        "pDirtyRegion=%p hDestWindowOverride=%p"
        "dwFlags=%d resource=%p\n",
        This, pSourceRect, pDestRect, pDirtyRegion,
        hDestWindowOverride, (int)dwFlags, This->buffers[0]->base.resource);

    if (pSourceRect)
        DBG("pSourceRect = (%u..%u)x(%u..%u)\n",
            pSourceRect->left, pSourceRect->right,
            pSourceRect->top, pSourceRect->bottom);
    if (pDestRect)
        DBG("pDestRect = (%u..%u)x(%u..%u)\n",
            pDestRect->left, pDestRect->right,
            pDestRect->top, pDestRect->bottom);

    /* TODO: in the case the source and destination rect have different size:
     * We need to allocate a new buffer, and do a blit to it to resize.
     * We can't use the present_buffer for that since when we created it,
     * we couldn't guess which size would have been needed.
     * If pDestRect or pSourceRect is null, we have to check the sizes
     * from the source size, and the destination window size.
     * In this case, either resize rngdata, or pass NULL instead
     */
    /* Note: This->buffers[0]->level should always be 0 */

    if (This->rendering_done)
        goto bypass_rendering;

    resource = This->buffers[0]->base.resource;

    if (This->params.SwapEffect == D3DSWAPEFFECT_DISCARD)
        handle_draw_cursor_and_hud(This, resource);

    pipe = NineDevice9_GetPipe(This->base.device);

    if (This->present_buffers[0]) {
        memset(&blit, 0, sizeof(blit));
        blit.src.resource = resource;
        blit.src.level = 0;
        blit.src.format = resource->format;
        blit.src.box.z = 0;
        blit.src.box.depth = 1;
        blit.src.box.x = 0;
        blit.src.box.y = 0;
        blit.src.box.width = resource->width0;
        blit.src.box.height = resource->height0;

        resource = This->present_buffers[0];

        blit.dst.resource = resource;
        blit.dst.level = 0;
        blit.dst.format = resource->format;
        blit.dst.box.z = 0;
        blit.dst.box.depth = 1;
        blit.dst.box.x = 0;
        blit.dst.box.y = 0;
        blit.dst.box.width = resource->width0;
        blit.dst.box.height = resource->height0;

        blit.mask = PIPE_MASK_RGBA;
        blit.filter = PIPE_TEX_FILTER_NEAREST;
        blit.scissor_enable = FALSE;
        blit.alpha_blend = FALSE;

        pipe->blit(pipe, &blit);
    }

    /* The resource we present has to resolve fast clears
     * if needed (and other things) */
    pipe->flush_resource(pipe, resource);

    if (This->params.SwapEffect != D3DSWAPEFFECT_DISCARD)
        handle_draw_cursor_and_hud(This, resource);

    fence = NULL;
    pipe->flush(pipe, &fence, PIPE_FLUSH_END_OF_FRAME);
    if (fence) {
        swap_fences_push_back(This, fence);
        This->screen->fence_reference(This->screen, &fence, NULL);
    }

    This->rendering_done = TRUE;
bypass_rendering:

    if (dwFlags & D3DPRESENT_DONOTWAIT) {
        UNTESTED(2);
        BOOL still_draw = FALSE;
        fence = swap_fences_see_front(This);
        if (fence) {
            still_draw = !This->screen->fence_finish(This->screen, NULL, fence, 0);
            This->screen->fence_reference(This->screen, &fence, NULL);
        }
        if (still_draw)
            return D3DERR_WASSTILLDRAWING;
    }

    if (!This->enable_threadpool) {
        This->tasks[0]=NULL;
        fence = swap_fences_pop_front(This);
        if (fence) {
            (void) This->screen->fence_finish(This->screen, NULL, fence, PIPE_TIMEOUT_INFINITE);
            This->screen->fence_reference(This->screen, &fence, NULL);
        }

        hr = ID3DPresent_PresentBuffer(This->present, This->present_handles[0], hDestWindowOverride, pSourceRect, pDestRect, pDirtyRegion, dwFlags);

        if (FAILED(hr)) { UNTESTED(3);return hr; }
    } else {
        pend_present(This, hDestWindowOverride);
    }
    This->rendering_done = FALSE;

    return D3D_OK;
}

HRESULT NINE_WINAPI
NineSwapChain9_Present( struct NineSwapChain9 *This,
                        const RECT *pSourceRect,
                        const RECT *pDestRect,
                        HWND hDestWindowOverride,
                        const RGNDATA *pDirtyRegion,
                        DWORD dwFlags )
{
    struct pipe_resource *res = NULL;
    D3DWindowBuffer *handle_temp;
    struct threadpool_task *task_temp;
    int i;
    HRESULT hr;

    DBG("This=%p pSourceRect=%p pDestRect=%p hDestWindowOverride=%p "
        "pDirtyRegion=%p dwFlags=%d\n",
        This, pSourceRect, pDestRect, hDestWindowOverride,
        pDirtyRegion,dwFlags);

    if (This->base.device->ex) {
        if (NineSwapChain9_GetOccluded(This)) {
            DBG("Present is occluded. Returning S_PRESENT_OCCLUDED.\n");
            return S_PRESENT_OCCLUDED;
        }
    } else {
        if (NineSwapChain9_GetOccluded(This) ||
            NineSwapChain9_ResolutionMismatch(This)) {
            This->base.device->device_needs_reset = TRUE;
        }
        if (This->base.device->device_needs_reset) {
            DBG("Device is lost. Returning D3DERR_DEVICELOST.\n");
            return D3DERR_DEVICELOST;
        }
    }

    nine_csmt_process(This->base.device);

    hr = present(This, pSourceRect, pDestRect,
                 hDestWindowOverride, pDirtyRegion, dwFlags);
    if (hr == D3DERR_WASSTILLDRAWING)
        return hr;

    if (This->base.device->minor_version_num > 2 &&
        This->params.SwapEffect == D3DSWAPEFFECT_DISCARD &&
        This->params.PresentationInterval == D3DPRESENT_INTERVAL_IMMEDIATE &&
        !This->actx->thread_submit) {
        int next_buffer = -1;

        while (next_buffer == -1) {
            /* Find a free backbuffer */
            for (i = 1; i < This->num_back_buffers; i++) {
                if (ID3DPresent_IsBufferReleased(This->present, This->present_handles[i])) {
                    DBG("Found buffer released: %d\n", i);
                    next_buffer = i;
                    break;
                }
            }
            if (next_buffer == -1) {
                DBG("Found no buffer released. Waiting for event\n");
                ID3DPresent_WaitBufferReleaseEvent(This->present);
            }
        }
        /* Switch with the released buffer */
        pipe_resource_reference(&res, This->buffers[0]->base.resource);
        NineSurface9_SetResourceResize(
            This->buffers[0], This->buffers[next_buffer]->base.resource);
        NineSurface9_SetResourceResize(
            This->buffers[next_buffer], res);
        pipe_resource_reference(&res, NULL);

        if (This->present_buffers[0]) {
            pipe_resource_reference(&res, This->present_buffers[0]);
            pipe_resource_reference(&This->present_buffers[0], This->present_buffers[next_buffer]);
            pipe_resource_reference(&This->present_buffers[next_buffer], res);
            pipe_resource_reference(&res, NULL);
        }

        handle_temp = This->present_handles[0];
        This->present_handles[0] = This->present_handles[next_buffer];
        This->present_handles[next_buffer] = handle_temp;

        /* Path not yet compatible with thread_submit */
        assert(!This->tasks[0] && !This->tasks[next_buffer]);
    } else {
        switch (This->params.SwapEffect) {
            case D3DSWAPEFFECT_OVERLAY: /* Not implemented, fallback to FLIP */
            case D3DSWAPEFFECT_FLIPEX: /* Allows optimizations over FLIP for windowed mode. */
            case D3DSWAPEFFECT_DISCARD: /* Allows optimizations over FLIP */
            case D3DSWAPEFFECT_FLIP:
                /* rotate the queue */
                pipe_resource_reference(&res, This->buffers[0]->base.resource);
                for (i = 1; i < This->num_back_buffers; i++) {
                    NineSurface9_SetResourceResize(This->buffers[i - 1],
                                                   This->buffers[i]->base.resource);
                }
                NineSurface9_SetResourceResize(
                    This->buffers[This->num_back_buffers - 1], res);
                pipe_resource_reference(&res, NULL);

                if (This->present_buffers[0]) {
                    pipe_resource_reference(&res, This->present_buffers[0]);
                    for (i = 1; i < This->num_back_buffers; i++)
                        pipe_resource_reference(&(This->present_buffers[i-1]), This->present_buffers[i]);
                    pipe_resource_reference(&(This->present_buffers[This->num_back_buffers - 1]), res);
                    pipe_resource_reference(&res, NULL);
                }

                handle_temp = This->present_handles[0];
                for (i = 1; i < This->num_back_buffers; i++) {
                    This->present_handles[i-1] = This->present_handles[i];
                }
                This->present_handles[This->num_back_buffers - 1] = handle_temp;
                task_temp = This->tasks[0];
                for (i = 1; i < This->num_back_buffers; i++) {
                    This->tasks[i-1] = This->tasks[i];
                }
                This->tasks[This->num_back_buffers - 1] = task_temp;
                break;

            case D3DSWAPEFFECT_COPY:
                /* do nothing */
                break;
        }

        if (This->tasks[0])
            _mesa_threadpool_wait_for_task(This->pool, &(This->tasks[0]));

        ID3DPresent_WaitBufferReleased(This->present, This->present_handles[0]);
    }

    This->base.device->context.changed.group |= NINE_STATE_FB;

    return hr;
}

HRESULT NINE_WINAPI
NineSwapChain9_GetFrontBufferData( struct NineSwapChain9 *This,
                                   IDirect3DSurface9 *pDestSurface )
{
    struct NineSurface9 *dest_surface = NineSurface9(pDestSurface);
    struct NineDevice9 *pDevice = This->base.device;
    unsigned int width, height;
    struct pipe_resource *temp_resource;
    struct NineSurface9 *temp_surface;
    D3DWindowBuffer *temp_handle;
    D3DSURFACE_DESC desc;
    HRESULT hr;

    DBG("GetFrontBufferData: This=%p pDestSurface=%p\n",
        This, pDestSurface);

    user_assert(dest_surface->base.pool == D3DPOOL_SYSTEMMEM, D3DERR_INVALIDCALL);

    width = dest_surface->desc.Width;
    height = dest_surface->desc.Height;

    /* Note: front window size and destination size are supposed
     * to match. However it's not very clear what should get taken in Windowed
     * mode. It may need a fix */
    create_present_buffer(This, width, height, &temp_resource, &temp_handle);

    if (!temp_resource || !temp_handle) {
        return D3DERR_INVALIDCALL;
    }

    desc.Type = D3DRTYPE_SURFACE;
    desc.Pool = D3DPOOL_DEFAULT;
    desc.MultiSampleType = D3DMULTISAMPLE_NONE;
    desc.MultiSampleQuality = 0;
    desc.Width = width;
    desc.Height = height;
    /* NineSurface9_CopyDefaultToMem needs same format. */
    desc.Format = dest_surface->desc.Format;
    desc.Usage = D3DUSAGE_RENDERTARGET;
    hr = NineSurface9_new(pDevice, NineUnknown(This), temp_resource, NULL, 0,
                          0, 0, &desc, &temp_surface);
    pipe_resource_reference(&temp_resource, NULL);
    if (FAILED(hr)) {
        DBG("Failed to create temp FrontBuffer surface.\n");
        return hr;
    }

    ID3DPresent_FrontBufferCopy(This->present, temp_handle);

    NineSurface9_CopyDefaultToMem(dest_surface, temp_surface);

    ID3DPresent_DestroyD3DWindowBuffer(This->present, temp_handle);
    NineUnknown_Destroy(NineUnknown(temp_surface));

    return D3D_OK;
}

HRESULT NINE_WINAPI
NineSwapChain9_GetBackBuffer( struct NineSwapChain9 *This,
                              UINT iBackBuffer,
                              D3DBACKBUFFER_TYPE Type,
                              IDirect3DSurface9 **ppBackBuffer )
{
    DBG("GetBackBuffer: This=%p iBackBuffer=%d Type=%d ppBackBuffer=%p\n",
        This, iBackBuffer, Type, ppBackBuffer);
    (void)user_error(Type == D3DBACKBUFFER_TYPE_MONO);
    /* don't touch ppBackBuffer on error */
    user_assert(ppBackBuffer != NULL, D3DERR_INVALIDCALL);
    user_assert(iBackBuffer < This->params.BackBufferCount, D3DERR_INVALIDCALL);

    NineUnknown_AddRef(NineUnknown(This->buffers[iBackBuffer]));
    *ppBackBuffer = (IDirect3DSurface9 *)This->buffers[iBackBuffer];
    return D3D_OK;
}

HRESULT NINE_WINAPI
NineSwapChain9_GetRasterStatus( struct NineSwapChain9 *This,
                                D3DRASTER_STATUS *pRasterStatus )
{
    DBG("GetRasterStatus: This=%p pRasterStatus=%p\n",
        This, pRasterStatus);
    user_assert(pRasterStatus != NULL, E_POINTER);
    return ID3DPresent_GetRasterStatus(This->present, pRasterStatus);
}

HRESULT NINE_WINAPI
NineSwapChain9_GetDisplayMode( struct NineSwapChain9 *This,
                               D3DDISPLAYMODE *pMode )
{
    D3DDISPLAYMODEEX mode;
    D3DDISPLAYROTATION rot;
    HRESULT hr;

    DBG("GetDisplayMode: This=%p pMode=%p\n",
        This, pMode);
    user_assert(pMode != NULL, E_POINTER);

    hr = ID3DPresent_GetDisplayMode(This->present, &mode, &rot);
    if (SUCCEEDED(hr)) {
        pMode->Width = mode.Width;
        pMode->Height = mode.Height;
        pMode->RefreshRate = mode.RefreshRate;
        pMode->Format = mode.Format;
    }
    return hr;
}

HRESULT NINE_WINAPI
NineSwapChain9_GetPresentParameters( struct NineSwapChain9 *This,
                                     D3DPRESENT_PARAMETERS *pPresentationParameters )
{
    DBG("GetPresentParameters: This=%p pPresentationParameters=%p\n",
        This, pPresentationParameters);
    user_assert(pPresentationParameters != NULL, E_POINTER);
    *pPresentationParameters = This->params;
    return D3D_OK;
}

IDirect3DSwapChain9Vtbl NineSwapChain9_vtable = {
    (void *)NineUnknown_QueryInterface,
    (void *)NineUnknown_AddRef,
    (void *)NineUnknown_Release,
    (void *)NineSwapChain9_Present,
    (void *)NineSwapChain9_GetFrontBufferData,
    (void *)NineSwapChain9_GetBackBuffer,
    (void *)NineSwapChain9_GetRasterStatus,
    (void *)NineSwapChain9_GetDisplayMode,
    (void *)NineUnknown_GetDevice, /* actually part of SwapChain9 iface */
    (void *)NineSwapChain9_GetPresentParameters
};

static const GUID *NineSwapChain9_IIDs[] = {
    &IID_IDirect3DSwapChain9,
    &IID_IUnknown,
    NULL
};

HRESULT
NineSwapChain9_new( struct NineDevice9 *pDevice,
                    BOOL implicit,
                    ID3DPresent *pPresent,
                    D3DPRESENT_PARAMETERS *pPresentationParameters,
                    struct d3dadapter9_context *pCTX,
                    HWND hFocusWindow,
                    struct NineSwapChain9 **ppOut )
{
    NINE_DEVICE_CHILD_NEW(SwapChain9, ppOut, pDevice, /* args */
                          implicit, pPresent, pPresentationParameters,
                          pCTX, hFocusWindow, NULL);
}

BOOL
NineSwapChain9_GetOccluded( struct NineSwapChain9 *This )
{
    if (This->base.device->minor_version_num > 0) {
        return ID3DPresent_GetWindowOccluded(This->present);
    }

    return FALSE;
}

BOOL
NineSwapChain9_ResolutionMismatch( struct NineSwapChain9 *This )
{
    if (This->base.device->minor_version_num > 1) {
        return ID3DPresent_ResolutionMismatch(This->present);
    }

    return FALSE;
}

HANDLE
NineSwapChain9_CreateThread( struct NineSwapChain9 *This,
                                 void *pFuncAddress,
                                 void *pParam )
{
    if (This->base.device->minor_version_num > 1) {
        return ID3DPresent_CreateThread(This->present, pFuncAddress, pParam);
    }

    return NULL;
}

void
NineSwapChain9_WaitForThread( struct NineSwapChain9 *This,
                                  HANDLE thread )
{
    if (This->base.device->minor_version_num > 1) {
        (void) ID3DPresent_WaitForThread(This->present, thread);
    }
}

static int
NineSwapChain9_GetBackBufferCountForParams( struct NineSwapChain9 *This,
                                            D3DPRESENT_PARAMETERS *pParams )
{
    int count = pParams->BackBufferCount;

    /* When we have flip behaviour, d3d9 expects we get back the screen buffer when we flip.
     * Here we don't get back the initial content of the screen. To emulate the behaviour
     * we allocate an additional buffer */
    if (pParams->SwapEffect != D3DSWAPEFFECT_COPY)
        count++;
    /* With DISCARD, as there is no guarantee about the buffer contents, we can use
     * an arbitrary number of buffers */
    if (pParams->SwapEffect == D3DSWAPEFFECT_DISCARD) {
        /* thread_submit has a throttling equivalent to the throttling
         * with throttling_value set to count-1. Most drivers use
         * 2 for throttling_value. For performance use count of at least 3
         * for thread_submit. */
        if (This->actx->thread_submit && count < 3)
            count = 3;
        /* When we enable AllowDISCARDDelayedRelease, we must ensure
         * to have at least 4 buffers to meet INTERVAL_IMMEDIATE,
         * since the display server/compositor can hold 3 buffers
         * without releasing them:
         * . Buffer on screen.
         * . Buffer scheduled kernel side to be next on screen.
         * . Last buffer sent.
         * For some reasons, 5 buffers are actually needed, because in
         * case a pageflip is missed because rendering wasn't finished,
         * the Xserver will hold 4 buffers. */
        if (!This->actx->thread_submit &&
            This->base.device->minor_version_num > 2 &&
            pParams->PresentationInterval == D3DPRESENT_INTERVAL_IMMEDIATE &&
            count < 5)
            count = 5;
    }

    return count;
}
