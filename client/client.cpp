#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <time.h>
#include <pthread.h>
#include <winsock2.h>
#include <windows.h>
#include <string>
#include "packet.h"

#pragma comment(lib, "ws2_32.lib")

#define MAX_LENGTH 1024         //数据包最大长度
using namespace std;

void Connect();
void Close();
void Request(int);              //request information from server
void Send();	                //sending message to other clients
void Exit();
void alterLock(bool);           //change the state whether the request is satisfied

SOCKET c_socket;
pthread_mutex_t mutex;
pthread_t receiver;
bool connectedToServer = false;
bool waitingStatus = false;
const int waitTime = 200;       //200ms=0.2s
const int maxWaitNum = 100;     //max wait time=20s
bool is_exit = false;

int time_reply_count = 0;

int main() {
	WORD wVersionRequested;
	WSADATA wsaData;
	int ret;
	int option; // operation chosen

	// 初始化WinSock
	wVersionRequested = MAKEWORD(2, 2); // expected WinSock DLL version
	ret = WSAStartup(wVersionRequested, &wsaData);
	if (ret != 0) {
		printf("WSAStartup() failed!\n");
		return 0;
	}
	// 检查2.2版本，如果不支持则退出。
	if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2) {
		WSACleanup();
		printf("Invalid Winsock version!\n");
		return 0;
	}

	// 创建socket
	c_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (c_socket == INVALID_SOCKET) {
		WSACleanup();
		printf("socket() failed!\n");
		return 0;
	}

	pthread_mutex_init(&mutex, NULL);
	//is_exit = false;

	// 从用户获得输入
	while (true)
	{
		// 菜单
		cout << "Operation list:" << endl;
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
		if (option < 1 || option > 7 || (!connectedToServer && 2 <= option && option <= 6)) {
			printf("ERROR:\tInvalid operation!\n");
			continue;
		}
		if (option == 1)
			Connect();
		else if (option == 2)
			Close();
		else if (option >= 3 && option <= 5)
			Request(option); // 调用Request获得时间、名字、客户端列表
		else if (option == 6)
			Send();
		else if (option == 7){
			if (connectedToServer){
				Close();							
			}
			break;
		}
	}
	return 0;
}

void alterLock(bool type)
{
	pthread_mutex_lock(&mutex);
	waitingStatus = type;
	pthread_mutex_unlock(&mutex);
}

void *receive(void *args)
{
	SOCKET *c_socket = (SOCKET *)args;
	
	while (true)
	{
		char buffer_recv[MAX_LENGTH] = {0};
		int ret = recv(*c_socket,buffer_recv,MAX_LENGTH,0);
		if (ret == SOCKET_ERROR || ret == 0)
			break;
			
		int t = (int)time(NULL);
		time_t curtime = time(NULL);
    	tm *ptm = localtime(&curtime);

    	char buf[64] = {0};
    	sprintf(buf, "%d/%02d/%02d %02d:%02d:%02d", ptm->tm_year+1900, ptm->tm_mon+1,ptm->tm_mday, ptm->tm_hour, ptm->tm_min, ptm->tm_sec);
		
        char* pstr = buffer_recv;
        if(strcmp(pstr.c_str(),"\n")!=0) {
        	packet pack = deserialize(pstr);
        	
        	if(pack.command >= 4 && pack.command <= 7)
				cout << "[" << buf << "]" << "Recieve a packet." << endl;
		
			// 如果不是一个回应数据包
			if(pack.command < 0 || pack.command > 7) {
				cout << pstr << endl;
			}
			
			switch (pack.command) {
        		case '1': { //时间
            		printf("Current server time: %s\n", pack.payload.c_str());
            		time_reply_count++;
            		printf("time reply count:%d\n",time_reply_count);
					alterLock(false);
           	 		break;
        		}
        		case '2': { //名字
					puts("Client computer name: ");
            		printf("%s\n", pack.payload.c_str());
					alterLock(false);
            		break;
        		}
        		case '3': { //客户端列表
            		puts("Client list:");
					printf("%s\n", pack.payload.c_str());
					alterLock(false);
            		break;
       			}
        		case '4': { //发送消息
        			puts("Message:");
            		printf("%s\n", pack.payload.c_str());
					alterLock(false);
            		break;
        		}
    		}
   	 	}
	}
}

void Connect() {
	if (connectedToServer) {
		printf("Already connected.\n");
		return;
	}

	static char ip[MAX_LENGTH];
	int port;
	cout << "Input IP (default: 127.0.0.1): ";
	cin >> ip;
	fflush(stdin);

	cout << "Input port (default: 3380): ";
	cin >> port;
	fflush(stdin);

	struct sockaddr_in saServer;
	//信息
	saServer.sin_family = AF_INET;
	saServer.sin_port = htons(port); //order of number
	saServer.sin_addr.S_un.S_addr = inet_addr(ip);

	int ret = connect(c_socket, (struct sockaddr *)&saServer, sizeof(saServer));
	if (ret == SOCKET_ERROR) {
		puts("ERROR:\tConnection failed!");
		closesocket(c_socket);
		WSACleanup();
		return;
	}
	else
		puts("SUCCESS:\tConnection Successful!");

	connectedToServer = true;

	// 创建子线程接收数据
	ret = pthread_create(&receiver, NULL, receive, &c_socket);
	if (ret != 0) {
		printf("ERROR:\tCreate pthread failed; Return code: %d\n", ret);
		return;
	}
	puts("");
	
	//连接成功，发送一个packet
	packet pack = {CONFIRM_NUM, 1, ""}; //connect
	string pstr = serialize(pack); //
	send(c_socket, pstr.c_str(), pstr.size(), 0);
}

void Close() {
	//关闭连接，发送一个packet
	packet pack = {CONFIRM_NUM, 5, ""}; //close
	string pstr = serialize(pack); //
	send(c_socket, pstr.c_str(), pstr.size(), 0);
	
	closesocket(c_socket);
	connectedToServer = false;
	pthread_join(receiver, NULL);
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

void Request(int option) {
	// option在3-5之间
	int type = option - 2;
	//option == 3，type = 1;        //time:1
	//option == 4，type = 2;        //name:2
	//option == 5，type = 3;        //client list:110
	
	// 发送所需信息的packet
	packet pack = {CONFIRM_NUM, type, ""}; //request
	string pstr = serialize(pack); //
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
	char* content = buf;

	int server_id = 0; 
	// 给server发送一个packet
	packet pack = packet{CONFIRM_NUM, 4, client_id, server_id, *content}; 
	string pstr = serialize(pack); 
	send(c_socket, pstr.c_str(), pstr.size(), 0);
	//cout << "pstr.c_str()=" << pstr.c_str() << endl;
	
	alterLock(true);	
	for(;;){ //wait for reply until receive a package
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