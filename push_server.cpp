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

#include <vector>
#include <iostream>
#include <udt.h>

#include "wqueue.h"

#include "cc.h"
#include "test_util.h"
// #include "common.h"

using namespace std;

class item{
public:
    char * data;
    int begin;
    int end;

    item(char* dat, int b, int e):data(dat),begin(b),end(e){}
    ~item(){}
};

// #include "common.h"
// test

UDTSOCKET receiver_sock;
vector<UDTSOCKET> regist_sock_list;
UDTSOCKET regist_sock;
wqueue<item*> queue;

int buff_size = 10240;


void* recvdata(void *);
void* pushdata(void *);

int listen_to_client(const char* port, UDTSOCKET& server)
{
   addrinfo hints;
   addrinfo* res;

   memset(&hints, 0, sizeof(struct addrinfo));

   hints.ai_flags = AI_PASSIVE;
   hints.ai_family = AF_INET;
   hints.ai_socktype = SOCK_STREAM;

   if (0 != getaddrinfo(NULL, port, &hints, &res))
   {
      cout << "illegal port number or port is busy.\n" << endl;
      return 0;
   }

   server = UDT::socket(res->ai_family, res->ai_socktype, res->ai_protocol);

   if (UDT::ERROR == UDT::bind(server, res->ai_addr, res->ai_addrlen))
   {
      cout << "bind: " << UDT::getlasterror().getErrorMessage() << endl;
      return 0;
   }

   freeaddrinfo(res);

   cout << "server listen to client at port:" << port << endl;

   if (UDT::ERROR == UDT::listen(server, 10))
   {
      cout << "listen: " << UDT::getlasterror().getErrorMessage() << endl;
      return 0;
   }

   return 0;

}

// void* accept_viewer(void* usocket){
void* accept_viewer(UDTSOCKET client){


   sockaddr_storage clientaddr;
   int addrlen = sizeof(clientaddr);

   while (true)
   {
      if (UDT::INVALID_SOCK == (regist_sock = UDT::accept(client, (sockaddr*)&clientaddr, &addrlen)))
      {
         cout << "accept: " << UDT::getlasterror().getErrorMessage() << endl;
         return 0;
      }

      regist_sock_list.push_back(regist_sock);
      
    
      char clienthost[NI_MAXHOST];
      char clientservice[NI_MAXSERV];
      getnameinfo((sockaddr *)&clientaddr, addrlen, clienthost, sizeof(clienthost), clientservice, sizeof(clientservice), NI_NUMERICHOST|NI_NUMERICSERV);
      cout << "new viewer: " << clienthost << ":" << clientservice << endl;

      #ifndef WIN32
         pthread_t pushthread;
         pthread_create(&pushthread, NULL, pushdata, new UDTSOCKET(regist_sock));
         pthread_join(pushthread, NULL);
      #else
         CreateThread(NULL, 0, pushdata, new UDTSOCKET(regist_sock), 0, NULL);
      #endif
   }

    
}


int receive_from_client(UDTSOCKET serv)
{
   sockaddr_storage clientaddr;
   int addrlen = sizeof(clientaddr);

   // while (true)
   // {
   if (UDT::INVALID_SOCK == (receiver_sock = UDT::accept(serv, (sockaddr*)&clientaddr, &addrlen)))
   {
      cout << "accept: " << UDT::getlasterror().getErrorMessage() << endl;
      return 0;
   }

   char clienthost[NI_MAXHOST];
   char clientservice[NI_MAXSERV];
   getnameinfo((sockaddr *)&clientaddr, addrlen, clienthost, sizeof(clienthost), clientservice, sizeof(clientservice), NI_NUMERICHOST|NI_NUMERICSERV);
   cout << "new connection: " << clienthost << ":" << clientservice << endl;

   #ifndef WIN32
      pthread_t rcvthread;
      pthread_create(&rcvthread, NULL, recvdata, new UDTSOCKET(receiver_sock));
      pthread_detach(rcvthread);
   #else
      CreateThread(NULL, 0, recvdata, new UDTSOCKET(receiver_sock), 0, NULL);
   #endif

   // }

   UDT::close(serv);

   return 0;
}


