/* $Id$ */
/** @file
 * VBoxBugReport - VirtualBox command-line diagnostics tool, main file.
 */

/*
 * Copyright (C) 2006-2016 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */


#include <VBox/com/com.h>
#include <VBox/com/string.h>
#include <VBox/com/array.h>
//#include <VBox/com/Guid.h>
#include <VBox/com/ErrorInfo.h>
#include <VBox/com/errorprint.h>
#include <VBox/com/VirtualBox.h>

#include <VBox/version.h>

#include <iprt/buildconfig.h>
#include <iprt/env.h>
#include <iprt/file.h>
#include <iprt/getopt.h>
#include <iprt/initterm.h>
#include <iprt/path.h>
#include <iprt/process.h>
#include <iprt/zip.h>
#include <iprt/cpp/exception.h>

#include <list>

#include "VBoxBugReport.h"

/* Implementation - Base */

#ifndef RT_OS_WINDOWS
/** @todo Replace with platform-specific implementations. */
void createBugReportOsSpecific(BugReport *pReport, const char *pszHome)
{
    RT_NOREF(pReport, pszHome);
}
#endif /* !RT_OS_WINDOWS */


/* Globals */

static char *g_pszVBoxManage = NULL;

static const RTGETOPTDEF g_aOptions[] =
{
    { "-all",     'A', RTGETOPT_REQ_NOTHING },
    { "--all",    'A', RTGETOPT_REQ_NOTHING },
    { "-output",  'o', RTGETOPT_REQ_STRING },
    { "--output", 'o', RTGETOPT_REQ_STRING },
    { "-text",    't', RTGETOPT_REQ_NOTHING },
    { "--text",   't', RTGETOPT_REQ_NOTHING }
};

static const char g_szUsage[] =
    "Usage: %s [-h|-?|--help] [-A|--all|<vmname>...] [-o <file>|--output=<file>]\n"
    "   Several VM names can be specified at once to be included into single report.\n"
    "   If none is given then no machines will be included. Specifying -A overrides\n"
    "   any VM names provided and includes all registered machines.\n"
    "Options:\n"
    "   -h, -help,    --help     Print usage information\n"
    "   -A, -all,     --all      Include all registered machines\n"
    "   -o, -output,  --output   Specifies the name of the output file\n"
    "   -t, -text,    --text     Produce a single text file instead of compressed TAR\n"
    "   -V, -version, --version  Print version information\n"
    "\n";


/*
 * This class stores machine-specific file paths that are obtained via
 * VirtualBox API. In case API is not functioning properly these paths
 * will be deduced on the best effort basis.
 */
class MachineInfo
{
public:
    MachineInfo(const char *name, const char *logFolder, const char *settingsFile);
    ~MachineInfo();
    const char *getName() const { return m_name; };
    const char *getLogPath() const { return m_logpath; };
    const char *getSettingsFile() const { return m_settings; };
private:
    char *m_name;
    char *m_logpath;
    char *m_settings;
};

MachineInfo::MachineInfo(const char *name, const char *logFolder, const char *settingsFile)
{
    m_name = RTStrDup(name);
    m_logpath = RTStrDup(logFolder);
    m_settings = RTStrDup(settingsFile);
}

MachineInfo::~MachineInfo()
{
    RTStrFree(m_logpath);
    RTStrFree(m_name);
    RTStrFree(m_settings);
    m_logpath = m_name = m_settings = 0;
}

typedef std::list<MachineInfo*> MachineInfoList;


class VBRDir
{
public:
    VBRDir(const char *pcszPath)
        {
            int rc = RTDirOpenFiltered(&m_pDir, pcszPath, RTDIRFILTER_WINNT, 0);
            if (RT_FAILURE(rc))
                throw RTCError(com::Utf8StrFmt("Failed to open directory '%s'\n", pcszPath));
        };
    ~VBRDir()
        {
            int rc = RTDirClose(m_pDir);
            AssertRC(rc);
        };
    const char *next(void)
        {
            int rc = RTDirRead(m_pDir, &m_DirEntry, NULL);
            if (RT_SUCCESS(rc))
                return m_DirEntry.szName;
            else if (rc == VERR_NO_MORE_FILES)
                return NULL;
            throw RTCError("Failed to read directory element\n");
        };

private:
    PRTDIR m_pDir;
    RTDIRENTRY m_DirEntry;
};


