/*
 * adder.c
 * SWJungle 4기 이혜진
 * Week07 : Web-Server
 * Problem 11.10, 11.11 관련 코드 추가
 *
 * Reference 
 * 1. Computer System : A Programmer's Perspective
 * 2. https://dreamanddead.github.io/CSAPP-3e-Solutions/chapter11/
 */
#include "csapp.h"

int main(void) {
    char *buf, *p, *method;
    char arg1[MAXLINE], arg2[MAXLINE], content[MAXLINE];
    int n1=0, n2=0;

    /* Extract the two arguments */
    if ((buf = getenv("QUERY_STRING")) != NULL) {
        p = strchr(buf, '&');
        *p = '\0';
        /*
        * Problem 11.10 관련 start
        */
        sscanf(buf, "first=%d", &n1);
        sscanf(p+1, "second=%d", &n2);
        /*
        * Problem 11.10 관련 end
        */
    }
    method = getenv("REQUEST_METHOD");
    /* Make the response body */
    sprintf(content, "Welcome to add.com: ");
    sprintf(content, "%sTHE Internet addition portal.\r\n<p>", content);
    sprintf(content, "%sThe answer is: %d + %d = %d\r\n<p>", content, n1, n2, n1 + n2);
    sprintf(content, "%sThanks for visiting!\r\n", content);
  
    /* Generate the HTTP response */
    printf("Connection: close\r\n");
    printf("Content-length: %d\r\n", (int)strlen(content));
    printf("Content-type: text/html\r\n\r\n");
    /*
    * Problem 11.11 관련 start
    */
    if (strcasecmp(method, "HEAD") != 0)
        printf("%s", content);
    /*
    * Problem 11.11 관련 end
    */
    fflush(stdout);

    exit(0);
}
