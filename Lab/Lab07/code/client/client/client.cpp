#define WIN32_LEAN_AND_MEAN
#define _CRT_SECURE_NO_WARNINGS
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
#include <map>
#include <mutex>
#include <ctime>
#include <ratio>
#include <chrono>
using namespace std;
//socket编程需要配置的库
#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "Mswsock.lib")
#pragma comment(lib, "AdvApi32.lib")

#define DEFAULT_BUFLEN 512
#define DEFAULT_PORT "4579"

mutex mtx_time;
mutex mtx_name;
mutex mtx_list;
mutex mtx_message;
mutex mtx_output;
condition_variable Time;
condition_variable name;
condition_variable list;
condition_variable message;

//菜单列表
void helpinfo();
//客户端接收信息后输出信息
void output(string response_content);
//客户端接收信息
int receivePacket(SOCKET connect_socket);
//客户端请求与服务端断开连接
void closeConnect(SOCKET connect_socket);
//客户端请求获得时间
void getTime(char* send_buf, SOCKET connect_socket);
//客户端请求获得服务器名字
void getName(char* send_buf, SOCKET connect_socket);
//客户端请求获得客户端列表
void getList(char* send_buf, SOCKET connect_socket);
//客户端请求发送信息
void sendMessage(char* send_buf, SOCKET connect_socket);
//客户端请求退出
void dropout(SOCKET connect_socket);

int main()
{
	WORD wVersionRequested;
	WSADATA wsaData;
	SOCKET connect_socket = INVALID_SOCKET;
	struct addrinfo* result = NULL;
	struct addrinfo* ptr = NULL;
	struct addrinfo hints;
	char send_buf[DEFAULT_BUFLEN]; //要发送的信息
	int ret;
	char ch;
	int recv_buflen = DEFAULT_BUFLEN; //收到的信息
	char ip[DEFAULT_BUFLEN];
	ZeroMemory(send_buf, DEFAULT_BUFLEN);
	while (1)
	{

		//开始界面
		helpinfo();

		int c;
		cin >> c;
		if (c == 2 || c == 7)
		{ //退出
			return 0;
		}
		else if (c == 1)
		{ //连接服务器
			printf("please input the server ip:\n");
			cin >> ip;
			if (ip)
				cout << "usage: " << ip << " server-name" << endl;

			//Winsock初始化
			wVersionRequested = MAKEWORD(2, 2); //希望使用的WinSock DLL的版本
			ret = WSAStartup(wVersionRequested, &wsaData);
			if (ret != 0)
			{
				cout << "WSAStartup failed with error: " << to_string(ret) << endl;
				system("pause");
				continue;
			}

			//确认WinSock DLL支持版本2.2：
			if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2)
			{
				WSACleanup();
				printf("Invalid Winsock version!\n");
				ch = getchar();
				return 0;
			}

			//构建本地地址信息
			ZeroMemory(&hints, sizeof(hints));
			hints.ai_family = AF_UNSPEC;     //地址家族
			hints.ai_socktype = SOCK_STREAM; //socket类型
			hints.ai_protocol = IPPROTO_TCP; //protocol

			//解析服务器端口和地址
			ret = getaddrinfo(ip, DEFAULT_PORT, &hints, &result);
			if (ret != 0)
			{
				cout << "getaddrinfo failed with error: " << to_string(ret) << endl;
				WSACleanup();
				continue;
			}
			int flag = 0;
			for (ptr = result; ptr != NULL; ptr = ptr->ai_next)
			{
				//创建用于连接服务器的SOCKET，使用TCP协议
				connect_socket = socket(ptr->ai_family, ptr->ai_socktype,
					ptr->ai_protocol);
				if (connect_socket == INVALID_SOCKET)
				{
					cout << "socket failed with error: " << to_string(WSAGetLastError()) << endl;
					WSACleanup();
					flag = 1;
					break;
				}

				//连接到服务器
				ret = connect(connect_socket, ptr->ai_addr, (int)ptr->ai_addrlen);
				if (ret == SOCKET_ERROR)
				{
					closesocket(connect_socket);
					connect_socket = INVALID_SOCKET;
					continue;
				}
				break;
			}
			if (flag == 1)
			{
				continue;
			}
			else
			{
				freeaddrinfo(result);
				if (connect_socket == INVALID_SOCKET)
				{
					cout << "Unable to connect to server!" << endl;
					WSACleanup();
					continue;
				}
				else
				{ //连接成功
					cout << "Connection succeed!" << endl;
					char* IP = NULL;
					int port = 0;

					//获得客户端信息
					SOCKADDR_IN clientInfo = { 0 };
					int addr_size = sizeof(clientInfo);
					thread::id this_id = this_thread::get_id();

					//获得现在的ip和port
					getpeername(connect_socket, (struct sockaddr*) & clientInfo, &addr_size);
					IP = inet_ntoa(clientInfo.sin_addr);
					port = clientInfo.sin_port;
					cout << "current IP: " << IP << endl;

					thread(receivePacket, move(connect_socket)).detach(); //接收数据

					while (1)
					{
						helpinfo();
						int a;
						cin >> a;
						if (a == 2)
						{ //请求结束连接
							closeConnect(connect_socket);
							break;
						}
						else if (a == 3)
						{ //请求获得时间
							getTime(send_buf, connect_socket);
							continue;
						}
						else if (a == 4)
						{ //请求获得服务器名字
							getName(send_buf, connect_socket);
							continue;
						}
						else if (a == 5)
						{ //请求获得客户端列表
							getList(send_buf, connect_socket);
							continue;
						}
						else if (a == 6)
						{ //请求向其它客户端发送信息
							sendMessage(send_buf, connect_socket);
							continue;
						}
						else if (a == 7)
						{//请求退出
							dropout(connect_socket);
							return 0;
						}
						else
						{
							continue;
						}
					}
					continue;
				}
			}
		}

		else
		{ //如果在尚未连接服务端时就企图发送请求
			cout << "You haven't connect to a server!" << endl;
			continue;
		}
	}
}

