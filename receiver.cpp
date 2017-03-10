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
#include "hashlibpp.h"
#include "common.h"
#include "../src/common.h"

using namespace std;

const char* cloud_server1 = "10.21.2.193";
const char* cloud_server2 = "10.21.2.251";
// const char* cloud_server1 = "139.199.94.164";
// const char* cloud_server2 = "139.199.165.244";
const char* cloud_server_port = SERVER_TO_RECEIVER_PORT;
fstream outfile;

void* recvdata(void*);
// void* monitor(void*);

// received data will be put into a map, with key seg_num, value block data char*.
// when the number of data blocks is enough for decode, do decoding.
class rcvdDataItem{
public:
    pthread_mutex_t  m_mutex;
    vector<char*> data;
    int isDecoded;

public:
    rcvdDataItem() {
        pthread_mutex_init(&m_mutex, NULL);
        isDecoded = 0;
    }

    ~rcvdDataItem(){
        pthread_mutex_destroy(&m_mutex);
        for(auto it : data){
            delete it;
        }
    }

    void push_back(char* item){
        pthread_mutex_lock(&m_mutex);
        data.push_back(item);
        pthread_mutex_unlock(&m_mutex);
    }

    int size(){
        pthread_mutex_lock(&m_mutex);
        int size = data.size();
        pthread_mutex_unlock(&m_mutex);
        return size; 
    }

	int decoding();

};

int rcvdDataItem::decoding(){

    pthread_mutex_lock(&m_mutex);
    if(isDecoded) {
        // cout << "data has been decoded!" << endl;
        pthread_mutex_lock(&m_mutex);
        return 1;
    }

    int data_size = data.size();
    vector<uint8_t> data_out;
    isDecoded = decode(data, data_out, data_size); 
    // cout << "decode status:" << isDecoded << endl;
    pthread_mutex_unlock(&m_mutex);

    if(!isDecoded) return 0;
    

    hashwrapper *myWrapper = new md5wrapper();
	try
	{
		string hash_res = myWrapper->getHashFromString((const char*)data_out.data());
		outfile << hash_res << endl;
	}
	catch(hlException &e)
	{
		cerr << "Get Md5 Error" << endl; 
	}
	delete myWrapper; 

    return 1;
}



map<uint32_t, rcvdDataItem> buffer; 


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
   outfile.open("recv_md5.txt", ios::out);
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
   outfile.close();
   UDT::close(client1);
   UDT::close(client2);

   return 0;
}

void print_mapinfo(){
    for(auto it : buffer){
        cout << it.first << ' ' << it.second.size() << ' ' << endl;
    }
}

#ifndef WIN32
void* recvdata(void* usocket)
#else
DWORD WINAPI recvdata(LPVOID usocket)
#endif
{
   UDTSOCKET recver = *(UDTSOCKET*)usocket;
   delete (UDTSOCKET*)usocket;

   const int size = ENCODED_BLOCK_SIZE;

   while (true)
   {
      int rs;
      char* data = new char[size];

      int rsize = 0;
      
      while(rsize < size){

         if (UDT::ERROR == (rs = UDT::recv(recver, data+rsize, size-rsize, 0)))
         {
            cout << "recv:" << UDT::getlasterror().getErrorMessage() << endl;
            // print_mapinfo();
            return 0;
         }

         rsize += rs;
      }

      cout << "------------------------------------------" << endl;
      // cout<< "recved " << rsize <<" bytes data."<< endl;

      uint32_t seg_num;
      char* start = 0;

      if((start = strstr(data, "seg:")) != NULL){

          memcpy(&seg_num, start + 4, sizeof(uint32_t));
          buffer[seg_num].push_back(data);
          
          // cout<< "from socket:" << recver << ", seg_num:" << seg_num << ' ' << \
              buffer[seg_num].size() << endl;

          if(buffer[seg_num].size() >= BLOCK_NUM && buffer[seg_num].isDecoded==0) {
          // if(buffer[seg_num].size() == BLOCK_NUM){
              
              // cout<< "decoding segment: " << seg_num <<endl;
              int decodeStatus = buffer[seg_num].decoding();
              if(decodeStatus == 1) {
                  uint64_t cur_time = CTimer::getTime() / 1000 % 1000000;
                  cout << "seg " << seg_num << " decoded at time: " << cur_time << endl;
              }
          }

          cout<<endl;
      }
      else{
        cout << "seg: not found " << data << endl;
      }

   }



   UDT::close(recver);

   #ifndef WIN32
      return NULL;
   #else
      return 0;
   #endif
}
