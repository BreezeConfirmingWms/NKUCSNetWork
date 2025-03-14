//Project: WebAppDeveloping
//Created By Yan Ming On 2023/10/13 
//Powered by VisualStudio_2019
//TODO:
//
//@CopyRight 2023-YanMing
//

#include <iostream>
#include <string>
#include "TCPServer.h"

#include<chrono>
#include<locale>

#include<map>
#include<queue>
#include<stdexcept>
#include<regex>

#include<zlib.h>



using namespace std;



string serverIP = "127.0.0.1";



int main() {

	TCPServer server(serverIP, 54010);

	if (server.initWinsock()) {

		server.run();

	}


}