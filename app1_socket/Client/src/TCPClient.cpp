//Project: WebDevChatClient01
//Created By Yan Ming On 2023/10/13 
//Powered by VisualStudio_2019
//TODO:
//
//@CopyRight 2023-YanMing
//


#include "TCPClient.h"
#include <iostream>
#include <string>
#include <thread>



using namespace std;

bool reqFlag = false;



//当我们创建客户端时，我们不希望线程运行并尝试从服务器接收数据，直到输入用户名载入初始化后
TCPClient::TCPClient()
{
	recvThreadRunning = false;
}


TCPClient::~TCPClient()
{
	closesocket(serverSocket);
	WSACleanup();
	if (recvThreadRunning) {
		recvThreadRunning = false;
		recvThread.join();	//Destroy safely to thread. 
	}
}


bool TCPClient::initWinsock() {

	WSADATA data;
	WORD ver = MAKEWORD(2, 2);
	int wsResult = WSAStartup(ver, &data);
	if (wsResult != 0) {
		cout << "Error: can't start Winsock." << endl;
		return false;
	}
	return true;
}

SOCKET TCPClient::createSocket() {

	SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock == INVALID_SOCKET) {
		cout << "Error: can't create socket." << endl;
		WSACleanup();
		return -1;
	}

	//Specify data for hint structure. 
	hint.sin_family = AF_INET;
	hint.sin_port = htons(serverPort);
	inet_pton(AF_INET, serverIP.c_str(), &hint.sin_addr);

	return sock;

}

void TCPClient::threadRecv() {

	recvThreadRunning = true;
	while (recvThreadRunning) {

		char buf[4096];
		ZeroMemory(buf, 4096);

		int bytesReceived = recv(serverSocket, buf, 4096, 0);
		auto bufMsg = string(buf);
		regex_search(bufMsg, recvParser, recvPattern);
		string header_match="";
		if (recvParser.length() > 1) {
			header_match = recvParser.str(1);
			//cout << "debug header: " << header_match << endl << flush;
		}

		if(bytesReceived>0)//根据接受字节数判定连接数，如果断开连接，则bytesReceived==-1
		{

			if (header_match == "CALL") {			

				auto num_match = stoi(recvParser.str(2));

				auto name_match = recvParser.str(3);

				size_t nameOpt_len = user_responseLine[name_match].size();

				int line_idx = user_responseLine[name_match].at(nameOpt_len - num_match);

				//user_responseLine[name_match].erase(user_responseLine[name_match].end() - num_match);


				line_idx = Line_cnt - line_idx;

				int c = 0;
				stack<string>().swap(tmpLineMsg);
				while(c<line_idx)
				{
					auto  msg = user_responseFlush.top();


					user_responseFlush.pop();
					if (c==line_idx-1)
					{
						auto pos = msg.find(name_match);
						size_t spos = pos + name_match.length();


						msg.replace(spos+2, string::npos, msg.size() - spos-3, '*');
						msg += " (BACK:已撤回)";

					}
					tmpLineMsg.push(msg);
					c++;
				}
				for(c=1;c<=line_idx;c++)
					ANSI_CLEAR_LINES_FORMAL(1);

				this_thread::sleep_for(chrono::seconds(1));

				while(!tmpLineMsg.empty())
				{
					auto msgRear = tmpLineMsg.top();
					cout << msgRear << "\n" << std::flush;
					user_responseFlush.push(msgRear);
					tmpLineMsg.pop();
				}

			}
			else
			{
				regex_search(bufMsg, uNameParser, userNamePattern);
				auto userName = uNameParser.str(1);
				user_responseLine[userName].push_back(Line_cnt);
				user_responseFlush.push(bufMsg);
				std::cout << string(buf, 0, bytesReceived) << std::endl;
				Line_cnt++;
			}
		}
	}
}

void TCPClient::connectSock() {

	

	serverSocket = createSocket();

	int connResult = connect(serverSocket, (sockaddr*)&hint, sizeof(hint));
	if (connResult == SOCKET_ERROR) {
		cout << "Error: can't connect to server." << endl;
		closesocket(serverSocket);
		WSACleanup();
		return;
	}

}

void TCPClient::sendMsg(string txt) {

	if (!txt.empty() && serverSocket != INVALID_SOCKET) {

		send(serverSocket, txt.c_str(), txt.size() + 1, 0);

		//此函数将发送消息并尝试处理接收。在等待接收到的消息时，它会卡住。
	}

}