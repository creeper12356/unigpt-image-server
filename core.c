#include <strings.h>
#include <assert.h>
#include <sys/mman.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

#include "config.h"
#include "csapp.h"
#include "core.h"

static rio_t rio;
static char buf[MAXLINE];

void doit(int fd) {
    char method[MAXLINE], uri[MAXLINE], version[MAXLINE];
    char endpoint[MAXLINE] = {};
    char filename[MAXLINE] = {};

    rio_readinitb(&rio, fd);
    rio_readlineb(&rio, buf, MAXLINE);
    printf("Request headers:\n");
    printf("%s", buf);
    sscanf(buf, "%s %s %s", method, uri, version);

    if(strcasecmp(method, "GET") && strcasecmp(method, "POST")) {
        serve_error_response(fd, method, "405", "Method Not Allowed", 
            "Tiny does not implement this method");
        return;
    }

    if(parse_uri(uri, endpoint, filename) == -1) {
        serve_error_response(fd, uri, "400", "Bad Request", "Tiny couldn't parse the uri");
        return ;
    }


    if(!strcmp(endpoint, "file") && !strcasecmp(method, "GET") ) {
        serve_static_file(fd, filename);
        return ;
    } else if(!strcmp(endpoint, "upload") && !strcasecmp(method, "POST")) {
        serve_upload_file(fd, filename);
        return ;
    } else {
        serve_error_response(fd, uri, "400", "Bad Request", "Tiny couldn't parse the uri");
    }
}


void serve_error_response(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg) {
    char body[MAXBUF];

    sprintf(body, "<html><title>Tiny Error</title>");
    sprintf(body, "%s<body bgcolor=""ffffff"">\r\n", body);
    sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
    sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
    sprintf(body, "%s<hr><em>The Tiny Web server</em>\r\n", body);

    sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
    rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-type: text/html\r\n");
    rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
    rio_writen(fd, buf, strlen(buf));
    rio_writen(fd, body, strlen(body));
}

void serve_json_response(int fd, char *status_code, char *json) {
    int json_len = strlen(json);

    sprintf(buf, "HTTP/1.0 %s OK\r\n", status_code);
    sprintf(buf, "%sServer: Tiny Web Server\r\n", buf);
    sprintf(buf, "%sConnection: close\r\n", buf);
    sprintf(buf, "%sContent-length: %d\r\n", buf, json_len);
    sprintf(buf, "%sContent-type: application/json\r\n\r\n", buf);
    rio_writen(fd, buf, strlen(buf));

    printf("Response headers:\n");
    printf("%s", buf);
    
    rio_writen(fd, json, json_len);
}

void read_requesthdrs(rio_t *rp) {
    rio_readlineb(rp, buf, MAXLINE);
    while(strcmp(buf, "\r\n")) {
        rio_readlineb(rp, buf, MAXLINE);
        printf("%s", buf);
    }
}

int parse_boundary_from_content_type(const char *content_type, char *boundary) {
    if(sscanf(content_type, "multipart/form-data; boundary=%s", boundary) != 1) {
        return -1;
    }
    return 0;
}
int read_request_headers(struct http_header_meta_data *meta_data) {
    rio_readlineb(&rio, buf, MAXLINE);
    while(strcmp(buf, "\r\n")) {
        if(strstr(buf, "Content-Type:")) {
            if(sscanf(buf, "Content-Type: %s", meta_data->content_type) != 1) {
                return -1;
            }
        }
        if(strstr(buf, "Content-Length:")) {
            if(sscanf(buf, "Content-Length: %d", &meta_data->content_length) != 1) {
                return -1;
            }
        }
        rio_readlineb(&rio, buf, MAXLINE);
    }
    return 0;
}

int parse_uri(char *uri, char *endpoint, char *filename) {
    if(sscanf(uri, "/%[^/]/%s", endpoint, filename) != 2) {
        // 解析uri失败
        return -1;
    }
    if(strcmp(endpoint, "upload") && strcmp(endpoint, "file")) {
        return -1;
    }
    return 0;
}

int gen_unique_file_name(char *filename) {
    char new_file_name[MAXLINE] = FILE_TEMPLATE;
    int tempfd = mkstemp(new_file_name);
    if(tempfd < 0) {
        return -1;
    }
    close(tempfd);
    unlink(new_file_name);
    strcat(new_file_name, ".png");
    strcpy(filename, new_file_name);
    return 0;
}
void serve_upload_file(int fd, char *filename) {
    char request_body[MAXFILESIZE];

    struct http_header_meta_data meta_data;
    read_request_headers(&meta_data);

    rio_readnb(&rio, request_body, meta_data.content_length);

    // 去除request_body前4行
    char *rawdata = request_body;
    for(int i = 0; i < 4; i++) {
        rawdata = strchr(rawdata, '\n') + 1;
    }

    // 将raw data写入文件
    char new_file_name[MAXLINE];
    if(gen_unique_file_name(new_file_name) < 0) {
        serve_error_response(fd, filename, "500", "Internal Server Error", "Tiny couldn't generate unique file name");
        return ;
    }
    int filefd = open(new_file_name, O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR);
    write(filefd, rawdata, meta_data.content_length);
    close(filefd);

    char response[MAXLINE];
    sprintf(
        response, 
        "{\"status\": \"success\", \"url\": \"%s://%s:%s/file/%s\"}",
        SERVER_PROTOCOL,
        SERVER_IP,
        SERVER_PORT,
        new_file_name
    );

    serve_json_response(fd, "200", response);
}

void serve_static_file(int fd, char *filename) {
    struct http_header_meta_data meta_data;
    read_request_headers(&meta_data);


    struct stat sbuf;
    if(stat(filename, &sbuf) < 0) {
        serve_error_response(fd, filename, "404", "Not found", "No such file");
        return ;
    }

    if(!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode)) {
        serve_error_response(fd, filename, "403", "Forbidden", "Tiny couldn't read the file");
        return ;
    }
    int filesize = sbuf.st_size;
    int srcfd;
    char *srcp, filetype[MAXLINE], buf[MAXBUF];

    get_filetype(filename, filetype);
    sprintf(buf, "HTTP/1.0 200 OK\r\n");
    sprintf(buf, "%sServer: Tiny Web Server\r\n", buf);
    sprintf(buf, "%sConnection: close\r\n", buf);
    sprintf(buf, "%sContent-length: %d\r\n", buf, filesize);
    sprintf(buf, "%sContent-type: %s\r\n\r\n", buf, filetype);
    rio_writen(fd, buf, strlen(buf));
    printf("Response headers:\n");
    printf("%s", buf);

    srcfd = open(filename, O_RDONLY, 0);
    srcp = mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0);
    close(srcfd);
    rio_writen(fd, srcp, filesize);
    munmap(srcp, filesize);
}

void get_filetype(char *filename, char *filetype) {
    if(strstr(filename, ".html")) {
        strcpy(filetype, "text/html");
    } else if(strstr(filename, ".gif")) {
        strcpy(filetype, "image/gif");
    } else if(strstr(filename, ".png")) {
        strcpy(filetype, "image/png");
    } else if(strstr(filename, ".jpg")) {
        strcpy(filetype, "image/jpeg");
    } else {
        strcpy(filetype, "text/plain");
    }
}