BugReportFilter::BugReportFilter() : m_pvBuffer(0), m_cbBuffer(0)
{
}

BugReportFilter::~BugReportFilter()
{
    if (m_pvBuffer)
        RTMemFree(m_pvBuffer);
}

void *BugReportFilter::allocateBuffer(size_t cbNeeded)
{
    if (m_pvBuffer)
    {
        if (cbNeeded > m_cbBuffer)
            RTMemFree(m_pvBuffer);
        else
            return m_pvBuffer;
    }
    m_pvBuffer = RTMemAlloc(cbNeeded);
    if (!m_pvBuffer)
        throw RTCError(com::Utf8StrFmt("Failed to allocate %ld bytes\n", cbNeeded));
    m_cbBuffer = cbNeeded;
    return m_pvBuffer;
}


/*
 * An abstract class serving as the root of the bug report item tree.
 */
BugReportItem::BugReportItem(const char *pszTitle)
{
    m_pszTitle = RTStrDup(pszTitle);
    m_filter = 0;
}

BugReportItem::~BugReportItem()
{
    if (m_filter)
        delete m_filter;
    RTStrFree(m_pszTitle);
}

void BugReportItem::addFilter(BugReportFilter *filter)
{
    m_filter = filter;
}

void *BugReportItem::applyFilter(void *pvSource, size_t *pcbInOut)
{
    if (m_filter)
        return m_filter->apply(pvSource, pcbInOut);
    return pvSource;
}

const char * BugReportItem::getTitle(void)
{
    return m_pszTitle;
}


BugReport::BugReport(const char *pszFileName)
{
    m_pszFileName = RTStrDup(pszFileName);
}

BugReport::~BugReport()
{
    for (unsigned i = 0; i < m_Items.size(); ++i)
    {
        delete m_Items[i];
    }
    RTStrFree(m_pszFileName);
}

int BugReport::getItemCount(void)
{
    return (int)m_Items.size();
}

void BugReport::addItem(BugReportItem* item, BugReportFilter *filter)
{
    if (filter)
        item->addFilter(filter);
    if (item)
        m_Items.append(item);
}

void BugReport::process(void)
{
    for (unsigned i = 0; i < m_Items.size(); ++i)
    {
        BugReportItem *pItem = m_Items[i];
        RTPrintf("%3u%% - collecting %s...\n", i * 100 / m_Items.size(), pItem->getTitle());
        processItem(pItem);
    }
    RTPrintf("100%% - compressing...\n\n");
}

void *BugReport::applyFilters(BugReportItem* item, void *pvSource, size_t *pcbInOut)
{
    return item->applyFilter(pvSource, pcbInOut);
}


BugReportStream::BugReportStream(const char *pszTitle) : BugReportItem(pszTitle)
{
    handleRtError(RTPathTemp(m_szFileName, RTPATH_MAX),
                  "Failed to obtain path to temporary folder");
    handleRtError(RTPathAppend(m_szFileName, RTPATH_MAX, "BugRepXXXXX.tmp"),
                  "Failed to append path");
    handleRtError(RTFileCreateTemp(m_szFileName, 0600),
                  "Failed to create temporary file '%s'", m_szFileName);
    handleRtError(RTStrmOpen(m_szFileName, "w", &m_Strm),
                  "Failed to open '%s'", m_szFileName);
}

BugReportStream::~BugReportStream()
{
    if (m_Strm)
        RTStrmClose(m_Strm);
    RTFileDelete(m_szFileName);
}

