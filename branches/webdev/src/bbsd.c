#include "bbs.h"

#include <sys/resource.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/telnet.h>

#ifdef SYSV
#include <sys/termios.h>
#else
#include <termios.h>
#endif

#ifdef LINUX
#include <sys/ioctl.h>
#endif

#ifdef LINUX
#define HAVE_CHKLOAD		/*	是否检查服务器负荷	*/
#endif

#define SOCKET_QLEN     4

#define TH_LOW          30
#define TH_HIGH         50

#define PID_FILE		BBSHOME"/reclog/bbsd.pid"
#define LOG_FILE		BBSHOME"/reclog/bbsd.log"
//#define BAD_HOST		BBSHOME"/.bad_host"
#define NOLOGIN			BBSHOME"/NOLOGIN"

#ifdef  HAVE_CHKLOAD
#define BANNER  "\r\n欢迎光临[1;33m"BBSNAME"[m[ [1;32m"BBSHOST"[m ] [1;33m"BBSVERSION"[m 请稍候...\r\n[1;36m最近 [33m(1,10,15)[36m 分钟平均负荷为[33m %s [36m(上限 = %d) [%s][0m\r\n"
#else
#define BANNER  "\r\n欢迎光临[1;33m"BBSNAME"[m[ [1;32m"BBSHOST"[m] [1;33m"BBSVERSION"[m \r\n"
#endif

jmp_buf byebye; //存储某个分支状态.longjump到此时将退出
char remoteusername[40] = "?"; //
extern char fromhost[60]; //存储用户的IP来源,或者是字符表示的域名
char genbuf[1024]; //通用字符串缓冲区,以进一步操作
#ifdef HAVE_CHKLOAD
char loadstr[1024]; //表示系统负荷的字符串
#endif
char status[64]; //根据系统负荷,用于显示的字符串

/* ----------------------------------------------------- */
/* FSA (finite state automata) for telnet protocol       */
/* ----------------------------------------------------- */

//	telnet初始化,与用户通信
static void telnet_init() {
	static char svr[] = { IAC, WILL, TELOPT_ECHO, IAC, WILL, TELOPT_SGA };
	struct timeval to;
	int rset = 1;
	char buf[256];

	send(0, svr, 6, 0);
	to.tv_sec = 6;
	to.tv_usec = 1;
	if (select(1, (fd_set *)(&rset), NULL, NULL, &to)> 0)
		recv(0, buf, sizeof(buf), 0);
}

/* ----------------------------------------------- */
/* 取得 remote user name 以判定身份                */
/* ----------------------------------------------- */

/*
 * rfc931() speaks a common subset of the RFC 931, AUTH, TAP, IDENT and RFC
 * 1413 protocols. It queries an RFC 931 etc. compatible daemon on a remote
 * host to look up the owner of a connection. The information should not be
 * used for authentication purposes. This routine intercepts alarm signals.
 *
 * Author: Wietse Venema, Eindhoven University of Technology, The Netherlands.
 */

#include <setjmp.h>

#define STRN_CPY(d,s,l) { strlcpy((d),(s),(l)); (d)[(l)-1] = 0; }
#define RFC931_TIMEOUT   10
#define RFC931_PORT     113	/* Semi-well-known port */
#define ANY_PORT        0	/* Any old port will do */

/* ------------------------- */
/* timeout - handle timeouts */
/* ------------------------- */
#ifdef DOMAIN_NAME	
//超时处理函数,跳转到byebye存储的状态中去
static void timeout(int sig)
{
	(void) longjmp(byebye, sig);
}
#endif

//从from中取得远程计算机的名称,并存储在rhost中
//rname为用户名,目前似乎并无用处
static void getremotename(struct sockaddr_in *from, char *rhost,
		char *rname) {
	//struct sockaddr_in our_sin;
	//struct sockaddr_in rmt_sin;
	//unsigned rmt_port, rmt_pt;
	//unsigned our_port, our_pt;
	//FILE   *fp;
	//char    buffer[512], user[80], *cp;
	//int     s;
#ifdef DOMAIN_NAME	
	struct hostent *hp;
	/* get remote host name */

	hp = NULL;
	if (setjmp(byebye) == 0)
	{
		(void) signal(SIGALRM, timeout);
		(void) alarm(3);
		hp = gethostbyaddr( (char *) &from->sin_addr,
				sizeof(struct in_addr),
				from->sin_family
		);
		(void) alarm(0);
	}
	(void) strcpy(rhost, hp ? hp->h_name : (char *) inet_ntoa(from->sin_addr));
#else  
	setjmp(byebye);
	(void)strcpy(rhost, (char *)inet_ntoa(from->sin_addr));
#endif	

	return;//added by roly 02.03.30
}

/* ----------------------------------- */
/* check system / memory / CPU loading */
/* ----------------------------------- */

