/*
 * csapp.c - CS:APP3e 책에서 사용하는 함수 모음
 *
 * 2016/10 reb 업데이트:
 *   - sio_ltoa가 음수를 처리하지 못하던 버그 수정
 *
 * 2016/02 droh 업데이트:
 *   - open_clientfd와 open_listenfd가 더 점잖게 실패하도록 수정
 *
 * 2014/08 droh 업데이트:
 *   - open_clientfd와 open_listenfd의 새 버전을 재진입 가능하고
 *     프로토콜 독립적으로 수정
 *
 *   - 프로토콜 독립 함수 inet_ntop과 inet_pton 추가
 *     inet_ntoa와 inet_aton은 이제 구식 함수임
 *
 * 2014/07 droh 업데이트:
 *   - 재진입 가능한 sio(시그널 안전 I/O) 루틴 추가
 *
 * 2013/04 droh 업데이트:
 *   - rio_readlineb의 경계 상황 버그 수정
 *   - rio_readnb에서 불필요한 EINTR 검사 제거
 */
/* $begin csapp.c */
#include "csapp.h"

/**************************
 * 오류 처리 함수
 **************************/
/*
 * 이 섹션의 함수들은 "오류를 사람이 읽을 수 있는 문장으로 출력하고
 * 프로그램을 종료"하는 공통 역할을 맡는다.
 *
 * 차이는 어떤 종류의 에러 코드를 해석하느냐에 있다.
 * - unix_error: errno 기반 시스템 콜 에러
 * - posix_error: pthread 같은 POSIX 함수의 반환 코드
 * - gai_error: getaddrinfo/getnameinfo 계열의 에러 코드
 * - app_error: 애플리케이션이 직접 만든 일반 오류
 * - dns_error: 오래된 DNS API(gethostbyname 등)용 오류
 */
/* $begin errorfuns */
/* $begin unixerror */
/* errno를 strerror로 해석해서 출력한 뒤 종료한다. */
void unix_error(char *msg) /* 유닉스 스타일 오류 */
{
    fprintf(stderr, "%s: %s\n", msg, strerror(errno));
    exit(0);
}
/* $end unixerror */

/* pthread 계열처럼 errno 대신 반환값으로 에러 코드를 주는 함수용 래퍼다. */
void posix_error(int code, char *msg) /* POSIX 스타일 오류 */
{
    fprintf(stderr, "%s: %s\n", msg, strerror(code));
    exit(0);
}

/* getaddrinfo/getnameinfo 계열의 에러 코드를 gai_strerror로 해석한다. */
void gai_error(int code, char *msg) /* getaddrinfo 스타일 오류 */
{
    fprintf(stderr, "%s: %s\n", msg, gai_strerror(code));
    exit(0);
}

/* 시스템 에러 코드 없이, 프로그램이 직접 만든 오류 메시지를 출력한다. */
void app_error(char *msg) /* 애플리케이션 오류 */
{
    fprintf(stderr, "%s\n", msg);
    exit(0);
}
/* $end errorfuns */

/* 스레드 안전하지 않은 예전 DNS API용 에러 출력 함수다. */
void dns_error(char *msg) /* 구식 gethostbyname 오류 */
{
    fprintf(stderr, "%s\n", msg);
    exit(0);
}

/*********************************************
 * 유닉스 프로세스 제어 함수 래퍼
 ********************************************/

/* $begin forkwrapper */
/* fork를 호출하고, 실패하면 즉시 종료한다. */
pid_t Fork(void)
{
    pid_t pid;

    if ((pid = fork()) < 0)
        unix_error("Fork error");
    return pid;
}
/* $end forkwrapper */

/* 현재 프로세스 이미지를 새 프로그램으로 교체한다. */
void Execve(const char *filename, char *const argv[], char *const envp[])
{
    if (execve(filename, argv, envp) < 0)
        unix_error("Execve error");
}

/* $begin wait */
/* 자식 프로세스 하나가 끝날 때까지 기다린다. */
pid_t Wait(int *status)
{
    pid_t pid;

    if ((pid = wait(status)) < 0)
        unix_error("Wait error");
    return pid;
}
/* $end wait */

/* 특정 자식 또는 조건에 맞는 자식을 기다린다. */
pid_t Waitpid(pid_t pid, int *iptr, int options)
{
    pid_t retpid;

    if ((retpid = waitpid(pid, iptr, options)) < 0)
        unix_error("Waitpid error");
    return (retpid);
}

/* $begin kill */
/* 대상 프로세스에 시그널을 보낸다. */
void Kill(pid_t pid, int signum)
{
    int rc;

    if ((rc = kill(pid, signum)) < 0)
        unix_error("Kill error");
}
/* $end kill */

/* 시그널이 올 때까지 현재 프로세스를 잠시 멈춘다. */
void Pause()
{
    (void)pause();
    return;
}

/* 지정한 초만큼 잠들고, 남은 시간이 있으면 반환한다. */
unsigned int Sleep(unsigned int secs)
{
    unsigned int rc;

    if ((rc = sleep(secs)) < 0)
        unix_error("Sleep error");
    return rc;
}

/* 일정 시간이 지나면 SIGALRM이 오도록 예약한다. */
unsigned int Alarm(unsigned int seconds)
{
    return alarm(seconds);
}

/* 프로세스 그룹을 바꿔 잡 제어나 시그널 전달 범위를 조정할 때 쓴다. */
void Setpgid(pid_t pid, pid_t pgid)
{
    int rc;

    if ((rc = setpgid(pid, pgid)) < 0)
        unix_error("Setpgid error");
    return;
}

