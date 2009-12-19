// Handle user cache.

#include "bbs.h"
#include <sys/ipc.h>
#include <sys/shm.h>
#include <stdio.h>
#include <time.h>

#define chartoupper(c)  ((c >= 'a' && c <= 'z') ? c+'A'-'a' : c)

// The starting address of cache for online users.
struct UTMPFILE *utmpshm = NULL;
// The starting address of cache for all users.
struct UCACHE *uidshm = NULL;
// A global variable to hold result when searching users.
struct userec lookupuser;

int cmpuids(void *uid, void *up)
{
	return !strncasecmp((char *)uid, ((struct userec *)up)->userid,
		sizeof(((struct userec *)up)->userid));
}

int dosearchuser(const char *userid, struct userec *user, int *unum)
{
	int id;

	if ((id = getuser(userid)) != 0) {
		if (cmpuids(userid, &lookupuser)) {
			memcpy(user, &lookupuser, sizeof(*user));
			return *unum = id;
		}
	}
	memset(user, 0, sizeof(*user));
	return *unum = 0;
}

int uhashkey(const char *id)
{
    int key = (int)strhash(id) % UCACHE_HASH_SIZE;
    return key;
}

int linkinsert(int i)
{
    //�������
    if (i <= 0 || uidshm->userid[i-1][0] == '\0')
        return 0;

    //HASH����
    int key = uhashkey(uidshm->userid[i - 1]);
    if (uidshm->hashkey[key] > 0)
        uidshm->h_prev[uidshm->hashkey[key] - 1] = i;
    uidshm->h_next[i - 1] = uidshm->hashkey[key];
    uidshm->h_prev[i - 1] = 0;
    uidshm->hashkey[key] = i;

    //��ĸ�����
    int *p = &uidshm->alphakey[tolower(uidshm->userid[i-1][0])][tolower(uidshm->userid[i-1][1])];
    if (*p > 0)
        uidshm->a_prev[*p - 1] = i;
    uidshm->a_next[i - 1] = *p;
    uidshm->a_prev[i - 1] = 0;
    *p = i;

    //��ͨ˫������
    if (uidshm->prev[i - 1])
        uidshm->next[uidshm->prev[i - 1] - 1] = uidshm->next[i - 1];
    else
        uidshm->first_available = uidshm->next[i - 1];
    if (uidshm->next[i - 1])
        uidshm->prev[uidshm->next[i - 1] - 1] = uidshm->prev[i - 1];

    if (uidshm->first_used)
        uidshm->prev[uidshm->first_used - 1] = i;
    uidshm->next[i - 1] = uidshm->first_used;
    uidshm->prev[i - 1] = 0;
    uidshm->first_used = i;

    //�޸ļ���
    uidshm->number++;

    return 1;
}

int linkdel(int i)
{
    //�������
    if (i <= 0 || uidshm->userid[i-1][0] == '\0')
        return 0;

    //HASH����
    if (uidshm->h_prev[i - 1])
        uidshm->h_next[uidshm->h_prev[i - 1] - 1] = uidshm->h_next[i - 1];
    else
        uidshm->hashkey[uhashkey(uidshm->userid[i - 1])] = uidshm->h_next[i - 1];

    if (uidshm->h_next[i - 1])
        uidshm->h_prev[uidshm->h_next[i - 1] - 1] = uidshm->h_prev[i - 1];


    //��ĸ������
    if (uidshm->a_prev[i - 1])
        uidshm->a_next[uidshm->a_prev[i - 1] - 1] = uidshm->a_next[i - 1];
    else
        uidshm->alphakey[tolower(uidshm->userid[i-1][0])][tolower(uidshm->userid[i-1][1])] = 
            uidshm->a_next[i - 1];

    if (uidshm->a_next[i - 1])
        uidshm->a_prev[uidshm->a_next[i - 1] - 1] = uidshm->a_prev[i - 1];


    //��ͨ˫������
    if (uidshm->prev[i - 1])
        uidshm->next[uidshm->prev[i - 1] - 1] = uidshm->next[i - 1];
    else
        uidshm->first_used = uidshm->next[i - 1];
    if (uidshm->next[i - 1])
        uidshm->prev[uidshm->next[i - 1] - 1] = uidshm->prev[i - 1];

    if (uidshm->first_available)
        uidshm->prev[uidshm->first_available - 1] = i;
    uidshm->next[i - 1] = uidshm->first_available;
    uidshm->prev[i - 1] = 0;
    uidshm->first_available = i;


    //�޸ļ���
    uidshm->number--;
    

    return 1;
}

