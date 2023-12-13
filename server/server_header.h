#ifndef SERVER_HEADER_H
#define SERVER_HEADER_H
#define MAX_CLIENT (5)                       //可同时连接客户端的最大数目
#define NAME_LENGTH (20)                     //服务器主机名最大长度
                                             //!socket相关头文件
#include <sys/socket.h>                       //Core socket functions and data structures.
#include <netinet/in.h>                       //AF_INET and AF_INET6 address families and their corresponding protocol families, PF_INET and PF_INET6. These include standard IP addresses and TCP and UDP port numbers.
#include <sys/un.h>                           //PF_UNIX and PF_LOCAL address family. Used for local communication between programs running on the same computer.
#include <arpa/inet.h>                        //Functions for manipulating numeric IP addresses.
#include <netdb.h>                            //Functions for translating protocol names and host names into numeric addresses. Searches local data as well as name services.
#include <map>
#include <string>
#include <chrono>
                                             //!server相关结构体
/*struct sockaddr_in {
     short            sin_family;            // 2 字节 ，地址族，e.g. AF_INET, AF_INET6
     unsigned short   sin_port;              // 2 字节 ，16位TCP/UDP 端口号 e.g. htons(1234)
     struct in_addr   sin_addr;              // 4 字节 ，32位IP地址
     char             sin_zero[8];           // 8 字节 ，不使用
};
struct in_addr {
     unsigned long s_addr; 	               // htonl(INADDR_ANY), inet_addr("127.0.0.1")
};
struct sockaddr{ 
    short sa_family;                         //2字节，地址族，AF_xxx
    char sa_data[14];                        //14字节，包含套接字中的目标地址和端口信息 
};*/
                                             //!server相关结构体
struct server_info{
     short _family;                          //地址族，AF_INET or IF_INET6
     unsigned short _port;                   //16位TCP/UDP 端口号 e.g. htons(1234)
     struct in_addr _addr;                   //32位IP地址
     char _name[NAME_LENGTH];                //服务器主机名
     int _socket;                            //服务器套接字
     socklen_t _socket_length;               //服务器套接字长度
};
struct client_info{
     sockaddr _addr;                         //客户端地址族+目标地址+端口信息
     int _socket;                            //客户端套接字
     socklen_t _socket_length;               //客户端套接字长度
     pthread_t _thread_id;                   //客户端对应线程号
};
#endif