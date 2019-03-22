/* $Id$ */
/** @file
 * OVF reader declarations.
 *
 * Depends only on IPRT, including the RTCString and IPRT XML classes.
 */

/*
 * Copyright (C) 2008-2019 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#define LOG_GROUP LOG_GROUP_MAIN_APPLIANCE
#include "ovfreader.h"
#include <VBox/log.h>
#include <vector>

using namespace std;
using namespace ovf;



////////////////////////////////////////////////////////////////////////////////
//
// OVF reader implementation
//
////////////////////////////////////////////////////////////////////////////////

/**
 * Constructor. This parses the given XML file out of the memory. Throws lots of exceptions
 * on XML or OVF invalidity.
 * @param pvBuf  the memory buffer to parse
 * @param cbSize the size of the memory buffer
 * @param path   path to a filename for error messages.
 */
OVFReader::OVFReader(const void *pvBuf, size_t cbSize, const RTCString &path)
    : m_strPath(path)
{
    xml::XmlMemParser parser;
    parser.read(pvBuf, cbSize,
                m_strPath,
                m_doc);
    /* Start the parsing */
    parse();
}

/**
 * Constructor. This opens the given XML file and parses it. Throws lots of exceptions
 * on XML or OVF invalidity.
 * @param path
 */
OVFReader::OVFReader(const RTCString &path)
    : m_strPath(path)
{
    xml::XmlFileParser parser;
    parser.read(m_strPath,
                m_doc);
    /* Start the parsing */
    parse();
}

void OVFReader::parse()
{
    const xml::ElementNode *pRootElem = m_doc.getRootElement();
    const xml::AttributeNode *pTypeAttr;
    const char *pcszTypeAttr = "";
    RTCString pcszNamespaceURI;

    if (!pRootElem || strcmp(pRootElem->getName(), "Envelope"))
        throw OVFLogicError(N_("Root element in OVF file must be \"Envelope\"."));

    pcszNamespaceURI = pRootElem->getNamespaceURI();
    if(pcszNamespaceURI.isEmpty())
    {
        throw OVFLogicError(N_("Error reading namespace URI in 'Envelope' element, line %d"), pRootElem->getLineNumber());
    }

    if (strncmp(ovf::OVF20_URI_string, pcszNamespaceURI.c_str(), pcszNamespaceURI.length()) == 0)
    {
        m_envelopeData.setOVFVersion(ovf::OVFVersion_2_0);
    }
    else if (strncmp(OVF10_URI_string, pcszNamespaceURI.c_str(), pcszNamespaceURI.length()) == 0)
    {
        m_envelopeData.setOVFVersion(ovf::OVFVersion_1_0);
    }
    else
    {
        m_envelopeData.setOVFVersion(ovf::OVFVersion_0_9);
    }

    if ((pTypeAttr = pRootElem->findAttribute("lang", "xml")))
    {
        pcszTypeAttr = pTypeAttr->getValue();
        m_envelopeData.lang = pcszTypeAttr;
    }

    // OVF has the following rough layout:
    /*
        -- <References> ....  files referenced from other parts of the file, such as VMDK images
        -- Metadata, comprised of several section commands
        -- virtual machines, either a single <VirtualSystem>, or a <VirtualSystemCollection>
        -- optionally <Strings> for localization
    */

    // get all "File" child elements of "References" section so we can look up files easily;
    // first find the "References" sections so we can look up files
    xml::ElementNodesList listFileElements;      // receives all /Envelope/References/File nodes
    const xml::ElementNode *pReferencesElem;
    if ((pReferencesElem = pRootElem->findChildElement("References")))
        pReferencesElem->getChildElements(listFileElements, "File");

    // now go though the sections
    LoopThruSections(pReferencesElem, pRootElem);
}

/**
 * Private helper method that goes thru the elements of the given "current" element in the OVF XML
 * and handles the contained child elements (which can be "Section" or "Content" elements).
 *
 * @param pReferencesElem "References" element from OVF, for looking up file specifications;
 *                        can be NULL if no such element is present.
 * @param pCurElem Element whose children are to be analyzed here.
 * @return
 */
void OVFReader::LoopThruSections(const xml::ElementNode *pReferencesElem,
                                 const xml::ElementNode *pCurElem)
{
    xml::NodesLoop loopChildren(*pCurElem);
    const xml::ElementNode *pElem;
    while ((pElem = loopChildren.forAllNodes()))
    {
        const char *pcszElemName = pElem->getName();
        const xml::AttributeNode *pTypeAttr = pElem->findAttribute("type");
        const char *pcszTypeAttr = pTypeAttr ? pTypeAttr->getValue() : "";

        if (    !strcmp(pcszElemName, "DiskSection")
             || (    !strcmp(pcszElemName, "Section")
                  && !strcmp(pcszTypeAttr, "ovf:DiskSection_Type")
                )
           )
        {
            HandleDiskSection(pReferencesElem, pElem);
        }
        else if (    !strcmp(pcszElemName, "NetworkSection")
                  || (    !strcmp(pcszElemName, "Section")
                       && !strcmp(pcszTypeAttr, "ovf:NetworkSection_Type")
                     )
                )
        {
            HandleNetworkSection(pElem);
        }
        else if (    !strcmp(pcszElemName, "DeploymentOptionSection"))
        {
            /// @todo
        }
        else if (    !strcmp(pcszElemName, "Info"))
        {
            // child of VirtualSystemCollection -- TODO
        }
        else if (    !strcmp(pcszElemName, "ResourceAllocationSection"))
        {
            // child of VirtualSystemCollection -- TODO
        }
        else if (    !strcmp(pcszElemName, "StartupSection"))
        {
            // child of VirtualSystemCollection -- TODO
        }
        else if (    !strcmp(pcszElemName, "VirtualSystem")
                  || (    !strcmp(pcszElemName, "Content")
                       && !strcmp(pcszTypeAttr, "ovf:VirtualSystem_Type")
                     )
                )
        {
            HandleVirtualSystemContent(pElem);
        }
        else if (    !strcmp(pcszElemName, "VirtualSystemCollection")
                  || (    !strcmp(pcszElemName, "Content")
                       && !strcmp(pcszTypeAttr, "ovf:VirtualSystemCollection_Type")
                     )
                )
        {
            /// @todo ResourceAllocationSection

            // recurse for this, since it has VirtualSystem elements as children
            LoopThruSections(pReferencesElem, pElem);
        }
    }
}

