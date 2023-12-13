//编译时加上参数-lwsock32
#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <time.h>
#include <pthread.h>
#include <winsock2.h>
#include <windows.h>
#include <string>
#include <cstdint>
#include "../packet.h"

#pragma comment(lib, "ws2_32.lib")

#define MAX_LENGTH 1024         //数据包最大长度
#define ClientId (-1)
using namespace std;

void Connect();
void Close();
void Request(short);              	//request information from server
void Send();	                	//sending message to other clients
void Exit();
void alterLock(bool);           	//change the state whether the request is satisfied

SOCKET c_socket;
pthread_mutex_t mutex;  			// 线程锁 
pthread_t receiver;					// 定义receiver子线程 
bool connectedToServer = false;     // 是否连接到server 
bool waitingStatus = false;			// 当前是否处于等待的状态 
const int waitTime = 200;       	//200ms=0.2s
const int maxWaitNum = 100;     	//max wait time=20s
bool is_exit = false;               // 是否exit 
bool is_get_clist = false;			// 是否已经获得client的list 
int time_reply_count = 0;
char inipayload[MAX_LENGTH] = "";   // 初始的payload的内容 


int main() {
	WORD wVersionRequested;			// typedef short WORD 
	WSADATA wsaData;				// 包含有关 Windows 套接字实现的信息
	int ret;
	int option; 					// 用户选择 

	
	wVersionRequested = MAKEWORD(2, 2); 				// 期望的 WinSock DLL version
	ret = WSAStartup(wVersionRequested, &wsaData);		// 初始化 WinSock
	if (ret != 0) {
		printf("WSAStartup() failed!\n");
		return 0;
	}
	// 检查2.2版本，如果不支持则退出
	if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2) {
		WSACleanup();									// 关闭一切socket 
		printf("Invalid Winsock version!\n");
		return 0;
	}

	// 创建socket
	c_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (c_socket == INVALID_SOCKET) {
		WSACleanup();									// 关闭一切socket 
		printf("socket() failed!\n");
		return 0;
	}

	pthread_mutex_init(&mutex, NULL);					// 初始化线程锁 
	//is_exit = false;

	// 从用户获得输入
	while (true)
	{
		// 菜单
		cout << "********************************" << endl;
		cout << "There are Operation Lists:" << endl;
		cout << "1)\tConnect" << endl;
		if (connectedToServer) {
			cout << "2)\tClose" << endl;
			cout << "3)\tRequest time" << endl;
			cout << "4)\tRequest name" << endl;
			cout << "5)\tRequest client list" << endl;
			cout << "6)\tSend message" << endl;
		}
		cout << "7)\tExit" << endl;
		cout << ">>";

		scanf("%d",&option);
		fflush(stdin);
		// 如果option的数值超出范围，或者在未连接server的情况下发送请求，则报错 
		if (option < 1 || option > 7 || (!connectedToServer && 2 <= option && option <= 6)) {
			printf("ERROR:\tInvalid operation!\n");
			continue;
		}
		// 否则正常执行 
		if (option == 1)
			Connect();
		else if (option == 2)
			Close();
		else if (option >= 3 && option <= 5){
			if(option == 5)
				is_get_clist = true;
			Request(option); // 调用Request获得时间、名字、客户端列表			
		}
		else if (option == 6){
			if(is_get_clist)
				Send();
			else
				printf("You shoule first get the client list before sending any message!\n"); 
		}
		else if (option == 7){
			if (connectedToServer){
				Close();							
			}
			break;
		}
	}
	return 0;
}

// 用于自动更新waitingStatus的值
void alterLock(bool type) {
	pthread_mutex_lock(&mutex);			// 获取与互斥变量相关联的锁
	waitingStatus = type;				// 对waitingStatus赋值 
	pthread_mutex_unlock(&mutex);		// 释放了对互斥变量的锁
}


void *receive(void *args) {
	SOCKET *c_socket = (SOCKET *)args;
	
	// 通过循环不断接收从客户端传来的数据，并对接收到的数据进行处理
	while (true)
	{
		char buffer_recv[MAX_LENGTH] = {0};
		int ret = recv(*c_socket,buffer_recv,MAX_LENGTH,0);   // 通过 recv 函数接收数据
		if (ret == SOCKET_ERROR || ret == 0)				  // 如果出错或是接收完毕就退出 
			break;
			
		int t = (int)time(NULL);							  // 获取当前时间
		time_t curtime = time(NULL);						  
    	tm *ptm = localtime(&curtime);

    	char buf[64] = {0};
    	sprintf(buf, "%d/%02d/%02d %02d:%02d:%02d", ptm->tm_year+1900, ptm->tm_mon+1,ptm->tm_mday, ptm->tm_hour, ptm->tm_min, ptm->tm_sec);
		
		// 将接收到的字符串转换为 packet 对象
        string pstr = buffer_recv;
        if(strcmp(pstr.c_str(),"\n")!=0) {
        	const char* p = pstr.data();
        	packet* pack = deserialize(p);
        	
        	// 输出在当前时间收到一个packet 
        	if(pack->command >= '1' && pack->command <= '4')
				cout << "[" << buf << "]" << "Recieve a packet." << endl;
		
			// 如果不是一个请求数据包
			if(pack->command < '1' || pack->command > '4') {
				cout << pstr << endl;
			}
			
			switch (pack->command) {
        		case '1': { //时间
            		printf("Current server time: %s\n", pack->payload);
            		time_reply_count++;
            		printf("time reply count:%d\n",time_reply_count);
					alterLock(false);
           	 		break;
        		}
        		case '2': { //名字
					puts("Client computer name: ");
            		printf("%s\n", pack->payload);
					alterLock(false);
            		break;
        		}
        		case '3': { //客户端列表
            		puts("Client list:");
					printf("%s\n", pack->payload);
					alterLock(false);
            		break;
       			}
        		case '4': { //发送消息
        			puts("Message:");
            		printf("%s\n", pack->payload);
					alterLock(false);
            		break;
        		}
    		}
   	 	}
	}
}

