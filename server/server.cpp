//使用 <pthread.h> 中的互斥锁需要在编译时链接 pthread 库，方法是在编译命令中添加 -pthread 参数。
#include<iostream>
#include<pthread.h>
#include"server_header.h"
#include"../packet.h"

void *receive(void* id);                                                                                //子进程入口函数
bool connect(int socket, uint16_t port);                                                                //主机连接到目标端口
struct server_info server;                                                                              //记录主机信息
std::map<int, struct client_info>client;                                                                //键值对是客户端id和信息
pthread_mutex_t mutex;                                                                                  //互斥锁

int main()
{
    bool _is_connected{false};                                                                          //主机是否连接到目标端口
    pthread_mutex_init(&mutex, NULL);                                                                   //互斥锁初始化
                                                                                                        //!运行初始化，调用socket()，向操作系统申请socket句柄
    server._socket = socket(AF_INET,SOCK_STREAM,0);
    if (server._socket == -1) {                                                                         //socket句柄申请失败
        std::cerr << "[Error]: Server Socket Creation Failed" << std::endl;
        return 1;
    }
    while(!_is_connected){
        std::cout << "\n请输入连接主机名" << std::endl;
        std::cin >> server._name;
        std::cout << "\n请输入连接端口" << std::endl;
        std::cin >> server._port;
        _is_connected = connect(server._socket,server._port);                                           //连接到目标端口，设置主机名
    }
                                                                                                        //!主线程循环调用accept()，直到返回一个有效的socket句柄
    int id = 0;
    server._socket_length = sizeof(server._socket);
    while(1){
        sockaddr client_addr;
        int new_socket = accept(server._socket, &client_addr, &server._socket_length);                  //server的写操作均发生在创建新线程前，无需加锁
        if(new_socket < 0){                                                                             //申请新句柄失败
            std::cerr << "[Error]: Server Accept Failed" << std::endl;
            continue;
        }
        else {
                                                                                                        //!增加一个新客户端的项目，并记录下该客户端句柄和连接状态、端口
            while(pthread_mutex_trylock(&mutex)){}                                                      //加锁
            pthread_t thread;
            if(pthread_create(&thread, NULL, receive, (void*)&new_socket)){                             //子进程创建失败
                std::cerr << "[Error]: pthread_create() Failed" << std::endl;
                continue;
            }
            struct client_info new_client = {client_addr, new_socket, sizeof(new_socket), thread};      //记录新添加客户端的信息
            client.insert(std::make_pair(++id, new_client));                                            //添加id和客户端信息的键值对
            pthread_mutex_unlock(&mutex);                                                               //释放锁
        }
    }
}

bool connect(int socket, uint16_t port){
                                                                                                        //!调用bind()，绑定监听端口
    sockaddr_in _server_addr = {AF_INET, htons(port), inet_addr("127.0.0.1")};
    sockaddr server_addr;
    strncpy(reinterpret_cast<char*>(&server_addr), reinterpret_cast<char*>(&_server_addr), sizeof(_server_addr));
    const sockaddr ss_ptr = server_addr;
    if(bind(socket, &ss_ptr, sizeof(_server_addr)) < 0) {                                               //监听端口绑定失败
        std::cerr << "[Error]: Server Bind Failed" << std::endl; 
        return false;
    }
                                                                                                        //!接着调用listen()，设置连接等待队列长度
    if(listen(socket, MAX_CLIENT) < 0){                                                                 //MAX_CLIENT为客户端最大连接数量
        std::cerr << "[Error]: Server Listen Failed" << std::endl;                                      //端口监听失败
        return false;
    }
    return true;
}

void *receive(void* id)
{
    int client_id;
    strncpy((char*)&client_id, (char*)id, sizeof(int));
    struct packet receive_packet;
                                                                                                            //!调用send()，发送一个hello消息给客户端
    char payload[PAYLOAD_LENGTH] = "Successfully Connect";
    struct packet message = {CONFIRM_NUM,SendMessage,ServerId,client_id,*payload};
    char* ss_ptr = serialize(message);                                                                      //序列化待发送数据包
    strncpy(payload, ss_ptr, PACKET_LENGTH);
    free(ss_ptr);
    while(pthread_mutex_trylock(&mutex)){}                                                                  //加锁
    send(client[client_id]._socket, (void*)(payload), PACKET_LENGTH, 0);                                    //发送连接成功确认，告知客户端id
    pthread_mutex_unlock(&mutex);                                                                           //释放锁
    while(1){
        ss_ptr = (char*)malloc(PACKET_LENGTH);
        if(recv(client[client_id]._socket, (void*)&ss_ptr, PACKET_LENGTH, 0) != PACKET_LENGTH){             //数据包长度错误
            std::cerr << "[Error]: Recv Length Error" << std::endl;
            continue;
        }
        strncpy((char*)&receive_packet, ss_ptr, PACKET_LENGTH);                                             //反序列化接收到的数据包
        free(ss_ptr);
        if(receive_packet.confirm_num != CONFIRM_NUM){                                                      //反序列化确认号错误
            std::cerr << "[Error]: Deserialize Error" << std::endl;
        }
                                                                                                            //!收到一个完整的包
        switch(receive_packet.command){
            case GetTime:{                                                                                  //获取时间
                break;
            }
            case GetServerName:{                                                                            //获取主机名
                break;
            }
            case GetClientList:{                                                                            //获取客户端列表
                break;
            }
            case SendMessage:{                                                                              //发送消息
                break;
            }
            case Disconnect:{                                                                               //断开连接
                break;
            }
            case Err:{                                                                                      //发生错误
                break;
            }
            default:{                                                                                       //未定义命令
                std::cerr << "[Error]: Undefined Command" << std::endl;
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