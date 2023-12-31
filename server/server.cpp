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
std::string _GetClientList(int );                                                                           //返回客户端列表

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
        socklen_t addr_len = sizeof(client_addr);
        int new_socket = accept(server._socket, &client_addr, &addr_len);                               //?server的写操作均发生在创建新线程前，无需加锁
        if(new_socket < 0){                                                                             //申请新句柄失败
            continue;
        }
        else {                                                                                          //申请新句柄成功,有新的客户端连接
            std::cout << "[Info]: successfully connected" << std::endl;
                                                                                                        //!增加一个新客户端的项目，并记录下该客户端句柄和连接状态、端口
            while(pthread_mutex_trylock(&mutex)){}                                                      //加锁
            pthread_t thread;
            id++;
            if(pthread_create(&thread, NULL, receive, (void*)&id)){                                     //子进程创建失败
                id--;
                std::cerr << "[Error]: Thread Creation Failed" << std::endl;
                pthread_mutex_unlock(&mutex);                                                           //释放锁
                continue;
            }
            struct client_info new_client = {client_addr, new_socket, sizeof(new_socket), thread};      //记录新添加客户端的信息
            client.insert(std::make_pair(id, new_client));                                              //添加id和客户端信息的键值对
            pthread_mutex_unlock(&mutex);                                                               //释放锁
            std::cout << "[Info]: client " << id << " create a new thread" << std::endl;
        }
    }
}

