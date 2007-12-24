/*
    Pirate Bulletin Board System
    Copyright (C) 1990, Edward Luke, lush@Athena.EE.MsState.EDU
    Eagles Bulletin Board System
    Copyright (C) 1992, Raymond Rocker, rocker@rock.b11.ingr.com
                        Guy Vega, gtvega@seabass.st.usm.edu
                        Dominic Tynes, dbtynes@seabass.st.usm.edu
    Firebird Bulletin Board System
    Copyright (C) 1996, Hsien-Tsung Chang, Smallpig.bbs@bbs.cs.ccu.edu.tw
                        Peng Piaw Foong, ppfoong@csie.ncu.edu.tw

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 1, or (at your option)
    any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
*/
/*
$Id: io.c 370 2007-05-14 11:58:08Z danielfree $
*/

#include "bbs.h"
#include "screen.h"
#ifdef AIX
#include <sys/select.h>
#endif
#include <arpa/telnet.h>

#define OBUFSIZE  (4096)
#define IBUFSIZE  (256)

#define INPUT_ACTIVE 0
#define INPUT_IDLE 1

extern int dumb_term;

//	定义输出缓冲区,及其已被使用的字节数
static char outbuf[OBUFSIZE];
static int obufsize = 0;

struct user_info uinfo;

char    inbuf[IBUFSIZE];
int     ibufsize = 0;
int     icurrchar = 0;
int     KEY_ESC_arg;

static int i_mode = INPUT_ACTIVE;
extern struct screenline *big_picture;

#ifdef ALLOWSWITCHCODE

#define BtoGtablefile "etc/b2g_table"
#define GtoBtablefile "etc/g2b_table"

unsigned char* GtoB,* BtoG;

#define GtoB_count 7614
#define BtoG_count 13973

extern int convcode;
extern void redoscr();

//	将字码转换状态取逆, 并重绘屏幕
int switch_code(){
	convcode=!convcode;
	redoscr();
}

void resolve_GbBig5Files(void)
{
        int fd;
        int i;
	BtoG =(unsigned char *)attach_shm("CONV_SHMKEY", 3013,GtoB_count*2+BtoG_count*2);
        fd = open( BtoGtablefile, O_RDONLY );
        if (fd == -1)
          for (i=0;i< BtoG_count; i++) {
                BtoG[i*2]=0xA1;
                BtoG[i*2+1]=0xF5;
          }
        else
        {
          read(fd,BtoG,BtoG_count*2);
          close(fd);
        }
        fd=open(GtoBtablefile,O_RDONLY);
        if (fd==-1)
                for (i=0;i< GtoB_count; i++) {
                        BtoG[BtoG_count*2+i*2]=0xA1;
                        BtoG[BtoG_count*2+i*2+1]=0xBC;
                        }
        else
        {
                read(fd,BtoG+BtoG_count*2,GtoB_count*2);
                close(fd);
        }
	GtoB = BtoG + BtoG_count*2;
}

//	将str字符串中的GB码汉字转换成相应的BIG5码汉字,并调用write函数输出
int write2(int	port, char *str, int len) // write gb to big
{
	register int	i, locate;
	register unsigned char	ch1, ch2, *ptr;
	
	for(i=0, ptr=str; i < len;i++){
		ch1 = (ptr+i)[0];
		if(ch1 < 0xA1 || (ch1 > 0xA9 && ch1 < 0xB0) || ch1 > 0xF7) 
			continue;
		ch2 = (ptr+i)[1];
		i ++;
		if(ch2 < 0xA0 || ch2 == 0xFF ) 
			continue;
		if((ch1 > 0xA0) && (ch1 < 0xAA)) //01～09区为符号数字
			locate = ((ch1 - 0xA1)*94 + (ch2 - 0xA1))*2;
		else //if((buf > 0xAF) && (buf < 0xF8)){ //16～87区为汉字
			locate = ((ch1 - 0xB0 + 9)*94 + (ch2 - 0xA1))*2;
		(ptr+i-1)[0] = GtoB[locate++];
		(ptr+i-1)[1] = GtoB[locate];
	}
	return write(port, str, len);
}

