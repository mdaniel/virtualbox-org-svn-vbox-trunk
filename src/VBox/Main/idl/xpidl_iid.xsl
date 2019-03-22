<?xml version="1.0"?>
<!-- $Id$ -->

<!--
 *  A template to generate a header file containing IIDs for XPCOM
 *  from the generic interface definition expressed in XML.

    Copyright (C) 2006-2016 Oracle Corporation

    This file is part of VirtualBox Open Source Edition (OSE), as
    available from http://www.virtualbox.org. This file is free software;
    you can redistribute it and/or modify it under the terms of the GNU
    General Public License (GPL) as published by the Free Software
    Foundation, in version 2 as it comes in the "COPYING" file of the
    VirtualBox OSE distribution. VirtualBox OSE is distributed in the
    hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
-->

<xsl:stylesheet version="1.0" xmlns:xsl="http://www.w3.org/1999/XSL/Transform">
<xsl:output method="text"/>

<xsl:strip-space elements="*"/>

<!--
//  templates
/////////////////////////////////////////////////////////////////////////////
-->


<!--
 *  not explicitly matched elements and attributes
-->
<xsl:template match="*"/>


<!--
 *  header
-->
<xsl:template match="/idl">
  <xsl:text>
/*
 *  DO NOT EDIT! This is a generated file.
 *
 *  XPCOM C definitions for VirtualBox Main API (IIDs for COM interfaces)
 *  generated from XIDL (XML interface definition).
 *
 *  Source    : src/VBox/Main/idl/VirtualBox.xidl
 *  Generator : src/VBox/Main/idl/xpidl_iid.xsl
 */

#ifndef nsID_h__
struct nsID
{
    unsigned int m0;
    unsigned short m1;
    unsigned short m2;
    unsigned char m3[8];
};

typedef struct nsID nsID;
typedef struct nsID nsIID;
typedef struct nsID nsCID;
#endif /* nsID_h__ */

#ifdef __cplusplus
extern "C" {
#endif

</xsl:text>
  <xsl:apply-templates/>
  <xsl:text>

#ifdef __cplusplus
}
#endif

</xsl:text>
</xsl:template>


<!--
 *  ignore all |if|s except those for XPIDL target
-->
<xsl:template match="if">
  <xsl:if test="@target='xpidl'">
    <xsl:apply-templates/>
  </xsl:if>
</xsl:template>


<!--
 *  libraries
-->
<xsl:template match="library">
  <xsl:apply-templates select="application/if | application/interface"/>
  <xsl:apply-templates select="application/module"/>
</xsl:template>


<!--
 *  interfaces
-->
<xsl:template match="interface">
  <xsl:text>const nsID IID_</xsl:text>
  <xsl:value-of select="@name"/>
  <xsl:text> = {&#x0A;</xsl:text>
  <xsl:text>    0x</xsl:text><xsl:value-of select="substring(@uuid,1,8)"/>
  <xsl:text>, 0x</xsl:text><xsl:value-of select="substring(@uuid,10,4)"/>
  <xsl:text>, 0x</xsl:text><xsl:value-of select="substring(@uuid,15,4)"/>
  <xsl:text>, \&#x0A;    </xsl:text>
  <xsl:text>{ 0x</xsl:text><xsl:value-of select="substring(@uuid,20,2)"/>
  <xsl:text>, 0x</xsl:text><xsl:value-of select="substring(@uuid,22,2)"/>
  <xsl:text>, 0x</xsl:text><xsl:value-of select="substring(@uuid,25,2)"/>
  <xsl:text>, 0x</xsl:text><xsl:value-of select="substring(@uuid,27,2)"/>
  <xsl:text>, 0x</xsl:text><xsl:value-of select="substring(@uuid,29,2)"/>
  <xsl:text>, 0x</xsl:text><xsl:value-of select="substring(@uuid,31,2)"/>
  <xsl:text>, 0x</xsl:text><xsl:value-of select="substring(@uuid,33,2)"/>
  <xsl:text>, 0x</xsl:text><xsl:value-of select="substring(@uuid,35,2)"/>
  <xsl:text> } \&#x0A;};&#x0A;&#x0A;</xsl:text>
</xsl:template>


<!--
 *  modules
-->
<xsl:template match="module">
  <xsl:apply-templates select="class"/>
</xsl:template>


<!--
 *  co-classes
-->
<xsl:template match="module/class">
  <xsl:text>const nsCID CLSID_</xsl:text>
  <xsl:value-of select="@name"/>
  <xsl:text> = {&#x0A;</xsl:text>
  <xsl:text>    0x</xsl:text><xsl:value-of select="substring(@uuid,1,8)"/>
  <xsl:text>, 0x</xsl:text><xsl:value-of select="substring(@uuid,10,4)"/>
  <xsl:text>, 0x</xsl:text><xsl:value-of select="substring(@uuid,15,4)"/>
  <xsl:text>, \&#x0A;    </xsl:text>
  <xsl:text>{ 0x</xsl:text><xsl:value-of select="substring(@uuid,20,2)"/>
  <xsl:text>, 0x</xsl:text><xsl:value-of select="substring(@uuid,22,2)"/>
  <xsl:text>, 0x</xsl:text><xsl:value-of select="substring(@uuid,25,2)"/>
  <xsl:text>, 0x</xsl:text><xsl:value-of select="substring(@uuid,27,2)"/>
  <xsl:text>, 0x</xsl:text><xsl:value-of select="substring(@uuid,29,2)"/>
  <xsl:text>, 0x</xsl:text><xsl:value-of select="substring(@uuid,31,2)"/>
  <xsl:text>, 0x</xsl:text><xsl:value-of select="substring(@uuid,33,2)"/>
  <xsl:text>, 0x</xsl:text><xsl:value-of select="substring(@uuid,35,2)"/>
  <xsl:text> } \&#x0A;};&#x0A;&#x0A;</xsl:text>
</xsl:template>


<xsl:template match="application[@uuid='ec0e78e8-fa43-43e8-ac0a-02c784c4a4fa']//module/class" />

<xsl:template match="application[@uuid='ec0e78e8-fa43-43e8-ac0a-02c784c4a4fa']/if//interface
| application[@uuid='ec0e78e8-fa43-43e8-ac0a-02c784c4a4fa']//interface" />

</xsl:stylesheet>