/**
 * Private helper method that handles disk sections in the OVF XML.
 *
 * Gets called indirectly from IAppliance::read().
 *
 * @param pReferencesElem   "References" element from OVF, for looking up file
 *                          specifications; can be NULL if no such element is
 *                          present.
 * @param pSectionElem      Section element for which this helper is getting called.
 */
void OVFReader::HandleDiskSection(const xml::ElementNode *pReferencesElem,
                                  const xml::ElementNode *pSectionElem)
{
    // contains "Disk" child elements
    xml::NodesLoop loopDisks(*pSectionElem, "Disk");
    const xml::ElementNode *pelmDisk;
    while ((pelmDisk = loopDisks.forAllNodes()))
    {
        DiskImage d;
        const char *pcszBad = NULL;
        const char *pcszDiskId;
        const char *pcszFormat;
        if (!pelmDisk->getAttributeValue("diskId", pcszDiskId))
            pcszBad = "diskId";
        else if (!pelmDisk->getAttributeValue("format", pcszFormat))
            pcszBad = "format";
        else if (!pelmDisk->getAttributeValue("capacity", d.iCapacity))
            pcszBad = "capacity";
        else
        {
            d.strDiskId = pcszDiskId;
            d.strFormat = pcszFormat;

            if (!pelmDisk->getAttributeValue("populatedSize", d.iPopulatedSize))
                // optional
                d.iPopulatedSize = -1;

            // optional vbox:uuid attribute (if OVF was exported by VirtualBox != 3.2)
            pelmDisk->getAttributeValue("uuid", d.uuidVBox, "vbox");

            const char *pcszFileRef;
            if (pelmDisk->getAttributeValue("fileRef", pcszFileRef)) // optional
            {
                // look up corresponding /References/File nodes (list built above)
                const xml::ElementNode *pFileElem;
                if (    pReferencesElem
                     && (pFileElem = pReferencesElem->findChildElementFromId(pcszFileRef)) != NULL
                   )
                {

                    // copy remaining values from file node then
                    const char *pcszBadInFile = NULL;
                    const char *pcszHref;
                    if (!pFileElem->getAttributeValue("href", pcszHref))
                        pcszBadInFile = "href";
                    else if (!pFileElem->getAttributeValue("size", d.iSize))
                        d.iSize = -1;       // optional

                    d.strHref = pcszHref;

                    // if (!(pFileElem->getAttributeValue("size", d.iChunkSize))) TODO
                    d.iChunkSize = -1;       // optional
                    const char *pcszCompression;
                    if (pFileElem->getAttributeValue("compression", pcszCompression))
                        d.strCompression = pcszCompression;

                    if (pcszBadInFile)
                        throw OVFLogicError(N_("Error reading \"%s\": missing or invalid attribute '%s' in 'File' element, line %d"),
                                            m_strPath.c_str(),
                                            pcszBadInFile,
                                            pFileElem->getLineNumber());
                }
                else
                    throw OVFLogicError(N_("Error reading \"%s\": cannot find References/File element for ID '%s' referenced by 'Disk' element, line %d"),
                                        m_strPath.c_str(),
                                        pcszFileRef,
                                        pelmDisk->getLineNumber());
            }
        }

        if (pcszBad)
            throw OVFLogicError(N_("Error reading \"%s\": missing or invalid attribute '%s' in 'DiskSection' element, line %d"),
                                m_strPath.c_str(),
                                pcszBad,
                                pelmDisk->getLineNumber());

        // suggest a size in megabytes to help callers with progress reports
        d.ulSuggestedSizeMB = 0;
        if (d.iCapacity != -1)
            d.ulSuggestedSizeMB = (uint32_t)(d.iCapacity / _1M);
        else if (d.iPopulatedSize != -1)
            d.ulSuggestedSizeMB = (uint32_t)(d.iPopulatedSize / _1M);
        else if (d.iSize != -1)
            d.ulSuggestedSizeMB = (uint32_t)(d.iSize / _1M);
        if (d.ulSuggestedSizeMB == 0)
            d.ulSuggestedSizeMB = 10000;         // assume 10 GB, this is for the progress bar only anyway

        m_mapDisks[d.strDiskId] = d;
    }
}

/**
 * Private helper method that handles network sections in the OVF XML.
 * Gets called indirectly from IAppliance::read().
 *
 * @return
 */