// Put userid(in struct uentp) into cache for all users.
// Find a proper entry of user hash.
static int fillucache(const struct userec *uentp, int count)
{

    if (count < MAXUSERS)
    {
        strlcpy(uidshm->userid[count++], uentp->userid, sizeof(uidshm->userid[0]));
        if(uentp->userid[0] != '\0')
            return linkinsert(count);
        else
        {
            if (uidshm->first_available)
                uidshm->prev[uidshm->first_available - 1] = count;
            uidshm->next[count - 1] = uidshm->first_available;
            uidshm->prev[count - 1] = 0;
            uidshm->first_available = count;
        }
    }
    return 0;
}

int del_uidshm(int num, char *userid)
{
	int i;

	if (num <= 0 || num > MAXUSERS)
		return 0;

	i = searchuser(userid);

    //�޴��û�
	if (i!= num || i<=0)
		return 0;

    //��������ɾ��
    linkdel(i);

    //���Ϊɾ��
	uidshm->userid[i - 1][0]='\0';

	return 1;
}

// Places an exclusive lock on file 'lockname'.
// Returns file descriptor if OK, -1 on error.
static int shm_lock(const char *lockname)
{
	int lockfd;

	lockfd = open(lockname, O_RDWR | O_CREAT, 0600);
	if (lockfd < 0) {
		return -1;
	}
	if (flock(lockfd, LOCK_EX) == -1)
		return -1;
	return lockfd;
}

// Removes an existing lock held by this process on file descriptor 'fd'.
static void shm_unlock(int fd)
{
	flock(fd, LOCK_UN);
	close(fd);
}

#define ucache_unlock(fd) shm_unlock(fd)
#define utmp_unlock(fd) shm_unlock(fd)
#define ucache_lock() shm_lock("tmp/.UCACHE.lock")
#define utmp_lock() shm_lock("tmp/.UTMP.lock")

// Loads PASSFILE into cache for all users.
// Returns 0 on success, -1 on error.
int load_ucache(void)
{
	int fd, iscreate = 0, passwdfd, i;

	// Lock cache.
	fd = ucache_lock();
	if (fd == -1) {
		return -1;
	}

	// Get shared memory.
	if (uidshm == NULL) {
		uidshm = attach_shm2("UCACHE_SHMKEY", 3696, sizeof(*uidshm),
				&iscreate);
		if(uidshm == NULL)
			exit(1);
	}
	log_usies("CACHE", "reload ucache", NULL);

	// Load PASSFILE.
	if ((passwdfd = open(PASSFILE, O_RDWR | O_CREAT, 0644)) == -1) {
		ucache_unlock(fd);
		exit(-1);
	}
	ftruncate(passwdfd, MAXUSERS * sizeof(struct userec));
	close(passwdfd);
	if (get_records(PASSFILE, uidshm->passwd, sizeof(struct userec), 1,
			MAXUSERS) != MAXUSERS) {
		ucache_unlock(fd);
		return -1;
	}

	// Initialize 'userid' and hash.
	memset(uidshm->userid, 0, sizeof(uidshm->userid));
	memset(uidshm->hashkey, 0, sizeof(uidshm->hashkey));
    memset(uidshm->alphakey, 0, sizeof(uidshm->alphakey));
    memset(uidshm->h_prev, 0, sizeof(uidshm->h_prev));
    memset(uidshm->h_next, 0, sizeof(uidshm->h_next));
    memset(uidshm->a_prev, 0, sizeof(uidshm->a_prev));
    memset(uidshm->a_next, 0, sizeof(uidshm->a_next));
    memset(uidshm->prev, 0, sizeof(uidshm->prev));
    memset(uidshm->next, 0, sizeof(uidshm->next));
    uidshm->first_used = 0;
    uidshm->first_available = 0;
    uidshm->number = 0;

	// Fill cache.
	for (i = 0; i < MAXUSERS; i++)
		fillucache(&(uidshm->passwd[i]), i);
	uidshm->uptime = time(NULL);

	// Unlock cache.
	ucache_unlock(fd);

	return 0;
}

int substitut_record(char *filename, void *rptr, size_t size, int id)
{
	memcpy(&(uidshm->passwd[id - 1]), rptr, size);
	return 0;
}

