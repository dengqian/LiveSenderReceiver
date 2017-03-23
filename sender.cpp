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
// #include "cc.h"
// #include "test_util.h"
#include "kodo/encode.h"
#include "kodo/decode.h"
#include "./common.h"
#include "../src/common.h"
#include "hashlibpp.h"     //md5 lib
#define EPSILON 1e-3
//#define FIXED_BW

using namespace std;

const char* cloud_server1 = "10.21.2.193";
const char* cloud_server2 = "192.168.1.2";
// const char* cloud_server1 = "139.199.94.164";
// const char* cloud_server2 = "139.199.165.244";
const char* cloud_server_port = SENDER_TO_SERVER_PORT;

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

wqueue<item*> queue1;
wqueue<item*> queue2;

volatile double sendrate1 = 1;
volatile double sendrate2 = 1;

struct ARGS{
	UDTSOCKET usocket;
	wqueue<item*> *queue;
};

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
       item* it = queue->pop_front();
       uint32_t snd_size = 0;
       int ss = 0;
	   UDT::TRACEINFO perf;
	   UDT::perfmon(client, &perf);
       last_time = CTimer::getTime();
       uint64_t cur_time = last_time;// / 1000 % 1000000;
       memcpy(&seg_num, it->data.data()+4, sizeof(int));	

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
	   //uint64_t time_used = CTimer::getTime() - last_time;
	   UDT::perfmon(client, &perf);
	   if (queue == &queue1){
	       sendrate1 = perf.mbpsBandwidth;
	       //sendrate1 = snd_size * 8.0 / (time_used * 1000);
		   // cout<<"link 1 :" << seg_num<< ' ' << m[seg_num] << ' ' << queue->size() << endl;
		   cout << "rate1 : " << sendrate1 <<  endl;
           cout << "segment " << seg_num << "sent at time " << cur_time << endl;
		}
	   else if (queue == &queue2){
	       sendrate2 = perf.mbpsBandwidth;
	       //sendrate2 = snd_size * 8.0 / (time_used * 1000);
		   // cout<<"link 2 :" << seg_num<< ' ' << m[seg_num] << ' ' << queue->size() << endl;
		   cout << "rate2 : " << sendrate2 <<  endl;
           cout << "segment " << seg_num << "sent at time " << cur_time << endl;
	   }
   }
   return 0;
}
static int xioctl(int fd, int request, void *arg)
{
    int r;
    do r = ioctl (fd, request, arg);
    while (-1 == r && EINTR == errno);
    return r;
}