int BugReportStream::printf(const char *pszFmt, ...)
{
    va_list va;
    va_start(va, pszFmt);
    int cb = RTStrmPrintfV(m_Strm, pszFmt, va);
    va_end(va);
    return cb;
}

int BugReportStream::putStr(const char *pszString)
{
    return RTStrmPutStr(m_Strm, pszString);
}

PRTSTREAM BugReportStream::getStream(void)
{
    RTStrmClose(m_Strm);
    handleRtError(RTStrmOpen(m_szFileName, "r", &m_Strm),
                  "Failed to open '%s'", m_szFileName);
    return m_Strm;
}


/* Implementation - Generic */

BugReportFile::BugReportFile(const char *pszPath, const char *pszShortName) : BugReportItem(pszShortName)
{
    m_Strm = 0;
    m_pszPath = RTStrDup(pszPath);
}

BugReportFile::~BugReportFile()
{
    if (m_Strm)
        RTStrmClose(m_Strm);
    if (m_pszPath)
        RTStrFree(m_pszPath);
}

PRTSTREAM BugReportFile::getStream(void)
{
    handleRtError(RTStrmOpen(m_pszPath, "rb", &m_Strm),
                  "Failed to open '%s'", m_pszPath);
    return m_Strm;
}


BugReportCommand::BugReportCommand(const char *pszTitle, const char *pszExec, ...)
    : BugReportItem(pszTitle), m_Strm(NULL)
{
    unsigned cArgs = 0;
    m_papszArgs[cArgs++] = RTStrDup(pszExec);

    const char *pszArg;
    va_list va;
    va_start(va, pszExec);
    do
    {
        if (cArgs >= RT_ELEMENTS(m_papszArgs))
        {
            va_end(va);
            throw RTCError(com::Utf8StrFmt("Too many arguments (%u > %u)\n", cArgs+1, RT_ELEMENTS(m_papszArgs)));
        }
        pszArg = va_arg(va, const char *);
        m_papszArgs[cArgs++] = pszArg ? RTStrDup(pszArg) : NULL;
    } while (pszArg);
    va_end(va);
}

BugReportCommand::~BugReportCommand()
{
    if (m_Strm)
        RTStrmClose(m_Strm);
    RTFileDelete(m_szFileName);
    for (size_t i = 0; i < RT_ELEMENTS(m_papszArgs) && m_papszArgs[i]; ++i)
        RTStrFree(m_papszArgs[i]);
}

PRTSTREAM BugReportCommand::getStream(void)
{
    handleRtError(RTPathTemp(m_szFileName, RTPATH_MAX),
                  "Failed to obtain path to temporary folder");
    handleRtError(RTPathAppend(m_szFileName, RTPATH_MAX, "BugRepXXXXX.tmp"),
                  "Failed to append path");
    handleRtError(RTFileCreateTemp(m_szFileName, 0600),
                  "Failed to create temporary file '%s'", m_szFileName);

    RTHANDLE hStdOutErr;
    hStdOutErr.enmType = RTHANDLETYPE_FILE;
    handleRtError(RTFileOpen(&hStdOutErr.u.hFile, m_szFileName,
                             RTFILE_O_WRITE | RTFILE_O_CREATE_REPLACE | RTFILE_O_DENY_WRITE),
                  "Failed to open temporary file '%s'", m_szFileName);

    RTPROCESS hProcess;
    handleRtError(RTProcCreateEx(m_papszArgs[0], m_papszArgs, RTENV_DEFAULT, 0,
                                 NULL, &hStdOutErr, &hStdOutErr,
                                 NULL, NULL, &hProcess),
                  "Failed to create process '%s'", m_papszArgs[0]);
    RTPROCSTATUS status;
    handleRtError(RTProcWait(hProcess, RTPROCWAIT_FLAGS_BLOCK, &status),
                  "Process wait failed");
    //if (status.enmReason == RTPROCEXITREASON_NORMAL) {}
    RTFileClose(hStdOutErr.u.hFile);

    handleRtError(RTStrmOpen(m_szFileName, "r", &m_Strm),
                  "Failed to open '%s'", m_szFileName);
    return m_Strm;
}


