#undef UNICODE
#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN
#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdlib.h>
#include <stdio.h>
#include <thread>
#include <iostream>
#include <streambuf> 
#include <fstream>
#include <string>
#include <thread>
#include <map>
#include <vector>
#include <mutex>
#include <ctime>
#include <ratio>
#include <chrono>

using namespace std;
// socket编程需要配置的库
#pragma comment (lib, "Ws2_32.lib")
#pragma comment (lib, "Mswsock.lib")
#pragma comment (lib, "AdvApi32.lib")

#define SERVER_NAME "2473-13107FF"
#define DEFAULT_PORT 4579
#define DEFAULT_BUFLEN 512

struct Client {
	char* IP;
	int port;
	int id;
	thread::id thread_id;
	SOCKET socket;
};
vector<struct Client*> client_list;
int client_num=0;
std::mutex thread_lock;

//将string转换为char*类型后，调用socket的send函数
void SendString(SOCKET s, string str);
//处理客户端发来的时间请求
void GetTime(SOCKET s);
//处理客户端发来的名字请求
void GetServerName(SOCKET s);
//处理客户端发来的列表请求
void GetClientList(SOCKET s);
//判断一个客户端是否存在于列表中
bool isAlive(int id);
//搜寻列表中客户端对应的套接字
SOCKET SearchSocket(int id);
//解析客户端发来的请求数据包
int ProcessRequestPacket(SOCKET s, char* rec_packet);
//每个客户端对应的子线程处理函数
void ClientThread(SOCKET sClient);

int main() {
	
	WORD wVersionRequested;
	WSADATA wsaData;
	int ret, nLeft, length;
	SOCKET sListen, sServer; //侦听套接字，连接套接字
	struct sockaddr_in saServer;//地址信息
	SOCKET sClient;
	char ch;

	printf("Welcome! Here is a server created by 3170102473 & 3170104579\n");
	printf("Server initilizing...\n");

	//WinSock初始化：
	wVersionRequested = MAKEWORD(2, 2);//希望使用的WinSock DLL的版本
	ret = WSAStartup(wVersionRequested, &wsaData);
	if (ret != 0) {
		printf("WSAStartup() failed!\n");
		ch=getchar();
		return 0;
	}
	//确认WinSock DLL支持版本2.2：
	if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2)	{
		WSACleanup();
		printf("Invalid Winsock version!\n");
		ch = getchar();
		return 0;
	}
	//创建socket，使用TCP协议：
	sListen = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sListen == INVALID_SOCKET)
	{
		WSACleanup();
		printf("socket() failed!\n");
		ch = getchar();
		return 0;
	}
	//构建本地地址信息：
	saServer.sin_family = AF_INET;//地址家族
	saServer.sin_port = htons(DEFAULT_PORT);//注意转化为网络字节序
	saServer.sin_addr.S_un.S_addr = htonl(INADDR_ANY);//使用INADDR_ANY指示任意地址
	//绑定：
	ret = bind(sListen, (struct sockaddr*) & saServer, sizeof(saServer));
	if (ret == SOCKET_ERROR){
		printf("bind() failed! code:%d\n", WSAGetLastError());
		closesocket(sListen);//关闭套接字
		WSACleanup();
		ch = getchar();
		return 0;
	}
	//侦听连接请求,设置连接等待队伍长度为5
	ret = listen(sListen, 5);
	if (ret == SOCKET_ERROR){
		printf("listen() failed! code:%d\n", WSAGetLastError());
		closesocket(sListen);//关闭套接字
		WSACleanup();
		ch = getchar();
		return 0;
	}
	printf("Success! Waiting for clients...\n");
    //循环调用accept来等待客户端
	for (;;) {
		sClient = accept(sListen, NULL, NULL);
		if (sClient == INVALID_SOCKET) { // accept出错
			printf("accept() failed! code:%d\n", WSAGetLastError());
			closesocket(sClient);
			WSACleanup();
			ch = getchar();
			return 0;
		}
		//创建一个新的子线程，并以引用的方式传入socket
		thread(ClientThread, ref(sClient)).detach();
	}
	//关闭socket
	closesocket(sListen);
	WSACleanup();
	return 0;
}

// 传入string类型的数据包，通过SOCKET s发送出去
void SendString(SOCKET s, string str) {
	char content[DEFAULT_BUFLEN];
	strcpy(content, str.c_str());
	int len = str.length();
	send(s, content, len, 0);
}