void OVFReader::HandleNetworkSection(const xml::ElementNode * /* pSectionElem */)
{
    // we ignore network sections for now

//     xml::NodesLoop loopNetworks(*pSectionElem, "Network");
//     const xml::Node *pelmNetwork;
//     while ((pelmNetwork = loopNetworks.forAllNodes()))
//     {
//         Network n;
//         if (!(pelmNetwork->getAttributeValue("name", n.strNetworkName)))
//             return setError(VBOX_E_FILE_ERROR,
//                             tr("Error reading \"%s\": missing 'name' attribute in 'Network', line %d"),
//                             pcszPath,
//                             pelmNetwork->getLineNumber());
//
//         m->mapNetworks[n.strNetworkName] = n;
//     }
}

/**
 * Private helper method that handles a "VirtualSystem" element in the OVF XML.
 * Gets called indirectly from IAppliance::read().
 *
 * @param pelmVirtualSystem
 * @return
 */
void OVFReader::HandleVirtualSystemContent(const xml::ElementNode *pelmVirtualSystem)
{
    /* Create a new virtual system and work directly on the list copy. */
    m_llVirtualSystems.push_back(VirtualSystem());
    VirtualSystem &vsys = m_llVirtualSystems.back();

    // peek under the <VirtualSystem> node whether we have a <vbox:Machine> node;
    // that case case, the caller can completely ignore the OVF but only load the VBox machine XML
    vsys.pelmVBoxMachine = pelmVirtualSystem->findChildElementNS("vbox", "Machine");

    // now look for real OVF
    const xml::AttributeNode *pIdAttr = pelmVirtualSystem->findAttribute("id");
    if (pIdAttr)
        vsys.strName = pIdAttr->getValue();

    xml::NodesLoop loop(*pelmVirtualSystem);      // all child elements
    const xml::ElementNode *pelmThis;
    while ((pelmThis = loop.forAllNodes()))
    {
        const char *pcszElemName = pelmThis->getName();
        const char *pcszTypeAttr = "";
        if (!strcmp(pcszElemName, "Section"))       // OVF 0.9 used "Section" element always with a varying "type" attribute
        {
            const xml::AttributeNode *pTypeAttr = pelmThis->findAttribute("type");
            if (pTypeAttr)
                pcszTypeAttr = pTypeAttr->getValue();
            else
                throw OVFLogicError(N_("Error reading \"%s\": element \"Section\" has no \"type\" attribute, line %d"),
                                    m_strPath.c_str(),
                                    pelmThis->getLineNumber());
        }

        if (    !strcmp(pcszElemName, "EulaSection")
             || !strcmp(pcszTypeAttr, "ovf:EulaSection_Type")
           )
        {
         /* <EulaSection>
                <Info ovf:msgid="6">License agreement for the Virtual System.</Info>
                <License ovf:msgid="1">License terms can go in here.</License>
            </EulaSection> */

            const xml::ElementNode *pelmLicense;
            if ((pelmLicense = pelmThis->findChildElement("License")))
                vsys.strLicenseText = pelmLicense->getValue();
        }
        if (    !strcmp(pcszElemName, "ProductSection")
             || !strcmp(pcszTypeAttr, "ovf:ProductSection_Type")
           )
        {
            /* <Section ovf:required="false" xsi:type="ovf:ProductSection_Type">
                <Info>Meta-information about the installed software</Info>
                <Product>VAtest</Product>
                <Vendor>SUN Microsystems</Vendor>
                <Version>10.0</Version>
                <ProductUrl>http://blogs.sun.com/VirtualGuru</ProductUrl>
                <VendorUrl>http://www.sun.com</VendorUrl>
               </Section> */
            const xml::ElementNode *pelmProduct;
            if ((pelmProduct = pelmThis->findChildElement("Product")))
                vsys.strProduct = pelmProduct->getValue();
            const xml::ElementNode *pelmVendor;
            if ((pelmVendor = pelmThis->findChildElement("Vendor")))
                vsys.strVendor = pelmVendor->getValue();
            const xml::ElementNode *pelmVersion;
            if ((pelmVersion = pelmThis->findChildElement("Version")))
                vsys.strVersion = pelmVersion->getValue();
            const xml::ElementNode *pelmProductUrl;
            if ((pelmProductUrl = pelmThis->findChildElement("ProductUrl")))
                vsys.strProductUrl = pelmProductUrl->getValue();
            const xml::ElementNode *pelmVendorUrl;
            if ((pelmVendorUrl = pelmThis->findChildElement("VendorUrl")))
                vsys.strVendorUrl = pelmVendorUrl->getValue();
        }
        else if (    !strcmp(pcszElemName, "VirtualHardwareSection")
                  || !strcmp(pcszTypeAttr, "ovf:VirtualHardwareSection_Type")
                )
        {
            const xml::ElementNode *pelmSystem, *pelmVirtualSystemType;
            if ((pelmSystem = pelmThis->findChildElement("System")))
            {
             /* <System>
                    <vssd:Description>Description of the virtual hardware section.</vssd:Description>
                    <vssd:ElementName>vmware</vssd:ElementName>
                    <vssd:InstanceID>1</vssd:InstanceID>
                    <vssd:VirtualSystemIdentifier>MyLampService</vssd:VirtualSystemIdentifier>
                    <vssd:VirtualSystemType>vmx-4</vssd:VirtualSystemType>
                </System>*/
                if ((pelmVirtualSystemType = pelmSystem->findChildElement("VirtualSystemType")))
                    vsys.strVirtualSystemType = pelmVirtualSystemType->getValue();
            }

            /* Parse the items into the hardware item vector. */
            {
                std::map<uint32_t, const VirtualHardwareItem *> mapHardwareItems;
                xml::NodesLoop childrenIterator(*pelmThis);
                const xml::ElementNode *pelmItem;
                while ((pelmItem = childrenIterator.forAllNodes()) != NULL)
                {
                    /* Parse according to type. */
                    VirtualHardwareItem *pItem;
                    const char *pszName = pelmItem->getName();
                    if (RTStrCmp(pszName, "Item") == 0)
                        pItem = new VirtualHardwareItem();
                    else if (RTStrCmp(pszName, "StorageItem") == 0)
                        pItem = new StorageItem();
                    else if (RTStrCmp(pszName, "EthernetPortItem") == 0)
                        pItem = new EthernetPortItem();
                    else
                        continue;
                    vsys.vecHardwareItems.push_back(pItem);
                    pItem->ulLineNumber = pelmItem->getLineNumber();
                    pItem->fillItem(pelmItem);

                    /* validate */
                    try
                    {
                        pItem->checkConsistencyAndCompliance();
                    }
                    catch (OVFLogicError &e)
                    {
                        throw OVFLogicError(N_("Error reading \"%s\": \"%s\""), m_strPath.c_str(), e.what());
                    }

                    /* Add to mapping vector (for parent ID lookups) if it has a valid instance ID. */
                    if (pItem->ulInstanceID != 0)
                    {
                        std::map<uint32_t, const VirtualHardwareItem *>::const_iterator itDup;
                        itDup = mapHardwareItems.find(pItem->ulInstanceID);
                        if (itDup == mapHardwareItems.end())
                            mapHardwareItems[pItem->ulInstanceID] = pItem;
                        else
#if 1
                            LogRel(("OVFREADER: Warning reading '%s'! Duplicate ulInstanceID %u on line %u, previous at %u!\n",
                                    m_strPath.c_str(), pItem->ulInstanceID, pItem->ulLineNumber, itDup->second->ulLineNumber));
#else
                            throw OVFLogicError(N_("Error reading '%s': Duplicate ulInstanceID %u on line %u, previous at %u"),
                                                m_strPath.c_str(), pItem->ulInstanceID,
                                                pItem->ulLineNumber, itDup->second->ulLineNumber);
#endif
                    }
                }
            }

            HardDiskController *pPrimaryIDEController = NULL;// will be set once found

            // now go thru all hardware items and handle them according to their type;
            // in this first loop we handle all items _except_ hard disk images,
            // which we'll handle in a second loop below
            HardwareItemVector::const_iterator itH;
            for (itH = vsys.vecHardwareItems.begin(); itH != vsys.vecHardwareItems.end(); ++itH)
            {
                const VirtualHardwareItem &i = **itH;

                // do some analysis
                switch (i.resourceType)
                {
                    case ResourceType_Processor:     // 3
                        /*  <rasd:Caption>1 virtual CPU</rasd:Caption>
                            <rasd:Description>Number of virtual CPUs</rasd:Description>
                            <rasd:ElementName>virtual CPU</rasd:ElementName>
                            <rasd:InstanceID>1</rasd:InstanceID>
                            <rasd:ResourceType>3</rasd:ResourceType>
                            <rasd:VirtualQuantity>1</rasd:VirtualQuantity>*/
                        if (i.ullVirtualQuantity < UINT16_MAX)
                            vsys.cCPUs = (uint16_t)i.ullVirtualQuantity;
                        else
                            throw OVFLogicError(N_("Error reading \"%s\": CPU count %RI64 is larger than %d, line %d"),
                                                m_strPath.c_str(),
                                                i.ullVirtualQuantity,
                                                UINT16_MAX,
                                                i.ulLineNumber);
                        break;

                    case ResourceType_Memory:        // 4
                        if (    i.strAllocationUnits == "MegaBytes"           // found in OVF created by OVF toolkit
                             || i.strAllocationUnits == "MB"                  // found in MS docs
                             || i.strAllocationUnits == "byte * 2^20"         // suggested by OVF spec DSP0243 page 21
                           )
                            vsys.ullMemorySize = i.ullVirtualQuantity * _1M;
                        else
                            throw OVFLogicError(N_("Error reading \"%s\": Invalid allocation unit \"%s\" specified with memory size item, line %d"),
                                                m_strPath.c_str(),
                                                i.strAllocationUnits.c_str(),
                                                i.ulLineNumber);
                        break;

                    case ResourceType_IDEController:          // 5
                    {
                        /*  <Item>
                                <rasd:Caption>ideController0</rasd:Caption>
                                <rasd:Description>IDE Controller</rasd:Description>
                                <rasd:InstanceId>5</rasd:InstanceId>
                                <rasd:ResourceType>5</rasd:ResourceType>
                                <rasd:Address>0</rasd:Address>
                                <rasd:BusNumber>0</rasd:BusNumber>
                            </Item> */
                        HardDiskController hdc;
                        hdc.system = HardDiskController::IDE;
                        hdc.idController = i.ulInstanceID;
                        hdc.strControllerType = i.strResourceSubType;

                        hdc.lAddress = i.lAddress;

                        if (!pPrimaryIDEController)
                            // this is the first IDE controller found: then mark it as "primary"
                            hdc.fPrimary = true;
                        else
                        {
                            // this is the second IDE controller found: If VMware exports two
                            // IDE controllers, it seems that they are given an "Address" of 0
                            // an 1, respectively, so assume address=0 means primary controller
                            if (    pPrimaryIDEController->lAddress == 0
                                 && hdc.lAddress == 1
                               )
                            {
                                pPrimaryIDEController->fPrimary = true;
                                hdc.fPrimary = false;
                            }
                            else if (    pPrimaryIDEController->lAddress == 1
                                      && hdc.lAddress == 0
                                    )
                            {
                                pPrimaryIDEController->fPrimary = false;
                                hdc.fPrimary = false;
                            }
                            else
                                // then we really can't tell, just hope for the best
                                hdc.fPrimary = false;
                        }

                        vsys.mapControllers[i.ulInstanceID] = hdc;
                        if (!pPrimaryIDEController)
                            pPrimaryIDEController = &vsys.mapControllers[i.ulInstanceID];
                        break;
                    }

                    case ResourceType_ParallelSCSIHBA:        // 6       SCSI controller
                    {
                        /*  <Item>
                                <rasd:Caption>SCSI Controller 0 - LSI Logic</rasd:Caption>
                                <rasd:Description>SCI Controller</rasd:Description>
                                <rasd:ElementName>SCSI controller</rasd:ElementName>
                                <rasd:InstanceID>4</rasd:InstanceID>
                                <rasd:ResourceSubType>LsiLogic</rasd:ResourceSubType>
                                <rasd:ResourceType>6</rasd:ResourceType>
                            </Item> */
                        HardDiskController hdc;
                        hdc.system = HardDiskController::SCSI;
                        hdc.idController = i.ulInstanceID;
                        hdc.strControllerType = i.strResourceSubType;

                        vsys.mapControllers[i.ulInstanceID] = hdc;
                        break;
                    }

                    case ResourceType_EthernetAdapter: // 10
                    {
                        /*  <Item>
                            <rasd:Caption>Ethernet adapter on 'Bridged'</rasd:Caption>
                            <rasd:AutomaticAllocation>true</rasd:AutomaticAllocation>
                            <rasd:Connection>Bridged</rasd:Connection>
                            <rasd:InstanceID>6</rasd:InstanceID>
                            <rasd:ResourceType>10</rasd:ResourceType>
                            <rasd:ResourceSubType>E1000</rasd:ResourceSubType>
                            </Item>

                            OVF spec DSP 0243 page 21:
                           "For an Ethernet adapter, this specifies the abstract network connection name
                            for the virtual machine. All Ethernet adapters that specify the same abstract
                            network connection name within an OVF package shall be deployed on the same
                            network. The abstract network connection name shall be listed in the NetworkSection
                            at the outermost envelope level." */

                        // only store the name
                        EthernetAdapter ea;
                        ea.strAdapterType = i.strResourceSubType;
                        ea.strNetworkName = i.strConnection;
                        vsys.llEthernetAdapters.push_back(ea);
                        break;
                    }

                    case ResourceType_FloppyDrive: // 14
                        vsys.fHasFloppyDrive = true;           // we have no additional information
                        break;

                    case ResourceType_CDDrive:       // 15
                        /*  <Item ovf:required="false">
                                <rasd:Caption>cdrom1</rasd:Caption>
                                <rasd:InstanceId>7</rasd:InstanceId>
                                <rasd:ResourceType>15</rasd:ResourceType>
                                <rasd:AutomaticAllocation>true</rasd:AutomaticAllocation>
                                <rasd:Parent>5</rasd:Parent>
                                <rasd:AddressOnParent>0</rasd:AddressOnParent>
                            </Item> */
                            // I tried to see what happens if I set an ISO for the CD-ROM in VMware Workstation,
                            // but then the ovftool dies with "Device backing not supported". So I guess if
                            // VMware can't export ISOs, then we don't need to be able to import them right now.
                        vsys.fHasCdromDrive = true;           // we have no additional information
                        break;

                    case ResourceType_HardDisk: // 17
                        // handled separately in second loop below
                        break;

                    case ResourceType_OtherStorageDevice:        // 20       SATA controller
                    {
                        /* <Item>
                            <rasd:Description>SATA Controller</rasd:Description>
                            <rasd:Caption>sataController0</rasd:Caption>
                            <rasd:InstanceID>4</rasd:InstanceID>
                            <rasd:ResourceType>20</rasd:ResourceType>
                            <rasd:ResourceSubType>AHCI</rasd:ResourceSubType>
                            <rasd:Address>0</rasd:Address>
                            <rasd:BusNumber>0</rasd:BusNumber>
                        </Item> */
                        if (   i.strResourceSubType.compare("AHCI", RTCString::CaseInsensitive) == 0
                            || i.strResourceSubType.compare("vmware.sata.ahci", RTCString::CaseInsensitive) == 0)
                        {
                            HardDiskController hdc;
                            hdc.system = HardDiskController::SATA;
                            hdc.idController = i.ulInstanceID;
                            hdc.strControllerType = i.strResourceSubType;

                            vsys.mapControllers[i.ulInstanceID] = hdc;
                        }
                        else
                            throw OVFLogicError(N_("Error reading \"%s\": Host resource of type \"Other Storage Device (%d)\" is supported with SATA AHCI controllers only, line %d (subtype:%s)"),
                                                m_strPath.c_str(),
                                                ResourceType_OtherStorageDevice,
                                                i.ulLineNumber, i.strResourceSubType.c_str() );
                        break;
                    }

                    case ResourceType_USBController: // 23
                        /*  <Item ovf:required="false">
                                <rasd:Caption>usb</rasd:Caption>
                                <rasd:Description>USB Controller</rasd:Description>
                                <rasd:InstanceId>3</rasd:InstanceId>
                                <rasd:ResourceType>23</rasd:ResourceType>
                                <rasd:Address>0</rasd:Address>
                                <rasd:BusNumber>0</rasd:BusNumber>
                            </Item> */
                        vsys.fHasUsbController = true;           // we have no additional information
                        break;

                    case ResourceType_SoundCard: // 35
                        /*  <Item ovf:required="false">
                                <rasd:Caption>sound</rasd:Caption>
                                <rasd:Description>Sound Card</rasd:Description>
                                <rasd:InstanceId>10</rasd:InstanceId>
                                <rasd:ResourceType>35</rasd:ResourceType>
                                <rasd:ResourceSubType>ensoniq1371</rasd:ResourceSubType>
                                <rasd:AutomaticAllocation>false</rasd:AutomaticAllocation>
                                <rasd:AddressOnParent>3</rasd:AddressOnParent>
                            </Item> */
                        vsys.strSoundCardType = i.strResourceSubType;
                        break;

                    default:
                    {
                        /* If this unknown resource type isn't required, we simply skip it. */
                        if (i.fResourceRequired)
                        {
                            throw OVFLogicError(N_("Error reading \"%s\": Unknown resource type %d in hardware item, line %d"),
                                                m_strPath.c_str(),
                                                i.resourceType,
                                                i.ulLineNumber);
                        }
                    }
                } // end switch
            }

            // now run through the items for a second time, but handle only
            // hard disk images; otherwise the code would fail if a hard
            // disk image appears in the OVF before its hard disk controller
            for (itH = vsys.vecHardwareItems.begin(); itH != vsys.vecHardwareItems.end(); ++itH)
            {
                const VirtualHardwareItem &i = **itH;

                // do some analysis
                switch (i.resourceType)
                {
                    case ResourceType_CDDrive: // 15
                        /*  <Item ovf:required="false">
                                <rasd:Caption>cdrom1</rasd:Caption>
                                <rasd:InstanceId>7</rasd:InstanceId>
                                <rasd:ResourceType>15</rasd:ResourceType>
                                <rasd:AutomaticAllocation>true</rasd:AutomaticAllocation>
                                <rasd:Parent>5</rasd:Parent>
                                <rasd:AddressOnParent>0</rasd:AddressOnParent>
                            </Item> */
                    case ResourceType_HardDisk: // 17
                    {
                        /*  <Item>
                                <rasd:Caption>Harddisk 1</rasd:Caption>
                                <rasd:Description>HD</rasd:Description>
                                <rasd:ElementName>Hard Disk</rasd:ElementName>
                                <rasd:HostResource>ovf://disk/lamp</rasd:HostResource>
                                <rasd:InstanceID>5</rasd:InstanceID>
                                <rasd:Parent>4</rasd:Parent>
                                <rasd:ResourceType>17</rasd:ResourceType>
                            </Item> */

                        // look up the hard disk controller element whose InstanceID equals our Parent;
                        // this is how the connection is specified in OVF
                        ControllersMap::const_iterator it = vsys.mapControllers.find(i.ulParent);
                        if (it == vsys.mapControllers.end())
                            throw OVFLogicError(N_("Error reading \"%s\": Disk item with instance ID %d specifies invalid parent %d, line %d"),
                                                m_strPath.c_str(),
                                                i.ulInstanceID,
                                                i.ulParent,
                                                i.ulLineNumber);

                        VirtualDisk vd;
                        vd.idController = i.ulParent;
                        i.strAddressOnParent.toInt(vd.ulAddressOnParent);
                        // ovf://disk/lamp
                        // 123456789012345
                        if (i.strHostResource.startsWith("ovf://disk/"))
                            vd.strDiskId = i.strHostResource.substr(11);
                        else if (i.strHostResource.startsWith("ovf:/disk/"))
                            vd.strDiskId = i.strHostResource.substr(10);
                        else if (i.strHostResource.startsWith("/disk/"))
                            vd.strDiskId = i.strHostResource.substr(6);

                        //the error may be missed for CD, because CD can be empty
                        if ((vd.strDiskId.isEmpty() || (m_mapDisks.find(vd.strDiskId) == m_mapDisks.end()))
                             && i.resourceType == ResourceType_HardDisk)
                          throw OVFLogicError(N_("Error reading \"%s\": Disk item with instance ID %d specifies invalid host resource \"%s\", line %d"),
                                              m_strPath.c_str(),
                                              i.ulInstanceID,
                                              i.strHostResource.c_str(),
                                              i.ulLineNumber);

                        vsys.mapVirtualDisks[vd.strDiskId] = vd;
                        break;
                    }
                    default:
                        break;
                }
            }
        }
        else if (    !strcmp(pcszElemName, "OperatingSystemSection")
                  || !strcmp(pcszTypeAttr, "ovf:OperatingSystemSection_Type")
                )
        {
            uint64_t cimos64;
            if (!(pelmThis->getAttributeValue("id", cimos64)))
                throw OVFLogicError(N_("Error reading \"%s\": missing or invalid 'ovf:id' attribute in operating system section element, line %d"),
                                    m_strPath.c_str(),
                                    pelmThis->getLineNumber());

            vsys.cimos = (CIMOSType_T)cimos64;
            const xml::ElementNode *pelmCIMOSDescription;
            if ((pelmCIMOSDescription = pelmThis->findChildElement("Description")))
                vsys.strCimosDesc = pelmCIMOSDescription->getValue();

            const xml::ElementNode *pelmVBoxOSType;
            if ((pelmVBoxOSType = pelmThis->findChildElementNS("vbox",            // namespace
                                                               "OSType")))        // element name
                vsys.strTypeVBox = pelmVBoxOSType->getValue();
        }
        else if (    (!strcmp(pcszElemName, "AnnotationSection"))
                  || (!strcmp(pcszTypeAttr, "ovf:AnnotationSection_Type"))
                )
        {
            const xml::ElementNode *pelmAnnotation;
            if ((pelmAnnotation = pelmThis->findChildElement("Annotation")))
                vsys.strDescription = pelmAnnotation->getValue();
        }
    }
}

