#include "bbs.h"
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>

#define BUFSIZE (MAXUSERS + 244)

USE_TRY;

//	ѭ��д�ַ����ļ���ֱ��д��size���ֽ�
//	origsz,bp�Ƕ���ģ�
static int safewrite(int fd, const void *buf, int size)
{
	int cc, sz = size, origsz = size;
	const void *bp = buf;

	do {
		cc = write(fd, bp, sz);
		if ((cc < 0) && (errno != EINTR)) {
			report("safewrite err!", "");
			return -1;
		}
		if (cc > 0) {
			bp += cc;
			sz -= cc;
		}
	} while (sz > 0);
	return origsz;
}

//	���ļ�filename������,����-1
//	���򷵻�filename�д�ŵļ�¼��
long get_num_records(const char *filename, const int size)
{
	struct stat st;
	if (stat(filename, &st) == -1)
		return 0;
	return (st.st_size / size);
}

//����һ����¼����СΪsize,�׵�ַΪrecord
//	�ļ���Ϊfilename
int append_record(const char *filename, const void *record, int size)
{
	int fd;
	if ((fd = open(filename, O_WRONLY | O_CREAT, 0644)) == -1) {
		report("open file error in append_record()", "");
		return -1;
	}
	FLOCK(fd, LOCK_EX);
	lseek(fd, 0, SEEK_END);
	if (safewrite(fd, record, size) == -1)
		report("apprec write err!", "");
	FLOCK(fd, LOCK_UN);
	close(fd);
	return 0;
}

//ȡ�ü�¼�ľ��,�������rptr��
//	fd���ļ���������,size��ʾ��¼�Ĵ�С,id��ʾ��¼��λ��
//	���ɹ�ʱ,����-1 ; �ɹ�ʱ,����0
static int get_record_handle(int fd, void *rptr, int size, int id)
{
	if (lseek(fd, size * (id - 1), SEEK_SET) == -1)
		return -1;
	if (read(fd, rptr, size) != size)
		return -1;
	return 0;
}

//ȡ�ü�¼,filename��ʾ�ļ���,����������get_record_handle
int get_record(char *filename, void *rptr, int size, int id)
{
	int fd;
	int ret;

	if ((fd = open(filename, O_RDONLY, 0)) == -1)
		return -1;
	ret = get_record_handle(fd, rptr, size, id);
	close(fd);
	return ret;
}

//���ļ�filename�еĵ�id-1����¼����ȡ��СΪsize,����Ϊnumber�ļ�¼��
//	���ʧ��,����-1,���δ�ܶ�ȡnumber����¼,�򷵻ض�ȡ�ļ�¼��
int get_records(const char *filename, void *rptr, int size, int id,
		int number)
{
	int fd;
	int n;
	if ((fd = open(filename, O_RDONLY, 0)) == -1)
		return -1;
	if (lseek(fd, (off_t) (size * (id - 1)), SEEK_SET) == -1) {
		close(fd);
		return 0;
	}
	if ((n = read(fd, rptr, size * number)) == -1) {
		close(fd);
		return -1;
	}
	close(fd);
	return (n / size);
}

#ifndef THREAD_C

//	����Ϊfilename�ļ�¼�ļ�ִ��fptr����
int apply_record(char *filename, APPLY_FUNC_ARG fptr, int size, void *arg,
		int applycopy, int reverse)
{
	void *buf, *buf1, *buf2 = NULL;
	int i;
	size_t file_size;
	int count;

	BBS_TRY {
		if ( safe_mmapfile( filename,
						O_RDONLY,
						PROT_READ,
						MAP_SHARED,
						&buf,//��ʼ��ַ����buf�� 
						&file_size, //����ӳ����ļ���С 
						NULL //�Էǻ��ⷽʽӳ���ļ����ڴ�
				)== 0
		) {
			BBS_RETURN(0);
		}
		count = file_size / size; //��¼����Ŀ
		if (reverse)
		buf1 = buf + (count - 1) * size;
		else
		buf1 = buf;
		for (i = 0; i < count; i++) {
			if (applycopy) {
				buf2 = malloc(size);
				memcpy(buf2, buf1, size);
			} else {
				buf2 = buf1;
			}
			if ((*fptr) (buf2, reverse ? count - i : i + 1, arg) == QUIT) {
				//ִ�к���fptr,bufΪ�������׵�ַ,argΪ��������
				end_mmapfile((void *) buf, file_size, -1); //��ֹ�ڴ�ӳ��,����û����,
				//���ڲ���Ҫ����
				if (applycopy)
				free(buf2);
				BBS_RETURN(QUIT);
			}
			if (reverse)
			buf1 -= size;
			else
			buf1 += size;
		}
	}
	BBS_CATCH {
	}
	BBS_END end_mmapfile((void *) buf, file_size, -1);

	if (applycopy)
		free(buf2);
	return 0;
}

