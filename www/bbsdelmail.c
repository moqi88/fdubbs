#include "libweb.h"

int main() { 
        FILE *fp; 
        struct fileheader f; 
        char path[80], file[80], *id; 
        int num=0; 
        init_all(); 
		printf("<b>ɾ���ʼ� �� %s </b><br>\n",BBSNAME);
		printpretable_lite();
        if(loginok == 0) http_fatal("����δ��¼"); 
        id=currentuser.userid; 
        strlcpy(file, getparm("file"), 32); 
        if(strncmp(file, "M.", 2) || strstr(file, "..")) http_fatal("����Ĳ���"); 
        sprintf(path, "mail/%c/%s/.DIR", toupper(id[0]), id); 
        fp=fopen(path, "r"); 
        if(fp==0) http_fatal("����Ĳ���2"); 
        while(1) { 
                if(fread(&f, sizeof(f), 1, fp)<=0) break; 
                num++; 
                if(!strcmp(f.filename, file)) { 
                        fclose(fp); 
                        del_record(path, sizeof(struct fileheader), num-1); 

                        sprintf(path, "mail/%c/%s/%s", toupper(id[0]), id, f.filename); 
                        unlink(path); 
                        printf("�ż���ɾ��.<br><a href=bbsmail>���������ż��б�</a>\n"); 
                        http_quit(); 
                } 
        } 
        fclose(fp); 
        http_fatal("�ż�������, �޷�ɾ��"); 
} 