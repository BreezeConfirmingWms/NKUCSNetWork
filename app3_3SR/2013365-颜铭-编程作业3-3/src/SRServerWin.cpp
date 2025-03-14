//Project: UdpReference
//Created By Yan Ming On 2023/12/08 
//Powered by VisualStudio_2019
//TODO:
//
//@CopyRight 2023-YanMing
//


#include <cstdlib> 

#include <WinSock2.h>
#include<WS2tcpip.h>
#pragma comment(lib,"ws2_32.lib")


#include<filesystem>
#include <fstream> 
#include <iostream>


#include<cmath>
#include<cassert>

#include<chrono>
#include<locale>
#include<ctime>
// #include <cstdint>
#include <vector>
#include<deque>
#include<queue>

using namespace std;


#include "packet.h"




#define CLIENT_IP "127.0.0.1"
#define SERVER_IP    "127.0.0.1"  //服务器IP地址 
#define SERVER_PORT 8888		//服务器端口号
#define CLIENT_PORT 1099
//#define CLIENT_PORT 8888
#define BUFFER sizeof(packet)  //缓冲区大小
#define WINDOWSIZE 16 //滑动窗口大小

SOCKADDR_IN addrServer;   //服务器地址
SOCKADDR_IN addrClient;   //客户端地址

SOCKET sockServer;//服务器套接字
SOCKET sockClient;//客户端套接字


char buffer[WINDOWSIZE][BUFFER];//选择重传缓冲区
char filepath[20];//文件路径
auto ack = vector<int>(WINDOWSIZE, 0);
int totalpacket;//数据包个数
int totalack = 0;//正确确认的数据包个数
int curseq = 0;//当前发送的数据包的序列号
int curack = 0;//当前等待被确认的数据包的序列号（最小）
int dupack = 0;//冗余ack
float cwnd = 1;//拥塞窗口大小，初始设置为1
int ssthresh = 32;//阈值，初始设置为32
unsigned long long seqnumber = static_cast<unsigned long long>(UINT32_MAX) + 1;//序列号个数
int sendwindow = WINDOWSIZE * BUFFER;//初始设置

int sendSize;
int recvSize;
int length = sizeof(SOCKADDR);
int STATE = 0;


bool srFlag = false;
int c = 0;
bool ishandleFlag = false;

int preack = 0;
int globalEnter = 0;
enum state
{
	SLOWSTART,//慢启动
	AVOID,//拥塞避免
	FASTRECO//快速恢复
};


int waitseq = 0;//等待的数据包

int seqnum = 40;//序列号个数
int len = sizeof(SOCKADDR);

int recvwindow = WINDOWSIZE * BUFFER;//接收窗口大小



bool ack_send[WINDOWSIZE];
const int mxnPacket = 5e6;
bool ack_check[mxnPacket];

char bufferCache[WINDOWSIZE][MESSAGELEN];




//拥塞避免的上限是滑动窗口大小
int minwindow(int a, int b)
{
	if (a >= b)
		return b;
	else
		return a;
}


//初始化工作
void inithandler()
{
	WORD wVersionRequested;
	WSADATA wsaData;
	//套接字加载时错误提示 
	int err;
	//版本 2.2 
	wVersionRequested = MAKEWORD(2, 2);
	//加载 dll 文件 Scoket 库   
	err = WSAStartup(wVersionRequested, &wsaData);
	if (err != 0) {
		//找不到 winsock.dll 
		cout << "WSAStartup failed with error: " << err << endl;
		return;
	}
	if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2)
	{
		cout << "Could not find a usable version of Winsock.dll" << endl;
		WSACleanup();
	}
	sockServer = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	sockClient = socket(AF_INET, SOCK_DGRAM, 0);

	//设置套接字为非阻塞模式 
	int iMode = 1; //1：非阻塞，0：阻塞 
	ioctlsocket(sockServer, FIONBIO, (u_long FAR*) & iMode);//非阻塞设置 

	addrServer.sin_addr.S_un.S_addr = htonl(INADDR_ANY);
	addrServer.sin_family = AF_INET;
	addrServer.sin_port = htons(SERVER_PORT);
	//inet_pton(AF_INET, SERVER_IP, &addrClient.sin_addr);




	addrClient.sin_family = AF_INET;
	addrClient.sin_port = htons(CLIENT_PORT);
	inet_pton(AF_INET, CLIENT_IP, &addrClient.sin_addr);


	err = bind(sockServer, (SOCKADDR*)&addrServer, sizeof(SOCKADDR));
	if (err) {
		err = GetLastError();
		cout << "Could  not  bind  the  port" << SERVER_PORT << "for  socket. Error  code is" << err << endl;
		WSACleanup();
		return;
	}
	else
	{
		cout << "服务器创建成功" << endl;
	}

}