#ifdef  HAVE_CHKLOAD
int fkmem;

//检查系统负荷,并返回负荷度
int chkload(int limit)
{
	double cpu_load[3];
	register int i;
#ifdef BSD44
	getloadavg(cpu_load, 3);
#elif defined(LINUX)
	FILE *fp;
	fp = fopen("/proc/loadavg", "r");

	if (!fp)
	cpu_load[0] = cpu_load[1] = cpu_load[2] = 0;
	else {
		float av[3]; /*	这个变量似乎有点多余	*/
		fscanf(fp, "%g %g %g", av, av + 1, av + 2);
		fclose(fp);
		cpu_load[0] = av[0];
		cpu_load[1] = av[1];
		cpu_load[2] = av[2];
	}
#else

#include <nlist.h>

#ifdef SOLARIS

#define VMUNIX  "/dev/ksyms"
#define KMEM    "/dev/kmem"

	static struct nlist nlst[] = {
		{	"avenrun"},
		{	0}
	};
#else

#define VMUNIX  "/vmunix"
#define KMEM    "/dev/kmem"

	static struct nlist nlst[] = {
		{	"_avenrun"},
		{	0}
	};
#endif

	long avenrun[3];
	static long offset = -1;
	int kmem;
	if ((kmem = open(KMEM, O_RDONLY)) == -1)
	return (1);

	if (offset < 0) {
		(void) nlist(VMUNIX, nlst);
		if (nlst[0].n_type == 0)
		return (1);
		offset = (long) nlst[0].n_value;
	}
	if (lseek(kmem, offset, L_SET) == -1) {
		close(kmem);
		return (1);
	}
	if (read(kmem, (char *) avenrun, sizeof(avenrun)) == -1) {
		close(kmem);
		return (1);
	}
	close(kmem);
#define loaddouble(la) ((double)(la) / (1 << 8))

	for (i = 0; i < 3; i++)
	cpu_load[i] = loaddouble(avenrun[i]);
#endif

	//	i = cpu_load[0];
	//	if (i < limit)
	i = 0;
	if(i) {
		strcpy(status,"超负荷，请稍后再来");
	} else if (cpu_load[0] >= (float) 0 && cpu_load[0] < (float) 1) {
		strcpy(status, "负荷正常");
	} else if (cpu_load[0] >= 1 && cpu_load[0] < (float) 10) {
		strcpy(status, "负荷偏高");
	} else {
		strcpy(status, "负荷过重");
	}
	sprintf(loadstr,"%.2f %.2f %.2f",cpu_load[0],cpu_load[1],cpu_load[2]);

	return i;
}
#endif

/* ----------------------------------------------------- */
/* stand-alone daemon                                    */
/* ----------------------------------------------------- */

static int mainset; /* read file descriptor set */
static struct sockaddr_in xsin;

//等待所有的子进程完成
static void reapchild() {
	int state, pid;
	while ((pid = waitpid(-1, &state, WNOHANG | WUNTRACED)) > 0)
		;
}

//启动服务进程,关闭所有打开的文件描述符,把所有的信号处理设置成ignore
//		忽视所有的外部信号 kill -9 应该不能忽视的:)
static void start_daemon() {
	int n;
	char buf[80];
	/*
	 * More idiot speed-hacking --- the first time conversion makes the C
	 * library open the files containing the locale definition and time
	 * zone. If this hasn't happened in the parent process, it happens in
	 * the children, once per connection --- and it does add up.
	 */

	time_t dummy = time(NULL);
	struct tm *dummy_time = localtime(&dummy);
	(void) strftime(buf, 80, "%d/%b/%Y:%H:%M:%S", dummy_time);

	n = getdtablesize();

	if (fork())
		exit(0);
	// we use setsid instead of open("/dev/tty") for compatibility.  by cucme
	if (setsid()==-1)
		exit(0);
	//

	if (fork()) // one more time 
		exit(0);

	sprintf(genbuf, "%d\t%s", getpid(), buf);

	while (n)
		(void) close(--n);
	/*
	 n = open("/dev/tty", O_RDWR);
	 if (n > 0) {
	 (void) ioctl(n, TIOCNOTTY, (char *) 0);
	 (void) close(n);
	 }
	 */

	for (n = 1; n <= NSIG; n++)
		(void) signal(n, SIG_IGN);

}

//退出服务进程
static void close_daemon() {
	exit(0);
}

//将时间,进程ID等信息写入日志文件
static void bbsd_log(char *str) {
	char buf[256];
	time_t mytime;
	struct tm *tm;

	mytime = time(0);
	tm = localtime(&mytime);
	sprintf(buf, "%.2d/%.2d/%.2d %.2d:%.2d:%.2d bbsd[%d]: %s", tm->tm_year
			%100, tm->tm_mon+1, tm->tm_mday, tm->tm_hour, tm->tm_min,
			tm->tm_sec, getpid(), str);
file_append(LOG_FILE, buf);
}