/* 현재 프로세스의 프로세스 그룹 ID를 돌려준다. */
pid_t Getpgrp(void)
{
    return getpgrp();
}

/************************************
 * 유닉스 시그널 함수 래퍼
 ***********************************/
/*
 * 이 섹션은 시그널 집합과 시그널 핸들러를 안전하게 다루기 위한 래퍼다.
 * 네트워크 서버에서는 EINTR 처리, 좀비 프로세스 회수, 타임아웃 등에 자주 등장한다.
 */

/* $begin sigaction */
/* sigaction 기반으로 핸들러를 등록한다. SA_RESTART를 켜서 느린 시스템 콜이 가능하면 자동 재시작되게 한다. */
handler_t *Signal(int signum, handler_t *handler)
{
    struct sigaction action, old_action;

    action.sa_handler = handler;
    sigemptyset(&action.sa_mask); /* 현재 처리 중인 종류의 시그널을 차단 */
    action.sa_flags = SA_RESTART; /* 가능하면 시스템 콜을 자동 재시작 */

    if (sigaction(signum, &action, &old_action) < 0)
        unix_error("Signal error");
    return (old_action.sa_handler);
}
/* $end sigaction */

/* 시그널 마스크를 변경해 특정 시그널을 잠시 막거나 해제한다. */
void Sigprocmask(int how, const sigset_t *set, sigset_t *oldset)
{
    if (sigprocmask(how, set, oldset) < 0)
        unix_error("Sigprocmask error");
    return;
}

/* 빈 시그널 집합을 만든다. */
void Sigemptyset(sigset_t *set)
{
    if (sigemptyset(set) < 0)
        unix_error("Sigemptyset error");
    return;
}

/* 모든 시그널이 들어 있는 집합을 만든다. */
void Sigfillset(sigset_t *set)
{
    if (sigfillset(set) < 0)
        unix_error("Sigfillset error");
    return;
}

/* 시그널 집합에 특정 시그널 하나를 추가한다. */
void Sigaddset(sigset_t *set, int signum)
{
    if (sigaddset(set, signum) < 0)
        unix_error("Sigaddset error");
    return;
}

/* 시그널 집합에서 특정 시그널 하나를 제거한다. */
void Sigdelset(sigset_t *set, int signum)
{
    if (sigdelset(set, signum) < 0)
        unix_error("Sigdelset error");
    return;
}

/* 특정 시그널이 집합에 포함되어 있는지 검사한다. */
int Sigismember(const sigset_t *set, int signum)
{
    int rc;
    if ((rc = sigismember(set, signum)) < 0)
        unix_error("Sigismember error");
    return rc;
}

/* 주어진 마스크 상태로 잠들었다가 시그널을 받으면 깨어난다. */
int Sigsuspend(const sigset_t *set)
{
    int rc = sigsuspend(set); /* 항상 -1을 반환한다 */
    if (errno != EINTR)
        unix_error("Sigsuspend error");
    return rc;
}

/*************************************************************
 * Sio(시그널 안전 I/O) 패키지 - 시그널 핸들러 안에서도 비교적 안전한
 * 단순 재진입 출력 함수 모음
 *************************************************************/
/*
 * SIO는 시그널 핸들러 안에서도 비교적 안전하게 쓸 수 있는 간단한 출력 도구다.
 * printf는 내부적으로 버퍼와 잠금을 쓰기 때문에 시그널 핸들러 안에서 쓰기 위험하지만,
 * 여기의 sio_* 함수들은 write 기반으로 최소 기능만 제공한다.
 */

/* 내부 sio 함수 */

/* $begin sioprivate */
/* sio_reverse - 문자열을 뒤집는다 (K&R에서 가져옴) */
/* 문자열을 뒤집는다. 정수를 문자열로 만든 뒤 순서를 바로잡을 때 사용한다. */
static void sio_reverse(char s[])
{
    int c, i, j;

    for (i = 0, j = strlen(s) - 1; i < j; i++, j--)
    {
        c = s[i];
        s[i] = s[j];
        s[j] = c;
    }
}

/* sio_ltoa - long 정수를 b진수 문자열로 바꾼다 (K&R에서 가져옴) */
/* long 정수를 원하는 진법의 문자열로 바꾼다. */
static void sio_ltoa(long v, char s[], int b)
{
    int c, i = 0;
    int neg = v < 0;

    if (neg)
        v = -v;

    do
    {
        s[i++] = ((c = (v % b)) < 10) ? c + '0' : c - 10 + 'a';
    } while ((v /= b) > 0);

    if (neg)
        s[i++] = '-';

    s[i] = '\0';
    sio_reverse(s);
}

/* sio_strlen - 문자열 길이를 반환한다 (K&R에서 가져옴) */
/* 널 종료 문자열의 길이를 직접 센다. */
static size_t sio_strlen(char s[])
{
    int i = 0;

    while (s[i] != '\0')
        ++i;
    return i;
}
/* $end sioprivate */

/* 공개 Sio 함수 */
/* $begin siopublic */

/* 문자열을 표준 출력으로 그대로 쓴다. */
ssize_t sio_puts(char s[]) /* 문자열 출력 */
{
    return write(STDOUT_FILENO, s, sio_strlen(s)); // line:csapp:siostrlen
}

