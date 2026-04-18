/*
 * hostinfo.c - 호스트 이름을 사람이 읽을 수 있는 하나 이상의 IP 주소로 변환한다.
 *
 * 왜 "nslookup"처럼 느껴지는가:
 * - 이 프로그램은 HTTP를 하지 않고, 웹 서버에 접속하지도 않는다.
 * - 대신 운영체제의 이름 해석기(name resolver)에게 호스트 이름의 주소 정보를 묻는다.
 * - 그리고 돌아온 주소들을 사람이 읽을 수 있는 문자열 형태로 출력한다.
 *
 * 즉, 이 코드는 작은 DNS/이름 해석 예제이기 때문에 구조 자체가
 * 축소판 "nslookup"과 비슷하게 보이는 것이 자연스럽다.
 *
 * 빌드 참고:
 * - 이 CS:APP 코드는 Linux/Unix의 POSIX 네트워크 API를 사용한다.
 * - 일반 Windows GCC 환경에서는 헤더 누락 오류가 나는 것이 정상이다.
 *   Linux, WSL, 또는 수업용 컨테이너에서 빌드해야 한다.
 */

#include "csapp.h"

int main(int argc, char **argv)
{
    struct addrinfo hints;         /* getaddrinfo에 줄 검색 조건 */
    struct addrinfo *listp = NULL; /* 반환된 주소 연결 리스트의 시작점 */
    struct addrinfo *p;            /* 연결 리스트를 순회할 포인터 */
    char buf[MAXLINE];             /* 출력용 IP 문자열이 저장될 버퍼 */
    int rc;
    int flags;

    /* 인자는 호스트 이름 하나만 받는다. 예: "./hostinfo www.google.com" */
    if (argc != 2)
    {
        fprintf(stderr, "usage: %s <domain name>\n", argv[0]);
        return 1;
    }

    /*
     * hints 구조체를 초기화한다.
     *
     * 자주 하는 실수:
     * - "hints"는 포인터가 아니라 실제 구조체 변수여야 한다.
     * - 먼저 전체를 0으로 초기화한 뒤, 필요한 필드만 채우는 식으로 쓴다.
     */
    memset(&hints, 0, sizeof(hints));

    /*
     * IPv4 + 스트림 소켓(TCP 클라이언트에 쓸 법한 주소)만 요청한다.
     *
     * 여기서 실제 TCP 연결을 여는 것은 아니다.
     * 단지 "이 호스트에 대해 TCP 연결에 쓸 수 있는 IPv4 주소들을 알려 달라"
     * 는 조건을 주는 것이다.
     */
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    /*
     * argv[1]의 호스트 이름을 sockaddr 연결 리스트로 해석한다.
     *
     * 예:
     *   "www.example.com" -> IPv4 소켓 주소들의 리스트
     *
     * 여기서는 호스트 이름만 알고 싶기 때문에 service는 NULL이다.
     * 즉, "http" 같은 서비스 이름을 포트 번호로 바꾸는 작업은 하지 않는다.
     */
    rc = getaddrinfo(argv[1], NULL, &hints, &listp);
    if (rc != 0)
    {
        fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(rc));
        return 1;
    }

    /*
     * NI_NUMERICHOST는 getnameinfo에게
     * "역방향 조회를 하지 말고 숫자 형태의 IP 문자열만 출력하라"
     * 는 뜻을 전달한다.
     *
     * 이 플래그가 없으면 getnameinfo가 주소를 다시 호스트 이름으로
     * 바꾸려 할 수 있다. 여기서는 "142.250.207.36" 같은 숫자 IP만 원한다.
     */
    flags = NULL; // NI_NUMERICHOST;

    /*
     * getaddrinfo가 반환한 연결 리스트를 순회한다.
     * 각 노드는 sockaddr 포인터와 주소 길이를 가지고 있고,
     * getnameinfo는 그 바이너리 주소를 사람이 읽을 수 있는 문자열로 바꿔 준다.
     */
    for (p = listp; p != NULL; p = p->ai_next)
    {
        Getnameinfo(p->ai_addr, p->ai_addrlen, buf, MAXLINE, NULL, 0, flags);
        printf("%s\n", buf);
    }

    /* getaddrinfo가 할당한 리스트이므로 마지막에 반드시 해제한다. */
    Freeaddrinfo(listp);
    return 0;
}