//绑定并监听端口port,返回socket描述符
static int bind_port(int port) {
	int sock, on;
	sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

	on = 1;
	(void) setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char *) &on,
			sizeof(on));

	xsin.sin_port = htons(port);
	if (bind(sock, (struct sockaddr *) &xsin, sizeof xsin) < 0) {
		sprintf(genbuf, "bbsd bind_port can't bind to %d\n", port);
		bbsd_log(genbuf);
		exit(1);
	}
	if (listen(sock, SOCKET_QLEN) < 0) {
		sprintf(genbuf, "bbsd bind_port can't listen to %d\n", port);
		bbsd_log(genbuf);
		exit(1);
	}
	FD_SET(sock, (fd_set *) &mainset);

	sprintf(genbuf, "started on port %d\n", port);
	bbsd_log(genbuf);

	return sock;
}

#ifdef BAD_HOST
//返回name是否在BAD_HOST宏所指定的文件中存在,
//		返回是否符合相关条件	1为true,0为false
static int bad_host(char *name)
{
	FILE *list;
	char buf[40], *ptr;

	if (list = fopen(BAD_HOST, "r")) {
		while (fgets(buf, 40, list)) {
			ptr = strtok(buf, " \n\t\r");
			if (ptr != NULL && *ptr != '#') {
				if(!strcmp(ptr, name))
				return 1;
				if(ptr[0] == '#')
				continue;
				if(ptr[0] == '-' && !strcmp(name,&ptr[1]))
				return 0;
				if(ptr[strlen(ptr)-1]=='.'&&!strncmp(ptr,name,strlen(ptr)-1))
				return 1;
				if(ptr[0]=='.'&&strlen(ptr)<strlen(name)&&
						!strcmp(ptr,name+strlen(name)-strlen(ptr)))
				return 1;
			}//	if (ptr != NULL && *ptr != '#')
		}//	while (fgets(buf, 40, list)) 
		fclose(list);
	}//if (list = fopen(BAD_HOST, "r")) 
	return 0;
}
#endif

#define MAX_DEAD 1000
int deadsock[MAX_DEAD];
time_t deadtime[MAX_DEAD];
int deadnum=0;