int getStreamFromCam(){
    int fd;
    fd = open("/dev/video0", O_RDWR);
    if (fd == -1)
    {
        // couldn't find capture device
        perror("Opening Video device");
        return 1;
    }
    struct v4l2_capability caps = {0};
    if (-1 == xioctl(fd, VIDIOC_QUERYCAP, &caps))
    {
        perror("Querying Capabilites");
        return 1;
    }
    if(!(caps.capabilities & V4L2_CAP_VIDEO_CAPTURE)){
        fprintf(stderr, "The device does not handle single-planar video capture.\n");
        exit(1);
    }

    // set format
#ifndef V4L2_PIX_FMT_H264
#define V4L2_PIX_FMT_H264     v4l2_fourcc('H', '2', '6', '4') /* H264 with start codes */
#endif
    struct v4l2_format fmt;
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width       = 640; //replace
    fmt.fmt.pix.height      = 480; //replace
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_H264; //replace
    if(ioctl(fd, VIDIOC_S_FMT, &fmt) < 0){
        perror("VIDIOC_S_FMT");
        exit(1);
    }
    // request buffers 
    struct v4l2_requestbuffers bufrequest;
    bufrequest.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    bufrequest.memory = V4L2_MEMORY_MMAP;
    bufrequest.count = 4;
     
    if(ioctl(fd, VIDIOC_REQBUFS, &bufrequest) < 0){
        perror("VIDIOC_REQBUFS");
        exit(1);
    }

    // map cache buffer to bufferinfo
   
    struct v4l2_buffer buf;  
    memset(&buf,0,sizeof(buf));  
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;  
    buf.memory = V4L2_MEMORY_MMAP;  
    buf.index = 0;  
    if (-1 == ioctl(fd, VIDIOC_QUERYBUF, &buf))  
        exit(-1);  

    // 映射内存  
    void* buffer_start = mmap (NULL,buf.length,PROT_READ | PROT_WRITE ,MAP_SHARED,fd, buf.m.offset);  
    if (MAP_FAILED==buffer_start){
        perror("mmap");
        exit(-1);  
    }

    struct v4l2_buffer bufferinfo;  
    bufferinfo.type =V4L2_BUF_TYPE_VIDEO_CAPTURE;  
    bufferinfo.memory =V4L2_MEMORY_MMAP;  
    bufferinfo.index = 1;  
    ioctl (fd,VIDIOC_QBUF, &bufferinfo);  
    
    int type =V4L2_BUF_TYPE_VIDEO_CAPTURE;  
    ioctl (fd,VIDIOC_STREAMON, &type);
    
    int cnt = 0;
    while(cnt++ < 100){
        if(ioctl(fd, VIDIOC_DQBUF, &bufferinfo) < 0){
            perror("VIDIOC_QBUF");
            exit(1);
        }
        
        bufferinfo.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        bufferinfo.memory = V4L2_MEMORY_MMAP;
             // 图像处理  
        int jpgfile;
        ofstream outfile ("new.mp4",ofstream::app);
        // if((jpgfile = open("/tmp/myimage.jpeg", O_WRONLY | O_CREAT, 0660)) < 0){
        //     perror("open");
        //     exit(1);
        // }
         
        outfile.write((const char*)buffer_start, bufferinfo.length);
        cout << (const char*) buffer_start << endl;
        // Queue the next one.
        if(ioctl(fd, VIDIOC_QBUF, &bufferinfo) < 0){
            perror("VIDIOC_QBUF");
            exit(1);
        }
    }
    // Deactivate streaming
    if(ioctl(fd, VIDIOC_STREAMOFF, &type) < 0){
        perror("VIDIOC_STREAMOFF");
        exit(1);
    }

 
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
	fstream out("segment_md5.txt", ios::out);
    const int size = SEGMENT_SIZE;
    uint32_t seg_num = 0;
    //bool first_test = true;
	int factor1 = 1, factor2 = 1;  //default trasmit data with 1:1

	char* buffer = new char[size];
	memset(buffer, 0, size);
	int cnt = 1;
    // getStreamFromCam();
    vector<char> buf;

    //while(!in.eof()){
    in.seekg(0, ios::end);
    int last_len = 0;
    int cur_len = in.tellg();
    //in.seekg(0, ios::beg);
    int new_len = cur_len-last_len;
    while(true){
        while(cur_len == last_len){
		    in.seekg(0, ios::end);
            cur_len = in.tellg(); 
        }
        new_len = cur_len - last_len;
        last_len = cur_len;
        cout << "new_len:" << new_len << endl;

        in.seekg(-new_len, ios::cur);
        char* tmp_buf = new char[new_len];
        in.read(tmp_buf, new_len);
        // in.seekg(0, ios::end);

        buf.insert(buf.end(), tmp_buf, tmp_buf+new_len);

        while(size < buf.size()){
            copy(buf.begin(), buf.begin()+size, buffer); 
            buf.erase(buf.begin(), buf.begin()+size);
            // cout << "buffer:" << buffer << endl;

            // write every segment's md5 to file	
		    hashwrapper *myWrapper = new md5wrapper();
		    try
		    {
		    	string hash_res = myWrapper->getHashFromString(buffer);
		    	out << "cnt=" << cnt << "  :   "<< hash_res << endl;
		    	cnt++;
		    }
		    catch(hlException &e)
		    {
		    	cerr << "Get Md5 Error" << endl; 
		    }
		    delete myWrapper;

		    		    
		    vector<uint8_t> data_out;
		    encode((uint8_t*) buffer, data_out, seg_num++);
            
            cout << "-----------------------------------------" << endl;
            //cout<<data_out.size()<<" bytes data encoded!"<<endl;
		    cout << "segment number:" << seg_num -1 << endl;
            cout << "encoded data size:" << data_out.size() <<endl;
            //cout<<" encoded_block_num:" << ENCODED_BLOCK_NUM <<endl;		

		    while( sendrate1 <= EPSILON && sendrate1 >= -EPSILON && sendrate2 <= EPSILON && sendrate2 >= -EPSILON);

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
		    //if (sendrate1 < -EPSILON && sendrate2 < -EPSILON){
		    //	queue1.add(new item(data_out, 0, SEGMENT_SIZE));
		    //	queue2.add(new item(data_out, 0, SEGMENT_SIZE));  
		    //}
		    int num1 = int((double)ENCODED_BLOCK_NUM * ((double)factor1/(factor1+factor2))+0.5);
		    cout << "block num of link1 : " << num1 <<endl;
		    int datasize1 = ENCODED_BLOCK_SIZE * num1;	
		    int datasize2 = ENCODED_BLOCK_SIZE * (ENCODED_BLOCK_NUM - num1);
		    queue1.add(new item(data_out, 0, datasize1));
		    queue2.add(new item(data_out, datasize1, datasize1+datasize2));  
		    // first sending is a link-rate test
		    //if ( first_test ){
		    //	first_test = false;
		    //	in.seekg(0, ios::beg);
		    //}
        }
        delete tmp_buf;
    }
    
    in.close();
	out.close(); 
    UDT::close(client1);
    UDT::close(client2);

    // delete [] data;
    return 0;
}

