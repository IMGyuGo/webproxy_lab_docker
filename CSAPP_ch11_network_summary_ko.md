# CSAPP 11장 네트워크 프로그래밍 정리 (11.1 ~ 11.7)

이 문서는 `CSAPP_2016.pdf`의 11장 내용을 바탕으로, 핵심 개념을 한국어로 다시 설명한 정리본이다. 책의 흐름을 따라가되, 단순 요약이 아니라 "왜 이런 설계를 하는가", "구현에서는 어디가 중요한가", "학생들이 많이 헷갈리는 지점은 무엇인가"까지 함께 풀어 썼다.

같이 보면 좋은 파일:

- 이 문서의 주석 달린 예제 코드: `CSAPP_ch11_network_examples_ko.c`
- 워크스페이스에 들어 있는 Tiny 서버 소스: `webproxy-lab/tiny/tiny.c`
- Tiny의 CGI 예제: `webproxy-lab/tiny/cgi-bin/adder.c`

## 0. 먼저 큰 그림

11장은 결국 아래 한 줄로 요약할 수 있다.

`클라이언트-서버 모델 + 인터넷 주소 체계(IP/DNS) + 소켓 API + HTTP + 프로세스/파일 I/O = 작은 웹 서버`

즉, 11장은 "네트워크"라는 완전히 새로운 세계를 설명하는 장이 아니라, 앞에서 배운 시스템 개념들을 네트워크 상황에 다시 조합하는 장이다.

이 장을 읽을 때 머릿속에 반드시 있어야 할 큰 흐름은 다음과 같다.

1. 클라이언트는 서비스를 요청한다.
2. 서버는 잘 알려진 포트에서 요청을 기다린다.
3. TCP 연결이 성립되면 양쪽은 파일 디스크립터처럼 소켓을 읽고 쓴다.
4. 웹 서버라면 그 위에서 HTTP 요청/응답을 주고받는다.
5. 정적 콘텐츠면 파일을 읽어 보내고, 동적 콘텐츠면 프로그램을 실행해 그 출력을 보낸다.

---

## 11.1 The Client-Server Programming Model

### 핵심 개념

모든 네트워크 애플리케이션은 기본적으로 `클라이언트-서버 모델` 위에서 동작한다.

- `클라이언트(client)`는 서비스를 요청하는 쪽이다.
- `서버(server)`는 어떤 자원(resource)을 관리하면서, 그 자원을 사용한 서비스를 제공하는 쪽이다.

예를 들면 다음과 같다.

- 웹 서버: 디스크의 파일과 실행 파일을 관리한다.
- FTP 서버: 파일 저장/전송 서비스를 제공한다.
- 메일 서버: 메일 스풀(spool) 파일을 관리한다.

### 클라이언트-서버 트랜잭션

이 모델의 기본 동작 단위는 `트랜잭션(transaction)`이다.

한 번의 트랜잭션은 보통 다음 네 단계로 생각하면 된다.

1. 클라이언트가 서버에 요청을 보낸다.
2. 서버가 요청을 해석하고 자원을 조작한다.
3. 서버가 결과를 응답으로 돌려준다.
4. 클라이언트가 응답을 받아 처리한다.

중요한 점은 여기서 말하는 transaction은 `데이터베이스 트랜잭션`과 다르다는 것이다. 원자성, 일관성 같은 성질을 뜻하는 것이 아니라, 단순히 "요청 한 번과 그에 대한 응답 한 번" 정도의 의미로 이해하면 된다.

### 정말 중요한 구분: 서버/클라이언트는 "호스트"가 아니라 "프로세스"다

이 부분을 처음 볼 때 많은 사람이 무심코 넘어가지만 아주 중요하다.

- 서버와 클라이언트는 컴퓨터 자체가 아니라 `프로세스`다.
- 한 호스트는 동시에 여러 클라이언트와 여러 서버를 함께 돌릴 수 있다.
- 클라이언트와 서버가 서로 다른 컴퓨터에 있을 수도 있고, 같은 컴퓨터 안에 있을 수도 있다.

즉, "서버 프로그램"은 특별한 종류의 하드웨어가 아니라 `연결 요청을 기다리는 프로세스`다.

### 왜 이 모델이 강력한가

이 모델은 역할 분리가 명확하다.

- 클라이언트는 "무엇을 원하는가"에 집중한다.
- 서버는 "자원을 어떻게 관리하고 응답할 것인가"에 집중한다.

그래서 확장도 쉽다.

- 클라이언트 수를 늘려도 서버 인터페이스는 그대로 유지할 수 있다.
- 서버 내부 구현이 바뀌어도 프로토콜이 유지되면 클라이언트는 거의 안 바뀐다.

### 구현 관점에서 보면

이 절은 뒤의 소켓 API와 정확히 연결된다.

- 클라이언트는 보통 `connect`로 연결을 시작한다.
- 서버는 `listen`으로 기다리고 `accept`로 연결을 받아들인다.
- 이후 양쪽은 `read/write` 또는 `rio` 같은 I/O 함수로 통신한다.

### 자주 헷갈리는 포인트

- "서버는 항상 한 대의 컴퓨터인가?"
  아니다. 논리적으로는 하나의 서버 서비스일 수 있지만, 실제로는 여러 프로세스/여러 머신/로드밸런서 뒤의 여러 인스턴스로 구성될 수 있다.

- "클라이언트는 꼭 사용자 프로그램인가?"
  아니다. 서버도 다른 서버의 클라이언트가 될 수 있다. 예를 들어 프록시 서버는 브라우저의 서버이면서 원본 서버의 클라이언트다.

---

## 11.2 Networks

### 프로그래머 관점의 네트워크: "그냥 또 하나의 I/O 장치"

CSAPP가 네트워크를 설명하는 방식의 핵심은 이것이다.

`호스트 입장에서 네트워크는 디스크, 키보드, 터미널처럼 또 하나의 I/O 장치다.`

