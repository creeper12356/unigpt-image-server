#ifndef CORE_H
#define CORE_H
#include "config.h"
#include "csapp.h"


/**
 * @brief http请求头的部分元数据
 * 
 */
struct http_header_meta_data {
    char content_type[MAXLINE]; // 请求体的类型
    int content_length; // 请求体的长度
};


void doit(int fd);

/**
 * @brief 客户端错误响应
 * 
 * @param fd 客户端连接的文件描述符
 * @param cause 
 * @param errnum 
 * @param shortmsg 
 * @param longmsg 
 */
void serve_error_response(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg);

/**
 * @brief 向客户端返回JSON响应
 * 
 * @param fd 客户端连接的文件描述符
 * @param status_code 状态码
 * @param json JSON字符串
 */
void serve_json_response(int fd, char *status_code, char *json);

/**
 * @brief 从multipart/form-data内容类型中解析"boundary"字符串
 * 
 * @param content_type 
 * @param boundary 输出的boundary字符串
 * @return int 成功返回0，失败返回-1
 */
int parse_boundary_from_content_type(const char *content_type, char *boundary);

/**
 * @brief 读取请求头，写入meta_data
 * 
 * @param meta_data 
 * @return int 成功返回0，失败返回-1
 */
int read_request_headers(struct http_header_meta_data *meta_data);

/**
 * @brief 解析uri中的端点和文件名
 * 
 * @param uri 
 * @param endpoint 输出的端点
 * @param filename 输出的文件名
 * @return int 成功返回0，失败返回-1
 */
int parse_uri(char *uri, char *endpoint, char *filename);


int gen_unique_file_name(char *filename);

/**
 * @brief 处理查看文件请求
 * 
 * @param fd 客户端连接的文件描述符
 * @param filename 文件名
 */
void serve_static_file(int fd, char *filename);

/**
 * @brief 处理上传文件请求
 * 
 * @param fd 客户端连接的文件描述符
 * @param filename 上传的文件名
 */
void serve_upload_file(int fd, char *filename);

/**
 * @brief 获取文件类型
 * 
 * @param filename 文件名
 * @param filetype 输出的文件类型
 */
void get_filetype(char *filename, char *filetype);
#endif // CORE_H