int read2(int port, char *str, int len) // read big from gb 
{
	/*
	 * Big-5 是一个双字节编码方案，其第一字节的值在 16 进
	 * 制的 A0～FE 之间，第二字节在 40～7E 和 A1～FE 之间。
	 * 因此，其第一字节的最高位是 1，第二字节的最高位则可
	 * 能是 1，也可能是 0。
	 */
	register unsigned char ch1,ch2, *ptr;
	register int 	locate, i=0;
	if(len == 0)  return 0;
	len = read(port, str, len);
	if( len < 1) 
		return len;

	for(i=0,ptr = str; i < len; i++) {
		ch1 = (ptr+i)[0];
		if(ch1 < 0xA1 || ch1 == 0xFF) 
			continue;
		ch2 = (ptr+i)[1];
		i ++;
		if(ch2 < 0x40 || ( ch2 > 0x7E && ch2 < 0xA1 ) || ch2 == 255)
			continue;
		if( (ch2 >= 0x40) && (ch2 <= 0x7E) )
			locate = ((ch1 - 0xA1) * 157 + (ch2 - 0x40)) * 2;
		else 
			locate = ((ch1 - 0xA1) * 157 + (ch2 - 0xA1) + 63) * 2;
		(ptr+i-1)[0] = BtoG[ locate++ ];
		(ptr+i-1)[1] = BtoG[ locate ];
	}
	return len;
}
#endif

//	超时处理函数,将非特权ID超时时踢出bbs
void hit_alarm_clock()
{
	if (HAS_PERM(PERM_NOTIMEOUT))
		return;
	if (i_mode == INPUT_IDLE) {
		clear();
		prints("Idle timeout exceeded! Booting...\n");
		kill(getpid(), SIGHUP);
	}
	i_mode = INPUT_IDLE;
	if (uinfo.mode == LOGIN)
		alarm(LOGIN_TIMEOUT);
	else
		alarm(IDLE_TIMEOUT);
}

//初始化超时时钟信号,将hit_alarm_clock函数挂在此信号处理句柄上
void init_alarm()
{
	signal(SIGALRM, hit_alarm_clock);
	alarm(IDLE_TIMEOUT);
}
#ifdef BBSD
//刷新输出缓冲区
void oflush()
{
	register int size;
	if (size = obufsize) {
#ifdef ALLOWSWITCHCODE
		if(convcode) write2(0, outbuf, size);
		else
#endif
		write(0, outbuf, size);
		obufsize = 0;
	}
}

//	把长度为len的字符串s放入缓冲区中,若缓冲区存放过多字节时,则刷新缓冲区
//		1) 若允许GB与BIG5转换,且用户使用的是BIG5码,使用write2函数输出
//		2)	否则,使用 write函数直接输出
//		3)	将新加的字符串放到缓冲区中
//	其中0是文件描述符,已经被映射到socket管道的输出
void	output(char   *s,int     len)
{
	/* Invalid if len >= OBUFSIZE */

	register int size;
	register char *data;
	size = obufsize;
	data = outbuf;
	if (size + len > OBUFSIZE) {
#ifdef ALLOWSWITCHCODE
		if(convcode) 
			write2(0, data, size);
		else
#endif
		write(0, data, size);
		size = len;
	} else {
		data += size;
		size += len;
	}
	memcpy(data, s, len);
	obufsize = size;
}

// 输出一个字符?
void ochar(register int c)
{
	register char *data;
	register int size;
	data = outbuf;
	size = obufsize;

	if (size > OBUFSIZE - 2) {	/* doin a oflush */
#ifdef ALLOWSWITCHCODE
		if(convcode) write2(0, data, size);
		else
#endif
		write(0, data, size);
		size = 0;
	}
	data[size++] = c;
	if (c == IAC) data[size++] = c;

	obufsize = size;
}
#else
//输出缓冲区中的字符
void oflush()
{
	if (obufsize)
#ifdef ALLOWSWITCHCODE
		if(convcode) write2(1, outbuf, obufsize);
		else
#endif
		write(1, outbuf, obufsize);
	obufsize = 0;
}

//输出字符串,缓冲区满时先刷新缓冲区,再把这个字符串放到缓冲区中
void output(char *s,int len)
{
	/* Invalid if len >= OBUFSIZE */

	if (obufsize + len > OBUFSIZE) {	/* doin a oflush */
#ifdef ALLOWSWITCHCODE
		if(convcode) write2(1, outbuf, obufsize);//转码输出
		else
#endif
		write(1, outbuf, obufsize);
		obufsize = 0;
	}
	memcpy(outbuf + obufsize, s, len);
	obufsize += len;
}

