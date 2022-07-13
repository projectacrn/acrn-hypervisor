<?xml version="1.0" encoding="utf-8"?>

<!-- Copyright (C) 2021-2022 Intel Corporation. -->
<!-- SPDX-License-Identifier: BSD-3-Clause -->

<xsl:stylesheet
    version="1.0"
    xmlns:xi="http://www.w3.org/2003/XInclude"
    xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
    xmlns:dyn="http://exslt.org/dynamic"
    xmlns:math="http://exslt.org/math"
    xmlns:acrn="http://projectacrn.org">
  <xsl:include href="lib.xsl" />
  <xsl:output method="text" indent="yes" />

  <xsl:template match="/acrn-offline-data">
     <!-- Declaration of license -->
    <xsl:value-of select="$license" />

    <!-- Included headers -->
    <xsl:value-of select="acrn:include('asm/vm_config.h')" />

    <xsl:apply-templates select="config-data/acrn-config/vm" />
  </xsl:template>

  <xsl:template name="format_remappings">
    <xsl:param name="pText" />
    <xsl:param name="index" />
    <xsl:if test="string-length($pText)">
      <xsl:variable name="tokens" select="substring-before(concat($pText,')'),')')" />
      <xsl:variable name="phys_gsi" select="substring-after(substring-before($tokens, ','), '(')" />
      <xsl:variable name="virt_gsi" select="substring-after($tokens, ',')" />
      <xsl:text>[</xsl:text>
      <xsl:value-of select="concat($index, 'U')" />
      <xsl:text>] = {</xsl:text>
      <xsl:value-of select="$newline" />
      <xsl:value-of select="acrn:initializer('phys_gsi', concat($phys_gsi, 'U'))" />
      <xsl:value-of select="acrn:initializer('virt_gsi', concat($virt_gsi, 'U'))" />
      <xsl:text>},</xsl:text>
      <xsl:value-of select="$newline" />
    </xsl:if>
  </xsl:template>

  <xsl:template match="config-data/acrn-config/vm">
    <xsl:if test="acrn:is-pre-launched-vm(load_order)">
      <xsl:variable name="vm_id" select="@id" />
      <xsl:variable name="pt_intx" select="acrn:get-intx-mapping(//vm[@id=$vm_id]//pt_intx)" />
      <xsl:variable name="length" select="count($pt_intx)" />

      <xsl:choose>
	<xsl:when test="$length">
	  <xsl:value-of select="acrn:array-initializer('struct pt_intx_config', concat('vm', @id, '_pt_intx'), concat($length, 'U'))" />
	  <xsl:for-each select="$pt_intx">
	    <xsl:call-template name="format_remappings">
              <xsl:with-param name="pText" select="." />
              <xsl:with-param name="index" select="position() - 1" />
	    </xsl:call-template>
	  </xsl:for-each>
	  <xsl:value-of select="$end_of_array_initializer" />
	</xsl:when>
	<xsl:otherwise>
	  <xsl:value-of select="acrn:array-initializer('struct pt_intx_config', concat('vm', @id, '_pt_intx'), '1U')" />
	  <xsl:value-of select="$end_of_array_initializer" />
	</xsl:otherwise>
      </xsl:choose>
      <xsl:value-of select="$newline" />
    </xsl:if>
  </xsl:template>

</xsl:stylesheet>
