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
#include <time.h>

#include "wqueue.h"
// #include "cc.h"
// #include "test_util.h"
#include "kodo/encode.h"
#include "kodo/decode.h"
#include "./common.h"
#include "../src/common.h"
#include "hashlibpp.h"     //md5 lib
#define EPSILON 1e-3

using namespace std;

int main(int argc, char* argv[]){
    fstream in(argv[1], ios::in | ios::binary);
    const int size = SEGMENT_SIZE;
    char* buffer = new char[size];
    int seg_num = 0;
    uint64_t total_encode_time = 0;
    uint64_t total_decode_time = 0;
    int cnt = 0;
    while(!in.eof()){
       in.read(buffer, size);

       uint64_t time_before_encode = CTimer::getTime();
       vector<uint8_t> data_out;
       encode((uint8_t*)buffer, data_out, seg_num++);

       uint64_t cur_time = CTimer::getTime();
       total_encode_time += cur_time - time_before_encode;

       cout << "data encoded" << endl;
       vector<uint8_t> decode_data;
       decode((char*)data_out.data(), decode_data, ENCODED_BLOCK_NUM);
       total_decode_time += CTimer::getTime() - cur_time;

       cnt ++;
    }
    cout << "avg encode time: " << (double)total_encode_time/cnt <<endl;
    cout << "avg decode time: " << (double)total_decode_time/cnt <<endl;
}

