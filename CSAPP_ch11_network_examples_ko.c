/*
 * CSAPP_ch11_network_examples_ko.c
 *
 * 목적:
 *   CSAPP 11장(네트워크 프로그래밍)의 핵심 흐름을
 *   "읽기 쉬운 교육용 예제"로 다시 작성한 파일이다.
 *
 * 특징:
 *   - POSIX/Linux 스타일 소켓 API를 기준으로 작성했다.
 *   - 책의 개념을 그대로 따라가되, 설명용 주석을 자세히 달았다.
 *   - 실서비스용 코드가 아니라 학습용 코드다.
 *
 * 들어 있는 예제:
 *   1) open_clientfd_ko  : 클라이언트 연결 헬퍼
 *   2) open_listenfd_ko  : 서버 listen 소켓 헬퍼
 *   3) run_echo_client_ko / run_iterative_echo_server_ko
 *   4) Tiny 스타일 HTTP 서버 흐름(handle_http_transaction_ko)
 *
 * 예시 실행(리눅스/WSL 기준):
 *   gcc -Wall -Wextra -std=c11 CSAPP_ch11_network_examples_ko.c -o ch11demo
 *   ./ch11demo echo-server 9000
 *   ./ch11demo echo-client localhost 9000
 *   ./ch11demo tiny-server 8000
 *
 * tiny-server 모드는 현재 작업 디렉터리에 정적 파일과 cgi-bin 실행 파일이
 * 있다고 가정한다. 예를 들어 webproxy-lab/tiny 디렉터리에서 실행하면,
 * home.html 과 cgi-bin/adder 같은 파일을 바로 실험할 수 있다.
 */

#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <netdb.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define MAXLINE_KO 4096
#define LISTENQ_KO 1024

static void warnx_ko(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
}

static void die_ko(const char *msg)
{
    perror(msg);
    exit(EXIT_FAILURE);
}

static void ignore_sigpipe_ko(void)
{
    /*
     * 실전 서버에서는 이미 닫힌 연결에 write 할 수 있다.
     * 기본 동작대로 두면 SIGPIPE 로 프로세스가 종료될 수 있으므로,
     * 교육용 예제에서도 이를 무시해 두는 편이 디버깅이 편하다.
     */
    if (signal(SIGPIPE, SIG_IGN) == SIG_ERR)
    {
        die_ko("signal(SIGPIPE)");
    }
}

static ssize_t write_all_ko(int fd, const void *buf, size_t n)
{
    /*
     * TCP 소켓의 write 는 한 번에 요청한 길이만큼 모두 쓰리라는 보장이 없다.
     * 그래서 남은 길이가 0이 될 때까지 반복해서 써야 한다.
     */
    const char *p = (const char *)buf;
    size_t left = n;

    while (left > 0)
    {
        ssize_t nwritten = write(fd, p, left);

        if (nwritten < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }
            return -1;
        }

        left -= (size_t)nwritten;
        p += nwritten;
    }

    return (ssize_t)n;
}

static ssize_t read_line_fd_ko(int fd, char *buf, size_t maxlen)
{
    /*
     * 한 번에 1바이트씩 읽는 단순한 line reader.
     * 성능은 좋지 않지만 텍스트 프로토콜(HTTP, echo) 흐름을 이해하기에는 명확하다.
     */
    size_t used = 0;

    while (used + 1 < maxlen)
    {
        char c;
        ssize_t nread = read(fd, &c, 1);

        if (nread == 1)
        {
            buf[used++] = c;
            if (c == '\n')
            {
                break;
            }
        }
        else if (nread == 0)
        {
            break;
        }
        else
        {
            if (errno == EINTR)
            {
                continue;
            }
            return -1;
        }
    }

    if (used == 0)
    {
        return 0;
    }

    buf[used] = '\0';
    return (ssize_t)used;
}

static int is_blank_line_ko(const char *line)
{
    return strcmp(line, "\r\n") == 0 || strcmp(line, "\n") == 0;
}

static void log_peer_ko(const struct sockaddr *sa, socklen_t salen)
{
    char host[NI_MAXHOST];
    char serv[NI_MAXSERV];
    int rc;

    /*
     * 이름 역변환을 하려면 DNS 조회가 일어날 수 있다.
     * 여기서는 흐름이 잘 보이도록 숫자 문자열로만 출력한다.
     */
    rc = getnameinfo(sa, salen, host, sizeof(host), serv, sizeof(serv),
                     NI_NUMERICHOST | NI_NUMERICSERV);
    if (rc == 0)
    {
        warnx_ko("peer = (%s, %s)\n", host, serv);
    }
    else
    {
        warnx_ko("getnameinfo error: %s\n", gai_strerror(rc));
    }
}

