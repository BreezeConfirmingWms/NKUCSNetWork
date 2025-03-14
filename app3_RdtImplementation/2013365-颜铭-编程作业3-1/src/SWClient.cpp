//Project: UdpReferenceClient
//Created By Yan Ming On 2023/11/10 
//Powered by VisualStudio_2019
//TODO:
//
//@CopyRight 2023-YanMing
//

#include"packet.h"

#pragma comment(lib,"ws2_32.lib")

#pragma warning(disable:4996)

#include <cstdlib> 

#include <WinSock2.h>
#include<WS2tcpip.h>



#include<filesystem>
#include <fstream> 
#include <iostream>


#include<cmath>
#include<cassert>

#include<cstdlib>


#include<ctime>
#include<chrono>
#include<locale>
#include<ctime>

#include <vector>
#include<deque>
#include<queue>
using namespace std;





#define MXN 5e5+5




#define CLIENT_PORT 8881
#define SERVER_PORT  1099 //接收数据的端口号 
//#define SERVER_PORT  8888 //接收数据的端口号 
#define SERVER_IP    "127.0.0.1" //  服务器的 IP 地址 
#define BUFFER sizeof(packet)
#define WINDOWSIZE 5
SOCKET socketClient;//客户端套接字
//SOCKET socketServer;
SOCKADDR_IN addrServer; //服务器地址
SOCKADDR_IN addrClient;
#define TIMEOUT 15
char filename[20];

int start_cnt = 0;

//模拟丢包
BOOL lossInLossRatio(float lossRatio) {
	int lossBound = (int)(lossRatio * 100);
	int r = rand() % 101;
	if (r <= lossBound) {
		return TRUE;
	}
	return FALSE;
}

int len = sizeof(SOCKADDR);
unsigned long long seqnumber = static_cast<unsigned long long>(UINT32_MAX) + 1;//序列号个数

char buffer[WINDOWSIZE][BUFFER];//选择重传缓冲区
char filepath[20];//文件路径
auto ack = vector<int>(WINDOWSIZE, 0);
int totalack = 0;//正确确认的数据包个数
int curseq = 0;//当前发送的数据包的序列号
int curack = 0;//当前等待被确认的数据包的序列号（最小）
int dupack = 0;//冗余ack
float cwnd = 1;//拥塞窗口大小，初始设置为1
int ssthresh = 1024;//阈值，初始设置为32

int sendwindow = WINDOWSIZE * BUFFER;//初始设置

int sendSize;
int recvSize;
int length = sizeof(SOCKADDR);
int STATE = 0;


bool swFlag = false;


int preack = 0;
int globalack = 0;
enum state
{
	SLOWSTART,//慢启动
	AVOID,//拥塞避免
	FASTRECO//快速恢复
};



//拥塞避免的上限是滑动窗口大小
int minwindow(int a, int b)
{
	if (a >= b)
		return b;
	else
		return a;
}



//超时重传
void timeouthandler()
{
	BOOL flag = false;
	packet* pkt1 = new packet;
	pkt1->init_packet();
	if (ack[curack % WINDOWSIZE] == 2)//快速重传之后还有没被确认的，认为包丢失
	{
		for (int i = curack; i != curseq; i = (i++) % seqnumber)
		{
			memcpy(pkt1, &buffer[i % WINDOWSIZE], BUFFER);
			sendto(socketClient, (char*)pkt1, BUFFER, 0, (SOCKADDR*)&addrServer, sizeof(SOCKADDR));
			cout << "[Repeat-Send（TimeOut） " << i << " 号数据包]" << endl << endl;
			flag = true;
		}
	}
	if (flag == true)
	{
		ssthresh = cwnd / 2;
		cwnd = 1;
		STATE = SLOWSTART;//检测到超时，就回到慢启动状态
		cout << "[TIMEOUT]检测到超时，回到慢启动阶段" << endl;
		cout << "cwnd=  " << cwnd << "     sstresh= " << ssthresh << endl << endl;
	}
}

//快速重传
void FASTRECOhandler()
{
	packet* pkt1 = new packet;
	pkt1->init_packet();
	for (int i = curack; i != curseq; i = (i++) % seqnumber)
	{
		memcpy(pkt1, &buffer[i % WINDOWSIZE], BUFFER);
		sendto(socketClient, (char*)pkt1, BUFFER, 0, (SOCKADDR*)&addrServer, sizeof(SOCKADDR));
		cout << "[Repeat-Send " << i << " 号数据包]" << endl;
	}
}