#ifndef WIN32
void* recvdata(void* usocket)
#else
DWORD WINAPI recvdata(LPVOID usocket)
#endif
{
   UDTSOCKET client = *(UDTSOCKET*)usocket;
   delete (UDTSOCKET*)usocket;

   

   while (true)
   {
      int rsize = 0;
      int rs;
      char* data = new char[buff_size];

      while (rsize < buff_size)
      {
         int rcv_size;
         int var_size = sizeof(int);
         UDT::getsockopt(receiver_sock, 0, UDT_RCVDATA, &rcv_size, &var_size);
         if (UDT::ERROR == (rs = UDT::recv(client, data + rsize, buff_size - rsize, 0)))
         {
            cout << "recv:" << UDT::getlasterror().getErrorMessage() << endl;
            break;
         }

         // send data directly.
         // int snd_size = 0;
         // int ss;
         // for(auto regist:regist_sock_list)
         // {
         // if(regist_sock_list.size() != 0)
         // { 
         //    while(snd_size < rs) {
         //       
         //       if (UDT::ERROR == (ss = UDT::send(regist_sock, \
         //                       data + rsize + snd_size, rs - snd_size, 0)))
         //       {
         //          cout << "send:" << UDT::getlasterror().getErrorMessage() << endl;
         //          regist_sock_list.pop_back();
         //          break;
         //       }
         //       snd_size += ss;
         //    }
         //    cout<<rs<<"bytes data pushed"<<endl;
         // }

         // add to shared buffer.
         queue.add(new item(data, rsize, rsize+rs));
        
         rsize += rs;
      }


      if (rsize < buff_size)
         break;
   }

   UDT::close(client);

   #ifndef WIN32
      return NULL;
   #else
      return 0;
   #endif
}

void* pushdata(void* usocket)
{
   UDTSOCKET client = *(UDTSOCKET*)usocket;
   delete (UDTSOCKET*)usocket;

   int size = 0;
   int s=queue.size();

   int buffer_block_size = 300;
   char* data_addr = queue.front()->data;

   if(s > buffer_block_size)
   {
       // cout<<"initial size:"<<s<<endl;
       for(int i=0; i<s-buffer_block_size; i++){
           item* it = queue.pop_front();

           if(it->data != data_addr){
                char* data_pushed = data_addr;
                delete [] data_pushed;
                data_addr = it->data;
           }
       }
   }

   while(true){

       item* it = queue.pop_front();

       if(it->data != data_addr){
           cout<< "queue size:" << queue.size() << " " <<size<<" bytes data pushed" <<endl;
           char* data_pushed = data_addr;
           delete [] data_pushed;
           data_addr = it->data;

       }

       int snd_size = 0;
       int ss = 0;

       while(snd_size < it->end-it->begin) {
          
          if (UDT::ERROR == (ss = UDT::send(client, \
                         it->data+it->begin+snd_size, it->end-it->begin-snd_size, 0)))
          {
             cout << "send:" << UDT::getlasterror().getErrorMessage() << endl;
             regist_sock_list.pop_back();
             return 0 ;
          }
          snd_size += ss;
          size += ss;
       }
   }

   return 0;

}


int main(int argc, char* argv[]){

    const char* upload_port = "9090";
    const char* client_port = "9000";

    UDTSOCKET listen_to_uploader;
    UDTSOCKET listen_to_regist;

    listen_to_client(upload_port, listen_to_uploader);
    listen_to_client(client_port, listen_to_regist);

    // #ifndef WIN32
    //    pthread_t accept_thread;
    //    pthread_create(&accept_thread, NULL, accept_viewer, new UDTSOCKET(listen_to_regist));
    //    pthread_detach(accept_thread);
    // #else
    //    CreateThread(NULL, 0, accept_viewer, new UDTSOCKET(listen_to_regist), 0, NULL);
    // #endif

    receive_from_client(listen_to_uploader);
    accept_viewer(listen_to_regist);

    return 0;
}