void helpinfo()
{
	puts("Please input the operation number:\n"
		"+-------+--------------------------------------+\n"
		"| Input |              Function                |\n"
		"+-------+--------------------------------------+\n"
		"|   1   |  Connect to a server.                |\n"
		"|   2   |  Close the connect.                  |\n"
		"|   3   |  Get the server time.                |\n"
		"|   4   |  Get the server name.                |\n"
		"|   5   |  Get the client list.                |\n"
		"|   6   |  Send a message.                     |\n"
		"|   7   |  Exit.                               |\n"
		"+-------+--------------------------------------+\n");
}

void output(string response_content)
{
	mtx_output.lock(); //对互斥量加锁
	cout << response_content << endl;
	mtx_output.unlock(); //对互斥量解锁
}

int receivePacket(SOCKET connect_socket)
{
	char recv_buf[DEFAULT_BUFLEN];
	int result;
	int recv_buflen = DEFAULT_BUFLEN;
	do
	{
		ZeroMemory(recv_buf, DEFAULT_BUFLEN);
		result = recv(connect_socket, recv_buf, recv_buflen, 0); //接收数据，接收到的数据包存在recv_buf中
		if (result > 0)                                          //接收成功，返回接收到的数据长度
		{
			string recv_packet = recv_buf; //接收到的数据包
			char packet_type;              //接收到的数据包类型
			string response_content;       //接收到的数据包内容

			int pos1 = 0;
			int pos2 = 0;

			//接收到的数据包类型
			pos1 = recv_packet.find("#") + 1; //接收到的数据包类型的位置（start）
			packet_type = recv_packet[pos1];  //接收到的数据包类型

			//接收到的数据包内容
			pos1 = recv_packet.find("*") + 1;   //找到服务器回答的位置（start）
			pos2 = recv_packet.find("#", pos1); //从pos1所在位置开始往后找，找到空格为止
			if ((pos2 - pos1) == 0)             //内容为空
				response_content = "";
			else //内容不为空
				response_content = recv_packet.substr(pos1, pos2 - pos1);

			switch (packet_type)
			{
			case 'T': //数据包类型为时间
				output(response_content);
				Time.notify_one();
				break;
			case 'N': //数据包类型为名字
				output(response_content);
				;
				name.notify_one();
				break;
			case 'L': //数据包类型为客户端列表
				output(response_content);
				list.notify_one();
				break;
			case 'M': //数据包类型为信息
				pos1 = recv_packet.find("-") + 1;
				char type = recv_packet[pos1];
				switch (type)
				{
				case 'Y': //表示发送消息成功
					//接收信息的一方的编号
					int request_ID;
					pos1 = recv_packet.find("*") + 1;
					pos2 = recv_packet.find(" ", pos1);
					request_ID = stoi(recv_packet.substr(pos1, pos2 - pos1)); //将字符串转化为数字
					response_content = "the message has been sent to ";
					response_content += to_string(request_ID);
					output(response_content);
					message.notify_one();
					break;
				case 'N': //表示发送消息失败
					output(response_content);
					message.notify_one();
					break;
				case 'M': //表示收到信息
					//发送信息的客户端ID
					int from_ID;
					pos1 = recv_packet.find("*") + 1;
					pos2 = recv_packet.find(" ", pos1);
					from_ID = stoi(recv_packet.substr(pos1, pos2 - pos1));

					//发送信息的客户端IP
					string from_IP;
					pos1 = pos2 + 1;
					pos2 = recv_packet.find("\n");
					from_IP = recv_packet.substr(pos1, pos2 - pos1);

					//收到的信息
					string receive_message;
					pos1 = pos2 + 1;
					pos2 = recv_packet.find("#", pos1);
					receive_message = recv_packet.substr(pos1, pos2 - pos1);

					mtx_output.lock();
					cout << "Message from " << to_string(from_ID) << " " << from_IP << endl;
					cout << receive_message << endl;
					mtx_output.unlock();
					break;
				}
			}
		}
		else if (result == 0) //连接结束
			printf("Connection closed\n");
		else //连接失败
			printf("recv failed with error: %d\n", WSAGetLastError());

	} while (result > 0);
	return 0;
}

