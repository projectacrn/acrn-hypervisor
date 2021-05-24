<?xml version='1.0' encoding='utf-8'?>

<!-- Copyright (C) 2021 Intel Corporation. -->
<!-- SPDX-License-Identifier: BSD-3-Clause -->

<xsl:stylesheet
    version="1.0"
    xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
    xmlns:dyn="http://exslt.org/dynamic"
    xmlns:math="http://exslt.org/math"
    xmlns:func="http://exslt.org/functions"
    xmlns:acrn="http://projectacrn.org"
    extension-element-prefixes="func">

  <xsl:variable name="digits" select="'0123456789abcdef'" />
  <xsl:variable name="lowercase" select="'abcdefghijklmnopqrstuvwxyz'" />
  <xsl:variable name="uppercase" select="'ABCDEFGHIJKLMNOPQRSTUVWXYZ'" />

  <!-- C code common variables -->
  <xsl:variable name="newline" select="'&#xa;'" />
  <xsl:variable name="quot" select="'&#x22;'" />
  <xsl:variable name="end_of_initializer" select="concat(',', $newline)" />
  <xsl:variable name="end_of_array_initializer" select="concat('};', $newline)" />
  <xsl:variable name="else" select="concat('#else', $newline)" />
  <xsl:variable name="endif" select="concat('#endif', $newline)" />
  <xsl:variable name="license">
    <xsl:text>/*&#xa;</xsl:text>
    <xsl:text> * Copyright (C) YEAR Intel Corporation.&#xa;</xsl:text>
    <xsl:text> *&#xa;</xsl:text>
    <xsl:text> * SPDX-License-Identifier: BSD-3-Clause&#xa;</xsl:text>
    <xsl:text> */&#xa;</xsl:text>
    <xsl:text>&#xa;</xsl:text>
  </xsl:variable>
  <!-- End of C code variables -->

  <!-- Generic library auxiliaries-->
  <!-- < Do not call auxiliaries directly externally and internally >-->
  <!-- < Please call auxiliaries wrappers instead >-->
  <func:function name="acrn:string-to-num-aux">
    <xsl:param name="v" />
    <xsl:param name="base" />
    <xsl:param name="acc" />
    <xsl:variable name="digit" select="string-length(substring-before($digits, translate(substring($v, 1, 1), $uppercase, $lowercase)))" />
    <xsl:choose>
      <xsl:when test="string-length($v) = 1">
        <func:result select="($acc * $base) + $digit" />
      </xsl:when>
      <xsl:otherwise>
        <func:result select="acrn:string-to-num-aux(substring($v, 2), $base, ($acc * $base) + $digit)" />
      </xsl:otherwise>
    </xsl:choose>
  </func:function>

  <func:function name="acrn:find-list-min-aux">
    <xsl:param name="list" />
    <xsl:param name="delimar" />
    <xsl:param name="min" />
    <func:result>
      <xsl:choose>
        <xsl:when test="contains($list, $delimar)">
          <xsl:value-of select="acrn:find-list-min-aux(substring-after($list, $delimar), $delimar, acrn:min(substring-before($list, $delimar), $min))" />
        </xsl:when>
        <xsl:otherwise>
          <xsl:value-of select="acrn:min($list, $min)" />
        </xsl:otherwise>
      </xsl:choose>
    </func:result>
  </func:function>

  <func:function name="acrn:num-to-string-aux">
    <xsl:param name="v" />
    <xsl:param name="base" />
    <xsl:param name="acc" />
    <xsl:variable name="digit" select="substring($digits, ($v mod $base) + 1 , 1)" />
    <xsl:choose>
      <xsl:when test="floor($v div $base) = 0">
        <func:result select="concat($digit, $acc)" />
      </xsl:when>
      <xsl:otherwise>
        <func:result select="acrn:num-to-string-aux(floor($v div $base), $base, concat($digit, $acc))" />
      </xsl:otherwise>
    </xsl:choose>
  </func:function>
  <!-- End of generic library auxiliaries-->

  <!-- Generic library routines-->
  <func:function name="acrn:string-to-num">
    <xsl:param name="v" />
    <xsl:param name="base" />
    <func:result select="acrn:string-to-num-aux($v, $base, 0)" />
  </func:function>

  <func:function name="acrn:num-to-string">
    <xsl:param name="v" />
    <xsl:param name="base" />
    <func:result select="acrn:num-to-string-aux($v, $base, '')" />
  </func:function>

  <func:function name="acrn:convert-num-base">
    <xsl:param name="v" />
    <xsl:param name="base" />
    <xsl:param name="newbase" />
    <xsl:param name="num" select="acrn:string-to-num($v, $base)" />
    <func:result select="acrn:num-to-string($num, $newbase)" />
  </func:function>

  <func:function name="acrn:repeat">
    <xsl:param name="string" />
    <xsl:param name="times" />
    <func:result>
      <xsl:if test="$times > 0">
        <xsl:value-of select="$string" />
        <xsl:value-of select="acrn:repeat($string, $times - 1)" />
      </xsl:if>
    </func:result>
  </func:function>

  <func:function name="acrn:min">
    <xsl:param name="a" />
    <xsl:param name="b" />
    <xsl:choose>
      <xsl:when test="$a &lt; $b">
        <func:result select="$a" />
      </xsl:when>
      <xsl:otherwise>
        <func:result select="$b" />
      </xsl:otherwise>
    </xsl:choose>
  </func:function>

  <func:function name="acrn:find-list-min">
    <xsl:param name="list" />
    <xsl:param name="delimar" />
    <func:result>
      <xsl:value-of select="acrn:find-list-min-aux($list, $delimar, '')" />
    </func:result>
  </func:function>

  <func:function name="acrn:string-join">
    <xsl:param name="content" />
    <xsl:param name="delimiter" />
    <xsl:param name="prefix" />
    <xsl:param name="suffix" />
    <func:result>
      <xsl:for-each select="$content">
        <xsl:value-of select="concat($prefix, current(), $suffix)" />
        <xsl:if test="position() != last()">
          <xsl:value-of select="$delimiter" />
        </xsl:if>
      </xsl:for-each>
    </func:result>
  </func:function>
  <!-- End of generic library routines-->

  <!-- C code templates-->
  <func:function name="acrn:initializer">
    <xsl:param name="member" />
    <xsl:param name="value" />
    <xsl:param name="not_end" />
    <func:result>
      <xsl:text>.</xsl:text>
      <xsl:value-of select="$member" />
      <xsl:text> = </xsl:text>
      <xsl:value-of select="$value" />
      <xsl:choose>
        <xsl:when test="$not_end">
          <xsl:value-of select="$newline" />
        </xsl:when>
        <xsl:otherwise>
          <xsl:value-of select="$end_of_initializer" />
        </xsl:otherwise>
      </xsl:choose>
    </func:result>
  </func:function>

  <func:function name="acrn:include">
    <xsl:param name="header" />
    <func:result>
      <xsl:text>#include &lt;</xsl:text>
      <xsl:value-of select="$header" />
      <xsl:text>&gt;&#xa;</xsl:text>
    </func:result>
  </func:function>

  <func:function name="acrn:define">
    <xsl:param name="name" />
    <xsl:param name="value" />
    <xsl:param name="suffix" />
    <xsl:variable name="whitespace" select="30 - string-length($name)" />
    <xsl:variable name="times">
      <xsl:choose>
        <xsl:when test="$whitespace > 0">
          <xsl:value-of select="$whitespace" />
        </xsl:when>
        <xsl:otherwise>
          <xsl:value-of select="'1'" />
        </xsl:otherwise>
      </xsl:choose>
    </xsl:variable>
    <func:result>
      <xsl:text>#define </xsl:text>
      <xsl:value-of select="$name" />
      <xsl:value-of select="acrn:repeat(' ', $times)" />
      <xsl:value-of select="$value" />
      <xsl:value-of select="$suffix" />
      <xsl:text>&#xa;</xsl:text>
    </func:result>
  </func:function>

  <func:function name="acrn:extern">
    <xsl:param name="type" />
    <xsl:param name="name" />
    <xsl:param name="size" />
    <func:result>
      <xsl:text>extern </xsl:text>
      <xsl:value-of select="$type" />
      <xsl:text> </xsl:text>
      <xsl:value-of select="$name" />
      <xsl:text>[</xsl:text>
      <xsl:value-of select="$size" />
      <xsl:text>];</xsl:text>
      <xsl:text>&#xa;</xsl:text>
    </func:result>
  </func:function>

  <func:function name="acrn:array-initializer">
    <xsl:param name="type" />
    <xsl:param name="name" />
    <xsl:param name="size" />
    <func:result>
      <xsl:value-of select="$type" />
      <xsl:text> </xsl:text>
      <xsl:value-of select="$name" />
      <xsl:text>[</xsl:text>
      <xsl:value-of select="$size" />
      <xsl:text>] = {</xsl:text>
      <xsl:text>&#xa;</xsl:text>
    </func:result>
  </func:function>

  <func:function name="acrn:ifdef">
    <xsl:param name="config" />
    <func:result>
      <xsl:text>#ifdef </xsl:text>
      <xsl:value-of select="$config" />
      <xsl:text>&#xa;</xsl:text>
    </func:result>
  </func:function>

  <func:function name="acrn:include-guard">
    <xsl:param name="name" />
    <func:result>
      <xsl:text>#ifndef </xsl:text>
      <xsl:value-of select="$name" />
      <xsl:text>&#xa;</xsl:text>
      <xsl:text>#define </xsl:text>
      <xsl:value-of select="$name" />
      <xsl:text>&#xa;</xsl:text>
      <xsl:text>&#xa;</xsl:text>
    </func:result>
  </func:function>

  <func:function name="acrn:include-guard-end">
    <xsl:param name="name" />
    <func:result>
      <xsl:text>&#xa;</xsl:text>
      <xsl:text>#endif </xsl:text>
      <xsl:value-of select="concat('/* ', $name, ' */')" />
      <xsl:text>&#xa;</xsl:text>
    </func:result>
  </func:function>

  <func:function name="acrn:comment">
    <xsl:param name="content" />
    <func:result>
      <xsl:text>/* </xsl:text>
      <xsl:value-of select="$content" />
      <xsl:text> */</xsl:text>
    </func:result>
  </func:function>
  <!-- End of C code templates-->

  <!-- Scenario-specific functions-->
  <func:function name="acrn:parse-shmem-size">
    <xsl:param name="v" />
    <xsl:if test="$v and $v != 'n' and $v != 'y'">
      <func:result select="acrn:string-to-num(substring-before(substring-after($v, ','), ','), 10)" />
    </xsl:if>
  </func:function>

  <func:function name="acrn:count-shmem-dev-num">
    <xsl:param name="v" />
    <func:result select="string-length($v) - string-length(translate($v, ':', ''))" />
  </func:function>

  <func:function name="acrn:pci-dev-num">
    <xsl:param name="vmid" />
    <xsl:for-each select="//config-data/acrn-config/vm[@id = $vmid]">
      <xsl:variable name="vmtype" select="./vm_type" />
      <xsl:variable name="ivshmem">
        <xsl:choose>
          <xsl:when test="../hv/FEATURES/IVSHMEM/IVSHMEM_ENABLED = 'y'">
            <xsl:value-of select="count(../hv/FEATURES/IVSHMEM/IVSHMEM_REGION[contains(substring-after(substring-after(text(), ','), ','), $vmid)])" />
          </xsl:when>
          <xsl:otherwise>
            <xsl:value-of select="0" />
          </xsl:otherwise>
        </xsl:choose>
      </xsl:variable>
      <xsl:variable name="console_vuart" select="count(./console_vuart/base[text() = 'PCI_VUART'])" />
      <xsl:variable name="communication_vuart" select="count(./communication_vuart/base[text() = 'PCI_VUART'])" />
      <xsl:variable name="pci_devs" select="count(./pci_devs/pci_dev[text() != ''])" />
      <xsl:variable name="pci_hostbridge" select="1" />
      <xsl:variable name="virtual_root_port">
        <xsl:choose>
          <xsl:when test="./PTM = 'y'">
            <xsl:value-of select="1" />
          </xsl:when>
          <xsl:otherwise>
            <xsl:value-of select="0" />
          </xsl:otherwise>
        </xsl:choose>
      </xsl:variable>
      <xsl:if test="acrn:is-pre-launched-vm($vmtype)">
        <xsl:if test="$ivshmem + $console_vuart + $communication_vuart + $pci_devs > 0">
          <func:result select="$ivshmem + $console_vuart + $communication_vuart + $pci_devs + $pci_hostbridge" />
        </xsl:if>
      </xsl:if>
      <xsl:if test="acrn:is-post-launched-vm($vmtype)">
        <func:result select="$ivshmem + $console_vuart + $communication_vuart + $virtual_root_port" />
      </xsl:if>
      <xsl:if test="acrn:is-sos-vm($vmtype)">
        <func:result select="$ivshmem + $console_vuart + $communication_vuart" />
      </xsl:if>
    </xsl:for-each>
  </func:function>

  <!-- acrn:get-hidden-device-num checks if a board has hidden devices which cannot be explored by scanning pci configuration space -->
  <func:function name="acrn:get-hidden-device-num">
    <xsl:choose>
      <!-- apl-mrb hidden devices list: [00:0d:0] -->
      <xsl:when test="//@board = 'apl-mrb'">
        <func:result select="1" />
      </xsl:when>
      <!-- apl-up2 hidden devices list: [00:0d:0] -->
      <xsl:when test="//@board = 'apl-up2'">
        <func:result select="1" />
      </xsl:when>
      <xsl:otherwise>
        <func:result select="0" />
      </xsl:otherwise>
    </xsl:choose>
  </func:function>

  <func:function name="acrn:is-rdt-enabled">
    <xsl:choose>
      <xsl:when test="//RDT_ENABLED = 'y'">
        <func:result select="true()" />
      </xsl:when>
      <xsl:otherwise>
        <func:result select="false()" />
      </xsl:otherwise>
    </xsl:choose>
  </func:function>

  <func:function name="acrn:is-cdp-enabled">
    <xsl:choose>
      <xsl:when test="acrn:is-rdt-enabled() and //CDP_ENABLED = 'y'">
        <func:result select="true()" />
      </xsl:when>
      <xsl:otherwise>
        <func:result select="false()" />
      </xsl:otherwise>
    </xsl:choose>
  </func:function>

  <func:function name="acrn:is-rdt-supported">
    <xsl:variable name="rdt_resource" select="substring-before(substring-after(//CLOS_INFO, 'rdt resources supported:'), 'rdt resource clos max:')" />
    <xsl:variable name="rdt_res_clos_max" select="substring-before(substring-after(//CLOS_INFO, 'rdt resource clos max:'), 'rdt resource mask max:')" />
    <xsl:choose>
      <xsl:when test="$rdt_resource and $rdt_res_clos_max">
        <func:result select="true()" />
      </xsl:when>
      <xsl:otherwise>
        <func:result select="false()" />
      </xsl:otherwise>
    </xsl:choose>
  </func:function>

  <func:function name="acrn:get-common-clos-max">
    <xsl:param name="min" />
    <xsl:param name="rdt_resource" />
    <xsl:param name="rdt_res_clos_max" />
    <xsl:variable name="resource">
      <xsl:choose>
        <xsl:when test="contains($rdt_resource, ',')">
          <xsl:value-of select="substring-before($rdt_resource, ',')" />
        </xsl:when>
        <xsl:otherwise>
          <xsl:value-of select="$rdt_resource" />
        </xsl:otherwise>
      </xsl:choose>
    </xsl:variable>
    <xsl:variable name="res_clos_max">
      <xsl:choose>
        <xsl:when test="contains($rdt_res_clos_max, ',')">
          <xsl:value-of select="number(substring-before($rdt_res_clos_max, ','))" />
        </xsl:when>
        <xsl:otherwise>
          <xsl:value-of select="number($rdt_res_clos_max)" />
        </xsl:otherwise>
      </xsl:choose>
    </xsl:variable>
    <xsl:variable name="cur_res_clos_max">
      <xsl:choose>
        <xsl:when test="acrn:is-rdt-enabled() and not(acrn:is-cdp-enabled())">
          <xsl:value-of select="$res_clos_max" />
        </xsl:when>
        <xsl:when test="acrn:is-rdt-enabled() and acrn:is-cdp-enabled()">
          <xsl:choose>
            <xsl:when test="$resource = 'MBA'">
              <xsl:value-of select="$res_clos_max" />
            </xsl:when>
            <xsl:otherwise>
              <xsl:value-of select="$res_clos_max div 2" />
            </xsl:otherwise>
          </xsl:choose>
        </xsl:when>
        <xsl:otherwise>
          <xsl:value-of select="0" />
        </xsl:otherwise>
      </xsl:choose>
    </xsl:variable>
    <func:result>
      <xsl:choose>
        <xsl:when test="substring-after($rdt_resource, ',')">
          <xsl:variable name="new_min" select="acrn:min($min, $cur_res_clos_max)" />
          <xsl:value-of select="acrn:get-common-clos-max($new_min, substring-after($rdt_resource, ','), substring-after($rdt_res_clos_max, ','))" />
        </xsl:when>
        <xsl:otherwise>
          <xsl:value-of select="acrn:min($min, $cur_res_clos_max)" />
        </xsl:otherwise>
      </xsl:choose>
    </func:result>
  </func:function>

  <func:function name="acrn:is-sos-vm">
    <xsl:param name="vm_type" />
    <xsl:choose>
      <xsl:when test="$vm_type = 'SOS_VM'">
        <func:result select="true()" />
      </xsl:when>
      <xsl:otherwise>
        <func:result select="false()" />
      </xsl:otherwise>
    </xsl:choose>
  </func:function>

  <func:function name="acrn:is-pre-launched-vm">
    <xsl:param name="vm_type" />
    <xsl:choose>
      <xsl:when test="$vm_type = 'PRE_RT_VM'">
        <func:result select="true()" />
      </xsl:when>
      <xsl:when test="$vm_type = 'PRE_STD_VM'">
        <func:result select="true()" />
      </xsl:when>
      <xsl:when test="$vm_type = 'SAFETY_VM'">
        <func:result select="true()" />
      </xsl:when>
      <xsl:otherwise>
        <func:result select="false()" />
      </xsl:otherwise>
    </xsl:choose>
  </func:function>

  <func:function name="acrn:is-post-launched-vm">
    <xsl:param name="vm_type" />
    <xsl:choose>
      <xsl:when test="$vm_type = 'POST_STD_VM'">
        <func:result select="true()" />
      </xsl:when>
      <xsl:when test="$vm_type = 'POST_RT_VM'">
        <func:result select="true()" />
      </xsl:when>
      <xsl:when test="$vm_type = 'KATA_VM'">
        <func:result select="true()" />
      </xsl:when>
      <xsl:otherwise>
        <func:result select="false()" />
      </xsl:otherwise>
    </xsl:choose>
  </func:function>

  <func:function name="acrn:is-kata-vm">
    <xsl:param name="vm_type" />
    <xsl:choose>
      <xsl:when test="$vm_type = 'KATA_VM'">
        <func:result select="true()" />
      </xsl:when>
      <xsl:otherwise>
        <func:result select="false()" />
      </xsl:otherwise>
    </xsl:choose>
  </func:function>

  <func:function name="acrn:is-tpm-passthrough-board">
    <xsl:choose>
      <xsl:when test="//@board = 'whl-ipc-i5'">
        <func:result select="true()" />
      </xsl:when>
      <xsl:when test="//@board = 'whl-ipc-i7'">
        <func:result select="true()" />
      </xsl:when>
      <xsl:when test="//@board = 'tgl-rvp'">
        <func:result select="true()" />
      </xsl:when>
      <xsl:when test="//@board = 'ehl-crb-b'">
        <func:result select="true()" />
      </xsl:when>
      <xsl:when test="//@board = 'cfl-k700-i7'">
        <func:result select="true()" />
      </xsl:when>
      <xsl:otherwise>
        <func:result select="false()" />
      </xsl:otherwise>
    </xsl:choose>
  </func:function>

  <!-- acrn:is-vmsix-supported-device checks the given params are matched with any of the listed devices -->
  <!-- The listed devices are known that can have a virtual vmsix -->
  <!-- Each pair of vendor and identifier represents a device which can have a vmsix (virtual msix) -->
  <func:function name="acrn:is-vmsix-supported-device">
    <xsl:param name="vendor" />
    <xsl:param name="identifier" />
    <xsl:choose>
      <!-- TSN devices -->
      <xsl:when test="$vendor = '0x8086' and $identifier = '0x4b30'">
        <func:result select="1" />
      </xsl:when>
      <xsl:when test="$vendor = '0x8086' and $identifier = '0x4b31'">
        <func:result select="1" />
      </xsl:when>
      <xsl:when test="$vendor = '0x8086' and $identifier = '0x4b32'">
        <func:result select="1" />
      </xsl:when>
      <xsl:when test="$vendor = '0x8086' and $identifier = '0x4ba0'">
        <func:result select="1" />
      </xsl:when>
      <xsl:when test="$vendor = '0x8086' and $identifier = '0x4ba1'">
        <func:result select="1" />
      </xsl:when>
      <xsl:when test="$vendor = '0x8086' and $identifier = '0x4ba2'">
        <func:result select="1" />
      </xsl:when>
      <xsl:when test="$vendor = '0x8086' and $identifier = '0x4bb0'">
        <func:result select="1" />
      </xsl:when>
      <xsl:when test="$vendor = '0x8086' and $identifier = '0x4bb1'">
        <func:result select="1" />
      </xsl:when>
      <xsl:when test="$vendor = '0x8086' and $identifier = '0x4bb2'">
        <func:result select="1" />
      </xsl:when>
      <xsl:when test="$vendor = '0x8086' and $identifier = '0xa0ac'">
        <func:result select="1" />
      </xsl:when>
      <xsl:when test="$vendor = '0x8086' and $identifier = '0x43ac'">
        <func:result select="1" />
      </xsl:when>
      <xsl:when test="$vendor = '0x8086' and $identifier = '0x43a2'">
        <func:result select="1" />
      </xsl:when>
      <!-- End of TSN devices -->
      <!-- GPIO devices -->
      <xsl:when test="$vendor = '0x8086' and $identifier = '0x4b88'">
        <func:result select="1" />
      </xsl:when>
      <xsl:when test="$vendor = '0x8086' and $identifier = '0x4b89'">
        <func:result select="1" />
      </xsl:when>
      <!-- End of GPIO devices -->
      <xsl:otherwise>
        <func:result select="0" />
      </xsl:otherwise>
    </xsl:choose>
  </func:function>

  <func:function name="acrn:get-vbdf">
    <xsl:param name="vmid" />
    <xsl:param name="name" />
    <xsl:variable name="bus" select="acrn:initializer('b', concat(//vm[@id = $vmid]/device[@name = $name]/bus, 'U'), '')" />
    <xsl:variable name="dev" select="acrn:initializer('d', concat(//vm[@id = $vmid]/device[@name = $name]/dev, 'U'), '')" />
    <xsl:variable name="func" select="acrn:initializer('f', concat(//vm[@id = $vmid]/device[@name = $name]/func, 'U'), '')" />
    <func:result select="concat('{', $bus, $dev, $func, '}')" />
  </func:function>

  <func:function name="acrn:get-pbdf">
    <xsl:param name="pci_dev" />
    <xsl:variable name="bus" select="acrn:initializer('b', concat('0x', translate(substring-before($pci_dev, ':'), $lowercase, $uppercase), 'U'), '')" />
    <xsl:variable name="dev" select="acrn:initializer('d', concat('0x', translate(substring-before(substring-after($pci_dev, ':'), '.'), $lowercase, $uppercase), 'U'), '')" />
    <xsl:variable name="func" select="acrn:initializer('f', concat('0x0', translate(substring-after(substring-before($pci_dev, ' '), '.'), $lowercase, $uppercase), 'U'), '')" />
    <func:result select="concat('{', $bus, $dev, $func, '}')" />
  </func:function>

  <func:function name="acrn:ptdev-name-suffix">
    <xsl:param name="pci_dev" />
    <xsl:variable name="bus" select="translate(substring-before($pci_dev, ':'), $lowercase, $uppercase)" />
    <xsl:variable name="dev" select="translate(substring-before(substring-after($pci_dev, ':'), '.'), $lowercase, $uppercase)" />
    <xsl:variable name="func" select="translate(substring-after(substring-before($pci_dev, ' '), '.'), $lowercase, $uppercase)" />
    <func:result select="concat('0X', $bus, '_0X', $dev, '000', $func)" />
  </func:function>
  <!-- End of scenario-specific functions-->

</xsl:stylesheet>