/*---   End of Addition     ---*/
/* search_record������Ԥ���Ż�,�Լ���ϵͳ���ô���,����ٶ�. ylsdd, 2001.4.24 */
/* COMMAN : use mmap to improve search speed */
//	��filename�ļ������� �ȽϺ���Ϊfptr,��������¼Ϊfarg
//	��������ΪO(n)���Կ��ǸĽ�
int search_record(char *filename, void *rptr, int size,
		RECORD_FUNC_ARG fptr, void *farg)
{
	int i;
	void *buf, *buf1;
	size_t filesize;

	BBS_TRY {
		if (safe_mmapfile( filename, O_RDONLY, PROT_READ,
						MAP_SHARED, (void **) &buf, &filesize, NULL)
				== 0
		)
		BBS_RETURN(0);
		for (i = 0, buf1 = buf; i < filesize / size; i++, buf1 += size) {
			if ((*fptr) (farg, buf1)) {
				if (rptr)
				memcpy(rptr, buf1, size);
				end_mmapfile((void *) buf, filesize, -1);
				BBS_RETURN(i + 1);
			}
		}
	}
	BBS_CATCH {
	}
	BBS_END end_mmapfile((void *) buf, filesize, -1);

	return 0;
}

//	��filename�ļ���id����¼�滻Ϊrptr��ָ�������
int substitute_record(char *filename, void *rptr, int size, int id)
{
	/*     * add by KCN      */
	struct flock ldata;
	int retval;
	int fd;

	if ((fd = open(filename, O_WRONLY | O_CREAT, 0644)) == -1)
		return -1;
	/*
	 * change by KCN
	 * flock(fd,LOCK_EX) ;
	 */
	ldata.l_type = F_WRLCK;
	ldata.l_whence = 0;
	ldata.l_len = size;
	ldata.l_start = size * (id - 1);
	if ((retval = fcntl(fd, F_SETLKW, &ldata)) == -1) {//�Ի��ⷽʽ���ļ�
		//bbslog("user", "%s", "reclock error");
		close(fd);
		/*---	period	2000-10-20	file should be closed	---*/
		return -1;
	}
	if (lseek(fd, size * (id - 1), SEEK_SET) == -1) { //�޷����ļ���ָ��λ��
		// bbslog("user", "%s", "subrec seek err");
		/*---	period	2000-10-24	---*/
		ldata.l_type = F_UNLCK;
		fcntl(fd, F_SETLK, &ldata);
		close(fd);
		return -1;
	}
	//��infotech�޸�,ȥ��safewrite���ж�,�Ժ����ʱ����������
	//if (safewrite(fd, rptr, size) != size)
	safewrite(fd, rptr, size);
	//bbslog("user", "%s", "subrec write err");
	/*
	 * change by KCN
	 * flock(fd,LOCK_UN) ;
	 */

	ldata.l_type = F_UNLCK;
	fcntl(fd, F_SETLK, &ldata);
	close(fd);

	return 0;
}

//from smthbbs, 2003.10.26
int delete_record(char *filename, int size, int id,
		RECORD_FUNC_ARG filecheck, void *arg)
{
	int fdr;
	size_t filesize;
	void *ptr;
	int ret;

	if (id <= 0)
		return 0;
	BBS_TRY {
		if (safe_mmapfile(filename, O_RDWR, PROT_READ | PROT_WRITE,
						MAP_SHARED, &ptr, &filesize, &fdr)
				== 0
		)
		BBS_RETURN(-1);
		ret = 0;
		if (id * size> filesize) {
			ret = -2;
		} else {
			if (filecheck) { //���filecheck�����Ƿ����?
				if (!(*filecheck) (ptr + (id - 1) * size, arg)) {
					for (id = 0; id * size < filesize; id++)
					if ((*filecheck) (ptr + (id - 1) * size, arg))
					break;
					if (id * size >= filesize)
					ret = -2;
				}
			}
		}
		if (ret == 0) {
			memcpy(ptr + (id - 1) * size, ptr + id * size, filesize - size * id);
			// ����ɾ����¼�����м�¼��ǰ�ƶ�һ��λ��,
			ftruncate(fdr, filesize - size);
			// ���ļ��ض�һ����¼
		}
	}
	BBS_CATCH {
		ret = -3;
	}
	BBS_END end_mmapfile(ptr, filesize, fdr);

	return ret;
}

//�����¼
int insert_record(char *filename, int size, RECORD_FUNC_ARG filecheck,
		void *arg)
{
	char *ptr, *rptr;
	int fdr;
	size_t filesize;
	int ret;

	if (!filecheck||!arg)
		return -1;
	fdr=open(filename, O_RDWR, 0600);
	lseek(fdr, 0, SEEK_END);
	write(fdr, arg, size);

	BBS_TRY {
		if (safe_mmapfile_handle( fdr, O_RDWR, PROT_READ | PROT_WRITE,
						MAP_SHARED, (void **) &ptr, &filesize
				)
				== 0
		)
		BBS_RETURN(-1);
		ret = 0;
		rptr = ptr;
		filesize -= size;
		while(filesize) {
			if(filecheck(rptr, arg)) {
				break;
			}
			rptr += size;
			filesize-= size;
		}
		if(filesize) {
			memmove(rptr + size, rptr, filesize);
			memcpy(rptr, arg, size);
		}
	}
	BBS_CATCH {
		ret = -3;
	}
	BBS_END end_mmapfile(ptr, filesize, fdr);
	return ret;
}

#endif