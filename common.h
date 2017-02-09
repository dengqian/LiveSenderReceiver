#ifndef _COMMON_H_
#define _COMMON_H_

#include <unistd.h>
#include <cstdlib>
#include <cstring>
#include <netdb.h>
#include <iostream>
#include <cstdint>

#include "cc.h"
#include "test_util.h"


#define SEGMENT_SIZE 10240   
#define BLOCK_SIZE 1024
#define BLOCK_NUM SEGMENT_SIZE / BLOCK_SIZE 
#define ENCODED_BLOCK_SIZE 1048

using namespace std;

const char* SENDER_TO_SERVER_PORT = "9090";
const char* SERVER_TO_RECEIVER_PORT = "9001";

const int g_serverNum = 2;  // num of cloud server


int createUDTSocket(UDTSOCKET& usock, const char* local_port)
{
   struct addrinfo hints, *local;

   memset(&hints, 0, sizeof(struct addrinfo));

   hints.ai_flags = AI_PASSIVE;
   hints.ai_family = AF_INET;
   hints.ai_socktype = SOCK_STREAM;
   //hints.ai_socktype = SOCK_DGRAM;

   if (0 != getaddrinfo(NULL, local_port, &hints, &local))
   {
      cout << "incorrect network address.\n" << endl;
      return 0;
   }

   usock = UDT::socket(local->ai_family, local->ai_socktype, local->ai_protocol);

   #ifdef WIN32
      UDT::setsockopt(usock, 0, UDT_MSS, new int(1052), sizeof(int));
   #endif
      
   freeaddrinfo(local);
   
   return 1;
}

int connect(UDTSOCKET& usock, const char *server_ip, const char* port)
{
   addrinfo hints, *peer;
   memset(&hints, 0, sizeof(struct addrinfo));

   hints.ai_flags = AI_PASSIVE;
   hints.ai_family = AF_INET;
   hints.ai_socktype = SOCK_STREAM;

   if (0 != getaddrinfo(server_ip, port, &hints, &peer))
   {
      cout << "incorrect server/peer address. " << server_ip << ":" << port << endl;
      return 0;
   }

   // connect to the server, implict bind
   if (UDT::ERROR == UDT::connect(usock, peer->ai_addr, peer->ai_addrlen))
   {
      cout << "connect: " << UDT::getlasterror().getErrorMessage() << endl;
      return 0;
   }

   freeaddrinfo(peer);
   
   return 1;
}

#endif

