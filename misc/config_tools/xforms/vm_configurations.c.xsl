<?xml version='1.0' encoding='utf-8'?>
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
    <xsl:value-of select="acrn:include('vuart.h')" />
    <xsl:value-of select="acrn:include('asm/pci_dev.h')" />

    <xsl:apply-templates select="config-data/acrn-config" />
  </xsl:template>

  <xsl:template match="config-data/acrn-config">
    <!-- Declaration of pci_devs -->
    <xsl:for-each select="vm">
      <xsl:choose>
        <xsl:when test="acrn:is-sos-vm(vm_type)">
          <xsl:value-of select="acrn:extern('struct acrn_vm_pci_dev_config', 'sos_pci_devs', 'CONFIG_MAX_PCI_DEV_NUM')" />
        </xsl:when>
        <xsl:when test="acrn:pci-dev-num(@id)">
          <xsl:value-of select="acrn:extern('struct acrn_vm_pci_dev_config', concat('vm', @id, '_pci_devs'), concat('VM', @id, '_CONFIG_PCI_DEV_NUM'))" />
        </xsl:when>
      </xsl:choose>
    </xsl:for-each>

    <!-- Declaration of pt_intx -->
    <xsl:variable name="pt_intx" select="normalize-space(vm[@id = 0]/pt_intx)" />
    <xsl:variable name="length" select="string-length($pt_intx) - string-length(translate($pt_intx, ',', ''))" />
    <xsl:choose>
      <xsl:when test="$length > 0">
        <xsl:value-of select="acrn:extern('struct pt_intx_config', 'vm0_pt_intx', concat($length, 'U'))" />
      </xsl:when>
      <xsl:otherwise>
        <xsl:value-of select="acrn:extern('struct pt_intx_config', 'vm0_pt_intx', '1U')" />
      </xsl:otherwise>
    </xsl:choose>

    <!-- Definition of vm_configs -->
    <xsl:value-of select="acrn:array-initializer('struct acrn_vm_config', 'vm_configs', 'CONFIG_MAX_VM_NUM')" />
    <xsl:apply-templates select="vm"/>
    <xsl:value-of select="$end_of_array_initializer" />
  </xsl:template>

  <xsl:template match="vm">
    <!-- Initializer of a acrn_vm_configs instance -->
    <xsl:text>{</xsl:text>
    <xsl:value-of select="acrn:comment(concat('VM', @id))" />
    <xsl:value-of select="$newline" />

    <xsl:apply-templates select="vm_type" />
    <xsl:apply-templates select="name" />
    <xsl:if test="acrn:is-sos-vm(vm_type)">
      <xsl:value-of select="acrn:comment('Allow Service VM to reboot the system since it is the highest priority VM.')" />
      <xsl:value-of select="$newline" />
    </xsl:if>
    <xsl:apply-templates select="guest_flags" />
    <xsl:apply-templates select="clos" />
    <xsl:call-template name="cpu_affinity" />
    <xsl:apply-templates select="epc_section" />
    <xsl:apply-templates select="memory" />
    <xsl:apply-templates select="os_config" />
    <xsl:call-template name="acpi_config" />
    <xsl:apply-templates select="legacy_vuart" />
    <xsl:call-template name="pci_dev_num" />
    <xsl:call-template name="pci_devs" />
    <xsl:if test="acrn:is-pre-launched-vm(vm_type)">
      <xsl:call-template name="pre_launched" />
    </xsl:if>

    <!-- End of the initializer -->
    <xsl:text>},</xsl:text>
    <xsl:value-of select="$newline" />
  </xsl:template>

  <xsl:template match="vm_type">
    <xsl:value-of select="concat('CONFIG_', current())" />
    <xsl:if test="not(acrn:is-sos-vm(current()))">
      <xsl:text>(</xsl:text>
      <xsl:value-of select="count(../preceding-sibling::vm[vm_type = current()]) + 1" />
      <xsl:text>)</xsl:text>
    </xsl:if>
    <xsl:text>,</xsl:text>
    <xsl:value-of select="$newline" />
  </xsl:template>

  <xsl:template match="name">
    <xsl:value-of select="acrn:initializer('name', concat($quot, current(), $quot))" />
  </xsl:template>

  <xsl:template name="cpu_affinity">
    <xsl:choose>
      <xsl:when test="acrn:is-sos-vm(vm_type)">
        <xsl:value-of select="acrn:initializer('cpu_affinity', 'SOS_VM_CONFIG_CPU_AFFINITY')" />
      </xsl:when>
      <xsl:otherwise>
        <xsl:if test="cpu_affinity">
          <xsl:value-of select="acrn:initializer('cpu_affinity', concat('VM', @id, '_CONFIG_CPU_AFFINITY'))" />
        </xsl:if>
      </xsl:otherwise>
    </xsl:choose>
  </xsl:template>

  <xsl:template match="guest_flags">
    <xsl:if test="not(acrn:is-post-launched-vm(../vm_type))">
      <xsl:if test="guest_flag">
        <xsl:choose>
          <xsl:when test="guest_flag = '' or guest_flag = '0' or guest_flag = '0UL'">
            <xsl:value-of select="acrn:initializer('guest_flags', '0UL')" />
          </xsl:when>
          <xsl:otherwise>
            <xsl:value-of select="acrn:initializer('guest_flags', concat('(', acrn:string-join(guest_flag, '|', '', ''),')'))" />
          </xsl:otherwise>
        </xsl:choose>
      </xsl:if>
    </xsl:if>
  </xsl:template>

  <xsl:template match="clos">
    <xsl:value-of select="acrn:ifdef('CONFIG_RDT_ENABLED')" />
    <xsl:value-of select="acrn:initializer('clos', concat('VM', ../@id, '_VCPU_CLOS'))" />
    <xsl:value-of select="$endif" />
  </xsl:template>

  <xsl:template match="memory">
    <xsl:value-of select="acrn:initializer('memory', '{', true())" />
    <xsl:choose>
      <xsl:when test="acrn:is-sos-vm(../vm_type)">
        <xsl:value-of select="acrn:initializer('start_hpa', concat(start_hpa, 'UL'))" />
      </xsl:when>
      <xsl:otherwise>
        <xsl:value-of select="acrn:initializer('start_hpa', concat('VM', ../@id, '_CONFIG_MEM_START_HPA'))" />
        <xsl:value-of select="acrn:initializer('size', concat('VM', ../@id, '_CONFIG_MEM_SIZE'))" />
        <xsl:value-of select="acrn:initializer('start_hpa2', concat('VM', ../@id, '_CONFIG_MEM_START_HPA2'))" />
        <xsl:value-of select="acrn:initializer('size_hpa2', concat('VM', ../@id, '_CONFIG_MEM_SIZE_HPA2'))" />
     </xsl:otherwise>
    </xsl:choose>
    <xsl:text>},</xsl:text>
    <xsl:value-of select="$newline" />
  </xsl:template>

   <xsl:template match="epc_section">
    <xsl:if test="base != '0' and size != '0'">
    <xsl:value-of select="acrn:initializer('epc', '{', true())" />
      <xsl:value-of select="acrn:initializer('base', base)" />
      <xsl:value-of select="acrn:initializer('size', size)" />
      <xsl:text>},</xsl:text>
      <xsl:value-of select="$newline" />
    </xsl:if>
  </xsl:template>

  <xsl:template match="os_config">
    <xsl:value-of select="acrn:initializer('os_config', '{', true())" />
    <xsl:value-of select="acrn:initializer('name', concat($quot, name, $quot))" />
    <xsl:value-of select="acrn:initializer('kernel_type', kern_type)" />
    <xsl:value-of select="acrn:initializer('kernel_mod_tag', concat($quot, kern_mod, $quot))" />
    <xsl:value-of select="acrn:initializer('ramdisk_mod_tag', concat($quot, ramdisk_mod, $quot))" />
    <xsl:if test="kern_load_addr">
      <xsl:value-of select="acrn:initializer('kernel_load_addr', kern_load_addr)" />
    </xsl:if>
    <xsl:if test="kern_entry_addr">
      <xsl:value-of select="acrn:initializer('kernel_entry_addr', kern_entry_addr)" />
    </xsl:if>
    <xsl:if test="normalize-space(bootargs)">
      <xsl:choose>
        <xsl:when test="acrn:is-sos-vm(../vm_type)">
          <xsl:value-of select="acrn:initializer('bootargs', 'SOS_VM_BOOTARGS')" />
        </xsl:when>
        <xsl:when test="acrn:is-pre-launched-vm(../vm_type)">
            <xsl:value-of select="acrn:initializer('bootargs', concat('VM', ../@id, '_BOOT_ARGS'))" />
        </xsl:when>
      </xsl:choose>
    </xsl:if>
    <xsl:text>},</xsl:text>
    <xsl:value-of select="$newline" />
  </xsl:template>

  <xsl:template name="acpi_config">
    <xsl:if test="acrn:is-pre-launched-vm(vm_type)">
      <xsl:value-of select="acrn:initializer('acpi_config', '{', true())" />
      <xsl:value-of select="acrn:initializer('acpi_mod_tag', concat($quot,'ACPI_VM', @id, $quot))" />
      <xsl:text>},</xsl:text>
      <xsl:value-of select="$newline" />
    </xsl:if>
  </xsl:template>

  <xsl:template match="legacy_vuart">
    <xsl:value-of select="acrn:initializer(concat('vuart[', @id, ']'), '{', true())" />
    <xsl:value-of select="acrn:initializer('type', type)" />
    <xsl:value-of select="acrn:initializer('addr.port_base', base)" />
    <xsl:if test="base != 'INVALID_COM_BASE'">
      <xsl:value-of select="acrn:initializer('irq', irq)" />
      <xsl:if test="@id = '1'">
        <xsl:value-of select="acrn:initializer('t_vuart.vm_id', concat(target_vm_id, 'U'))" />
        <xsl:value-of select="acrn:initializer('t_vuart.vuart_id', concat(target_uart_id, 'U'))" />
      </xsl:if>
    </xsl:if>
    <xsl:text>},</xsl:text>
    <xsl:value-of select="$newline" />
  </xsl:template>

  <xsl:template name="pci_dev_num">
    <xsl:choose>
      <xsl:when test="acrn:is-sos-vm(vm_type)">
        <xsl:value-of select="acrn:initializer('pci_dev_num', concat(acrn:pci-dev-num(@id), 'U'))" />
      </xsl:when>
      <xsl:otherwise>
        <xsl:if test="acrn:pci-dev-num(@id)">
          <xsl:value-of select="acrn:initializer('pci_dev_num', concat('VM', @id, '_CONFIG_PCI_DEV_NUM'))" />
        </xsl:if>
      </xsl:otherwise>
    </xsl:choose>
  </xsl:template>

  <xsl:template name="pci_devs">
    <xsl:choose>
      <xsl:when test="acrn:is-sos-vm(vm_type)">
        <xsl:value-of select="acrn:initializer('pci_devs', 'sos_pci_devs')" />
      </xsl:when>
      <xsl:when test="acrn:pci-dev-num(@id)">
        <xsl:value-of select="acrn:initializer('pci_devs', concat('vm', @id, '_pci_devs'))" />
      </xsl:when>
    </xsl:choose>
  </xsl:template>

  <xsl:template name="pre_launched">
    <xsl:if test="@id = '0'">
      <xsl:value-of select="acrn:ifdef('VM0_PASSTHROUGH_TPM')" />
      <xsl:value-of select="acrn:initializer('pt_tpm2', 'true')" />
      <xsl:value-of select="acrn:initializer('mmiodevs[0]', '{', true())" />
      <xsl:value-of select="acrn:initializer('user_vm_pa', 'VM0_TPM_BUFFER_BASE_ADDR_GPA')" />
      <xsl:value-of select="acrn:initializer('service_vm_pa', 'VM0_TPM_BUFFER_BASE_ADDR')" />
      <xsl:value-of select="acrn:initializer('size', 'VM0_TPM_BUFFER_SIZE')" />
      <xsl:text>},</xsl:text>
      <xsl:value-of select="$newline" />
      <xsl:value-of select="$endif" />
      <xsl:value-of select="acrn:ifdef('P2SB_BAR_ADDR')" />
      <xsl:value-of select="acrn:initializer('pt_p2sb_bar', 'true')" />
      <xsl:value-of select="acrn:initializer('mmiodevs[0]', '{', true())" />
      <xsl:value-of select="acrn:initializer('user_vm_pa', 'P2SB_BAR_ADDR_GPA')" />
      <xsl:value-of select="acrn:initializer('service_vm_pa', 'P2SB_BAR_ADDR')" />
      <xsl:value-of select="acrn:initializer('size', 'P2SB_BAR_SIZE')" />
      <xsl:text>},</xsl:text>
      <xsl:value-of select="$newline" />
      <xsl:value-of select="$endif" />
      <xsl:value-of select="acrn:initializer('pt_intx_num', 'VM0_PT_INTX_NUM')" />
      <xsl:value-of select="acrn:initializer('pt_intx', '&amp;vm0_pt_intx[0U]')" />
    </xsl:if>
  </xsl:template>

</xsl:stylesheet>
