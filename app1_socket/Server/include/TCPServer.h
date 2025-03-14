#pragma once

//FILENAME: TCPServer.h 
//Located On: WebAppDeveloping 
// Created By BreezeConfirming(颜铭) On 2023/10/13
//
//CopyRight@YanMing 2023 All Right Reserved
//
//Description:


#ifndef TCPSERVER_H
#define TCPSERVER_H

#define MAX_CLIENT_NUM 10
#define BASE_INDEX_CODE 10347


enum CliMessageType {
	MSG_TYPE_UNKNOWN = 0,
	MSG_TYPE_LOGIN,
	MSG_TYPE_CALL,
	MSG_TYPE_CHAT,
	MSG_TYPE_GET,
	MSG_TYPE_LOGOUT
};


#include<ctime>
#include<vector>
#include<fstream>
#include<map>
#include <string>

#include <WS2tcpip.h>
#include<WinSock2.h>
#pragma comment (lib, "ws2_32.lib")

inline struct ClientsMsg
{
	int u_id = 0;
	SOCKET soc;
	std::string name;
	std::string address = "127.0.0.1";
	std::string routing;


	std::time_t login_T;
	std::time_t logout_T;
	std::string login_time="0";
	std::string logout_time = "-1";

	std::string msg_data;
	unsigned long checksum;
	CliMessageType msgType;
	bool isOnLine;
}Sc[MAX_CLIENT_NUM];

class TcpNwLogger
{
private:
	std::string signature = "To Hash: Protect Safety For User";
	std::time_t timestamp;
	bool isClosed;
protected:
	char* buffer;

	std::fstream logFile;
	std::FILE* f_id;

public:
	TcpNwLogger(std::string filename);
	~TcpNwLogger();
	std::string computeSign(const std::string& data);
	void Log(const ClientsMsg& CliMsg);
};


class TCPServer;

//Callback fct = fct with fct as parameter.
typedef void(*MessageReceivedHandler)(TCPServer* listener, int socketID, std::string msg);

class TCPServer {
public:
	TCPServer();
	TCPServer(std::string ipAddress, int port);
	~TCPServer();

	void sendMsg(int clientSocket, std::string msg);
	bool initWinsock();
	void run();
	void cleanupWinsock();


private:
	SOCKET createSocket();
	std::string listenerIPAddress;
	int listenerPort;
	//MessageReceivedHandler messageReceived; 
};

#endif