void VirtualHardwareItem::fillItem(const xml::ElementNode *item)
{
    xml::NodesLoop loopItemChildren(*item);// all child elements
    const xml::ElementNode *pelmItemChild;
    while ((pelmItemChild = loopItemChildren.forAllNodes()))
    {
        const char *pcszItemChildName = pelmItemChild->getName();
        if (!strcmp(pcszItemChildName, "Description"))
            strDescription = pelmItemChild->getValue();
        else if (!strcmp(pcszItemChildName, "Caption"))
            strCaption = pelmItemChild->getValue();
        else if (!strcmp(pcszItemChildName, "ElementName"))
            strElementName = pelmItemChild->getValue();
        else if (   !strcmp(pcszItemChildName, "InstanceID")
                 || !strcmp(pcszItemChildName, "InstanceId") )
            pelmItemChild->copyValue(ulInstanceID);
        else if (!strcmp(pcszItemChildName, "HostResource"))
            strHostResource = pelmItemChild->getValue();
        else if (!strcmp(pcszItemChildName, "ResourceType"))
        {
            uint32_t ulType;
            pelmItemChild->copyValue(ulType);
            resourceType = (ResourceType_T)ulType;
            fResourceRequired = true;
            const char *pcszAttValue;
            if (item->getAttributeValue("required", pcszAttValue))
            {
                if (!strcmp(pcszAttValue, "false"))
                    fResourceRequired = false;
            }
        }
        else if (!strcmp(pcszItemChildName, "OtherResourceType"))
            strOtherResourceType = pelmItemChild->getValue();
        else if (!strcmp(pcszItemChildName, "ResourceSubType"))
            strResourceSubType = pelmItemChild->getValue();
        else if (!strcmp(pcszItemChildName, "AutomaticAllocation"))
            fAutomaticAllocation = (!strcmp(pelmItemChild->getValue(), "true")) ? true : false;
        else if (!strcmp(pcszItemChildName, "AutomaticDeallocation"))
            fAutomaticDeallocation = (!strcmp(pelmItemChild->getValue(), "true")) ? true : false;
        else if (!strcmp(pcszItemChildName, "Parent"))
            pelmItemChild->copyValue(ulParent);
        else if (!strcmp(pcszItemChildName, "Connection"))
            strConnection = pelmItemChild->getValue();
        else if (!strcmp(pcszItemChildName, "Address"))
        {
            strAddress = pelmItemChild->getValue();
            pelmItemChild->copyValue(lAddress);
        }
        else if (!strcmp(pcszItemChildName, "AddressOnParent"))
            strAddressOnParent = pelmItemChild->getValue();
        else if (!strcmp(pcszItemChildName, "AllocationUnits"))
            strAllocationUnits = pelmItemChild->getValue();
        else if (!strcmp(pcszItemChildName, "VirtualQuantity"))
            pelmItemChild->copyValue(ullVirtualQuantity);
        else if (!strcmp(pcszItemChildName, "Reservation"))
            pelmItemChild->copyValue(ullReservation);
        else if (!strcmp(pcszItemChildName, "Limit"))
            pelmItemChild->copyValue(ullLimit);
        else if (!strcmp(pcszItemChildName, "Weight"))
            pelmItemChild->copyValue(ullWeight);
        else if (!strcmp(pcszItemChildName, "ConsumerVisibility"))
            strConsumerVisibility = pelmItemChild->getValue();
        else if (!strcmp(pcszItemChildName, "MappingBehavior"))
            strMappingBehavior = pelmItemChild->getValue();
        else if (!strcmp(pcszItemChildName, "PoolID"))
            strPoolID = pelmItemChild->getValue();
        else if (!strcmp(pcszItemChildName, "BusNumber"))
            pelmItemChild->copyValue(ulBusNumber);
//      else if (pelmItemChild->getPrefix() == NULL
//               || strcmp(pelmItemChild->getPrefix(), "vmw"))
//          throw OVFLogicError(N_("Unknown element \"%s\" under Item element, line %d"),
//                              pcszItemChildName,
//                              ulLineNumber);
    }
}

