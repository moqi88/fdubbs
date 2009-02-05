<?xml version="1.0" encoding="gb2312"?>
<xsl:stylesheet version="2.0"
xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
xmlns="http://www.w3.org/1999/xhtml">
	<xsl:template match="/">
		<html>
			<head>
				<title>�����Ƽ�����</title>
				<meta http-equiv="content-type" content="text/html; charset=gb2312" />
				<link>
					<xsl:attribute name="rel">stylesheet</xsl:attribute>
					<xsl:attribute name="type">text/css</xsl:attribute>
					<xsl:attribute name="href">/css/bbs<xsl:value-of select="bbssec/style" />.css</xsl:attribute>
				</link>
				<link rel="stylesheet" type="text/css" href="/css/bbssec.css" />
			</head>
		</html>
		<body>
			<img src="/images/secbanner.jpg" />
			<xsl:for-each select="bbssec/sector">
				<div class="pt9lc">
					<hr />
					<span><xsl:value-of select="./name" /></span>
					<xsl:element name="a">
						<xsl:attribute name="href">bbsboa?<xsl:value-of select="./name" />
						</xsl:attribute>
						<xsl:value-of select="./title" />
					</xsl:element>
					<ul class="pt9dc">
						<xsl:for-each select="./board">
							<li class="board">
								<img src="/images/types/folder.gif" align="absmiddle" />
								<xsl:element name="a">
									<xsl:attribute name="href">bbsdoc?board=<xsl:value-of select="./url" />
									</xsl:attribute>
									<xsl:value-of select="./title" />
								</xsl:element>
							</li>
						</xsl:for-each>
					</ul>
				</div>
			</xsl:for-each>
		</body>
	</xsl:template>
</xsl:stylesheet>