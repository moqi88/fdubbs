#include "BBSLIB.inc"

int main() {
	init_all();
	printf("<b>���� �� %s </b><br>\n",BBSNAME);
	printpretable_lite();
	if(!loginok) http_fatal("�Ҵҹ��Ͳ��ܽ��д˲���, ���ȵ�¼");
	if(!(currentuser.userlevel & PERM_CLOAK)) http_fatal("����Ĳ���");
	if(u_info->invisible) {
		u_info->invisible=0;
		printf("����״̬�Ѿ�ֹͣ��.");
	} else {
		u_info->invisible=1;
		printf("����״̬�Ѿ���ʼ��.");
	}
}