/* 정수를 문자열로 바꾼 뒤 표준 출력으로 출력한다. */
ssize_t sio_putl(long v) /* long 정수 출력 */
{
    char s[128];

    sio_ltoa(v, s, 10); /* K&R의 itoa() 아이디어를 바탕으로 함 */ // line:csapp:sioltoa
    return sio_puts(s);
}

/* 에러 문자열을 출력하고 _exit로 즉시 종료한다. */
void sio_error(char s[]) /* 오류 메시지를 출력하고 종료 */
{
    sio_puts(s);
    _exit(1); // line:csapp:sioexit
}
/* $end siopublic */

/*******************************
 * SIO 루틴용 래퍼
 ******************************/
/* sio_putl에 에러 검사를 덧붙인 래퍼다. */
ssize_t Sio_putl(long v)
{
    ssize_t n;

    if ((n = sio_putl(v)) < 0)
        sio_error("Sio_putl error");
    return n;
}

/* sio_puts에 에러 검사를 덧붙인 래퍼다. */
ssize_t Sio_puts(char s[])
{
    ssize_t n;

    if ((n = sio_puts(s)) < 0)
        sio_error("Sio_puts error");
    return n;
}

/* sio_error를 그대로 호출하는 래퍼다. */
void Sio_error(char s[])
{
    sio_error(s);
}

/********************************
 * 유닉스 I/O 루틴 래퍼
 ********************************/
/*
 * 이 섹션의 함수들은 파일 디스크립터 기반 저수준 I/O를 감싼다.
 * 반환값은 원래 시스템 콜과 거의 같고, 실패 시만 unix_error로 종료한다.
 */

/* 파일을 열고 새 파일 디스크립터를 반환한다. */
int Open(const char *pathname, int flags, mode_t mode)
{
    int rc;

    if ((rc = open(pathname, flags, mode)) < 0)
        unix_error("Open error");
    return rc;
}

/* 파일 디스크립터에서 지정한 바이트 수만큼 읽는다. */
ssize_t Read(int fd, void *buf, size_t count)
{
    ssize_t rc;

    if ((rc = read(fd, buf, count)) < 0)
        unix_error("Read error");
    return rc;
}

/* 파일 디스크립터에 지정한 바이트 수만큼 쓴다. */
ssize_t Write(int fd, const void *buf, size_t count)
{
    ssize_t rc;

    if ((rc = write(fd, buf, count)) < 0)
        unix_error("Write error");
    return rc;
}

/* 파일 오프셋을 이동한다. 임의 위치 읽기/쓰기에 쓴다. */
off_t Lseek(int fildes, off_t offset, int whence)
{
    off_t rc;

    if ((rc = lseek(fildes, offset, whence)) < 0)
        unix_error("Lseek error");
    return rc;
}

/* 파일 디스크립터를 닫는다. */
void Close(int fd)
{
    int rc;

    if ((rc = close(fd)) < 0)
        unix_error("Close error");
}

/* 여러 디스크립터를 동시에 감시할 때 쓰는 select 래퍼다. */
int Select(int n, fd_set *readfds, fd_set *writefds,
           fd_set *exceptfds, struct timeval *timeout)
{
    int rc;

    if ((rc = select(n, readfds, writefds, exceptfds, timeout)) < 0)
        unix_error("Select error");
    return rc;
}

/* fd1을 fd2에 복제한다. 입출력 리다이렉션에 자주 사용한다. */
int Dup2(int fd1, int fd2)
{
    int rc;

    if ((rc = dup2(fd1, fd2)) < 0)
        unix_error("Dup2 error");
    return rc;
}

/* 경로 이름 기준으로 파일 메타데이터를 읽는다. */
void Stat(const char *filename, struct stat *buf)
{
    if (stat(filename, buf) < 0)
        unix_error("Stat error");
}

/* 이미 열린 파일 디스크립터 기준으로 파일 메타데이터를 읽는다. */
void Fstat(int fd, struct stat *buf)
{
    if (fstat(fd, buf) < 0)
        unix_error("Fstat error");
}

/*********************************
 * 디렉터리 함수 래퍼
 *********************************/
/* 디렉터리를 열고 DIR 포인터를 반환한다. */

DIR *Opendir(const char *name)
{
    DIR *dirp = opendir(name);

    if (!dirp)
        unix_error("opendir error");
    return dirp;
}

/* 디렉터리 엔트리를 하나씩 읽는다. NULL은 파일 끝(EOF)일 수도 있으므로 errno를 함께 본다. */
struct dirent *Readdir(DIR *dirp)
{
    struct dirent *dep;

    errno = 0;
    dep = readdir(dirp);
    if ((dep == NULL) && (errno != 0))
        unix_error("readdir error");
    return dep;
}

/* 열려 있는 디렉터리 스트림을 닫는다. */
int Closedir(DIR *dirp)
{
    int rc;

    if ((rc = closedir(dirp)) < 0)
        unix_error("closedir error");
    return rc;
}

/***************************************
 * 메모리 매핑 함수 래퍼
 ***************************************/
/* 파일이나 익명 메모리를 프로세스 주소 공간에 매핑한다. */
void *Mmap(void *addr, size_t len, int prot, int flags, int fd, off_t offset)
{
    void *ptr;

    if ((ptr = mmap(addr, len, prot, flags, fd, offset)) == ((void *)-1))
        unix_error("mmap error");
    return (ptr);
}

/* mmap으로 잡은 매핑을 해제한다. */
void Munmap(void *start, size_t length)
{
    if (munmap(start, length) < 0)
        unix_error("munmap error");
}

