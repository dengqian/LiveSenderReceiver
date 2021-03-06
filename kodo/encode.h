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

#include "LiveSenderReceiver/common.h"
#include <iostream>


const char* tag = "seg:";
uint8_t* tag_t = (uint8_t*)tag;

int encode(uint8_t* data, std::vector<uint8_t>& data_out, uint32_t segment_number)
{
    uint32_t symbol_size = BLOCK_SIZE; // encode block size
    uint32_t symbols = BLOCK_NUM; // number of blocks to be encoded.

    if(symbols == 0) symbols = 1; // number of blocks can't be 0.

    // Initialize the factory with the chosen symbols and symbol size
    kodocpp::encoder_factory encoder_factory(
        kodocpp::codec::on_the_fly,
        kodocpp::field::binary8,
        symbols, symbol_size);
    
    kodocpp::encoder encoder = encoder_factory.build();
    uint32_t payload_size = encoder.payload_size();

    // Create the buffer needed for the payload
    std::vector<uint8_t> payload(payload_size);
    std::vector<uint8_t> data_in(data, data+SEGMENT_SIZE); 

	uint32_t last_rank = -1, rank = 0;
	int cnt = 0;
	
	using std::cout;
	using std::endl;

	while(cnt < ENCODED_BLOCK_NUM)
    {
        // Add a new symbol if the encoder rank is less than the maximum number
        // of symbols
		cnt ++;
        rank = encoder.rank();
		// cout<<cnt<<" current rank: " << rank << endl;	

		if (rank == last_rank && rank < symbols){
			continue;
		}

        if (rank < encoder.symbols())
        {
            // Calculate the offset to the next symbol to insert
            uint8_t* symbol = data + rank * encoder.symbol_size();
            encoder.set_const_symbol(rank, symbol, encoder.symbol_size());
        }

        // write a encoded payload to data_out 
        encoder.write_payload(payload.data());

        // insert segment number for each block
        uint8_t d[4] = {0};
        for (int i=0; i<4 ;++i){
            d[i] = ((uint8_t*)&segment_number)[i];
		}
        data_out.insert(data_out.end(), tag_t, tag_t+4);
		data_out.insert(data_out.end(), d, d+4);
        data_out.insert(data_out.end(), payload.begin(), payload.end());

		last_rank = rank;
    }

    // cout << "encoded data:" << data_out.data() << endl;
    
    return 0;
}

#endif
