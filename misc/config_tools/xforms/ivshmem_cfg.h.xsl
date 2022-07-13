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
    <xsl:value-of select="acrn:include-guard('IVSHMEM_CFG_H')" />

    <xsl:apply-templates select="config-data/acrn-config" />

    <xsl:value-of select="acrn:include-guard-end('IVSHMEM_CFG_H')" />
  </xsl:template>

  <xsl:template match="config-data/acrn-config">
    <xsl:if test="count(//hv//IVSHMEM/IVSHMEM_REGION[PROVIDED_BY = 'Hypervisor']) > 0">
      <!-- Included headers -->
      <xsl:value-of select="acrn:include('ivshmem.h')" />
      <xsl:value-of select="acrn:include('asm/pgtable.h')" />

      <xsl:call-template name="ivshmem_shm_region_name" />
      <xsl:call-template name="ivshmem_shm_mem" />
      <xsl:apply-templates select="hv//IVSHMEM" />
      <xsl:call-template name="ivshmem_shm_region" />
    </xsl:if>
  </xsl:template>

  <xsl:template name ="ivshmem_shm_region_name">
    <xsl:for-each select="hv//IVSHMEM/IVSHMEM_REGION[PROVIDED_BY = 'Hypervisor']">
      <xsl:value-of select="acrn:define(concat('IVSHMEM_SHM_REGION_', position() - 1), concat($quot, 'hv:/', NAME, $quot), '')" />
    </xsl:for-each>
    <xsl:value-of select="$newline" />
  </xsl:template>

  <xsl:template name ="ivshmem_shm_mem">
    <xsl:value-of select="acrn:comment('The IVSHMEM_SHM_SIZE is the sum of all memory regions. The size range of each memory region is [2MB, 512MB] and is a power of 2.')" />
    <xsl:value-of select="$newline" />
    <xsl:variable name="memsize" select="sum(//IVSHMEM_REGION[PROVIDED_BY = 'Hypervisor']/IVSHMEM_SIZE)" />
    <xsl:value-of select="acrn:define('IVSHMEM_SHM_SIZE', concat('0x', acrn:convert-num-base(string($memsize), 10, 16), '00000UL'))" />
  </xsl:template>

  <xsl:template match="IVSHMEM">
    <xsl:variable name="dev" select="count(//hv//IVSHMEM/IVSHMEM_REGION[PROVIDED_BY = 'Hypervisor']/IVSHMEM_VMS/IVSHMEM_VM)" />
    <xsl:value-of select="acrn:define('IVSHMEM_DEV_NUM', string($dev), 'UL')" />
  </xsl:template>

  <xsl:template name ="ivshmem_shm_region">
    <xsl:value-of select="acrn:comment('All user defined memory regions')" />
    <xsl:value-of select="$newline" />
    <xsl:value-of select="acrn:define('IVSHMEM_SHM_REGIONS', '', ' \')" />
    <xsl:for-each select="hv//IVSHMEM/IVSHMEM_REGION[PROVIDED_BY = 'Hypervisor']">
      <xsl:if test="text()">
        <xsl:variable name="memsize" select="IVSHMEM_SIZE" />
        <xsl:text>{ \</xsl:text>
        <xsl:value-of select="$newline" />
        <xsl:value-of select="acrn:initializer('name', concat('IVSHMEM_SHM_REGION_', position() - 1, ', \'), true())" />
        <xsl:value-of select="acrn:initializer('size', concat('0x', acrn:convert-num-base(string($memsize), 10, 16), '00000UL', ', \'), true())" />
        <xsl:choose>
          <xsl:when test="last() = position()">
            <xsl:text>},</xsl:text>
          </xsl:when>
          <xsl:otherwise>
            <xsl:text>}, \</xsl:text>
          </xsl:otherwise>
        </xsl:choose>
        <xsl:value-of select="$newline" />
      </xsl:if>
    </xsl:for-each>
    <xsl:value-of select="$newline" />
  </xsl:template>

</xsl:stylesheet>
