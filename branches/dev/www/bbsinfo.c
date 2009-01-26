#include "BBSLIB.inc"

int main() {
	int n, type;
  	init_all();

	/* added by roly  2002.05.10 ȥ��cache */
	printf("<meta http-equiv=\"pragma\" content=\"no-cache\">");
	/* add end */

	if(!loginok) http_fatal("����δ��¼");
	type=atoi(getparm("type"));
	printf("<b>�û��������� �� %s </b><br>\n", BBSNAME);
	printpretable_lite();
	if(type!=0) {
		check_info();
		http_quit();
	}
 	printf("<form action=bbsinfo?type=1 method=post>");
  	printf("�����ʺ�: %s<br>\n", currentuser.userid);
  	printf("�����ǳ�: <input type=text name=nick value='%s' size=24 maxlength=30><br>\n",
		currentuser.username);
  	printf("��������: %d ƪ<br>\n", currentuser.numposts);
  	printf("�ż�����: %d ��<br>\n", currentuser.nummails);
  	printf("��վ����: %d ��<br>\n", currentuser.numlogins);
  	printf("��վʱ��: %d ����<br>\n", currentuser.stay/60);
  	printf("��ʵ����: <input type=text name=realname value='%s' size=16 maxlength=16><br>\n",
	 	currentuser.realname);
  	printf("��ס��ַ: <input type=text name=address value='%s' size=40 maxlength=40><br>\n",
 		currentuser.address);
//  	printf("�ʺŽ���: %s<br>", Ctime(currentuser.firstlogin));
//  	printf("�������: %s<br>", Ctime(currentuser.lastlogin));
//modified by iamfat 2002.08.01
  	printf("�ʺŽ���: %s<br>", cn_Ctime(currentuser.firstlogin));
  	printf("�������: %s<br>", cn_Ctime(currentuser.lastlogin));
  	printf("��Դ��ַ: %s<br>", currentuser.lasthost);
//  	printf("�����ʼ�: <input type=text name=email value='%s' size=32 maxlength=32><br>\n", 
//		currentuser.email);
  	printf("��������: <input type=text name=year value=%d size=4 maxlength=4>��", 
		currentuser.birthyear+1900);
  	printf("<input type=text name=month value=%d size=2 maxlength=2>��", 
		currentuser.birthmonth);
  	printf("<input type=text name=day value=%d size=2 maxlength=2>��<br>\n", 
		currentuser.birthday);
  	printf("�û��Ա�: ");
    	printf("��<input type=radio value=M name=gender %s>", 
		currentuser.gender=='M' ? "checked" : "");
    	printf("Ů<input type=radio value=F name=gender %s><br>",
		currentuser.gender=='F' ? "checked" : "");
  	printf("<br><input type=submit value=ȷ��>   <input type=reset value=��ԭ>\n");
  	printf("</form>");
	printposttable_lite();
	http_quit();
}

int check_info() {
  	int m, n;
  	char buf[256];
    	strsncpy(buf, getparm("nick"), 30);
    	for(m=0; m<strlen(buf); m++) if(buf[m]<32 && buf[m]>0 || buf[m]==-1) buf[m]=' ';
    	if(strlen(buf)>1) {
		strcpy(currentuser.username, buf);
	} else {
		printf("����: �ǳ�̫��!<br>\n");
	}
    	strsncpy(buf, getparm("realname"), 9);
    	if(strlen(buf)>1) {
		strcpy(currentuser.realname, buf); 
	} else {
		printf("����: ��ʵ����̫��!<br>\n");
	}
    	strsncpy(buf, getparm("address"), 40);
    	if(strlen(buf)>8) {
		strcpy(currentuser.address, buf);
	} else {
		printf("����: ��ס��ַ̫��!<br>\n");
	}
    	strsncpy(buf, getparm("year"), 5);
    	if(atoi(buf)>1910 && atoi(buf)<1998) {
		currentuser.birthyear=atoi(buf)-1900;
	} else {
		printf("����: ����ĳ������!<br>\n");
	}
    	strsncpy(buf, getparm("month"), 3);
    	if(atoi(buf)>0 && atoi(buf)<=12) {
		currentuser.birthmonth=atoi(buf);
	} else {
		printf("����: ����ĳ����·�!<br>\n");
	}
    	strsncpy(buf, getparm("day"), 3);
    	if(atoi(buf)>0 && atoi(buf)<=31) {
		currentuser.birthday=atoi(buf);
	} else {
		printf("����: ����ĳ�������!<br>\n");
	}
    	strsncpy(buf, getparm("gender"), 2);
    	if(!strcasecmp(buf, "F")) currentuser.gender='F';
    	if(!strcasecmp(buf, "M")) currentuser.gender='M';
    	save_user_data(&currentuser);
    	printf("[%s] ���������޸ĳɹ�.", currentuser.userid);
}