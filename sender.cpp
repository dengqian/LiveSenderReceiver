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
#include <fstream>

// #include "cc.h"
// #include "test_util.h"

#include "kodo/encode.h"

#include "common.h"

using namespace std;

// const char* cloud_server1 = "10.21.2.193";
// const char* cloud_server2 = "192.168.1.2";
const char* cloud_server_port = SENDER_TO_SERVER_PORT;


// void* monitor(void*);

int main(int argc, char* argv[])
{
   if (2 != argc)
   {
      cout << "usage: appclient filename" << endl;
      return 0;
   }

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

    
    fstream in(argv[1], ios::in | ios::binary);

    const int size = SEGMENT_SIZE;
    char* buffer = new char[size];
    uint8_t seg_num = 0;

    while(!in.eof()){

        in.read(buffer, size); 

        vector<uint8_t> data_out;
        encode((uint8_t*) buffer, data_out, size, seg_num++);
        const char* data = (const char*)data_out.data();
        cout<<data_out.size()<<" bytes data encoded!"<<endl;
        cout<<"encoded_block_size:" << ENCODED_BLOCK_SIZE <<' '<< data_out.data()<<endl;

        int send_size = data_out.size();
        int ssize = 0;
        int ss;
        while (ssize < send_size)
        {
           if (UDT::ERROR == (ss = UDT::send(client1, data + ssize, ENCODED_BLOCK_SIZE*2, 0)))
           {
              cout << "send:" << UDT::getlasterror().getErrorMessage() << endl;
              return 0;
           }

           ssize += ss;

           if(ssize >= send_size) 
               break;

           if (UDT::ERROR == (ss = UDT::send(client2, data + ssize, ENCODED_BLOCK_SIZE, 0)))
           {
              cout << "send:" << UDT::getlasterror().getErrorMessage() << endl;
              return 0;
           }
           ssize += ss;
        }

        if (ssize < send_size)
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