네트워크 어댑터가 물리 인터페이스를 제공하고, 들어온 데이터는 버스를 통해 메모리로 복사된다. 반대로 메모리의 데이터도 어댑터를 통해 네트워크로 나간다. 즉, 네트워크 프로그래밍도 결국은 `데이터를 어디서 읽고 어디에 쓰는가`의 문제다.

### LAN, Ethernet, 허브, 브리지

가장 아래 계층에서는 보통 LAN(local area network)을 생각한다.

- `Ethernet segment`: 가까운 공간(방, 층 등)을 묶는 작은 네트워크
- `Hub`: 받은 비트를 모든 포트로 그대로 복사하는 장치
- `Frame`: 이더넷이 전송하는 기본 단위
- `MAC 주소`: 이더넷 어댑터가 가지는 고유 48비트 주소

허브 환경에서는 모든 호스트가 모든 프레임을 "보게" 된다. 다만 목적지 주소가 자기 것이 아닌 호스트는 그 프레임을 무시한다.

브리지(bridge)는 허브보다 똑똑하다.

- 어느 호스트가 어느 포트 쪽에 있는지 학습한다.
- 꼭 필요한 포트로만 프레임을 전달한다.
- 그래서 불필요한 트래픽 복사를 줄이고 대역폭을 더 효율적으로 쓴다.

### Router와 internet

여러 LAN/WAN을 연결하면 `internet`이 된다. 여기서 소문자 internet은 일반 개념이고, 대문자 Internet은 전 세계 IP 인터넷을 뜻한다.

Router의 역할은 다음과 같다.

- 서로 다른 네트워크 기술을 이어 붙인다.
- 패킷의 목적지 주소를 보고 어디로 보낼지 결정한다.

### 인터넷이 가능해지는 두 가지 추상화

서로 다른 네트워크 기술을 묶으려면 최소 두 가지가 표준화되어야 한다.

1. `Naming scheme`
   모든 호스트를 공통 형식의 주소로 식별해야 한다.

2. `Delivery mechanism`
   데이터를 공통 형식의 덩어리로 묶어 보내야 한다.

이때 등장하는 것이 `packet`이다.

- packet header: 출발지/목적지 주소, 길이 등
- payload: 실제 사용자 데이터

### 11.2의 진짜 핵심: Encapsulation

이 절에서 제일 중요한 개념 하나만 꼽으라면 `encapsulation(캡슐화)`이다.

호스트 A에서 호스트 B로 데이터를 보낼 때 실제로는 다음처럼 포장된다.

1. 사용자 데이터가 있다.
2. 그 앞에 IP 계층 정보가 붙어 `인터넷 패킷`이 된다.
3. 그 바깥에 현재 LAN에서 쓰는 frame header가 붙어 `프레임`이 된다.
4. 라우터에 도착하면, 라우터는 기존 frame header를 벗기고 다음 네트워크용 frame header를 새로 붙인다.
5. 최종 목적지에 도달하면 헤더들이 벗겨지고 원래 데이터가 애플리케이션으로 올라간다.

즉, `IP 패킷은 종단 간(end-to-end) 추상화`이고, `프레임은 각 링크(local hop)마다 달라지는 지역 포장`이다.

### 프레임과 패킷을 구분해야 하는 이유

많이 헷갈리는 질문이다.

- 프레임은 "현재 링크에서 어떻게 전달할까?"의 문제다.
- 패킷은 "인터넷 전체에서 어디로 갈까?"의 문제다.

그래서 라우터는 프레임 헤더를 갈아끼우지만, 그 안의 IP 패킷은 목적지까지 유지되는 논리적 단위로 취급된다.

### 구현 관점에서 보면

애플리케이션 개발자는 프레임을 직접 만질 일이 거의 없다. 하지만 다음 사실은 꼭 알아야 한다.

- 네트워크는 계층적이다.
- 주소와 전송 단위는 계층마다 다르다.
- 우리가 `socket`으로 다루는 것은 대체로 TCP/IP 위의 추상화다.

---

## 11.3 The Global IP Internet

### 큰 그림

전 세계 Internet의 관점에서 보면, 프로그래머는 다음 세 가지를 이해하면 된다.

1. 호스트는 `IP 주소`를 가진다.
2. IP 주소는 사람이 기억하기 쉬운 `도메인 이름`과 매핑된다.
3. 프로세스들은 `connection` 위에서 바이트 스트림을 주고받는다.

TCP/IP는 여러 프로토콜의 집합이다.

- `IP`: 호스트에서 호스트로 데이터그램을 보낸다. 신뢰성 보장은 없다.
- `UDP`: 프로세스 수준까지 약간 확장된 비신뢰성 데이터그램 서비스다.
- `TCP`: 신뢰적인 양방향 바이트 스트림 연결을 제공한다.

이 장에서는 주로 TCP를 기반으로 한 소켓 프로그래밍을 다룬다.

### 11.3.1 IP Addresses

#### IP 주소란

IPv4 기준 IP 주소는 `32비트 unsigned integer`다.

CSAPP에서는 이를 `struct in_addr`로 다룬다.

```c
struct in_addr {
    uint32_t s_addr;   // 네트워크 바이트 순서(big-endian)로 저장
};
```

이 구조체 형태는 역사적 이유가 크다. 지금 기준으로 보면 그냥 정수형 별칭 타입으로 만드는 편이 자연스럽지만, 초기 소켓 인터페이스 설계의 흔적이 남아 있는 것이다.

#### Host byte order vs Network byte order

이 부분은 매우 중요하다.

- x86 계열 호스트는 보통 little-endian이다.
- 네트워크에서는 `network byte order = big-endian`을 표준으로 쓴다.

그래서 네트워크에 싣기 전에 바이트 순서를 바꿔야 할 수 있다.

- `htonl`: host to network long(32비트)
- `htons`: host to network short(16비트)
- `ntohl`: network to host long
- `ntohs`: network to host short

포트 번호도 16비트이므로 마찬가지로 network byte order를 사용한다.

