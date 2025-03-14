//Project: WebAppDeveloping
//Created By Yan Ming On 2023/10/13 
//Powered by VisualStudio_2019
//TODO:
//
//@CopyRight 2023-YanMing
//



#include "TCPServer.h"
#include <iostream>
#include <string>
#include <sstream>
#include<locale>
#include<chrono>


#include <thread>
#include<mutex>
#include<memory>
#include<zlib.h>


#include<cassert>
#include<regex>

#pragma warning(disable:4996)

const int MAX_BUFFER_SIZE = 4096;			//Constant value for the buffer size = where we will store the data received.



std::map<std::string, bool>user_base;
std::map<std::string, std::vector<std::string>>history_msg;

bool isCalled[MAX_CLIENT_NUM];
bool isGetFrom[MAX_CLIENT_NUM];
int calledIdx[MAX_CLIENT_NUM];
int getfromIdx[MAX_CLIENT_NUM];
int chatCount[MAX_CLIENT_NUM];
int user_count = 0;
int prev_count = 0;
bool new_login = false;
std::string register_info;

bool  protected_msg = false;




std::regex requestPattern(R"(^\[([A-Z]+)\]\((\d+)\))");// 解析请求信息

std::regex userParser(R"(^\(([a-zA-Z]+)\)\((\d+)\))");//解析用户响应署名信息
std::regex loginParser(R"(([a-zA-Z]+) joined the chat!)");//解析登录信息正则
std::regex oneMsgParser(R"(([a-zA-Z]+): (\w+))");

std::smatch reqMatch;
std::smatch uParserMatch;
std::smatch loginMatch;
std::smatch oneMsgMatch;

bool get_flag = false;
bool call_flag = false;
bool onemsg_flag = false;

int get_index = -1;

TcpNwLogger logger("C:\\Users\\30805\\Newlog.txt");



unsigned long calculateChecksum(const std::string& data) {
	return crc32(0L, (const Bytef*)data.c_str(), data.length());
}


bool validateChecksum(const std::string& tcpmsg, unsigned long checkSum) {
	if (tcpmsg.length() < sizeof(unsigned long)) {
		// 消息太短，没有足够的空间存放校验和
		return false;
	}

	std::string message = tcpmsg.substr(0, tcpmsg.length() - sizeof(unsigned long));

	unsigned long calculatedChecksum = calculateChecksum(message);

	return checkSum == calculatedChecksum; // 比较校验和
}

TcpNwLogger::TcpNwLogger(std::string filename)
{
	logFile.open(filename, std::ios::app);
	isClosed = false;
	if (!logFile.is_open()) {
		std::cerr << "Error: Unable to open log file." << std::endl;
		isClosed = true;
	}
}
TcpNwLogger::~TcpNwLogger()
{
	if (isClosed)
	{
		logFile.close();
	}
}