char filename[30];
int totalrecv = 0;
int main()
{
	//<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<初始化工作<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<//
	inithandler();

	packet* pkt = new packet;
	pkt->init_packet();
	int stage = 0;
	std::ofstream out_result;
	int recvSize;
	int sendSize;

	int c1 = 0;
	int c2 = 0;

	while (true) {
		recvSize = recvfrom(sockServer, (char*)pkt, BUFFER, 0, ((SOCKADDR*)&addrServer), &length);
		int count = 0;
		int waitcount = 0;
		while (recvSize < 0)
		{
			count++;
			recvSize = recvfrom(sockServer, (char*)pkt, BUFFER, 0, ((SOCKADDR*)&addrServer), &length);

			Sleep(100);
			if (count > 20)
			{



				cout << "当前没有客户端请求连接！" << endl << endl;
				count = 0;

			}
			if (recvSize >= 0)
				break;
		}
		//握手建立连接阶段
		//服务器收到客户端发来的TAG=0的数据报，标识请求连接
		//服务器向客户端发送一个 100 大小的状态码，表示服务器准备好了，可以发送数据
		//客户端收到 100 之后回复一个 200 大小的状态码，表示客户端准备好了，可以接收数据了
		//服务器收到 200 状态码之后，就开始发送数据了 
		if (pkt->tag == 1)
		{
			clock_t st = clock();//开始计时
			cout << "开始建立连接..." << endl;
			int stage = 0;
			bool runFlag = true;
			int waitCount = 0;
			while (runFlag)
			{
				switch (stage)
				{
				case 0://发送100阶段
					pkt = connecthandlerClient(100);
					sendSize = sendto(sockServer, (char*)pkt, BUFFER, 0, (SOCKADDR*)&addrClient, sizeof(SOCKADDR));

					Sleep(100);
					stage = 1;
					break;
				case 1://等待接收200阶段
					ZeroMemory(pkt, sizeof(*pkt));
					recvSize = recvfrom(sockServer, (char*)pkt, BUFFER, 0, ((SOCKADDR*)&addrServer), &length);
					if (recvSize < 0)
					{
						++waitCount;
						Sleep(200);
						if (waitCount > 20)
						{
							runFlag = false;
							cout << "连接建立失败！等待建立新连接..." << endl;
							break;
						}

					}
					else
					{
						if (pkt->tag == 200)
						{
							totalpacket = pkt->len_buffer;
							cout << "准备建立连接，总共有" << totalpacket << "个数据包" << endl;
							memcpy(filename, pkt->data, pkt->len);
							out_result.open(filename, std::ios::out | std::ios::binary);
							cout << "文件名为：" << filename << endl;
							if (!out_result.is_open())
							{
								cout << "文件打开失败！！！" << endl;
								exit(1);
							}


							pkt = connecthandlerClient(201);

							sendSize = sendto(sockServer, (char*)pkt, BUFFER, 0, (SOCKADDR*)&addrClient, sizeof(SOCKADDR));
							stage = 2;

						}
					}
					break;
				case 2:
					pkt->init_packet();
					recvSize = recvfrom(sockServer, (char*)pkt, BUFFER, 0, (SOCKADDR*)&addrServer, &len);
					while (recvSize < 0)
					{

						//Sleep(50);
						recvSize = recvfrom(sockServer, (char*)pkt, BUFFER, 0, (SOCKADDR*)&addrServer, &len);
						if (recvSize > 0)
						{
							break;
						}
					}


					if (pkt->tag == 88)
					{
						cout << "**************************************" << endl;
						cout << "文件传输成功"<< endl;
						cout << "**************************************" << endl;
						pkt->init_packet();
						pkt = connecthandlerClient(188);
						sendto(sockServer, (char*)pkt, BUFFER, 0, (SOCKADDR*)&addrClient, sizeof(SOCKADDR));

						goto success;
					

					}
					if (!corrupt(pkt))
					{
						if (ack_check[pkt->seq]) {
							make_mypkt(pkt, pkt->seq, recvwindow);
							sendto(sockServer, (char*)pkt, BUFFER, 0, (SOCKADDR*)&addrClient, sizeof(SOCKADDR));
							continue;
						}
						if (pkt->seq < waitseq + WINDOWSIZE && pkt->seq >= waitseq && totalrecv < totalpacket)
						{

							
							cout << "[Recv 第" << pkt->seq << "号数据包 ]" << endl << endl;
							ack_check[pkt->seq] = true;
							ack_send[pkt->seq - waitseq] = true;
							memcpy(&bufferCache[pkt->seq - waitseq], pkt->data, pkt->len);



							int ack_s = 0;
							totalrecv++;
							while (ack_send[ack_s] && ack_s < WINDOWSIZE)
							{
								out_result.write(bufferCache[ack_s], MESSAGELEN);
								out_result.flush();
								waitseq++;
								// if (waitseq == WINDOWSIZE)
								// 	waitseq = 0;
								ack_s += 1;
							}

							make_mypkt(pkt, pkt->seq, recvwindow);
							cout << "[Send 第" << pkt->seq << "号数据包的ACK确认]" << endl;


							sendto(sockServer, (char*)pkt, BUFFER, 0, (SOCKADDR*)&addrClient, sizeof(SOCKADDR));
							//向前滑动窗口
							if (ack_s > 0)
							{
								for (int i = 0; i < WINDOWSIZE; i++)
								{
									if (ack_s + i < WINDOWSIZE)
									{
										ack_send[i] = ack_send[i + ack_s];
										ack_send[i + ack_s] = false;
										memcpy(&bufferCache[i], &bufferCache[i + ack_s], pkt->len);
										ZeroMemory(&bufferCache[i + ack_s], sizeof(bufferCache[i + ack_s]));
									}
									else
									{
										ack_send[i] = false;
										ZeroMemory(&bufferCache[i], sizeof(bufferCache[i]));
									}
								}
							}
						}
						else if (pkt->seq >= waitseq - WINDOWSIZE && pkt->seq <= waitseq - 1)
						{
							ack_check[pkt->seq] = true;

							cout << "[SR: 不在窗口内，重发一个ACK" << pkt->seq << " ]" << endl << endl;
							make_mypkt(pkt, pkt->seq, recvwindow);

							sendto(sockServer, (char*)pkt, BUFFER, 0, (SOCKADDR*)&addrClient, sizeof(SOCKADDR));
						}
						else
						{

							if(totalpacket==totalrecv)
							{
								pkt->init_packet();
								pkt = connecthandlerClient(188);
								sendto(sockServer, (char*)pkt, BUFFER, 0, (SOCKADDR*)&addrClient, sizeof(SOCKADDR));
							}
							globalEnter++;
							if (globalEnter % 10==1) {
								cout << "[Interruption]: 发送包不在窗口内" << endl << endl;
								
							}
							// if(globalEnter>2000)
							// {
							// 	goto success;
							// }
						}
					}
					else
					{
						cout << "packet corrupt!not operation!!!" << endl << endl;
					}
					break;
				}

			}
		}

	}


success:
	{


	/*
	 *挥手
	 */
	 pkt = connecthandlerClient(202);
	 sendSize = sendto(sockServer, (char*)pkt, BUFFER, 0, (SOCKADDR*)&addrClient, sizeof(SOCKADDR));
	 int count_wave = 0;

	 while (sendSize < 0)
	 {
		 count_wave++;
		 sendSize = sendto(sockServer, (char*)pkt, BUFFER, 0, (SOCKADDR*)&addrClient, length);
		 if (count_wave > 100)
		 {
			 cout << "[Assertion]目标客户端已经断开,请按下q结束程序" << endl;
			 char c;
			 while (c = cin.get())
			 {
				 if (c == 'q')
				 {
					 goto over;
				 }
			 }
		 }
	 }

	 pkt = new packet;
	 pkt->init_packet();
	 recvSize = recvfrom(sockServer, (char*)pkt, BUFFER, 0, (SOCKADDR*)&addrServer, &length);

	 while (recvSize < 0)
	 {
		 recvSize = recvfrom(sockServer, (char*)pkt, BUFFER, 0, (SOCKADDR*)&addrServer, &length);

	 }
	 pkt = connecthandlerClient(213);
	 sendto(sockServer, (char*)pkt, BUFFER, 0, (SOCKADDR*)&addrClient, length);
	 cout << "[WAVE  SUCCESS]挥手完成，成功断开连接" << endl;
	 //Sleep(1000);
	 cout << "可以按下q键退出服务器的接收程序" << endl;
	 char c;
	 while (c = cin.get())
	 {
		 if (c == 'q')
		 {
			 goto over;
		 }
	 }

	}


over: {

	closesocket(sockServer);
	WSACleanup();



	out_result.close();
	exit(0);
	}
//关闭套接字 

return 0;
}