#### 왜 바이트 순서가 중요할까

예를 들어 어떤 호스트가 `0x01020304`를 little-endian으로 저장하고 있고, 다른 호스트는 big-endian이라고 하자. 표준을 정하지 않으면 네트워크를 통해 같은 4바이트를 보내도 수신 쪽 해석이 달라진다. 그래서 패킷 헤더에 들어가는 정수는 반드시 표준 순서로 맞춘다.

#### 사람이 읽는 표현: dotted-decimal

IP 주소는 보통 `128.2.194.242` 같은 점-십진 표기(dotted-decimal notation)로 쓴다.

- 각 바이트를 10진수로 쓰고
- 점(`.`)으로 구분한다

#### 변환 함수

- `inet_pton`: presentation string -> network binary
- `inet_ntop`: network binary -> presentation string

즉,

- 사람이 읽는 문자열을 프로그램이 쓸 이진 주소로 바꿀 때 `inet_pton`
- 이진 주소를 로그 출력용 문자열로 바꿀 때 `inet_ntop`

### 11.3.2 Internet Domain Names

#### 왜 도메인 이름이 필요한가

사람은 `128.2.210.175`보다 `whaleshark.ics.cs.cmu.edu`를 더 기억하기 쉽다. 그래서 인터넷은 IP 주소와 별도로 `도메인 이름 체계`를 운영한다.

도메인 이름은 점으로 구분된 계층 구조다.

예:

`whaleshark.ics.cs.cmu.edu`

이를 오른쪽에서 왼쪽으로 읽으면 계층 구조가 보인다.

- `edu`: 최상위 도메인
- `cmu.edu`: 2단계 도메인
- `cs.cmu.edu`: 그 아래 서브도메인
- `ics.cs.cmu.edu`
- `whaleshark.ics.cs.cmu.edu`

#### DNS는 무엇인가

옛날에는 이름-주소 대응을 하나의 큰 텍스트 파일(`HOSTS.TXT`)로 관리했다. 당연히 규모가 커지자 한계가 왔고, 지금은 분산 데이터베이스인 `DNS(Domain Name System)`가 이를 관리한다.

#### 도메인 이름과 IP 주소의 관계는 1:1이 아니다

이건 시험에서도, 구현에서도 자주 중요하다.

가능한 경우는 다음과 같다.

- 하나의 도메인 이름 -> 하나의 IP
- 여러 도메인 이름 -> 같은 IP
- 하나의 도메인 이름 -> 여러 IP
- 유효한 도메인 이름이지만 특정 호스트 주소로 바로 매핑되지 않을 수도 있음

왜 하나의 이름이 여러 IP를 가질까?

- 부하 분산(load balancing)
- 장애 대응
- 지역별 서비스 분산

#### localhost의 의미

`localhost`는 특별한 이름이다.

- 보통 `127.0.0.1`로 매핑된다.
- 자기 자신과 통신하는 루프백(loopback) 주소다.

그래서 네트워크 프로그램을 디버깅할 때 매우 편리하다.

### 11.3.3 Internet Connections

#### connection이란 무엇인가

인터넷에서 클라이언트와 서버는 `byte stream`을 connection 위에서 주고받는다.

TCP connection은 다음 세 가지 성질로 요약할 수 있다.

- `point-to-point`
  한 쌍의 프로세스를 연결한다.

- `full duplex`
  양방향으로 동시에 데이터가 흐를 수 있다.

- `reliable`
  치명적인 네트워크 장애가 없다면, 보낸 바이트가 순서대로 도착한다고 기대할 수 있다.

여기서 핵심은 "메시지"가 아니라 `바이트 스트림`이라는 점이다. 즉, 애플리케이션이 경계를 직접 관리해야 한다. 줄 단위 프로토콜이면 줄바꿈을 기준으로, HTTP라면 헤더/본문 규칙으로 경계를 정한다.

#### socket과 socket address

connection의 양 끝점(endpoint)이 `socket`이다.

각 소켓은 보통 다음 형식으로 생각하면 된다.

`address:port`

예:

- 클라이언트: `128.2.194.242:51213`
- 서버: `208.216.181.15:80`

#### ephemeral port와 well-known port

- 클라이언트 포트는 보통 커널이 자동 할당한다.
  이것을 `ephemeral port`라고 한다.

- 서버 포트는 서비스에 따라 잘 알려진 번호를 쓴다.
  예: HTTP는 80, SMTP는 25

#### socket pair가 connection을 유일하게 식별한다

한 연결은 아래 튜플로 유일하게 식별된다.

`(client_ip:client_port, server_ip:server_port)`

이게 왜 중요할까?

서버는 포트 80 하나만 열어도 동시에 수많은 연결을 구별할 수 있다. 이유는 각 클라이언트의 IP/포트 조합이 다르기 때문이다.

### 자주 묻는 질문

"서버 포트는 같은데 어떻게 여러 클라이언트를 구분하나?"

답은 `socket pair`다. 서버 쪽 포트가 같아도 클라이언트 쪽 IP/포트가 다르면 다른 연결이다.

---

## 11.4 The Sockets Interface

이 절은 11장의 실전 핵심이다. 여기서부터는 "개념"이 실제 C 코드와 연결된다.

### 큰 흐름 먼저 보기

클라이언트 쪽 흐름:

1. `getaddrinfo`
2. `socket`
3. `connect`
4. `read/write`
5. `close`

서버 쪽 흐름:

1. `getaddrinfo`
2. `socket`
3. `bind`
4. `listen`
5. `accept`
6. `read/write`
7. `close`

이 순서를 통째로 기억해야 한다.

### 11.4.1 Socket Address Structures

인터넷 소켓 주소는 주로 `struct sockaddr_in`으로 표현한다.

```c
struct sockaddr_in {
    uint16_t       sin_family;   // AF_INET
    uint16_t       sin_port;     // network byte order
    struct in_addr sin_addr;     // IP address, network byte order
    unsigned char  sin_zero[8];  // padding
};
```