/***************************************************
 * 동적 메모리 할당 함수 래퍼
 ***************************************************/
/* malloc의 실패를 검사하는 래퍼다. */

void *Malloc(size_t size)
{
    void *p;

    if ((p = malloc(size)) == NULL)
        unix_error("Malloc error");
    return p;
}

/* realloc의 실패를 검사하는 래퍼다. */
void *Realloc(void *ptr, size_t size)
{
    void *p;

    if ((p = realloc(ptr, size)) == NULL)
        unix_error("Realloc error");
    return p;
}

/* calloc의 실패를 검사하는 래퍼다. */
void *Calloc(size_t nmemb, size_t size)
{
    void *p;

    if ((p = calloc(nmemb, size)) == NULL)
        unix_error("Calloc error");
    return p;
}

/* 동적 메모리를 해제한다. free는 실패 코드를 주지 않으므로 단순 호출이다. */
void Free(void *ptr)
{
    free(ptr);
}

/******************************************
 * 표준 I/O 함수 래퍼
 ******************************************/
/*
 * 표준 입출력(FILE *) 기반 I/O를 다루는 래퍼들이다.
 * fopen/fgets/fputs/fread/fwrite 같은 표준 라이브러리 호출의 실패를
 * 공통 방식으로 처리하기 위해 모아 두었다.
 */

/* FILE 형식의 스트림을 닫는다. */
void Fclose(FILE *fp)
{
    if (fclose(fp) != 0)
        unix_error("Fclose error");
}

/* 이미 존재하는 fd를 FILE * 스트림으로 감싼다. */
FILE *Fdopen(int fd, const char *type)
{
    FILE *fp;

    if ((fp = fdopen(fd, type)) == NULL)
        unix_error("Fdopen error");

    return fp;
}

/* 한 줄을 읽는다. 파일 끝(EOF)과 오류를 구분하기 위해 ferror를 함께 본다. */
char *Fgets(char *ptr, int n, FILE *stream)
{
    char *rptr;

    if (((rptr = fgets(ptr, n, stream)) == NULL) && ferror(stream))
        app_error("Fgets error");

    return rptr;
}

/* 경로 이름으로 파일을 열고 FILE *를 얻는다. */
FILE *Fopen(const char *filename, const char *mode)
{
    FILE *fp;

    if ((fp = fopen(filename, mode)) == NULL)
        unix_error("Fopen error");

    return fp;
}

/* 문자열을 표준 입출력 스트림에 출력한다. */
void Fputs(const char *ptr, FILE *stream)
{
    if (fputs(ptr, stream) == EOF)
        unix_error("Fputs error");
}

/* 원소 nmemb개를 읽는다. 일부만 읽은 경우 에러인지 EOF인지 ferror로 구분한다. */
size_t Fread(void *ptr, size_t size, size_t nmemb, FILE *stream)
{
    size_t n;

    if (((n = fread(ptr, size, nmemb, stream)) < nmemb) && ferror(stream))
        unix_error("Fread error");
    return n;
}

/* 원소 nmemb개를 쓴다. */
void Fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream)
{
    if (fwrite(ptr, size, nmemb, stream) < nmemb)
        unix_error("Fwrite error");
}

/****************************
 * 소켓 인터페이스 래퍼
 ****************************/
/*
 * 소켓 생성, 옵션 설정, bind/listen/accept/connect 같은 기본 네트워크
 * 시스템 콜을 감싼 래퍼들이다. 프록시, 웹 서버, 클라이언트 구현에서
 * 반복적으로 사용하는 저수준 API를 단순화한다.
 */

/* 새 소켓 디스크립터를 만든다. */
int Socket(int domain, int type, int protocol)
{
    int rc;

    if ((rc = socket(domain, type, protocol)) < 0)
        unix_error("Socket error");
    return rc;
}

/* 소켓 옵션(SO_REUSEADDR 등)을 설정한다. */
void Setsockopt(int s, int level, int optname, const void *optval, int optlen)
{
    int rc;

    if ((rc = setsockopt(s, level, optname, optval, optlen)) < 0)
        unix_error("Setsockopt error");
}

/* 소켓에 로컬 주소와 포트를 결합한다. */
void Bind(int sockfd, struct sockaddr *my_addr, int addrlen)
{
    int rc;

    if ((rc = bind(sockfd, my_addr, addrlen)) < 0)
        unix_error("Bind error");
}

/* 소켓을 리스닝 상태로 바꿔 연결 요청을 받을 준비를 한다. */
void Listen(int s, int backlog)
{
    int rc;

    if ((rc = listen(s, backlog)) < 0)
        unix_error("Listen error");
}

/* 리스닝 소켓에서 새 연결 하나를 받아 연결 소켓을 반환한다. */
int Accept(int s, struct sockaddr *addr, socklen_t *addrlen)
{
    int rc;

    if ((rc = accept(s, addr, addrlen)) < 0)
        unix_error("Accept error");
    return rc;
}

/* 원격 서버에 TCP 연결을 시도한다. */
void Connect(int sockfd, struct sockaddr *serv_addr, int addrlen)
{
    int rc;

    if ((rc = connect(sockfd, serv_addr, addrlen)) < 0)
        unix_error("Connect error");
}

/*******************************
 * 프로토콜 독립 래퍼
 *******************************/
/*
 * 이 섹션은 IPv4/IPv6 차이를 직접 의식하지 않고 주소를 다룰 수 있게 해 준다.
 * 현대 네트워크 코드에서는 gethostbyname보다 getaddrinfo/getnameinfo를
 * 쓰는 쪽이 표준적이고 스레드 안전하다.
 */
