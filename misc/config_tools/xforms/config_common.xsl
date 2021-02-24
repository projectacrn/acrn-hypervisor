<?xml version="1.0" encoding="utf-8"?>

<!-- Copyright (C) 2021 Intel Corporation. All rights reserved. -->
<!-- SPDX-License-Identifier: BSD-3-Clause -->

<xsl:stylesheet
    version="1.0"
    xml:id="config_common"
    xmlns:xsl="http://www.w3.org/1999/XSL/Transform">

  <!-- XML tree visitors -->

  <xsl:template match="//board-data/acrn-config">
    <xsl:call-template name="string-by-key-value">
      <xsl:with-param name="key" select="'BOARD'" />
      <xsl:with-param name="value" select="@board" />
    </xsl:call-template>
  </xsl:template>

  <xsl:template match="//config-data/acrn-config">
    <xsl:call-template name="string-by-key-value">
      <xsl:with-param name="key" select="'SCENARIO'" />
      <xsl:with-param name="value" select="@scenario" />
    </xsl:call-template>

    <xsl:apply-templates select="hv/DEBUG_OPTIONS" />
    <xsl:apply-templates select="hv/FEATURES" />
    <xsl:apply-templates select="hv/MEMORY" />
    <xsl:apply-templates select="hv/CAPACITIES" />
    <xsl:apply-templates select="hv/MISC_CFG" />
  </xsl:template>

  <xsl:template match="DEBUG_OPTIONS">
    <xsl:call-template name="boolean-by-key">
      <xsl:with-param name="key" select="'RELEASE'" />
    </xsl:call-template>

    <xsl:call-template name="integer-by-key-value">
      <xsl:with-param name="key" select="'MEM_LOGLEVEL_DEFAULT'" />
      <xsl:with-param name="value" select="MEM_LOGLEVEL" />
    </xsl:call-template>

    <xsl:call-template name="integer-by-key-value">
      <xsl:with-param name="key" select="'NPK_LOGLEVEL_DEFAULT'" />
      <xsl:with-param name="value" select="NPK_LOGLEVEL" />
    </xsl:call-template>

    <xsl:call-template name="integer-by-key-value">
      <xsl:with-param name="key" select="'CONSOLE_LOGLEVEL_DEFAULT'" />
      <xsl:with-param name="value" select="CONSOLE_LOGLEVEL" />
    </xsl:call-template>

    <xsl:call-template name="integer-by-key">
      <xsl:with-param name="key" select="'LOG_DESTINATION'" />
    </xsl:call-template>

    <xsl:call-template name="integer-by-key">
      <xsl:with-param name="key" select="'LOG_BUF_SIZE'" />
    </xsl:call-template>

    <xsl:apply-templates select="SERIAL_CONSOLE" />
  </xsl:template>

  <xsl:template match="FEATURES">
    <xsl:call-template name="boolean-by-key-value">
      <xsl:with-param name="key" select="SCHEDULER" />
      <xsl:with-param name="value" select="'y'" />
    </xsl:call-template>

    <xsl:call-template name="boolean-by-key">
      <xsl:with-param name="key" select="'RELOC'" />
    </xsl:call-template>

    <xsl:call-template name="boolean-by-key">
      <xsl:with-param name="key" select="'MULTIBOOT2'" />
    </xsl:call-template>

    <xsl:call-template name="boolean-by-key-value">
      <xsl:with-param name="key" select="'RDT_ENABLED'" />
      <xsl:with-param name="value" select="RDT/RDT_ENABLED" />
    </xsl:call-template>

    <xsl:if test="RDT/RDT_ENABLED = 'y'">
      <xsl:call-template name="boolean-by-key-value">
	<xsl:with-param name="key" select="'CDP_ENABLED'" />
	<xsl:with-param name="value" select="RDT/CDP_ENABLED" />
      </xsl:call-template>
    </xsl:if>

    <xsl:call-template name="boolean-by-key">
      <xsl:with-param name="key" select="'HYPERV_ENABLED'" />
    </xsl:call-template>

    <xsl:call-template name="boolean-by-key">
      <xsl:with-param name="key" select="'ACPI_PARSE_ENABLED'" />
    </xsl:call-template>

    <xsl:call-template name="boolean-by-key-value">
      <xsl:with-param name="key" select="'L1D_FLUSH_VMENTRY_ENABLED'" />
      <xsl:with-param name="value" select="L1D_VMENTRY_ENABLED" />
    </xsl:call-template>

    <xsl:call-template name="boolean-by-key-value">
      <xsl:with-param name="key" select="'MCE_ON_PSC_WORKAROUND_DISABLED'" />
      <xsl:with-param name="value" select="MCE_ON_PSC_DISABLED" />
    </xsl:call-template>

    <xsl:call-template name="boolean-by-key-value">
      <xsl:with-param name="key" select="'PSRAM_ENABLED'" />
      <xsl:with-param name="value" select="PSRAM/PSRAM_ENABLED" />
    </xsl:call-template>

    <xsl:call-template name="boolean-by-key-value">
      <xsl:with-param name="key" select="'IVSHMEM_ENABLED'" />
      <xsl:with-param name="value" select="IVSHMEM/IVSHMEM_ENABLED" />
    </xsl:call-template>
  </xsl:template>

  <xsl:template match="MEMORY">
    <xsl:call-template name="integer-by-key">
      <xsl:with-param name="key" select="'HV_RAM_START'" />
      <xsl:with-param name="default" select="//allocation-data/acrn-config/hv/MEMORY/HV_RAM_START" />
    </xsl:call-template>

    <xsl:call-template name="integer-by-key">
      <xsl:with-param name="key" select="'HV_RAM_SIZE'" />
      <xsl:with-param name="default" select="//allocation-data/acrn-config/hv/MEMORY/HV_RAM_SIZE" />
    </xsl:call-template>

    <xsl:call-template name="integer-by-key">
      <xsl:with-param name="key" select="'PLATFORM_RAM_SIZE'" />
    </xsl:call-template>

    <xsl:call-template name="integer-by-key">
      <xsl:with-param name="key" select="'LOW_RAM_SIZE'" />
    </xsl:call-template>

    <xsl:call-template name="integer-by-key">
      <xsl:with-param name="key" select="'SOS_RAM_SIZE'" />
    </xsl:call-template>

    <xsl:call-template name="integer-by-key">
      <xsl:with-param name="key" select="'UOS_RAM_SIZE'" />
    </xsl:call-template>

    <xsl:call-template name="integer-by-key">
      <xsl:with-param name="key" select="'STACK_SIZE'" />
    </xsl:call-template>
  </xsl:template>

  <xsl:template match="CAPACITIES">
    <xsl:call-template name="integer-by-key">
      <xsl:with-param name="key" select="'IOMMU_BUS_NUM'" />
    </xsl:call-template>

    <xsl:call-template name="integer-by-key">
      <xsl:with-param name="key" select="'MAX_IOAPIC_NUM'" />
    </xsl:call-template>

    <xsl:call-template name="integer-by-key">
      <xsl:with-param name="key" select="'MAX_IR_ENTRIES'" />
    </xsl:call-template>

    <xsl:call-template name="integer-by-key">
      <xsl:with-param name="key" select="'MAX_PCI_DEV_NUM'" />
    </xsl:call-template>

    <xsl:call-template name="integer-by-key">
      <xsl:with-param name="key" select="'MAX_IOAPIC_LINES'" />
    </xsl:call-template>

    <xsl:call-template name="integer-by-key">
      <xsl:with-param name="key" select="'MAX_PT_IRQ_ENTRIES'" />
    </xsl:call-template>

    <xsl:call-template name="integer-by-key">
      <xsl:with-param name="key" select="'MAX_MSIX_TABLE_NUM'" />
      <xsl:with-param name="default" select="normalize-space(/acrn-offline-data/board-data/acrn-config/MAX_MSIX_TABLE_NUM)" />
    </xsl:call-template>

    <xsl:call-template name="integer-by-key-value">
      <xsl:with-param name="key" select="'MAX_EMULATED_MMIO_REGIONS'" />
      <xsl:with-param name="value" select="MAX_EMULATED_MMIO" />
    </xsl:call-template>
  </xsl:template>

  <xsl:template match="SERIAL_CONSOLE">
    <xsl:variable name="tokens" select="concat(substring-before(substring-after(/acrn-offline-data/board-data/acrn-config/TTYS_INFO, concat('seri:', current())), '&#xa;'), ' ')" />
    <xsl:variable name="type" select="substring-before(substring-after($tokens, 'type:'), ' ')" />
    <xsl:variable name="base" select="substring-before(substring-after($tokens, 'base:'), ' ')" />
    <xsl:variable name="irq" select="substring-before(substring-after($tokens, 'irq:'), ' ')" />
    <xsl:variable name="bdf_string" select="substring-before(substring-after($tokens, 'bdf:'), ' ')" />
    <xsl:variable name="bdf" select="substring($bdf_string, 2, string-length($bdf_string) - 2)" />

    <xsl:choose>
      <!-- TODO: Raise an error upon incomplete serial port info -->
      <xsl:when test="$type = 'portio'">
	<xsl:call-template name="boolean-by-key-value">
	  <xsl:with-param name="key" select="'SERIAL_LEGACY'" />
	  <xsl:with-param name="value" select="'y'" />
	</xsl:call-template>
	<xsl:call-template name="integer-by-key-value">
	  <xsl:with-param name="key" select="'SERIAL_PIO_BASE'" />
	  <xsl:with-param name="value" select="$base" />
	</xsl:call-template>
      </xsl:when>
      <xsl:when test="($type = 'mmio') and ($bdf != '')">
	<xsl:call-template name="boolean-by-key-value">
	  <xsl:with-param name="key" select="'SERIAL_PCI'" />
	  <xsl:with-param name="value" select="'y'" />
	</xsl:call-template>

	<xsl:variable name="bus" select="substring-before($bdf, ':')" />
	<xsl:variable name="device" select="substring-before(substring-after($bdf, ':'), '.')" />
	<xsl:variable name="function" select="substring-after($bdf, '.')" />

	<xsl:call-template name="integer-by-key-value">
	  <xsl:with-param name="key" select="'SERIAL_PCI_BDF'" />
	</xsl:call-template>

	<xsl:text>0b</xsl:text>
	<xsl:call-template name="hex-to-bin">
	  <xsl:with-param name="s" select="$bus" />
	  <xsl:with-param name="width" select="4" />
	</xsl:call-template>
	<xsl:call-template name="hex-to-bin">
	  <xsl:with-param name="s" select="$device" />
	  <xsl:with-param name="width" select="1" />
	</xsl:call-template>
	<xsl:call-template name="hex-to-bin">
	  <xsl:with-param name="s" select="$function" />
	  <xsl:with-param name="width" select="3" />
	</xsl:call-template>
	<xsl:value-of select="$integer-suffix" />
	<xsl:text>&#xa;</xsl:text>
      </xsl:when>
      <xsl:otherwise>
	<xsl:call-template name="boolean-by-key-value">
	  <xsl:with-param name="key" select="'SERIAL_MMIO'" />
	  <xsl:with-param name="value" select="'y'" />
	</xsl:call-template>
	<xsl:if test="$base != ''">
	  <xsl:call-template name="integer-by-key-value">
	    <xsl:with-param name="key" select="'SERIAL_MMIO_BASE'" />
	    <xsl:with-param name="value" select="$base" />
	  </xsl:call-template>
	</xsl:if>
      </xsl:otherwise>
    </xsl:choose>
  </xsl:template>

  <xsl:template match="MISC_CFG">
    <xsl:call-template name="integer-by-key">
      <xsl:with-param name="key" select="'GPU_SBDF'" />
    </xsl:call-template>
  </xsl:template>

  <xsl:template name="hex-to-bin">
    <xsl:param name="s" />
    <xsl:param name="width" />

    <xsl:if test="string-length($s) > 0">
      <xsl:variable name="digit" select="substring($s, 1, 1)" />
      <xsl:choose>
	<xsl:when test="$digit='0'"><xsl:value-of select="substring('0000', 5 - $width)" /></xsl:when>
	<xsl:when test="$digit='1'"><xsl:value-of select="substring('0001', 5 - $width)" /></xsl:when>
	<xsl:when test="$digit='2'"><xsl:value-of select="substring('0010', 5 - $width)" /></xsl:when>
	<xsl:when test="$digit='3'"><xsl:value-of select="substring('0011', 5 - $width)" /></xsl:when>
	<xsl:when test="$digit='4'"><xsl:value-of select="substring('0100', 5 - $width)" /></xsl:when>
	<xsl:when test="$digit='5'"><xsl:value-of select="substring('0101', 5 - $width)" /></xsl:when>
	<xsl:when test="$digit='6'"><xsl:value-of select="substring('0110', 5 - $width)" /></xsl:when>
	<xsl:when test="$digit='7'"><xsl:value-of select="substring('0111', 5 - $width)" /></xsl:when>
	<xsl:when test="$digit='8'"><xsl:value-of select="substring('1000', 5 - $width)" /></xsl:when>
	<xsl:when test="$digit='9'"><xsl:value-of select="substring('1001', 5 - $width)" /></xsl:when>
	<xsl:when test="$digit='a'"><xsl:value-of select="substring('1010', 5 - $width)" /></xsl:when>
	<xsl:when test="$digit='b'"><xsl:value-of select="substring('1011', 5 - $width)" /></xsl:when>
	<xsl:when test="$digit='c'"><xsl:value-of select="substring('1100', 5 - $width)" /></xsl:when>
	<xsl:when test="$digit='d'"><xsl:value-of select="substring('1101', 5 - $width)" /></xsl:when>
	<xsl:when test="$digit='e'"><xsl:value-of select="substring('1110', 5 - $width)" /></xsl:when>
	<xsl:when test="$digit='f'"><xsl:value-of select="substring('1111', 5 - $width)" /></xsl:when>
      </xsl:choose>

      <xsl:call-template name="hex-to-bin">
	<xsl:with-param name="s" select="substring($s, 2)" />
	<xsl:with-param name="width" select="4" />
      </xsl:call-template>
    </xsl:if>
  </xsl:template>

  <!-- Common library routines -->

  <xsl:template name="integer-by-key-value">
    <xsl:param name="key" />
    <xsl:param name="value" />
    <xsl:param name="default" />

    <xsl:choose>
      <xsl:when test="$value != ''">
	<xsl:call-template name="entry-by-key-value">
	  <xsl:with-param name="key" select="$key" />
	  <xsl:with-param name="value" select="concat($value, $integer-suffix)" />
	</xsl:call-template>
      </xsl:when>
      <xsl:when test="($value = '') and ($default != '')">
	<xsl:call-template name="entry-by-key-value">
	  <xsl:with-param name="key" select="$key" />
	  <xsl:with-param name="value" select="concat($default, $integer-suffix)" />
	</xsl:call-template>
      </xsl:when>
      <xsl:otherwise>
	<xsl:call-template name="entry-by-key-value">
	  <xsl:with-param name="key" select="$key" />
	</xsl:call-template>
      </xsl:otherwise>
    </xsl:choose>
  </xsl:template>

  <xsl:template name="boolean-by-key">
    <xsl:param name="key" />

    <xsl:call-template name="boolean-by-key-value">
      <xsl:with-param name="key" select="$key" />
      <xsl:with-param name="value" select="./*[name() = $key]" />
    </xsl:call-template>
  </xsl:template>

  <xsl:template name="integer-by-key">
    <xsl:param name="key" />
    <xsl:param name="default" />

    <xsl:call-template name="integer-by-key-value">
      <xsl:with-param name="key" select="$key" />
      <xsl:with-param name="value" select="./*[name() = $key]" />
      <xsl:with-param name="default" select="$default" />
    </xsl:call-template>
  </xsl:template>

</xsl:stylesheet>
