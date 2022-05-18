/*
 * proxy.c
 * SWJungle 4기 이혜진
 * Week07 : Web-Server
 * proxy II: concurrent
 *
 * Reference 
 * 1. Computer System : A Programmer's Perspective
 * 2. https://github.com/Yerimi11/webproxy-jungle
 */
#include <stdio.h>
#include "csapp.h"

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

/* request header convention */
static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 " "Firefox/10.0.3\r\n";
static const char *conn_hdr = "Connection: close\r\n";
static const char *prox_hdr = "Proxy-Connection: close\r\n";
static const char *host_hdr_format = "Host: %s\r\n";
static const char *requestline_hdr_format = "GET %s HTTP/1.0\r\n";
static const char *endof_hdr = "\r\n";

static const char *user_agent_key = "User-Agent";
static const char *connection_key = "Connection";
static const char *proxy_connection_key = "Proxy-Connection";
static const char *host_key = "Host";

void doit(int connfd);
void parse_uri(char *uri, char *hostname, char *path, int *port);
void build_http_header(char *http_header, char *hostname, char *path, int port, rio_t *client_rio);
int connect_endServer(char *hostname, int port);
void *thread(void *vargsp);

int main(int argc, char **argv){
    int listenfd, connfd;
    socklen_t clientlen;
    char hostname[MAXLINE], port[MAXLINE];
    struct sockaddr_storage clientaddr;
    pthread_t tid;

    /* 포트번호가 입력되지 않았으면 에러메세지 출력 */
    if (argc != 2){
        fprintf(stderr, "usage :%s <port> \n", argv[0]);
        exit(1);
    }

    listenfd = Open_listenfd(argv[1]);
    
    while (1) {
        clientlen = sizeof(clientaddr);
        connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);

        Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0);
        printf("Accepted connection from (%s %s).\n", hostname, port);

        // Pthread_create(스레드 식별자, 스레드 속성(default: NULL, 스레드 함수, 스레드함수의 매개변수)
        Pthread_create(&tid, NULL, thread, (void *)connfd);
    }
    return 0;
}

/*
 * thread - 스레드에서 실행될 함수
 */
void *thread(void *vargs) {
    int connfd = (int)vargs; // argument로 받은 것을 connfd에 넣는다
    Pthread_detach(pthread_self()); // pthread_detach : 스레드가 종료되면 자원 반납, pthread_self : 현재 스레드 식별자 확인
    doit(connfd); // 각 스레드별로 doit함수 실행(각자의 connfd를 갖는다)
    Close(connfd);
}

/*
 * doit - HTTP request/response 처리하는 함수
 */
void doit(int connfd){ 
    int end_serverfd; 
    char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
    char endserver_http_header[MAXLINE];
    char hostname[MAXLINE], path[MAXLINE];
    int port;
    rio_t rio, server_rio; 

    Rio_readinitb(&rio, connfd);
    Rio_readlineb(&rio, buf, MAXLINE);
    sscanf(buf, "%s %s %s", method, uri, version); 

    if (strcasecmp(method, "GET")) { // GET요청이 아니면 에러메세지 출력
        printf("Proxy does not implement the method");
        return;
    }
    
    parse_uri(uri, hostname, path, &port); // uri로부터 hostname, path, port 추출

    build_http_header(endserver_http_header, hostname, path, port, &rio); // end_server에 보낼 request header 만들기
    
    end_serverfd = connect_endServer(hostname, port); // end_server와 연결할 connfd(end_serverfd)소켓 만들기
    
    if (end_serverfd < 0) {
        printf("connection failed\n");
        return;
    }

    Rio_readinitb(&server_rio, end_serverfd); // server_rio 빈 버퍼와 end_serverfd(connfd) 연결
    Rio_writen(end_serverfd, endserver_http_header, strlen(endserver_http_header)); // end_serverfd에 request header(endserver_http_header) 보내기

    size_t n;
    while ((n = Rio_readlineb(&server_rio, buf, MAXLINE)) != 0){ // server_rio에 입력된(buf에 담긴) 길이 n이 0이 아니면 while문 실행
        printf("proxy received %d bytes,then send\n", (int)n);
        Rio_writen(connfd, buf, n); // 클라이언트한테 end-server로부터 받은 내용 보내기
    }
    Close(end_serverfd);
}

