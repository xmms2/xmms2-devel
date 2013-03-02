<?xml version="1.0" encoding="UTF-8" ?>

<xsl:stylesheet version="1.0"
				xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
				xmlns="http://www.w3.org/1999/xhtml"
				xmlns:xmms="https://xmms2.org/ipc.xsd">

  <xsl:output method="xml"
              doctype-public="-//W3C//DTD XHTML 1.0 Strict//EN"
              doctype-system="http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd"
              encoding="UTF-8"
              indent="yes"/>

  <xsl:template match="/">
    <html>
      <head>
        <title>XMMS2 Protocol</title>
        <style type="text/css">
          body {
            font-size: 0.8em;
            font-family: 'Lucida Sans Unicode', 'Lucida Grande', sans-serif;
          }
          a:link {
            text-decoration: none;
            color: black;
          }
          a:visited {
            text-decoration: none;
            color: black;
          }
          a:hover {
            text-decoration: underline;
            color: black;
          }
          a:active {
            text-decoration: underline;
            color: black;
          }
          .xmms-object {
            border: 1px solid black;
          }
          .xmms-object .name {
            padding: 5px;
            font-size: 110%;
            text-transform: capitalize;
            color: white;
            background-color: black;
            border-top: 1px solid black;
            border-bottom: 1px solid black;
          }
          .xmms-object .name a {
            color: white;
          }
          .xmms-object .call .name {
            font-size: 105%;
            color: black;
            text-transform: none;
            background-color: lightgrey;
          }
          .xmms-object .call .name a {
            color: black;
          }
          .xmms-object .call .documentation {
            padding: 5px;
          }
          .parameters {
            width: 100%;
            border-collapse:collapse;
          }
          .parameter {
            vertical-align: top;
          }
          .parameter-name {
            text-align: right;
            width: 120px;
            border: 1px solid lightgrey;
            padding: 3px;
          }
          .parameter-documentation {
            border: 1px solid lightgrey;
            padding: 3px;
          }
        </style>
      </head>
      <body>
        <xsl:apply-templates/>
      </body>
    </html>
  </xsl:template>

  <xsl:template name="documentation">
    <xsl:element name="a">
      <xsl:attribute name="id"><xsl:value-of select="parent::xmms:object/xmms:name"/>_<xsl:value-of select="xmms:name"/></xsl:attribute>
    </xsl:element>
    <div class="call">
      <div class="name">
        <xsl:value-of select="name()"/><xsl:text>::</xsl:text>
        <xsl:element name="a"><xsl:attribute name="href">#<xsl:value-of select="parent::xmms:object/xmms:name"/>_<xsl:value-of select="xmms:name"/></xsl:attribute>
          <xsl:value-of select="xmms:name"/>
        </xsl:element>
        <xsl:text>(</xsl:text>
        <xsl:for-each select="xmms:argument">
          <xsl:value-of select="xmms:name"/>
          <xsl:if test="not(position()=last())">
            <xsl:text>, </xsl:text>
          </xsl:if>
        </xsl:for-each>
        <xsl:text>) -> result</xsl:text>
      </div>
      <div class="documentation">
        <xsl:value-of select="xmms:documentation"/>
        <br/>
        <br/>
        <xsl:if test="xmms:argument">
          <table class="parameters" cellspacing="0">
            <xsl:for-each select="xmms:argument">
              <tr class="parameter">
                <td class="parameter-name"><xsl:value-of select="xmms:name"/></td><td class="parameter-documentation"><xsl:value-of select="xmms:documentation/text()"/></td>
              </tr>
            </xsl:for-each>
          </table>
          <br/>
        </xsl:if>
        <xsl:if test="xmms:return_value">
          <xsl:text>Returns: </xsl:text><xsl:value-of select="xmms:return_value/xmms:documentation"/>
        </xsl:if>
      </div>
    </div>
  </xsl:template>

  <xsl:template match="xmms:ipc">
    <xsl:for-each select="xmms:object">
      <div class="xmms-object">
        <xsl:element name="a">
		  <xsl:attribute name="id"><xsl:value-of select="xmms:name"/></xsl:attribute>
		</xsl:element>
        <div class="name">
          <xsl:element name="a">
            <xsl:attribute name="href">#<xsl:value-of select="xmms:name"/></xsl:attribute>
            <xsl:value-of select="translate(xmms:name, '_', ' ')"/>
          </xsl:element>
        </div>
        <xsl:for-each select="xmms:method">
          <xsl:sort select="xmms:name"/>
          <xsl:call-template name="documentation"/>
        </xsl:for-each>
        <xsl:for-each select="xmms:broadcast">
          <xsl:sort select="xmms:name"/>
          <xsl:call-template name="documentation"/>
        </xsl:for-each>
        <xsl:for-each select="xmms:signal">
          <xsl:sort select="xmms:name"/>
          <xsl:call-template name="documentation"/>
        </xsl:for-each>
      </div>
    </xsl:for-each>
  </xsl:template>
</xsl:stylesheet>