//输出一个字符,缓冲区满时先刷新缓冲区,再把这个字符放到缓冲区里
void ochar(int	c)
{
	if (obufsize > OBUFSIZE - 1) {	/* doin a oflush */
#ifdef ALLOWSWITCHCODE
		if(convcode) write2(1, outbuf, obufsize);
		else
#endif
		write(1, outbuf, obufsize);
		obufsize = 0;
	}
	outbuf[obufsize++] = c;
}
#endif

int     i_newfd = 0;
struct timeval i_to, *i_top = NULL;
int     (*flushf) () = NULL;

void add_io(int fd,int timeout)
{
	i_newfd = fd;
	if (timeout) {
		i_to.tv_sec = timeout;
		i_to.tv_usec = 0;
		i_top = &i_to;
	} else
		i_top = NULL;
}

//	将flushf函数指针指向函数flushfunc
void add_flush(int	(*flushfunc)())
{
	flushf = flushfunc;
}

/*
int
num_in_buf()
{
	return icurrchar - ibufsize;
}
*/
//	返回缓冲中的字符数
int num_in_buf()
{
	int n;
	if((n = icurrchar - ibufsize) < 0) 
		n=0;
	return n;
}

#ifdef BBSD
static int iac_count(char 	*current)
{
	switch (*(current + 1) & 0xff) {
	case DO:
	case DONT:
	case WILL:
	case WONT:
		return 3;
	case SB:		/* loop forever looking for the SE */
		{
			register char *look = current + 2;
			for (;;) {
				if ((*look++ & 0xff) == IAC) {
					if ((*look++ & 0xff) == SE) {
						return look - current;
					}
				}
			}
		}
	default:
		return 1;
	}
}
#endif

#ifdef BBSD
int igetch()
{
	static int trailing = 0;
	//modified by iamfat 2002.08.21
	//static int repeats = 0;
	//static time_t timestart=0;
	//static int repeatch=0;
	//modified end
	register int ch;
	register char *data;
	data = inbuf;

	for (;;) {
		if (ibufsize == icurrchar) {
			fd_set  readfds;
			struct timeval to;
			register fd_set *rx;
			register int fd, nfds;
			rx = &readfds;
			fd = i_newfd;

	igetnext:

			uinfo.idle_time = time(0);
			update_utmp();	/* 应该是需要 update 一下 :X */

			FD_ZERO(rx);
			FD_SET(0, rx);
			if (fd) {
				FD_SET(fd, rx);
				nfds = fd + 1;
			} else
				nfds = 1;
			to.tv_sec = to.tv_usec = 0;
			if ((ch = select(nfds, rx, NULL, NULL, &to)) <= 0) {
				if (flushf)
					(*flushf) ();

				if (big_picture)
					refresh();
				else
					oflush();

				FD_ZERO(rx);
				FD_SET(0, rx);
				if (fd)
					FD_SET(fd, rx);

				while ((ch = select(nfds, rx, NULL, NULL, i_top)) < 0) {
					if (errno != EINTR)
						return -1;
				}
				if (ch == 0)
					return I_TIMEOUT;
			}
			if (fd && FD_ISSET(fd, rx))
				return I_OTHERDATA;

			for (;;) {
#ifdef ALLOWSWITCHCODE
				if( convcode ) ch = read2(0, data, IBUFSIZE);
				else 
#endif
				ch = read(0, data, IBUFSIZE);

				if (ch > 0)
					break;
				if ((ch < 0) && (errno == EINTR))
					continue;
				//longjmp(byebye, -1);
				abort_bbs();/* 非正常断线的处理 */
			}
			icurrchar = (*data & 0xff) == IAC ? iac_count(data) : 0;
			if (icurrchar >= ch)
				goto igetnext;
			ibufsize = ch;
			i_mode = INPUT_ACTIVE;
		}
		ch = data[icurrchar++];
		if (trailing) {
			trailing = 0;
			if (ch == 0 || ch == 0x0a)
				continue;
		}
		//Modified by iamfat 2002.08.21
		//防止挂站
		/*
		if(repeats==0)timestart=time(0);
		else if(time(0)-timestart>900)
		{
			if(repeats)abort_bbs();
			timestart=time(0);
		}
		if(!HAS_PERM(PERM_EXT_IDLE) && ch==repeatch)
//			&& (ch == Ctrl('L') || ch == Ctrl('@')))
		{
			repeats=1;
		}
		else 
		{
			repeats=0;
			repeatch=ch;
		}
		*/
		//Modified end.
		if (ch == Ctrl('L'))
		{
			redoscr();
			continue;
		}
		if (ch == 0x0d) {
			trailing = 1;
			ch = '\n';
		}
		return (ch);
	}
}
#else
int igetch()
{
	igetagain:
	if (ibufsize == icurrchar) {
		fd_set  readfds;
		struct timeval to;
		int     sr;
		to.tv_sec = 0;
		to.tv_usec = 0;
		FD_ZERO(&readfds);
		FD_SET(0, &readfds);
		if (i_newfd)
			FD_SET(i_newfd, &readfds);
		if ((sr = select(FD_SETSIZE, &readfds, NULL, NULL, &to)) <= 0) {
			if (flushf)
				(*flushf) ();
			if (dumb_term)
				oflush();
			else
				refresh();
			FD_ZERO(&readfds);
			FD_SET(0, &readfds);
			if (i_newfd)
				FD_SET(i_newfd, &readfds);
			while ((sr = select(FD_SETSIZE, &readfds, NULL, NULL, i_top)) < 0) {
				if (errno == EINTR)
					continue;
				else {
					report("abnormal select conditions\n");
					return -1;
				}
			}
			if (sr == 0)
				return I_TIMEOUT;
		}
		if (i_newfd && FD_ISSET(i_newfd, &readfds))
			return I_OTHERDATA;
#ifdef ALLOWSWITCHCODE
		if( convcode ) 
			ibufsize = read2(0, inbuf, IBUFSIZE);
		else
#endif
		ibufsize = read(0, inbuf, IBUFSIZE);
		while ( ibufsize <= 0 ) {
			if (ibufsize == 0)
				longjmp(byebye, -1);
			if (ibufsize < 0 && errno != EINTR)
				longjmp(byebye, -1);
#ifdef ALLOWSWITCHCODE
                	if( convcode )
                        	ibufsize = read2(0, inbuf, IBUFSIZE);
                	else
#endif
                	ibufsize = read(0, inbuf, IBUFSIZE);
		}
		icurrchar = 0;
	}
	i_mode = INPUT_ACTIVE;
	switch (inbuf[icurrchar]) {
	case Ctrl('L'):
		redoscr();
		icurrchar++;
		goto igetagain;
	default:
		break;
	}
	return inbuf[icurrchar++];
}
#endif