void Connect() {
	// 先判断是否已经建立连接 
	if (connectedToServer) {
		printf("Already connected.\n");
		return;
	}

	// 从用户获得想要连接的ip地址和端口号 
	static char ip[MAX_LENGTH];
	int port;
	cout << "Input IP (default: 127.0.0.1): ";
	cin >> ip;
	fflush(stdin);

	cout << "Input port (default: 3380): ";
	cin >> port;
	fflush(stdin);

	// 初始化服务器端的地址
	struct sockaddr_in saServer;
	saServer.sin_family = AF_INET;
	saServer.sin_port = htons(port); 
	saServer.sin_addr.S_un.S_addr = inet_addr(ip);
	printf("%s\n", ((struct sockaddr*)&saServer)->sa_data);

	// 判断能否建立连接 
	int ret = connect(c_socket, (struct sockaddr *)&saServer, sizeof(saServer));
	if (ret == SOCKET_ERROR) {
		puts("ERROR:\tConnection failed!");
		closesocket(c_socket);  // 关闭现有套接字
		WSACleanup();           // 终止所有线程的 Windows 套接字操作
		return;
	}
	else
		puts("SUCCESS:\tConnection Successful!");

	// 设置flag为true 
	connectedToServer = true;

	// 创建子线程接收数据
	ret = pthread_create(&receiver, NULL, receive, &c_socket);
	if (ret != 0) {
		printf("ERROR:\tCreate pthread failed; Return code: %d\n", ret);
		return;
	}
	else 
		puts("SUCCESS:\tCreate pthread Successful!");
	
	//连接成功，发送一个packet
	//packet* pack = {CONFIRM_NUM, 1, 0, 0, ""}; //connect
	//string pstr = serialize(pack); //
	//send(c_socket, pstr.c_str(), pstr.size(), 0);
}

void Close() {
	//关闭连接，发送一个packet
	/*packet pack = {CONFIRM_NUM, 5, ""}; //close
	string pstr = serialize(pack); //
	send(c_socket, pstr.c_str(), pstr.size(), 0);*/
	
	// 关闭socket并修改flag，等待子线程的结束 
	closesocket(c_socket);
	connectedToServer = false;
	pthread_join(receiver, NULL);
	// 判断是否需要重新创建 socket 连接
	if (!is_exit) {
		c_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (c_socket == INVALID_SOCKET) {
			WSACleanup();
			printf("ERROR: Require Socket failed.\n");
			exit(0);
		}
	}
	printf("Close successfully\n");
}

void Request(short option) {
	// option在3-5之间
	short type = option - 2;
	//option == 3，type = 1;        //time:1
	//option == 4，type = 2;        //name:2
	//option == 5，type = 3;        //client list:110
	
	// 创建发送所需信息的packet
	struct packet pack;
	//pack.confirm_num = CONFIRM_NUM; // Set confirm_num member
	pack.command = type;
	pack.src = ServerId;
	pack.dst = ClientId;
	strcpy(pack.payload, ""); 

	string pstr = serialize(pack); // string类和char* 的区别？ 
	send(c_socket, pstr.c_str(), pstr.size(), 0);

	alterLock(true);	
	for(;;) { // 等待回复，直到收到packet
		pthread_mutex_lock(&mutex);
		if (!waitingStatus) {
			pthread_mutex_unlock(&mutex);
			break;
		}
		pthread_mutex_unlock(&mutex);
		Sleep(waitTime);
	}
	
}

void Send() {
	// 读取客户端发送到的消息
	int client_id;
	puts("Input client id, sent to:");
	scanf("%d", &client_id);
	fflush(stdin);

	// 读入message
	puts("Input message (end with '#'):");
	char buf[MAX_LENGTH] = {0};
	scanf("%[^#]",buf); //end with '#'
	fflush(stdin);
	const char* content = buf;

	int server_id = 0; 
	// 给server发送一个packet 
	struct packet pack;
	//pack.confirm_num = CONFIRM_NUM; 
	pack.command = 4;
	pack.src = client_id;
	pack.dst = ServerId;
	strcpy(pack.payload, content);
	
	string pstr = serialize(pack);  // string类和char* 的区别？ 
	send(c_socket, pstr.c_str(), pstr.size(), 0);
	
	alterLock(true);	
	for(;;){ //等待回应直到receive收到一个packet 
		pthread_mutex_lock(&mutex);
		if (!waitingStatus) {
			pthread_mutex_unlock(&mutex);
			break;
		}
		pthread_mutex_unlock(&mutex);
		Sleep(waitTime);
	}
}

void Exit()
{
	if (connectedToServer)
		Close();
	//is_exit = true;
	exit(0);
}