#include "BBSLIB.inc"

char *defines[] = {
        "�������ر�ʱ���ú��Ѻ���",     /* DEF_FRIENDCALL */
        "���������˵�ѶϢ",             /* DEF_ALLMSG */
        "���ܺ��ѵ�ѶϢ",               /* DEF_FRIENDMSG */
        "�յ�ѶϢ��������",             /* DEF_SOUNDMSG */
        "ʹ�ò�ɫ",                     /* DEF_COLOR */
        "��ʾ�����",                 /* DEF_ACBOARD */
        "��ʾѡ����ѶϢ��",             /* DEF_ENDLINE */
        "�༭ʱ��ʾ״̬��",             /* DEF_EDITMSG */
        "ѶϢ������һ��/����ģʽ",      /* DEF_NOTMSGFRIEND */
        "ѡ������һ��/����ģʽ",        /* DEF_NORMALSCR */
        "������������ New ��ʾ",        /* DEF_NEWPOST */
        "�Ķ������Ƿ�ʹ���ƾ�ѡ��",     /* DEF_CIRCLE */
        "�Ķ������α�ͣ춵�һƪδ��",   /* DEF_FIRSTNEW */
        "��վʱ��ʾ��������",           /* DEF_LOGFRIEND */
        "������վ֪ͨ",                 /* DEF_LOGINFROM */
        "�ۿ����԰�",                   /* DEF_NOTEPAD*/
        "��Ҫ�ͳ���վ֪ͨ������",       /* DEF_NOLOGINSEND */
        "����ʽ����",                   /* DEF_THESIS */
        "�յ�ѶϢ�Ⱥ��Ӧ�����",       /* DEF_MSGGETKEY */
        "��վʱ�ۿ���վ�˴�ͼ",         /* DEF_GRAPH */
        "��վʱ�ۿ�ʮ�����а�",         /* DEF_TOP10 */
        "ʹ������ǩ����",               /* DEF_RANDSIGN */
        "��ʾ����",                     /* DEF_S_HOROSCOPE */
        "����ʹ����ɫ����ʾ�Ա�",       /* DEF_COLOREDSEX */
        "ȱʡת������Ϊվ������",       /* DEF_FMAIL */
         NULL
};

int main() {
	int i, perm=1, type;
	init_all();
	type=atoi(getparm("type"));
	printf("%s �� %s �޸ĸ��˲��� \n", currentuser.userid, BBSNAME);
	if(!loginok) http_fatal("�Ҵҹ��Ͳ����趨����");
	if(type) return read_form();
	printf("<center>");
	printpretable();
	printf("<form action=bbsparm?type=1 method=post>\n");
	printf("<table width=610>\n");
	for(i=0; defines[i]; i++) {
		char *ptr="";
		if(i%2==0) printf("<tr>\n");
		if(currentuser.userdefine & perm) ptr=" checked";
		printf("<td><input type=checkbox name=perm%d%s><td>%s", i, ptr, defines[i]);
		perm=perm*2;
	}
	printf("</table>");
	printf("<input type=submit value=ȷ���޸�></form><br>���ϲ���������telnet��ʽ�²�������\n");
	printposttable();
	printf("</center>");
	http_quit();
}

int read_form() {
	int i, perm=1, def=0;
	char var[100];
	for(i=0; i<32; i++) {
		sprintf(var, "perm%d", i);
		if(strlen(getparm(var))==2) def+=perm;
		perm=perm*2;
	}
	currentuser.userdefine=def;
	save_user_data(&currentuser);
	printf("���˲������óɹ�.<br><a href=bbsparm>���ظ��˲�������ѡ��</a>");
}