여기서 중요한 필드는 세 개다.

- `sin_family`: 주소 체계. IPv4라면 `AF_INET`
- `sin_port`: 포트 번호
- `sin_addr`: IP 주소

#### 그런데 왜 `sockaddr`로 캐스팅하나

소켓 함수(`connect`, `bind`, `accept`)는 역사적인 이유로 `struct sockaddr *`를 받는다.

```c
struct sockaddr {
    uint16_t sa_family;
    char     sa_data[14];
};
```

즉, 실제로는 `sockaddr_in`을 쓰지만 함수 호출 시에는 `sockaddr *`로 캐스팅해야 한다.

이게 어색해 보이는 이유는 당시 C에 `void *` 같은 범용 포인터 개념이 없던 설계 흔적이기 때문이다.

#### `sockaddr_storage`가 중요한 이유

서버에서 `accept` 결과를 받을 때는 종종 `struct sockaddr_storage`를 쓴다.

- IPv4/IPv6 어떤 주소가 와도 담을 수 있을 만큼 충분히 크다.
- 따라서 코드가 프로토콜 독립적(protocol-independent)이 된다.

### 11.4.2 The `socket` Function

`socket`은 소켓 디스크립터를 만든다.

```c
int socket(int domain, int type, int protocol);
```

예:

```c
int fd = socket(AF_INET, SOCK_STREAM, 0);
```

뜻은 다음과 같다.

- `AF_INET`: IPv4 주소 체계 사용
- `SOCK_STREAM`: TCP 연결형 소켓
- `0`: 이 조합에 맞는 기본 프로토콜 사용

중요한 점:

- 이 시점의 소켓은 아직 `부분적으로만 열린 상태`다.
- 클라이언트라면 이후 `connect`가 필요하다.
- 서버라면 이후 `bind`와 `listen`이 필요하다.

### 11.4.3 The `connect` Function

클라이언트는 `connect`로 서버와 연결을 맺는다.

```c
int connect(int clientfd, const struct sockaddr *addr, socklen_t addrlen);
```

성공하면:

- 커널이 클라이언트 쪽 ephemeral port를 정할 수 있다.
- `clientfd`는 읽기/쓰기가 가능한 연결 끝점이 된다.

이 함수는 보통 `연결이 되거나 실패할 때까지 블록`된다.

### 11.4.4 The `bind` Function

서버는 `bind`로 자기 소켓에 로컬 주소와 포트를 붙인다.

```c
int bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
```

의미:

- "이 소켓은 이 주소/포트를 대표한다"라고 커널에 등록하는 행위

쉽게 말해:

- 클라이언트의 `connect`는 "저기 서버로 붙고 싶다"
- 서버의 `bind`는 "나는 이 포트에서 기다릴 거다"

### 11.4.5 The `listen` Function

`listen`은 서버 소켓을 수동적(passive) 대기 상태로 바꾼다.

```c
int listen(int sockfd, int backlog);
```

`backlog`는 대기열(queue) 크기에 대한 힌트다.

중요한 해석:

- `socket`만 한 서버 소켓은 아직 "클라이언트 역할"에 더 가깝다.
- `listen`을 호출해야 비로소 "연결 요청을 받는 서버 소켓"이 된다.

### 11.4.6 The `accept` Function

서버는 `accept`로 클라이언트 연결 요청을 받는다.

```c
int accept(int listenfd, struct sockaddr *addr, socklen_t *addrlen);
```

이 함수는 다음 일을 한다.

1. `listenfd`에 도착한 연결 요청을 기다린다.
2. 연결이 오면 클라이언트 주소를 `addr`에 채운다.
3. `새로운 connected descriptor`를 반환한다.

#### listenfd와 connfd를 꼭 구분해야 한다

이 장에서 가장 많이 헷갈리는 지점 중 하나다.

- `listenfd`
  서버가 평생 들고 있는 "대기 전용" 소켓

- `connfd`
  특정 클라이언트와 실제 데이터를 주고받는 "연결 전용" 소켓

왜 굳이 둘을 나눌까?

- 그래야 서버가 계속 새 연결을 받을 수 있다.
- 동시에 여러 클라이언트를 처리하는 동시성 서버도 만들 수 있다.

이 구분이 12장의 `concurrent server`와 직접 이어진다.

### 11.4.7 Host and Service Conversion

이 부분은 현대적 네트워크 프로그래밍에서 정말 중요하다.

예전 함수:

- `gethostbyname`
- `gethostbyaddr`

이 함수들은 오래됐고, thread-safe/reentrant 문제도 있다.

현대적 대체는 다음 두 함수다.

- `getaddrinfo`
- `getnameinfo`

#### `getaddrinfo`

문자열 형태의 호스트명/주소, 서비스명/포트번호를 `실제 socket address 구조체 목록`으로 바꿔준다.

```c
int getaddrinfo(const char *host, const char *service,
                const struct addrinfo *hints,
                struct addrinfo **result);
```

반환 결과는 연결 리스트다.

각 노드는 대체로 다음 정보를 담는다.

- `ai_family`
- `ai_socktype`
- `ai_protocol`
- `ai_addr`
- `ai_addrlen`
- `ai_next`

#### 왜 리스트로 돌려줄까

하나의 이름이 여러 주소를 가질 수 있기 때문이다.

예:

- 하나의 도메인이 여러 IPv4/IPv6 주소를 가질 수 있다.
- 어떤 주소는 현재 호스트에서 쓸 수 있고 어떤 것은 아닐 수 있다.

그래서 애플리케이션은 리스트를 순회하며 "되는 주소"를 찾아야 한다.

#### 중요한 `hints` 플래그

`hints`는 "원하는 주소만 골라달라"는 요청이다.

자주 쓰는 값:

- `AF_INET`
  IPv4만 원할 때

- `SOCK_STREAM`
  TCP 연결형 소켓만 원할 때

- `AI_ADDRCONFIG`
  현재 머신이 실제로 쓸 수 있는 주소 체계만 반환하도록 도움