/* $begin getaddrinfo */
/* 호스트 이름/서비스 이름을 실제 소켓 주소 리스트로 변환한다. */
void Getaddrinfo(const char *node, const char *service,
                 const struct addrinfo *hints, struct addrinfo **res)
{
    int rc;

    if ((rc = getaddrinfo(node, service, hints, res)) != 0)
        gai_error(rc, "Getaddrinfo error");
}
/* $end getaddrinfo */

/* 바이너리 sockaddr를 사람이 읽을 수 있는 호스트/서비스 문자열로 바꾼다. */
void Getnameinfo(const struct sockaddr *sa, socklen_t salen, char *host,
                 size_t hostlen, char *serv, size_t servlen, int flags)
{
    int rc;

    if ((rc = getnameinfo(sa, salen, host, hostlen, serv,
                          servlen, flags)) != 0)
        gai_error(rc, "Getnameinfo error");
}

/* getaddrinfo가 할당한 연결 리스트를 해제한다. */
void Freeaddrinfo(struct addrinfo *res)
{
    freeaddrinfo(res);
}

/* 네트워크 주소를 "127.0.0.1" 같은 문자열로 바꾼다. */
void Inet_ntop(int af, const void *src, char *dst, socklen_t size)
{
    if (!inet_ntop(af, src, dst, size))
        unix_error("Inet_ntop error");
}

/* 문자열 IP 주소를 바이너리 주소 구조로 바꾼다. */
void Inet_pton(int af, const char *src, void *dst)
{
    int rc;

    rc = inet_pton(af, src, dst);
    if (rc == 0)
        app_error("inet_pton error: invalid dotted-decimal address");
    else if (rc < 0)
        unix_error("Inet_pton error");
}

/*******************************************
 * DNS 인터페이스 래퍼
 *
 * 참고: 아래 함수들은 스레드 안전하지 않으므로 이제는 구식이다.
 * 대신 getaddrinfo와 getnameinfo를 사용하는 편이 좋다.
 ***********************************/
/*
 * 아래 두 함수는 예전 스타일의 DNS 조회 함수다.
 * 교재 이해에는 도움이 되지만, 멀티스레드 코드에서는 안전하지 않아서
 * 현대 코드에서는 보통 getaddrinfo/getnameinfo로 대체한다.
 */

/* $begin gethostbyname */
/* 호스트 이름을 hostent 구조체로 바꾼다. */
struct hostent *Gethostbyname(const char *name)
{
    struct hostent *p;

    if ((p = gethostbyname(name)) == NULL)
        dns_error("Gethostbyname error");
    return p;
}
/* $end gethostbyname */

/* 바이너리 주소를 hostent 구조체로 역변환한다. */
struct hostent *Gethostbyaddr(const char *addr, int len, int type)
{
    struct hostent *p;

    if ((p = gethostbyaddr(addr, len, type)) == NULL)
        dns_error("Gethostbyaddr error");
    return p;
}

/************************************************
 * Pthreads 스레드 제어 함수 래퍼
 ************************************************/
/*
 * 멀티스레드 프로그램에서 스레드를 만들고, 기다리고, 분리하고,
 * 종료시키는 POSIX pthread API 래퍼들이다.
 */

/* 새 스레드를 하나 생성한다. */
void Pthread_create(pthread_t *tidp, pthread_attr_t *attrp,
                    void *(*routine)(void *), void *argp)
{
    int rc;

    if ((rc = pthread_create(tidp, attrp, routine, argp)) != 0)
        posix_error(rc, "Pthread_create error");
}

/* 대상 스레드에 취소 요청을 보낸다. */
void Pthread_cancel(pthread_t tid)
{
    int rc;

    if ((rc = pthread_cancel(tid)) != 0)
        posix_error(rc, "Pthread_cancel error");
}

/* 대상 스레드가 끝날 때까지 기다리고 반환값을 받는다. */
void Pthread_join(pthread_t tid, void **thread_return)
{
    int rc;

    if ((rc = pthread_join(tid, thread_return)) != 0)
        posix_error(rc, "Pthread_join error");
}

/* $begin detach */
/* 스레드를 detach 상태로 바꿔, 종료 시 자원이 자동 회수되게 한다. */
void Pthread_detach(pthread_t tid)
{
    int rc;

    if ((rc = pthread_detach(tid)) != 0)
        posix_error(rc, "Pthread_detach error");
}
/* $end detach */

/* 현재 스레드를 종료하고, 선택적으로 반환값을 남긴다. */
void Pthread_exit(void *retval)
{
    pthread_exit(retval);
}

/* 현재 실행 중인 스레드의 ID를 반환한다. */
pthread_t Pthread_self(void)
{
    return pthread_self();
}

/* 여러 스레드가 와도 초기화 함수가 정확히 한 번만 실행되게 한다. */
void Pthread_once(pthread_once_t *once_control, void (*init_function)())
{
    pthread_once(once_control, init_function);
}

/*******************************
 * POSIX 세마포어 래퍼
 *******************************/
/*
 * 세마포어는 공유 자원 접근을 직렬화하거나, 생산자-소비자처럼
 * 스레드 사이의 진행 순서를 맞출 때 사용한다.
 */

/* 세마포어의 초기값을 설정한다. */
void Sem_init(sem_t *sem, int pshared, unsigned int value)
{
    if (sem_init(sem, pshared, value) < 0)
        unix_error("Sem_init error");
}

