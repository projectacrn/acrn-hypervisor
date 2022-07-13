<?xml version='1.0' encoding='utf-8'?>

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

    <!-- Included headers -->
    <xsl:value-of select="acrn:include('asm/vm_config.h')" />
    <xsl:value-of select="acrn:include('vpci.h')" />
    <xsl:value-of select="acrn:include('asm/mmu.h')" />
    <xsl:value-of select="acrn:include('asm/page.h')" />
    <xsl:value-of select="acrn:include('vmcs9900.h')" />
    <xsl:value-of select="acrn:include('ivshmem_cfg.h')" />

    <xsl:value-of select="acrn:define('INVALID_PCI_BASE', '0', 'U')" />

    <xsl:apply-templates select="config-data/acrn-config/vm" />
  </xsl:template>

  <xsl:template match="config-data/acrn-config/vm">
    <!-- Initializer of a acrn_vm_pci_dev_config instance -->
    <xsl:choose>
      <xsl:when test="acrn:is-service-vm(load_order)">
        <xsl:value-of select="acrn:array-initializer('struct acrn_vm_pci_dev_config', 'sos_pci_devs', 'CONFIG_MAX_PCI_DEV_NUM')" />
      </xsl:when>
      <xsl:when test="acrn:pci-dev-num(@id)">
        <xsl:value-of select="acrn:array-initializer('struct acrn_vm_pci_dev_config', concat('vm', @id, '_pci_devs'), concat('VM', @id, '_CONFIG_PCI_DEV_NUM'))" />
      </xsl:when>
    </xsl:choose>

    <xsl:if test="acrn:is-pre-launched-vm(load_order) and acrn:pci-dev-num(@id)">
      <xsl:call-template name="virtual_pci_hostbridge" />
    </xsl:if>
    <xsl:call-template name="ivshmem_shm_mem" />
    <xsl:apply-templates select="console_vuart" />
    <xsl:call-template name="communication_vuart" />
    <xsl:if test="acrn:is-pre-launched-vm(load_order)">
      <xsl:apply-templates select="pci_devs" />
    </xsl:if>
    <xsl:if test="acrn:is-post-launched-vm(load_order)">
      <xsl:apply-templates select="PTM" />
    </xsl:if>

    <xsl:if test="acrn:is-service-vm(load_order) or acrn:pci-dev-num(@id)">
      <xsl:value-of select="$end_of_array_initializer" />
    </xsl:if>
  </xsl:template>

  <xsl:template name="virtual_pci_hostbridge">
    <xsl:text>{</xsl:text>
    <xsl:value-of select="$newline" />
      <xsl:value-of select="acrn:initializer('emu_type', 'PCI_DEV_TYPE_HVEMUL', '')" />
      <xsl:value-of select="acrn:initializer('vdev_ops', '&amp;vhostbridge_ops', '')" />
      <xsl:value-of select="acrn:initializer('vbdf.bits', '{.b = 0x00U, .d = 0x00U, .f = 0x00U}', '')" />
    <xsl:text>},</xsl:text>
    <xsl:value-of select="$newline" />
  </xsl:template>

  <xsl:template match="console_vuart">
    <xsl:if test="base != 'INVALID_PCI_BASE'">
      <xsl:variable name="vm_id" select="../@id" />
      <xsl:variable name="dev_name" select="concat('VUART_', @id)" />
      <xsl:text>{</xsl:text>
      <xsl:value-of select="$newline" />
        <xsl:value-of select="acrn:initializer('vuart_idx', @id, '')" />
        <xsl:value-of select="acrn:initializer('emu_type', 'PCI_DEV_TYPE_HVEMUL', '')" />
        <xsl:value-of select="acrn:initializer('vdev_ops', '&amp;vmcs9900_ops', '')" />
        <xsl:choose>
          <xsl:when test="acrn:is-post-launched-vm(../load_order)">
            <xsl:value-of select="acrn:initializer('vbar_base[0]', 'INVALID_PCI_BASE', '')" />
            <xsl:value-of select="acrn:initializer('vbdf.value', 'UNASSIGNED_VBDF', '')" />
          </xsl:when>
          <xsl:otherwise>
            <xsl:for-each select="//vm[@id = $vm_id]/device[@name = $dev_name]/bar">
              <xsl:value-of select="acrn:initializer(concat('vbar_base[', @id,']'), concat(text(), 'UL'), '')" />
            </xsl:for-each>
            <xsl:value-of select="acrn:initializer('vbdf.bits', acrn:get-vbdf(../@id, $dev_name), '')" />
          </xsl:otherwise>
        </xsl:choose>
      <xsl:text>},</xsl:text>
      <xsl:value-of select="$newline" />
    </xsl:if>
  </xsl:template>

  <xsl:template name="communication_vuart">
    <xsl:variable name="vm_id" select="@id" />
    <xsl:variable name="vm_name" select="name/text()" />
    <xsl:variable name="load_order" select="load_order/text()" />
    <xsl:for-each select="//vuart_connection[endpoint/vm_name/text() = $vm_name]">
      <xsl:variable name="connection_name" select="name/text()" />
      <xsl:if test="./type/text() = 'pci'">
        <xsl:variable name="vuart_id" select="position()"/>
        <xsl:variable name="dev_name" select="concat('VUART_', $vuart_id)" />
        <xsl:text>{</xsl:text>
        <xsl:value-of select="$newline" />
        <xsl:value-of select="acrn:initializer('vuart_idx', $vuart_id, '')" />
        <xsl:value-of select="acrn:initializer('emu_type', 'PCI_DEV_TYPE_HVEMUL', '')" />
        <xsl:value-of select="acrn:initializer('vdev_ops', '&amp;vmcs9900_ops', '')" />
        <xsl:for-each select="endpoint">
          <xsl:choose>
            <xsl:when test="vm_name = $vm_name">
              <xsl:choose>
                <xsl:when test="acrn:is-post-launched-vm($load_order)">
                  <xsl:value-of select="acrn:initializer('vbar_base[0]', 'INVALID_PCI_BASE', '')" />
                  <xsl:value-of select="acrn:initializer('vbdf.value', 'UNASSIGNED_VBDF')" />
                </xsl:when>
                <xsl:otherwise>
                  <xsl:for-each select="//vm[@id = $vm_id]/device[@name = $dev_name]/bar">
                    <xsl:value-of select="acrn:initializer(concat('vbar_base[', @id,']'), concat(text(), 'UL'), '')" />
                  </xsl:for-each>
                  <xsl:variable name="b" select="substring-before(vbdf/text(), ':')" />
                  <xsl:variable name="d" select="substring-before(substring-after(vbdf/text(), ':'), '.')" />
                  <xsl:variable name="f" select="substring-after(vbdf/text(), '.')" />
                  <xsl:value-of select="acrn:initializer('vbdf.bits', concat('{', '0x', $b, ', ', '0x', $d, ', ', '0x', $f, '}'))" />
                </xsl:otherwise>
              </xsl:choose>
            </xsl:when>
            <xsl:otherwise>
              <xsl:variable name="target_name" select="vm_name" />
              <xsl:value-of select="acrn:initializer('t_vuart.vm_id', concat(//vm[name = $target_name]/@id, 'U'))" />
              <xsl:for-each select="//vuart_connection[endpoint/vm_name = $target_name]">
                <xsl:variable name="uart_num" select="position()"/>
                <xsl:if test="name = $connection_name">
                  <xsl:value-of select="acrn:initializer('t_vuart.vuart_id', concat($uart_num, 'U'))" />
                </xsl:if>
              </xsl:for-each>
            </xsl:otherwise>
          </xsl:choose>
        </xsl:for-each>
      <xsl:text>},</xsl:text>
      <xsl:value-of select="$newline" />
      </xsl:if>
    </xsl:for-each>
  </xsl:template>

  <xsl:template match="pci_devs">
    <xsl:variable name="vm_id" select="../@id" />
    <xsl:for-each select="pci_dev/text()">
      <xsl:text>{</xsl:text>
      <xsl:value-of select="$newline" />
      <xsl:variable name="devname_suffix" select="acrn:ptdev-name-suffix(.)" />
      <xsl:variable name="dev_name" select="concat('PTDEV_', $devname_suffix)" />
      <xsl:value-of select="acrn:initializer('emu_type', 'PCI_DEV_TYPE_PTDEV', '')" />
      <xsl:value-of select="acrn:initializer('vbdf.bits', acrn:get-vbdf($vm_id, $dev_name), '')" />
      <xsl:value-of select="acrn:initializer('pbdf.bits', acrn:get-pbdf(.), '')" />
      <xsl:for-each select="//device[@name = $dev_name]/bar">
        <xsl:value-of select="acrn:initializer(concat('vbar_base[', @id,']'), concat(text(), 'UL'), '')" />
      </xsl:for-each>
      <xsl:text>},</xsl:text>
      <xsl:value-of select="$newline" />
    </xsl:for-each>
  </xsl:template>

  <xsl:template name="ivshmem_shm_mem">
    <xsl:variable name="vm_id" select="@id" />
    <xsl:variable name="vm_name" select="name" />
    <xsl:variable name="load_order" select="load_order" />
    <xsl:for-each select="//hv//IVSHMEM/IVSHMEM_REGION[PROVIDED_BY = 'Hypervisor']/IVSHMEM_VMS/IVSHMEM_VM[VM_NAME = $vm_name]">
      <xsl:text>{</xsl:text>
      <xsl:value-of select="$newline" />
      <xsl:value-of select="acrn:initializer('emu_type', 'PCI_DEV_TYPE_HVEMUL', '')" />
      <xsl:value-of select="acrn:initializer('vdev_ops', '&amp;vpci_ivshmem_ops', '')" />
      <xsl:choose>
        <xsl:when test="acrn:is-post-launched-vm($load_order)">
          <xsl:value-of select="acrn:initializer('vbdf.value', 'UNASSIGNED_VBDF', '')" />
        </xsl:when>
        <xsl:otherwise>
          <xsl:variable name="dev_name" select="concat('IVSHMEM_', acrn:shmem-index(ancestor::IVSHMEM_REGION/NAME))" />
          <xsl:value-of select="acrn:initializer('vbdf.bits', acrn:get-vbdf($vm_id, $dev_name), '')" />
          <xsl:for-each select="//vm[@id = $vm_id]/device[@name = $dev_name]/bar">
            <xsl:value-of select="acrn:initializer(concat('vbar_base[', @id,']'), concat(text(), 'UL'), '')" />
          </xsl:for-each>
        </xsl:otherwise>
      </xsl:choose>
      <xsl:value-of select="acrn:initializer('shm_region_name', concat('IVSHMEM_SHM_REGION_', acrn:shmem-index(ancestor::IVSHMEM_REGION/NAME)), '')" />
      <xsl:text>},</xsl:text>
      <xsl:value-of select="$newline" />
    </xsl:for-each>
  </xsl:template>

  <xsl:template match="PTM">
    <xsl:if test="text() = 'y'">
      <xsl:text>{</xsl:text>
      <xsl:value-of select="$newline" />
      <xsl:value-of select="acrn:initializer('vbdf.value', 'UNASSIGNED_VBDF', '')" />
      <xsl:value-of select="acrn:initializer('vrp_sec_bus', '1', '')" />
      <xsl:text>},</xsl:text>
      <xsl:value-of select="$newline" />
    </xsl:if>
  </xsl:template>

</xsl:stylesheet>
