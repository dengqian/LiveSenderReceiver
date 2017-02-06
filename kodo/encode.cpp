#include "encode.h"

#include <vector>
#include <iostream>


int main(){
   int size = 10240;
   char* data_raw = new char[size];
   memset(data_raw, 'a', size); 
   const char* data = encode((uint8_t*)data_raw, size);
   
}

