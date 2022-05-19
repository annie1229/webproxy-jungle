/*
 * tiny.c
 * SWJungle 4기 이혜진
 * Week07 : Web-Server
 * tiny 웹서버 구현 + Problem 11.6-c, 11.7, 11.9, 11.11 관련 코드 추가
 *
 * Reference 
 * 1. Computer System : A Programmer's Perspective
 * 2. https://dreamanddead.github.io/CSAPP-3e-Solutions/chapter11/
 */
#include "csapp.h"

void doit(int fd);
void read_requesthdrs(rio_t *rp);
int parse_uri(char *uri, char *filename, char *cgiargs);
void serve_static(int fd, char *filename, int filesize, char *method); // Problem 11.11 관련 method 매개변수 추가
void get_filetype(char *filename, char *filetype);
void serve_dynamic(int fd, char *filename, char *cgiargs, char *method);  // Problem 11.11 관련 method 매개변수 추가
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg);

int main(int argc, char **argv) {
    int listenfd, connfd;
    char hostname[MAXLINE], port[MAXLINE];
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;

    /* Check command line args */
    if (argc != 2) {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(1);
    }

    listenfd = Open_listenfd(argv[1]); // 포트를 넘기면 listen descriptor 생성
    
    while (1) {
        clientlen = sizeof(clientaddr);
        connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);

        Getnameinfo((SA *) &clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0);
        printf("Accepted connection from (%s, %s)\n", hostname, port); // 연결된 client의 hostname, port 출력
        
        doit(connfd);
        Close(connfd);
    }
}

/*
 * doit - HTTP request/response 처리하는 함수
 */
void doit(int fd) {
    int is_static;
    struct stat sbuf;
    char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
    char filename[MAXLINE], cgiargs[MAXLINE];
    rio_t rio;

    /* Read request line and headers */
    Rio_readinitb(&rio, fd);
    if (!Rio_readlineb(&rio, buf, MAXLINE))
        return;
    printf("%s", buf);
    sscanf(buf, "%s %s %s", method, uri, version);

    if (!(strcasecmp(method, "GET") == 0 || strcasecmp(method, "HEAD") == 0)) {
        clienterror(fd, method, "501", "Not Implemented", "Tiny does not implement this method");
        return;
    }
    read_requesthdrs(&rio);

    /* Parse URI from GET request */
    is_static = parse_uri(uri, filename, cgiargs);
    if (stat(filename, &sbuf) < 0) {
	    clienterror(fd, filename, "404", "Not found", "Tiny couldn't find this file");
	    return;
    }

    if (is_static) { /* Serve static content */          
        if (!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode)) {
            clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't read the file");
            return;
        }
        serve_static(fd, filename, sbuf.st_size, method);
    }
    else { /* Serve dynamic content */
        if (!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode)) {
            clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't run the CGI program");
            return;
	    }
	    serve_dynamic(fd, filename, cgiargs, method);
    }
}

/*
 * read_requesthdrs - HTTP request header 읽는 함수
 */
void read_requesthdrs(rio_t *rp) {
    char buf[MAXLINE];

    Rio_readlineb(rp, buf, MAXLINE);
    printf("%s", buf);
    while(strcmp(buf, "\r\n")) {
        Rio_readlineb(rp, buf, MAXLINE);
        printf("%s", buf);
    }
    return;
}

/*
 * parse_uri - uri로부터 filename, cgiargs 추출하고, 동적콘텐츠(1)인지 정적콘텐츠(0)인지 구분하여 리턴
 */
int parse_uri(char *uri, char *filename, char *cgiargs) {
    char *ptr;

    if (!strstr(uri, "cgi-bin")) {  /* Static content */
        strcpy(cgiargs, "");
        strcpy(filename, ".");
        strcat(filename, uri);
        if (uri[strlen(uri)-1] == '/')
            strcat(filename, "adder.html");
        return 1;
    }
    else {  /* Dynamic content */
        ptr = index(uri, '?');
        if (ptr) {
            strcpy(cgiargs, ptr+1);
            *ptr = '\0';
	    }
        else 
            strcpy(cgiargs, "");
        strcpy(filename, ".");
        strcat(filename, uri);
        return 0;
    }
    return 0;
}