//收到数据包，判断是否是正确的ack，并做相应处理
void ackhandler(unsigned int a)
{
	long long index = a;
	switch (STATE)
	{
	case SLOWSTART:
		if ((index + seqnumber - curack) % seqnumber < minwindow(cwnd, WINDOWSIZE))
		{

			time_t now = time(0);
			tm* curTime = localtime(&now);

			char timestamp[20];
			std::strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", curTime);

			cout << "[Recv 第" << index << "号数据包的ACK确认]( "+string(timestamp)+" )"<< endl << endl;
			ack[index % WINDOWSIZE] = 3;
			if (cwnd <= ssthresh)
			{
				cwnd++;
				if (start_cnt % 100 == 0) {
					cout << "[慢启动阶段 " << endl;
					cout << "cwnd=  " << cwnd << "     sstresh= " << ssthresh << " ]" << endl << endl;
				}

			}

			totalack++;
			curack = (curack + 1) % seqnumber;
			swFlag = true;

			
		}
		else if (index == curack - 1 && !swFlag)
		{
			dupack++;
			if (dupack == 2)//进入快速重传 状态跳转到拥塞避免
			{
				FASTRECOhandler();
				ssthresh = cwnd / 2;
				cwnd = ssthresh + 3;
				STATE = AVOID;
				dupack = 0;
			}
		}
		break;
	case AVOID:
		break;
	}

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


	addrServer.sin_family = AF_INET;
	addrServer.sin_port = htons(SERVER_PORT);
	inet_pton(AF_INET, SERVER_IP, &addrServer.sin_addr);

	//
	addrClient.sin_family = AF_INET;
	addrClient.sin_port = htons(CLIENT_PORT);
	inet_pton(AF_INET, SERVER_IP, &addrClient.sin_addr);

	err = bind(socketClient, (SOCKADDR*)&addrClient, sizeof(SOCKADDR));
	if (err) {
		err = GetLastError();
		cout << "Could  not  bind  the  port" << SERVER_PORT << "for  socket. Error  code is" << err << endl;
		WSACleanup();
		return;
	}
	else
	{
		cout << "客户端注册成功" << endl;
	}

	for (int i = 0; i < WINDOWSIZE; i++)
	{
		ack[i] = 1;//初始标记为1表示最初的序列号
	}

}

int totalpacket = 0;
int waitcount = 0;
bool initFalg = false;