// Flushes cache for all users to PASSFILE.
int flush_ucache(void)
{
	return substitute_record(PASSFILE, uidshm->passwd,
			sizeof(uidshm->passwd), 1);
}

// Returns -1 if 'uidshm' == NULL and shared memory does not exist.
// Otherwise does nothing and returns 0.
int resolve_ucache(void)
{
	int iscreate = 0;
	if (uidshm == NULL) {
		uidshm = attach_shm2("UCACHE_SHMKEY", 3696, sizeof(*uidshm),
				&iscreate);
		if (uidshm == NULL)
			return -1;
	}
	if (iscreate) {
		remove_shm("UCACHE_SHMKEY", 3696, sizeof(*uidshm));
		report("Error: miscd is not running!", "");
		return -1;
	}
	return 0;
}

void setuserid(int num, char *userid)
{
    if (num > 0 && num <= MAXUSERS)
    {
        linkdel(num);
        strlcpy(uidshm->userid[num - 1], userid, IDLEN + 1);

        if (strcmp(userid, "new"))
            linkinsert(num);
    }
}

int searchnewuser(void)
{
    if (resolve_ucache() == -1)
        return 0;
    return uidshm->first_available;
}

// Get 'userid' according to ('uid' - 1).
int getuserid(char *userid, int uid, size_t len)
{
	if (resolve_ucache() == -1)
		return -1;
	strlcpy(userid, uidshm->userid[uid - 1], len);
	return 0;
}

// Returns the place of 'userid' in cache for all users, 0 if not found.
int searchuser(const char *userid)
{
	int i;
	int key;

	if (resolve_ucache() == -1)
		return 0;
	key = uhashkey(userid);
	i = uidshm->hashkey[key];
	while (i)
    {
		if (!strncasecmp(userid, uidshm->userid[i - 1],	sizeof(uidshm->userid[0])))
			break;
		i = uidshm->h_next[i - 1];
	}
	return i;
}

// Gets struct userec in cache for all users according to 'userid'.
// struct userec is stored in *'u'. Returns uid.
int getuserec(const char *userid, struct userec *u)
{
	int uid = searchuser(userid);
	if (uid > 0)
        *u = uidshm->passwd[uid - 1];
	return uid;
}

// Similar to 'getuserec',
// but stores result in global variable 'lookupuser'.
int getuser(const char *userid)
{
	int uid = searchuser(userid);
	if (uid > 0)
        lookupuser = uidshm->passwd[uid - 1];
	return uid;
}

// Puts struct userec in *'u' according to ('uid' - 1).
int getuserbyuid(struct userec *u, int uid)
{
	if (resolve_ucache() == -1)
		return -1;
	*u = uidshm->passwd[uid - 1];
	return uid;
}

int get_status(int uid)
{
	if (resolve_ucache() == -1)
		return 0;
	if (!HAS_PERM(PERM_SEECLOAK)
			&& (uidshm->passwd[uid - 1].userlevel & PERM_LOGINCLOAK)
			&& (uidshm->passwd[uid - 1].flags[0] & CLOAK_FLAG))
		return 0;
	return uidshm->status[uid - 1];
}

// If 'utmpshm' == NULL, gets shared memory for online users
// and puts its starting address in utmpshm.
void resolve_utmp(void)
{
	if (utmpshm == NULL) {
		utmpshm = attach_shm("UTMP_SHMKEY", 3699, sizeof(*utmpshm));
		if (utmpshm == NULL)
			exit(1); // TODO: leave to callers.
	}
}

// Returns realtime count of all users who has logged on more than once.
int allusers(void)
{
	if (resolve_ucache() == -1)
		return 0;
	struct userec *user;
	struct userec *end = uidshm->passwd 
			+ sizeof(uidshm->passwd) / sizeof(uidshm->passwd[0]);
	int count = 0;
	for (user = uidshm->passwd; user != end; ++user)
		count += (user->numlogins != 0 && user->userid[0] != '\0');
	return count;
}

// Returns (non-realtime) count of online users.
int get_online(void)
{
	resolve_utmp();
	return utmpshm->total_num;
}

