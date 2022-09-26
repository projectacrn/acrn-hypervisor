<?xml version="1.0" encoding="utf-8"?>

<!-- Copyright (C) 2021-2022 Intel Corporation. -->
<!-- SPDX-License-Identifier: BSD-3-Clause -->

<xsl:stylesheet
    version="1.0"
    xmlns:xi="http://www.w3.org/2003/XInclude"
    xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
    xmlns:dyn="http://exslt.org/dynamic"
    xmlns:math="http://exslt.org/math"
    xmlns:str="http://exslt.org/strings"
    xmlns:acrn="http://projectacrn.org">
  <xsl:include href="lib.xsl" />
  <xsl:output method="text" />

  <xsl:template match="/acrn-offline-data">
    <!-- Declaration of license -->
    <xsl:value-of select="$license" />

    <!-- Header include guard -->
    <xsl:value-of select="acrn:include-guard('VM_CONFIGURATIONS_H')" />

    <!-- Included headers -->
    <xsl:value-of select="acrn:include('misc_cfg.h')" />
    <xsl:value-of select="acrn:include('pci_devices.h')" />

    <xsl:apply-templates select="config-data/acrn-config" />

    <xsl:value-of select="acrn:include-guard-end('VM_CONFIGURATIONS_H')" />
  </xsl:template>

  <xsl:template match="config-data/acrn-config">
    <xsl:call-template name="vm_count" />
    <xsl:call-template name="sos_vm_bootarges" />
    <xsl:call-template name="vm_vuart_num" />
    <xsl:value-of select = "acrn:define('MAX_IR_ENTRIES', //MAX_IR_ENTRIES, 'U')" />
  </xsl:template>

  <xsl:template name ="vm_vuart_num">
    <xsl:variable name="vuart_nums">
      <xsl:for-each select = "//config-data//vm">
        <xsl:variable name = "vm_name" select = "./name/text()" />
        <xsl:variable name = "vuart_num" select = "count(//endpoint[vm_name = $vm_name]) + 1" />
        <xsl:value-of select = "$vuart_num" />
        <xsl:if test = "position() != last()">
          <xsl:value-of select = "','" />
        </xsl:if>
      </xsl:for-each>
    </xsl:variable>
    <xsl:value-of select = "acrn:define('MAX_VUART_NUM_PER_VM', math:max(str:split($vuart_nums, ',')), 'U')" />
  </xsl:template>

  <xsl:template name ="vm_count">
    <xsl:value-of select="acrn:comment('SERVICE_VM_NUM can only be 0 or 1; When SERVICE_VM_NUM is 1, MAX_POST_VM_NUM must be 0 too.')" />
    <xsl:value-of select="$newline" />
    <xsl:value-of select="acrn:define('PRE_VM_NUM', count(vm[acrn:is-pre-launched-vm(load_order)]), 'U')" />
    <xsl:value-of select="acrn:define('SERVICE_VM_NUM', count(vm[acrn:is-service-vm(load_order)]), 'U')" />
    <xsl:value-of select="acrn:define('MAX_POST_VM_NUM', hv/CAPACITIES/MAX_VM_NUM - count(vm[acrn:is-pre-launched-vm(load_order)]) - count(vm[acrn:is-service-vm(load_order)]) , 'U')" />
    <xsl:value-of select="acrn:define('MAX_TRUSTY_VM_NUM', count(vm[./secure_world_support/text() = 'y']) , 'U')" />
    <xsl:value-of select="acrn:define('CONFIG_MAX_VM_NUM', hv/CAPACITIES/MAX_VM_NUM , 'U')" />
  </xsl:template>

  <xsl:template name ="sos_vm_bootarges">
    <xsl:if test="count(vm[load_order='SERVICE_VM'])">
      <xsl:value-of select="acrn:comment(concat('SERVICE_VM == VM', vm[load_order='SERVICE_VM']/@id))" />
      <xsl:value-of select="$newline" />
      <xsl:value-of select="acrn:define('SERVICE_VM_OS_BOOTARGS', 'SERVICE_VM_ROOTFS SERVICE_VM_IDLE SERVICE_VM_BOOTARGS_DIFF SERVICE_VM_BOOTARGS_MISC', '')" />
    </xsl:if>
  </xsl:template>

</xsl:stylesheet>