/* 세마포어를 1 감소시키며, 값이 0이면 기다린다. */
void P(sem_t *sem)
{
    if (sem_wait(sem) < 0)
        unix_error("P error");
}

/* 세마포어를 1 증가시켜 대기 중인 스레드를 깨울 수 있게 한다. */
void V(sem_t *sem)
{
    if (sem_post(sem) < 0)
        unix_error("V error");
}

/****************************************
 * Rio 패키지 - 견고한 I/O 함수
 ****************************************/
/*
 * RIO(Robust I/O)는 네트워크 프로그래밍에서 자주 생기는 두 문제를 줄여 준다.
 * 1. read/write가 EINTR로 끊기거나 일부 바이트만 처리되는 문제
 * 2. 한 줄 단위 텍스트를 안정적으로 읽고 싶은 요구
 *
 * rio_readn / rio_writen:
 * - 버퍼링 없는 저수준 함수
 * - "정확히 n바이트"를 목표로 반복 호출한다.
 *
 * rio_readinitb / rio_read / rio_readnb / rio_readlineb:
 * - 내부 버퍼를 두는 고수준 읽기 함수
 * - HTTP 헤더처럼 줄 단위 입력을 받을 때 특히 편하다.
 */

/*
 * rio_readn - n바이트를 안정적으로 읽는다 (버퍼링 없음)
 */
/* $begin rio_readn */
/* 파일 끝(EOF)이나 오류가 나기 전까지 n바이트를 최대한 끝까지 읽는다. */
ssize_t rio_readn(int fd, void *usrbuf, size_t n)
{
    size_t nleft = n;
    ssize_t nread;
    char *bufp = usrbuf;

    while (nleft > 0)
    {
        if ((nread = read(fd, bufp, nleft)) < 0)
        {
            if (errno == EINTR) /* 시그널 핸들러 복귀로 중단됨 */
                nread = 0;      /* 그리고 read()를 다시 호출 */
            else
                return -1; /* errno는 read()가 설정 */
        }
        else if (nread == 0)
            break; /* 파일 끝 */
        nleft -= nread;
        bufp += nread;
    }
    return (n - nleft); /* 0 이상 반환 */
}
/* $end rio_readn */

/*
 * rio_writen - n바이트를 안정적으로 쓴다 (버퍼링 없음)
 */
/* $begin rio_writen */
/* write가 일부만 쓰더라도 남은 바이트를 끝까지 다시 쓴다. */
ssize_t rio_writen(int fd, void *usrbuf, size_t n)
{
    size_t nleft = n;
    ssize_t nwritten;
    char *bufp = usrbuf;

    while (nleft > 0)
    {
        if ((nwritten = write(fd, bufp, nleft)) <= 0)
        {
            /**
             *  대표 상황:

                read(), write(), accept(), select() 같은
                블로킹 시스템 콜 수행 중

                👉 그때

                SIGINT (Ctrl+C)
                SIGALRM
                기타 signal

                이 오면…

                👉 커널이 시스템 콜을 멈추고 -1 반환
                👉 그리고 errno = EINTR

                -> 오류가 아님 잠시 끊긴 것 -> 다시 시도 하면 됨
             */
            if (errno == EINTR) /* 시그널 핸들러 복귀로 중단됨 */
                nwritten = 0;   /* 그리고 write()를 다시 호출 */
            else
                return -1; /* errno는 write()가 설정 */
        }
        nleft -= nwritten;
        bufp += nwritten;
    }
    return n;
}
/* $end rio_writen */

/*
 * rio_read - 유닉스 read()를 감싼 내부 도우미 함수다.
 *    내부 버퍼에서 사용자 버퍼로 min(n, rio_cnt) 바이트를 복사한다.
 *    여기서 n은 사용자가 요청한 바이트 수이고,
 *    rio_cnt는 내부 버퍼에 남아 있는 미사용 바이트 수다.
 *    함수 진입 시 내부 버퍼가 비어 있으면 read()를 호출해
 *    버퍼를 다시 채운다.
 */
/* $begin rio_read */
/* 내부 버퍼에서 먼저 읽고, 비어 있으면 커널에서 새로 채워 넣는다. */
static ssize_t rio_read(rio_t *rp, char *usrbuf, size_t n)
{
    int cnt;

    while (rp->rio_cnt <= 0)
    { /* 버퍼가 비어 있으면 다시 채움 */
        rp->rio_cnt = read(rp->rio_fd, rp->rio_buf,
                           sizeof(rp->rio_buf));
        if (rp->rio_cnt < 0)
        {
            if (errno != EINTR) /* 시그널 핸들러 복귀로 중단된 경우 제외 */
                return -1;
        }
        else if (rp->rio_cnt == 0) /* 파일 끝 */
            return 0;
        else
            rp->rio_bufptr = rp->rio_buf; /* 버퍼 포인터 초기화 */
    }

    /* 내부 버퍼에서 사용자 버퍼로 min(n, rp->rio_cnt) 바이트 복사 */
    cnt = n;
    if (rp->rio_cnt < n)
        cnt = rp->rio_cnt;
    memcpy(usrbuf, rp->rio_bufptr, cnt);
    rp->rio_bufptr += cnt;
    rp->rio_cnt -= cnt;
    return cnt;
}
/* $end rio_read */

/*
 * rio_readinitb - 디스크립터를 읽기 버퍼와 연결하고 버퍼 상태를 초기화한다
 */
