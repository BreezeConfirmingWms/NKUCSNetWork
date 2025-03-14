#pragma once


#ifndef PACKET_REFER


#define PACKET_REFER

#include<WS2tcpip.h>
// #include<WinSock2.h>
//
//
// #pragma comment(lib,"ws2_32.lib")

#define MESSAGELEN 4096
struct packet
{
	unsigned char tag;//连接建立、断开标识 
	unsigned int seq;//序列号 
	unsigned int ack;//确认号
	unsigned short len;//数据部分长度
	unsigned short len_buffer;//预留数据段位置长度信息。
	unsigned short checksum;//校验和
	unsigned short window;//窗口(停等机制不需要使用)
	char data[MESSAGELEN];//数据长度

	void init_packet()
	{
		this->tag = -1;
		this->seq = -1;
		this->ack = -1;
		this->len = -1;
		this->len_buffer = -1;
		this->checksum = -1;
		this->window = -1;
		ZeroMemory(this->data, MESSAGELEN);
	}
};

unsigned short make_sum(int count, char* buf)
{
	unsigned long sum = 0;

	while (count)
	{

		sum += *buf;


		if (sum & 0xffff0000)
		{

			sum &= 0xffff;

			sum++;
		}



		return ~(sum & 0xffff);
	}
}

void make_pkt(packet* pkt, unsigned int nextseqnum, unsigned int length)
{
	pkt->seq = nextseqnum;
	pkt->len = length;
	pkt->checksum = make_sum(sizeof(packet) / 2, pkt->data);
}

// 判断包是否损坏
bool corrupt(packet* pkt)
{
	int count = sizeof(pkt->data) / 2;
	register unsigned long sum = 0;
	unsigned short* buf = (unsigned short*)(pkt->data);
	while (count--) {
		sum += *buf++;
		if (sum & 0xFFFF0000) {
			sum &= 0xFFFF;
			sum++;
		}
	}
	if (pkt->checksum == ~(sum & 0xFFFF))
		return true;
	return false;
}


packet* connecthandler(int tag, int packetnum)
{
	packet* pkt = new packet;
	pkt->tag = tag;
	pkt->len_buffer = packetnum;
	return pkt;
}



void make_mypkt(packet* pkt, long long ack, unsigned short window)
{
	pkt->ack = ack;
	pkt->window = window;
}


packet* connecthandlerClient(int tag)
{
	packet* pkt = new packet;
	pkt->init_packet();
	pkt->tag = tag;
	return pkt;
}
#endif