int main()
{
	//<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<初始化工作<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<//
	init();


	cout << "请输入要发送的文件名：";
	cin >> filepath;


	ifstream is(filepath, ifstream::in | ios::binary);//以二进制方式打开文件
	if (!is.is_open()) {
		cout << "文件无法打开!" << endl;
		exit(1);
	}
	is.seekg(0, std::ios_base::end);  //将文件流指针定位到流的末尾
	int length1 = is.tellg();
	totalpacket = length1 / MESSAGELEN + 1;
	cout << "文件大小为" << length1 << "Bytes,总共有" << totalpacket << "个数据包" << endl;
	is.seekg(0, std::ios_base::beg);
	int iMode = 1; //1：非阻塞，0：阻塞 
	ioctlsocket(socketClient, FIONBIO, (u_long FAR*) & iMode);//非阻塞设置 
	packet* pkt = new packet;
	pkt->init_packet();
	pkt->tag = 1;
	int stage = 0;
	float packetLossRatio = 0.2;  //默认包丢失率 
	float ackLossRatio = 0.2;  //默认 ACK 丢失率  							  
	srand((unsigned)time(NULL));//随机种子，放在循环的最外面 
	BOOL b;
	int recvSize;
	int sendSize;

	int c1 = 0;
	int c2 = 0;
	bool runFlag;
	clock_t st;
	while (true)
	{
		//<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<建立连接<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<//
		pkt->init_packet();
		pkt->tag = 1;
		sendSize = sendto(socketClient, (char*)pkt, BUFFER, 0, (SOCKADDR*)&addrServer, sizeof(SOCKADDR));

		while (true)
		{
			//等待 server 回复
			switch (stage)
			{
			case 0://等待握手阶段 
				recvSize = recvfrom(socketClient, (char*)pkt, sizeof(*pkt), 0, (SOCKADDR*)&addrClient, &len);
				while (recvSize < 0)
				{
					pkt = new packet;
					pkt->init_packet();
					pkt->tag = 0;
					sendto(socketClient, (char*)pkt, BUFFER, 0, (SOCKADDR*)&addrServer, sizeof(SOCKADDR));
					Sleep(200);
					if (c1 % 100 == 0) {
						cout << "等待握手" << endl;
					}
					
					recvSize = recvfrom(socketClient, (char*)pkt, sizeof(*pkt), 0, (SOCKADDR*)&addrClient, &len);
					c1++;
					if (recvSize >= 0)
					{
						c1 = 0;
						break; 
					}
				}

				if (pkt->tag == 100)
				{

					pkt = connecthandler(200, totalpacket);
					cout << "服务器准备好接受数据" << endl << endl;
					memcpy(pkt->data, filepath, strlen(filepath));
					pkt->len = strlen(filepath);

					sendto(socketClient, (char*)pkt, sizeof(*pkt), 0, (SOCKADDR*)&addrServer, sizeof(SOCKADDR));
					stage = 1;

					break;
				}


			case 1:
				recvSize = recvfrom(socketClient, (char*)pkt, sizeof(*pkt), 0, (SOCKADDR*)&addrClient, &len);
				while (recvSize < 0)
				{
					recvSize = recvfrom(socketClient, (char*)pkt, sizeof(*pkt), 0, (SOCKADDR*)&addrClient, &len);
					if (recvSize > 0)
					{

						break;
					}
				}
				if (pkt->tag == 201)
				{
					st = clock();
					stage = 2;
				}
				break;
			case 2:
				if (totalack == totalpacket)//数据包传输完毕
				{
					pkt->init_packet();
					pkt->tag = 88;
					cout << "*************************************" << endl;
					cout << "数据传输成功！" << endl;
					cout << "传输用时: " << (clock() - st) * 1000.0 / CLOCKS_PER_SEC << "ms" << endl;
					cout << "*************************************" << endl;
					sendto(socketClient, (char*)pkt, sizeof(*pkt), 0, (SOCKADDR*)&addrServer, sizeof(SOCKADDR));
					pkt = new packet;
					pkt->init_packet();
					recvSize = recvfrom(socketClient, (char*)pkt, sizeof(*pkt), 0, (SOCKADDR*)&addrClient, &len);
					int recv_cnt = 0;
					while(recvSize<0)
					{
						Sleep(100);
						pkt = new packet;
						pkt->init_packet();
						recvSize = recvfrom(socketClient, (char*)pkt, sizeof(*pkt), 0, (SOCKADDR*)&addrClient, &len);
						recv_cnt++;

						if (recv_cnt> 100)
						{
							cout << "[Assertion]服务主机未知状态,可选择按下q键离开" << endl;
							Sleep(100);
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
					if (pkt->tag==188) {
						goto wave;
					}
				}
				if(!initFalg)
				{
					curseq = 0;
					initFalg = true;
				}
				if((curseq + seqnumber - curack)%seqnumber==0&& totalack<totalpacket&&!corrupt(pkt))
				{
					swFlag = true;
					ZeroMemory(buffer[curseq % WINDOWSIZE], BUFFER);
						pkt->init_packet();
						if (length1 >= MESSAGELEN)
						{
							is.read(pkt->data, MESSAGELEN);
							make_pkt(pkt, curseq, MESSAGELEN);
							length1 -= MESSAGELEN;
						}
						else
						{
							is.read(pkt->data, length1);
							make_pkt(pkt, curseq, length1);
						}
						memcpy(buffer[curseq % WINDOWSIZE], pkt, BUFFER);
						sendto(socketClient, (char*)pkt, BUFFER, 0, (SOCKADDR*)&addrServer, sizeof(SOCKADDR));
						time_t now = time(0);
						tm* curTime;
						curTime = localtime(&now);

					char timestamp[20];
						std::strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", curTime);
					

						
						cout << "[Send 第 " << curseq << " 号数据包]("+string(timestamp)+")" << endl << endl;
						ack[curseq % WINDOWSIZE] = 2;
						++curseq;
						curseq %= seqnumber;
				}
				//
				//等待接收确认
				pkt->init_packet();
				recvSize = recvfrom(socketClient, (char*)pkt, BUFFER, 0, ((SOCKADDR*)&addrClient), &length);
				if (recvSize < 0)
				{
					waitcount++;
					Sleep(200);
					if (waitcount > (int)TIMEOUT/0.2)
					{
						cout << "[TIMEOUT] 检测到存在丢包超时" << endl << endl;
						timeouthandler();
						waitcount = 0;
					}
				}
				else
				{
					ackhandler(pkt->ack);
					cout << "ACK: " << pkt->ack << " CHECKSUM: " << pkt->checksum<<endl<<endl;
					sendwindow = pkt->window;
				}
				break;
			}
		}
	}
	wave:{
		recvSize = recvfrom(socketClient, (char*)pkt, sizeof(*pkt), 0, (SOCKADDR*)&addrClient, &length);
		int count_wave = 0;
		while(recvSize<0)
		{
			count_wave++;
			recvSize = recvfrom(socketClient, (char*)pkt, sizeof(*pkt), 0, (SOCKADDR*)&addrClient, &length);
			Sleep(100);
			if(count_wave>100)
			{
				cout << "[Assertion]服务主机未知状态,可选择按下q键离开" << endl;

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

		if(pkt->tag==202)
		{
			pkt = connecthandlerClient(212);
			sendto(socketClient, (char*)pkt, sizeof(*pkt), 0, (SOCKADDR*)&addrServer, sizeof(SOCKADDR));
		}


		recvSize = recvfrom(socketClient, (char*)pkt, sizeof(*pkt), 0, (SOCKADDR*)&addrClient, &length);
		while(recvSize<0)
		{

			recvSize = recvfrom(socketClient, (char*)pkt, sizeof(*pkt), 0, (SOCKADDR*)&addrClient, &length);
			
		}
		if(pkt->tag==213)
		{
			cout << "#############################" << endl;
			cout << "挥手成功，断开连接完成" << endl;
			cout << "#############################" << endl<<endl;
			closesocket(socketClient);
			WSACleanup();
			cout << "请按下q键离开" << endl << endl;
			char f;
			while (f = cin.get())
			{
				if (f == 'q')
				{
					exit(0);
				}
			}
			
		}

	}
	over:{
	closesocket(socketClient);
	WSACleanup();
	exit(0);
	}
}