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

#include "wqueue.h"
// #include "cc.h"
// #include "test_util.h"
#include "kodo/encode.h"
#include "./common.h"
#include "../src/common.h"
#define EPSILON 1e-3
//#define FIXED_BW

using namespace std;

const char* cloud_server1 = "10.21.2.193";
const char* cloud_server2 = "192.168.1.2";
const char* cloud_server_port = SENDER_TO_SERVER_PORT;

class item{
public:
    char * data;
    int begin;
    int end;

    item(char* dat, int b, int e):data(dat),begin(b),end(e){}
    ~item(){}
};

wquque<item*> queue1;
wquque<item*> queue2;

void* pushdata(void* usocket)
{
   UDTSOCKET client = *(UDTSOCKET*)usocket;
   delete (UDTSOCKET*)usocket;

   int size = 0;
   int s=queue.size();

   char* data_addr = queue.front()->data;

   // init buffer block size can't exceed buffer_block_size.
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

       // pointer changed, delete data from queue.
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
   
   // test connection bandwidth
   UDT::TRACEINFO perf1;
   UDT::TRACEINFO perf2;
   // #ifndef WIN32
   //    pthread_create(new pthread_t, NULL, monitor, &client2);
   // #else
   //    CreateThread(NULL, 0, monitor, &client2, 0, NULL);
   // #endif

    
    fstream in(argv[1], ios::in | ios::binary);

    const int size = SEGMENT_SIZE;
    char* buffer = new char[size];
    uint32_t seg_num = 0;
    bool first_test = true;
	vector<int> factor(2, 1);  //default trasmit data with 1:1
    while(!in.eof()){
        in.read(buffer, size); 

        vector<uint8_t> data_out;
        encode((uint8_t*) buffer, data_out, size, seg_num++);
        const char* data = (const char*)data_out.data();
		cout << "-----------------------------------------" << endl;
        cout<<data_out.size()<<" bytes data encoded!"<<endl;
        cout<<"encoded_block_size:" << ENCODED_BLOCK_SIZE <<' '<< data_out.data()<<endl;

		for(int i=0; i<2; i++)
			cout << "factor " << i << " is :" << factor[i] << endl;
		vector<double> sendrate(2, 0);
        int send_size = data_out.size();
        int ssize = 0;
        int ss;
		uint64_t perf1_time = 0;
		uint64_t perf2_time = 0;
		int ss1 = 0, ss2 = 0;	
        while (ssize < send_size)
        {
		   uint64_t last_time = CTimer::getTime();   // in ms
           if (UDT::ERROR == (ss = UDT::send(client1, data + ssize, ENCODED_BLOCK_SIZE*factor[0], 0)))
           {
              cout << "send:" << UDT::getlasterror().getErrorMessage() << endl;
              return 0;
           }
		   perf1_time += CTimer::getTime() - last_time;
		   ssize += ss;
		   ss1 += ss;
		   if ( first_test )
				ssize -= ss; 
		   if(ssize >= send_size) 
               break;
			
		   last_time = CTimer::getTime();	
           if (UDT::ERROR == (ss = UDT::send(client2, data + ssize, ENCODED_BLOCK_SIZE*factor[1], 0)))
           {
              cout << "send:" << UDT::getlasterror().getErrorMessage() << endl;
              return 0;
           }	
		   perf2_time += CTimer::getTime() - last_time;
		   ssize += ss;
		   ss2 += ss;
        }
		UDT::perfmon(client1, &perf1);
		cout << "1 : pkt sent : " << perf1.pktSent << endl;
		cout << "1 : data size : " << ss1 << endl;
		cout << "1 : perf time: " << perf1_time << endl;
		if (perf1_time > 0)
			//sendrate[0] = double(perf1.pktSent) * 1456 *8.0 / (perf1_time * 1000) ;
			sendrate[0] = double(ss1)*8.0 / (perf1_time * 1000) ;
		else {      //if no data has been sent, perf_time will be zero
			sendrate[0] = 0;
		}
		//cout << "send duration : " << perf1_time << endl;  
		
		UDT::perfmon(client2, &perf2);
		cout << "2 : pkt sent : " << perf2.pktSent << endl;
		cout << "2 : data size : " << ss2 << endl;
		cout << "2 : perf time: " << perf2_time << endl;

		if (perf2_time > 0)
			//sendrate[1] = double(perf2.pktSent) * 1456 *8.0 / (perf2_time * 1000);
			sendrate[1] = double(ss2)*8.0 / (perf2_time * 1000) ;
        else {
			sendrate[1] = 0;
		}
			
		if (ssize < send_size)
           break;
        cout<<"segment sent completed"<<endl;
		//compute the factor
		cout << "rate 0 : " << sendrate[0] << "    rate 1 : " << sendrate[1] << endl;
		// first test
		if ( first_test ){
			first_test = false;
			in.seekg(0, ios::beg);
		}
		double res = 0;
		if ( sendrate[0] > EPSILON && sendrate[1] > EPSILON) 
		    res = sendrate[0] > sendrate[1] ? sendrate[0]/sendrate[1] : sendrate[1] / sendrate[0];
		else if (sendrate[0] <= EPSILON && sendrate[1] <= EPSILON){
			factor[0] = factor[1] = 1;
			continue;
		}
		else if (sendrate[0] <= EPSILON){
			factor[0] = 1;
			continue;
		}
		else {
			factor[1] = 1;
			continue;
		}
		if (sendrate[0] > sendrate[1]){
			factor[0] = (int)(res + 0.5);
			factor[1] = 1;
			if (factor[0] > 10){
				factor[0] = 5;
				factor[1] = 0;   // the mbps gap is too large , so transmit on only one link
			}
		}
		else{
			factor[0] = 1;
			factor[1] = (int)(res + 0.5);
			if (factor[1] > 10){
				factor[0] = 0;
				factor[1] = 5;
			}
		}
		#ifdef FIXED_BW
		factor[0] = 2;
		factor[1] = 1;
		#endif
	//	for(int i=0; i<2; i++)
	//		cout << "factor " << i << " is :" << factor[i] << endl;
		
    }
   
    in.close();
   
   UDT::close(client1);
   UDT::close(client2);

   // delete [] data;
   return 0;
}