int igetkey()
{
	int     mode;
	int     ch, last;
	extern int RMSG;
	mode = last = 0;
	while (1) {
		if ((uinfo.in_chat == YEA || uinfo.mode == TALK || uinfo.mode == PAGE || uinfo.mode == FIVE) && RMSG == YEA) {
			char    a;
#ifdef ALLOWSWITCHCODE
			if(convcode) read2(0, &a, 1);
			else
#endif
			read(0, &a, 1);
			ch = (int) a;
		} else
			ch = igetch();
		if ((ch == Ctrl('Z')) && (RMSG == NA) && uinfo.mode != LOCKSCREEN) {
			r_msg2();
			return 0;
		}
		if (mode == 0) {
			if (ch == KEY_ESC)
				mode = 1;
			else
				return ch;	/* Normal Key */
		} else if (mode == 1) {	/* Escape sequence */
			if (ch == '[' || ch == 'O')
				mode = 2;
			else if (ch == '1' || ch == '4')
				mode = 3;
			else {
				KEY_ESC_arg = ch;
				return KEY_ESC;
			}
		} else if (mode == 2) {	/* Cursor key */
			if (ch >= 'A' && ch <= 'D')
				return KEY_UP + (ch - 'A');
			else if (ch >= '1' && ch <= '6')
				mode = 3;
			else
				return ch;
		} else if (mode == 3) {	/* Ins Del Home End PgUp PgDn */
			if (ch == '~')
				return KEY_HOME + (last - '1');
			else
				return ch;
		}
		last = ch;
	}
}

void top_show(char *prompt)
{
	if (editansi) {
		prints(ANSI_RESET);
		refresh();
	}
	move(0, 0);
	clrtoeol();
	standout();
	prints("%s", prompt);
	standend();
}

int ask(char *prompt)
{
	int     ch;
	top_show(prompt);
	ch = igetkey();
	move(0, 0);
	clrtoeol();
	return (ch);
}

extern int enabledbchar;