int open_clientfd_ko(const char *hostname, const char *port)
{
    /*
     * CSAPP 의 open_clientfd 아이디어를 그대로 따르되,
     * 주석을 늘려 학습용으로 다시 작성했다.
     */
    struct addrinfo hints;
    struct addrinfo *listp = NULL;
    struct addrinfo *p;
    int clientfd = -1;
    int rc;

    memset(&hints, 0, sizeof(hints));
    hints.ai_socktype = SOCK_STREAM;                /* TCP 연결 */
    hints.ai_flags = AI_NUMERICSERV | AI_ADDRCONFIG;

    rc = getaddrinfo(hostname, port, &hints, &listp);
    if (rc != 0)
    {
        warnx_ko("getaddrinfo(%s, %s): %s\n", hostname, port, gai_strerror(rc));
        return -1;
    }

    /*
     * 도메인 이름 하나가 여러 주소 후보를 가질 수 있으므로,
     * 될 때까지 차례대로 socket + connect 를 시도한다.
     */
    for (p = listp; p != NULL; p = p->ai_next)
    {
        clientfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (clientfd < 0)
        {
            continue;
        }

        if (connect(clientfd, p->ai_addr, p->ai_addrlen) == 0)
        {
            break;
        }

        close(clientfd);
        clientfd = -1;
    }

    freeaddrinfo(listp);
    return clientfd;
}

int open_listenfd_ko(const char *port)
{
    struct addrinfo hints;
    struct addrinfo *listp = NULL;
    struct addrinfo *p;
    int listenfd = -1;
    int optval = 1;
    int rc;

    memset(&hints, 0, sizeof(hints));
    hints.ai_socktype = SOCK_STREAM; /* TCP listen 소켓 */
    hints.ai_flags = AI_PASSIVE | AI_ADDRCONFIG | AI_NUMERICSERV;

    /*
     * host 를 NULL 로 주고 AI_PASSIVE 를 켜면,
     * "이 머신의 아무 IP 에서나 요청을 받겠다"는 wildcard 주소를 얻게 된다.
     */
    rc = getaddrinfo(NULL, port, &hints, &listp);
    if (rc != 0)
    {
        warnx_ko("getaddrinfo(NULL, %s): %s\n", port, gai_strerror(rc));
        return -1;
    }

    for (p = listp; p != NULL; p = p->ai_next)
    {
        listenfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (listenfd < 0)
        {
            continue;
        }

        /*
         * 개발 중 서버를 내렸다가 바로 다시 띄우면 TIME_WAIT 때문에
         * bind 가 실패하는 경우가 있는데, SO_REUSEADDR 이 이를 완화해 준다.
         */
        if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR,
                       &optval, sizeof(optval)) < 0)
        {
            close(listenfd);
            listenfd = -1;
            continue;
        }

        if (bind(listenfd, p->ai_addr, p->ai_addrlen) == 0)
        {
            break;
        }

        close(listenfd);
        listenfd = -1;
    }

    freeaddrinfo(listp);

    if (listenfd < 0)
    {
        return -1;
    }

    if (listen(listenfd, LISTENQ_KO) < 0)
    {
        close(listenfd);
        return -1;
    }

    return listenfd;
}

static void echo_session_ko(int connfd)
{
    char line[MAXLINE_KO];
    ssize_t nread;

    while ((nread = read_line_fd_ko(connfd, line, sizeof(line))) > 0)
    {
        warnx_ko("echo server received %zd bytes: %s", nread, line);

        if (write_all_ko(connfd, line, (size_t)nread) < 0)
        {
            perror("write");
            return;
        }
    }

    if (nread < 0)
    {
        perror("read");
    }
}

