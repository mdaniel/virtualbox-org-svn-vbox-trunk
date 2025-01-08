#pragma once

#if defined(VBOX) && defined(_WIN32)
#include <iprt/win/d3d9.h>
#else
#include <d3d9.h>
#endif

namespace dxvk {
  using NTSTATUS = LONG;

  // Slightly modified definitions...
  struct D3DKMT_CREATEDCFROMMEMORY {
    void*         pMemory;
    D3DFORMAT     Format;
    UINT          Width;
    UINT          Height;
    UINT          Pitch;
    HDC           hDeviceDc;
    PALETTEENTRY* pColorTable;
    HDC           hDc;
    HANDLE        hBitmap;
  };

  struct D3DKMT_DESTROYDCFROMMEMORY {
    HDC    hDC     = nullptr;
    HANDLE hBitmap = nullptr;
  };

  using D3DKMTCreateDCFromMemoryType  = NTSTATUS(STDMETHODCALLTYPE*) (D3DKMT_CREATEDCFROMMEMORY*);
  NTSTATUS D3DKMTCreateDCFromMemory (D3DKMT_CREATEDCFROMMEMORY*  Arg1);

  using D3DKMTDestroyDCFromMemoryType = NTSTATUS(STDMETHODCALLTYPE*) (D3DKMT_DESTROYDCFROMMEMORY*);
  NTSTATUS D3DKMTDestroyDCFromMemory(D3DKMT_DESTROYDCFROMMEMORY* Arg1);

}