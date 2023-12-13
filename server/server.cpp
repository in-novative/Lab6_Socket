//使用 <pthread.h> 中的互斥锁需要在编译时链接 pthread 库，方法是在编译命令中添加 -pthread 参数。
/*
a)向客户端传送服务端所在机器的当前时间
b)向客户端传送服务端所在机器的名称
c)向客户端传送当前连接的所有客户端信息
d)将某客户端发送过来的内容转发给指定编号的其他客户端
e)采用异步多线程编程模式，正确处理多个客户端同时连接，同时发送消息的情况
*/
#include <iostream>
#include <pthread.h>
#include "server_header.h"
#include "../packet.h"

void *receive(void* id);                                                                                //子进程入口函数
bool connect(int socket, uint16_t port);                                                                //主机连接到目标端口
std::string _GetTime();                                                                                 //返回当前时间
std::string _GetServerName();                                                                           //返回主机名
std::string _GetClientList();                                                                           //返回客户端列表

struct server_info server;                                                                              //记录主机信息，详见server_header.h
std::map<int, struct client_info>client;                                                                //键值对是客户端id和相关信息，详见server_header.h
pthread_mutex_t mutex;                                                                                  //互斥锁
int id{0};                                                                                              //客户端id

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
        std::cout << "请输入连接主机名\n";
        std::cin >> server._name;
        std::cout << "请输入连接端口\n";
        std::cin >> server._port;
        _is_connected = connect(server._socket,server._port);                                           //连接到目标端口，设置主机名
    }
                                                                                                        //!主线程循环调用accept()，直到返回一个有效的socket句柄
    server._socket_length = sizeof(server._socket);
    while(1){
        sockaddr client_addr;
        int new_socket = accept(server._socket, &client_addr, &server._socket_length);                  //?server的写操作均发生在创建新线程前，无需加锁
        if(new_socket < 0){                                                                             //申请新句柄失败
            //std::cerr << "[Error]: Server Accept Failed" << std::endl;
            continue;
        }
        else {                                                                                          //申请新句柄成功,有新的客户端连接
                                                                                                        //!增加一个新客户端的项目，并记录下该客户端句柄和连接状态、端口
            while(pthread_mutex_trylock(&mutex)){}                                                      //加锁
            pthread_t thread;
            if(pthread_create(&thread, NULL, receive, (void*)&new_socket)){                             //子进程创建失败
                std::cerr << "[Error]: Thread Creation Failed" << std::endl;
                pthread_mutex_unlock(&mutex);                                                           //释放锁
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
    delete ss_ptr;
    while(pthread_mutex_trylock(&mutex)){}                                                                  //加锁
    send(client[client_id]._socket, (void*)(payload), PACKET_LENGTH, 0);                                    //发送连接成功确认，告知客户端id
    pthread_mutex_unlock(&mutex);                                                                           //释放锁
    while(1){
        ss_ptr = new char[PACKET_LENGTH];
        if(recv(client[client_id]._socket, (void*)&ss_ptr, PACKET_LENGTH, 0) != PACKET_LENGTH){             //数据包长度错误
            std::cerr << "[Error]: Recv Length Error" << std::endl;
            continue;
        }
        memcpy((char*)&receive_packet, (char*)deserialize(ss_ptr), PACKET_LENGTH);                          //反序列化接收到的数据包
        delete ss_ptr;
        if(receive_packet.confirm_num != CONFIRM_NUM){                                                      //反序列化确认号错误
            std::cerr << "[Error]: Deserialize Failed" << std::endl;
            continue;
        }
        if(receive_packet.src != client_id){                                                                //客户端id错误
            std::cerr << "[Error]: Client Id Error" << std::endl;
            continue;
        }
                                                                                                            //!收到一个完整的包
        switch(receive_packet.command){
            case GetTime:{                                                                                  //获取时间
                if(receive_packet.dst==ServerId && client.find(receive_packet.src)!=client.end()){
                    struct packet message = {CONFIRM_NUM, receive_packet.command, ServerId, receive_packet.src, *_GetTime().c_str()};
                    send(client[receive_packet.src]._socket, (void*)serialize(message), PACKET_LENGTH, 0);
                }
                break;
            }
            case GetServerName:{                                                                            //获取主机名
                if(receive_packet.dst==ServerId && client.find(receive_packet.src)!=client.end()){
                    struct packet message = {CONFIRM_NUM, receive_packet.command, ServerId, receive_packet.src, *_GetClientList().c_str()};
                    send(client[receive_packet.src]._socket, (void*)serialize(message), PACKET_LENGTH, 0);
                }
                break;
            }
            case GetClientList:{                                                                            //获取客户端列表
                if(receive_packet.dst==ServerId && client.find(receive_packet.src)!=client.end()){
                    struct packet message = {CONFIRM_NUM, receive_packet.command, ServerId, receive_packet.src, *_GetServerName().c_str()};
                    send(client[receive_packet.src]._socket, (void*)serialize(message), PACKET_LENGTH, 0);
                }
                break;
            }
            case SendMessage:{                                                                              //发送消息(暂时只处理转发)
                if(client.find(receive_packet.src)!=client.end() && client.find(receive_packet.dst)!=client.end()){
                    struct packet message = {CONFIRM_NUM, receive_packet.command, receive_packet.src, receive_packet.dst, *receive_packet.payload};
                    send(client[receive_packet.dst]._socket, (void*)serialize(message), PACKET_LENGTH, 0);
                }
                break;
            }
            case Disconnect:{                                                                               //断开连接
                if(receive_packet.dst==ServerId && client.find(receive_packet.src)!=client.end()){
                    client.erase(receive_packet.src);                                                       //删除client中的键值对
                    _Exit(0);                                                                               //退出子进程
                }
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

std::string _GetTime()
{
    std::ostringstream ss;
    auto now = std::chrono::system_clock::now();
    std::time_t now_c = std::chrono::system_clock::to_time_t(now);
    ss << std::put_time(std::localtime(&now_c), "%Y.%m.%d.%H.%M");
    std::string current_time = ss.str();
    return current_time;
}

std::string _GetClientList()                            //格式为 序号: 1    地址: 127.0.0.1:8080
{
    while(pthread_mutex_trylock(&mutex)){}              //加锁
    std::string ClientList;
    for(int i=0; i<id; i++){
        std::string clientInfo = "序号: " + std::to_string(i) + "\t" + "地址: " + client[i]._addr.sa_data + "\n";
        ClientList += clientInfo;
    }
    pthread_mutex_unlock(&mutex);                       //释放锁
    return ClientList;
}

std::string _GetServerName()
{
    while(pthread_mutex_trylock(&mutex)){}              //加锁
    std::string ServerName(server._name);
    pthread_mutex_unlock(&mutex);                       //释放锁
    return ServerName;
}