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
#include <unordered_map>

// #include "cc.h"
// #include "test_util.h"

#include "kodo/decode.h"

#include "common.h"

using namespace std;

const char* cloud_server1 = "139.199.94.164";
const char* cloud_server2 = "139.199.165.244";
const char* cloud_server_port = SERVER_TO_RECEIVER_PORT;

void* recvdata(void*);
void* monitor(void*);


unordered_map<uint8_t, vector<char*>> buffer; 


int main(int argc, char* argv[])
{
   if (1 != argc)
   {
      cout << "usage: appclient filename" << endl;
      return 0;
   }

   UDTUpDown _udt_;

   UDTSOCKET client1, client2;
   createUDTSocket(client1, "9000");
   createUDTSocket(client2, "9001");

   connect(client1, cloud_server1, cloud_server_port);
   connect(client2, cloud_server2, cloud_server_port);
   
   // #ifndef WIN32
   //    pthread_create(new pthread_t, NULL, monitor, &client2);
   // #else
   //    CreateThread(NULL, 0, monitor, &client2, 0, NULL);
   // #endif

   #ifndef WIN32
      pthread_t rcvthread1, rcvthread2;

      cout << "creating thread1, recv from socket:" << client1 << endl;
      pthread_create(&rcvthread1, NULL, recvdata, new UDTSOCKET(client1));

      cout << "creating thread1, recv from socket:" << client1 << endl;
      pthread_create(&rcvthread2, NULL, recvdata, new UDTSOCKET(client2));

      pthread_join(rcvthread1, NULL); 
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

   int size = ENCODED_BLOCK_SIZE;
   int total = 0;

   while (true)
   {
      int rs;
      char* data = new char[size];

      // int rcv_size;
      // int var_size = sizeof(int);
      // UDT::getsockopt(recver, 0, UDT_RCVDATA, &rcv_size, &var_size);

      int rsize = 0;
      
      while(rsize < size){

         if (UDT::ERROR == (rs = UDT::recv(recver, data+rsize, size-rsize, 0)))
         {
            cout << "recv:" << UDT::getlasterror().getErrorMessage() << endl;
            break;
         }

         rsize += rs;

         total += rs;
         cout<< rs <<" bytes data rcvd"<<endl;
      }


      uint8_t seg_num;
      char* start = 0;
      cout<<"data: "<<data<<endl;
      if((start = strstr(data, "seg:")) != NULL){
          seg_num = *(start+4); 
          cout<< "form socket:" << recver << ", seg_num:" << int(seg_num)<<endl;
      }


      // vector<uint8_t> decode_data;
      // decode((uint8_t*)data, decode_data, size); 
      // const char* data_decoded = (const char*)decode_data.data();
      // cout<<data_decoded<<endl;
	
      // cout<<"recvd data:"<<rs<<" bytes. "<<data<<endl;
   }


   // delete [] data;

   UDT::close(recver);

   #ifndef WIN32
      return NULL;
   #else
      return 0;
   #endif
}