int getdata (	int line,		
				int col, 
				char *prompt,
				char * buf,
				int len,	
				int echo, //是否显示，若否则打印*****
				int clearlabel
			)
{
	int     ch, clen = 0, curr = 0, x, y;
	int     currDEC=0,i,patch=0;
	char    tmp[STRLEN];
	extern unsigned char scr_cols;
	extern int RMSG;
	extern int msg_num;
	if (clearlabel == YEA) {
		buf[0]='\0';
	}
	move(line, col);
	if (prompt)
		prints("%s", prompt);
	y = line;
	col += (prompt == NULL) ? 0 : strlen(prompt);
	x = col;
	clen = strlen(buf);
	curr = (clen >= len) ? len - 1 : clen;
	buf[curr] = '\0';
	prints("%s", buf);

	if (dumb_term || echo == NA) {
		while ((ch = igetkey()) != '\r') {
			if (RMSG == YEA && msg_num == 0) {
				if (ch == Ctrl('Z') || ch == KEY_UP) {
					buf[0] = Ctrl('Z');
					clen = 1;
					break;
				}
				if (ch == Ctrl('A') || ch == KEY_DOWN) {
					buf[0] = Ctrl('A');
					clen = 1;
					break;
				}
			}
			if (ch == '\n')
				break;
			if (ch == '\177' || ch == Ctrl('H')) {
				if (clen == 0) {
					continue;
				}
				clen--;
				ochar(Ctrl('H'));
				ochar(' ');
				ochar(Ctrl('H'));
				continue;
			}
			if (!isprint2(ch)) { //不可打印字符
				continue;
			}
			if (clen >= len - 1) {
				continue;
			}
			buf[clen++] = ch;
			if (echo)
				ochar(ch);
			else
				ochar('*');
		}
		buf[clen] = '\0';
		outc('\n');
		oflush();
		return clen;
	}
	clrtoeol();
	while (1) {
		if  (	(uinfo.in_chat == YEA || uinfo.mode == TALK || uinfo.mode == FIVE) 
				&& RMSG == YEA
			) {
			refresh();
		}
		ch = igetkey();
		if ((RMSG == YEA) && msg_num == 0) {
			if (ch == Ctrl('Z') || ch == KEY_UP) {
				buf[0] = Ctrl('Z');
				clen = 1;
				break;
			}
			if (ch == Ctrl('A') || ch == KEY_DOWN) {
				buf[0] = Ctrl('A');
				clen = 1;
				break;
			}
		}
		if (ch == '\n' || ch == '\r')
			break;
        if (ch == Ctrl('R')){
        	enabledbchar=~enabledbchar&1;
            continue;
        }
		if (ch == '\177' || ch == Ctrl('H')) {
			if (curr == 0) {
				continue;
			}
			currDEC = patch = 0;
            if (enabledbchar&&buf[curr-1]&0x80) {
            	for (i=curr-2;i>=0&&buf[i]&0x80;i--)
					patch ++;
			if(patch%2==0 && buf[curr]&0x80)
				patch = 1;
			else if(patch%2)
				patch = currDEC = 1;
			else 
				patch = 0; 
            }
			if(currDEC) 
				curr --;
			strcpy(tmp, &buf[curr+patch]);
			buf[--curr] = '\0';
			(void) strcat(buf, tmp);
			clen--;
			if(patch) clen --;
			move(y, x);
			prints("%s", buf);
			clrtoeol();
			move(y, x + curr);
			continue;
		}
		if (ch == KEY_DEL) {
			if (curr >= clen) {
				curr = clen;
				continue;
			}
			strcpy(tmp, &buf[curr + 1]);
			buf[curr] = '\0';
			(void) strcat(buf, tmp);
			clen--;
			move(y, x);
			prints("%s", buf);
			clrtoeol();
			move(y, x + curr);
			continue;
		}
		if (ch == KEY_LEFT) {
			if (curr == 0) {
				continue;
			}
			curr--;
			move(y, x + curr);
			continue;
		}
		if (ch == Ctrl('E') || ch == KEY_END) {
			curr = clen;
			move(y, x + curr);
			continue;
		}
		if (ch == Ctrl('A') || ch == KEY_HOME) {
			curr = 0;
			move(y, x + curr);
			continue;
		}
		if (ch == KEY_RIGHT) {
			if (curr >= clen) {
				curr = clen;
				continue;
			}
			curr++;
			move(y, x + curr);
			continue;
		}
		if (!isprint2(ch)) {
			continue;
		}
		if (x + clen >= scr_cols || clen >= len - 1) {
			continue;
		}
		if (!buf[curr]) {
			buf[curr + 1] = '\0';
			buf[curr] = ch;
		} else {
			strncpy(tmp, &buf[curr], len);
			buf[curr] = ch;
			buf[curr + 1] = '\0';
			strncat(buf, tmp, len - curr);
		}
		curr++;
		clen++;
		move(y, x);
		prints("%s", buf);
		move(y, x + curr);
	}
	buf[clen] = '\0';
	if (echo) {
		move(y, x);
		prints("%s", buf);
	}
	outc('\n');
	refresh();
	return clen;
}



