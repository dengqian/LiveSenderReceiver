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
#include <unordered_map> 
#include <iostream>
#include <fstream>

#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/ioctl.h>

#include <linux/videodev2.h>


#include "wqueue.h"
#include "kodo/encode.h"
#include "kodo/decode.h"
#include "./common.h"
#include "../src/common.h"
#include "hashlibpp.h"     //md5 lib
#define EPSILON 1e-3
//#define FIXED_BW
#define PORT 5004

using namespace std;

// servers for different connections
const char* cloud_server1 = "10.21.2.193";
const char* cloud_server2 = "192.168.1.2";

// server's listen port
const char* cloud_server_port = SENDER_TO_SERVER_PORT;

// send queue item
class item{
public:
    vector<uint8_t> data;
    // int begin;
    // int end;

    item(vector<uint8_t>& content, int start, int end){
		// vector<uint8_t> tmp(content.begin()+start, content.begin()+end);
		// data = tmp;
        data.insert(data.end(), content.begin()+start, content.begin()+end);
	}
	
    ~item(){}
};

// send queue for different connections, used to add data recvd from udp socket to queue
wqueue<item*> queue1;
wqueue<item*> queue2;

// send rate for different connnections
volatile double sendrate1 = 1;
volatile double sendrate2 = 1;

// pushdata arg struct
struct ARGS{
	UDTSOCKET usocket;
	wqueue<item*> *queue;
};

// pushdata routine for data sending thread, continuously pop item from queue and send out,
// if no data item in queue, blocked there. 
void* pushdata(void* args)
{
	ARGS arg = *(ARGS*)args;
	UDTSOCKET client = arg.usocket;
	wqueue<item *> *queue = arg.queue;
 
	unordered_map<int, int> m;
    int size = 0;
    uint64_t last_time = 0;

    uint32_t seg_num = 0;

    while(true){
        // if no item in queue, will be blocked.
        item* it = queue->pop_front();

        uint32_t snd_size = 0;
        int ss = 0;
        UDT::TRACEINFO perf;
        UDT::perfmon(client, &perf);
        last_time = CTimer::getTime();
        uint64_t cur_time = last_time; // / 1000 % 1000000;
        memcpy(&seg_num, it->data.data()+4, sizeof(int));

        // send data here
        while(snd_size < it->data.size()) {
     	  if (UDT::ERROR == (ss = UDT::send(client, \
                          (const char*)it->data.data()+snd_size, it->data.size()-snd_size, 0)))
           {
              cout << "send:" << UDT::getlasterror().getErrorMessage() << endl;
              return 0 ;
           }
           snd_size += ss;
           size += ss;
        }

        // monitor on connection performance
        UDT::perfmon(client, &perf);

        // calculate send rate for different connections
        if (queue == &queue1){
            sendrate1 = perf.mbpsBandwidth;
            //sendrate1 = snd_size * 8.0 / (time_used * 1000);
     	    cout << "rate1 : " << sendrate1 <<  endl;
            cout << "segment " << seg_num << "sent at time " << cur_time << endl;
     	}
        else if (queue == &queue2){
            sendrate2 = perf.mbpsBandwidth;
            //sendrate2 = snd_size * 8.0 / (time_used * 1000);
     	    cout << "rate2 : " << sendrate2 <<  endl;
            cout << "segment " << seg_num << "sent at time " << cur_time << endl;
        }
    }
    return 0;
}
    
int main(int argc, char* argv[])
{

    // UDT sockets for data sending 
    UDTSOCKET client1, client2;
    createUDTSocket(client1, "9000");
    createUDTSocket(client2, "9001");

    // establish connections
    connect(client1, cloud_server1, cloud_server_port);
    connect(client2, cloud_server2, cloud_server_port);
   
    // create data sending threads
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

    
	fstream out("segment_md5.txt", ios::out);
    
    // segment number base
    uint32_t seg_num = 0;
	int factor1 = 1, factor2 = 1;  //default trasmit data with 1:1

	char* buffer = new char[BLOCK_SIZE];
	int cnt = 1;
    // getStreamFromCam();
    
    // receive udp data from vlc  
    int sockfd,len;  
    struct sockaddr_in addr;  
    int addr_len = sizeof(struct sockaddr_in);  

    // establish udp socket
    if((sockfd=socket(AF_INET,SOCK_DGRAM,0))<0){  
        perror ("socket");  
        exit(1);  
    }  

    bzero ( &addr, sizeof(addr) );  
    addr.sin_family=AF_INET;  
    addr.sin_port=htons(PORT);  
    addr.sin_addr.s_addr=htonl(INADDR_ANY) ;  
    if (bind(sockfd, (struct sockaddr *)&addr, sizeof(addr))<0){  
        perror("connect");  
        exit(1);  
    }  

    // buffer to put all data recvd from udp socket in.
    vector<char> buf;
    int bufflen = 0;

    // segment buffer size
    const int size = SEGMENT_SIZE;
    char *databuff = new char[size];

    while(true){

        // while buffer length < size, recv from udp socket
        while(bufflen < size){
            len = recvfrom(sockfd, buffer, BLOCK_SIZE, 0 , (struct sockaddr *)&addr ,(socklen_t*)&addr_len);
            // write every segment's md5 to file	
		    hashwrapper *myWrapper = new md5wrapper();
		    try
		    {
		    	string hash_res = myWrapper->getHashFromString(buffer);
		    	out << hash_res << endl;
		    	cnt++;
		    }
		    catch(hlException &e)
		    {
		    	cerr << "Get Md5 Error" << endl; 
		    }
		    delete myWrapper;

            bufflen += len;
            buf.insert(buf.end(), buffer, buffer+len);
            cout << "recv from udp len:" << bufflen << endl;
        }
         
        // buffer size is enough for encoding, get one segment into databuff
        copy(buf.begin(), buf.begin()+size, databuff); 
        buf.erase(buf.begin(), buf.begin()+size);
        bufflen -= size ;
        				    
        // encode data, encoded data will be put into data_out
		vector<uint8_t> data_out;
		encode((uint8_t*) databuff, data_out, seg_num++);
        
        cout << "-----------------------------------------" << endl;
		cout << "segment number:" << seg_num -1 << endl;
        cout << "encoded data size:" << data_out.size() <<endl;

        // last segment has not sent yet, wait
	    while( sendrate1 <= EPSILON && sendrate1 >= -EPSILON && sendrate2 <= EPSILON && sendrate2 >= -EPSILON);

		// compute factor according to send rate monitored in last send phrase
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
		else{
			factor1 = factor2 = 1;
		}
		
		sendrate1 = sendrate2 = 0;
		#ifdef FIXED_BW
		factor1 = 2;
		factor2 = 1;
		#endif
		//cout << "factor1 : " << factor1 << endl;
		//cout << "factor2 : " << factor2 << endl;

        // calculate number of blocks should be added into the queues
		int num1 = int((double)ENCODED_BLOCK_NUM * ((double)factor1/(factor1+factor2))+0.5);
		cout << "block num of link1 : " << num1 <<endl;
		int datasize1 = ENCODED_BLOCK_SIZE * num1;	
		int datasize2 = ENCODED_BLOCK_SIZE * (ENCODED_BLOCK_NUM - num1);
		queue1.add(new item(data_out, 0, datasize1));
		queue2.add(new item(data_out, datasize1, datasize1+datasize2));  
    }
    
	out.close(); 
    UDT::close(client1);
    UDT::close(client2);

    delete databuff;
    delete buffer;

    return 0;
}

