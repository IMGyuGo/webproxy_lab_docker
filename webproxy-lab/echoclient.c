/*
 * echoclient.c - CS:APP 책의 기본 echo 클라이언트 예제
 *
 * 동작 방식:
 * 1. 명령행 인자로 받은 host와 port로 서버에 접속한다.
 * 2. 표준 입력(stdin)에서 한 줄씩 읽는다.
 * 3. 읽은 줄을 서버에 보낸다.
 * 4. 서버가 그대로 돌려준 응답을 다시 읽어서 화면에 출력한다.
 *
 * 서버가 echo 서버라면, 사용자가 입력한 줄이 그대로 되돌아오는 것을 볼 수 있다.
 */

#include "csapp.h"

int main(int argc, char **argv)
{
    int clientfd;
    char *host;
    char *port;
    char buf[MAXLINE];
    rio_t rio;

    if (argc != 3) {
        fprintf(stderr, "usage: %s <host> <port>\n", argv[0]);
        return 1;
    }

    host = argv[1];
    port = argv[2];

    /* 서버(host:port)로 TCP 연결을 연다. */
    clientfd = Open_clientfd(host, port);

    /* 서버 소켓을 RIO 읽기 버퍼와 연결한다. */
    Rio_readinitb(&rio, clientfd);

    /*
     * 사용자가 한 줄 입력할 때마다:
     * 1. 서버로 전송
     * 2. 서버가 돌려준 echo 응답 수신
     * 3. 화면에 출력
     */
    while (Fgets(buf, MAXLINE, stdin) != NULL) {
        Rio_writen(clientfd, buf, strlen(buf));
        Rio_readlineb(&rio, buf, MAXLINE);
        Fputs(buf, stdout);
    }

    Close(clientfd);
    return 0;
}