/* $begin rio_readinitb */
/* rio_t 구조체를 특정 fd와 연결하고 내부 버퍼 상태를 초기화한다. */
void rio_readinitb(rio_t *rp, int fd)
{
    rp->rio_fd = fd;
    rp->rio_cnt = 0;
    rp->rio_bufptr = rp->rio_buf;
}
/* $end rio_readinitb */

/*
 * rio_readnb - n바이트를 안정적으로 읽는다 (버퍼링 있음)
 */
/* $begin rio_readnb */
/* 내부 버퍼를 활용해 최대 n바이트를 안정적으로 읽는다. */
ssize_t rio_readnb(rio_t *rp, void *usrbuf, size_t n)
{
    size_t nleft = n;
    ssize_t nread;
    char *bufp = usrbuf;

    while (nleft > 0)
    {
        if ((nread = rio_read(rp, bufp, nleft)) < 0)
            return -1; /* errno는 read()가 설정 */
        else if (nread == 0)
            break; /* 파일 끝 */
        nleft -= nread;
        bufp += nread;
    }
    return (n - nleft); /* 0 이상 반환 */
}
/* $end rio_readnb */

/*
 * rio_readlineb - 텍스트 한 줄을 안정적으로 읽는다 (버퍼링 있음)
 */
/* $begin rio_readlineb */
/* 개행 문자를 만날 때까지 한 줄을 읽는다. 텍스트 프로토콜 처리에 유용하다. */
ssize_t rio_readlineb(rio_t *rp, void *usrbuf, size_t maxlen)
{
    int n, rc;
    char c, *bufp = usrbuf;

    for (n = 1; n < maxlen; n++)
    {
        if ((rc = rio_read(rp, &c, 1)) == 1)
        {
            *bufp++ = c;
            if (c == '\n')
            {
                n++;
                break;
            }
        }
        else if (rc == 0)
        {
            if (n == 1)
                return 0; /* 파일 끝, 읽은 데이터 없음 */
            else
                break; /* 파일 끝, 일부 데이터는 읽음 */
        }
        else
            return -1; /* 오류 */
    }
    *bufp = 0;
    return n - 1;
}
/* $end rio_readlineb */

/**********************************
 * Robust I/O 루틴 래퍼
 **********************************/
/* 아래 함수들은 rio_* 계열에 에러 종료 정책을 붙인 래퍼들이다. */

/* 버퍼링 없이 n바이트 읽기. */
ssize_t Rio_readn(int fd, void *ptr, size_t nbytes)
{
    ssize_t n;

    if ((n = rio_readn(fd, ptr, nbytes)) < 0)
        unix_error("Rio_readn error");
    return n;
}

/* 버퍼링 없이 n바이트 쓰기. */
void Rio_writen(int fd, void *usrbuf, size_t n)
{
    if (rio_writen(fd, usrbuf, n) != n)
        unix_error("Rio_writen error");
}

/* 내부 버퍼 기반 읽기를 시작하기 위한 초기화 함수다. */
void Rio_readinitb(rio_t *rp, int fd)
{
    rio_readinitb(rp, fd);
}

/* 내부 버퍼를 이용해 n바이트 읽는다. */
ssize_t Rio_readnb(rio_t *rp, void *usrbuf, size_t n)
{
    ssize_t rc;

    if ((rc = rio_readnb(rp, usrbuf, n)) < 0)
        unix_error("Rio_readnb error");
    return rc;
}

/* 내부 버퍼를 이용해 한 줄을 읽는다. */
ssize_t Rio_readlineb(rio_t *rp, void *usrbuf, size_t maxlen)
{
    ssize_t rc;

    if ((rc = rio_readlineb(rp, usrbuf, maxlen)) < 0)
        unix_error("Rio_readlineb error");
    return rc;
}

/********************************
 * 클라이언트/서버 헬퍼 함수
 ********************************/
/*
 * open_clientfd / open_listenfd는 교재에서 가장 많이 재사용하는
 * 네트워크 헬퍼다. getaddrinfo를 이용해 프로토콜 독립적으로 주소를
 * 찾고, 실제로 연결 또는 리슨 가능한 소켓 하나를 골라 반환한다.
 */
/*
 * open_clientfd - <호스트 이름, 포트> 서버에 연결을 열고
 *     읽기/쓰기가 가능한 소켓 디스크립터를 반환한다.
 *     이 함수는 재진입 가능하고 프로토콜 독립적이다.
 *
 *     오류 시 반환값:
 *       -2: getaddrinfo 오류
 *       -1: 그 외 오류 (errno 설정됨)
 */