void TcpNwLogger::Log(const ClientsMsg& CliMsg)
{

	/*
	 * 自定义日志系统实现
	 */
	if (!isClosed)
	{
		std::time_t now = std::time(0);
		std::tm* currentTime = std::localtime(&now);
		char timestamp[20];
		std::strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", currentTime);

		int uid;
		std::string msg_special;

		switch (CliMsg.msgType) {
		case CliMessageType::MSG_TYPE_LOGIN:
			std::cout << "[" << "LOGIN" << "] " << CliMsg.name << ",Connect!!! [" << timestamp << "] " << std::endl;
			logFile << "[" << "LOGIN" << "] " << CliMsg.name << ",Welcome To Connect!!! [" << timestamp << "] " << std::endl;
			break;
		case CliMessageType::MSG_TYPE_LOGOUT:
			std::cout << "[" << "LOGOUT" << "] " << CliMsg.name << ",GoodBye!!! [" << timestamp << "] " << std::endl;
			logFile << "[" << "LOGOUT" << "] " << CliMsg.name << ",GoodBye!!! [" << timestamp << "] " << std::endl;
			break;
		case CliMessageType::MSG_TYPE_CHAT:
			std::cout << "[" << "CHAT ONLINE" << "] " << CliMsg.msg_data << " [" << timestamp << "] " << "(" << CliMsg.name << ")" << std::endl;
			logFile << "[" << "CHAT ONLINE" << "] " << CliMsg.msg_data << " [" << timestamp << "] " << "(" << CliMsg.name << ")" << std::endl;
			break;
		case CliMessageType::MSG_TYPE_CALL:
			uid = CliMsg.u_id - BASE_INDEX_CODE;
			msg_special = history_msg[CliMsg.name][chatCount[uid] - calledIdx[uid]];
			std::cout << "[" << "CALL" << "] " << msg_special << " [" << timestamp << "] " << "(" << CliMsg.name << ")" << std::endl;
			logFile << "[" << "CALL" << "] " << msg_special << " [" << timestamp << "] " << "(" << CliMsg.name << ")" << std::endl;
			break;
		case CliMessageType::MSG_TYPE_GET:
			uid = CliMsg.u_id - BASE_INDEX_CODE;
			msg_special = history_msg[CliMsg.name][chatCount[uid] - calledIdx[uid]];
			std::cout << "[" << "GET" << "] " << msg_special << " [" << timestamp << "] " << "(" << CliMsg.name << ")" << std::endl;
			logFile << "[" << "GET" << "] " << msg_special << " [" << timestamp << "] " << "(" << CliMsg.name << ")" << std::endl;
			break;
		case CliMessageType::MSG_TYPE_UNKNOWN:
			msg_special = "Unknown Received Msg";
			std::cout << "[" << "UNKNOWN" << "] " << "message loss!!!: "<<msg_special << " [" << timestamp << "] " << "(" << CliMsg.name << ")" << std::endl;
			logFile << "[" << "UNKNOWN" << "] " << "message loss!!!: "<<msg_special << " [" << timestamp << "] " << "(" << CliMsg.name << ")" << std::endl;
			break;
		default:
			std::cerr << "somthing wrong,shutdown?" << std::endl;
			break;
		}
	}
}
TCPServer::TCPServer() { }


TCPServer::TCPServer(std::string ipAddress, int port)
	: listenerIPAddress(ipAddress), listenerPort(port) {
}

TCPServer::~TCPServer() {
	cleanupWinsock();			//Cleanup Winsock when the server shuts down. 
}


//Function to check whether we were able to initialize Winsock & start the server. 
bool TCPServer::initWinsock() {

	WSADATA data;
	WORD ver = MAKEWORD(2, 2);

	int wsInit = WSAStartup(ver, &data);

	if (wsInit != 0) {
		std::cout << "Error: can't initialize Winsock." << std::endl;
		return false;
	}

	return true;

}


//Function that creates a listening socket of the server. 
SOCKET TCPServer::createSocket() {

	SOCKET listeningSocket = socket(AF_INET, SOCK_STREAM, 0);	//AF_INET = IPv4. 

	if (listeningSocket != INVALID_SOCKET) {

		sockaddr_in hint;		//Structure used to bind IP address & port to specific socket. 
		hint.sin_family = AF_INET;		//Tell hint that we are IPv4 addresses. 
		hint.sin_port = htons(listenerPort);	//Tell hint what port we are using. 
		inet_pton(AF_INET, listenerIPAddress.c_str(), &hint.sin_addr); 	//Converts IP string to bytes & pass it to our hint. hint.sin_addr is the buffer. 

		int bindCheck = bind(listeningSocket, (sockaddr*)&hint, sizeof(hint));	//Bind listeningSocket to the hint structure. We're telling it what IP address family & port to use. 

		if (bindCheck != SOCKET_ERROR) {			//If bind OK:

			int listenCheck = listen(listeningSocket, SOMAXCONN);	//Tell the socket is for listening. 
			if (listenCheck == SOCKET_ERROR) {
				return -1;
			}
		}

		else {
			return -1;
		}

		return listeningSocket;

	}

}


