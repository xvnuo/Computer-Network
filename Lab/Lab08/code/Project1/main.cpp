#include"webServer.h"
#pragma comment(lib,"ws2_32.lib")
int main()
{
	webServer ws;
	ws.start();
	system("pause");
}