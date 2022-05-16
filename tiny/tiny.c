/*
 * tiny.c - tiny server using HTTP1.0
 *
 */

#include "csapp.h"

void doit(int fd);
void read_requesthdrs(rio_t *rp);
int parse_uri(char *uri, char *filename, char *cgiargs);
void serve_static(int fd, char *filename, int filesize);
void get_filetype(char *filename, char *filetype);
void serve_dynamic(int fd, char *filename, char *cgiargs);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg,
                 char *longmsg);



int main(int argc, char **argv){

    int listenfd, connfd;
    char hostname[MAXLINE], port[MAXLINE];
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;

    /* check command line args(port) */
    if (argc != 2) {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(1);
    }
    
    /* open listen socket and save descriptor */
    listenfd = Open_listenfd(argv[1]);
    while(1) {
        clientlen = sizeof(clientaddr);
        connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);

        /* fill in the linked list */
        Getnameinfo((SA *) &clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE,0);
        printf("Accepted connection from (%s, %s)\n", hostname, port);

        /* process transaction */
        doit(connfd);
        Close(connfd);           
    }
}

void doit(int fd)
{
    int is_static;
    struct stat sbuf;
    char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
    char filename[MAXLINE], cgiargs[MAXLINE];
    rio_t rio; 
    
    /* Read request line and headers */
    Rio_readinitb(&rio, fd);            // associate read buffer with descriptor
    Rio_readlineb(&rio, buf, MAXLINE);  // copy from rio to buf endswith null character
    printf("Request headers: \n");
    printf("%s", buf);
    sscanf(buf, "%s %s %s", method, uri, version); // input from buffer

    /* check request method */
    if (strcasecmp(method, "GET")){
        clienterror(fd, method, "501", "Not implemented", "Tiny does not implement this method");
        return;
    }
    read_requesthdrs(&rio); // read headers and print it

    /*Parse URI from GET request */
    is_static = parse_uri(uri, filename, cgiargs);
    if (stat(filename, &sbuf) < 0){ // metadata will be saved in stat struct
        clienterror(fd, filename, "404", "Not found", "Tiny couldn't find this file");
        return;
    }

    /* serve static or dynamic content */
    if (is_static) { /* Serve static content */
        if (!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode)){
            /* if the file isn't regular file or doesn't have permission for read */
            clienterror(fd, filename,"403", "Forbidden", "Tiny couldn't read the file'");
        }
        serve_static(fd, filename, sbuf.st_size);
    }
    else { /* serve dynamic content */
        if (!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode)) {
            /* if the file isn't regular file or doesn't have permission for execute' */
            clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't run the CGI program");
            return;
        }
        serve_dynamic(fd, filename, cgiargs);
    }
}



/*
 * clienterror - sends an error message to the client using Rio writen
 */
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg)
{
    char buf[MAXLINE], body[MAXBUF];

    /* Print the HTTP response headers */
    sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-type: text/html\r\n\r\n");
    Rio_writen(fd, buf, strlen(buf));

    /* Print the HTTP response body */
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
    Rio_writen(fd, body, strlen(body));
    /* sprintf - add string to buffer(body) */

}



/*
 *  read_requesthdrs - reads and ignores request headers.
 *
 *  Q. Why ignore request headers?
 *  A. tiny server doesn't use any of the information in the request headers.
 *
 */
void read_requesthdrs(rio_t *rp)
{
    char buf[MAXLINE];

    Rio_readlineb(rp, buf, MAXLINE);
    while(strcmp(buf, "\r\n")) {
        Rio_readlineb(rp, buf, MAXLINE);
        printf("%s", buf);
    }
    return;
}

/*
 * parse_uri - parses an HTTP URI -> CGI args
 *             return 0 if dynamic content else 1
 */
int parse_uri(char *uri, char *filename, char *cgiargs)
{
    char *ptr;

    /* serve static */
    if (!strstr(uri, "cgi-bin")){ // it return true if "cgi-bin" in uri
        strcpy(cgiargs, "");      // clear cgiargs, because we don't need it
        strcpy(filename, ".");    // init filename
        strcat(filename, uri);    // append uri to filename with null string

        if (uri[strlen(uri)-1] == '/')
            strcat(filename, "home.html"); // append home.html to filename "./home.html"
        return 1;    
    }

    /* serve dynamic */
    else {
        ptr = index(uri, '?');     // find character "?"
        if (ptr) {
            strcpy(cgiargs, ptr+1);// save characters after the "?"
            *ptr = '\0';           // change "?" to "\0"
        }
        else{
            strcpy(cgiargs, "");   // if there isn't any query string, cgiargs == ''
        }
        strcpy(filename, ".");      
        strcat(filename, uri);     // " .{uri} "
        return 0;
    }
}


/*
 * get_filetype - derive file type from file name
 */
void get_filetype(char *filename, char *filetype) 
{
    /* strstr return substring if there is */
    if (strstr(filename, ".html"))
	strcpy(filetype, "text/html");
    else if (strstr(filename, ".gif"))
	strcpy(filetype, "image/gif");
    else if (strstr(filename, ".png"))
	strcpy(filetype, "image/png");
    else if (strstr(filename, ".jpg"))
	strcpy(filetype, "image/jpeg");
    else if (strstr(filename, ".mp4")) // 11.7 MPG 
    strcpy(filetype, "video/mp4");
    else
	strcpy(filetype, "text/plain");
}  


/*
 * serve_static - copy a file back to the client 
 */
void serve_static(int fd, char *filename, int filesize)
{
    int srcfd;
    char *srcp, filetype[MAXLINE], buf[MAXBUF];

    /* Send response headers to client */
    get_filetype(filename, filetype);    
    sprintf(buf, "HTTP/1.0 200 OK\r\n"); 
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Server: Tiny Web Server\r\n");
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-length: %d\r\n", filesize);
    Rio_writen(fd, buf, strlen(buf));
    /* repeated for readreqhdrs, it indicates finish of the header */
    sprintf(buf, "Content-type: %s\r\n\r\n", filetype);
    Rio_writen(fd, buf, strlen(buf));    

    /* Send response body to client */
    srcfd = Open(filename, O_RDONLY, 0); 
    srcp = (char *)malloc(filesize); // filesize from metadata
    Rio_readn(srcfd, srcp, filesize);
    Close(srcfd);                       
    Rio_writen(fd, srcp, filesize);    
    free(srcp);
}

/* $begin serve_dynamic */
void serve_dynamic(int fd, char *filename, char *cgiargs) 
{
    char buf[MAXLINE], *emptylist[] = { NULL };

    /* Return first part of HTTP response */
    sprintf(buf, "HTTP/1.0 200 OK\r\n"); 
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Server: Tiny Web Server\r\n");
    Rio_writen(fd, buf, strlen(buf));
  
    if (Fork() == 0) { /* Child */
        /* Real server would set all CGI vars here */
        setenv("QUERY_STRING", cgiargs, 1);
        Dup2(fd, STDOUT_FILENO);         /* Redirect stdout to client */
        Execve(filename, emptylist, environ); /* Run CGI program */
    }
    Wait(NULL); /* Parent waits for and reaps child */
}

