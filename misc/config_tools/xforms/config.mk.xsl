<?xml version="1.0" encoding="utf-8"?>

<!-- Copyright (C) 2021-2022 Intel Corporation. -->
<!-- SPDX-License-Identifier: BSD-3-Clause -->

<xsl:stylesheet
    version="1.0"
    xmlns:xi="http://www.w3.org/2003/XInclude"
    xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
    xmlns:math="http://exslt.org/math"
    xmlns:exslt="http://exslt.org/common"
    xmlns:acrn="http://projectacrn.org">
  <xsl:include href="lib.xsl" />
  <xsl:output method="text" />

  <xsl:variable name="integer-suffix" select="''" />

  <xi:include href="config_common.xsl" xpointer="xpointer(id('config_common')/*)" />

  <xsl:template match="/acrn-offline-data">
    <xsl:apply-templates select="board-data/acrn-config" />
    <xsl:apply-templates select="config-data/acrn-config" />
  </xsl:template>

  <xsl:template name="entry-by-key-value">
    <xsl:param name="prefix" />
    <xsl:param name="key" />
    <xsl:param name="value" />
    <xsl:param name="default" />

    <xsl:choose>
      <xsl:when test="$prefix != ''">
        <xsl:value-of select="$prefix" />
      </xsl:when>
      <xsl:otherwise>
        <xsl:text>CONFIG_</xsl:text>
      </xsl:otherwise>
    </xsl:choose>
    <xsl:value-of select="$key" />
    <xsl:text>=</xsl:text>
    <xsl:choose>
      <xsl:when test="$value != ''">
	<xsl:value-of select="$value" />
      </xsl:when>
      <xsl:when test="$default != ''">
	<xsl:value-of select="$default" />
      </xsl:when>
    </xsl:choose>
    <xsl:value-of select="$newline" />
  </xsl:template>

  <xsl:template name="boolean-by-key-value">
    <xsl:param name="key" />
    <xsl:param name="value" />

    <xsl:choose>
      <xsl:when test="($value = 'true') or ($value = 'y')">
	<xsl:call-template name="entry-by-key-value">
	  <xsl:with-param name="key" select="$key" />
	  <xsl:with-param name="value" select="'y'" />
        </xsl:call-template>
      </xsl:when>
      <xsl:when test="($key = 'RELEASE')">
	<xsl:call-template name="entry-by-key-value">
	  <xsl:with-param name="key" select="$key" />
	  <xsl:with-param name="value" select="'n'" />
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
