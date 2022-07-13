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
  <xsl:output method="text" />

  <xsl:template match="/acrn-offline-data">
    <!-- Declaration of license -->
    <xsl:value-of select="$license" />

    <!-- Header include guard -->
    <xsl:value-of select="acrn:include-guard('BOARD_INFO_H')" />

    <xsl:apply-templates select="config-data/acrn-config" />
    <xsl:apply-templates select="board-data/acrn-config" />
    <xsl:apply-templates select="allocation-data/acrn-config" />

    <xsl:value-of select="acrn:include-guard-end('BOARD_INFO_H')" />
  </xsl:template>

  <xsl:template match="config-data/acrn-config">
    <xsl:if test="count(//p2sb[text() = 'y'])">
      <xsl:call-template name="p2sb" />
    </xsl:if>
    <xsl:call-template name="MAX_HIDDEN_PDEVS_NUM" />
  </xsl:template>

  <xsl:template match="board-data/acrn-config">
    <xsl:call-template name="MAX_PCPU_NUM" />
    <xsl:call-template name="MAX_VMSIX_ON_MSI_PDEVS_NUM" />
    <xsl:variable name="physical_address_bits" select="//processors/model/attribute[@id='physical_address_bits']/text()" />
    <xsl:if test="$physical_address_bits">
      <xsl:value-of select="acrn:define('MAXIMUM_PA_WIDTH', $physical_address_bits[1], 'U')" />
    </xsl:if>
  </xsl:template>

  <xsl:template match="allocation-data/acrn-config">
    <xsl:apply-templates select="hv/MMIO" />
  </xsl:template>

  <xsl:template name="p2sb">
    <xsl:value-of select="acrn:define('P2SB_VGPIO_DM_ENABLED', '', '')" />
    <xsl:value-of select="acrn:define('P2SB_BAR_ADDR', '0xFD000000', 'UL')" />
    <xsl:value-of select="acrn:define('P2SB_BAR_ADDR_GPA', '0xFD000000', 'UL')" />
    <xsl:value-of select="acrn:define('P2SB_BAR_SIZE', '0x1000000', 'UL')" />
    <xsl:value-of select="acrn:define('P2SB_BASE_GPIO_PORT_ID', '0x69', 'U')" />
    <xsl:value-of select="acrn:define('P2SB_MAX_GPIO_COMMUNITIES', '0x6', 'U')" />
  </xsl:template>

  <xsl:template name="MAX_HIDDEN_PDEVS_NUM">
    <xsl:value-of select="acrn:define('MAX_HIDDEN_PDEVS_NUM', acrn:get-hidden-device-num(), 'U')" />
  </xsl:template>

  <xsl:template name="MAX_PCPU_NUM">
    <xsl:value-of select="acrn:define('MAX_PCPU_NUM', count(//processors//thread), 'U')" />
  </xsl:template>

  <xsl:template name="MAX_VMSIX_ON_MSI_PDEVS_NUM">
    <xsl:value-of select="acrn:define('MAX_VMSIX_ON_MSI_PDEVS_NUM', sum(dyn:map(//device, 'acrn:is-vmsix-supported-device(./vendor, ./identifier)')), 'U')" />
  </xsl:template>

  <xsl:template match="MMIO">
    <xsl:value-of select="acrn:define('MMIO32_START', //MMIO32_START, 'UL')" />
    <xsl:value-of select="acrn:define('MMIO32_END', //MMIO32_END, 'UL')" />
    <xsl:value-of select="acrn:define('MMIO64_START', //MMIO64_START, 'UL')" />
    <xsl:value-of select="acrn:define('MMIO64_END', //MMIO64_END, 'UL')" />
    <xsl:value-of select="acrn:define('HI_MMIO_START', //HI_MMIO_START, 'UL')" />
    <xsl:value-of select="acrn:define('HI_MMIO_END', //HI_MMIO_END, 'UL')" />
  </xsl:template>

</xsl:stylesheet>