BugReportCommandTemp::BugReportCommandTemp(const char *pszTitle, const char *pszExec, ...)
    : BugReportItem(pszTitle), m_Strm(NULL)
{
    handleRtError(RTPathTemp(m_szFileName, RTPATH_MAX),
                  "Failed to obtain path to temporary folder");
    handleRtError(RTPathAppend(m_szFileName, RTPATH_MAX, "BugRepXXXXX.tmp"),
                  "Failed to append path");
    handleRtError(RTFileCreateTemp(m_szFileName, 0600),
                  "Failed to create temporary file '%s'", m_szFileName);

    unsigned cArgs = 0;
    m_papszArgs[cArgs++] = RTStrDup(pszExec);

    const char *pszArg;
    va_list va;
    va_start(va, pszExec);
    do
    {
        if (cArgs >= RT_ELEMENTS(m_papszArgs) - 1)
        {
            va_end(va);
            throw RTCError(com::Utf8StrFmt("Too many arguments (%u > %u)\n", cArgs+1, RT_ELEMENTS(m_papszArgs)));
        }
        pszArg = va_arg(va, const char *);
        m_papszArgs[cArgs++] = RTStrDup(pszArg ? pszArg : m_szFileName);
    } while (pszArg);
    va_end(va);

    m_papszArgs[cArgs++] = NULL;
}

BugReportCommandTemp::~BugReportCommandTemp()
{
    if (m_Strm)
        RTStrmClose(m_Strm);
    RTFileDelete(m_szErrFileName);
    RTFileDelete(m_szFileName);
    for (size_t i = 0; i < RT_ELEMENTS(m_papszArgs) && m_papszArgs[i]; ++i)
        RTStrFree(m_papszArgs[i]);
}

PRTSTREAM BugReportCommandTemp::getStream(void)
{
    handleRtError(RTPathTemp(m_szErrFileName, RTPATH_MAX),
                  "Failed to obtain path to temporary folder");
    handleRtError(RTPathAppend(m_szErrFileName, RTPATH_MAX, "BugRepErrXXXXX.tmp"),
                  "Failed to append path");
    handleRtError(RTFileCreateTemp(m_szErrFileName, 0600),
                  "Failed to create temporary file '%s'", m_szErrFileName);

    RTHANDLE hStdOutErr;
    hStdOutErr.enmType = RTHANDLETYPE_FILE;
    handleRtError(RTFileOpen(&hStdOutErr.u.hFile, m_szErrFileName,
                             RTFILE_O_WRITE | RTFILE_O_CREATE_REPLACE | RTFILE_O_DENY_WRITE),
                  "Failed to open temporary file '%s'", m_szErrFileName);

    /* Remove the output file to prevent errors or confirmation prompts */
    handleRtError(RTFileDelete(m_szFileName),
                  "Failed to delete temporary file '%s'", m_szFileName);

    RTPROCESS hProcess;
    handleRtError(RTProcCreateEx(m_papszArgs[0], m_papszArgs, RTENV_DEFAULT, 0,
                                 NULL, &hStdOutErr, &hStdOutErr,
                                 NULL, NULL, &hProcess),
                  "Failed to create process '%s'", m_papszArgs[0]);
    RTPROCSTATUS status;
    handleRtError(RTProcWait(hProcess, RTPROCWAIT_FLAGS_BLOCK, &status),
                  "Process wait failed");
    RTFileClose(hStdOutErr.u.hFile);

    if (status.enmReason == RTPROCEXITREASON_NORMAL && status.iStatus == 0)
        handleRtError(RTStrmOpen(m_szFileName, "r", &m_Strm), "Failed to open '%s'", m_szFileName);
    else
        handleRtError(RTStrmOpen(m_szErrFileName, "r", &m_Strm), "Failed to open '%s'", m_szErrFileName);
    return m_Strm;
}