/* Added by Ashinmarch on 2007.12.01
 * used to support display of multi-line msgs
 * */

int show_data(char *buf, int maxcol, int line, int col)
{
#ifdef DEBUGMSG
    char temp[500];
    sprintf(temp, "show_data: %s", buf);
    file_append("/home/bbs/msglog", temp);
#endif


    //int y = line, x = col;
    int y, x;
    getyx(&y, &x);
    int chk = 0, i;
    for(i = 0; i < strlen(buf); i++)
    {
        if(chk) chk = 0;
        else if(buf[i] < 0) chk = 1;
        if(chk && x >= maxcol) x++;
        if(buf[i] != 13 && buf[i] != 10)
        {
            if(x > maxcol) 
            {
                clrtoeol();
                x = 0; 
                y++;
                move(y,x);
            }
            prints("%c", buf[i]);
            x++;
        }
        else
        {
            clrtoeol();
            x = 0;
            y++;
            move(y,x);
        }
    }
    return y;
}


/* Added by Ashinmarch on 2007.12.01,
 * to support multi-line msgs
 * */


/* 取消Ctrl('Q')回车的功能,改为ESC的功能
 * 取消Ctrl('Y')的功能。在目前设置下，信息只是比较长的一行
 */
int multi_getdata(int line, int col, int maxcol, char *prompt, char *buf, int len, int maxline, int clearlabel, int textmode)
{
 	    int ch, x, y, startx, starty, curr, i, k, chk, cursorx, cursory;
 	    //char savebuffer[25][LINELEN];
 	    bool init=true;
 	    char tmp[MAX_MSG_SIZE+1];
       // extern int scr_lns;
 	    extern int RMSG;
		 extern int msg_num;
 	   // int ingetdata = false;
	
 	    //if(uinfo.mode!=MSG && uinfo.mode != POSTING )
 	     int   ingetdata = true;
 	    if (clearlabel == YEA) {
 	       // buf[0] = 0;
           memset(buf, 0, sizeof(buf));
	    }
 	    move(line, col);
 	    if (prompt)
 	        prints("%s", prompt);
 	    getyx(&starty, &startx);
 	    curr = strlen(buf);
 	    //for(i=0;i<=24;i++)
 	    //    saveline_buf(i, 0, savebuffer[i]);
 	    strncpy(tmp, buf, MAX_MSG_SIZE);
 	    tmp[MAX_MSG_SIZE]=0;
 	    cursory = starty;
 	    cursorx = startx;
 	
 	    while (1) {
 	        y = starty; x = startx;
 	        move(y, x);
 	        chk = 0;
 	        if(curr==0) {
 	            cursory = y;
 	            cursorx = x;
 	        }

            //以下遍历buf的功能是显示出每次igetkey的动作。
 	        for(i=0; i<strlen(buf); i++) {
 	            if(chk) chk=0;
 	            else if(buf[i]<0) chk=1;
 	            if(chk&&x>=maxcol) x++;
 	            if(buf[i]!=13&&buf[i]!=10) {
 	                if(x>maxcol)  
                    {
 	                    clrtoeol();
 	                    x = 0;
 	                    y++;
 	                   // if(y>=scr_lns) {
 	                   //     scroll();
 	                   //     starty--;
 	                   //     cursory--;
 	                   //     y--;
 	                   // }
 	                    move(y, x);
 	                }
      //Ctrl('H')中退行bug
                   if(x == maxcol && y - starty + 1 < MAX_MSG_LINE)
                   {
                        move(y + 1, 0);
                        clrtoeol();
                        move(y, x);
                   }
 	                if(init) prints("\x1b[4m");
 	                prints("%c", buf[i]);
 	         
 	                x++;
 	            }
 	            else {
 	                clrtoeol();
 	                x = 0;
 	                y++;
 	               // if(y>=scr_lns) {
 	              //      scroll();
 	               //     starty--;
 	               //     cursory--;
 	               //     y--;
 	               // }
 	                move(y, x);
 	            }
 	            if(i==curr-1) { //打印到buf最后一个字符时的x和y是下一步初始xy
 	                cursory = y;
 	                cursorx = x;
 	            }
 	        }
 	        clrtoeol();
 	        move(cursory, cursorx);
 	        ch = igetkey();

		    if ((RMSG == YEA) && msg_num == 0) 
            {

      #ifdef DEBUGMSG
            char temp[40];
            sprintf(temp, "multi_getdata: Ctrl+Z or A\n");
            file_append("/home/bbs/msglog", temp);
      #endif
 
			    if (ch == Ctrl('Z') ) 
                {
				    buf[0] = Ctrl('Z');
				    x = 1;
				    break; //可以改成return 某个行试试
			    }
			    if (ch == Ctrl('A') ) 
                {
				    buf[0] = Ctrl('A');
				    x = 1;
				    break;
			    }
		    }

            if(ch == Ctrl('Q'))
            {
                init = true;
                buf[0]=0; curr=0;
                for(k=0; k < MAX_MSG_LINE;k++)
                {
                    move(starty+k,0);
                    clrtoeol();
                }
                continue;
            }


 	        if(textmode == 0){
 	          if ((ch == '\n' || ch == '\r'))
 	             break;
 	        }
            else{
 	          if (ch == Ctrl('W'))
 	             break;
 	        }
         
 	        switch(ch) {
 	            /*case KEY_ESC:
 	                init=true;
 	                //strcpy(buf, tmp);
 	                //buf[MAX_MSG_SIZE]=0;
 	                //curr=strlen(buf);
                    buf[0] = 0;
                    curr = 0;
                    for(k = 0; k < MAX_MSG_LINE;k++)
                    {
                        move(starty +k, 0);
                        clrtoeol();
                    }
 	                break;
                */
 
 	            case KEY_UP:
 	                init=false;
 	                if(cursory>starty) {
 	                    y = starty; x = startx;
 	                    chk = 0;
 	                    if(y==cursory-1&&x<=cursorx)
 	                        curr=0;
 	                    for(i=0; i<strlen(buf); i++) {
 	                        if(chk) chk=0;
 	                        else if(buf[i]<0) chk=1;
 	                        if(chk&&x>=maxcol) x++;
 	                        if(buf[i]!=13&&buf[i]!=10) {
 	                            if(x>maxcol) {
 	                                x = col;
 	                                y++;
 	                            }
 	                            x++;
 	                        }
 	                        else {
 	                            x = col;
 	                            y++;
 	                        }
 	                        if(y==cursory-1&&x<=cursorx)
 	                            curr=i+1;
 	                    }
 	                }
 	                break;
 	            case KEY_DOWN:
 	                init=false;
 	                if(cursory<y) {
 	                    y = starty; x = startx;
 	                    chk = 0;
 	                    if(y==cursory+1&&x<=cursorx)
 	                        curr=0;
 	                    for(i=0; i<strlen(buf); i++) {
 	                        if(chk) chk=0;
 	                        else if(buf[i]<0) chk=1;
 	                        if(chk&&x>=maxcol) x++;
 	                        if(buf[i]!=13&&buf[i]!=10) {
 	                            if(x>maxcol) {
 	                                x = col;
 	                                y++;
 	                            }
 	                            x++;
 	                        }
 	                        else {
 	                            x = col;
 	                            y++;
 	                        }
 	                        if(y==cursory+1&&x<=cursorx)
 	                           curr=i+1;
 	                    }
 	                }
 	                break;
 	            case '\177':
 	            case Ctrl('H'):
 	                if(init) {
 	                    init=false;
 	                    buf[0]=0;
 	                    curr=0;
 	                }
 	                if(curr>0) {
 	                   // for(i=curr-1;i<strlen(buf);i++)
 	                   //     buf[i]=buf[i+1];
 	                   //curr--;
                       int currDec = 0, patch = 0;
                       if(buf[curr-1] < 0){
                           for(i = curr - 2; i >=0 && buf[i]<0; i--)
                               patch++;
                           if(patch%2 == 0 && buf[curr] < 0)
                               patch = 1;
                           else if(patch%2)
                               patch = currDec = 1;
                           else
                               patch = 0;
                       }
                       if(currDec) curr--;
                       strcpy(tmp, &buf[curr+patch]);
                       buf[--curr] = 0;
                       strcat(buf, tmp);
 	                }
 	                break;
 	            case KEY_DEL:
 	                if(init) {
 	                    init=false;
 	                    buf[0]=0;
 	                    curr=0;
 	                }
 	                if(curr<strlen(buf)) {
 	                    for(i=curr;i<strlen(buf);i++)
 	                        buf[i]=buf[i+1];
 	                }
 	                break;
 	            case KEY_LEFT:
 	                init=false;
 	                if(curr>0) {
 	                    curr--;
 	                }
 	                break;
 	            case KEY_RIGHT:
 	                init=false;
 	                if(curr<strlen(buf)) {
 	                    curr++;
 	                }
 	                break;
 	            case KEY_HOME:
 	            case Ctrl('A'):
 	                init=false;
 	                curr--;
 	                while(curr>=0&&buf[curr]!='\n'&&buf[curr]!='\r') curr--;
 	                curr++;
 	                break;
 	            case KEY_END:
 	            case Ctrl('E'):
 	                init=false;
 	                while(curr<strlen(buf)&&buf[curr]!='\n'&&buf[curr]!='\r') curr++;
 	                break;
 	            case KEY_PGUP:
 	                init=false;
 	                curr=0;
 	                break;
 	            case KEY_PGDN:
 	                init=false;
 	                curr = strlen(buf);
 	                break;
 	            /*
                 case Ctrl('Y'):
 	                if(init) {
 	                    init=false;
 	                    buf[0]=0;
 	                    curr=0;
 	                }
 	                i0 = strlen(buf);
 	                i=curr-1;
 	                while(i>=0&&buf[i]!='\n'&&buf[i]!='\r') i--;
 	                i++;
 	                if(!buf[i]) break;
 	                j=curr;
 	                while(j<i0-1&&buf[j]!='\n'&&buf[j]!='\r') j++;
 	                if(j>=i0-1) j=i0-1;
 	                j=j-i+1;
 	                if(j<0) j=0;
 	                for(k=0;k<i0-i-j+1;k++)
 	                    buf[i+k]=buf[i+j+k];
 	
 	                y = starty; x = startx;
 	                chk = 0;
 	                if(y==cursory&&x<=cursorx)
 	                    curr=0;
 	                for(i=0; i<strlen(buf); i++) {
 	                    if(chk) chk=0;
 	                    else if(buf[i]<0) chk=1;
 	                    if(chk&&x>=maxcol) x++;
 	                    if(buf[i]!=13&&buf[i]!=10) {
 	                        if(x>maxcol) {
 	                            x = col;
 	                            y++;
 	                        }
 	                        x++;
 	                    }
 	                    else {
 	                        x = col;
 	                        y++;
 	                    }
 	                    if(y==cursory&&x<=cursorx)
 	                        curr=i+1;
 	                }
 	
 	                if(curr>strlen(buf)) curr=strlen(buf);
 	                break;
                    */
 	            default:
 	                if(isprint2(ch)&&strlen(buf)<len-1) {
 	                    if(init) {
 	                        init=false;
 	                        buf[0]=0;
 	                        curr=0;
 	                    }
 	                    for(i=strlen(buf)+1;i>curr;i--)
 	                        buf[i]=buf[i-1];
 	                    buf[curr++]=ch;
 	                    y = starty; x = startx;
 	                    chk = 0;
 	                    for(i=0; i<strlen(buf); i++) {
 	                        if(chk) chk=0;
 	                        else if(buf[i]<0) chk=1;
 	                        if(chk&&x>=maxcol) x++;
 	                        if(buf[i]!=13&&buf[i]!=10) {
 	                            if(x>maxcol) {
 	                                x = col;
 	                                y++;
 	                            }
 	                            x++;
 	                        }
 	                        else {
 	                            x = col;
 	                            y++;
 	                        }
 	                    }
                        //采用先插入后检查是否超过maxline，如果超过，那么删去这个字符调整
 	                    if(y-starty+1>maxline) {
 	                        for(i=curr-1;i<strlen(buf);i++)
 	                            buf[i]=buf[i+1];
 	                        curr--;
 	                    }
 	                }
 	                init=false;
 	                break;
 	        }
 	    }
 	
 	    ingetdata = false;
 	    return y-starty+1;
 	}