void TCPServer::run() {

/*
 *
 * 执行服务器的主要工作->评估套接字并接受连接或接收数据。
 */
	char buf[MAX_BUFFER_SIZE];		//创建缓冲区以接收来自客户机的数据. 
	SOCKET listeningSocket = createSocket();		//为服务器创建侦听的套接字

	std::cout << "||=====Server Status======||\n" << std::endl<<std::flush;

	while (true) {

		if (listeningSocket == INVALID_SOCKET) {
			break;
		}

		fd_set master;				//存储所有套接字的文件描述符。
		FD_ZERO(&master);			//空文件载入描述符。

		FD_SET(listeningSocket, &master);		//将侦听套接字加入文件描述符
		while (true) {

			fd_set copy = master;	//创建新的文件描述符bc，文件描述符每次都会被销毁。
			int socketCount = select(0, &copy, nullptr, nullptr, nullptr);				//确定套接字的状态并返回正在“工作”的套接字。

			for (int i = 0; i < socketCount; i++) {				//服务器只能接受连接和接收来自客户端的消息。
				SOCKET sock = copy.fd_array[i];					//循环遍历文件描述符中标识为非侦听的收发对象对应的所有套接字。

				if (sock == listeningSocket) {				//Case 1: accept new connection.

					SOCKET client = accept(listeningSocket, nullptr, nullptr);		//接受传入的连接并将其识别为新客户端。
					FD_SET(client, &master);		//向套接字列表中添加新连接。
					std::string welcomeMsg = "Welcome to YanMing's MultiThread Chat App(欢迎来到多人聊天室)\n";			
					std::cout << "New user joined the chat." << std::endl;			//在服务器端记录连接。
					recv(client, buf, MAX_BUFFER_SIZE, 0);
					std::string bufString(buf);
					//std::cout << "recv " << bufString << std::endl;
					register_info = bufString;
					std::regex_search(bufString, loginMatch, loginParser);

					auto user_name = loginMatch.str(1);

					Sc[user_count].name = user_name;
					Sc[user_count].msgType = CliMessageType::MSG_TYPE_LOGIN;
					std::time_t now = std::time(0);
					Sc[user_count].login_T = now;
					std::tm* currentTime = std::localtime(&now);
					char timestamp[20];
					std::strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", currentTime);
					Sc[user_count].login_time =static_cast<std::string>(timestamp);
					Sc[user_count].u_id = BASE_INDEX_CODE + user_count;
					Sc[user_count].isOnLine = true;
					Sc[user_count].soc = client;

					logger.Log(Sc[user_count]);
					user_base[user_name] = true;
					user_count += 1;
					new_login = true;
					std::cout << "Current Online User Count is（当前在线有用户总人数） " << user_count << std::endl;
					

				}
				else {										

					ZeroMemory(buf, MAX_BUFFER_SIZE);
					if(user_count==0)
					{
						return;
					}
					//Clear the buffer before receiving data. 
					int bytesReceived = recv(sock, buf, MAX_BUFFER_SIZE, 0);	//Receive data into buf & put it into bytesReceived. 

					if (bytesReceived <= 0 && !new_login) {	//No msg = drop client. 
						closesocket(sock);
						FD_CLR(sock, &master);	//从文件描述符中移除
					}
					else {

						//Send msg to other clients & not listening socket. 

					
						std::string recvMsg(buf);
						std::regex_search(recvMsg, uParserMatch, userParser);
						std::string userName = "root";
						unsigned long checkStr = 0;
						if (uParserMatch.length() > 1) {
							userName = uParserMatch.str(1);
							checkStr = std::stoul(uParserMatch.str(2));
						}
						int c = 0;
						int opt = 0;
						while (c < MAX_CLIENT_NUM)
						{

							if (Sc[c].name == userName) {

								break;
							}
							c++;
						}
						if (!Sc[c].isOnLine)
						{
							continue;
						}
					
						auto m_data = recvMsg.substr(uParserMatch.length());


						//std::cout << "debug info: " << m_data << std::endl;
						std::regex_search(m_data, reqMatch, requestPattern);


						if (reqMatch.size() < 1)
						{

							Sc[c].msgType = CliMessageType::MSG_TYPE_CHAT;
							auto checkStrSum = calculateChecksum(m_data);
							if(checkStrSum!=checkStr)
							{
								Sc[c].msgType = CliMessageType::MSG_TYPE_UNKNOWN; //   校验和失效，设置信息为不安全的UNKNOWN状态
							}
							Sc[c].checksum = checkStr;
							Sc[c].msg_data = m_data;


							recvMsg = m_data;
							history_msg[Sc[c].name].push_back(m_data);

							logger.Log(Sc[c]);
						}
						else
						{
							auto reqCommand = reqMatch.str(1); // 解析请求的头部
							if (reqCommand == "GET")
							{
								Sc[c].msgType = CliMessageType::MSG_TYPE_GET;

								opt = std::stoi(reqMatch.str(2));

								//std::cout << "debug: " << opt << std::endl;
								if (opt > history_msg[Sc[c].name].size())
								{
									continue;
								}
								get_flag = true;
								get_index = c;
								recvMsg = history_msg[Sc[c].name][history_msg[Sc[c].name].size() - opt];
								Sc[c].msg_data = m_data;
								history_msg[Sc[c].name].erase(history_msg[Sc[c].name].end() - opt);
								logger.Log(Sc[c]);
							}
							else if(reqCommand=="CALL")
							{
								recvMsg = m_data;
								Sc[c].msgType = CliMessageType::MSG_TYPE_CALL;
								opt = std::stoi(reqMatch.str(2));

								//std::cout << "debug: " << opt << std::endl;

								if (opt > history_msg[Sc[c].name].size())
								{
									continue;
								}
								recvMsg += "(" + Sc[c].name + ")";
								Sc[c].msg_data = m_data;
								//history_msg[Sc[c].name].erase(history_msg[Sc[c].name].end() - opt);
								logger.Log(Sc[c]);
								call_flag = true;
							}
							else if(reqCommand=="LOGOUT")
							{
								Sc[c].msgType = CliMessageType::MSG_TYPE_LOGOUT;
								std::time_t now = time(0);
								std::tm*endTime = localtime(&now);
								char timestamp[30];
								strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S",endTime);
								Sc[c].logout_T = now;
								Sc[c].logout_time = static_cast<std::string>(timestamp);

								auto time_diff = Sc[c].logout_T - Sc[c].login_T;

								auto tdiff_str = std::to_string(time_diff);
;								Sc[c].msg_data = "OFFLINE;BYE EVERYONE(From Server)[ONLINE TIME: "
									+ tdiff_str+ " s]";
								recvMsg = Sc[c].msg_data;
								Sc[c].isOnLine = false;
								history_msg[Sc[c].name].clear();
								logger.Log(Sc[c]);
								user_count--;
							}
						}
						std::string local_getMsg = recvMsg;
						

						for (int i = 0; i < master.fd_count; i++) {			//Loop through the sockets. 
							SOCKET outSock = master.fd_array[i];
							if (!get_flag) {
								if (outSock != listeningSocket) {

									if (outSock == sock) {		//如果当前套接字是发送消息的套接字:
										// std::string msgSent = "Message delivered.";
										// send(outSock, msgSent.c_str(), msgSent.size() + 1, 0);
										std::cout << "Message Delivered From " << userName << "\n";
										// //Notify the client that the msg was delivered. 	
									}
									else {
										if(!call_flag)//如果当前sock不是发送者->它应该接收msg。
										{
											std::regex_search(recvMsg, oneMsgMatch, oneMsgParser);
											
											if(oneMsgMatch.size()<=1)
											{
												recvMsg = userName + ": " + recvMsg;
											}
											// if (!onemsg_flag) {
											// 	recvMsg = userName + ": " + recvMsg;
											// 	onemsg_flag = true;
											// }
										}
											
										send(outSock, recvMsg.c_str(), strlen(recvMsg.data()), 0);
										//Send the msg to the current socket. 
									}

								}
							}
							else
							{
								if (outSock != listeningSocket) {

									if (outSock == sock) {		//如果当前套接字是发送消息的套接字:

										std::cout << "Message Delivered From " << userName << "\n";
										// //标记信息被成功从某个用户处发送
									}
									else {						//如果当前sock不是发送者->它应该接收msg。

										std::regex_search(local_getMsg, oneMsgMatch, oneMsgParser);
										
										if (oneMsgMatch.size() <= 1)
										{
											local_getMsg = userName + ": " + local_getMsg;
										}
									
										send(outSock, local_getMsg.c_str(), strlen (local_getMsg.data()), 0);
										//向当前套接字发送消息。
									}

								}
								if (outSock == listeningSocket)
								{
									recvMsg = "聊天记录前" + std::to_string(opt)+"条:"+ recvMsg;
									send(outSock, recvMsg.c_str(), strlen(recvMsg.data()), 0);

									assert(get_index != -1);
							
									send(Sc[get_index].soc, recvMsg.c_str(), strlen(recvMsg.data()), 0);

								}
							
							
							}
						}
						get_flag = false;
						call_flag = false;


					}

				}
				
			}
			prev_count = user_count;
		}


	}

}


//Function to send the message to a specific client. 
void TCPServer::sendMsg(int clientSocket, std::string msg) {

	send(clientSocket, msg.c_str(), msg.size() + 1, 0);

}


void TCPServer::cleanupWinsock() {

	WSACleanup();

}