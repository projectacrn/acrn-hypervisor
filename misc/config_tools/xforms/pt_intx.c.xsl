<?xml version="1.0" encoding="utf-8"?>

<!-- Copyright (C) 2021 Intel Corporation. All rights reserved. -->
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
    <xsl:value-of select="acrn:include('x86/vm_config.h')" />

    <xsl:apply-templates select="config-data/acrn-config" />
  </xsl:template>

  <xsl:template match="config-data/acrn-config">
    <xsl:choose>
      <xsl:when test="(vm/pt_intx) and (normalize-space(vm/pt_intx) != '')">
        <xsl:value-of select="$newline" />
        <xsl:apply-templates select="vm/pt_intx" />
      </xsl:when>
      <xsl:otherwise>
        <xsl:text>struct pt_intx_config vm0_pt_intx[1U];</xsl:text>
        <xsl:value-of select="$newline" />
      </xsl:otherwise>
    </xsl:choose>
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
      <xsl:call-template name="format_remappings">
        <xsl:with-param name="pText" select="substring-after($pText, ')')" />
        <xsl:with-param name="index" select="$index + 1" />
      </xsl:call-template>
    </xsl:if>
  </xsl:template>

  <xsl:template match="vm/pt_intx">
    <xsl:variable name="length" select="string-length(current()) - string-length(translate(current(), ',', ''))" />
    <xsl:if test="$length">
      <xsl:value-of select="acrn:array-initializer('struct pt_intx_config', 'vm0_pt_intx', concat($length, 'U'))" />
      <xsl:variable name="pText" select="normalize-space(current())" />
      <xsl:variable name="index" select="'0'" />
      <xsl:call-template name="format_remappings">
        <xsl:with-param name="pText" select="$pText" />
        <xsl:with-param name="index" select="$index" />
      </xsl:call-template>
      <xsl:value-of select="$end_of_array_initializer" />
    </xsl:if>
    <xsl:value-of select="$newline" />
  </xsl:template>

</xsl:stylesheet>
