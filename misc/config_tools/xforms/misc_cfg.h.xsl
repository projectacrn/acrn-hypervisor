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
    <xsl:value-of select="acrn:include-guard('MISC_CFG_H')" />

    <xsl:apply-templates select="board-data/acrn-config" />
    <xsl:apply-templates select="config-data/acrn-config" />

    <xsl:apply-templates select="allocation-data/acrn-config" />

    <xsl:value-of select="acrn:include-guard-end('MISC_CFG_H')" />
  </xsl:template>

  <xsl:template match="board-data/acrn-config">
    <xsl:apply-templates select="BLOCK_DEVICE_INFO" />
  </xsl:template>

  <xsl:template match="config-data/acrn-config">
    <xsl:if test="count(vm[acrn:is-service-vm(load_order)])">
      <xsl:call-template name="sos_bootargs_diff" />
    </xsl:if>
    <xsl:call-template name="cpu_affinity" />
    <xsl:call-template name="rdt" />
    <xsl:call-template name="vm0_passthrough_tpm" />
    <xsl:call-template name="vm_config_pci_dev_num" />
    <xsl:call-template name="vm_boot_args" />
  </xsl:template>

  <xsl:template match="allocation-data/acrn-config">
    <xsl:choose>
      <xsl:when test="//ssram">
        <xsl:value-of select="acrn:define('PRE_RTVM_SW_SRAM_MAX_SIZE', //ssram/max_size, 'UL')" />
      </xsl:when>
      <xsl:otherwise>
        <xsl:value-of select="acrn:define('PRE_RTVM_SW_SRAM_MAX_SIZE', 0, 'UL')" />
      </xsl:otherwise>
    </xsl:choose>
  </xsl:template>

  <xsl:template match="BLOCK_DEVICE_INFO">
    <xsl:variable name="block_devices_list_1" select="translate(current(), $newline, ',')" />
    <xsl:variable name="block_devices_list_2" select="translate($block_devices_list_1, $whitespaces, '')" />
    <xsl:variable name="block_devices_list" select="str:split($block_devices_list_2, ',')" />
    <xsl:for-each select="$block_devices_list">
      <xsl:variable name="pos" select="position()" />
      <xsl:variable name="block_device" select="$block_devices_list[$pos]" />
      <xsl:if test="not(contains($block_device, 'ext4'))">
        <xsl:value-of select="acrn:define(concat('ROOTFS_', $pos), concat($quot, 'root=', substring-before($block_device, ':'), ' ', $quot))" />
      </xsl:if>
    </xsl:for-each>
  </xsl:template>

<xsl:template name="sos_bootargs_diff">
  <xsl:variable name="sos_rootfs">
  <xsl:variable name="bootargs" select="str:split(//vm[acrn:is-service-vm(load_order)]/os_config/bootargs[text()], ' ')" />
  <xsl:for-each select="$bootargs">
    <xsl:variable name="pos" select="position()" />
    <xsl:variable name="bootarg" select="$bootargs[$pos]" />
      <xsl:if test="contains($bootarg, 'root=')">
        <xsl:value-of select="$bootarg" />
      </xsl:if>
  </xsl:for-each>
  </xsl:variable>
  <xsl:variable name="sos_bootargs" select="normalize-space(str:replace(//vm[acrn:is-service-vm(load_order)]/os_config/bootargs[text()], $sos_rootfs, ''))" />
  <xsl:variable name="maxcpunum" select="count(//vm[acrn:is-service-vm(load_order)]/cpu_affinity//pcpu_id)" />
  <xsl:variable name="maxcpus">
    <xsl:choose>
      <xsl:when test="$maxcpunum != 0">
        <xsl:value-of select="concat('maxcpus=', $maxcpunum)" />
      </xsl:when>
      <xsl:otherwise>
        <xsl:value-of select="''" />
      </xsl:otherwise>
    </xsl:choose>
  </xsl:variable>
  <xsl:variable name="hugepage_kernelstring">
    <xsl:if test="//board-data//processors//capability[@id='gbyte_pages']">
      <xsl:value-of select="concat('hugepagesz=1G hugepages=', //allocation-data//vm[acrn:is-service-vm(load_order)]/hugepages/gb)" />
    </xsl:if>
  </xsl:variable>
  <xsl:value-of select="acrn:define('SERVICE_VM_ROOTFS', concat($quot, $sos_rootfs, ' ', $quot), '')" />
  <xsl:value-of select="acrn:define('SERVICE_VM_BOOTARGS_DIFF', concat($quot, $sos_bootargs, ' ', $maxcpus, ' ', $hugepage_kernelstring, ' ', $quot), '')" />
  <xsl:value-of select="acrn:define('SERVICE_VM_BOOTARGS_MISC', concat($quot, 'udmabuf.list_limit=8192 ', $quot), '')" />
</xsl:template>

<xsl:template name="cpu_affinity">
  <xsl:for-each select="vm">
    <xsl:choose>
      <xsl:when test="acrn:is-service-vm(load_order)">
        <xsl:value-of select="acrn:define('SERVICE_VM_CONFIG_CPU_AFFINITY', concat('(', acrn:string-join(//vm[acrn:is-service-vm(load_order)]/cpu_affinity//pcpu_id, '|', 'AFFINITY_CPU(', 'U)'),')'), '')" />
      </xsl:when>
      <xsl:otherwise>
        <xsl:value-of select="acrn:define(concat('VM', @id, '_CONFIG_CPU_AFFINITY'), concat('(', acrn:string-join(cpu_affinity//pcpu_id, '|', 'AFFINITY_CPU(', 'U)'),')'), '')" />
      </xsl:otherwise>
    </xsl:choose>
  </xsl:for-each>
