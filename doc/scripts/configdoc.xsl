<?xml version="1.0" encoding="utf-8"?>

<xsl:stylesheet
    version="1.0"
    xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
    xmlns:xs="http://www.w3.org/2001/XMLSchema">
  <xsl:output method="text"/>

  <xsl:variable name="newline" select="'&#xa;'"/>
  <xsl:variable name="section_adornment" select="'#*=-%+@`'"/>

  <xsl:variable name="vLower" select= "'abcdefghijklmnopqrstuvwxyz'"/>
  <xsl:variable name="vUpper" select= "'ABCDEFGHIJKLMNOPQRSTUVWXYZ'"/>

  <!--
      Visitors of XSD elements
  -->

  <xsl:template match="/xs:schema">

    <xsl:apply-templates select="xs:complexType[@name='ACRNConfigType']">
      <xsl:with-param name="level" select="2"/>
      <xsl:with-param name="prefix" select="''"/>
    </xsl:apply-templates>

  </xsl:template>

  <xsl:template match="xs:complexType[@name='ACRNConfigType']">
    <xsl:param name="level"/>
    <xsl:param name="prefix"/>

    <xsl:apply-templates select="descendant::xs:element">
      <xsl:with-param name="level" select="$level"/>
      <xsl:with-param name="prefix" select="$prefix"/>
    </xsl:apply-templates>
  </xsl:template>

  <xsl:template match="xs:element">
    <xsl:param name="level"/>
    <xsl:param name="prefix"/>

    <xsl:variable name="ty" select="@type"/>

    <!-- Only visit elements having complex types. Those having simple types are
         described as an option.. -->
    <xsl:choose>
      <xsl:when test="//xs:complexType[@name=$ty]">
<!-- for top level sections (level 2) put a begin/end comment for
     potential use in rst include directives -->
       <xsl:if test="$level = 2">
         <xsl:value-of select="concat('.. section-start ', $prefix, @name, $newline)"/>
       </xsl:if>
       <!-- The section header -->
       <xsl:if test="$level &lt;= 4">
         <xsl:call-template name="section-header">
           <xsl:with-param name="title" select="concat($prefix, @name)"/>
           <xsl:with-param name="label" select="concat($prefix, @name)"/>
           <xsl:with-param name="level" select="$level"/>
         </xsl:call-template>

         <!-- Description of this menu / entry -->
         <xsl:call-template name="print-annotation" >
            <xsl:with-param name="indent" select="''"/>
         </xsl:call-template>

         <xsl:value-of select="$newline"/>
<!-- Occurence requirements (removed, but save just in case)
         <xsl:call-template name="print-occurs">
           <xsl:with-param name="name" select="@name"/>
         </xsl:call-template>
-->
          <xsl:value-of select="$newline"/>
       </xsl:if>

       <!-- Visit the complex type to generate menus and/or entries -->
       <xsl:apply-templates select="//xs:complexType[@name=$ty]">
         <xsl:with-param name="level" select="$level"/>
         <xsl:with-param name="name" select="concat($prefix, @name)"/>
       </xsl:apply-templates>
<!-- for top level sections (level 2) put a begin/end comment for
     potential use in rst include directives -->
       <xsl:if test="$level = 2">
         <xsl:value-of select="concat('.. section-end ', $prefix, @name, $newline)"/>
       </xsl:if>
      </xsl:when>
      <xsl:otherwise>
       <!-- Write a section header for elements with a simple type -->
       <xsl:if test="$level = 3">
         <xsl:call-template name="section-header">
           <xsl:with-param name="title" select="concat($prefix, @name)"/>
           <xsl:with-param name="label" select="concat($prefix, @name)"/>
           <xsl:with-param name="level" select="$level"/>
         </xsl:call-template>
       </xsl:if>

       <xsl:call-template name="option-header">
         <xsl:with-param name="label" select="concat($prefix, @name)"/>
       </xsl:call-template>
       <xsl:value-of select="$newline"/>
       <!-- Print the description, type, and occurrence requirements -->
       <xsl:text>   - </xsl:text>
       <xsl:call-template name="print-annotation" >
          <xsl:with-param name="indent" select="'     '"/>
       </xsl:call-template>
       <xsl:choose>
         <xsl:when test="//xs:simpleType[@name=$ty]">
           <xsl:apply-templates select="//xs:simpleType[@name=$ty]"/>
         </xsl:when>
         <xsl:when test="starts-with($ty, 'xs:')">
           <xsl:text>   - </xsl:text>
           <xsl:value-of select="concat(translate(substring($ty, 4,1), $vLower, $vUpper), substring($ty,5))"/>
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
<!-- removing occurs info for now
       <xsl:text>   - </xsl:text>
       <xsl:call-template name="print-occurs" >
           <xsl:with-param name="name" select="@name"/>
       </xsl:call-template>
        <xsl:value-of select="$newline"/>
-->
      </xsl:otherwise>
    </xsl:choose>
  </xsl:template>

  <xsl:template match="xs:complexType">
    <xsl:param name="level"/>
    <xsl:param name="name"/>

    <!-- Visit the sub-menus -->
    <xsl:apply-templates select="descendant::xs:element">
      <xsl:with-param name="level" select="$level + 1"/>
      <xsl:with-param name="prefix" select="concat($name, '.')"/>
    </xsl:apply-templates>
  </xsl:template>

  <xsl:template match="xs:simpleType">
    <xsl:text>   - </xsl:text>
    <xsl:call-template name="print-annotation" >
       <xsl:with-param name="indent" select="'     '"/>
    </xsl:call-template>
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

    <xsl:text>The **</xsl:text>
    <xsl:value-of select="$name"/>
    <xsl:text>** option is </xsl:text>
    <xsl:choose>
      <xsl:when test="$min = 0">
        <xsl:text>optional</xsl:text>
      </xsl:when>
      <xsl:otherwise>
        <xsl:text>required</xsl:text>
      </xsl:otherwise>
    </xsl:choose>
    <xsl:text> and with </xsl:text>

    <xsl:choose>
      <xsl:when test="($min = $max) or ($min = 0)">
        <xsl:value-of select="$max"/>
        <xsl:text> occurrence</xsl:text>
        <xsl:if test="$max &gt;  1">
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
    <xsl:text>.</xsl:text>
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
        </xsl:when>
        <!-- could show (Required) here instead, but it seemed too noisy -->
        <xsl:otherwise><xsl:text></xsl:text></xsl:otherwise>
      </xsl:choose>
    </xsl:variable>
    <!-- append a default value annotation if default was defined -->
    <xsl:variable name="default">
      <xsl:choose>
        <xsl:when test="@default">
          <xsl:value-of select="concat(' (Default value is ``', @default, '``)')"/>
        </xsl:when>
        <xsl:otherwise><xsl:text></xsl:text></xsl:otherwise>
      </xsl:choose>
    </xsl:variable>

    <xsl:choose>
      <xsl:when test="xs:annotation">
         <xsl:call-template name="printIndented">
           <xsl:with-param name="text" select="concat(xs:annotation/xs:documentation, $optional, $default)"/>
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
    <xsl:if test="$times > 0">
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

  <xsl:template name="option-header">
    <xsl:param name="label"/>
    <!-- we're using the reST option directive for creating linkable
         config option sections using the :option: role.  This also
         gives us the option directive HTML formatting. -->
    <xsl:value-of select="$newline"/>
    <xsl:text>.. option:: </xsl:text>
    <xsl:value-of select="$label"/>
    <xsl:value-of select="$newline"/>
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
        <xsl:if test="substring-before(concat($nextLine, $newline), $newline)" >
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
</xsl:stylesheet>