void run_iterative_echo_server_ko(const char *port)
{
    int listenfd;

    ignore_sigpipe_ko();

    listenfd = open_listenfd_ko(port);
    if (listenfd < 0)
    {
        die_ko("open_listenfd_ko");
    }

    warnx_ko("iterative echo server listening on port %s\n", port);

    while (1)
    {
        struct sockaddr_storage clientaddr;
        socklen_t clientlen = sizeof(clientaddr);
        int connfd = accept(listenfd, (struct sockaddr *)&clientaddr, &clientlen);

        if (connfd < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }
            perror("accept");
            continue;
        }

        log_peer_ko((struct sockaddr *)&clientaddr, clientlen);
        echo_session_ko(connfd);
        close(connfd);
    }
}

void run_echo_client_ko(const char *host, const char *port)
{
    int clientfd;
    char sendbuf[MAXLINE_KO];
    char recvbuf[MAXLINE_KO];
    ssize_t nread;

    clientfd = open_clientfd_ko(host, port);
    if (clientfd < 0)
    {
        die_ko("open_clientfd_ko");
    }

    warnx_ko("connected to %s:%s\n", host, port);
    warnx_ko("type a line and press Enter; Ctrl+D ends the client.\n");

    while (fgets(sendbuf, sizeof(sendbuf), stdin) != NULL)
    {
        if (write_all_ko(clientfd, sendbuf, strlen(sendbuf)) < 0)
        {
            die_ko("write");
        }

        nread = read_line_fd_ko(clientfd, recvbuf, sizeof(recvbuf));
        if (nread < 0)
        {
            die_ko("read");
        }
        if (nread == 0)
        {
            warnx_ko("server closed the connection\n");
            break;
        }

        fputs(recvbuf, stdout);
    }

    close(clientfd);
}

static const char *guess_mime_type_ko(const char *filename)
{
    const char *ext = strrchr(filename, '.');

    if (ext == NULL)
    {
        return "text/plain";
    }
    if (strcmp(ext, ".html") == 0 || strcmp(ext, ".htm") == 0)
    {
        return "text/html";
    }
    if (strcmp(ext, ".gif") == 0)
    {
        return "image/gif";
    }
    if (strcmp(ext, ".png") == 0)
    {
        return "image/png";
    }
    if (strcmp(ext, ".jpg") == 0 || strcmp(ext, ".jpeg") == 0)
    {
        return "image/jpeg";
    }
    if (strcmp(ext, ".css") == 0)
    {
        return "text/css";
    }
    if (strcmp(ext, ".js") == 0)
    {
        return "application/javascript";
    }
    return "text/plain";
}

static void send_client_error_ko(int fd,
                                 const char *status,
                                 const char *shortmsg,
                                 const char *longmsg,
                                 const char *cause)
{
    char body[MAXLINE_KO * 2];
    char header[MAXLINE_KO];
    int bodylen;
    int headerlen;

    /*
     * 브라우저는 오류도 HTTP 응답 형식으로 받아야 이해할 수 있다.
     * 따라서 상태 줄 + 헤더 + HTML 본문을 모두 만들어 보낸다.
     */
    bodylen = snprintf(body, sizeof(body),
                       "<html><head><title>%s %s</title></head>\r\n"
                       "<body>\r\n"
                       "<h1>%s %s</h1>\r\n"
                       "<p>%s: %s</p>\r\n"
                       "<hr><em>CSAPP Chapter 11 demo server</em>\r\n"
                       "</body></html>\r\n",
                       status, shortmsg,
                       status, shortmsg,
                       longmsg, cause ? cause : "");

    headerlen = snprintf(header, sizeof(header),
                         "HTTP/1.0 %s %s\r\n"
                         "Content-Type: text/html\r\n"
                         "Content-Length: %d\r\n"
                         "Connection: close\r\n"
                         "\r\n",
                         status, shortmsg, bodylen);

    if (headerlen > 0)
    {
        (void)write_all_ko(fd, header, (size_t)headerlen);
    }
    if (bodylen > 0)
    {
        (void)write_all_ko(fd, body, (size_t)bodylen);
    }
}

static void skip_request_headers_ko(int fd)
{
    char line[MAXLINE_KO];
    ssize_t nread;

    /*
     * Tiny 는 요청 헤더를 거의 쓰지 않기 때문에,
     * 빈 줄이 나올 때까지 읽고 버리는 방식으로 단순화한다.
     */
    while ((nread = read_line_fd_ko(fd, line, sizeof(line))) > 0)
    {
        if (is_blank_line_ko(line))
        {
            break;
        }
    }
}