- `AI_NUMERICSERV`
  service 인자를 숫자 포트로만 해석

- `AI_CANONNAME`
  공식 canonical name을 얻고 싶을 때

- `AI_PASSIVE`
  서버용 로컬 바인드 주소를 만들 때 사용. 보통 `host = NULL`과 같이 쓴다.

#### `getaddrinfo`의 우아한 점

반환된 `addrinfo`의 필드들은 거의 그대로 소켓 함수에 넣으면 된다.

- `socket(p->ai_family, p->ai_socktype, p->ai_protocol)`
- `connect(..., p->ai_addr, p->ai_addrlen)`
- `bind(..., p->ai_addr, p->ai_addrlen)`

이 덕분에 코드가 특정 IP 버전에 덜 묶인다.

#### `getnameinfo`

반대 방향 변환 함수다.

```c
int getnameinfo(const struct sockaddr *sa, socklen_t salen,
                char *host, size_t hostlen,
                char *service, size_t servlen, int flags);
```

역할:

- 소켓 주소 구조체 -> 문자열 호스트명/서비스명

자주 쓰는 플래그:

- `NI_NUMERICHOST`
  이름 대신 숫자 주소 문자열로 받고 싶을 때

- `NI_NUMERICSERV`
  서비스 이름 대신 숫자 포트 문자열로 받고 싶을 때

#### `inet_ntop`보다 `getnameinfo`가 더 좋은 이유

`inet_ntop`는 주소 필드 구조를 직접 파고 들어가야 한다. 반면 `getnameinfo`는 `sockaddr` 전체를 받아서 알아서 풀어 준다. 즉 더 높은 수준의 추상화다.

### 11.4.8 Helper Functions for the Sockets Interface

CSAPP는 복잡한 소켓 초기화 과정을 감싸기 위해 두 개의 helper를 제공한다.

- `open_clientfd`
- `open_listenfd`

이 두 함수는 실전에서도 매우 좋은 패턴이다.

#### `open_clientfd`의 알고리즘

1. `getaddrinfo(host, port, ...)` 호출
2. 반환된 후보 주소 리스트를 순회
3. 각 항목마다 `socket`
4. 성공하면 `connect`
5. 실패하면 `close` 후 다음 후보 시도
6. 하나라도 성공하면 그 소켓 반환

핵심은 "첫 번째 성공하는 주소를 찾을 때까지 순회"다.

#### `open_listenfd`의 알고리즘

1. `getaddrinfo(NULL, port, AI_PASSIVE | ...)`
2. 후보 주소 리스트 순회
3. 각 항목마다 `socket`
4. `setsockopt(... SO_REUSEADDR ...)`
5. `bind`
6. 성공하면 `listen`
7. 완성된 listening descriptor 반환

#### `SO_REUSEADDR`가 왜 중요한가

서버를 종료하고 곧바로 다시 켤 때 `Address already in use` 오류가 날 수 있다. 디버깅할 때 매우 짜증난다. `SO_REUSEADDR`는 이 문제를 크게 줄여 준다.

### 11.4.9 Example Echo Client and Server

에코 서버는 가장 단순하면서도 좋은 학습 예제다.

클라이언트:

1. 서버에 연결한다.
2. 표준 입력에서 한 줄 읽는다.
3. 서버로 보낸다.
4. 서버가 돌려준 한 줄을 읽는다.
5. 화면에 출력한다.

서버:

1. 포트에서 연결을 기다린다.
2. 연결을 받는다.
3. 클라이언트가 보낸 줄을 읽는다.
4. 같은 내용을 다시 써 준다.

#### iterative server

CSAPP의 첫 에코 서버는 `iterative server`다.

- 한 번에 한 클라이언트만 처리한다.
- 현재 클라이언트 처리가 끝나야 다음 클라이언트를 받는다.

이 구조는 단순하지만 동시성이 없다. 그래서 12장에서 process/thread/I/O multiplexing으로 확장한다.

#### EOF가 connection에서 의미하는 것

디스크 파일에서 EOF는 파일 끝을 넘어서 읽을 때 생긴다. 네트워크 연결에서는 다르다.

`TCP connection에서 EOF는 상대 프로세스가 자기 쪽 끝을 닫았다는 뜻이다.`

즉,

- 클라이언트가 `close(clientfd)` 하면
- 서버의 `read` 혹은 `rio_readlineb`가 결국 `0`을 반환한다

이걸 이해 못하면 서버 종료 조건을 헷갈리기 쉽다.

### 11.4의 핵심 암기 포인트

- `socket`: 소켓 생성
- `connect`: 클라이언트가 연결 시작
- `bind`: 서버 주소/포트 고정
- `listen`: 수동적 대기 상태 전환
- `accept`: 실제 연결용 새 fd 생성
- `getaddrinfo/getnameinfo`: 현대적 주소 변환
- `listenfd != connfd`

---

## 11.5 Web Servers

이 절에서는 지금까지 배운 네트워크 개념을 HTTP 위에 올린다.

### 11.5.1 Web Basics

웹은 `HTTP`라는 텍스트 기반 애플리케이션 계층 프로토콜 위에서 동작한다.

고전적 HTTP/1.0 스타일의 흐름은 단순하다.

1. 브라우저가 서버에 연결한다.
2. 콘텐츠를 요청한다.
3. 서버가 응답한다.
4. 서버가 연결을 닫는다.

#### HTML의 핵심

웹이 단순 파일 전송과 다른 이유는 콘텐츠가 `HTML`일 수 있기 때문이다.

HTML은:

- 텍스트를 어떻게 보여줄지 지시하고
- 다른 문서로 가는 링크(hyperlink)를 포함할 수 있다

즉, 웹의 핵심은 "단순한 파일 전달"이 아니라 "문서와 문서의 연결"이다.

### 11.5.2 Web Content

웹 서버 관점에서 콘텐츠는 다음으로 정의된다.

