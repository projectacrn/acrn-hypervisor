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
    <xsl:call-template name="dm_guest_flag" />
    <xsl:call-template name="pre_launched_vm_hpa" />
    <xsl:call-template name="sos_vm_bootarges" />
  </xsl:template>

  <xsl:template name ="vm_count">
    <xsl:value-of select="acrn:comment('SOS_VM_NUM can only be 0U or 1U; When SOS_VM_NUM is 0U, MAX_POST_VM_NUM must be 0U too; MAX_POST_VM_NUM must be bigger than CONFIG_MAX_KATA_VM_NUM.')" />
    <xsl:value-of select="$newline" />
    <xsl:value-of select="acrn:define('PRE_VM_NUM', count(vm[acrn:is-pre-launched-vm(vm_type)]), 'U')" />
    <xsl:value-of select="acrn:define('SOS_VM_NUM', count(vm[acrn:is-sos-vm(vm_type)]), 'U')" />
    <xsl:value-of select="acrn:define('MAX_POST_VM_NUM', count(vm[acrn:is-post-launched-vm(vm_type)]), 'U')" />
    <xsl:value-of select="acrn:define('CONFIG_MAX_KATA_VM_NUM', count(vm[acrn:is-kata-vm(vm_type)]), 'U')" />
  </xsl:template>

  <xsl:template name ="dm_guest_flag">
    <xsl:choose>
      <xsl:when test="count(vm[vm_type='SOS_VM'])">
        <xsl:value-of select="acrn:comment('Bitmask of guest flags that can be programmed by device model. Other bits are set by hypervisor only.')" />
        <xsl:value-of select="$newline" />
        <xsl:value-of select="acrn:define('DM_OWNED_GUEST_FLAG_MASK', '(GUEST_FLAG_SECURE_WORLD_ENABLED | GUEST_FLAG_LAPIC_PASSTHROUGH | GUEST_FLAG_RT | GUEST_FLAG_IO_COMPLETION_POLLING | GUEST_FLAG_TPM2_FIXUP)', '')" />
      </xsl:when>
      <xsl:otherwise>
      <xsl:value-of select="acrn:define('DM_OWNED_GUEST_FLAG_MASK', '0', 'UL')" />
      </xsl:otherwise>
    </xsl:choose>
  </xsl:template>

  <xsl:template name ="sos_vm_bootarges">
    <xsl:if test="count(vm[vm_type='SOS_VM'])">
      <xsl:value-of select="acrn:comment(concat('SOS_VM == VM', vm[vm_type='SOS_VM']/@id))" />
      <xsl:value-of select="$newline" />
      <xsl:value-of select="acrn:define('SOS_VM_BOOTARGS', 'SOS_ROOTFS SOS_CONSOLE SOS_IDLE SOS_BOOTARGS_DIFF', '')" />
    </xsl:if>
  </xsl:template>

  <xsl:template name ="pre_launched_vm_hpa">
    <xsl:for-each select="vm">
      <xsl:if test="acrn:is-pre-launched-vm(vm_type)">
        <xsl:value-of select="acrn:define(concat('VM', @id, '_CONFIG_MEM_START_HPA'), memory/start_hpa, 'UL')" />
        <xsl:value-of select="acrn:define(concat('VM', @id, '_CONFIG_MEM_SIZE'), memory/size, 'UL')" />
        <xsl:value-of select="acrn:define(concat('VM', @id, '_CONFIG_MEM_START_HPA2'), memory/start_hpa2, 'UL')" />
        <xsl:value-of select="acrn:define(concat('VM', @id, '_CONFIG_MEM_SIZE_HPA2'), memory/size_hpa2, 'UL')" />
      </xsl:if>
    </xsl:for-each>
  </xsl:template>

</xsl:stylesheet>
