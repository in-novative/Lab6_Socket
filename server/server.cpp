//使用 <pthread.h> 中的互斥锁需要在编译时链接 pthread 库，方法是在编译命令中添加 -pthread 参数。
#include<iostream>
#include<pthread.h>
#include"server_header.h"

void *receive(void*);
bool connect(int socket, uint16_t port);
struct server_info server;
std::map<int, struct client_info>client;      //键值对是客户端id和信息

int main()
{
    bool _is_connected{false};
    pthread_mutex_t mutex;
    pthread_mutex_init(&mutex, NULL);

    //运行初始化，调用socket()，向操作系统申请socket句柄
    server._socket = socket(AF_INET,SOCK_STREAM,0);
    if (server._socket == -1) {
        std::cerr << "Error: Server Socket creation failed" << std::endl;
        return 1;
    }
    while(!_is_connected){
        std::cout << "\n请输入连接主机名" << std::endl;
        std::cin >> server._name;
        std::cout << "\n请输入连接端口" << std::endl;
        std::cin >> server._port;
        _is_connected = connect(server._socket,server._port);
    }

    //主线程循环调用accept()，直到返回一个有效的socket句柄
    int id = 0;
    server._socket_length = sizeof(server._socket);
    while(1){
        sockaddr client_addr;
        int new_socket = accept(server._socket, &client_addr, &server._socket_length);      //server的写操作发生在创建新线程前
        if(new_socket < 0){
            std::cerr << "Error: Server Accept failed" << std::endl;
            continue;
        }
        else {
            //增加一个新客户端的项目，并记录下该客户端句柄和连接状态、端口
            while(pthread_mutex_trylock(&mutex)){}       //资源被其他进程占用
            pthread_t thread;
            if(pthread_create(&thread, NULL, receive, (void*)&new_socket)){
                std::cerr << "Error: pthread_create() failed" << std::endl;
                continue;
            }
            struct client_info new_client = {client_addr, new_socket, sizeof(new_socket), thread};
            client.insert(std::make_pair(++id, new_client));
            pthread_mutex_unlock(&mutex);
        }
    }
}

bool connect(int socket, uint16_t port){
    //调用bind()，绑定监听端口
    sockaddr_in _server_addr = {AF_INET, htons(port), inet_addr("127.0.0.1")};
    sockaddr server_addr;
    strncpy(reinterpret_cast<char*>(&server_addr), reinterpret_cast<char*>(&_server_addr), sizeof(_server_addr));
    const sockaddr ss = server_addr;
    if(bind(socket, &ss, sizeof(_server_addr)) < 0) {
        std::cerr << "Error: Server Bind failed" << std::endl; 
        return false;
    }
    //接着调用listen()，设置连接等待队列长度
    if(listen(socket, MAX_CLIENT) < 0){
        std::cerr << "Error: Server Listen failed" << std::endl;
        return false;
    }
    return true;
}

void *receive(void* new_socket)
{
    int socket;
    strncpy((char*)&socket, (char*)new_socket, sizeof(socket));
    //调用send()，发送一个hello消息给客户端
    char payload[] = "successfully connect";
    send(socket, (void*)payload, sizeof(payload), 0);
    while(1){
        int receive_length;
        packet receive_packet;
        receive_length = recv(socket, (void*)&receive_packet, sizeof(receive_packet), 0);
        if(receive_length>0 && receive_length==receive_packet.length){      //收到一个完整的包
            switch(receive_packet.command){
                case '1':{
                    break;
                }
                case '2':{
                    break;
                }
                case '3':{
                    break;
                }
                case '4':{
                    break;
                }
                case '5':{
                    break;
                }
                default:{

                }
            }
        }
    }
}

/*
a)向客户端传送服务端所在机器的当前时间
b)向客户端传送服务端所在机器的名称
c)向客户端传送当前连接的所有客户端信息
d)将某客户端发送过来的内容转发给指定编号的其他客户端
e)采用异步多线程编程模式，正确处理多个客户端同时连接，同时发送消息的情况
*/