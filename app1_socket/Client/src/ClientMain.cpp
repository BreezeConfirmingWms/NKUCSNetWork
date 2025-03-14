//Project: WebDevChatClient01
//Created By Yan Ming On 2023/10/13 
//Powered by VisualStudio_2019
//TODO:
//
//@CopyRight 2023-YanMing
//
#include <iostream>
#include <regex>

#include "TCPClient.h"
#include <string>
#include <sstream>

#include<atomic>
#include<mutex>
#include<condition_variable>

#include<zlib.h>
#pragma comment (lib, "ws2_32.lib")

using namespace std;


atomic<bool> ctrlPressed = false;
bool exitPressed = false;

mutex mtx;
condition_variable cv;


void CheckKeyCtrl()
{
	if (GetAsyncKeyState(VK_CONTROL) & 0x8000)
	{
		ctrlPressed = true;
	}
	else
		ctrlPressed = false;

	this_thread::sleep_for(chrono::milliseconds(10));
		
}

void CheckLogOutKey() {
	while (true) {
		if (GetAsyncKeyState(VK_CONTROL) & 0x8000 && GetAsyncKeyState('Q') & 0x8000) {
			exitPressed= true;
			cv.notify_one(); // 通知主线程
			return;
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}
}



unsigned long ChecksumCalc(const std::string& data) {

	/*
	 *
	 * crc32 校验和计算方法
	 * 通过校验和检查信息是否流失（UNKNOWN）
	 */
	return crc32(0L, (const Bytef*)data.c_str(), data.length());
}




int main() {

	TCPClient* client = new TCPClient;
	string msg = "~";
	string usernameEntered=" ";


	cout << "Enter your username.(Cli)" << endl;
	Line_cnt++;
	user_responseFlush.push("Enter your username.(Cli)");
	cin >> usernameEntered;
	Line_cnt++;
	client->username = usernameEntered;
	Line_cnt += 2;
	user_responseFlush.push(usernameEntered);
	
	if (client->initWinsock()) {

		client->connectSock();

		client->recvThread = thread([&] {
			client->threadRecv();
		});

		

		while (true) {

			std::string messageToSend;
	
			getline(cin, msg);

			// std::thread keyboardThread(CheckLogOutKey);
			//
			// std::unique_lock<std::mutex> lock(mtx);
			// cv.wait(lock, [&] { return exitPressed || !msg.empty() ; });
			//

			user_responseFlush.push(msg);
			Line_cnt++;


		
			if (client->joinChat == false) {
				std::ostringstream ss;
				// ss << client->username << ": " << msg;
				// messageToSend = ss.str();

				auto checkSum = ChecksumCalc(msg);
				messageToSend = "(" + usernameEntered + ")" + "(" + std::to_string(checkSum) + ")" + msg;
				if (msg == "[OUT]")
				{
					messageToSend = "(" + usernameEntered + ")"+"(" + std::to_string(checkSum) + ")" + "[LOGOUT](0)";
					//client->sendMsg(messageToSend);
					exitPressed = true;
				}

				
				//std::cout << "debug: "<<std::to_string(checkSum) << std::endl
				//;
				
				cout << "(From You)\n" << std::flush;
				user_responseFlush.push("(From You)\r");
				Line_cnt+=1;
			}
			else if (client->joinChat == true) {
				std::ostringstream ss;
				ss << client->username << " joined the chat!";
				messageToSend = ss.str();
				client->joinChat = false;
			}

			
			client->sendMsg(messageToSend);

			if (exitPressed)
				break;
			//keyboardThread.join();
			//ctrlThreadExample.join();
		}
		
	}

	
	delete client;
	cin.get();
	return 0;

}