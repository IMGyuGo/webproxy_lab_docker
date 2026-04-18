/*
 * echoserver.c - CS:APP 책의 기본 반복형(iterative) echo 서버 예제
 *
 * 동작 방식:
 * 1. 명령행 인자로 받은 포트에서 리스닝 소켓을 연다.
 * 2. 클라이언트가 접속할 때까지 기다린다.
 * 3. 연결이 성립되면 클라이언트가 보낸 한 줄을 읽는다.
 * 4. 읽은 내용을 그대로 다시 돌려보낸다.
 * 5. 클라이언트가 연결을 끊을 때까지 반복한다.
 *
 * 이 서버는 "반복형"이므로 한 번에 한 클라이언트만 처리한다.
 * 여러 클라이언트를 동시에 처리하는 버전은 fork, thread, select/poll 등을
 * 추가로 사용해야 한다.
 */

#include "csapp.h"

int main(int argc, char **argv)
{
    int listenfd;
    int connfd;
    socklen_t clientlen;
    // 다양한 주소 타입(AF_INET, AF_INET6 등)을 모두 담기 위한 범용 구조체
    // 어떤 종류의 소켓 주소가 와도 받을 수 있는 버퍼
    struct sockaddr_storage clientaddr;
    char client_hostname[MAXLINE];
    char client_port[MAXLINE];

    if (argc != 2)
    {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        return 1;
    }

    /* 주어진 포트에서 연결 요청을 받을 리스닝 소켓을 연다. */
    listenfd = Open_listenfd(argv[1]);

    while (1)
    {
        clientlen = sizeof(clientaddr);

        /*
         * 클라이언트의 접속을 기다렸다가 연결 소켓(connfd)을 얻는다.
         * listenfd는 계속 살아 있고, 실제 데이터 송수신은 connfd로 한다.
         */
        connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);

        /*
         * 접속한 클라이언트의 주소를 사람이 읽을 수 있는
         * 호스트 이름/포트 문자열로 바꿔서 출력한다.
         */
        Getnameinfo((SA *)&clientaddr, clientlen,
                    client_hostname, MAXLINE,
                    client_port, MAXLINE, 0);

        printf("Connected to (%s, %s)\n", client_hostname, client_port);

        /* 이 연결 동안에는 csapp.c의 echo 함수를 이용해 echo 서비스를 수행한다. */
        echo(connfd);

        /* 클라이언트가 나가면 연결 소켓만 닫고, 다음 클라이언트를 기다린다. */
        Close(connfd);
    }
}
