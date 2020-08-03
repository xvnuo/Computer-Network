#define _CRT_SECURE_NO_WARNINGS  

#include"webServer.h"  
#include<cstdio>  
#include<cstdlib>  
#include<fstream>  
#include<cstring>  
#include<string>
#include<windows.h>
#include<iostream>


void sendErrorMSG(message msg)
{
	char protocol[] = "HTTP/1.0 400 Bad Request \r\n"; //协议
	char servName[] = "Server:my web server\r\n";
	char cntLen[] = "Content-length:2048\r\n";
	char cntType[] = "Content-type:text\html\r\n\r\n";
	char content[] = "<html><head><title>MY-WEB</title></head><font size=+5><br>find ERROR!</font></html>";

	send(msg.clientSocket, protocol, strlen(protocol), 0);
	send(msg.clientSocket, servName, strlen(servName), 0);
	send(msg.clientSocket, cntLen, strlen(cntLen), 0);
	send(msg.clientSocket, cntType, strlen(cntType), 0);
	send(msg.clientSocket, content, strlen(content), 0);
	closesocket(msg.clientSocket);

}

// 浏览器端发送信息
void sendMessage(std::string path, message msg)
{

	FILE* sendFile;

	int sp;
	if ((sendFile = fopen(path.c_str(), "rb")) == NULL)
	{
		sendErrorMSG(msg);
		return;
	}

	else {
		strcpy(msg.data, "HTTP/1.1 200 OK\n");
		strcat(msg.data, "Content-Type: text/html;charset=ISO-8859-1\nContent-Length: ");
		fseek(sendFile, 0, SEEK_END);
		int flen = ftell(sendFile); // 文件长度
		rewind(sendFile);
		char* buf = (char*)malloc(flen * sizeof(char));
		char length[10];
		_itoa(flen, length, 10);
		strcat(msg.data, length);
		strcat(msg.data, "\n\n");
		int r = send(msg.clientSocket, msg.data, strlen(msg.data), 0);
		if (r == SOCKET_ERROR) {
			printf("send failed\n");
			*msg.isActive = false;
			return;
		}
		else {
			printf("send succeeded\n");
		}
;
		fread(buf, 1,flen, sendFile);
		r = send(msg.clientSocket, buf, flen, 0);
		if (r == SOCKET_ERROR) {
			printf("send failed\n");
			*msg.isActive = false;
			return;
		}
	}
}

// 处理收到的信息
void handleMSG(message msg)
{
	int i = 0, cnt = 0;
	bool flag = false;
	bool post_flag = false;
	std::string str = "";
	std::string type = "";
	std::string data = "";
	if (msg.data == "" || msg.data == "\n") {
		*msg.isActive = false;
		return;
	}
	// 解析请求类型
	if (msg.data[0] == 'G') { //GET
		type = "GET";
		data = msg.data + 4;
		int pos = data.find(' ');
		data = data.substr(0, pos);
	}
	if (msg.data[0] == 'P') { //POST
		type = "POST";
		data = msg.data + 5;
		int pos = data.find(' ');
		data = data.substr(0, pos);
	}

	// 处理请求
	if (type == "POST") { //POST

		bool login_flag = false;
		bool pass_flag = false;
		std::string name = "";
		std::string passwd = "";
		std::string msg_data(msg.data);
		int pos_login = msg_data.find("login");
		int pos_pwd = msg_data.find("pass");
		name = msg_data.substr(pos_login + 6, pos_pwd - pos_login - 7);
		passwd = msg_data.substr(pos_pwd + 5);
		if (name == "2473" && passwd == "123") {
			char response[200];
			strcpy(response, "<html><body>欢迎您,");
			strcat(response, name.c_str());
			strcat(response, "!</body></html>\n");
			int len = strlen(response);
			char length[20];
			sprintf(length, "%d", len);
			strcpy(msg.data, "HTTP/1.1 200 OK\n");
			strcat(msg.data, "Content-Type: text/html;charset=gb2312\nContent-Length: ");
			strcat(msg.data, length);
			strcat(msg.data, "\n\n");
			strcat(msg.data, response);
			printf("Info: login succeeded!\n");
			int r = send(msg.clientSocket, msg.data, 10000, 0);

			if (r == SOCKET_ERROR) {
				printf("send failed\n");
				*msg.isActive = false;
				return;
			}
			printf("send succeeded\n");
			*msg.isActive = false;
			return;
		}
		else {
			char response[200];
			strcpy(response, "<html><body>登录失败</body></html>\n");
			int len = strlen(response);
			char length[20];
			sprintf(length, "%d", len);
			strcpy(msg.data, "HTTP/1.1 200 OK\n");
			strcat(msg.data, "Content-Type: text/html;charset=gb2312\nContent-Length: ");
			strcat(msg.data, length);
			strcat(msg.data, "\n\n");
			strcat(msg.data, response);
			printf("Info: login failed!\n");
			int r = send(msg.clientSocket, msg.data, 10000, 0);

			if (r == SOCKET_ERROR) {
				printf("send failed\n");
				*msg.isActive = false;
				return;
			}
			printf("send succeeded\n");
			*msg.isActive = false;
			return;
		}

		*msg.isActive = false;
		return;
	}
	else if (type == "GET" && data != "") {

		memset(msg.data, 0, sizeof(msg.data));
		if (data.substr(0, 4) == "/hi/") {
			std::string str = "";
			std::string str1 = "";
			std::string passwd;
			std::string name;
			std::string path;

			bool txt_flag = false;
			int pos = data.find('.');
			str = data.substr(pos + 1);
			if (str == "") {
				*msg.isActive = false;
				return;
			}

			if (str == "txt") {
				path = "static/txt/" + data.substr(4);
			}
			else if (str == "html") {
				path = "static/html/" + data.substr(4);
			}

			sendMessage(path, msg);
		}
		else if (data.substr(0, 5) == "/img/") {
			std::string path = "static/img/" + data.substr(5);
			sendMessage(path, msg);
		}

	}
	closesocket(msg.clientSocket);
	*msg.isActive = false;
}