BugReportText::BugReportText(const char *pszFileName) : BugReport(pszFileName)
{
    handleRtError(RTStrmOpen(pszFileName, "w", &m_StrmTxt),
                  "Failed to open '%s'", pszFileName);
}

BugReportText::~BugReportText()
{
    if (m_StrmTxt)
        RTStrmClose(m_StrmTxt);
}

void BugReportText::processItem(BugReportItem* item)
{
    int cb = RTStrmPrintf(m_StrmTxt, "[ %s ] -------------------------------------------\n", item->getTitle());
    if (!cb)
        throw RTCError(com::Utf8StrFmt("Write failure (cb=%d)\n", cb));

    PRTSTREAM strmIn = NULL;
    try
    {
        strmIn = item->getStream();
    }
    catch (RTCError &e)
    {
        strmIn = NULL;
        RTStrmPutStr(m_StrmTxt, e.what());
    }

    int rc = VINF_SUCCESS;

    if (strmIn)
    {
        char buf[64*1024];
        size_t cbRead, cbWritten;
        cbRead = cbWritten = 0;
        while (RT_SUCCESS(rc = RTStrmReadEx(strmIn, buf, sizeof(buf), &cbRead)) && cbRead)
        {
            rc = RTStrmWriteEx(m_StrmTxt, applyFilters(item, buf, &cbRead), cbRead, &cbWritten);
            if (RT_FAILURE(rc) || cbRead != cbWritten)
                throw RTCError(com::Utf8StrFmt("Write failure (rc=%d, cbRead=%lu, cbWritten=%lu)\n",
                                               rc, cbRead, cbWritten));
        }
    }

    handleRtError(RTStrmPutCh(m_StrmTxt, '\n'), "Write failure");
}


BugReportTarGzip::BugReportTarGzip(const char *pszFileName)
    : BugReport(pszFileName), m_hTar(NIL_RTTAR), m_hTarFile(NIL_RTTARFILE)
{
    VfsIoStreamHandle hVfsOut;
    handleRtError(RTVfsIoStrmOpenNormal(pszFileName, RTFILE_O_WRITE | RTFILE_O_CREATE | RTFILE_O_DENY_WRITE,
                                        hVfsOut.getPtr()),
                  "Failed to create output file '%s'", pszFileName);
    handleRtError(RTZipGzipCompressIoStream(hVfsOut.get(), 0, 6, m_hVfsGzip.getPtr()),
                  "Failed to create compressed stream for '%s'", pszFileName);

    handleRtError(RTPathTemp(m_szTarName, RTPATH_MAX),
                  "Failed to obtain path to temporary folder");
    handleRtError(RTPathAppend(m_szTarName, RTPATH_MAX, "BugRepXXXXX.tar"),
                  "Failed to append path");
    handleRtError(RTFileCreateTemp(m_szTarName, 0600),
                  "Failed to create temporary file '%s'", m_szTarName);
    handleRtError(RTFileDelete(m_szTarName),
                  "Failed to delete temporary file '%s'", m_szTarName);
    handleRtError(RTTarOpen(&m_hTar, m_szTarName,  RTFILE_O_CREATE | RTFILE_O_WRITE | RTFILE_O_DENY_ALL),
                  "Failed to create TAR file '%s'", m_szTarName);

}

BugReportTarGzip::~BugReportTarGzip()
{
    if (m_hTarFile != NIL_RTTARFILE)
        RTTarFileClose(m_hTarFile);
    if (m_hTar != NIL_RTTAR)
        RTTarClose(m_hTar);
}

