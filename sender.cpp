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
#define FIXED_BW

using namespace std;

const char* cloud_server1 = "10.21.2.193";
const char* cloud_server2 = "192.168.1.2";
const char* cloud_server_port = SENDER_TO_SERVER_PORT;

class item{
public:
    const char * data;
    int begin;
    int end;

    item(const char* dat, int b, int e):data(dat),begin(b),end(e){}
    ~item(){}
};

wqueue<item*> queue1;
wqueue<item*> queue2;

volatile double sendrate1 = -1.0;
volatile double sendrate2 = -1.0;

struct ARGS{
	UDTSOCKET usocket;
	wqueue<item*> *queue;
};

void* pushdata(void* args)
{
	ARGS arg = *(ARGS*)args;
	UDTSOCKET client = arg.usocket;
	wqueue<item *> *queue = arg.queue;
 
   int size = 0;
   const char* data_addr = queue->front()->data;
   while(true){
       item* it = queue->pop_front();
       int snd_size = 0;
       int ss = 0;
	   //UDT::TRACEINFO perf;
	   //UDT::perfmon(client, &perf);
	   uint64_t last_time = CTimer::getTime();
       while(snd_size < it->end-it->begin) {
         cout << "data size : " << it->end - it->begin <<endl; 
		  if (UDT::ERROR == (ss = UDT::send(client, \
                         it->data+it->begin+snd_size, it->end-it->begin-snd_size, 0)))
          {
             cout << "send:" << UDT::getlasterror().getErrorMessage() << endl;
             return 0 ;
          }
		  cout << "snd_size : " << ss <<endl;
          snd_size += ss;
          size += ss;
       }
	   uint64_t time_used = CTimer::getTime() - last_time;
	   cout << "send  data completed." << endl;
	   //UDT::perfmon(client, &perf);
	   if (queue == &queue1){
	       //sendrate1 = perf.mbpsSendRate;
		   
	       sendrate1 = snd_size * 8.0 / (time_used * 1000);
		   cout << "sendrate1 : " << sendrate1 << endl;
		}
	   else if (queue == &queue2){
	       //sendrate2 = perf.mbpsSendRate;
	       sendrate2 = snd_size * 8.0 / (time_used * 1000);
		   cout << "sendrate2 : " << sendrate2 << endl;
	   }
   }
   return 0;
}



int main(int argc, char* argv[])
{
   if (2 != argc)
   {
      cout << "usage: sender filename" << endl;
      return 0;
   }

   UDTSOCKET client1, client2;
   createUDTSocket(client1, "9000");
   createUDTSocket(client2, "9001");

   connect(client1, cloud_server1, cloud_server_port);
   connect(client2, cloud_server2, cloud_server_port);
   
   #ifndef WIN32
		ARGS arg1, arg2;
		arg1.usocket = client1;
		arg1.queue = &queue1;
		arg2.usocket = client2;
		arg2.queue = &queue2;	
		pthread_t thread1, thread2;
        pthread_create(&thread1, NULL, pushdata, &arg1);
        pthread_create(&thread2, NULL, pushdata, &arg2);
		pthread_detach(thread1);
		pthread_detach(thread2);
   #else
      CreateThread(NULL, 0, monitor, &client2, 0, NULL);
   #endif

    
    fstream in(argv[1], ios::in | ios::binary);

    const int size = SEGMENT_SIZE;
    uint32_t seg_num = 0;
    bool first_test = true;
	int factor1 = 1, factor2 = 1;  //default trasmit data with 1:1

	char* buffer = new char[size];
    vector<uint8_t> data_out;
    while(!in.fail() && !in.eof()) {
        in.read(buffer, size); 

		while( sendrate1 <= EPSILON && sendrate1 >= -EPSILON && sendrate2 <= EPSILON && sendrate2 >= -EPSILON);
        data_out.clear();
		encode((uint8_t*) buffer, data_out, size, seg_num++);
        const char* data = (const char*)data_out.data();
		cout << "-----------------------------------------" << endl;
		// cout << in.tellg() << ' ' << in.fail() << ' ' << in.eof() << endl;
        cout<<data_out.size()<<" bytes data encoded!"<<endl;
        cout<<"encoded_block_size:" << ENCODED_BLOCK_SIZE <<' '<< data_out.data()<<endl;		
		//to do; compute factor
		if ( sendrate1 > EPSILON && sendrate2 > EPSILON) {
		    double res = sendrate1 > sendrate2 ? sendrate1 / sendrate2 : sendrate2 / sendrate1;
			if (sendrate1 > sendrate2){
				factor1 = (int)(res + 0.5);
				factor2 = 1;
				if (factor1 > 10){
					factor1 = BLOCK_NUM; 
					factor2 = ENCODED_BLOCK_NUM - BLOCK_NUM;   // the mbps gap is too large , so transmit on only one link, the other link only transmit the redundant data
				}
			}
			else{
				factor1 = 1;
				factor2 = (int)(res + 0.5);
				if (factor2 > 10){
					factor1 = ENCODED_BLOCK_NUM - BLOCK_NUM;
					factor2 = BLOCK_NUM;
				}
			}
		}
		else if (sendrate1 <= EPSILON && sendrate2 <= EPSILON){
			factor1 = factor2 = 1;
		}
		else if (sendrate1 <= EPSILON){
			factor1 = 1;
		}
		else {
			factor2 = 1;
		}
		sendrate1 = sendrate2 = 0;
		#ifdef FIXED_BW
		factor1 = 2;
		factor2 = 1;
		#endif
		cout << "factor1 : " << factor1 << endl;
		cout << "factor2 : " << factor2 << endl;
		if (sendrate1 < -EPSILON && sendrate2 < -EPSILON){
			queue1.add(new item(data, 0, SEGMENT_SIZE));
			queue2.add(new item(data, 0, SEGMENT_SIZE));  
		}
		else {
			int num1 = ENCODED_BLOCK_NUM * ((double)factor1/(factor1+factor2));
			cout << "block num of link1 : " << num1 <<endl;
			int datasize1 = ENCODED_BLOCK_SIZE * num1;	
			int datasize2 = ENCODED_BLOCK_SIZE * (ENCODED_BLOCK_NUM - num1);
			queue1.add(new item(data, 0, datasize1));
			queue2.add(new item(data, datasize1, datasize1+datasize2));  
		}
		
		// first sending is a link-rate test
		if ( first_test ){
			first_test = false;
			in.seekg(0, ios::beg);
		}
    }

    in.close();
   
   UDT::close(client1);
   UDT::close(client2);

   // delete [] data;
   return 0;
}