int main(int argc, char *argv[])
{
	register int msock, csock; /* socket for Master and Child */
	register int nfds; /* number of sockets */
	register int overload;
	register pid_t pid;
	register time_t uptime;
	int readset;
	int value;
	struct timeval tv;
#ifdef IP_2_NAME
	int i_counter;
	struct ip_name {
		char ip[16];
		char name[60];
	} table[256];
#endif			

	/* --------------------------------------------------- */
	/* setup standalone                                    */
	/* --------------------------------------------------- */

	start_daemon();

	(void) signal(SIGCHLD, reapchild); //收到此信号时,等候所有子进程结束
	(void) signal(SIGTERM, close_daemon); //结束时,关闭daemon


	/* --------------------------------------------------- */
	/* port binding                                        */
	/* --------------------------------------------------- */

	xsin.sin_family = AF_INET;
	if (argc > 1) { //监听端口为csock
		csock = atoi(argv[1]) ;
	}
	if (csock <= 0) {
		csock = 12345;
	}
	msock = bind_port(csock);
	if (msock < 0) {
		exit(1);
	}
	nfds = msock + 1;

	/* --------------------------------------------------- */
	/* Give up root privileges: no way back from here      */
	/* --------------------------------------------------- */

	(void) setgid((gid_t)BBSGID);
	(void) setuid((uid_t)BBSUID);
	(void) chdir(BBSHOME);
	umask((mode_t)022); //默认创建的文件目录权限都是644

	sprintf(genbuf, "%d %d\n", csock, getpid());
	file_append(PID_FILE, genbuf);

	/* --------------------------------------------------- */
	/* main loop                                           */
	/* --------------------------------------------------- */

	tv.tv_sec = 60 * 30;
	tv.tv_usec = 0;

	overload = uptime = 0;
	/*
	   Commented by Erebus 2004-11-03
	   define IP_2_NAME in fuctions.h
	   use domain name instand of ip when shuttling
	   */
#ifdef IP_2_NAME
	{//#ifdef IP_2_NAME
		FILE *netip;
		char line[STRLEN], *special;
		memset(table, 0, sizeof(struct ip_name)*256);

		netip = fopen("etc/hosts", "r");//设置IP到DNS的映射,如
		//		202.120.3.1->bbs.sjtu.edu.cn
		if (netip != NULL) {
			i_counter = 0;
			while(fgets(line, STRLEN, netip)) {
				special = strtok(line, " \r\n\t");
				if (special) {
					strlcpy(table[i_counter].ip,
							special,
							strlen(special)>15?15:strlen(special)
						   );
					special = strtok(NULL, " \r\n\t");
					if (special) {
						strlcpy(table[i_counter].name,
								special,
								strlen(special)>59?59:strlen(special)
							   );
						i_counter++;
					} else {
						memset(table[i_counter].ip, 0, 16);
					}//		if (special)
				}//	if (special)
				if (i_counter> 255)
					break;
			}//	while(fgets(line, STRLEN, netip))
			fclose(netip);
		}//if (netip != NULL)
	}//#ifdef IP_2_NAME
#endif

	for (;;) {
#ifdef  HAVE_CHKLOAD
		pid = time(0);
		if (pid> uptime) {
			overload = chkload(TH_LOW); //chkload(overload ? TH_LOW : TH_HIGH);
			uptime = pid + 10; /* 短时间内不再检查 system load */
		}
#endif

again: readset = mainset;

	   value = sizeof(xsin);
	   do {
		   csock = accept(msock, (struct sockaddr *) &xsin, &value);
	   } while (csock < 0 && errno == EINTR);

	   if (csock < 0) {
		   goto again;
	   }

#ifdef HAVE_CHKLOAD
	   sprintf(genbuf, BANNER, loadstr, TH_LOW, status);
#else
	   sprintf		(genbuf, BANNER);
#endif
	   (void) write(csock, genbuf, strlen(genbuf));
#ifdef  HAVE_CHKLOAD
	   {
		   register int idead;
		   time_t now=time(0);
		   idead=deadnum;
		   while(idead--) {
			   if(now-deadtime[idead]>4) {
				   close(deadsock[idead]);
				   if(idead<deadnum-1) {
					   deadtime[idead]=deadtime[deadnum-1];
					   deadsock[idead]=deadsock[deadnum-1];
				   }
				   deadnum--;
			   }
		   }
	   }// HAVE_CHKLOAD

	   sprintf(genbuf, "\n目前正有%d人正尝试连接", deadnum);
	   write(csock, genbuf, strlen(genbuf));

	   if (overload) {
		   if(deadnum<MAX_DEAD) {
			   deadsock[deadnum]=csock;
			   deadtime[deadnum++]=time(0);
		   } else {
			   close(csock);
		   }
		   continue;
	   }// if(overload)


#endif

	   /*
		  Commented by Erebus 2004-11-03
		  if wanna to maitain the system,create a file named"NOLOGIN" under the BBSHOME path
		  */
#ifdef NOLOGIN
	   {
		   FILE *fp;
		   char buf[256];
#define MYBANNER "\r\nFB2000 [bbsd NOLOGIN] 系统处于[1;33m暂停登陆[m状态\r\n[1;32m[本站程序维护可以删除 \'[36m~bbs/NOLOGIN[32m\' 后解除该状态][m\r\n\r\n＝＝＝＝＝＝关于系统进入暂停登陆状态的【公告】＝＝＝＝＝＝\r\n"

		   if ((fp = fopen(NOLOGIN, "r")) != NULL) {
			   (void) write(csock, MYBANNER, strlen(MYBANNER));
			   while (fgets(buf, 255, fp) != 0) {
				   strcat(buf, "\r");
				   (void) write(csock, buf, strlen(buf));
			   }//while
			   fclose(fp);
			   sleep(5);
			   close(csock);
			   continue;
		   }//if
	   }
#endif

	   pid = fork();
	   if (!pid) {

#ifdef  HAVE_CHKLOAD
		   (void) close(fkmem);
#endif
		   while (--nfds >= 0)
			   (void) close(nfds);
		   (void) dup2(csock, 0);
		   (void) close(csock);
		   (void) dup2(0, 1);
		   getremotename(&xsin, fromhost, remoteusername); /* FC931 */

		   /* ban 掉 bad host / bad user */
#ifdef BAD_HOST
		   if (bad_host(fromhost) && !strcmp(remoteusername, "?"))
			   exit(1);
#endif

#ifdef IP_2_NAME
		   i_counter = 0;
#ifndef FDQUAN
		   if (strncmp(fromhost, "10.", 3) && strncmp(fromhost, "192.", 4))
#endif
			   //不是从校内来的IP,则:
			   while (table[i_counter].ip[0] != '\0') {
				   if(!strcasecmp(table[i_counter].ip, fromhost)) {
					   strcpy(fromhost, table[i_counter].name);
					   break;
				   }
				   i_counter++;
			   }

#endif
		   //下面两个调用似乎多了一个参数
		   bbssetenv("REMOTEHOST", fromhost, 1);
		   bbssetenv("REMOTEUSERNAME", remoteusername, 1);

		   telnet_init();
		   start_client();
	   }//if(!pid)
	   (void) close(csock);
	}//for
}
