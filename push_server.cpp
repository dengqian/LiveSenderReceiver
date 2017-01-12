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
#include <queue>

#include "cc.h"
#include "test_util.h"

using namespace std;


// #include "common.h"

UDTSOCKET receiver_sock;
vector<UDTSOCKET> regist_sock_list;
UDTSOCKET regist_sock;

void* recvdata(void *);

int listent_to_client(const char* port, UDTSOCKET& server)
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

   cout << "server listent to client at port:" << port << endl;

   if (UDT::ERROR == UDT::listen(server, 10))
   {
      cout << "listen: " << UDT::getlasterror().getErrorMessage() << endl;
      return 0;
   }

   return 0;

}

void* accept_viewer(void* usocket){

   UDTSOCKET client = *(UDTSOCKET*)usocket;
   delete (UDTSOCKET*)usocket;

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

      // #ifndef WIN32
      //    pthread_t pushthread;
      //    pthread_create(&pushthread, NULL, pushdata, new UDTSOCKET(regist));
      //    pthread_detach(pushthread);
      // #else
      //    CreateThread(NULL, 0, pushdata, new UDTSOCKET(regist), 0, NULL);
      // #endif
   }

    
}


int receive_from_client(UDTSOCKET serv)
{
   sockaddr_storage clientaddr;
   int addrlen = sizeof(clientaddr);

   while (true)
   {
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
   }

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

   int size = 10240;
   char* data = new char[size];
   

   while (true)
   {
      int rsize = 0;
      int rs;

      while (rsize < size)
      {
         int rcv_size;
         int var_size = sizeof(int);
         UDT::getsockopt(receiver_sock, 0, UDT_RCVDATA, &rcv_size, &var_size);
         if (UDT::ERROR == (rs = UDT::recv(client, data + rsize, size - rsize, 0)))
         {
            cout << "recv:" << UDT::getlasterror().getErrorMessage() << endl;
            break;
         }
         rsize += rs;

         int snd_size = 0;
         int ss;
         // for(auto regist:regist_sock_list)
         // {
         if(regist_sock_list.size() != 0)
         { 
            while(snd_size < rs) {
               
               if (UDT::ERROR == (ss = UDT::send(regist_sock, \
                               data + rsize + snd_size, rs - snd_size, 0)))
               {
                  cout << "send:" << UDT::getlasterror().getErrorMessage() << endl;
                  regist_sock_list.pop_back();
                  break;
               }
               snd_size += ss;
            }
            // cout<<rs<<"bytes data pushed"<<endl;
         } 
        
      }


      // add to shared buffer.
      // buffer.add(item);
	
      if (rsize < size)
         break;
   }

   UDT::close(client);

   #ifndef WIN32
      return NULL;
   #else
      return 0;
   #endif
}


int main(int argc, char* argv[]){

    const char* upload_port = "9090";
    const char* client_port = "9000";

    UDTSOCKET listent_to_uploader;
    UDTSOCKET listent_to_regist;

    listent_to_client(upload_port, listent_to_uploader);
    listent_to_client(client_port, listent_to_regist);

    #ifndef WIN32
       pthread_t accept_thread;
       pthread_create(&accept_thread, NULL, accept_viewer, new UDTSOCKET(listent_to_regist));
       pthread_detach(accept_thread);
    #else
       CreateThread(NULL, 0, accept_viewer, new UDTSOCKET(listent_to_regist), 0, NULL);
    #endif

    receive_from_client(listent_to_uploader);

    return 0;
}
