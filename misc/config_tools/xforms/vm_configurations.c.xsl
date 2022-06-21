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
    <xsl:value-of select="acrn:include('asm/pgtable.h')" />
    <xsl:value-of select="acrn:include('schedule.h')" />
    <xsl:value-of select="$newline" />
    <xsl:value-of select="$newline" />

    <xsl:apply-templates select="config-data/acrn-config" />
  </xsl:template>

  <xsl:template match="config-data/acrn-config">
    <!-- Declaration of pci_devs -->
    <xsl:for-each select="vm">
      <xsl:variable name="vm_id" select="@id" />
      <xsl:choose>
        <xsl:when test="acrn:is-service-vm(load_order)">
          <xsl:value-of select="acrn:extern('struct acrn_vm_pci_dev_config', 'sos_pci_devs', 'CONFIG_MAX_PCI_DEV_NUM')" />
        </xsl:when>
        <xsl:when test="acrn:pci-dev-num(@id)">
          <xsl:value-of select="acrn:extern('struct acrn_vm_pci_dev_config', concat('vm', @id, '_pci_devs'), concat('VM', @id, '_CONFIG_PCI_DEV_NUM'))" />
        </xsl:when>
      </xsl:choose>

      <!-- Declaration of pt_intx -->
      <xsl:if test="acrn:is-pre-launched-vm(load_order)">
	<xsl:variable name="length" select="count(acrn:get-intx-mapping(//vm[@id=$vm_id]//pt_intx))" />
	<xsl:choose>
	  <xsl:when test="$length">
            <xsl:value-of select="acrn:extern('struct pt_intx_config', concat('vm', @id, '_pt_intx'), concat($length, 'U'))" />
	  </xsl:when>
	  <xsl:otherwise>
            <xsl:value-of select="acrn:extern('struct pt_intx_config', concat('vm', @id, '_pt_intx'), '1U')" />
	  </xsl:otherwise>
	</xsl:choose>

        <!-- Initializer of memory  -->
        <xsl:value-of select="concat('static struct vm_hpa_regions ', concat('vm', @id, '_hpa'), '[] = {')" />
        <xsl:for-each select="//allocation-data/acrn-config/vm[@id=$vm_id]/memory/hpa_region" >
          <xsl:variable name="pos" select="position()" />
          <xsl:variable name="start_hpa" select="acrn:initializer('start_hpa', ./start_hpa, 'UL')" />
          <xsl:variable name="size_hpa" select="acrn:initializer('size_hpa', ./size_hpa, 'UL')" />
          <xsl:value-of select="concat('{', $start_hpa, ', ',  $size_hpa, '}', ',')" />
        </xsl:for-each>
        <xsl:text>};</xsl:text>
        <xsl:value-of select="$newline" />
      </xsl:if>
    </xsl:for-each>

    <xsl:if test="acrn:is-rdt-enabled()">
      <xsl:value-of select="$newline" />
      <xsl:value-of select="acrn:ifdef('CONFIG_RDT_ENABLED')" />

      <xsl:for-each select="vm">
          <xsl:variable name="vm_id" select="@id" />
          <xsl:value-of select="concat('static uint16_t ', concat('vm', @id, '_vcpu_clos'), '[', count(//allocation-data/acrn-config/vm[@id=$vm_id]/clos/vcpu_clos), 'U] = {')" />
          <xsl:value-of select="acrn:string-join(//allocation-data/acrn-config/vm[@id=$vm_id]/clos/vcpu_clos, ', ', '', 'U')" />
          <xsl:text>};</xsl:text>
          <xsl:value-of select="$newline" />
      </xsl:for-each>

      <xsl:value-of select="$endif" />
      <xsl:value-of select="$newline" />
    </xsl:if>

    <!-- Definition of vm_configs -->
    <xsl:value-of select="acrn:array-initializer('struct acrn_vm_config', 'vm_configs', 'CONFIG_MAX_VM_NUM')" />
    <xsl:apply-templates select="vm"/>
    <xsl:if test="count(vm) &lt; hv/CAPACITIES/MAX_VM_NUM">
        <xsl:value-of select="acrn:vm_fill(count(vm), hv/CAPACITIES/MAX_VM_NUM)"/>
    </xsl:if>
    <xsl:value-of select="$newline"/>
    <xsl:value-of select="$end_of_array_initializer" />
  </xsl:template>

  <xsl:template match="vm">
    <!-- Initializer of a acrn_vm_configs instance -->
    <xsl:text>{</xsl:text>
    <xsl:value-of select="acrn:comment(concat('Static configured VM', @id))" />
    <xsl:value-of select="$newline" />

    <xsl:call-template name="load_order" />
    <xsl:apply-templates select="name" />
    <xsl:if test="acrn:is-service-vm(load_order)">
      <xsl:value-of select="acrn:comment('Allow Service VM to reboot the system since it is the highest priority VM.')" />
      <xsl:value-of select="$newline" />
    </xsl:if>
    <xsl:value-of select="acrn:initializer('vm_prio', priority)" />
    <xsl:value-of select="acrn:initializer('companion_vm_id', concat(companion_vmid, 'U'))" />
    <xsl:call-template name="guest_flags" />

    <xsl:if test="acrn:is-rdt-enabled()">
      <xsl:call-template name="clos" />
    </xsl:if>

    <xsl:call-template name="cpu_affinity" />
    <xsl:apply-templates select="epc_section" />
    <xsl:if test="acrn:is-pre-launched-vm(load_order)">
      <xsl:apply-templates select="memory" />
    </xsl:if>
    <xsl:apply-templates select="os_config" />
    <xsl:call-template name="acpi_config" />
    <xsl:apply-templates select="console_vuart" />
    <xsl:call-template name="vuart_connection" />
    <xsl:call-template name="pci_dev_num" />
    <xsl:call-template name="pci_devs" />
    <xsl:if test="acrn:is-pre-launched-vm(load_order)">
      <xsl:call-template name="pre_launched" />
    </xsl:if>

    <!-- End of the initializer -->
    <xsl:text>}</xsl:text>
    <xsl:if test="not(position() = last())">
      <xsl:text>,</xsl:text>
      <xsl:value-of select="$newline" />
    </xsl:if>
  </xsl:template>

  <xsl:template name="load_order">
    <xsl:variable name="vm_type" select="vm_type" />
    <xsl:choose>
      <xsl:when test="load_order = 'SERVICE_VM'">
        <xsl:value-of select="'CONFIG_SERVICE_VM,'" />
      </xsl:when>
      <xsl:when test="load_order = 'PRE_LAUNCHED_VM' and $vm_type = 'RTVM'">
        <xsl:value-of select="'CONFIG_PRE_RT_VM,'" />
      </xsl:when>
      <xsl:when test="load_order = 'PRE_LAUNCHED_VM' and $vm_type != 'RTVM'">
        <xsl:value-of select="'CONFIG_PRE_STD_VM,'" />
      </xsl:when>
      <xsl:when test="load_order = 'POST_LAUNCHED_VM' and $vm_type = 'RTVM'">
        <xsl:value-of select="'CONFIG_POST_RT_VM,'" />
      </xsl:when>
      <xsl:when test="load_order = 'POST_LAUNCHED_VM' and $vm_type != 'RTVM'">
        <xsl:value-of select="'CONFIG_POST_STD_VM,'" />
      </xsl:when>
    </xsl:choose>
    <xsl:value-of select="$newline" />
  </xsl:template>

  <xsl:template match="name">
    <xsl:value-of select="acrn:initializer('name', concat($quot, current(), $quot))" />
  </xsl:template>

  <xsl:template name="cpu_affinity">
    <xsl:choose>
      <xsl:when test="acrn:is-service-vm(load_order)">
        <xsl:value-of select="acrn:initializer('cpu_affinity', 'SERVICE_VM_CONFIG_CPU_AFFINITY')" />
      </xsl:when>
      <xsl:otherwise>
        <xsl:if test="cpu_affinity">
          <xsl:value-of select="acrn:initializer('cpu_affinity', concat('VM', @id, '_CONFIG_CPU_AFFINITY'))" />
        </xsl:if>
      </xsl:otherwise>
    </xsl:choose>
  </xsl:template>

  <xsl:template name="guest_flags">
    <xsl:variable name="vm_id" select="@id" />
    <xsl:value-of select="acrn:initializer('guest_flags', concat('(', acrn:string-join(//allocation-data/acrn-config/vm[@id=$vm_id]/guest_flags/guest_flag, '|', '', ''),')'))" />
  </xsl:template>

  <xsl:template name="clos">
    <xsl:value-of select="acrn:ifdef('CONFIG_RDT_ENABLED')" />
    <xsl:value-of select="acrn:initializer('pclosids', concat('vm', @id, '_vcpu_clos'))" />

    <xsl:variable name="vm_id" select="@id" />
    <xsl:variable name="vm_name" select="name/text()" />
    <xsl:choose>
      <xsl:when test="acrn:is-vcat-enabled() and virtual_cat_support[text() = 'y']">
        <xsl:value-of select="acrn:initializer('num_pclosids', concat(count(//vm[@id=$vm_id]/virtual_cat_number), 'U'))" />
        <xsl:variable name="rdt_res_str" select="acrn:get-normalized-closinfo-rdt-res-str()" />

        <xsl:if test="contains($rdt_res_str, 'L2')">
          <xsl:value-of select="acrn:initializer('max_l2_pcbm', //CACHE_ALLOCATION[CACHE_LEVEL='2']/POLICY[VM=$vm_name]/CLOS_MASK)" />
        </xsl:if>

        <xsl:if test="contains($rdt_res_str, 'L3')">
          <xsl:value-of select="acrn:initializer('max_l3_pcbm', //CACHE_ALLOCATION[CACHE_LEVEL='3']/POLICY[VM=$vm_name]/CLOS_MASK)" />
        </xsl:if>
      </xsl:when>
      <xsl:otherwise>
        <xsl:value-of select="acrn:initializer('num_pclosids', concat(count(//allocation-data/acrn-config/vm[@id=$vm_id]/clos/vcpu_clos), 'U'))" />
      </xsl:otherwise>
    </xsl:choose>

    <xsl:value-of select="$endif" />
  </xsl:template>

  <xsl:template match="memory">
    <xsl:variable name="vm_id" select="../@id" />
    <xsl:value-of select="acrn:initializer('memory', '{', true())" />
    <xsl:value-of select="acrn:initializer('region_num', count(//allocation-data/acrn-config/vm[@id=$vm_id]/memory/hpa_region))" />
    <xsl:value-of select="acrn:initializer('host_regions', concat('vm', ../@id, '_hpa'))" />
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
    <xsl:if test="kern_load_addr/text()">
      <xsl:value-of select="acrn:initializer('kernel_load_addr', kern_load_addr)" />
    </xsl:if>
    <xsl:if test="kern_entry_addr/text()">
      <xsl:value-of select="acrn:initializer('kernel_entry_addr', kern_entry_addr)" />
    </xsl:if>
    <xsl:if test="normalize-space(bootargs)">
      <xsl:choose>
        <xsl:when test="acrn:is-service-vm(../load_order)">
          <xsl:value-of select="acrn:initializer('bootargs', 'SERVICE_VM_OS_BOOTARGS')" />
        </xsl:when>
        <xsl:when test="acrn:is-pre-launched-vm(../load_order)">
            <xsl:value-of select="acrn:initializer('bootargs', concat('VM', ../@id, '_BOOT_ARGS'))" />
        </xsl:when>
      </xsl:choose>
    </xsl:if>
    <xsl:text>},</xsl:text>
    <xsl:value-of select="$newline" />
  </xsl:template>

  <xsl:template name="acpi_config">
    <xsl:if test="acrn:is-pre-launched-vm(load_order)">
      <xsl:value-of select="acrn:initializer('acpi_config', '{', true())" />
      <xsl:value-of select="acrn:initializer('acpi_mod_tag', concat($quot,'ACPI_VM', @id, $quot))" />
      <xsl:text>},</xsl:text>
      <xsl:value-of select="$newline" />
    </xsl:if>
  </xsl:template>

  <xsl:template match="console_vuart">
    <xsl:value-of select="acrn:initializer('vuart[0]', '{', true())" />
    <xsl:choose>
      <xsl:when test="./text() = 'COM Port 1'">
	<xsl:value-of select="acrn:initializer('type', 'VUART_LEGACY_PIO')" />
	<xsl:value-of select="acrn:initializer('addr.port_base', '0x3F8U')" />
	<xsl:value-of select="acrn:initializer('irq', '4U')" />
      </xsl:when>
      <xsl:when test="./text() = 'COM Port 2'">
	<xsl:value-of select="acrn:initializer('type', 'VUART_LEGACY_PIO')" />
	<xsl:value-of select="acrn:initializer('addr.port_base', '0x2F8U')" />
	<xsl:value-of select="acrn:initializer('irq', '3U')" />
      </xsl:when>
      <xsl:when test="./text() = 'COM Port 3'">
	<xsl:value-of select="acrn:initializer('type', 'VUART_LEGACY_PIO')" />
	<xsl:value-of select="acrn:initializer('addr.port_base', '0x3E8U')" />
	<xsl:value-of select="acrn:initializer('irq', '4U')" />
      </xsl:when>
      <xsl:when test="./text() = 'COM Port 4'">
	<xsl:value-of select="acrn:initializer('type', 'VUART_LEGACY_PIO')" />
	<xsl:value-of select="acrn:initializer('addr.port_base', '0x2E8U')" />
	<xsl:value-of select="acrn:initializer('irq', '3U')" />
      </xsl:when>
      <xsl:when test="./text() = 'PCI'">
	<xsl:value-of select="acrn:initializer('type', 'VUART_PCI')" />
      </xsl:when>
    </xsl:choose>
    <xsl:text>},</xsl:text>
    <xsl:value-of select="$newline" />
  </xsl:template>

  <xsl:template name="vuart_connection">
    <xsl:variable name="vm_id" select="@id" />
    <xsl:variable name="vmname" select="name/text()" />
    <xsl:for-each select="//vuart_connection[endpoint/vm_name = $vmname]">
      <xsl:variable name="connection_name" select="name/text()" />
      <xsl:variable name="type" select="type/text()" />
      <xsl:variable name="vuart_id" select="position()"/>
      <xsl:value-of select="acrn:initializer(concat('vuart[', $vuart_id, ']'), '{', true())" />
      <xsl:if test="$type = 'legacy'">
        <xsl:value-of select="acrn:initializer('irq', concat(//allocation-data/acrn-config/vm[@id=$vm_id]/legacy_vuart[@id=$vuart_id]/irq, 'U'))" />
        <xsl:value-of select="acrn:initializer('type', 'VUART_LEGACY_PIO')" />
        <xsl:for-each select="endpoint">
          <xsl:choose>
            <xsl:when test="vm_name = $vmname">
              <xsl:value-of select="acrn:initializer('addr.port_base', concat(io_port, 'U'))" />
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
      </xsl:if>
      <xsl:text>},</xsl:text>
      <xsl:value-of select="$newline" />
    </xsl:for-each>
  </xsl:template>

  <xsl:template name="pci_dev_num">
    <xsl:choose>
      <xsl:when test="acrn:is-service-vm(load_order)">
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
      <xsl:when test="acrn:is-service-vm(load_order)">
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
      <xsl:value-of select="acrn:initializer('name', concat($quot, 'tpm2', $quot))" />
      <xsl:value-of select="acrn:initializer('res[0]', '{', true())" />
      <xsl:value-of select="acrn:initializer('user_vm_pa', 'VM0_TPM_BUFFER_BASE_ADDR_GPA')" />
      <xsl:value-of select="acrn:initializer('host_pa', 'VM0_TPM_BUFFER_BASE_ADDR')" />
      <xsl:value-of select="acrn:initializer('size', 'VM0_TPM_BUFFER_SIZE')" />
      <xsl:value-of select="acrn:initializer('mem_type', 'EPT_UNCACHED')" />
      <xsl:text>},</xsl:text>
      <xsl:value-of select="$newline" />
      <xsl:if test="//capability[@id='log_area']">
        <xsl:value-of select="acrn:initializer('res[1]', '{', true())" />
        <xsl:value-of select="acrn:initializer('user_vm_pa', 'VM0_TPM_EVENTLOG_BASE_ADDR')" />
        <xsl:value-of select="acrn:initializer('host_pa', 'VM0_TPM_EVENTLOG_BASE_ADDR_HPA')" />
        <xsl:value-of select="acrn:initializer('size', 'VM0_TPM_EVENTLOG_SIZE')" />
        <xsl:value-of select="acrn:initializer('mem_type', 'EPT_WB')" />
        <xsl:text>},</xsl:text>
        <xsl:value-of select="$newline" />
      </xsl:if>
      <xsl:text>},</xsl:text>
      <xsl:value-of select="$newline" />
      <xsl:value-of select="$endif" />
      <xsl:value-of select="acrn:ifdef('P2SB_BAR_ADDR')" />
      <xsl:value-of select="acrn:initializer('pt_p2sb_bar', 'true')" />
      <xsl:value-of select="acrn:initializer('mmiodevs[0]', '{', true())" />
      <xsl:value-of select="acrn:initializer('res[0]', '{', true())" />
      <xsl:value-of select="acrn:initializer('user_vm_pa', 'P2SB_BAR_ADDR_GPA')" />
      <xsl:value-of select="acrn:initializer('host_pa', 'P2SB_BAR_ADDR')" />
      <xsl:value-of select="acrn:initializer('size', 'P2SB_BAR_SIZE')" />
      <xsl:text>},</xsl:text>
      <xsl:value-of select="$newline" />
      <xsl:text>},</xsl:text>
      <xsl:value-of select="$newline" />
      <xsl:value-of select="$endif" />
    </xsl:if>

    <xsl:variable name="vm_id" select="@id" />
    <xsl:variable name="length" select="count(acrn:get-intx-mapping(//vm[@id=$vm_id]//pt_intx))" />
    <xsl:value-of select="acrn:initializer('pt_intx_num', $length)" />
    <xsl:value-of select="acrn:initializer('pt_intx', concat('vm', @id, '_pt_intx'))" />
  </xsl:template>

</xsl:stylesheet>