static int parse_uri_ko(const char *uri,
                        char *filename,
                        size_t filename_sz,
                        char *cgiargs,
                        size_t cgiargs_sz)
{
    char scratch[PATH_MAX];
    char *question;

    if (uri == NULL || uri[0] == '\0')
    {
        return 1;
    }

    snprintf(scratch, sizeof(scratch), "%s", uri);

    /*
     * 책의 Tiny 와 같은 정책:
     *   - "cgi-bin" 이 URI 에 없으면 정적 콘텐츠
     *   - 있으면 동적 콘텐츠
     */
    if (strstr(scratch, "cgi-bin") == NULL)
    {
        cgiargs[0] = '\0';

        if (scratch[strlen(scratch) - 1] == '/')
        {
            snprintf(filename, filename_sz, ".%shome.html", scratch);
        }
        else
        {
            snprintf(filename, filename_sz, ".%s", scratch);
        }

        return 1;
    }

    question = strchr(scratch, '?');
    if (question != NULL)
    {
        *question = '\0';
        snprintf(cgiargs, cgiargs_sz, "%s", question + 1);
    }
    else
    {
        cgiargs[0] = '\0';
    }

    snprintf(filename, filename_sz, ".%s", scratch);
    return 0;
}

static void serve_static_ko(int fd, const char *filename, const struct stat *st)
{
    int srcfd;
    const char *mime;
    char header[MAXLINE_KO];
    int headerlen;

    mime = guess_mime_type_ko(filename);
    headerlen = snprintf(header, sizeof(header),
                         "HTTP/1.0 200 OK\r\n"
                         "Server: CSAPP Chapter 11 demo\r\n"
                         "Connection: close\r\n"
                         "Content-Length: %lld\r\n"
                         "Content-Type: %s\r\n"
                         "\r\n",
                         (long long)st->st_size, mime);

    if (write_all_ko(fd, header, (size_t)headerlen) < 0)
    {
        perror("write header");
        return;
    }

    srcfd = open(filename, O_RDONLY);
    if (srcfd < 0)
    {
        send_client_error_ko(fd, "404", "Not Found",
                             "Cannot open file", filename);
        return;
    }

    /*
     * Tiny 의 핵심 포인트를 살리기 위해 mmap 을 사용한다.
     * 파일을 메모리에 매핑해 두고, 그 메모리 범위를 그대로 소켓에 써 준다.
     */
    if (st->st_size > 0)
    {
        void *srcp = mmap(NULL, (size_t)st->st_size, PROT_READ, MAP_PRIVATE, srcfd, 0);

        close(srcfd);

        if (srcp == MAP_FAILED)
        {
            send_client_error_ko(fd, "500", "Internal Server Error",
                                 "mmap failed for file", filename);
            return;
        }

        if (write_all_ko(fd, srcp, (size_t)st->st_size) < 0)
        {
            perror("write body");
        }

        if (munmap(srcp, (size_t)st->st_size) < 0)
        {
            perror("munmap");
        }
    }
    else
    {
        /*
         * 길이가 0인 파일은 본문이 비어 있으므로 헤더만 보내면 된다.
         */
        close(srcfd);
    }
}

static void serve_dynamic_cgi_ko(int fd, const char *filename, const char *cgiargs)
{
    char header[MAXLINE_KO];
    int headerlen;
    pid_t pid;

    /*
     * CGI 프로그램이 본문과 Content-Type 같은 추가 헤더를 생성한다.
     * 여기서는 상태 줄과 최소한의 서버 식별 헤더만 먼저 보낸다.
     */
    headerlen = snprintf(header, sizeof(header),
                         "HTTP/1.0 200 OK\r\n"
                         "Server: CSAPP Chapter 11 demo\r\n"
                         "Connection: close\r\n");

    if (write_all_ko(fd, header, (size_t)headerlen) < 0)
    {
        perror("write CGI prelude");
        return;
    }

    pid = fork();
    if (pid < 0)
    {
        send_client_error_ko(fd, "500", "Internal Server Error",
                             "fork failed", filename);
        return;
    }

    if (pid == 0)
    {
        /*
         * 자식 프로세스:
         *   1) QUERY_STRING 설정
         *   2) stdout 을 소켓으로 바꾸기
         *   3) CGI 프로그램 실행
         *
         * 그러면 CGI 안의 printf 출력이 곧바로 클라이언트로 간다.
         */
        if (setenv("QUERY_STRING", cgiargs, 1) < 0)
        {
            _exit(1);
        }

        if (dup2(fd, STDOUT_FILENO) < 0)
        {
            _exit(1);
        }

        if (fd != STDOUT_FILENO)
        {
            close(fd);
        }

        execl(filename, filename, (char *)NULL);
        _exit(127);
    }

    /*
     * Tiny 는 반복 서버이므로, 부모가 여기서 기다렸다가 자식을 회수한다.
     * 동시성 서버라면 이 부분이 달라질 수 있다.
     */
    if (waitpid(pid, NULL, 0) < 0)
    {
        perror("waitpid");
    }
}