void VirtualHardwareItem::_checkConsistencyAndCompliance() RT_THROW(OVFLogicError)
{
    RTCString name = getItemName();
    if (resourceType == 0)
        throw OVFLogicError(N_("Empty element ResourceType under %s element, line %d. see DMTF Schema Documentation %s"),
                            name.c_str(), ulLineNumber, DTMF_SPECS_URI);

    /* Don't be too uptight about the ulInstanceID value.  There are OVAs out
       there which have ulInstanceID="%iid% for memory for instance, which is
       no good reason for not being able to process them.  bugref:8997 */
    if (ulInstanceID == 0)
    {
        if (   resourceType == ResourceType_IDEController
            || resourceType == ResourceType_OtherStorageDevice
            || resourceType == ResourceType_ParallelSCSIHBA
            || resourceType == ResourceType_iSCSIHBA //??
            || resourceType == ResourceType_IBHCA )  //??
            throw OVFLogicError(N_("Element InstanceID is absent under %s element, line %d. see DMTF Schema Documentation %s"),
                                name.c_str(), ulLineNumber, DTMF_SPECS_URI);
        else
            LogRel(("OVFREADER: Warning: Ignoring missing or invalid ulInstanceID under element %s, line %u\n",
                    name.c_str(), ulLineNumber));
    }
}