/*
 * build_http_header - hostname, path, port등 정보를 받고 client가 보낸 request header를 읽어서 end_server에 보낼 request header 만드는 함수
 */
void build_http_header(char *http_header, char *hostname, char *path, int port, rio_t *client_rio){
    char buf[MAXLINE], request_hdr[MAXLINE], other_hdr[MAXLINE], host_hdr[MAXLINE];
    
    sprintf(request_hdr, requestline_hdr_format, path); // request header 첫번째줄 request_hdr에 담기
    
    while (Rio_readlineb(client_rio, buf, MAXLINE) > 0) {
        if (strcmp(buf, endof_hdr) == 0) //  EOF('\r\n')이 입력되면 클라이언트가 보낸 request header 다 읽었으므로 while문 탈출
            break;

        if (!strncasecmp(buf, host_key, strlen(host_key))) { // 읽어온 헤더 정보(buf)가 Host관련 정보면
            strcpy(host_hdr, buf); // buf에 담긴 값 host_hdr에 복사
            continue;
        }

        if (strncasecmp(buf, connection_key, strlen(connection_key)) 
            &&strncasecmp(buf, proxy_connection_key, strlen(proxy_connection_key)) 
            &&strncasecmp(buf, user_agent_key, strlen(user_agent_key))) {
            strcat(other_hdr, buf); // 위 세가지와 관련된 정보가 아니면 other_hdr에 추가
        }
    }

    if (strlen(host_hdr) == 0) { // 만약 클라이언트가 보낸 request header 정보에 Host관련 정보가 없었으면 uri로부터 추출했던 hostname으로 host_hdr 만들기
        sprintf(host_hdr, host_hdr_format, hostname);
    }

    // end_server에 보낼 request header 정보들을 http_header에 담기
    sprintf(http_header, "%s%s%s%s%s%s%s",
            request_hdr,
            host_hdr,
            conn_hdr,
            prox_hdr,
            user_agent_hdr,
            other_hdr,
            endof_hdr);
    return;
}

/*
 * connect_endServer - end_server의 hostname, port 입력받아서 end_server와 연결한 connfd 만들고 반환하는 함수 
 */
inline int connect_endServer(char *hostname, int port) {
    char portStr[100]; // 포트번호 담을 문자열
    sprintf(portStr, "%d", port); // postStr에 port 담기
    return Open_clientfd(hostname, portStr); // hostname:portStr 서버랑 연결 설정, connfd 반환
}

/*
 * parse_uri - 클라이언트의 request header로부터 추출한 uri를 입력받고, uri로부터 hostname, path, port를 추출해서 저장하는 함수
 */
void parse_uri(char *uri, char *hostname, char *path, int *port) {
    char *pos, *pos2;
    *port = 80; // 사용자가 별도의 port를 설정하지 않았으면 기본 80포트

    pos = strstr(uri, "//"); // uri에 스킴이 포함되어있는지 확인
    pos = pos ? (pos + 2) : uri;  // uri에 http://부분(스킴)이 있다면 http://부분 자르고 pos에 저장
    pos2 = index(pos, ':'); // pos(가공된 uri)에 포트번호가 포함되어있는지 확인

    if (pos2) { // 포트번호가 있는 경우
        *pos2 = '\0'; // ':'앞까지만 hostname에 담기 위해 '\0'으로 문자열의 끝을 알림
        sscanf(pos, "%s", hostname); // hostname 정보 담기
        sscanf(pos2 + 1, "%d%s", port, path); // ':'이후에 port랑 path 정보 담기
    } else { // 포트번호가 없는 경우
        pos2 = index(pos, '/'); // path 시작지점 찾기
        if (pos2){ // hostname 뒤에 path가 있는 경우
            *pos2 = '\0'; // '/'앞까지만 hostname에 담기 위해 '\0'으로 문자열의 끝을 알림
            sscanf(pos, "%s", hostname);  // hostname 정보 담기
            *pos2 = '/'; // path에 '/'도 포함되어야하므로 다시 '\0' -> '/'로 다시 변경
            sscanf(pos2, "%s", path);  // path 정보 담기
        } else{ // hostname 뒤에 path가 없는 경우
            sscanf(pos, "%s", hostname);  // hostname 정보 담기
        }
    }
    return;
}