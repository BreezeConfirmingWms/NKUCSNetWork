//Project: UdpReferenceClient
//Created By Yan Ming On 2023/12/08 
//Powered by VisualStudio_2019
//TODO:
//
//@CopyRight 2023-YanMing


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

#include <vector>
#include<deque>
#include<queue>

#include<thread>
#include<mutex>
#include<condition_variable>

#include<future>
#include <iomanip>


// using namespace std;


using std::cout;
using std::cin;
using std::thread;
using std::vector;
using std::endl;
using std::unique_lock;
using std::lock_guard;
using std::future;
using std::launch;
using std::async;
using std::mutex;
using std::condition_variable;


namespace mybind {
	using namespace std::placeholders;
	using std::bind;
}




#define MXN 5e6+5




#define CLIENT_PORT 8881
#define SERVER_PORT  1099 //接收数据的端口号 
//#define SERVER_PORT  8888 //接收数据的端口号 
#define SERVER_IP    "127.0.0.1" //  服务器的 IP 地址 
#define BUFFER sizeof(packet)
 int WINDOWSIZE = 5;
 const int WINDOWSIZE_MAX = 32;
#define TIMEOUT 5



SOCKET socketClient;//客户端套接字
//SOCKET socketServer;
SOCKADDR_IN addrServer; //服务器地址
SOCKADDR_IN addrClient;

char filename[30];



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

// char buffer[WINDOWSIZE][BUFFER];//选择重传缓冲区

char** buffer;
char filepath[30];//文件路径
auto ack = vector<int>(WINDOWSIZE_MAX, 0);
int totalack = 0;//正确确认的数据包个数
int curseq = 0;//当前发送的数据包的序列号
int curack = 0;//当前等待被确认的数据包的序列号（最小）
int dupack = 0;//冗余ack
float cwnd = 1;//拥塞窗口大小，初始设置为1
int ssthresh = 32;//阈值，初始设置为32

int sendwindow = WINDOWSIZE * BUFFER;//初始设置

int sendSize;
int recvSize;
int length = sizeof(SOCKADDR);
int STATE = 0;


int c = 0;
bool handleFlag = 1;

bool srFlag = false;


int pre_ack = 0;
int wait_ack = 0;
int total_bytes = 0;


mutex mtx;
mutex amtx;
mutex signalMtx;
condition_variable cv;
bool packet_ready = false;
bool ack_check[(int)MXN];

HANDLE hThread[2];
int timerID[WINDOWSIZE_MAX];
int sendbase = 0;
MSG sysmsg;

clock_t ackStart;
bool ackSrFlag = false;

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
void timeouthandler(HWND hwnd,UINT uMsg,UINT idEvent,DWORD dwTime)
{
	

	unsigned int seq = -1;




	for (int i=0;i<minwindow(cwnd,WINDOWSIZE);i++)
	{
		if(timerID[i]==idEvent && timerID[i]!=0)
		{
			seq = i + sendbase;//序列号的判定
		}
	}

	if (seq == -1) {
		return; //如果没有超时的，就退出定时器的事件(可能是由于ack处理的问题)
	}
		packet* pkt_s = new packet();
	    pkt_s->init_packet();
		memcpy(pkt_s, buffer[seq-sendbase], BUFFER);
		sendto(socketClient, (char*)pkt_s, BUFFER, 0, (SOCKADDR*)&addrServer, sizeof(SOCKADDR));
		cout << "[Repeat-Send（TimeOut） " <<seq << " 号数据包]" << endl << endl;
		ssthresh = cwnd / 2 == 0 ? 1 : cwnd / 2;//RENO 流量阈值的动态调整
		cwnd = 1;//窗口大小的复位
		STATE = SLOWSTART;//检测到超时，就回到慢启动状态
		cout << "==========================检测到超时，回到慢启动阶段============================" << endl;
		cout << "cwnd=  " << cwnd << "     sstresh= " << ssthresh << endl << endl;

}

//快速重传
void fastReCoHandler(unsigned int seq)
{

	// lock_guard<mutex>glock(mtx);
	packet* pkt_s = new packet();
	pkt_s->init_packet();
	memcpy(pkt_s, buffer[seq - sendbase], BUFFER);
	sendto(socketClient, (char*)pkt_s, sizeof(*pkt_s), 0, (SOCKADDR*)&addrServer, sizeof(SOCKADDR));
	
}


