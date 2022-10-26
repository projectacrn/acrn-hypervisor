<?xml version="1.0" encoding="utf-8"?>
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
                xmlns:xs="http://www.w3.org/2001/XMLSchema"
                xmlns:acrn="https://projectacrn.org"
                xmlns:func="http://exslt.org/functions"
                extension-element-prefixes="func"
                version="1.0">
  <xsl:output method="text"/>
  <xsl:variable name="newline" select="'&#10;'"/>
  <xsl:variable name="section_adornment" select="'#*=-%+@`'"/>
  <xsl:variable name="vLower" select="'abcdefghijklmnopqrstuvwxyz'"/>
  <xsl:variable name="vUpper" select="'ABCDEFGHIJKLMNOPQRSTUVWXYZ'"/>
  <!-- Default is to not show hidden options (acrn:views='') overridded by passing - -paramstring showHidden 'y' to xsltproc -->
  <xsl:param name="showHidden" select="n" />
  <!-- xslt script to autogenerate config option documentation -->
  <!-- Get things started with the ACRNConfigType element -->
  <xsl:template match="/xs:schema">
    <xsl:apply-templates select="xs:complexType[@name='ACRNConfigType']">
      <xsl:with-param name="level" select="2"/>
      <xsl:with-param name="prefix" select="''"/>
    </xsl:apply-templates>
  </xsl:template>
  <!-- Walk through all the ACRNConfigType element's children -->
  <xsl:template match="xs:complexType[@name='ACRNConfigType']">
    <xsl:param name="level"/>
    <xsl:param name="prefix"/>
    <xsl:apply-templates select="descendant::xs:element">
      <xsl:with-param name="level" select="$level"/>
      <xsl:with-param name="prefix" select="$prefix"/>
      <xsl:with-param name="views-of-parent" select="'basic, advanced'"/>
      <xsl:with-param name="applicable-vms-of-parent" select="''"/>
    </xsl:apply-templates>
  </xsl:template>
  <!-- ... and process all the child elements -->
  <xsl:template match="xs:element">
    <xsl:param name="level"/>
    <xsl:param name="prefix"/>
    <xsl:param name="views-of-parent"/>
    <xsl:param name="applicable-vms-of-parent"/>

    <xsl:variable name="views" select="acrn:conditional(count(xs:annotation/@acrn:views) = 1, xs:annotation/@acrn:views, $views-of-parent)"/>
    <xsl:variable name="applicable-vms" select="acrn:conditional(count(xs:annotation/@acrn:applicable-vms) = 1, xs:annotation/@acrn:applicable-vms, $applicable-vms-of-parent)"/>
    <xsl:variable name="ty" select="acrn:conditional(count(@type) = 1, @type, .//xs:alternative[1]/@type)"/>

    <!-- dxnamePure and dxname hold the element name with and without spaces converted to underscores -->
    <xsl:variable name="dxnamePure">
      <xsl:choose>
        <xsl:when test=".//descendant::xs:annotation/@acrn:title">
          <xsl:value-of select=".//descendant::xs:annotation/@acrn:title"/>
        </xsl:when>
        <xsl:otherwise>
          <xsl:value-of select="@name"/>
        </xsl:otherwise>
      </xsl:choose>
    </xsl:variable>
    <xsl:variable name="dxname">
      <xsl:value-of select="translate($dxnamePure,' ','_')"/>
    </xsl:variable>
    <!-- Only visit elements having complex types. Those having simple types are
         described as an option -->
    <xsl:choose>
      <!-- don't document elements if not viewable -->
      <xsl:when test="$views='' and $showHidden='n'"/>
      <xsl:when test="//xs:complexType[@name=$ty]">
        <!-- The section header -->
          <xsl:if test="$level &lt;= 4">
              <xsl:if test="$dxnamePure!=''">
              <xsl:call-template name="section-header">
                <xsl:with-param name="title" select="$dxnamePure"/>
                <xsl:with-param name="label" select="concat($prefix, $dxname)"/>
                <xsl:with-param name="level" select="$level"/>
              </xsl:call-template>
              <!-- Description of this menu / entry -->
              <xsl:call-template name="print-annotation">
                <xsl:with-param name="indent" select="''"/>
              </xsl:call-template>
              <xsl:value-of select="$newline"/>
              <!-- use a glossary to show a (sorted) list of config options in this section -->
              <xsl:value-of select="concat('.. glossary::',$newline,$newline)"/>
                      <!-- <xsl:value-of select="concat('.. glossary::',$newline,' :sorted:',$newline,$newline)"/> -->
                  </xsl:if>
        </xsl:if>
        <!-- Visit the complex type to generate menus and/or entries -->
        <xsl:apply-templates select="//xs:complexType[@name=$ty]">
          <xsl:with-param name="level" select="$level"/>
          <xsl:with-param name="name" select="concat($prefix, $dxname)"/>
          <xsl:with-param name="views-of-parent" select="$views"/>
          <xsl:with-param name="applicable-vms-of-parent" select="$applicable-vms"/>
        </xsl:apply-templates>
      </xsl:when>
      <xsl:otherwise>
        <xsl:if test="$level = 3">
            <!-- No longer writing a section header for elements with a simple type
           <xsl:call-template name="section-header">
             <xsl:with-param name="title" select="concat('S: ',$dxnamePure)"/>
             <xsl:with-param name="label" select="concat($prefix, $dxname)"/>
             <xsl:with-param name="level" select="$level"/>
         </xsl:call-template> -->
        </xsl:if>
        <!-- print option as a glossary item -->
        <!-- put a sortable character in front of options not available in UI -->
        <!-- <xsl:if test="xs:annotation/@acrn:views=''">
            <xsl:text> ~</xsl:text>
        </xsl:if> -->
        <xsl:value-of select="concat(' ',$dxnamePure,$newline)"/>
        <!-- print icons for basic/advanced, hypervisor/VM applicability -->
        <xsl:variable name="option-icons">
          <!-- acrn:views describes if this option appears on a "basic" or "advanced" tab, or not at all
                when the value is "".  If the acrn:views attribute is missing, then the containing parent's
                acrn:views value is used instead. Similarly for the acrn:applicable-vms attribute.  -->
          <xsl:choose>
            <xsl:when test="contains($prefix,'Hypervisor')">
              <xsl:text>|icon-hypervisor| </xsl:text>
            </xsl:when>
            <xsl:otherwise>
              <xsl:if test="$applicable-vms = '' or contains($applicable-vms, 'pre-launched')">
                <xsl:text>|icon-pre-launched-vm| </xsl:text>
              </xsl:if>
              <xsl:if test="$applicable-vms = '' or contains($applicable-vms, 'post-launched')">
                <xsl:text>|icon-post-launched-vm| </xsl:text>
              </xsl:if>
              <xsl:if test="$applicable-vms = '' or contains($applicable-vms, 'service-vm')">
                <xsl:text>|icon-service-vm| </xsl:text>
              </xsl:if>
            </xsl:otherwise>
          </xsl:choose>
          <xsl:text>/ </xsl:text>
          <xsl:if test="contains($views, 'basic')">
            <xsl:text>|icon-basic| </xsl:text>
          </xsl:if>
          <xsl:if test="contains($views, 'advanced')">
            <xsl:text>|icon-advanced| </xsl:text>
          </xsl:if>
          <xsl:if test="$views = ''">
            <xsl:text>|icon-not-availble| </xsl:text>
          </xsl:if>
        </xsl:variable>
        <xsl:if test="$option-icons!=''">
          <xsl:value-of select="concat('   ',$option-icons,$newline)"/>
        </xsl:if>
        <xsl:value-of select="$newline"/>
        <!-- Print the description, type, and occurrence requirements -->
        <xsl:text>   - </xsl:text>
        <xsl:call-template name="print-annotation">
          <xsl:with-param name="indent" select="'     '"/>
        </xsl:call-template>
        <xsl:choose>
          <xsl:when test="//xs:simpleType[@name=$ty]">
            <xsl:apply-templates select="//xs:simpleType[@name=$ty]">
              <xsl:with-param name="level" select="$level"/>
              <xsl:with-param name="prefix" select="''"/>
            </xsl:apply-templates>
          </xsl:when>
          <xsl:when test="starts-with($ty, 'xs:')">
            <xsl:text>   - </xsl:text>
            <!-- if unnamed type such as xs:string or xs:integer, print that simple type name with an uppercase first letter -->
            <xsl:value-of select="concat(translate(substring($ty, 4,1), $vLower, $vUpper), substring($ty,5),' value')"/>
            <xsl:value-of select="$newline"/>
          </xsl:when>
          <xsl:otherwise>
            <!-- element doesn't have a named type, check for an unnamed simpleType child -->
            <xsl:apply-templates select="descendant::xs:simpleType">
              <xsl:with-param name="level" select="$level"/>
              <xsl:with-param name="prefix" select="''"/>
            </xsl:apply-templates>
          </xsl:otherwise>
        </xsl:choose>
        <!-- Removed printing occurrance information
                <xsl:text>   - </xsl:text>
        <xsl:call-template name="print-occurs">
          <xsl:with-param name="name" select="$dxnamePure"/>
        </xsl:call-template> -->
        <xsl:value-of select="$newline"/>
      </xsl:otherwise>
    </xsl:choose>
  </xsl:template>
  <xsl:template match="xs:complexType">
    <xsl:param name="level"/>
    <xsl:param name="name"/>
    <xsl:param name="views-of-parent"/>
    <xsl:param name="applicable-vms-of-parent"/>

        <!-- Visit the sub-menus -->
    <xsl:variable name="newLevel">
      <xsl:choose>
        <xsl:when test=".//descendant::xs:annotation/@acrn:title=''">
          <xsl:value-of select="$level"/>
        </xsl:when>
        <xsl:otherwise><xsl:value-of select="$level + 1"/></xsl:otherwise>
      </xsl:choose>
    </xsl:variable>
    <xsl:apply-templates select="descendant::xs:element">
      <xsl:with-param name="level" select="$newLevel"/>
      <xsl:with-param name="prefix" select="concat($name, '.')"/>
      <xsl:with-param name="views-of-parent" select="$views-of-parent"/>
      <xsl:with-param name="applicable-vms-of-parent" select="$applicable-vms-of-parent"/>
    </xsl:apply-templates>
  </xsl:template>
  <xsl:template match="xs:simpleType">
    <xsl:text>   - </xsl:text>
    <xsl:choose>
      <xsl:when test="xs:annotation/xs:documentation">
        <xsl:call-template name="print-annotation">
          <xsl:with-param name="indent" select="'     '"/>
        </xsl:call-template>
      </xsl:when>
      <xsl:when test="starts-with(xs:restriction/@base, 'xs:')">
        <xsl:variable name="ty" select="xs:restriction/@base"/>
        <xsl:value-of select="concat(translate(substring($ty, 4,1), $vLower, $vUpper), substring($ty,5),' value')"/>
        <xsl:value-of select="$newline"/>
      </xsl:when>
      <xsl:otherwise>
        <xsl:text>No type annotation found</xsl:text>
        <xsl:value-of select="$newline"/>
      </xsl:otherwise>
    </xsl:choose>
  </xsl:template>
  <xsl:template name="print-occurs">
    <xsl:param name="name"/>
    <!-- use the min/maxOccurs data to figure out if this is an optional
       item, and how many occurrences are allowed -->
    <xsl:variable name="min">
      <xsl:choose>
        <xsl:when test="@minOccurs">
          <xsl:value-of select="@minOccurs"/>
        </xsl:when>
        <xsl:otherwise>1</xsl:otherwise>
      </xsl:choose>
    </xsl:variable>
    <xsl:variable name="max">
      <xsl:choose>
        <xsl:when test="@maxOccurs">
          <xsl:value-of select="@maxOccurs"/>
        </xsl:when>
        <xsl:otherwise>1</xsl:otherwise>
      </xsl:choose>
    </xsl:variable>
    <xsl:text>The ``</xsl:text>
    <xsl:value-of select="$name"/>
    <xsl:text>`` option is </xsl:text>
    <xsl:choose>
      <xsl:when test="$min = 0">
        <xsl:text>optional</xsl:text>
      </xsl:when>
      <xsl:otherwise>
        <xsl:text>required</xsl:text>
      </xsl:otherwise>
    </xsl:choose>
    <xsl:text> (</xsl:text>
    <xsl:choose>
      <xsl:when test="($min = $max) or ($min = 0)">
        <xsl:value-of select="$max"/>
        <xsl:text> occurrence</xsl:text>
        <xsl:if test="$max &gt;  1 or $max='unbounded'">
          <xsl:text>s</xsl:text>
        </xsl:if>
      </xsl:when>
      <xsl:otherwise>
        <xsl:value-of select="$min"/>
        <xsl:text> to </xsl:text>
        <xsl:value-of select="$max"/>
        <xsl:text> occurrences</xsl:text>
      </xsl:otherwise>
    </xsl:choose>
    <xsl:text>).</xsl:text>
    <xsl:value-of select="$newline"/>
  </xsl:template>
  <xsl:template name="print-annotation">
    <xsl:param name="indent"/>
    <!-- append an (Optional) annotation if minOccurs=0) -->
    <xsl:variable name="optional">
      <xsl:choose>
        <xsl:when test="@minOccurs">
          <xsl:if test="@minOccurs = 0">
            <xsl:text> *(Optional)*</xsl:text>
          </xsl:if>
          <xsl:if test="@minOccurs != 0">
            <xsl:text> *(Required)**</xsl:text>
          </xsl:if>
        </xsl:when>
        <!-- could show (Required) here instead, but it seemed too noisy -->
        <xsl:otherwise>
          <xsl:text/>
        </xsl:otherwise>
      </xsl:choose>
    </xsl:variable>
    <!-- append a default value annotation if default was defined -->
    <xsl:variable name="default">
      <xsl:choose>
        <xsl:when test="@default">
          <xsl:value-of select="concat(' (Default value is ``', @default, '``)')"/>
        </xsl:when>
        <xsl:otherwise>
          <xsl:text/>
        </xsl:otherwise>
      </xsl:choose>
    </xsl:variable>
    <xsl:choose>
      <xsl:when test="xs:annotation">
        <xsl:call-template name="printIndented">
          <xsl:with-param name="text" select="concat(xs:annotation/xs:documentation[1], $default)"/>
          <xsl:with-param name="indent" select="$indent"/>
        </xsl:call-template>
        <!-- append a second xs:annotation if found.  This allows more content for the documentation that
              won't be included in the Configurator tooltip -->
        <xsl:call-template name="printIndented">
          <xsl:with-param name="text" select="concat($newline, xs:annotation/xs:documentation[2])"/>
          <xsl:with-param name="indent" select="$indent"/>
        </xsl:call-template>
      </xsl:when>
      <xsl:otherwise>
        <!-- <xsl:text>&lt;description is missing &gt;</xsl:text> -->
        <xsl:value-of select="$newline"/>
      </xsl:otherwise>
    </xsl:choose>
  </xsl:template>
  <!--
      Library Routines
  -->
  <xsl:template name="repeat">
    <xsl:param name="string"/>
    <xsl:param name="times"/>
    <xsl:if test="$times &gt; 0">
      <xsl:value-of select="$string"/>
      <xsl:call-template name="repeat">
        <xsl:with-param name="string" select="$string"/>
        <xsl:with-param name="times" select="$times - 1"/>
      </xsl:call-template>
    </xsl:if>
  </xsl:template>
  <xsl:template name="section-header">
    <xsl:param name="title"/>
    <xsl:param name="label"/>
    <xsl:param name="level"/>
    <xsl:if test="$label != ''">
      <xsl:value-of select="concat($newline, '.. _', $label, ':', $newline, $newline)"/>
    </xsl:if>
    <xsl:value-of select="concat($title, $newline)"/>
    <xsl:call-template name="repeat">
      <xsl:with-param name="string">
        <xsl:value-of select="substring($section_adornment, $level, 1)"/>
      </xsl:with-param>
      <xsl:with-param name="times" select="string-length($title)"/>
    </xsl:call-template>
    <xsl:value-of select="concat($newline, $newline)"/>
  </xsl:template>
  <xsl:template name="printIndented">
    <xsl:param name="text"/>
    <xsl:param name="indent"/>
    <!-- Handle multi-line documentation text and indent it properly for
       the bullet list display we're using for node descriptions (but not for
       heading descriptions -->
    <xsl:if test="$text">
      <xsl:variable name="thisLine" select="substring-before($text, $newline)"/>
      <xsl:variable name="nextLine" select="substring-after($text, $newline)"/>
      <xsl:choose>
        <xsl:when test="$thisLine or $nextLine">
          <!-- $text contains at least one newline, and there's more coming so print it -->
          <xsl:value-of select="concat($thisLine, $newline)"/>
          <!-- watch for two newlines in a row and avoid writing the indent -->
          <xsl:if test="substring-before(concat($nextLine, $newline), $newline)">
            <xsl:value-of select="$indent"/>
          </xsl:if>
          <!-- and recurse to process the rest -->
          <xsl:call-template name="printIndented">
            <xsl:with-param name="text" select="$nextLine"/>
            <xsl:with-param name="indent" select="$indent"/>
          </xsl:call-template>
        </xsl:when>
        <xsl:otherwise>
          <xsl:value-of select="concat($text, $newline)"/>
        </xsl:otherwise>
      </xsl:choose>
    </xsl:if>
  </xsl:template>
  <func:function name="acrn:conditional">
    <xsl:param name="cond"/>
    <xsl:param name="a"/>
    <xsl:param name="b"/>
    <xsl:choose>
      <xsl:when test="$cond">
        <func:result select="$a"/>
      </xsl:when>
      <xsl:otherwise>
        <func:result select="$b"/>
      </xsl:otherwise>
    </xsl:choose>
  </func:function>
</xsl:stylesheet>
