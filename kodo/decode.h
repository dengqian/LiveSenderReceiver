#ifndef _KODO_DECODE_
#define _KODO_DECODE_

#include <cassert>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>


#include <kodocpp/kodocpp.hpp>
#include "LiveSenderReceiver/common.h"


int decode(vector<char*> data_in, std::vector<uint8_t>& data_out, int size) 
// int decode(char* data_in, std::vector<uint8_t>& data_out, int size) 
{
   
    // Initialize the factory with the chosen symbols and symbol size
    kodocpp::decoder_factory decoder_factory(
        kodocpp::codec::on_the_fly,
        kodocpp::field::binary8,
        BLOCK_NUM, BLOCK_SIZE);
    
    kodocpp::decoder decoder = decoder_factory.build();
    
    uint32_t payload_size = decoder.payload_size();
    std::vector<uint8_t> payload(payload_size);

    // Set the storage for the decoder
    data_out.resize(SEGMENT_SIZE);
    decoder.set_mutable_symbols(data_out.data(),SEGMENT_SIZE); 

    // Keeps track of which symbols have been decoded
    // std::vector<bool> decoded(BLOCK_NUM, false);

    uint32_t offset = 8;
    int cnt = 0;

    // Receiver loop
    while (!decoder.is_complete())
    {
        if(cnt >= size){
            std::cout<<cnt<<" Data decode failed!"<<endl;
            return 0;
        }

        // std::cout<<"decoding phrase "<<cnt<<endl;
        memcpy(payload.data(), data_in[cnt]+offset, payload_size); 

        cnt ++;

        // Packet got through - pass that packet to the decoder
        decoder.read_payload(payload.data());

        payload.clear();
    }

    printf("Data decoded!\n");

    return 1; 

}

#endif
