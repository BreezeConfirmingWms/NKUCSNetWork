#include"packet.h"

#pragma comment(lib,"ws2_32.lib") 
#include <cstdlib> 

#include <WinSock2.h>
#include<WS2tcpip.h>



#include<filesystem>
#include <fstream> 
#include <iostream>


#include<cmath>
#include<cassert>

#include<cstdlib>

#include<chrono>
#include<locale>
#include<ctime>
// #include <cstdint>
#include <vector>
#include<deque>
#include<queue>
using namespace std;







#define CLIENT_PORT 8888
#define SERVER_PORT  1099 //接收数据的端口号 
#define SERVER_IP    "127.0.0.1" //  服务器的 IP 地址 
#define BUFFER sizeof(packet)
#define WINDOWSIZE 20
SOCKET socketClient;//客户端套接字
SOCKET socketServer;
SOCKADDR_IN addrServer; //服务器地址
SOCKADDR_IN addrClient;

char filename[20];
int waitseq = 0;//等待的数据包
int totalpacket;//数据包总数
int seqnum = 40;//序列号个数
int len = sizeof(SOCKADDR);
int totalrecv = 0;
int recvwindow = WINDOWSIZE * BUFFER;//接收窗口大小
unsigned long long seqnumber = static_cast<unsigned long long>(UINT32_MAX) + 1;//序列号个数


//模拟丢包
BOOL lossInLossRatio(float lossRatio) {
	int lossBound = (int)(lossRatio * 100);
	int r = rand() % 101;
	if (r <= lossBound) {
		return TRUE;
	}
	return FALSE;
}


//初始化工作
void init()
{
	//加载套接字库（必须） 
	WORD wVersionRequested;
	WSADATA wsaData;
	//套接字加载时错误提示 
	int err;
	//版本 2.2 
	wVersionRequested = MAKEWORD(2, 2);
	//加载 dll 文件 Scoket 库   
	err = WSAStartup(wVersionRequested, &wsaData);
	if (err != 0)
	{
		//找不到 winsock.dll 
		cout << "WSAStartup failed with error: " << err << endl;
		return;
	}
	if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2)
	{
		cout << "Could not find a usable version of Winsock.dll" << endl;
		WSACleanup();
	}
	else
	{
		cout << "套接字创建成功" << endl;
	}
	socketClient = socket(AF_INET, SOCK_DGRAM, 0);
	socketServer= socket(AF_INET, SOCK_DGRAM, 0);

	addrServer.sin_family = AF_INET;
	addrServer.sin_port = htons(SERVER_PORT);
	inet_pton(AF_INET, SERVER_IP, &addrServer.sin_addr);

	//
	// addrClient.sin_family = AF_INET;
	// addrClient.sin_port = htons(CLIENT_PORT);
	// inet_pton(AF_INET, SERVER_IP, &addrClient.sin_addr);
}



