/*
| 命令号 | 命令 |
|  '1'  |  获取时间  |
|  '2'  |  获取服务器名字  |
|  '3'  |  获取客户端列表  |
|  '4'  |  发送消息  |
|  '5'  |  断开连接  |
|  '6'  |  发生错误  |
*/
#ifndef PACKET_H
#define PACKET_H
#include <string>
#include <iomanip>
#include <sstream>
#include <iostream>
#include <cstring>
#define CONFIRM_NUM (0x114514)                                                                      //数据包确认号
#define PAYLOAD_LENGTH (0x200)                                                                      //payload最大长度
#define PACKET_LENGTH (PAYLOAD_LENGTH+18)                                                           //数据包长度
#define ServerId (0)                                                                                //服务端id号
#define GetTime ('1')                                                                               //获取时间
#define GetServerName ('2')                                                                         //获取服务器名字
#define GetClientList ('3')                                                                         //获取客户端列表
#define SendMessages ('4')                                                                          //发送消息
#define Disconnect ('5')                                                                            //断开连接
#define Err ('6')                                                                                   //发生错误

struct packet {
    uint32_t confirm_num{CONFIRM_NUM};                                                              //反序列化时确认是否正确
    short command;                                                                                  //命令类型
    int src;                                                                                        //消息的源地址
    int dst;                                                                                        //消息的目标地址(服务器为0，消息转发时为目标id)
    char *payload = new char[PAYLOAD_LENGTH];                                                       //消息内容
};                                                                                                  // 自定义的数据包格式

char* serialize(struct packet message)                                                              //数据包序列化函数
{
    char* string_out = new char[PACKET_LENGTH];                                                     //?内存泄露问题，在调用后释放内存
    std::stringstream ss;
    ss << std::hex << std::setfill('0') << std::setw(6) << message.confirm_num;
    ss << std::hex << std::setfill('0') << std::setw(4) << message.command;
    ss << std::hex << std::setfill('0') << std::setw(4) << message.src;
    ss << std::hex << std::setfill('0') << std::setw(4) << message.dst;
    strncpy(string_out, ss.str().c_str(),strlen(ss.str().c_str()));
    strncat(string_out, message.payload, PAYLOAD_LENGTH);
    return string_out;
}

struct packet* deserialize(const char* string_in)                                                   //数据包反序列化函数
{
    char *ss = new char[PACKET_LENGTH];
    struct packet* message = new packet();                                                          //?内存泄露问题，在调用后释放内存
    memcpy(ss, string_in, 6);
    message->confirm_num = static_cast<uint32_t>(std::stoul(ss, nullptr, 16));
    memset(ss, 0, PACKET_LENGTH);
    memcpy(ss, string_in+6, 4);
    message->command = static_cast<short>(std::stoi(ss, nullptr, 16));
    memset(ss, 0, PACKET_LENGTH);
    memcpy(ss, string_in+10, 4);
    message->src = static_cast<int>(std::stoi(ss, nullptr, 16));
    memset(ss, 0, PACKET_LENGTH);
    memcpy(ss, string_in+14, 4);
    message->dst = static_cast<int>(std::stoi(ss, nullptr, 16));
    memset(message->payload, 0, PAYLOAD_LENGTH);
    memcpy(message->payload, string_in+18, PAYLOAD_LENGTH);
    return message;
}
#endif