`콘텐츠 = 바이트열 + MIME 타입`

예:

- `text/html`
- `text/plain`
- `image/png`
- `image/jpeg`

#### 정적 콘텐츠 vs 동적 콘텐츠

정적 콘텐츠:

- 디스크 파일을 읽어 그대로 반환
- 예: HTML, 이미지, 텍스트 파일

동적 콘텐츠:

- 실행 파일을 돌려 그 출력을 반환
- 예: CGI 프로그램 출력

#### URL과 URI

예시 URL:

`http://www.google.com:80/index.html`

구성 요소:

- `http`
  어떤 프로토콜로 접근할지

- `www.google.com`
  어느 호스트인지

- `80`
  어느 포트인지

- `/index.html`
  서버 안에서 어떤 리소스를 원하는지

CSAPP 문맥에서는 서버가 주로 해석하는 것은 뒤쪽 `URI` 부분이다.

#### URI 해석에서 중요한 세 가지

1. 정적/동적 구분 규칙은 표준이 아니라 서버 정책이다.
   예를 들어 `cgi-bin` 밑은 실행 파일로 간주하는 식.

2. URI 앞의 `/`는 리눅스 전체 루트(`/`)가 아니라 서버의 콘텐츠 홈 디렉터리를 뜻한다.

3. 단순히 `/`만 요청해도 서버가 기본 페이지(`/index.html`, `/home.html` 등)로 확장할 수 있다.

### 11.5.3 HTTP Transactions

HTTP는 텍스트 줄(line) 기반이다. 그래서 `telnet` 같은 도구로도 직접 실험할 수 있다.

#### HTTP request 형식

요청은 보통 다음 순서다.

1. request line
2. request headers
3. 빈 줄

request line 형식:

`method URI version`

예:

`GET / HTTP/1.1`

#### 자주 나오는 메서드

- `GET`
- `POST`
- `HEAD`
- `PUT`
- `DELETE`

이 장은 주로 `GET`에 집중한다.

#### HTTP/1.0 vs HTTP/1.1

실무적으로 아주 중요한 차이점은 많지만, 이 장에서 꼭 알아둘 포인트는 두 개면 충분하다.

1. HTTP/1.1은 `Host` 헤더가 사실상 필수다.
2. HTTP/1.1은 지속 연결(persistent connection) 같은 더 복잡한 기능을 지원한다.

Tiny는 단순화를 위해 주로 HTTP/1.0 스타일로 생각하면 된다.

#### 왜 `Host` 헤더가 필요한가

프록시나 가상 호스팅 환경에서는 같은 IP/포트 뒤에 여러 사이트가 있을 수 있다. `Host` 헤더가 있어야 서버나 중간 프록시가 "정확히 어느 사이트에 대한 요청인지" 판단할 수 있다.

#### HTTP response 형식

응답도 비슷하다.

1. response line
2. response headers
3. 빈 줄
4. response body

response line 형식:

`version status-code status-message`

예:

`HTTP/1.0 200 OK`

#### 자주 보는 상태 코드

| 코드  | 의미             |
| ----- | ---------------- |
| `200` | 성공             |
| `301` | 영구 이동        |
| `400` | 잘못된 요청      |
| `403` | 접근 금지        |
| `404` | 파일 없음        |
| `501` | 메서드 미구현    |
| `505` | HTTP 버전 미지원 |

#### 특히 중요한 응답 헤더

- `Content-Type`
  본문 MIME 타입

- `Content-Length`
  본문 길이(byte)

브라우저는 이 정보를 바탕으로 본문을 해석한다.

### 11.5.4 Serving Dynamic Content

정적 콘텐츠는 파일을 읽어 보내면 되지만, 동적 콘텐츠는 질문이 생긴다.

- 인자를 어떻게 넘길까?
- 서버는 그 인자를 자식 프로세스에 어떻게 전달할까?
- CGI 프로그램이 만든 출력을 클라이언트에 어떻게 보낼까?

이걸 해결하는 고전적 규약이 `CGI(Common Gateway Interface)`다.

#### GET 요청에서 인자 전달

GET에서는 인자가 URI 뒤쪽에 붙는다.

예:

`/cgi-bin/adder?15000&213`

구조:

- `?` 앞: 실행 파일 경로
- `?` 뒤: 인자 문자열
- 인자 구분: `&`

공백은 직접 넣지 않고 `%20` 같은 URL 인코딩을 쓴다.

#### 서버가 자식에게 인자를 넘기는 법

서버는 보통 다음 순서를 따른다.

1. `fork`로 자식 생성
2. `QUERY_STRING` 같은 환경변수 설정
3. `dup2`로 표준 출력(stdout)을 연결 소켓으로 리다이렉트
4. `execve`로 CGI 프로그램 실행

이 구조를 이해하면 CGI의 핵심이 보인다.

- 자식 프로그램은 그냥 표준 출력으로 쓰기만 하면 된다.
- 그 출력이 곧 클라이언트에게 가는 HTTP 응답 본문(및 일부 헤더)이 된다.

#### CGI 환경변수

대표적으로 다음 값들이 있다.

- `QUERY_STRING`
- `SERVER_PORT`
- `REQUEST_METHOD`
- `REMOTE_HOST`
- `REMOTE_ADDR`
- `CONTENT_TYPE`
- `CONTENT_LENGTH`

즉, CGI 프로그램은 인자를 함수 파라미터로 받는 게 아니라 `환경변수`와 표준 입출력으로 받는다고 이해하면 된다.

#### 왜 child가 직접 헤더를 써야 하나

서버는 자식 프로그램이 어떤 종류의 데이터를 얼마나 만들어낼지 미리 모른다. 그래서 동적 콘텐츠에서는 CGI 프로그램이 보통 아래를 직접 출력한다.

- `Content-type`
- 필요하면 `Content-length`
- 빈 줄
- 실제 본문

#### POST는 어떻게 다른가

책은 주로 GET을 다루지만, 개념적으로는 이 차이만 기억하면 된다.

