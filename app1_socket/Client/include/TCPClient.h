#pragma once


#pragma once
#include <string>
#include <thread>
#include <WS2tcpip.h>
#include<Windows.h>
#pragma comment (lib, "ws2_32.lib")

#include<map>
#include<vector>
#include<queue>
#include<regex>
#include<stack>

#include<algorithm>




#define ANSI_CLEAR_LINES_TEMPLATE "\033[%dA\033[K"

// 宏函数，接受一个整数参数并生成相应的控制序列
#define ANSI_CLEAR_LINES_FORMAL(numLines) do { \
    char ansiClearLines[20]; \
    sprintf_s(ansiClearLines, ANSI_CLEAR_LINES_TEMPLATE, numLines); \
    printf("%s", ansiClearLines); \
} while (0)



inline std::map<std::string, std::vector<int>>user_responseLine;

//inline std::queue<std::string>user_responseFlush;
inline std::stack<std::string>user_responseFlush;

//std::vector < std::string>tmpLineMsg;
inline std::stack < std::string>tmpLineMsg;

inline int Line_cnt = 0;
inline int LineSkip_C = 0;


inline std::regex recvPattern(R"(^\[([A-Z]+)\]\((\d+)\)\(([a-zA-Z]+)\))");
inline std::regex userNamePattern(R"(([a-zA-Z]+): (\w+))");

inline std::smatch recvParser;
inline std::smatch uNameParser;


class TCPClient;

typedef void(*MessageReceivedHandler)(std::string msg);

class TCPClient
{
public:

	TCPClient();
	~TCPClient();
	bool initWinsock();
	void connectSock();
	void sendMsg(std::string txt);
	std::thread recvThread;
	void threadRecv();
	std::string username;
	bool joinChat = true;


private:
	SOCKET createSocket();
	std::string serverIP = "127.0.0.1";
	int serverPort = 54010;
	sockaddr_in hint;
	SOCKET serverSocket;		//服务器的Socket一定要全局维护
	bool recvThreadRunning;


};