static void handle_http_transaction_ko(int fd)
{
    char line[MAXLINE_KO];
    char method[64];
    char uri[PATH_MAX];
    char version[64];
    char filename[PATH_MAX];
    char cgiargs[PATH_MAX];
    struct stat st;
    int is_static;
    ssize_t nread;

    nread = read_line_fd_ko(fd, line, sizeof(line));
    if (nread <= 0)
    {
        return;
    }

    warnx_ko("request line: %s", line);

    if (sscanf(line, "%63s %1023s %63s", method, uri, version) != 3)
    {
        send_client_error_ko(fd, "400", "Bad Request",
                             "Could not parse request line", line);
        return;
    }

    /*
     * Tiny 는 GET 만 처리한다.
     * 다른 메서드가 들어오면 501 로 단순 응답한다.
     */
    if (strcasecmp(method, "GET") != 0)
    {
        send_client_error_ko(fd, "501", "Not Implemented",
                             "This demo only supports GET", method);
        return;
    }

    skip_request_headers_ko(fd);

    is_static = parse_uri_ko(uri, filename, sizeof(filename),
                             cgiargs, sizeof(cgiargs));

    if (stat(filename, &st) < 0)
    {
        send_client_error_ko(fd, "404", "Not Found",
                             "File does not exist", filename);
        return;
    }

    if (is_static)
    {
        if (!S_ISREG(st.st_mode) || access(filename, R_OK) < 0)
        {
            send_client_error_ko(fd, "403", "Forbidden",
                                 "File is not readable", filename);
            return;
        }

        serve_static_ko(fd, filename, &st);
    }
    else
    {
        if (!S_ISREG(st.st_mode) || access(filename, X_OK) < 0)
        {
            send_client_error_ko(fd, "403", "Forbidden",
                                 "CGI is not executable", filename);
            return;
        }

        serve_dynamic_cgi_ko(fd, filename, cgiargs);
    }
}

void run_tiny_like_server_ko(const char *port)
{
    int listenfd;

    ignore_sigpipe_ko();

    listenfd = open_listenfd_ko(port);
    if (listenfd < 0)
    {
        die_ko("open_listenfd_ko");
    }

    warnx_ko("tiny-like server listening on port %s\n", port);
    warnx_ko("serving files from the current working directory\n");

    while (1)
    {
        struct sockaddr_storage clientaddr;
        socklen_t clientlen = sizeof(clientaddr);
        int connfd = accept(listenfd, (struct sockaddr *)&clientaddr, &clientlen);

        if (connfd < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }
            perror("accept");
            continue;
        }

        log_peer_ko((struct sockaddr *)&clientaddr, clientlen);
        handle_http_transaction_ko(connfd);
        close(connfd);
    }
}

static void usage_ko(const char *prog)
{
    fprintf(stderr,
            "usage:\n"
            "  %s echo-server <port>\n"
            "  %s echo-client <host> <port>\n"
            "  %s tiny-server <port>\n",
            prog, prog, prog);
}

int main(int argc, char **argv)
{
    if (argc < 3)
    {
        usage_ko(argv[0]);
        return EXIT_FAILURE;
    }

    if (strcmp(argv[1], "echo-server") == 0)
    {
        run_iterative_echo_server_ko(argv[2]);
        return EXIT_SUCCESS;
    }

    if (strcmp(argv[1], "echo-client") == 0)
    {
        if (argc != 4)
        {
            usage_ko(argv[0]);
            return EXIT_FAILURE;
        }

        run_echo_client_ko(argv[2], argv[3]);
        return EXIT_SUCCESS;
    }

    if (strcmp(argv[1], "tiny-server") == 0)
    {
        run_tiny_like_server_ko(argv[2]);
        return EXIT_SUCCESS;
    }

    usage_ko(argv[0]);
    return EXIT_FAILURE;
}
