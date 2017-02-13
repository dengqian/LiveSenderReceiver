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


const int SEGMENT_SIZE = 102400*4;
const int BLOCK_SIZE = 1024*4;
const int BLOCK_NUM = SEGMENT_SIZE / BLOCK_SIZE;
const int ENCODED_BLOCK_SIZE = BLOCK_SIZE+BLOCK_NUM+14;

using namespace std;

const char* SENDER_TO_SERVER_PORT = "9090";
const char* SERVER_TO_RECEIVER_PORT = "9000";

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

#ifndef WIN32
void* monitor(void* s)
#else
DWORD WINAPI monitor(LPVOID s)
#endif
{
   UDTSOCKET u = *(UDTSOCKET*)s;

   UDT::TRACEINFO perf;

   cout << "SendRate(Mb/s)\tRTT(ms)\tFlowWnd\tCWnd\tPktSndPeriod(us)\tRecvACK\tRecvNAK" << endl;

   while (true)
   {
      #ifndef WIN32
         sleep(1);
      #else
         Sleep(1000);
      #endif

      if (UDT::ERROR == UDT::perfmon(u, &perf))
      {
         cout << "perfmon: " << UDT::getlasterror().getErrorMessage() << endl;
         break;
      }

      cout << perf.mbpsSendRate << "\t\t" 
           << perf.msRTT << "\t" 
           << perf.pktFlowWindow << "\t" 
           << perf.pktCongestionWindow << "\t" 
           << perf.usPktSndPeriod << "\t\t\t" 
           << perf.pktRecvACK << "\t" 
           << perf.pktRecvNAK << endl;
   }

   #ifndef WIN32
      return NULL;
   #else
      return 0;
   #endif
}

#endif

