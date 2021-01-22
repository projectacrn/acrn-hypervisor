<?xml version="1.0" encoding="utf-8"?>

<!-- Copyright (C) 2021 Intel Corporation. All rights reserved. -->
<!-- SPDX-License-Identifier: BSD-3-Clause -->

<xsl:stylesheet
    version="1.0"
    xmlns:xi="http://www.w3.org/2003/XInclude"
    xmlns:xsl="http://www.w3.org/1999/XSL/Transform">
  <xsl:output method="text" />

  <xsl:variable name="integer-suffix" select="''" />

  <xi:include href="config_common.xsl" xpointer="xpointer(id('config_common')/*)" />

  <xsl:template match="/acrn-offline-data">
    <xsl:apply-templates select="board-data/acrn-config" />
    <xsl:apply-templates select="config-data/acrn-config" />
  </xsl:template>

  <xsl:template name="entry-by-key-value">
    <xsl:param name="key" />
    <xsl:param name="value" />
    <xsl:param name="default" />

    <xsl:text>CONFIG_</xsl:text>
    <xsl:value-of select="$key" />
    <xsl:text>=</xsl:text>
    <xsl:choose>
      <xsl:when test="$value != ''">
	<xsl:value-of select="$value" />
	<xsl:text>&#xa;</xsl:text>
      </xsl:when>
      <xsl:when test="($value = '') and ($default != '')">
	<xsl:value-of select="$default" />
	<xsl:text>&#xa;</xsl:text>
      </xsl:when>
    </xsl:choose>
  </xsl:template>

  <xsl:template name="boolean-by-key-value">
    <xsl:param name="key" />
    <xsl:param name="value" />

    <xsl:choose>
      <xsl:when test="($value = 'y') or ($key = 'RELEASE')">
	<xsl:call-template name="entry-by-key-value">
	  <xsl:with-param name="key" select="$key" />
	  <xsl:with-param name="value" select="$value" />
	</xsl:call-template>
      </xsl:when>
      <xsl:otherwise>
	<xsl:text># CONFIG_</xsl:text>
	<xsl:value-of select="$key" />
	<xsl:text> is not set&#xa;</xsl:text>
      </xsl:otherwise>
    </xsl:choose>
  </xsl:template>

  <xsl:template name="string-by-key-value">
    <xsl:param name="key" />
    <xsl:param name="value" />

    <xsl:call-template name="entry-by-key-value">
      <xsl:with-param name="key" select="$key" />
      <xsl:with-param name="value" select="$value" />
    </xsl:call-template>
  </xsl:template>

</xsl:stylesheet>