void StorageItem::fillItem(const xml::ElementNode *item)
{
    VirtualHardwareItem::fillItem(item);

    xml::NodesLoop loopItemChildren(*item);// all child elements
    const xml::ElementNode *pelmItemChild;
    while ((pelmItemChild = loopItemChildren.forAllNodes()))
    {
        const char *pcszItemChildName = pelmItemChild->getName();
        if (!strcmp(pcszItemChildName, "HostExtentName"))
            strHostExtentName = pelmItemChild->getValue();
        else if (!strcmp(pcszItemChildName, "OtherHostExtentNameFormat"))
            strOtherHostExtentNameFormat = pelmItemChild->getValue();
        else if (!strcmp(pcszItemChildName, "OtherHostExtentNameNamespace"))
            strOtherHostExtentNameNamespace = pelmItemChild->getValue();
        else if (!strcmp(pcszItemChildName, "VirtualQuantityUnits"))
            strVirtualQuantityUnits = pelmItemChild->getValue();
        else if (!strcmp(pcszItemChildName, "Access"))
        {
            uint32_t temp;
            pelmItemChild->copyValue(temp);
            accessType = (StorageAccessType_T)temp;
        }
        else if (!strcmp(pcszItemChildName, "HostExtentNameFormat"))
        {
        }
        else if (!strcmp(pcszItemChildName, "HostExtentNameNamespace"))
        {
        }
        else if (!strcmp(pcszItemChildName, "HostExtentStartingAddress"))
        {
        }
        else if (!strcmp(pcszItemChildName, "HostResourceBlockSize"))
        {
            int64_t temp;
            pelmItemChild->copyValue(temp);
            hostResourceBlockSize = temp;
        }
        else if (!strcmp(pcszItemChildName, "Limit"))
        {
            int64_t temp;
            pelmItemChild->copyValue(temp);
            limit = temp;
        }
        else if (!strcmp(pcszItemChildName, "Reservation"))
        {
            int64_t temp;
            pelmItemChild->copyValue(temp);
            reservation = temp;
        }
        else if (!strcmp(pcszItemChildName, "VirtualQuantity"))
        {
            int64_t temp;
            pelmItemChild->copyValue(temp);
            virtualQuantity = temp;
        }
        else if (!strcmp(pcszItemChildName, "VirtualResourceBlockSize"))
        {
            int64_t temp;
            pelmItemChild->copyValue(temp);
            virtualResourceBlockSize = temp;
        }
    }
}


