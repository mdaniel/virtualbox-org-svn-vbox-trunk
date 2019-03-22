/* $Id$ */
/** @file
 * VirtualBox COM class implementation - MediumIO.
 */

/*
 * Copyright (C) 2018 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ____H_MEDIUMIOIMPL
#define ____H_MEDIUMIOIMPL

#include "MediumIOWrap.h"
#include "VirtualBoxBase.h"
#include "AutoCaller.h"

class ATL_NO_VTABLE MediumIO :
    public MediumIOWrap
{
public:
    /** @name Dummy/standard constructors and destructors.
     * @{ */
    DECLARE_EMPTY_CTOR_DTOR(MediumIO)
    HRESULT FinalConstruct();
    void    FinalRelease();
    /** @} */

    /** @name Initializer & uninitializer.
     * @{ */
    HRESULT initForMedium(Medium *pMedium, VirtualBox *pVirtualBox, bool fWritable,
                          com::Utf8Str const &rStrKeyId, com::Utf8Str const &rStrPassword);
    void    uninit();
    /** @} */

private:
    /** @name Wrapped IMediumIO properties
     * @{ */
    HRESULT getMedium(ComPtr<IMedium> &a_rPtrMedium);
    HRESULT getWritable(BOOL *a_fWritable);
    HRESULT getExplorer(ComPtr<IVFSExplorer> &a_rPtrExplorer);
    /** @} */

    /** @name Wrapped IMediumIO methods
     * @{ */
    HRESULT read(LONG64 a_off, ULONG a_cbRead, std::vector<BYTE> &a_rData);
    HRESULT write(LONG64 a_off, const std::vector<BYTE> &a_rData, ULONG *a_pcbWritten);
    HRESULT formatFAT(BOOL a_fQuick);
    HRESULT initializePartitionTable(PartitionTableType_T a_enmFormat, BOOL a_fWholeDiskInOneEntry);
    HRESULT convertToStream(const com::Utf8Str &aFormat,
                            const std::vector<MediumVariant_T> &aVariant,
                            ULONG aBufferSize,
                            ComPtr<IDataStream> &aStream,
                            ComPtr<IProgress> &aProgress);
    HRESULT close();
    /** @} */

    /** @name Internal workers.
     *  @{ */
    void    i_close();
    /** @} */

    struct Data;
    Data *m;

    class StreamTask;
    friend class StreamTask;
};

#endif
/* vi: set tabstop=4 shiftwidth=4 expandtab: */

