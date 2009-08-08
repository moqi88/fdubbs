<?xml version="1.0" encoding="gb2312"?>
<xsl:stylesheet version="2.0" xmlns:xsl="http://www.w3.org/1999/XSL/Transform" xmlns="http://www.w3.org/1999/xhtml">
	<xsl:import href='misc.xsl' />
	<xsl:import href='showpost.xsl' />
	<xsl:output method='html' encoding='gb2312' />
	<xsl:template match="bbsdoc">
		<html>
			<head>
				<title><xsl:value-of select='desc' /> - ���¹⻪BBS</title>
				<meta http-equiv="content-type" content="text/html; charset=gb2312" />
				<link rel="stylesheet" type="text/css" href="/css/bbs0.css" />
			</head>
			<body>
				<xsl:if test='icon'>
					<img align='absmiddle'><xsl:attribute name='src'><xsl:value-of select='icon' /></xsl:attribute></img>
				</xsl:if>
				<xsl:choose>
					<xsl:when test='banner'>
						<img align='absmiddle'><xsl:attribute name='src'><xsl:value-of select='icon' /></xsl:attribute></img>
					</xsl:when>
					<xsl:otherwise>
						<h1><xsl:value-of select='desc' /> [<xsl:value-of select='title' />]<xsl:if test='link = "g"'> - ��ժ��</xsl:if><xsl:if test='link = "t"'> - ����ģʽ</xsl:if></h1>
					</xsl:otherwise>
				</xsl:choose>
				<strong>���� [ 
					<xsl:call-template name='splitbm'>
						<xsl:with-param name='names' select='bm' />
						<xsl:with-param name='isdir' select='@dir' />
						<xsl:with-param name='isfirst' select='1' />
					</xsl:call-template> ]  ������ [ <xsl:value-of select='total' /> ]
				</strong>
				<table width="100%" bgcolor="#ffffff">
					<tr class="pt9h">
						<th>���</th><th>���</th><th>����</th><th>����ʱ��</th><th>����</th>
					</tr>
					<xsl:for-each select='post'>
						<tr>
							<xsl:attribute name='class'>
								<xsl:if test='position() mod 2 = 1'>pt9lc</xsl:if>
								<xsl:if test='position() mod 2 = 0'>pt9dc</xsl:if>
							</xsl:attribute>
							<!-- No. -->
							<td align='right'><xsl:value-of select='position() - 1 + /bbsdoc/start' /></td>
							<!-- Mark -->
							<td align='center'><xsl:value-of select='mark' /></td>
							<!-- Author -->
							<td><strong><a>
								<xsl:attribute name='href'>qry?u=<xsl:value-of select='author' /></xsl:attribute><xsl:value-of select='author' />
							</a></strong></td>
							<!-- Time -->
							<td>
								<xsl:call-template name='timeconvert'>
									<xsl:with-param name='time' select='time' />
								</xsl:call-template>
							</td>
							<!-- Title -->
							<td width='100%'>
								<xsl:choose>
									<xsl:when test='substring(title, 1, 4) = "Re: "'>
										<img align='absmiddle' border='0' src='/images/types/reply.gif' />
										<a>
											<xsl:attribute name='href'><xsl:value-of select='/bbsdoc/link' />con?bid=<xsl:value-of select='/bbsdoc/bid' />&amp;f=<xsl:value-of select='id' /></xsl:attribute>
											<xsl:value-of select='substring(title, 5)' />
										</a>
									</xsl:when>
									<xsl:otherwise>
										<img align='absmiddle' border='0' src='/images/types/text.gif' />
										<a>
											<xsl:attribute name='href'><xsl:value-of select='/bbsdoc/link' />con?bid=<xsl:value-of select='/bbsdoc/bid' />&amp;f=<xsl:value-of select='id' /></xsl:attribute>
											<xsl:call-template name='ansi-escape'>
												<xsl:with-param name='content'><xsl:value-of select='title' /></xsl:with-param>
												<xsl:with-param name='fgcolor'>37</xsl:with-param>
												<xsl:with-param name='bgcolor'>ignore</xsl:with-param>
												<xsl:with-param name='ishl'>0</xsl:with-param>
											</xsl:call-template>
										</a>
									</xsl:otherwise>
								</xsl:choose>
							</td>
						</tr>
					</xsl:for-each>
				</table>
				<xsl:if test='start > 1'>
					<xsl:variable name='prev'>
						<xsl:choose>
							<xsl:when test='start - page &lt; 1'>1</xsl:when>
							<xsl:otherwise><xsl:value-of select='start - page' /></xsl:otherwise>
						</xsl:choose>
					</xsl:variable>
					<a><xsl:attribute name='href'><xsl:value-of select='link' />doc?bid=<xsl:value-of select='bid' />&amp;start=<xsl:value-of select='$prev' /></xsl:attribute>[ <img src='/images/button/up.gif' />��һҳ ]</a>
				</xsl:if>
				<xsl:if test='total > start + page - 1'>
					<xsl:variable name='next'><xsl:value-of select='start + page' /></xsl:variable>
					<a><xsl:attribute name='href'><xsl:value-of select='link' />doc?bid=<xsl:value-of select='bid' />&amp;start=<xsl:value-of select='$next' /></xsl:attribute>[ <img src='/images/button/down.gif' />��һҳ ]</a>
				</xsl:if>
				<a><xsl:attribute name='href'>clear?board=<xsl:value-of select='title' />&amp;start=<xsl:value-of select='start' /></xsl:attribute>[���δ��]</a>
				<xsl:if test='link != ""'><a><xsl:attribute name='href'>doc?bid=<xsl:value-of select='bid' /></xsl:attribute>[<img src='/images/button/home.gif' />һ��ģʽ]</a></xsl:if>
				<xsl:if test='link != "t"'><a><xsl:attribute name='href'>tdoc?bid=<xsl:value-of select='bid' /></xsl:attribute>[<img src='/images/button/content.gif' />����ģʽ]</a></xsl:if>
				<xsl:if test='link != "g"'><a><xsl:attribute name='href'>gdoc?bid=<xsl:value-of select='bid' /></xsl:attribute>[��ժ��]</a></xsl:if>
				<a><xsl:attribute name='href'>not?board=<xsl:value-of select='title' /></xsl:attribute>[���滭��]</a>
			</body>
		</html>
	</xsl:template>
</xsl:stylesheet>