- GET: 인자가 URI에 있다
- POST: 인자가 request body에 있다

그래서 POST를 처리하는 CGI는 표준 입력(stdin)으로 본문을 읽도록 리다이렉트하는 구조가 흔하다.

### 자주 헷갈리는 포인트

- "동적 콘텐츠도 결국 파일인가?"
  맞다. 보통 실행 가능한 파일이다. 다만 반환되는 바이트는 파일 내용이 아니라 "실행 결과"다.

- "서버가 CGI 프로그램 출력을 복사해 다시 써 주는가?"
  꼭 그렇지 않다. `dup2`로 표준 출력을 소켓에 붙이면 CGI가 직접 클라이언트에게 쓰는 것과 같은 효과가 난다.

---

## 11.6 Putting It Together: The Tiny Web Server

11.6은 이 장의 종합편이다. Tiny는 작지만, 앞에서 배운 개념들이 거의 다 들어 있다.

### Tiny를 이루는 핵심 부품

Tiny는 크게 아래 흐름으로 읽으면 된다.

1. `main`
   연결을 받고 반복한다.

2. `doit`
   한 번의 HTTP 트랜잭션을 처리한다.

3. `read_requesthdrs`
   요청 헤더를 소비한다.

4. `parse_uri`
   정적/동적 요청을 구분하고 경로를 만든다.

5. `serve_static`
   파일을 읽어 응답한다.

6. `serve_dynamic`
   CGI 프로그램을 실행해 응답한다.

7. `clienterror`
   오류 응답을 보낸다.

### Tiny의 실행 흐름

#### 1. main

`main`은 반복 서버(iterative server)다.

- 포트를 받아 listening socket을 연다.
- 무한 루프에서 `accept`를 호출한다.
- 새 연결이 오면 `doit(connfd)`를 수행한다.
- 끝나면 `connfd`를 닫는다.

즉, Tiny는 동시에 여러 클라이언트를 처리하지 않는다. 한 번에 하나씩만 처리한다.

#### 2. `doit`

이 함수가 실제 핵심이다.

작업 순서:

1. 요청 첫 줄(request line)을 읽는다.
2. 메서드/URI/버전을 파싱한다.
3. GET이 아니면 `501 Not Implemented`
4. 나머지 요청 헤더를 읽고 버린다.
5. URI를 파일명/CGI 인자로 분해한다.
6. `stat`로 파일 존재 여부와 메타데이터 확인
7. 정적이면 읽기 권한 확인 후 `serve_static`
8. 동적이면 실행 권한 확인 후 `serve_dynamic`

이 함수만 정확히 이해하면 Tiny 전체의 70%는 이해한 셈이다.

### `clienterror`

서버는 오류가 나도 조용히 끊으면 안 된다. 최소한 브라우저가 이해할 수 있는 HTTP 오류 응답은 보내야 한다.

`clienterror`는:

- 상태 줄 작성
- `Content-Type`
- `Content-Length`
- 에러 메시지를 담은 간단한 HTML body

를 만들어 클라이언트로 보낸다.

여기서 중요한 것은 `HTTP도 결국 텍스트 프로토콜`이라는 점이다. 오류도 규약에 맞춰 보내야 브라우저가 정상 해석한다.

### `read_requesthdrs`

Tiny는 요청 헤더를 거의 활용하지 않는다. 그래서 빈 줄이 나올 때까지 읽고 버린다.

핵심 포인트:

- 헤더 목록의 끝은 `\r\n` 한 줄이다.
- 즉, "빈 줄"이 프로토콜 의미를 가진다.

텍스트 프로토콜을 구현할 때는 이런 경계 규칙이 특히 중요하다.

### `parse_uri`

Tiny는 아주 단순한 정책을 쓴다.

- URI에 `cgi-bin`이 없으면 정적 콘텐츠
- 있으면 동적 콘텐츠

그리고 다음 작업을 한다.

- 정적 요청이면 `.`을 앞에 붙여 현재 디렉터리 기준 파일 경로 생성
- URI가 `/`로 끝나면 기본 파일명(`home.html`)을 붙임
- 동적 요청이면 `?` 뒤를 CGI 인자로 떼어냄

실제 프로덕션 서버라면 훨씬 복잡한 라우팅/보안 검사가 필요하지만, 학습용으로는 아주 좋은 단순화다.

### `serve_static`

정적 콘텐츠 응답은 크게 두 단계다.

1. 응답 헤더 전송
2. 파일 본문 전송

헤더에는 보통 다음이 들어간다.

- `HTTP/1.0 200 OK`
- `Server: Tiny Web Server`
- `Connection: close`
- `Content-length`
- `Content-type`

그다음 파일을 열고 `mmap`으로 메모리에 매핑한 뒤, 그 메모리를 소켓으로 써 준다.

#### 왜 `mmap`을 쓰는가

장점:

- 파일 내용을 사용자 버퍼로 다시 읽어오는 코드가 간단해진다.
- 운영체제 가상 메모리 메커니즘을 활용할 수 있다.

주의:

- 매핑 후 파일 디스크립터를 닫아야 한다.
- 다 쓴 뒤 `Munmap` 해야 한다.

둘 중 하나를 빼먹으면 자원 누수(memory/resource leak)가 난다.

### `get_filetype`

파일 확장자를 보고 MIME 타입을 결정한다.

아주 단순하지만 핵심 아이디어는 중요하다.

브라우저는 단순히 "바이트"만 받는 것이 아니라, `이 바이트를 무엇으로 해석해야 하는지`를 알아야 한다. 그래서 `Content-Type`이 필요하다.

### `serve_dynamic`

동적 콘텐츠는 완전히 다른 경로를 탄다.

1. 서버가 일단 상태 줄과 일부 헤더를 보낸다.
2. `fork`
3. 자식이 `QUERY_STRING` 환경변수 설정
4. 자식이 `dup2(fd, STDOUT_FILENO)`로 표준 출력을 클라이언트 소켓에 연결
5. 자식이 `execve`로 CGI 프로그램 실행
6. 부모는 `wait`로 자식을 회수

