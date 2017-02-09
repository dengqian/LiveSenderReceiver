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
#include <map>

#include "kodo/decode.h"

#include "common.h"

using namespace std;

const char* cloud_server1 = "139.199.94.164";
const char* cloud_server2 = "139.199.165.244";
const char* cloud_server_port = SERVER_TO_RECEIVER_PORT;

void* recvdata(void*);
// void* monitor(void*);

// received data will be put into a map, with key seg_num, value block data char*.
// when the number of data blocks is enough for decode, do decoding.
class recv_data{
public:
    vector<char*> data;
    const char* decoded_data;
    int data_size;

public:
    recv_data() {
        decoded_data = 0;
    }

    ~recv_data(){
        for(auto it : data){
            delete [] it;
        }
        delete [] decoded_data;
    }

    void push_back(char* item){
        data.push_back(item);
    }

    int size(){
        return data.size();
    }

    void decoding(){

        data_size = data.size();

        if (data_size < SEGMENT_SIZE / BLOCK_SIZE) return;

        int length = data_size * ENCODED_BLOCK_SIZE;
        
        char* data_in = new char[length];
        strcpy(data_in, data[0]);

        for(int i=1; i<data_size; i++){
            strcat(data_in, data[i]); 
        }

        vector<uint8_t> data_out;
        decode((uint8_t*)data_in, data_out, data_size); 
        decoded_data = (const char*)data_out.data();

        data.clear();
        delete [] data_in;
    }

};


map<uint8_t, recv_data>buffer; 


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

      cout << "creating thread2, recv from socket:" << client2 << endl;
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
          buffer[seg_num].push_back(start+5);
          if(buffer[seg_num].size() == SEGMENT_SIZE / BLOCK_SIZE) {
              buffer[seg_num].decoding();
              cout << "seg " << int(seg_num) << ":" << buffer[seg_num].data_size \
                  <<" blocks,"<<buffer[seg_num].decoded_data << endl;
          }
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
