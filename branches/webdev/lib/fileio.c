#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <dirent.h>
#include <unistd.h>
#include <errno.h>
#include <sys/file.h>
#include <sys/types.h>
#include <sys/stat.h>

#define        BLK_SIZ         4096

static int rm_dir();

//����Ϣmsg���뵽�ļ�fpath��,дʱ��ס�ļ�,д�����
//****	�ļ��������ǵײ��ļ�IO����,�ӿ��ٶ�
void file_append(const char *fpath, const char *msg) {
	int fd;
	if ((fd = open(fpath, O_WRONLY | O_CREAT, 0644)) >= 0) {
		flock(fd, LOCK_EX);
		lseek(fd, 0, SEEK_END);
		write(fd, msg, strlen(msg));
		flock(fd, LOCK_UN);
		close(fd);
	}
}
//��fname����,��Ϊ�����ļ�,������
int dashf(char *fname) {
	struct stat st;
	return (stat(fname, &st) == 0 && S_ISREG(st.st_mode));
}

//���fname����,��ΪĿ¼,������
int dashd(char *fname) {
	struct stat st;
	return (stat(fname, &st) == 0 && S_ISDIR(st.st_mode));
}
/* mode == O_EXCL / O_APPEND / O_TRUNC */
int part_cp(char *src, char *dst, char *mode) {
	int flag =0;
	char buf[256];
	FILE *fsrc, *fdst;

	fsrc = fopen(src, "r");
	if (fsrc == NULL)
		return 0;
	fdst = fopen(dst, mode);
	if (fdst == NULL) {
		fclose(fsrc);
		return 0;
	}
	while (fgets(buf, 256, fsrc)!=NULL) {
		if (flag==1&&!strcmp(buf, "--\n")) {
			fputs(buf, fdst);
			break;
		}
		if (flag==0&&(!strncmp(buf+2, "����: ", 6) ||!strncmp(buf,
				"[1;41;33m������: ", 18))) {
			fputs(buf, fdst);
			continue;
		}
		if (flag==0&&(buf[0]=='\0'||buf[0]=='\n'/*||!strncmp(buf+2,"����: ",6)*/
		|| !strncmp(buf, "��  ��: ", 8)||!strncmp(buf, "����վ: ", 8)
		/*|| !strncmp(buf,"[1;41;33m������: ",18)*/))
			continue;
		flag =1;
		fputs(buf, fdst);
	}
	fclose(fdst);
	fclose(fsrc);
	return 1;
}

int f_cp(char *src, char *dst, int mode) {
	int fsrc, fdst, ret;
	ret = 0;

	if ((fsrc = open(src, O_RDONLY)) >= 0) {
		ret = -1;

		if ((fdst = open(dst, O_WRONLY | O_CREAT | mode, 0600)) >= 0) {
			char pool[BLK_SIZ];
			src = pool;
			do {
				ret = read(fsrc, src, BLK_SIZ);
				if (ret <= 0)
					break;
			} while (write(fdst, src, ret) > 0);
			close(fdst);
		}
		close(fsrc);
	}
	return ret;
}

int f_ln(src, dst)
char *src, *dst;
{
	int ret;

	if ((ret = link(src, dst))!=0) {
		if (errno != EEXIST)
		ret = f_cp(src, dst, O_EXCL);
	}
	return ret;
}

int valid_fname(char *str) {
	char ch;
	while ((ch = *str++) != '\0') {
		if ((ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') || (ch
				>='0' && ch <= '9') || ch=='-' || ch=='_') {
			;
		} else {
			return 0;
		}
	}
	return 1;
}

int touchfile(char *filename) {
	int fd;
	if ((fd = open(filename, O_RDWR | O_CREAT, 0600)) > 0)
		close(fd);

	return fd;
}
/*
 Commented by Erebus 2004-11-08 
 rm file and folder
 */
int f_rm(char *fpath) {
	struct stat st;
	if (stat(fpath, &st)) //statδ�ܳɹ�
		return -1;

	if (!S_ISDIR(st.st_mode)) //����Ŀ¼,��ɾ�����ļ�
		return unlink(fpath);

	return rm_dir(fpath); //ɾ��Ŀ¼
}

/*
 Commented by Erebus 2004-11-08
 rm folder
 */

static int rm_dir(char *fpath) {
	struct stat st;
	DIR * dirp;
	struct dirent *de;
	char buf[256], *fname;
	if (!(dirp = opendir(fpath)))
		return -1;

	for (fname = buf; (*fname = *fpath) != '\0'; fname++, fpath++)
		;

	*fname++ = '/';

	readdir(dirp);
	readdir(dirp);

	while ((de = readdir(dirp)) != NULL) {
		fpath = de->d_name;
		if (*fpath) {
			strcpy(fname, fpath);
			if (!stat(buf, &st)) {
				if (S_ISDIR(st.st_mode))
					rm_dir(buf);
				else
					unlink(buf);
			}
		}
	}
	closedir(dirp);

	*--fname = '\0';
	return rmdir(buf);
}