#### 이 코드에서 꼭 이해해야 할 한 줄

`Dup2(fd, STDOUT_FILENO);`

이 한 줄 때문에 CGI 프로그램이 `printf`만 해도 그 결과가 브라우저로 간다.

즉,

- CGI 프로그램은 네트워크를 직접 몰라도 된다.
- 그냥 표준 출력으로 HTTP 형식의 텍스트를 쓰면 된다.

이게 Unix 철학의 강력함이다. "모든 것을 파일처럼 다룬다"는 관점이 네트워크에도 적용된다.

### Tiny가 일부러 단순화한 부분

Tiny는 학습용 서버이므로 실제 서버 기능이 많이 빠져 있다.

- GET만 지원
- 반복 서버라서 동시성 없음
- 요청 헤더 거의 무시
- HTTP/1.1의 지속 연결, 캐시, 범위 요청 등 미지원
- 보안 검증 빈약
- 예외 상황 처리 제한적

그렇다고 Tiny가 의미 없는 건 아니다. 오히려 중요한 부분만 남겨서, 웹 서버의 본질을 가장 잘 보여 주는 예제에 가깝다.

### Tiny를 읽으며 꼭 봐야 하는 시스템 포인트

1. `accept`와 `connfd`
   네트워크 연결 수락

2. `rio` 계열 함수
   짧은 읽기/쓰기 문제를 덜어주는 robust I/O

3. `stat`
   파일 존재 여부, 일반 파일 여부, 권한 검사

4. `mmap`
   정적 파일 전송

5. `fork/execve/wait`
   CGI 실행

6. `dup2`
   표준 출력을 소켓으로 리다이렉트

### 실전 관점에서의 추가 주의: SIGPIPE / EPIPE

책도 마지막에 짚지만, 실제 서버는 클라이언트가 중간에 연결을 끊는 상황을 반드시 고려해야 한다.

문제 상황:

- 서버가 이미 닫힌 연결에 `write`
- 첫 번째는 그냥 지나갈 수도 있음
- 다음 `write`에서 `SIGPIPE`로 프로세스가 죽을 수 있음

대응:

- `SIGPIPE` 무시 또는 핸들링
- `write` 오류에서 `EPIPE` 체크

즉, Tiny는 "핵심 구조를 보여 주는 서버"이지, "실서비스에 바로 올릴 수 있는 서버"는 아니다.

---

## 11.7 Summary

11.7은 앞 내용을 한 번 더 압축해서 보여 주는데, 아래처럼 정리하면 머리에 오래 남는다.

### 이 장의 최종 압축본

1. 모든 네트워크 프로그램은 클라이언트-서버 모델 위에서 생각할 수 있다.
2. 인터넷은 서로 다른 네트워크를 하나로 묶는 추상화이며, 핵심은 주소 체계와 패킷 전달이다.
3. 프로그래머는 IP 주소, DNS 이름, 포트, 연결의 개념을 알아야 한다.
4. 실제 코드는 소켓 인터페이스(`socket/connect/bind/listen/accept`)로 쓴다.
5. 현대 코드에서는 `getaddrinfo/getnameinfo`를 쓰는 것이 좋다.
6. 웹 서버는 이 소켓 위에서 HTTP를 해석하고 파일 또는 프로그램 출력을 응답으로 보낸다.
7. Tiny는 그 모든 것을 작게 엮은 종합 예제다.

### 한 번에 연결해서 기억하기

아래 흐름이 자연스럽게 이어지면 11장을 제대로 이해한 것이다.

1. 브라우저는 서버 도메인 이름을 안다.
2. DNS를 통해 IP 주소를 얻는다.
3. `connect`로 서버의 well-known port에 연결한다.
4. 서버는 `listen` 중이다가 `accept`로 새 연결을 받는다.
5. 브라우저는 HTTP request를 보낸다.
6. 서버는 URI를 보고 정적/동적 처리를 결정한다.
7. 정적이면 파일을 읽어 보내고, 동적이면 CGI를 실행한다.
8. 응답은 `Content-Type`, `Content-Length` 같은 헤더와 함께 돌아간다.

### 시험/면접/구현에서 특히 자주 나오는 포인트

- 네트워크는 또 하나의 I/O 장치라는 관점
- encapsulation: 데이터 -> 패킷 -> 프레임
- host byte order vs network byte order
- DNS는 1:1 매핑이 아니라는 점
- socket pair가 연결을 유일하게 식별한다는 점
- `listenfd`와 `connfd`의 차이
- `AI_PASSIVE`, `AI_ADDRCONFIG`, `AI_NUMERICSERV`
- HTTP/1.1에서 `Host` 헤더의 의미
- CGI에서 `QUERY_STRING`, `dup2`, `stdout`
- Tiny는 반복 서버라서 동시성이 없다는 점
- `SIGPIPE/EPIPE` 같은 실제 서버의 함정

---

## 마지막으로: 이 장을 공부하는 좋은 순서

추천 순서는 다음과 같다.

1. `11.1`로 클라이언트-서버 모델을 먼저 확실히 잡기
2. `11.2`에서 encapsulation과 router/frame/packet 차이 이해하기
3. `11.3`에서 IP, DNS, port, connection 개념 정리하기
4. `11.4`에서 소켓 함수 흐름을 손으로 직접 써 보기
5. `11.5`에서 telnet으로 HTTP 요청/응답을 직접 찍어 보기
6. `11.6`에서 Tiny 코드를 위에서 아래로 따라가며 읽기
7. 마지막으로 `CSAPP_ch11_network_examples_ko.c`를 보며 구현 흐름을 다시 연결하기

---

## 빠른 복습용 한 문장 요약

`11장은 "네트워크를 파일 I/O처럼 다루는 법"을 배우고, 그 결과물로 작은 웹 서버를 직접 머릿속에 구현할 수 있게 만드는 장이다.`
