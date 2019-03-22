/* $Id$ */
/** @file
 * Gallium D3D testcase. Various D3D helpers.
 */

/*
 * Copyright (C) 2017-2019 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef __D3DHLP__H
#define __D3DHLP__H
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#ifdef VBOX
#include <iprt/win/d3d9.h>
#else
#include <d3d9.h>
#endif

#define D3D_RELEASE(ptr) do { \
    if (ptr)                  \
    {                         \
        (ptr)->Release();     \
        (ptr) = 0;            \
    }                         \
} while (0)

#define GA_W_OFFSET_OF(_Type, _Member) ( (WORD)(uintptr_t) &( ((_Type *)0)->_Member ) )

#ifdef D3DTEST_STANDALONE
inline void D3DTestShowError(HRESULT hr, const char *pszString)
{
    (void)hr;
    MessageBoxA(0, pszString, 0, 0);
}
#else
#define D3DTestShowError(_hr, _s) do { } while(0)
#endif

/* Expand __LINE__ number to string. */
#define D3DTEST_S(n) #n
#define D3DTEST_N2S(n) D3DTEST_S(n)

#define GaAssertHR(_hr) do { if (FAILED(_hr)) D3DTestShowError((_hr), __FILE__ "@" D3DTEST_N2S(__LINE__)); } while(0)

#define HTEST(a) do { \
    hr = a;           \
    GaAssertHR(hr);   \
} while (0)

/*
 * D3D vector and matrix math helpers.
 */
void d3dMatrixTranspose(D3DMATRIX *pM);
void d3dMatrixIdentity(D3DMATRIX *pM);
void d3dMatrixScaleTranslation(D3DMATRIX *pM, const float s, const float dx, const float dy, const float dz);
void d3dMatrixRotationAxis(D3DMATRIX *pM, const D3DVECTOR *pV, float angle);
void d3dMatrixView(D3DMATRIX *pM, const D3DVECTOR *pR, const D3DVECTOR *pU, const D3DVECTOR *pL, const D3DVECTOR *pP);
void d3dMatrixPerspectiveProjection(D3DMATRIX *pM, float verticalFoV, float aspectRatio, float zNear, float zFar);
void d3dMatrixMultiply(D3DMATRIX *pM, const D3DMATRIX *pM1, const D3DMATRIX *pM2);
void d3dVectorMatrixMultiply(D3DVECTOR *pR, const D3DVECTOR *pV, float w, const D3DMATRIX *pM);
void d3dVectorNormalize(D3DVECTOR *pV);
void d3dVectorCross(D3DVECTOR *pC, const D3DVECTOR *pV1, const D3DVECTOR *pV2);
float d3dVectorDot(const D3DVECTOR *pV1, const D3DVECTOR *pV2);
void d3dVectorInit(D3DVECTOR *pV, float x, float y, float z);

/*
 * Helper to compute view and projection matrices for a camera.
 */
class D3DCamera
{
    public:
        D3DCamera();

        const D3DMATRIX *ViewProjection(void);

        void SetupAt(const D3DVECTOR *pPos, const D3DVECTOR *pAt, const D3DVECTOR *pUp);
        void SetProjection(float verticalFoV, float aspectRatio, float zNear, float zFar);

        void TimeAdvance(float dt);

    private:
        void computeView(void);

        /* Camera location in the world space. */
        D3DVECTOR mPosition;
        D3DVECTOR mRight;
        D3DVECTOR mUp;
        D3DVECTOR mLook;

        D3DMATRIX mView;
        D3DMATRIX mProjection;
        D3DMATRIX mViewProjection;

        float mTime;
};

HRESULT d3dCreateCubeTexture(IDirect3DDevice9 *pDevice, IDirect3DCubeTexture9 **ppCubeTexture);
HRESULT d3dCreateCubeVertexBuffer(IDirect3DDevice9 *pDevice, float EdgeLength, IDirect3DVertexBuffer9 **ppVertexBuffer);

#endif
