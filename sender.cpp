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
#include<fstream>
//#include <udt.h>
#include "cc.h"
#include "test_util.h"

#include "kodo/encode.h"
#include "kodo/decode.h"

using namespace std;

const char cloud_server1[] = "139.199.94.164";
const char cloud_server2[] = "139.199.165.244";
const char cloud_server_port[] = "9090";
const int g_serverNum = 2;  // num of cloud server


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


#ifndef WIN32
void* monitor(void*);
#else
DWORD WINAPI monitor(LPVOID);
#endif

int main(int argc, char* argv[])
{
   if (2 != argc)
   {
      cout << "usage: appclient filename" << endl;
      return 0;
   }

   UDTSOCKET client1, client2;
   createUDTSocket(client1, cloud_server1, "9000");
   createUDTSocket(client2, cloud_server2, "9001");

   connect(client1, cloud_server1, cloud_server_port);
   connect(client2, cloud_server2, cloud_server_port);

   // #ifndef WIN32
   //    pthread_create(new pthread_t, NULL, monitor, &client2);
   // #else
   //    CreateThread(NULL, 0, monitor, &client2, 0, NULL);
   // #endif

    
    fstream in(argv[1], ios::in | ios::binary);
    
    const int size = 10240*3;
    char* buffer = new char[size];
    uint8_t seg_num = 0;
    while(!in.eof()){
        // in.get(buffer);
        in.read(buffer, size); 

        vector<uint8_t> data_out;
        encode((uint8_t*) buffer, data_out, size, seg_num++);
        const char* data = (const char*)data_out.data();
        cout<<"data encoded!"<<endl;
        // cout<<data<<endl;
        int ssize = 0;
        int ss;
        while (ssize < size)
        {
           if (UDT::ERROR == (ss = UDT::send(client1, data + ssize, size - ssize, 0)))
           {
              cout << "send:" << UDT::getlasterror().getErrorMessage() << endl;
              return 0;
           }
           ssize += ss;
           if (UDT::ERROR == (ss = UDT::send(client2, data + ssize, size - ssize, 0)))
           {
              cout << "send:" << UDT::getlasterror().getErrorMessage() << endl;
              return 0;
           }
           ssize += ss;
        }

        if (ssize < size)
           break;
        cout<<"data sent"<<endl;
    }
   
    in.close();

   // char* data;
   // int size = 10240;
   // data = new char[size];

   // for (int i = 0; ; i ++)
   // {
   //    int ssize = 0;
   //    int ss;
   //    while (ssize < size)
   //    {
   //       if (UDT::ERROR == (ss = UDT::send(client1, data + ssize, size - ssize, 0)))
   //       {
   //          cout << "send:" << UDT::getlasterror().getErrorMessage() << endl;
   //          break;
   //       }

   //       ssize += ss;
   //      
   //       if (UDT::ERROR == (ss = UDT::send(client2, data + ssize, size - ssize, 0)))
   //       {
   //          cout << "send:" << UDT::getlasterror().getErrorMessage() << endl;
   //          break;
   //       }

   //       ssize += ss;
   //    }

   //    if (ssize < size)
   //       break;
   // }

   UDT::close(client1);
   UDT::close(client2);
   // delete [] data;
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
