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


const int block_size = BLOCK_SIZE;

int decode(uint8_t* data_in, std::vector<uint8_t>& data_out, int num) 
{


    uint32_t symbol_size = block_size;
    uint32_t symbols = num;

    std::cout<<symbols<<std::endl;

    // Initialize the factory with the chosen symbols and symbol size
    kodocpp::decoder_factory decoder_factory(
        kodocpp::codec::on_the_fly,
        kodocpp::field::binary8,
        symbols, symbol_size);

    kodocpp::decoder decoder = decoder_factory.build();

    // Create the buffer needed for the payload
    uint32_t payload_size = decoder.payload_size();
    std::cout<<"payload_size:"<<payload_size<<'\n';
    std::vector<uint8_t> payload(payload_size);

    // Set the storage for the decoder
    // std::cout<<decoder.block_size()<<'\n';
    // std::vector<uint8_t> data_out(decoder.block_size());
    data_out.resize(decoder.block_size());
    decoder.set_mutable_symbols(data_out.data(), decoder.block_size());

    // Keeps track of which symbols have been decoded
    std::vector<bool> decoded(symbols, false);

    uint32_t offset = 5;
    int cnt = 0;

    // Receiver loop
    while (!decoder.is_complete())
    {
        cnt ++;
        std::cout<<"decoding phrase "<<cnt<<endl;
        // Receive message
        // remote_address_size = sizeof(remote_address);
        memcpy(payload.data(), data_in+offset, payload_size); 

        offset += payload_size + 5;
        // std::cout<<payload.data()<<std::endl;

        // Packet got through - pass that packet to the decoder
        decoder.read_payload(payload.data());

        /*
        if (decoder.has_partial_decoding_interface() &&
            decoder.is_partially_complete())
        {
            for (uint32_t i = 0; i < decoder.symbols(); ++i)
            {
                if (!decoded[i] && decoder.is_symbol_uncoded(i))
                {
                    // Update that this symbol has been decoded,
                    // in a real application we could process that symbol
                    // printf("Symbol %d was decoded\n", i);
                    decoded[i] = true;
                }
            }
        }
        */

        // std::cout<<payload.data()<<std::endl;
        payload.clear();
    }

    printf("Data decoded!");
    // std::cout<<data_out.size()<<' '<<data_out.data()<<'\n';

    return 0; 

}

#endif
