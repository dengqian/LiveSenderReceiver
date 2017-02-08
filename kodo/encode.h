#ifndef _KODO_ENCODE_H_
#define _KODO_ENCODE_H_

#include <cassert>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <vector>

#include <kodocpp/kodocpp.hpp>
#include <iostream>


int encode(uint8_t* data, std::vector<uint8_t>& data_out, int size, uint8_t segment_number)
{
    uint32_t symbol_size = 1024; // encode block size
    uint32_t symbols = size / symbol_size ; // number of blocks to be encoded.

    if(symbols == 0) symbols = 1; // number of blocks can't be 0.

    // Initialize the factory with the chosen symbols and symbol size
    kodocpp::encoder_factory encoder_factory(
        kodocpp::codec::on_the_fly,
        kodocpp::field::binary8,
        symbols, symbol_size);

    kodocpp::encoder encoder = encoder_factory.build();

    // Create the buffer needed for the payload
    uint32_t payload_size = encoder.payload_size();
    std::vector<uint8_t> payload(payload_size);
    std::vector<uint8_t> data_in(data, data+size); 
    // std::vector<uint8_t> data_out;


    const char* tag = "seg:";
    uint8_t* tag_t = (uint8_t*)tag;

    for (uint32_t i = 0; i < symbols+1; ++i)
    {
        // Add a new symbol if the encoder rank is less than the maximum number
        // of symbols
        uint32_t rank = encoder.rank();
        if (rank < encoder.symbols())
        {
            // Calculate the offset to the next symbol to insert
            uint8_t* symbol = data_in.data() + rank * encoder.symbol_size();
            encoder.set_const_symbol(rank, symbol, encoder.symbol_size());
        }
        uint32_t bytes_used = encoder.write_payload(payload.data());
        

        int len = data_out.size();

        data_out.insert(data_out.end(), tag_t, tag_t+4);
        data_out.push_back(segment_number);
        data_out.insert(data_out.end(), payload.begin(), payload.end());

        std::cout<<int(segment_number)<<' '<<data_out.size()-len<<std::endl;
        // return_code = sendto(socket_descriptor, (const char*)payload.data(),
        //                      bytes_used, 0, (struct sockaddr*) &remote_address,
        //                      sizeof(remote_address));

        // if (return_code < 0)
        // {
        //     printf("%s: cannot send packet %d \n", argv[0], i - 1);
        //     exit(1);
        // }

        // sleep_here(delay);
    }
    // std::cout<<data_out.data()<<std::endl;
    
    return 0;
}

#endif