bool connect(int socket, uint16_t port){
                                                                                                        //!调用bind()，绑定监听端口
    sockaddr_in _server_addr;
    _server_addr.sin_family = AF_INET;
    _server_addr.sin_port = htons(port);
    _server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    if(bind(socket, (struct sockaddr*)&_server_addr, sizeof(_server_addr)) < 0) {                                               //监听端口绑定失败
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
    char *ss_ptr = new char[PACKET_LENGTH];
                                                                                                            //!调用send()，发送一个hello消息给客户端
    struct packet m = {CONFIRM_NUM,SendMessages,ServerId,client_id};
    strncpy(m.payload, "Successfully Connected", PAYLOAD_LENGTH);
    char *m_ptr = serialize(m);
    send(client[client_id]._socket, (void*)m_ptr, PACKET_LENGTH, 0);                                        //发送连接成功确认，告知客户端id
    delete m_ptr;
                                                                                                            //!循环调用recv()，接收客户端消息
    while(1){
        memset(ss_ptr, 0, PACKET_LENGTH);
        if(recv(client[client_id]._socket, (void*)ss_ptr, PACKET_LENGTH, 0) == -1){                         //未收到数据包
            continue;
        }
        struct packet *ss = deserialize(ss_ptr);
        if(ss == nullptr){
            //std::cerr << "[Error]: Deserialize Failed" << std::endl;
            continue;
        }
        memcpy((char*)&receive_packet, (char*)ss, PACKET_LENGTH);                                           //反序列化接收到的数据包
        delete ss;
        if(receive_packet.confirm_num != CONFIRM_NUM){                                                      //反序列化确认号错误
            //std::cerr << "[Error]: Deserialize Failed" << std::endl;
            continue;
        }
        std::cout << "[Info]: receive a packet from client " << client_id << std::endl;
                                                                                                            //!收到一个完整的包
        switch(receive_packet.command){
            case GetTime:{                                                                                  //获取时间
                std::cout << "[Info]: application for time" << std::endl;
                if(receive_packet.dst==ServerId && client.find(client_id)!=client.end()){
                    struct packet *message = new packet();
                    message->confirm_num = CONFIRM_NUM;
                    message->command = receive_packet.command;
                    message->src = ServerId;
                    message->dst = client_id;
                    std::string Time = _GetTime();
                    strncpy(message->payload, Time.c_str(), Time.length());
                    send(client[client_id]._socket, (void*)serialize(*message), PACKET_LENGTH, 0);
                    delete message;
                }
                break;
            }
            case GetServerName:{                                                                            //获取主机名
                std::cout << "[Info]: application for server name" << std::endl;
                if(receive_packet.dst==ServerId && client.find(client_id)!=client.end()){
                    struct packet *message = new packet();
                    message->confirm_num = CONFIRM_NUM;
                    message->command = receive_packet.command;
                    message->src = ServerId;
                    message->dst = client_id;
                    std::string ServerName = _GetServerName();
                    strncpy(message->payload, ServerName.c_str(), ServerName.length());
                    send(client[client_id]._socket, (void*)serialize(*message), PACKET_LENGTH, 0);
                    delete message;
                }
                break;
            }
            case GetClientList:{                                                                            //获取客户端列表
                std::cout << "[Info]: application for client list" << std::endl;
                if(receive_packet.dst==ServerId && client.find(client_id)!=client.end()){
                    struct packet *message = new packet();
                    message->confirm_num = CONFIRM_NUM;
                    message->command = receive_packet.command;
                    message->src = ServerId;
                    message->dst = client_id;
                    std::string ClientList = _GetClientList(client_id);
                    strncpy(message->payload, ClientList.c_str(), ClientList.length());
                    send(client[client_id]._socket, (void*)serialize(*message), PACKET_LENGTH, 0);
                    int result = send(client[client_id]._socket, (void*)serialize(*message), PACKET_LENGTH, 0);
                    delete message;
                }
                break;
            }
            case SendMessages:{                                                                              //发送消息(暂时只处理转发)
                if(client.find(client_id)!=client.end() && client.find(receive_packet.dst)!=client.end() && client_id!=receive_packet.dst){
                    printf("[Info]: message transportation from %d to %d\n", client_id, receive_packet.dst);
                    struct packet *message1 = new packet(), *message2 = new packet();
                    message1->confirm_num = CONFIRM_NUM;
                    message2->confirm_num = CONFIRM_NUM;
                    message1->command = receive_packet.command;
                    message2->command = receive_packet.command;
                    message1->src = client_id;
                    message2->src = ServerId;
                    message1->dst = receive_packet.dst;
                    message2->dst = client_id;
                    strncpy(message1->payload, receive_packet.payload, strlen(receive_packet.payload));
                    snprintf(message2->payload, PAYLOAD_LENGTH, "successfully sned message to client %d", receive_packet.dst);
                    send(client[receive_packet.dst]._socket, (void*)serialize(*message1), PACKET_LENGTH, 0);
                    send(client[client_id]._socket, (void*)serialize(*message2), PACKET_LENGTH, 0);
                    delete message1;
                    delete message2;
                }
                else{
                    printf("[Info]: cant send message to %d\n",receive_packet.dst);
                    struct packet *message = new packet();
                    message->confirm_num = CONFIRM_NUM;
                    message->command = receive_packet.command;
                    message->src = ServerId;
                    message->dst = client_id;
                    const char error1[] = "[Error] cant send message to yourself";
                    const char error2[] = "[Error] cannot find destination";
                    if(client_id==receive_packet.dst){
                        strncpy(message->payload, error1, PAYLOAD_LENGTH);
                    }
                    else if(client.find(receive_packet.dst)==client.end()){
                        strncpy(message->payload, error2, PAYLOAD_LENGTH);
                    }
                    send(client[client_id]._socket, (void*)serialize(*message), PACKET_LENGTH, 0);
                    delete message;
                }
                break;
            }
            case Disconnect:{                                                                               //断开连接
                std::cout << "[Info]: application for disconnect" << std::endl;
                if(receive_packet.dst==ServerId && client.find(client_id)!=client.end()){
                    struct packet *message = new packet();
                    message->confirm_num = CONFIRM_NUM;
                    message->command = receive_packet.command;
                    message->src = ServerId;
                    message->dst = client_id;
                    strncpy(message->payload, "Successfully Disconnect", PAYLOAD_LENGTH);
                    send(client[client_id]._socket, (void*)serialize(*message), PACKET_LENGTH, 0);
                    close(client[client_id]._socket);                                                //关闭客户端套接字
                    client.erase(client_id);                                                       //删除client中的键值对
                    delete message;
                    delete[] ss_ptr;
                    //pthread_exit(NULL);                                                                              //退出子进程
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
    ss << std::put_time(std::localtime(&now_c), "%Y.%m.%d %H:%M");
    std::string current_time = ss.str();
    std::cout << "[Info]: current time is " << current_time << std::endl;
    return current_time;
}

std::string _GetClientList(int my_id)
{
    while(pthread_mutex_trylock(&mutex)){}              //加锁
    std::ostringstream ss;
    for(int i=1; i<=id; i++){
        if(client.find(i)==client.end()) 
            continue;      //跳过已经断开连接的客户端
        std::string clientInfo = "\nid: " + std::to_string(i);
        if(i == my_id){
            clientInfo += " (yourself)";
        }
        ss << clientInfo;
    }
    std::string ClientList = ss.str();
    pthread_mutex_unlock(&mutex);                       //释放锁
    std::cout << "[Info]: client list is" << ClientList << std::endl;
    return ClientList;
}

std::string _GetServerName()
{
    while(pthread_mutex_trylock(&mutex)){}              //加锁
    std::string ServerName(server._name);
    pthread_mutex_unlock(&mutex);                       //释放锁
    std::cout << "[Info]: server name is " << ServerName << std::endl;
    return ServerName;
}