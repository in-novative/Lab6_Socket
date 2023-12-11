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
#include<string>
#include<iomanip>
#include <sstream>
#define CONFIRM_NUM 0x114514

struct packet {
    uint32_t confirm_num{CONFIRM_NUM};      //反序列化时确认是否正确
    short command;                //命令类型
    int dest;                   //消息的目标地址(默认为0，消息转发时为目标id)
    int length;                 //payload长度(单位为字节)
    std::string payload;
}; // 自定义的数据包格式

std::string serialize(struct packet message)
{
    std::string string_out;
    std::stringstream ss;
    ss << std::hex << std::setfill('0') << std::setw(sizeof(uint32_t)) << message.confirm_num;
    ss << std::hex << std::setfill('0') << std::setw(sizeof(short)) << message.command;
    ss << std::hex << std::setfill('0') << std::setw(sizeof(int)) << message.dest;
    ss << std::hex << std::setfill('0') << std::setw(sizeof(int)) << message.length;
    string_out = ss.str() + message.payload;
    return string_out;
}
struct packet deserializa(std::string string_in)
{

}
#endif