// 客户端子线程处理函数
void ClientThread(SOCKET sClient) {
	char rec_packet[DEFAULT_BUFLEN]="\0"; 
	char* IP = NULL;
	int port;

	SOCKADDR_IN client_info = { 0 };
	int addrsize = sizeof(client_info);
	// 获取线程编号
	thread::id thread_id = this_thread::get_id();
	// 获取客户端的IP与端口
	getpeername(sClient, (struct sockaddr*) & client_info, &addrsize);
	IP = inet_ntoa(client_info.sin_addr);
	port = client_info.sin_port;
	printf("Connected to client: %s\n", IP);
	//创建一个新的Client数据
	struct Client c;
	c.socket = sClient; c.IP = IP; c.id = client_num++;
	c.port = port; c.thread_id = thread_id;
	struct Client* this_client = &c;
	//将这个Client加入现有的列表，在写入数据前加锁
	thread_lock.lock();
	client_list.push_back(this_client);
	thread_lock.unlock(); //写完数据后解锁
	
	//循环调用recv()等待客户端的请求数据包
	int ret = 0;
	bool is_close = false;
	while (!is_close) {
		ZeroMemory(rec_packet, DEFAULT_BUFLEN);
		ret = recv(sClient, rec_packet, DEFAULT_BUFLEN, 0);
		if (ret > 0) { //正常
			printf("Packet received! Processing...\n");
			//处理T,N,L类型请求，即请求时间、服务器姓名、客户端列表
			int flag = ProcessRequestPacket(sClient, rec_packet);
			if (flag==0) { //处理M类型请求，即发送信息
				// 向指定客户端发送信息
				string str = rec_packet;  
				int pos1, pos2;
				pos1 = str.find("*") + 1;
				pos2 = str.find("*", pos1);
				int to_id = stoi(str.substr(pos1, pos2 - pos1)); // 获取目的地ID
				pos1 = pos2 + 1;
				pos2 = str.find("#", pos1);
				string message = str.substr(pos1, pos2 - pos1); // 获取消息内容
				if (isAlive(to_id)) {
					SOCKET sDestination = SearchSocket(to_id); // 获取目标客户端的socket
					//封装指示数据包，转发信息
					string response = "#M-M*";
					response += to_string(this_client->id);
					response += " ";
					response += this_client->IP;
					response += "\n";
					response += message;
					response += "#\0";
					SendString(sDestination, response);
					printf("Send a message to destination client!\n");
					//发送响应数据包，表示发送成功
					response = "#M-Y*";
					response += to_string(to_id);
					response += " ";
					response += message;
					response += "#\0";
					SendString(sClient, response);
					printf("Send a message to start client!\n");
				}
				else {
					// 发送响应数据包，表示发送失败
					string response = "#M-N*Destination doesn't exist!#";
					SendString(sClient, response);
					printf("Send message failed!\n");
				}
			}
		}
		else if (ret == 0) { //连接中断
			printf("Connection closed!\n");
			is_close = true;
		}
		else { //运行出错
			printf("recv() failed! code:%d\n", WSAGetLastError());
			closesocket(sClient);
			WSACleanup();
			return;
		}
	}
	//停止与该客户端的连接
	closesocket(sClient);
	vector<struct Client*>::iterator it;
	for (it = client_list.begin(); it != client_list.end(); it++) {
		if ((*it)->id == this_client->id && (*it)->IP == this_client->IP) {
			break;
		}
	}
	//在列表中删去该客户端
	thread_lock.lock();
	client_list.erase(it);
	client_num--;
	//delete this_client;
	thread_lock.unlock();
	return;
}

int ProcessRequestPacket(SOCKET s, char* rec_packet){
	if (rec_packet[0] != '#') {
		printf("It's not a packet!\n");
		return -1;
	}
	char packet_type = rec_packet[1];
	switch (packet_type) {
	case 'T':
		GetTime(s);
		break;
	case 'N':
		GetServerName(s);
		break;
	case 'L':
		GetClientList(s);
		break;
	default:
		return 0;
		break;
	}
	return 1;
}

void GetTime(SOCKET s) {
	//for (int i = 0; i < 100; i++) {
		string response = "#T*";
		using std::chrono::system_clock;
		system_clock::time_point today = system_clock::now();
		std::time_t tt;
		tt = system_clock::to_time_t(today);
		response += ctime(&tt);
		response += "#\0";
		SendString(s, response);
	//	Sleep(500);
//	}	
	printf("Time send.\n");
}

void GetServerName(SOCKET s) {
	string response = "#N*";
	response += SERVER_NAME;
	response += "#\0";
	SendString(s, response);
	printf("Server name send.\n");
}

void GetClientList(SOCKET s) {
	string response = "#L*";
	thread_lock.lock();
	vector<struct Client*>::iterator it;
	for (it = client_list.begin(); it != client_list.end(); it++) {
		response += to_string((*it)->id);
		response += ' ';
		response += (*it)->IP;
		response += '\n';
	}
	response += "#\0";
	thread_lock.unlock();
	SendString(s, response);
	printf("Client list send.\n");
}

bool isAlive(int id) {
	bool flag = false;
	thread_lock.lock();
	vector<struct Client*>::iterator it;
	for (it = client_list.begin(); it != client_list.end(); it++) {
		if ((*it)->id == id) {
			flag = true;
			break;
		}
	}
	thread_lock.unlock();
	return flag;
}

SOCKET SearchSocket(int id) {
	SOCKET socket;
	thread_lock.lock();
	vector<struct Client*>::iterator it;
	for (it = client_list.begin(); it != client_list.end(); it++) {
		if ((*it)->id == id) {
			socket = (*it)->socket;
			break;
		}
	}
	thread_lock.unlock();
	return socket;
}