void listenForClose(close_message msg)
{
	std::string str;
	while (1) {
		std::cin >> str;

		if (str == "quit") {
			while (1) {
				bool flag = true;
				for (int i = 0; i < webServer::MAX; i++) {
					if (msg.isActive[i]) {
						flag = false;
						break;
					}
				}
				if (flag) {
					closesocket(msg.serverSocket);
					exit(0);
				}
			}
		}
		else {
			printf("syntex error!\n");
		}
	}
}

bool webServer::start()
{
	int on = 1;
	memset(isActive, false, sizeof(isActive));
	close_message msg(isActive, serverSocket);
	th = new std::thread(listenForClose, msg);
	//初始化服务器  
	memset(&serverChannel, 0, sizeof(serverChannel));
	serverChannel.sin_family = AF_INET;
	serverChannel.sin_addr.s_addr = htonl(INADDR_ANY);
	serverChannel.sin_port = htons(SERVER_PORT);

	//创建套接字  
	serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (serverSocket < 0) {
		printf("cannot create socket\n");
		return false;
	}
	else printf("successfully create socket\n");
	setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR,
		(char*)&on, sizeof(on));

	//绑定  
	int b = bind(serverSocket, (sockaddr*)&serverChannel,
		sizeof(serverChannel));
	if (b < 0) {
		printf("bind error\n");
		return false;
	}
	else printf("successfully bind\n");
	//监听  
	int l = listen(serverSocket, 1);
	if (l < 0) {
		printf("listen failed\n");
		return false;
	}
	else printf("successfully listen\n");
	int len = sizeof(serverChannel);
	//服务器等待连接  

	while (true) {
		printf("\nwaiting for connection...\n");

		//接受一个连接  
		clientSocket = accept(serverSocket, (sockaddr*)&serverChannel, &len);

		if (clientSocket < 0) {
			printf("accept failed\n");
		}
		else {
			printf("successfully connect\n");
			memset(buffer, 0, sizeof(buffer));

			int ret;
			ret = recv(clientSocket, buffer, BUFFER_SIZE, 0);
			if (ret == SOCKET_ERROR) {
				printf("sorry receive failed\n");
			}
			else if (ret == 0) {
				printf("the client socket is closed\n");
			}
			else {
				printf("successfully receive\n");
				for (int i = 0; i < MAX; i++) {
					if (!isActive[i]) {
						isActive[i] = true;
						message msg(buffer, &isActive[i], clientSocket, i);
						std::thread temp(&handleMSG, std::ref(msg));
						t[i] = &temp;
						t[i]->join();
						break;
					}
				}
			}
		}
	}
}