/* $begin open_clientfd */
/* 호스트 이름과 포트로 접속 가능한 소켓 하나를 찾아 연결한다. */
int open_clientfd(char *hostname, char *port)
{
    int clientfd, rc;
    struct addrinfo hints, *listp, *p;

    /* 이름 해석 결과로 나올 수 있는 후보 주소들을 얻는다. */
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_socktype = SOCK_STREAM; /* 연결을 여는 소켓 */
    hints.ai_flags = AI_NUMERICSERV; /* 숫자형 포트 인자 사용 */
    hints.ai_flags |= AI_ADDRCONFIG; /* 연결용으로 권장되는 설정 */
    if ((rc = getaddrinfo(hostname, port, &hints, &listp)) != 0)
    {
        fprintf(stderr, "getaddrinfo failed (%s:%s): %s\n", hostname, port, gai_strerror(rc));
        return -2;
    }

    /* 후보 주소들을 돌면서 실제 연결이 성공하는 첫 번째 소켓을 찾는다. */
    for (p = listp; p; p = p->ai_next)
    {
        /* 현재 후보 주소에 맞는 소켓을 생성한다. */
        if ((clientfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) < 0)
            continue; /* 소켓 생성 실패, 다음 후보 시도 */

        /* 연결이 성공하면 이 소켓을 그대로 반환한다. */
        if (connect(clientfd, p->ai_addr, p->ai_addrlen) != -1)
            break; /* 성공 */
        if (close(clientfd) < 0)
        { /* 연결 실패, 다른 후보 시도 */ // line:netp:openclientfd:closefd
            fprintf(stderr, "open_clientfd: close failed: %s\n", strerror(errno));
            return -1;
        }
    }

    /* 주소 후보 리스트는 getaddrinfo가 할당했으므로 반드시 해제한다. */
    freeaddrinfo(listp);
    if (!p) /* 모든 연결 시도 실패 */
        return -1;
    else /* 마지막 연결 시도는 성공 */
        return clientfd;
}
/* $end open_clientfd */

/*
 * open_listenfd - 주어진 포트에서 리스닝 소켓을 열고 반환한다.
 *     이 함수는 재진입 가능하고 프로토콜 독립적이다.
 *
 *     오류 시 반환값:
 *       -2: getaddrinfo 오류
 *       -1: 그 외 오류 (errno 설정됨)
 */
/* $begin open_listenfd */
/* 주어진 포트에서 연결 요청을 받을 수 있는 리스닝 소켓 하나를 연다. */
int open_listenfd(char *port)
{
    struct addrinfo hints, *listp, *p;
    int listenfd, rc, optval = 1;

    /* 로컬 머신에서 바인드할 수 있는 후보 주소 목록을 가져온다. */
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_socktype = SOCK_STREAM;             /* 연결 요청을 받는 소켓 */
    hints.ai_flags = AI_PASSIVE | AI_ADDRCONFIG; /* 모든 로컬 IP 주소에서 대기 */
    hints.ai_flags |= AI_NUMERICSERV;            /* 숫자형 포트 번호 사용 */
    if ((rc = getaddrinfo(NULL, port, &hints, &listp)) != 0)
    {
        fprintf(stderr, "getaddrinfo failed (port %s): %s\n", port, gai_strerror(rc));
        return -2;
    }

    /* 바인드가 실제로 성공하는 주소 하나를 찾을 때까지 순회한다. */
    for (p = listp; p; p = p->ai_next)
    {
        /* 현재 주소 후보용 소켓을 생성한다. */
        if ((listenfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) < 0)
            continue; /* 소켓 생성 실패, 다음 후보 시도 */

        /* 재실행 직후에도 포트를 다시 바인드하기 쉽도록 옵션을 켠다. */
        setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, // line:netp:csapp:setsockopt
                   (const void *)&optval, sizeof(int));

        /* 바인드에 성공하면 이 주소를 사용한다. */
        if (bind(listenfd, p->ai_addr, p->ai_addrlen) == 0)
            break; /* 성공 */
        if (close(listenfd) < 0)
        { /* 바인드 실패, 다음 후보 시도 */
            fprintf(stderr, "open_listenfd close failed: %s\n", strerror(errno));
            return -1;
        }
    }

    /* 후보 주소 목록 정리 */
    freeaddrinfo(listp);
    if (!p) /* 어떤 주소 후보도 동작하지 않음 */
        return -1;

    /* 마지막으로 listen을 걸어 연결 요청을 받을 수 있는 소켓으로 만든다. */
    if (listen(listenfd, LISTENQ) < 0)
    {
        close(listenfd);
        return -1;
    }
    return listenfd;
}
/* $end open_listenfd */

/****************************************************
 * 재진입 가능한 프로토콜 독립 헬퍼 래퍼
 ****************************************************/
/* open_clientfd의 실패를 unix_error 정책으로 감싼 래퍼다. */
int Open_clientfd(char *hostname, char *port)
{
    int rc;

    if ((rc = open_clientfd(hostname, port)) < 0)
        unix_error("Open_clientfd error");
    return rc;
}

/* open_listenfd의 실패를 unix_error 정책으로 감싼 래퍼다. */
int Open_listenfd(char *port)
{
    int rc;

    if ((rc = open_listenfd(port)) < 0)
        unix_error("Open_listenfd error");
    return rc;
}

/********************************
 * CS:APP 예제용 헬퍼 함수
 ********************************/
/*
 * echo - 서버 쪽 연결 소켓에서 한 줄씩 읽어 같은 내용을 그대로 되돌려 보낸다.
 *
 * 이 함수는 CS:APP의 기본 echo 서버 예제에서 사용하는 보조 함수다.
 * connfd는 이미 accept가 끝난 "연결된 소켓"이어야 한다.
 */
void echo(int connfd)
{
    size_t n;
    char buf[MAXLINE];
    rio_t rio;

    /* 연결 소켓을 RIO 읽기 버퍼와 연결한다. */
    Rio_readinitb(&rio, connfd);

    /*
     * 클라이언트가 보낸 텍스트를 한 줄씩 읽는다.
     * 읽은 길이만큼 다시 그대로 보내면 echo 동작이 된다.
     */
    while ((n = Rio_readlineb(&rio, buf, MAXLINE)) != 0)
    {
        printf("server received %zu bytes\n", n);
        Rio_writen(connfd, buf, n);
    }
}

/* $end csapp.c */