void StorageItem::_checkConsistencyAndCompliance() RT_THROW(OVFLogicError)
{
    VirtualHardwareItem::_checkConsistencyAndCompliance();

    RTCString name = getItemName();

    if (accessType == StorageAccessType_Unknown)
    {
        //throw OVFLogicError(N_("Access type is unknown under %s element, line %d"),
        //        name.c_str(), ulLineNumber);
    }

    if (hostResourceBlockSize <= 0 && reservation > 0)
    {
        throw OVFLogicError(N_("Element HostResourceBlockSize is absent under %s element, line %d. "
                               "see DMTF Schema Documentation %s"),
                name.c_str(), ulLineNumber, DTMF_SPECS_URI);
    }

    if (virtualResourceBlockSize <= 0 && virtualQuantity > 0)
    {
        throw OVFLogicError(N_("Element VirtualResourceBlockSize is absent under %s element, line %d. "
                               "see DMTF Schema Documentation %s"),
                name.c_str(), ulLineNumber, DTMF_SPECS_URI);
    }

    if (virtualQuantity > 0 && strVirtualQuantityUnits.isEmpty())
    {
        throw OVFLogicError(N_("Element VirtualQuantityUnits is absent under %s element, line %d. "
                               "see DMTF Schema Documentation %s"),
                name.c_str(), ulLineNumber, DTMF_SPECS_URI);
    }

    if (virtualResourceBlockSize <= 1 &&
        strVirtualQuantityUnits.compare(RTCString("count"), RTCString::CaseInsensitive) == 0
       )
    {
        throw OVFLogicError(N_("Element VirtualQuantityUnits is set to \"count\" "
                               "while VirtualResourceBlockSize is set to 1. "
                               "under %s element, line %d. "
                               "It's needed to change on \"byte\". "
                               "see DMTF Schema Documentation %s"),
                                name.c_str(), ulLineNumber, DTMF_SPECS_URI);
    }
}

void EthernetPortItem::fillItem(const xml::ElementNode *item)
{
    VirtualHardwareItem::fillItem(item);

    xml::NodesLoop loopItemChildren(*item);// all child elements
    const xml::ElementNode *pelmItemChild;
    while ((pelmItemChild = loopItemChildren.forAllNodes()))
    {
    }
}

void EthernetPortItem::_checkConsistencyAndCompliance() RT_THROW(OVFLogicError)
{
    VirtualHardwareItem::_checkConsistencyAndCompliance();
}

////////////////////////////////////////////////////////////////////////////////
//
// Errors
//
////////////////////////////////////////////////////////////////////////////////

OVFLogicError::OVFLogicError(const char *aFormat, ...)
{
    char *pszNewMsg;
    va_list args;
    va_start(args, aFormat);
    RTStrAPrintfV(&pszNewMsg, aFormat, args);
    setWhat(pszNewMsg);
    RTStrFree(pszNewMsg);
    va_end(args);
}