void BugReportTarGzip::processItem(BugReportItem* item)
{
    /*
     * @todo Our TAR implementation does not support names larger than 100 characters.
     * We truncate the title to make sure it will fit into 100-character field of TAR header.
     */
    RTCString strTarFile = RTCString(item->getTitle()).substr(0, RTStrNLen(item->getTitle(), 99));
    handleRtError(RTTarFileOpen(m_hTar, &m_hTarFile, strTarFile.c_str(),
                                RTFILE_O_CREATE | RTFILE_O_WRITE | RTFILE_O_DENY_NONE),
                  "Failed to open '%s' in TAR", strTarFile.c_str());

    PRTSTREAM strmIn = NULL;
    try
    {
        strmIn = item->getStream();
    }
    catch (RTCError &e)
    {
        strmIn = NULL;
        handleRtError(RTTarFileWriteAt(m_hTarFile, 0, e.what(), RTStrNLen(e.what(), 1024), NULL),
                      "Failed to write %u bytes to TAR", RTStrNLen(e.what(), 1024));
    }

    int rc = VINF_SUCCESS;

    if (strmIn)
    {
        char buf[64*1024];
        size_t cbRead = 0;
        for (uint64_t offset = 0;
             RT_SUCCESS(rc = RTStrmReadEx(strmIn, buf, sizeof(buf), &cbRead)) && cbRead;
             offset += cbRead)
        {
            handleRtError(RTTarFileWriteAt(m_hTarFile, offset, applyFilters(item, buf, &cbRead), cbRead, NULL),
                          "Failed to write %u bytes to TAR", cbRead);
        }
    }

    if (m_hTarFile)
    {
        handleRtError(RTTarFileClose(m_hTarFile), "Failed to close '%s' in TAR", strTarFile.c_str());
        m_hTarFile = NIL_RTTARFILE;
    }
}

void BugReportTarGzip::complete(void)
{
    if (m_hTarFile != NIL_RTTARFILE)
    {
        RTTarFileClose(m_hTarFile);
        m_hTarFile = NIL_RTTARFILE;
    }
    if (m_hTar != NIL_RTTAR)
    {
        RTTarClose(m_hTar);
        m_hTar = NIL_RTTAR;
    }

    VfsIoStreamHandle hVfsIn;
    handleRtError(RTVfsIoStrmOpenNormal(m_szTarName, RTFILE_O_READ | RTFILE_O_OPEN | RTFILE_O_DENY_NONE,
                                        hVfsIn.getPtr()),
                  "Failed to open TAR file '%s'", m_szTarName);

    int rc;
    char buf[_64K];
    size_t cbRead = 0;
    while (RT_SUCCESS(rc = RTVfsIoStrmRead(hVfsIn.get(), buf, sizeof(buf), true, &cbRead)) && cbRead)
        handleRtError(RTVfsIoStrmWrite(m_hVfsGzip.get(), buf, cbRead, true, NULL),
                      "Failed to write into compressed stream");
    handleRtError(rc, "Failed to read from TAR stream");
    handleRtError(RTVfsIoStrmFlush(m_hVfsGzip.get()), "Failed to flush output stream");
    m_hVfsGzip.release();
}


/* Implementation - Main */

void createBugReport(BugReport* report, const char *pszHome, MachineInfoList& machines)
{
    /* Collect all log files from VBoxSVC */
    VBRDir HomeDir(PathJoin(pszHome, "VBoxSVC.log*"));
    const char *pcszSvcLogFile = HomeDir.next();
    while (pcszSvcLogFile)
    {
        report->addItem(new BugReportFile(PathJoin(pszHome, pcszSvcLogFile), pcszSvcLogFile));
        pcszSvcLogFile = HomeDir.next();
    }

    report->addItem(new BugReportFile(PathJoin(pszHome, "VirtualBox.xml"), "VirtualBox.xml"));
    report->addItem(new BugReportCommand("HostUsbDevices", g_pszVBoxManage, "list", "usbhost", NULL));
    report->addItem(new BugReportCommand("HostUsbFilters", g_pszVBoxManage, "list", "usbfilters", NULL));
    for (MachineInfoList::iterator it = machines.begin(); it != machines.end(); ++it)
    {
        VBRDir VmDir(PathJoin((*it)->getLogPath(), "VBox.log*"));
        const char *pcszVmLogFile = VmDir.next();
        while (pcszVmLogFile)
        {
            report->addItem(new BugReportFile(PathJoin((*it)->getLogPath(), pcszVmLogFile),
                                              PathJoin((*it)->getName(), pcszVmLogFile)));
            pcszVmLogFile = VmDir.next();
        }
        report->addItem(new BugReportFile((*it)->getSettingsFile(),
                                         PathJoin((*it)->getName(), RTPathFilename((*it)->getSettingsFile()))));
        report->addItem(new BugReportCommand(PathJoin((*it)->getName(), "GuestProperties"),
                                            g_pszVBoxManage, "guestproperty", "enumerate",
                                            (*it)->getName(), NULL));
    }

    createBugReportOsSpecific(report, pszHome);
}

