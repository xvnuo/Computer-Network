#pragma once
#pragma once  
#include<winsock.h>  
#include<thread>
#define BUF_SIZE 1024

struct message
{
	char* data;
	bool* isActive;
	int clientSocket;
	int id;
	message(char* d, bool* is, int c, int i) :data(d), isActive(is), clientSocket(c), id(i) { }
};

struct close_message
{
	bool* isActive;
	int serverSocket;
	close_message(bool* is, int s) : isActive(is), serverSocket(s) { }
};

class webServer
{
private:
	enum {
		SERVER_PORT = 2473,
		BUFFER_SIZE = 13000,
		QUEUE_SIZE = 10,
		MAX = 100
	};
	char buffer[BUFFER_SIZE];
	sockaddr_in serverChannel;
	std::thread* t[MAX];
	std::thread* th;
	char rootDir[50];
	char name[50];
	bool isActive[MAX];
	int serverSocket; //socket  
	int clientSocket;
	friend void handleMSG(message msg);
	friend void listenForClose(close_message msg);
	friend void sendErrorMSG(message msg);
public:
	webServer(){
		WORD wVersionRequested;
		WSADATA wsaData;
		int ret;

		//WinSock初始化：  
		wVersionRequested = MAKEWORD(2, 2);//希望使用的WinSock DLL的版本  
		ret = WSAStartup(wVersionRequested, &wsaData);
		if (ret != 0)
		{
			printf("WSAStartup() failed!\n");
		}
		//确认WinSock DLL支持版本2.2
		if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2)
		{
			WSACleanup();
			printf("Invalid Winsock version!\n");
		}
	}
	bool start();//开启服务器  
};