void closeConnect(SOCKET connect_socket)
{
	int ret;
	ret = shutdown(connect_socket, SD_SEND);
	if (ret == SOCKET_ERROR)
	{
		cout << "shutdown failed with error: " << to_string(WSAGetLastError()) << endl;
		closesocket(connect_socket);
		WSACleanup();
	}
}

void getTime(char* send_buf, SOCKET connect_socket)
{
	int ret;
	string packet = "#T#";
	memcpy(send_buf, packet.c_str(), packet.size());
	//for (int i = 1; i <= 100; i++) {
		ret = send(connect_socket, send_buf, (int)strlen(send_buf), 0);
		unique_lock<mutex> lck(mtx_time);
		Time.wait(lck);
	//}
	
}

void getName(char* send_buf, SOCKET connect_socket)
{
	int ret;
	string packet = "#N#";
	memcpy(send_buf, packet.c_str(), packet.size());
	ret = send(connect_socket, send_buf, (int)strlen(send_buf), 0);
	unique_lock<mutex> lck(mtx_name);
	name.wait(lck);
}

void getList(char* send_buf, SOCKET connect_socket)
{
	int ret;
	string packet = "#L#";
	memcpy(send_buf, packet.c_str(), packet.size());
	ret = send(connect_socket, send_buf, (int)strlen(send_buf), 0);
	unique_lock<mutex> lck(mtx_list);
	list.wait(lck);
}

void sendMessage(char* send_buf, SOCKET connect_socket)
{
	string sendMessage;
	char buffer[1024] = "\0";
	int dst_ID = 0;
	int ret;

	//客户端的ID
	cout << "please input the destination id:" << endl;
	cin >> dst_ID;
	cin.getline(buffer, 1024);

	//要发送的消息内容
	cout << "please input the message:" << endl;
	cin.getline(buffer, 1024);
	sendMessage = buffer;

	//发送的数据包
	string packet = "#M*";
	packet += to_string(dst_ID);
	packet += "*";
	packet += sendMessage;
	packet += "#";
	ZeroMemory(send_buf, DEFAULT_BUFLEN);
	memcpy(send_buf, packet.c_str(), packet.size());
	ret = send(connect_socket, send_buf, (int)strlen(send_buf), 0);
	unique_lock<mutex> lck(mtx_message);
	message.wait(lck);
}

void dropout(SOCKET connect_socket)
{
	int ret;
	ret = shutdown(connect_socket, SD_SEND);
	if (ret == SOCKET_ERROR)
	{
		cout << "shutdown failed with error: " << to_string(WSAGetLastError()) << endl;
		closesocket(connect_socket);
		WSACleanup();
	}
}