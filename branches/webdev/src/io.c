// Terminal I/O handlers.

#include "bbs.h"
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

char inbuf[IBUFSIZE];
int ibufsize = 0;
int icurrchar = 0;
int KEY_ESC_arg;

static int i_mode= INPUT_ACTIVE;
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
int switch_code() {
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
int write2(int port, char *str, int len) // write gb to big
{
	register int i, locate;
	register unsigned char ch1, ch2, *ptr;

	for(i=0, ptr=str; i < len;i++) {
		ch1 = (ptr+i)[0];
		if(ch1 < 0xA1 || (ch1> 0xA9 && ch1 < 0xB0) || ch1> 0xF7)
		continue;
		ch2 = (ptr+i)[1];
		i ++;
		if(ch2 < 0xA0 || ch2 == 0xFF )
		continue;
		if((ch1> 0xA0) && (ch1 < 0xAA)) //01～09区为符号数字
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
	register int locate, i=0;
	if(len == 0) return 0;
	len = read(port, str, len);
	if( len < 1)
	return len;

	for(i=0,ptr = str; i < len; i++) {
		ch1 = (ptr+i)[0];
		if(ch1 < 0xA1 || ch1 == 0xFF)
		continue;
		ch2 = (ptr+i)[1];
		i ++;
		if(ch2 < 0x40 || ( ch2> 0x7E && ch2 < 0xA1 ) || ch2 == 255)
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
void hit_alarm_clock() {
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
void init_alarm() {
	signal(SIGALRM, hit_alarm_clock);
	alarm(IDLE_TIMEOUT);
}

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
void output(char *s,int len)
{
	/* Invalid if len >= OBUFSIZE */

	register int size;
	register char *data;
	size = obufsize;
	data = outbuf;
	if (size + len> OBUFSIZE) {
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

	if (size> OBUFSIZE - 2) { /* doin a oflush */
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

int i_newfd = 0;
struct timeval i_to, *i_top = NULL;
int (*flushf)() = NULL;

void add_io(int fd, int timeout) {
	i_newfd = fd;
	if (timeout) {
		i_to.tv_sec = timeout;
		i_to.tv_usec = 0;
		i_top = &i_to;
	} else
		i_top = NULL;
}

//	将flushf函数指针指向函数flushfunc
void add_flush(int (*flushfunc)()) {
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
int num_in_buf() {
	int n;
	if ((n = icurrchar - ibufsize) < 0)
		n=0;
	return n;
}

static int iac_count(char *current)
{
	switch (*(current + 1) & 0xff) {
		case DO:
		case DONT:
		case WILL:
		case WONT:
		return 3;
		case SB: /* loop forever looking for the SE */
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
			fd_set readfds;
			struct timeval to;
			register fd_set *rx;
			register int fd, nfds;
			rx = &readfds;
			fd = i_newfd;

			igetnext:

			uinfo.idle_time = time(0);
			update_utmp(); /* 应该是需要 update 一下 :X */

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

				if (ch> 0)
				break;
				if ((ch < 0) && (errno == EINTR))
				continue;
				//longjmp(byebye, -1);
				abort_bbs(0);
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

int igetkey() {
	int mode;
	int ch, last;
	extern int RMSG;
	mode = last = 0;
	while (1) {
		if ((uinfo.in_chat == YEA || uinfo.mode == TALK || uinfo.mode
				== PAGE || uinfo.mode == FIVE) && RMSG == YEA) {
			char a;
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
				return ch; /* Normal Key */
		} else if (mode == 1) { /* Escape sequence */
			if (ch == '[' || ch == 'O')
				mode = 2;
			else if (ch == '1' || ch == '4')
				mode = 3;
			else {
				KEY_ESC_arg = ch;
				return KEY_ESC;
			}
		} else if (mode == 2) { /* Cursor key */
			if (ch >= 'A' && ch <= 'D')
				return KEY_UP + (ch - 'A');
			else if (ch >= '1' && ch <= '6')
				mode = 3;
			else
				return ch;
		} else if (mode == 3) { /* Ins Del Home End PgUp PgDn */
			if (ch == '~')
				return KEY_HOME + (last - '1');
			else
				return ch;
		}
		last = ch;
	}
}

int egetch(void)
{
	extern int talkrequest; //main.c
	extern int refscreen; //main.c
	int rval;

	check_calltime();
	if (talkrequest) {
		talkreply();
		refscreen = YEA;
		return -1;
	}
	while (1) {
		rval = igetkey();
		if (talkrequest) {
			talkreply();
			refscreen = YEA;
			return -1;
		}
		if (rval != Ctrl('L'))
			break;
		redoscr();
	}
	refscreen = NA;
	return rval;
}

void top_show(char *prompt) {
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

int ask(char *prompt) {
	int ch;
	top_show(prompt);
	ch = igetkey();
	move(0, 0);
	clrtoeol();
	return (ch);
}

extern int enabledbchar;

int getdata(int line, int col, char *prompt, char * buf, int len,
		int echo, int clearlabel) {
	int ch, clen = 0, curr = 0, x, y;
	int currDEC=0, i, patch=0;
	char tmp[STRLEN];
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
			if (!isprint2(ch)) {
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
		if ( (uinfo.in_chat == YEA || uinfo.mode == TALK || uinfo.mode
				== FIVE) && RMSG == YEA) {
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
		if (ch == Ctrl('R')) {
			enabledbchar=~enabledbchar&1;
			continue;
		}
		if (ch == '\177' || ch == Ctrl('H')) {
			if (curr == 0) {
				continue;
			}
			currDEC = patch = 0;
			if (enabledbchar&&buf[curr-1]&0x80) {
				for (i=curr-2; i>=0&&buf[i]&0x80; i--)
					patch ++;
				if (patch%2==0 && buf[curr]&0x80)
					patch = 1;
				else if (patch%2)
					patch = currDEC = 1;
				else
					patch = 0;
			}
			if (currDEC)
				curr --;
			strcpy(tmp, &buf[curr+patch]);
			buf[--curr] = '\0';
			(void) strcat(buf, tmp);
			clen--;
			if (patch)
				clen --;
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
			strlcpy(tmp, &buf[curr], len);
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

char *boardmargin() {
	static char buf[STRLEN];

	//Modified by IAMFAT 2002-05-26 Add ' '
	//Modified by IAMFAT 2002-05-28
	//Roll Back 2002-05-29
	if (selboard)
		sprintf(buf, "讨论区 [%s]", currboard);
	else {
		brc_initial(currentuser.userid, DEFAULTBOARD);
		changeboard(currbp, currboard, DEFAULTBOARD);
		if (!getbnum(currboard, &currentuser))
			setoboard(currboard);
		selboard = 1;
		sprintf(buf, "讨论区 [%s]", currboard);
	}
	return buf;
}

void update_endline() {
	extern time_t login_start_time; //main.c
	extern int WishNum; //main.c
	extern int orderWish; //main.c
	extern char GoodWish[][STRLEN - 3]; //main.c

	char buf[255], fname[STRLEN], *ptr;
	time_t now;
	FILE *fp;
	int i, allstay, foo, foo2;

	move(t_lines - 1, 0);
	clrtoeol();

	if (!DEFINE(DEF_ENDLINE))
		return;

	now = time(0);
	allstay = getdatestring(now, NA); // allstay 为当前秒数
	if (allstay == 0) {
		nowishfile: resolve_boards();
		strcpy(datestring, brdshm->date);
		allstay = 1;
	}
	if (allstay < 5) {
		allstay = (now - login_start_time) / 60;
		sprintf(buf, "[[36m%.12s[33m]", currentuser.userid);
		num_alcounter();
		//Modified by IAMFAT 2002-05-26
		//Roll Back 2002-05-29
		prints(
				"[1;44;33m[[36m%29s[33m][[36m%4d[33m人/[1;36m%3d[33m友][[36m%1s%1s%1s%1s%1s%1s[33m]帐号%-24s[[36m%3d[33m:[36m%2d[33m][m",
				datestring, count_users, count_friends, (uinfo.pager
						& ALL_PAGER) ? "P" : "p", (uinfo.pager
						& FRIEND_PAGER) ? "O" : "o", (uinfo.pager
						& ALLMSG_PAGER) ? "M" : "m", (uinfo.pager
						& FRIENDMSG_PAGER) ? "F" : "f",
				(DEFINE(DEF_MSGGETKEY)) ? "X" : "x",
				(uinfo.invisible == 1) ? "C" : "c", buf, (allstay / 60)
						% 1000, allstay % 60);
		return;
	}
	setuserfile(fname, "HaveNewWish");
	if (WishNum == 9999 || dashf(fname)) {
		if (WishNum != 9999)
			unlink(fname);
		WishNum = 0;
		orderWish = 0;

		if (is_birth(currentuser)) {
			strcpy(GoodWish[WishNum],
			//Roll Back 2002-05-29
					"                     啦啦～～，生日快乐!   记得要请客哟 :P                   ");
			WishNum++;
		}

		setuserfile(fname, "GoodWish");
		if ((fp = fopen(fname, "r")) != NULL) {
			for (; WishNum < 20;) {
				if (fgets(buf, 255, fp) == NULL)
					break;
				buf[STRLEN - 4] = '\0';
				ptr = strtok(buf, "\n\r");
				if (ptr == NULL || ptr[0] == '#')
					continue;
				strcpy(buf, ptr);
				for (ptr = buf; *ptr == ' ' && *ptr != 0; ptr++)
					;
				if (*ptr == 0 || ptr[0] == '#')
					continue;
				for (i = strlen(ptr) - 1; i < 0; i--)
					if (ptr[i] != ' ')
						break;
				if (i < 0)
					continue;
				foo = strlen(ptr);
				foo2 = (STRLEN - 3 - foo) / 2;
				strcpy(GoodWish[WishNum], "");
				for (i = 0; i < foo2; i++)
					strcat(GoodWish[WishNum], " ");
				strcat(GoodWish[WishNum], ptr);
				for (i = 0; i < STRLEN - 3 - (foo + foo2); i++)
					strcat(GoodWish[WishNum], " ");
				GoodWish[WishNum][STRLEN - 4] = '\0';
				WishNum++;
			}
			fclose(fp);
		}
	}
	if (WishNum == 0)
		goto nowishfile;
	if (orderWish >= WishNum * 2)
		orderWish = 0;
	//Modified by IAMFAT 2002-05-26 insert space
	//Roll Back 2002-05-29
	prints("[0;1;44;33m[[36m%77s[33m][m", GoodWish[orderWish / 2]);
	orderWish++;
}

/*ReWrite by SmallPig*/
void showtitle(char *title, char *mid) {
	extern char BoardName[]; //main.c

	char buf[STRLEN], *note;
	int spc1;
	int spc2;

	note = boardmargin();
	spc1 = 39 + num_ans_chr(title) - strlen(title) - strlen(mid) / 2;
	//if(spc1 < 2) 
	//      spc1 = 2;
	//Modified by IAMFAT 2002-05-28
	//Roll Back 2002-05-29
	spc2 = 79 - (strlen(title) - num_ans_chr(title) + spc1 + strlen(note)
			+ strlen(mid));
	//if (spc2 < 1) 
	//      spc2 = 1;
	spc1 += spc2;
	spc1 = (spc1 > 2) ? spc1 : 2; //防止过小
	spc2 = spc1 / 2;
	spc1 -= spc2;
	move(0, 0);
	clrtoeol();
	sprintf(buf, "%*s", spc1, "");
	if (!strcmp(mid, BoardName))
		prints("[1;44;33m%s%s[37m%s[1;44m", title, buf, mid);
	else if (mid[0] == '[')
		prints("[1;44;33m%s%s[5;36m%s[m[1;44m", title, buf, mid);
	else
		prints("[1;44;33m%s%s[36m%s", title, buf, mid);
	sprintf(buf, "%*s", spc2, "");
	prints("%s[33m%s[m\n", buf, note);
	update_endline();
	move(1, 0);
}
void firsttitle(char *title) {
	extern int mailXX; //main.c
	extern char BoardName[]; //main.c
	char middoc[30];

	if (chkmail())
		strcpy(middoc, strstr(title, "讨论区列表") ? "[您有信件，按 M 看新信]"
				: "[您有信件]");
	else if (mailXX == 1)
		strcpy(middoc, "[信件过量，请整理信件!]");
	else
		strcpy(middoc, BoardName);

	showtitle(title, middoc);
}
void docmdtitle(char *title, char *prompt) {
	firsttitle(title);
	move(1, 0);
	clrtoeol();
	prints("%s", prompt);
	clrtoeol();
}