//收到数据包，判断是否是正确的ack，并做相应处理
void ackhandler(unsigned int a)
{

	//
	// unique_lock<mutex>glock(mtx);
	long long index = a;
	// if (ack_check[index % seqnumber])
	// 	return;
	switch (STATE)
	{
	case SLOWSTART:
		if ((index + seqnumber - sendbase) % seqnumber < minwindow(cwnd, WINDOWSIZE))
		{

			KillTimer(NULL, timerID[index - sendbase]);

			timerID[index - sendbase] = 0;
			cout << "[Recv 第" << index << "号数据包的ack]" << endl << endl;
			ack[index % WINDOWSIZE] = 3;
			if (cwnd <= ssthresh)
			{
				cwnd++;
				cout << "==========================慢启动阶段============================" << endl;
				cout << "cwnd=  " << cwnd << "     sstresh= " << ssthresh << endl << endl;

			}
			else
			{
				STATE = AVOID;
			}
			if(index==sendbase)
			{

				for(int i=0;i<WINDOWSIZE;i++)
				{
					if (timerID[i])
						break;
					sendbase++;
				}

				int offset = sendbase- index;


				for(int i=0;i<WINDOWSIZE-offset;i++)
				{
					timerID[i] = timerID[i + offset];
					timerID[i + offset] = -1;
					memcpy(buffer[i], buffer[i + offset],BUFFER);

				}

				for(int i=WINDOWSIZE-offset;i<WINDOWSIZE;i++)
				{

					timerID[i] = -1;
				}
			}
			totalack++;

		}

		break;
	case AVOID:
		if ((index + seqnumber - sendbase) % seqnumber < minwindow(cwnd, WINDOWSIZE))
		{

			KillTimer(NULL, timerID[index - sendbase]);

			timerID[index - sendbase] = 0;
			cout << "[Recv " << index << "号数据包的ack]" << endl << endl;
			ack[index % WINDOWSIZE] = 3;
			cwnd = cwnd + 1 / cwnd;
			cout << "==========================达到阈值，进入拥塞避免阶段============================" << endl;
			cout << "cwnd=  " << int(cwnd) << "     sstresh=" << ssthresh << endl << endl;
			//选择确认

			if (index == sendbase)
			{

				for (int i = 0; i < WINDOWSIZE; i++)
				{
					if (timerID[i])
						break;
					sendbase++;
				}

				int offset = sendbase - index;


				for (int i = 0; i < WINDOWSIZE - offset; i++)
				{
					timerID[i] = timerID[i + offset];
					timerID[i + offset] = -1;
					memcpy(buffer[i], buffer[i + offset], sizeof(packet));

				}

				for (int i = WINDOWSIZE - offset; i < WINDOWSIZE; i++)
				{

					timerID[i] = -1;
				}
			}


			totalack++;

		}
		else if (index != sendbase)
		{
			dupack++;
			if (dupack == 3)
			{
				fastReCoHandler(sendbase);
				STATE = AVOID;
				dupack = 0;
			}
		}

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
		cout << "套接字创建成功" << endl << endl;
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


	std::string opt = "N";
	int len;
	cout << "CHOOSE WINDOW SIZE(default 5)? Pleas Input Y/N" << endl;

	cin >> opt;

	if (opt == "Y")
	{
		cout << "[SENDER]WINDOW SIZE PLEASE:" << endl;
		cin >> len;
		assert(len >= 4 && len <= 32);
		WINDOWSIZE = len;

		sendwindow = WINDOWSIZE * BUFFER;

		for (int i = 0; i < WINDOWSIZE; i++)
		{
			ack[i] = 1;//初始都标记为1
		}

	}
	else
	{
		assert(opt == "N");
	}


	for (int i = 0; i < WINDOWSIZE; i++)
	{
		ack[i] = 1;//初始都标记为1
	}
	

}

int totalpacket = 0;





std::ifstream is;
int lengthFile;


bool srSendFlag = false;
bool srOverFlag = false;

DWORD WINAPI sendEvents(LPVOID lparam)
{


	// unique_lock<mutex>glock(mtx);
	// auto sender= static_cast<int*>(lparam);
	// int sendbase = *sender;


	sendbase = 0;
	curseq = 0;

	buffer = new char* [WINDOWSIZE];
	for(int i=0;i<WINDOWSIZE;i++)
	{
		buffer[i] = new char[BUFFER];
	}
	for(int i=0;i<WINDOWSIZE;i++)
	{

		timerID[i] = -1;
	}


	clock_t start = clock();

	auto pkt = new packet();
	while (sendbase < totalpacket) {
		mtx.lock();
	
		while ((curseq + seqnumber - sendbase) % seqnumber < WINDOWSIZE && totalack < totalpacket)
		{
			
			pkt->init_packet();
			if (lengthFile >= MESSAGELEN)
			{
				is.read(pkt->data, MESSAGELEN);
				make_pkt(pkt, curseq, MESSAGELEN);
				lengthFile -= MESSAGELEN;
			}
			else
			{
				is.read(pkt->data, lengthFile);
				make_pkt(pkt, curseq, lengthFile);
			}


			memcpy(buffer[curseq - sendbase], pkt, BUFFER);
			sendto(socketClient, (char*)pkt, sizeof(*pkt), 0, (SOCKADDR*)&addrServer, sizeof(SOCKADDR));
			cout << "[Send 第 " <<curseq << " 号数据包]" << endl << endl;

			timerID[curseq - sendbase] = SetTimer(NULL, 0, TIMEOUT, (TIMERPROC)timeouthandler);
			cout << "[TimeID -" << curseq << "]: " << timerID[curseq - sendbase] << endl;
			++curseq;
			
			curseq %= seqnumber;

			std::this_thread::sleep_for(std::chrono::milliseconds(10));
		}
		// glock.unlock();

		
		mtx.unlock();


		while (PeekMessage(&sysmsg, NULL, 0, 0, PM_REMOVE))
		{
			if (sysmsg.message == WM_TIMER)
			{
				DispatchMessage(&sysmsg);
			}
		}
	}
	clock_t end = clock();
	pkt->init_packet();
	pkt->tag = 88;
	sendto(socketClient, (char*)pkt, sizeof(*pkt), 0, (SOCKADDR*)&addrServer, sizeof(SOCKADDR));


	pkt = new packet;
	pkt->init_packet();
	recvSize = recvfrom(socketClient, (char*)pkt, sizeof(*pkt), 0, (SOCKADDR*)&addrClient, &len);
	int recv_cnt = 0;
	while (recvSize < 0)
	{
		Sleep(100);
		pkt = new packet;
		pkt->init_packet();
		recvSize = recvfrom(socketClient, (char*)pkt, sizeof(*pkt), 0, (SOCKADDR*)&addrClient, &len);
		recv_cnt++;

		if (recv_cnt > 100)
		{
			cout << "[Assertion]服务主机未知状态,可选择按下q键离开" << endl;
			Sleep(100);
			char c;
			while (c = cin.get())
			{
				if (c == 'q')
				{
					; srOverFlag = true;
				}
			}
		}
	}
	if (pkt->tag == 188) {
		cout << "*************************************" << endl;
		cout << "数据传输成功！" << endl;
		cout << "传输用时: " << (end-start ) * 1000.0 / CLOCKS_PER_SEC << "ms" << endl;
		cout << "*************************************" << endl;
		srSendFlag = true;
	}


	for(int i=0;i<WINDOWSIZE;i++)
	{

		KillTimer(NULL, timerID[i]);
	}

	return 0;
}

int recvPnum = 0;
DWORD WINAPI recvAckEvents(LPVOID lparam)
{


	// unique_lock<mutex>glock(mtx);
	auto pkt = new packet();
	

	if (!ackSrFlag)
	{
		ackStart = clock();
		ackSrFlag = true;
	}
	while (sendbase < totalpacket) {

		recvSize = recvfrom(socketClient, (char*)pkt, sizeof(*pkt), 0, (SOCKADDR*)&addrClient, &len);
		if(recvSize>0)
		{
			recvPnum += 1;
		
			mtx.lock();
			ackhandler(pkt->ack);
			mtx.unlock();
			cout << "ACK: " << pkt->ack;
			cout << " CHECKSUM: " << pkt->checksum;
			auto use_time = clock() - ackStart;
			cout << " THROUGHPUT: " << std::fixed << std::setprecision(3) << (min(recvPnum, sendbase) * MESSAGELEN) /(static_cast<double> (use_time / CLOCKS_PER_SEC))<< " Bytes/s" << endl << endl;
		}

	}
	return 0;
}


int main()
{
	//<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<初始化工作<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<//
	init();


	cout << "请输入要发送的文件名：";
	cin >> filepath;


	is = std::ifstream (filepath, std::ifstream::in | std::ios::binary);//以二进制方式打开文件
	if (!is.is_open()) {
		cout << "文件无法打开!" << endl;
		exit(1);
	}
	is.seekg(0, std::ios_base::end);  //将文件流指针定位到流的末尾
	 lengthFile = is.tellg();
	totalpacket = lengthFile / MESSAGELEN + 1;
	cout << "文件大小为" << lengthFile << "Bytes,总共有" << totalpacket << "个数据包" << endl;
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
			if(stage==2)
			{
				goto mainthread;
			}
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
					if (c1 % 10 == 0) {
						cout << "等待握手" << endl;
					}
					std::this_thread::sleep_for(std::chrono::milliseconds(100));
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
					// st = clock();
					stage = 2;
				}
				break;
			}
		}
	}