// Refreshes utmp(cache for online users.)
int refresh_utmp(void)
{
	int utmpfd, ucachefd;
	struct user_info *uentp;
	int n;
	int count = 0; // Online users count.
	time_t now;

	resolve_utmp();
	if (resolve_ucache() == -1)
		return -1;
	now = time(NULL);
	// Lock caches.
	utmpfd = utmp_lock();
	if (utmpfd == -1)
		return -1;
	ucachefd = ucache_lock();
	if (ucachefd == -1)
		return -1;

	memset(uidshm->status, 0, sizeof(uidshm->status));
	for (n = 0; n < USHM_SIZE; n++) {
		uentp = &(utmpshm->uinfo[n]);
		if (uentp->active && uentp->pid) {
			 // See if pid exists.
			if (bbskill(uentp, 0) == -1) {
				memset(uentp, 0, sizeof(struct user_info));
				continue;
			} else {
				// Kick idle users out.
				if (uentp->mode != BBSNET
						&& now - uentp->idle_time > IDLE_TIMEOUT) {
					bbskill(uentp, SIGHUP);
					memset(uentp, 0, sizeof(struct user_info));
				} else {
					// Increase status.
					uidshm->status[uentp->uid - 1]++;
					// Count online users.
					++count;
				}
			}
		}
	}
	utmpshm->total_num = count;
	// Get count of all users from ucache.
	utmpshm->usersum = allusers();
	// Unlock caches.
	ucache_unlock(ucachefd);
	utmp_unlock(utmpfd);
	return count;
}

int getnewutmpent(struct user_info *up)
{
	int utmpfd, ucachefd;
	struct user_info *uentp;
	int i;

	resolve_utmp();
	if (resolve_ucache() == -1)
		return -1;

	utmpfd=utmp_lock();
	if (utmpfd == -1) {
		return -1;
	}

	if (utmpshm->max_login_num < get_online())
		utmpshm->max_login_num = get_online();
	for (i = 0; i < USHM_SIZE; i++) {
		uentp = &(utmpshm->uinfo[i]);
		if (!uentp->active || !uentp->pid)
			break;
	}
	if (i >= USHM_SIZE) {
		utmp_unlock(utmpfd);
		return -2;
	}
	utmpshm->uinfo[i] = *up;
	utmpshm->total_num++;

	utmp_unlock(utmpfd);

	ucachefd=ucache_lock();
	uidshm->status[up->uid-1]++;
	ucache_unlock(ucachefd);

	return i + 1;
}

int apply_ulist(int (*fptr)())
{
	struct user_info *uentp, utmp;
	int i, max;

	resolve_utmp();
	max = USHM_SIZE - 1;
	while (max > 0 && utmpshm->uinfo[max].active == 0)
		max--;
	for (i = 0; i <= max; i++) {
		uentp = &(utmpshm->uinfo[i]);
		utmp = *uentp;
		if ((*fptr)(&utmp) == QUIT)
			return QUIT;
	}
	return 0;
}

int search_ulist(struct user_info *uentp, int (*fptr)(), int farg)
{
	int i;
	resolve_utmp();
	for (i = 0; i < USHM_SIZE; i++) {
		*uentp = utmpshm->uinfo[i];
		if ((*fptr) (farg, uentp))
		return i + 1;
	}
	return 0;
}

int search_ulistn(struct user_info *uentp, int (*fptr)(), int farg, int unum)
{
	int i, j;
	j = 1;
	resolve_utmp();
	for (i = 0; i < USHM_SIZE; i++) {
		*uentp = utmpshm->uinfo[i];
		if ((*fptr) (farg, uentp)) {
			if (j == unum)
			return i + 1;
			else
			j++;
		}
	}
	return 0;
}

// Copies user_info *'uentp' to ('uent'th - 1) entry of utmp.
void update_ulist(struct user_info *uentp, int uent)
{
	resolve_utmp();
	if (uent > 0 && uent <= USHM_SIZE) {
		utmpshm->uinfo[uent - 1] = *uentp;
	}
}

/* added by djq 99.7.19*/
/* function added by douglas 990305
 set uentp to the user who is calling me
 solve the "one of 2 line call sb. to five" problem
 */

int who_callme(struct user_info *uentp, int (*fptr)(), int farg, int me)
{
	int i;

	resolve_utmp();
	for( i = 0; i < USHM_SIZE; i++ )
	{
		*uentp = utmpshm->uinfo[ i ];
		if( (*fptr)( farg, uentp ) && uentp->destuid==me )
		return i+1;
	}
	return 0;
}

// Returns count of online users.
int count_online(void)
{
	int i, total = 0;
	for (i = 0; i < MAXACTIVE; i++)
		if (utmpshm->uinfo[i].active)
			total++;
	return total;
}