/*
 * serve_static - 정적 콘텐츠 처리하는 함수
 */
void serve_static(int fd, char *filename, int filesize, char *method) {
    int srcfd;
    char *srcp, filetype[MAXLINE], buf[MAXBUF];

    if (strcasecmp(method, "HEAD") == 0) // Problem 11.11 관련 내용
        return;

    /* Send response headers to client */
    get_filetype(filename, filetype);
    sprintf(buf, "HTTP/1.0 200 OK\r\n");
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Server: Tiny Web Server\r\n");
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-length: %d\r\n", filesize);
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-type: %s\r\n\r\n", filetype);
    Rio_writen(fd, buf, strlen(buf));

    /* Send response body to client */
    srcfd = Open(filename, O_RDONLY, 0);
    /*
     * Problem 11.9 관련 start
     */
    srcp = (char *)Malloc(filesize);
    Rio_readn(srcfd, srcp, filesize);
    Close(srcfd);
    Rio_writen(fd, srcp, filesize);
    free(srcp);
    /*
     * Problem 11.9 관련 start
     */
}

/*
 * get_filetype - filename으로부터 알아낸 filetype 담기
 */
void get_filetype(char *filename, char *filetype) {
    if (strstr(filename, ".html"))
	    strcpy(filetype, "text/html");
    else if (strstr(filename, ".gif"))
	    strcpy(filetype, "image/gif");
    else if (strstr(filename, ".png"))
	    strcpy(filetype, "image/png");
    else if (strstr(filename, ".jpg"))
	    strcpy(filetype, "image/jpeg");
    /*
     * Problem 11.7 관련 start
     */
    else if (strstr(filename, ".mpeg"))
	    strcpy(filetype, "video/mpeg");
    else if (strstr(filename, ".mp4"))
	    strcpy(filetype, "video/mp4");
    /*
     * Problem 11.7 관련 end
     */
    else
	    strcpy(filetype, "text/plain");
}  

/*
 * serve_dynamic - 동적콘텐츠 처리하는 함수(자식 프로세스를 만들어서 처리)
 */
void serve_dynamic(int fd, char *filename, char *cgiargs, char *method) 
{
    char buf[MAXLINE], *emptylist[] = { NULL };

    /* Return first part of HTTP response */
    sprintf(buf, "HTTP/1.0 200 OK\r\n"); 
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Server: Tiny Web Server\r\n");
    Rio_writen(fd, buf, strlen(buf));
  
    if (Fork() == 0) { /* 자식프로세스라면 */
        setenv("QUERY_STRING", cgiargs, 1);
        setenv("REQUEST_METHOD", method, 1);
        Dup2(fd, STDOUT_FILENO); /* stdout을 클라이언트(fd)로 연결 */
        Execve(filename, emptylist, environ); /* Run CGI program */
    }
    Wait(NULL); /* 부모프로세스는 자식프로세스가 끝날때까지 기다림 */
}

/*
 * clienterror - 클라이언트에 출력할 에러메세지 담아서 보내기
 */
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg) {
    char buf[MAXLINE];

    /* HTTP response headers */
    sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-type: text/html\r\n\r\n");
    Rio_writen(fd, buf, strlen(buf));

    /* HTTP response body */
    sprintf(buf, "<html><title>Tiny Error</title>");
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "<body bgcolor=""ffffff"">\r\n");
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "%s: %s\r\n", errnum, shortmsg);
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "<p>%s: %s\r\n", longmsg, cause);
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "<hr><em>The Tiny Web server</em>\r\n");
    Rio_writen(fd, buf, strlen(buf));
}
