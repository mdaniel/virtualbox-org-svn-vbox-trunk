/* $Id$ */
/** @file
 * VBoxNetUDP - IntNet Client Library.
 */

/*
 * Copyright (C) 2009-2016 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___VBoxNetBaseService_h___
#define ___VBoxNetBaseService_h___

#include <iprt/critsect.h>


class VBoxNetHlpUDPService
{
public:
virtual int                 hlpUDPBroadcast(unsigned uSrcPort, unsigned uDstPort,
                                        void const *pvData, size_t cbData) const = 0;
};


class VBoxNetLockee
{
public:
    virtual int  syncEnter() = 0;
    virtual int  syncLeave() = 0;
};


class VBoxNetALock
{
public:
    VBoxNetALock(VBoxNetLockee *a_lck):m_lck(a_lck)
    {
        if (m_lck)
            m_lck->syncEnter();
    }

    ~VBoxNetALock()
    {
        if (m_lck)
            m_lck->syncLeave();
    }

private:
    VBoxNetLockee *m_lck;
};

# ifndef BASE_SERVICES_ONLY
class VBoxNetBaseService: public VBoxNetHlpUDPService, public VBoxNetLockee
{
public:
    VBoxNetBaseService(const std::string& aName, const std::string& aNetworkName);
    virtual ~VBoxNetBaseService();
    int                 parseArgs(int argc, char **argv);
    int                 tryGoOnline(void);
    void                shutdown(void);
    int                 syncEnter();
    int                 syncLeave();
    int                 waitForIntNetEvent(int cMillis);
    int                 abortWait();
    int                 sendBufferOnWire(PCINTNETSEG paSegs, size_t cSegs, size_t cbBuffer);
    void                flushWire();

    virtual int         hlpUDPBroadcast(unsigned uSrcPort, unsigned uDstPort,
                                        void const *pvData, size_t cbData) const;
    virtual void        usage(void) = 0;
    virtual int         parseOpt(int rc, const RTGETOPTUNION& getOptVal) = 0;
    virtual int         processFrame(void *, size_t) = 0;
    virtual int         processGSO(PCPDMNETWORKGSO, size_t) = 0;
    virtual int         processUDP(void *, size_t) = 0;


    virtual int         init(void);
    virtual int         run(void);
    virtual bool        isMainNeeded() const;

protected:
    const std::string getServiceName() const;
    void setServiceName(const std::string&);

    const std::string getNetworkName() const;
    void setNetworkName(const std::string&);

    const RTMAC getMacAddress() const;
    void setMacAddress(const RTMAC&);

    const RTNETADDRIPV4 getIpv4Address() const;
    void setIpv4Address(const RTNETADDRIPV4&);

    const RTNETADDRIPV4 getIpv4Netmask() const;
    void setIpv4Netmask(const RTNETADDRIPV4&);

    uint32_t getSendBufSize() const;
    void setSendBufSize(uint32_t);

    uint32_t getRecvBufSize() const;
    void setRecvBufSize(uint32_t);

    int32_t getVerbosityLevel() const;
    void setVerbosityLevel(int32_t);

    void addCommandLineOption(const PRTGETOPTDEF);

    /**
     * Print debug message depending on the m_cVerbosity level.
     *
     * @param   iMinLevel       The minimum m_cVerbosity level for this message.
     * @param   fMsg            Whether to dump parts for the current DHCP message.
     * @param   pszFmt          The message format string.
     * @param   ...             Optional arguments.
     */
    void debugPrint(int32_t iMinLevel, bool fMsg, const char *pszFmt, ...) const;
    virtual void debugPrintV(int32_t iMinLevel, bool fMsg, const char *pszFmt, va_list va) const;

    private:
    void doReceiveLoop();

    /** starts receiving thread and enter event polling loop. */
    int startReceiveThreadAndEnterEventLoop();

    protected:
    /* VirtualBox instance */
    ComPtr<IVirtualBox> virtualbox;
    ComPtr<IVirtualBoxClient> virtualboxClient;

    private:
    struct Data;
    Data *m;

    private:
    PRTGETOPTDEF getOptionsPtr();
};
# endif
#endif