</xsl:template>

<!-- HV_SUPPORTED_MAX_CLOS:
  The maximum CLOS that is allowed by ACRN hypervisor.
  Its value is set to be least common Max CLOS (CPUID.(EAX=0x10,ECX=ResID):EDX[15:0])
  among all supported RDT resources in the platform. In other words, it is
  min(maximum CLOS of L2, L3 and MBA). This is done in order to have consistent
  CLOS allocations between all the RDT resources. -->
<!-- MAX_MBA_CLOS_NUM_ENTRIES:
  Max number of Cache Mask entries corresponding to each CLOS.
  This can vary if CDP is enabled vs disabled, as each CLOS entry will have corresponding
  cache mask values for Data and Code when CDP is enabled. -->
<!-- MAX_CACHE_CLOS_NUM_ENTRIES:
  Max number of MBA delay entries corresponding to each CLOS. -->
<xsl:template name="rdt">
  <xsl:variable name="rdt_res_clos_max" select="acrn:get-normalized-closinfo-rdt-clos-max-str()" />
  <xsl:variable name="common_clos_max" select="acrn:get-common-clos-count()"/>
  <xsl:choose>
    <xsl:when test="acrn:is-cdp-enabled()">
      <xsl:value-of select="acrn:ifdef('CONFIG_RDT_ENABLED')" />
      <xsl:value-of select="acrn:ifdef('CONFIG_CDP_ENABLED')" />
      <xsl:value-of select="acrn:define('HV_SUPPORTED_MAX_CLOS', $common_clos_max, 'U')" />
      <xsl:value-of select="acrn:define('MAX_CACHE_CLOS_NUM_ENTRIES', 2 * $common_clos_max, 'U')" />
      <xsl:value-of select="$else" />
      <xsl:value-of select="acrn:define('HV_SUPPORTED_MAX_CLOS', acrn:find-list-min($rdt_res_clos_max, ','), 'U')" />
      <xsl:value-of select="acrn:define('MAX_CACHE_CLOS_NUM_ENTRIES', $common_clos_max, 'U')" />
      <xsl:value-of select="$endif" />
      <xsl:value-of select="acrn:define('MAX_MBA_CLOS_NUM_ENTRIES', $common_clos_max, 'U')" />
    </xsl:when>
    <xsl:otherwise>
      <xsl:value-of select="acrn:ifdef('CONFIG_RDT_ENABLED')" />
      <xsl:value-of select="acrn:define('HV_SUPPORTED_MAX_CLOS', $common_clos_max, 'U')" />
      <xsl:value-of select="acrn:define('MAX_MBA_CLOS_NUM_ENTRIES', $common_clos_max, 'U')" />
      <xsl:value-of select="acrn:define('MAX_CACHE_CLOS_NUM_ENTRIES', $common_clos_max, 'U')" />
      <xsl:if test="not(acrn:is-rdt-supported())">
        <xsl:value-of select="$endif" />
      </xsl:if>
    </xsl:otherwise>
  </xsl:choose>
  <xsl:if test="acrn:is-rdt-supported()">
    <xsl:value-of select="$endif" />
  </xsl:if>
</xsl:template>

<xsl:template name="vm0_passthrough_tpm">
  <xsl:if test="acrn:is-pre-launched-vm(vm[@id = 0]/load_order)">
    <xsl:if test="//vm/mmio_resources/TPM2/text() = 'y' and //device[@id = 'MSFT0101' or compatible_id = 'MSFT0101']">
      <xsl:value-of select="acrn:define('VM0_PASSTHROUGH_TPM', '', '')" />
      <xsl:value-of select="acrn:define('VM0_TPM_BUFFER_BASE_ADDR', '0xFED40000', 'UL')" />
      <xsl:value-of select="acrn:define('VM0_TPM_BUFFER_BASE_ADDR_GPA', '0xFED40000', 'UL')" />
      <xsl:value-of select="acrn:define('VM0_TPM_BUFFER_SIZE', '0x5000', 'UL')" />
      <xsl:if test="//capability[@id='log_area']">
        <xsl:value-of select="acrn:define('VM0_TPM_EVENTLOG_BASE_ADDR', //allocation-data/acrn-config/vm[@id = '0']/log_area_start_address, 'UL')" />
        <xsl:value-of select="acrn:define('VM0_TPM_EVENTLOG_BASE_ADDR_HPA', //capability[@id='log_area']/log_area_start_address, 'UL')" />
        <xsl:value-of select="acrn:define('VM0_TPM_EVENTLOG_SIZE', //allocation-data/acrn-config/vm[@id = '0']/log_area_minimum_length, 'UL')" />
      </xsl:if>
    </xsl:if>
  </xsl:if>
</xsl:template>

<xsl:template name="vm_config_pci_dev_num">
  <xsl:for-each select="vm">
    <xsl:if test="acrn:pci-dev-num(@id)">
      <xsl:value-of select="acrn:define(concat('VM', @id, '_CONFIG_PCI_DEV_NUM'), acrn:pci-dev-num(@id), 'U')" />
    </xsl:if>
  </xsl:for-each>
  <xsl:value-of select="$newline" />
</xsl:template>

<xsl:template name="vm_boot_args">
  <xsl:for-each select="vm">
    <xsl:if test="acrn:is-pre-launched-vm(load_order)">
    <xsl:variable name="bootargs" select="normalize-space(os_config/bootargs)" />
      <xsl:if test="$bootargs">
        <xsl:value-of select="acrn:define(concat('VM', @id, '_BOOT_ARGS'), concat($quot, $bootargs, ' ', $quot), '')" />
      </xsl:if>
    </xsl:if>
  </xsl:for-each>
</xsl:template>

</xsl:stylesheet>