int main()
{
	//<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<初始化工作<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<//
	init();
	std::ofstream out_result;
	int iMode = 1; //1：非阻塞，0：阻塞 
	ioctlsocket(socketClient, FIONBIO, (u_long FAR*) & iMode);//非阻塞设置 
	packet* pkt = new packet;
	pkt->init_packet();
	int stage = 0;
	float packetLossRatio = 0.2;  //默认包丢失率 
	float ackLossRatio = 0.2;  //默认 ACK 丢失率  							  
	srand((unsigned)time(NULL));//随机种子，放在循环的最外面 
	BOOL b;
	int recvSize;
	int sendSize;

	int c1 = 0;
	int c2 = 0;
	while (true)
	{
		//<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<建立连接<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<//
		pkt->init_packet();
		pkt->tag = 0;
		sendSize=sendto(socketClient, (char*)pkt, BUFFER, 0, (SOCKADDR*)&addrServer, sizeof(SOCKADDR));
		while (true)
		{
			//等待 server 回复
			switch (stage)
			{
			case 0://等待握手阶段 
				recvSize=recvfrom(socketClient, (char*)pkt, sizeof(*pkt), 0, (SOCKADDR*)&addrServer, &len);
				while(recvSize<0)
				{
					pkt = new packet;
					pkt->init_packet();
					pkt->tag = 0;
					sendto(socketClient, (char*)pkt, BUFFER, 0, (SOCKADDR*)&addrServer, sizeof(SOCKADDR));
					if (c1 % 20 == 0) {
						cout << "等待握手" << endl;
					}
					Sleep(100);
					recvSize = recvfrom(socketClient, (char*)pkt, sizeof(*pkt), 0, (SOCKADDR*)&addrClient, &len);
					c1++;
					if(recvSize>=0)
					{
						c1 = 0;
						break;
					}
				}
				totalpacket = pkt->len;
				cout << "准备建立连接，总共有" << totalpacket << "个数据包" << endl;
				pkt->init_packet();
				pkt = connecthandlerClient(200);
				sendto(socketClient, (char*)pkt, sizeof(*pkt), 0, (SOCKADDR*)&addrServer, sizeof(SOCKADDR));
				stage = 1;
				break;
			case 1:
				recvSize=recvfrom(socketClient, (char*)pkt, sizeof(*pkt), 0, (SOCKADDR*)&addrClient, &len);
				while(recvSize<0)
				{
					recvSize = recvfrom(socketClient, (char*)pkt, sizeof(*pkt), 0, (SOCKADDR*)&addrClient, &len);
					if(recvSize>0)
					{
						break;
					}
				}
				memcpy(filename, pkt->data, pkt->len);
				out_result.open(filename, std::ios::out | std::ios::binary);
				cout << "文件名为：" << filename << endl;
				if (!out_result.is_open())
				{
					cout << "文件打开失败！！！" << endl;
					exit(1);
				}
				stage = 2;
				break;
				//<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<文件传输<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<//
			case 2:
				pkt->init_packet();
				recvSize=recvfrom(socketClient, (char*)pkt, BUFFER, 0, (SOCKADDR*)&addrClient, &len);
				if (pkt->tag == 88)
				{
					cout << "**************************************" << endl;
					cout << "文件传输成功";
					cout << "**************************************" << endl;
					goto success;

				}
				//GBN实现				
				if (pkt->seq == waitseq && totalrecv < totalpacket && !corrupt(pkt))
				{

					// b = lossInLossRatio(packetLossRatio);
					// if (b) {
					// 	cout << "***************第  " << pkt->seq << " 号数据包丢失" << endl << endl;
					// 	continue;
					// }
					cout << "[Recv" << pkt->seq << "号数据包]" << endl << endl;
					recvwindow -= BUFFER;
					out_result.write(pkt->data, pkt->len);
					out_result.flush();
					recvwindow += BUFFER;
					make_mypkt(pkt, waitseq, recvwindow);
					cout << "发送对第" << waitseq << "号数据包的确认" << endl;
					sendto(socketClient, (char*)pkt, BUFFER, 0, (SOCKADDR*)&addrServer, sizeof(SOCKADDR));
					waitseq++;
					waitseq %= seqnumber;
					totalrecv++;
				}
				else
				{
					make_mypkt(pkt, waitseq - 1, recvwindow);
					if (c2 % 200== 0) {
						cout << "[不是期待的数据包，发送了一个重复ack]" << waitseq - 1 << endl;
						c2 = 0;
						c2++;
					}
					sendto(socketClient, (char*)pkt, BUFFER, 0, (SOCKADDR*)&addrServer, sizeof(SOCKADDR));
					
				}
				//SR实现
				/*
				if (pkt.seq <= waitseq + windowsize && pkt.seq >= waitseq&&totalrecv<totalpacket)
				{

					b = lossInLossRatio(packetLossRatio);
					if (b) {
						cout << "***************第  " << pkt.seq << " 号数据包丢失" << endl << endl;
						continue;
					}
					cout << "<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<收到第" << pkt.seq << "号数据包" << endl << endl;
					ack_send[pkt.seq - waitseq] = true;
					memcpy(&buffer_1[pkt.seq], &buffer[11], pkt.len);
					buffer[2] = pkt.seq;
					cout << "发送对第" << pkt.seq << "号数据包的确认" << endl;
					sendto(socketClient, buffer, BUFFER, 0, (SOCKADDR*)&addrServer, sizeof(SOCKADDR));
					int ack_s = 0;
					totalrecv++;
					while (ack_send[ack_s] && ack_s < windowsize)
					{
						out_result.write(buffer_1[ack_s], 1024);
						out_result.flush();
						waitseq++;
						if (waitseq == 20)
							waitseq = 0;
						ack_s += 1;
					}
					//向前滑动窗口
					if (ack_s > 0)
					{
						for (int i = 0; i < windowsize; i++)
						{
							if (ack_s + i < windowsize)
							{
								ack_send[i] = ack_send[i + ack_s];
								memcpy(buffer_1[i], buffer_1[i + ack_s], pkt.len);
								ZeroMemory(buffer_1[i + ack_s], sizeof(buffer_1[i + ack_s]));
							}
							else
							{
								ack_send[i] = false;
								ZeroMemory(buffer_1[i], sizeof(buffer_1[i]));
							}
						}
					}
				}
				else// if (pkt.seq >= waitseq - windowsize && pkt.seq <= windowsize - 1)
				{
					ZeroMemory(buffer, BUFFER);
					buffer[2] = waitseq;
					cout << "不在窗口内，发送了一个重复ACK" << waitseq-1 << endl;
					sendto(socketClient, buffer, 9, 0, (SOCKADDR*)&addrServer, sizeof(SOCKADDR));
				}
				break;
		*/
			}
		}
	success:
		{
			out_result.close();
			exit(0);
		}
	}
	closesocket(socketClient);
	WSACleanup();
	return 0;
}