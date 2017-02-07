#ifndef WIN32
   #include <unistd.h>
   #include <cstdlib>
   #include <cstring>
   #include <netdb.h>
#else
   #include <winsock2.h>
   #include <ws2tcpip.h>
   #include <wspiapi.h>
#endif

#include <iostream>

#include "cc.h"
#include "test_util.h"
#include "kodo/decode.h"

using namespace std;

const char* cloud_server1 = "139.199.94.164";
const char* cloud_server2 = "139.199.165.244";
const char* cloud_server_port = "9000";
const int g_serverNum = 2;  // num of cloud server

void* recvdata(void*);
void* monitor(void*);


int createUDTSocket(UDTSOCKET& usock, const char* server_ip, const char* server_port)
{
   struct addrinfo hints, *local;

   memset(&hints, 0, sizeof(struct addrinfo));

   hints.ai_flags = AI_PASSIVE;
   hints.ai_family = AF_INET;
   hints.ai_socktype = SOCK_STREAM;
   //hints.ai_socktype = SOCK_DGRAM;

   if (0 != getaddrinfo(NULL, "9000", &hints, &local))
   {
      cout << "incorrect network address.\n" << endl;
      return 0;
   }

   usock = UDT::socket(local->ai_family, local->ai_socktype, local->ai_protocol);

   #ifdef WIN32
      UDT::setsockopt(usock, 0, UDT_MSS, new int(1052), sizeof(int));
   #endif
      
   freeaddrinfo(local);
   
   return 0;
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
   
   return 0;
}


int main(int argc, char* argv[])
{
   if (1 != argc)
   {
      cout << "usage: appclient filename" << endl;
      return 0;
   }

   UDTUpDown _udt_;

   UDTSOCKET client1, client2;
   createUDTSocket(client1, cloud_server1, cloud_server_port);
   createUDTSocket(client2, cloud_server2, cloud_server_port);

   connect(client1, cloud_server1, cloud_server_port);
   connect(client2, cloud_server2, cloud_server_port);
   
   // #ifndef WIN32
   //    pthread_create(new pthread_t, NULL, monitor, &client2);
   // #else
   //    CreateThread(NULL, 0, monitor, &client2, 0, NULL);
   // #endif

   #ifndef WIN32
      pthread_t rcvthread1, rcvthread2;
      pthread_create(&rcvthread1, NULL, recvdata, new UDTSOCKET(client1));
      pthread_join(rcvthread1, NULL);
      pthread_create(&rcvthread2, NULL, recvdata, new UDTSOCKET(client2));
      pthread_join(rcvthread2, NULL);
   #else
      CreateThread(NULL, 0, recvdata, new UDTSOCKET(recver), 0, NULL);
   #endif


   UDT::close(client1);
   UDT::close(client2);

   return 0;
}

#ifndef WIN32
void* monitor(void* s)
#else
DWORD WINAPI monitor(LPVOID s)
#endif
{
   UDTSOCKET u = *(UDTSOCKET*)s;

   UDT::TRACEINFO perf;

   cout << "RecvRate(Mb/s)\tRTT(ms)\tFlowWnd\tCWnd\tPktSndPeriod(us)\tRecvACK\tRecvNAK" << endl;

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

      cout << perf.mbpsRecvRate << "\t\t" 
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


#ifndef WIN32
void* recvdata(void* usocket)
#else
DWORD WINAPI recvdata(LPVOID usocket)
#endif
{
   UDTSOCKET recver = *(UDTSOCKET*)usocket;
   delete (UDTSOCKET*)usocket;

   int size = 10240;
   char* data = new char[size];
   int total = 0;

   while (true)
   {
      int rsize = 0;
      int rs;
      while (rsize < size)
      {
         int rcv_size;
         int var_size = sizeof(int);
         UDT::getsockopt(recver, 0, UDT_RCVDATA, &rcv_size, &var_size);

         if (UDT::ERROR == (rs = UDT::recv(recver, data + rsize, size - rsize, 0)))
         {
            cout << "recv:" << UDT::getlasterror().getErrorMessage() << endl;
            break;
         }

         total += rs;
         // cout<< total <<"bytes data rcvd"<<endl;

         rsize += rs;
      }

      // vector<uint8_t> decode_data;
      // decode((uint8_t*)data, decode_data, size); 
      // const char* data_decoded = (const char*)decode_data.data();
      // cout<<data_decoded<<endl;
	
      cout<<"recvd data:"<<data<<endl;
      if (rsize < size)
         break;
   }


   delete [] data;

   UDT::close(recver);

   #ifndef WIN32
      return NULL;
   #else
      return 0;
   #endif
}