void addMachine(MachineInfoList& list, ComPtr<IMachine> machine)
{
    com::Bstr name, logFolder, settingsFile;
    handleComError(machine->COMGETTER(Name)(name.asOutParam()),
                   "Failed to get VM name");
    handleComError(machine->COMGETTER(LogFolder)(logFolder.asOutParam()),
                   "Failed to get VM log folder");
    handleComError(machine->COMGETTER(SettingsFilePath)(settingsFile.asOutParam()),
                   "Failed to get VM settings file path");
    list.push_back(new MachineInfo(com::Utf8Str(name).c_str(),
                                   com::Utf8Str(logFolder).c_str(),
                                   com::Utf8Str(settingsFile).c_str()));
}


static void printHeader(void)
{
    RTStrmPrintf(g_pStdErr, VBOX_PRODUCT " Bug Report Tool " VBOX_VERSION_STRING "\n"
                 "(C) " VBOX_C_YEAR " " VBOX_VENDOR "\n"
                 "All rights reserved.\n\n");
}

int main(int argc, char *argv[])
{
    /*
     * Initialize the VBox runtime without loading
     * the support driver.
     */
    RTR3InitExe(argc, &argv, 0);

    bool fAllMachines = false;
    bool fTextOutput  = false;
    const char *pszOutputFile = NULL;
    std::list<const char *> nameList;
    RTGETOPTUNION ValueUnion;
    RTGETOPTSTATE GetState;
    int ret = RTGetOptInit(&GetState, argc, argv,
                          g_aOptions, RT_ELEMENTS(g_aOptions),
                          1 /* First */, 0 /*fFlags*/);
    if (RT_FAILURE(ret))
        return ret;
    int ch;
    while ((ch = RTGetOpt(&GetState, &ValueUnion)))
    {
        switch(ch)
        {
            case 'h':
                printHeader();
                RTStrmPrintf(g_pStdErr, g_szUsage, argv[0]);
                return 0;
            case 'A':
                fAllMachines = true;
                break;
            case 'o':
                pszOutputFile = ValueUnion.psz;
                break;
            case 't':
                fTextOutput = true;
                break;
            case 'V':
                RTPrintf("%sr%s\n", RTBldCfgVersion(), RTBldCfgRevisionStr());
                return 0;
            case VINF_GETOPT_NOT_OPTION:
                nameList.push_back(ValueUnion.psz);
                break;
            default:
                return RTGetOptPrintError(ch, &ValueUnion);
        }
    }

    printHeader();

    HRESULT hr = S_OK;
    char homeDir[RTPATH_MAX];
    com::GetVBoxUserHomeDirectory(homeDir, sizeof(homeDir));

    try
    {
        /* Figure out the full path to VBoxManage */
        char szVBoxBin[RTPATH_MAX];
        if (!RTProcGetExecutablePath(szVBoxBin, sizeof(szVBoxBin)))
            throw RTCError("RTProcGetExecutablePath failed\n");
        RTPathStripFilename(szVBoxBin);
        g_pszVBoxManage = RTPathJoinA(szVBoxBin, VBOXMANAGE);
        if (!g_pszVBoxManage)
            throw RTCError("Out of memory\n");

        handleComError(com::Initialize(), "Failed to initialize COM");

        MachineInfoList list;

        do
        {
            ComPtr<IVirtualBoxClient> virtualBoxClient;
            ComPtr<IVirtualBox> virtualBox;
            ComPtr<ISession> session;

            hr = virtualBoxClient.createLocalObject(CLSID_VirtualBoxClient);
            if (SUCCEEDED(hr))
                hr = virtualBoxClient->COMGETTER(VirtualBox)(virtualBox.asOutParam());
            else
                hr = virtualBox.createLocalObject(CLSID_VirtualBox);
            if (FAILED(hr))
                RTStrmPrintf(g_pStdErr, "WARNING: Failed to create the VirtualBox object (hr=0x%x)\n", hr);
            else
            {
                hr = session.createInprocObject(CLSID_Session);
                if (FAILED(hr))
                    RTStrmPrintf(g_pStdErr, "WARNING: Failed to create a session object (hr=0x%x)\n", hr);
            }

            if (SUCCEEDED(hr))
            {
                if (fAllMachines)
                {
                    com::SafeIfaceArray<IMachine> machines;
                    hr = virtualBox->COMGETTER(Machines)(ComSafeArrayAsOutParam(machines));
                    if (SUCCEEDED(hr))
                    {
                        for (size_t i = 0; i < machines.size(); ++i)
                        {
                            if (machines[i])
                                addMachine(list, machines[i]);
                        }
                    }
                }
                else
                {
                    for ( std::list<const char *>::iterator it = nameList.begin(); it != nameList.end(); ++it)
                    {
                        ComPtr<IMachine> machine;
                        handleComError(virtualBox->FindMachine(com::Bstr(*it).raw(), machine.asOutParam()),
                                       "No such machine '%s'", *it);
                        addMachine(list, machine);
                    }
                }
            }

        }
        while(0);

        RTTIMESPEC  TimeSpec;
        RTTIME      Time;
        RTTimeExplode(&Time, RTTimeNow(&TimeSpec));
        RTCStringFmt strOutFile("%04d-%02d-%02d-%02d-%02d-%02d-bugreport.%s",
                                Time.i32Year, Time.u8Month, Time.u8MonthDay,
                                Time.u8Hour, Time.u8Minute, Time.u8Second,
                                fTextOutput ? "txt" : "tgz");
        RTCString strFallbackOutFile;
        if (!pszOutputFile)
        {
            RTFILE tmp;
            pszOutputFile = strOutFile.c_str();
            int rc = RTFileOpen(&tmp, pszOutputFile, RTFILE_O_WRITE | RTFILE_O_CREATE | RTFILE_O_DENY_WRITE);
            if (rc == VERR_ACCESS_DENIED)
            {
                char szUserHome[RTPATH_MAX];
                handleRtError(RTPathUserHome(szUserHome, sizeof(szUserHome)), "Failed to obtain home directory");
                strFallbackOutFile.printf("%s/%s", szUserHome, strOutFile.c_str());
                pszOutputFile = strFallbackOutFile.c_str();
            }
            else if (RT_SUCCESS(rc))
            {
                RTFileClose(tmp);
                RTFileDelete(pszOutputFile);
            }
        }
        BugReport *pReport;
        if (fTextOutput)
            pReport = new BugReportText(pszOutputFile);
        else
            pReport = new BugReportTarGzip(pszOutputFile);
        createBugReport(pReport, homeDir, list);
        pReport->process();
        pReport->complete();
        RTPrintf("Report was written to '%s'\n", pszOutputFile);
        delete pReport;
    }
    catch (RTCError &e)
    {
        RTStrmPrintf(g_pStdErr, "ERROR: %s\n", e.what());
    }

    com::Shutdown();

    if (g_pszVBoxManage)
        RTStrFree(g_pszVBoxManage);

    return SUCCEEDED(hr) ? 0 : 1;
}