mainthread: {
	hThread[0] = CreateThread(NULL, 0, sendEvents, NULL, 0, NULL);
	hThread[1] = CreateThread(NULL, 0, recvAckEvents, NULL, 0, NULL);
	WaitForMultipleObjects(2, hThread, TRUE, INFINITE);

	CloseHandle(hThread[0]);
	CloseHandle(hThread[1]);

	if(srOverFlag)
	{
		goto over;
	}

	if(srSendFlag)
	{
		goto wave;
	}
	else
	{
		cout << "程序已经发送所有文件，未知错误" << endl << endl;
	}
}



wave: {
recvSize = recvfrom(socketClient, (char*)pkt, sizeof(*pkt), 0, (SOCKADDR*)&addrClient, &length);
int count_wave = 0;
while (recvSize < 0)
{
	count_wave++;
	recvSize = recvfrom(socketClient, (char*)pkt, sizeof(*pkt), 0, (SOCKADDR*)&addrClient, &length);
	Sleep(100);
	if (count_wave > 100)
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

if (pkt->tag == 202)
{

	cout << "#############################" << endl;
	cout << "挥手成功，断开连接完成" << endl;
	cout << "#############################" << endl << endl;
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
over: {
closesocket(socketClient);
WSACleanup();
exit(0);
}

}
