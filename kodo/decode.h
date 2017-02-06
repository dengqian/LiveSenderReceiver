#ifndef _KODO_DECODE_
#define _KODO_DECODE_

#include <cassert>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>


#include <kodocpp/kodocpp.hpp>

// Count the total number of packets received in order to decode
unsigned int rx_packets;


int decode(uint8_t* data_in, std::vector<uint8_t>& data_out, uint32_t length) 
{


    uint32_t symbol_size = 1024;
    uint32_t symbols = length / symbol_size ;
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

    uint32_t offset = 1;

    // Receiver loop
    while (!decoder.is_complete())
    {
        // Receive message
        // remote_address_size = sizeof(remote_address);
        memcpy(payload.data(), data_in+offset, payload_size); 
        // std::cout<<payload.data()<<std::endl;
        offset = offset+payload_size+1;

        // bytes_received = recvfrom(
        //     socket_descriptor, (char*)payload.data(), payload_size, 0,
        //     (struct sockaddr*) &remote_address, &remote_address_size);

        // if (bytes_received < 0)
        // {
        //     printf("%s: recvfrom error %d\n", argv[0], bytes_received);
        //     fflush(stdout);
        //     continue;
        // }


        // ++rx_packets;

        // Packet got through - pass that packet to the decoder
        decoder.read_payload(payload.data());

        // if (decoder.has_partial_decoding_interface() &&
        //     decoder.is_partially_complete())
        // {
        //     for (uint32_t i = 0; i < decoder.symbols(); ++i)
        //     {
        //         if (!decoded[i] && decoder.is_symbol_uncoded(i))
        //         {
        //             // Update that this symbol has been decoded,
        //             // in a real application we could process that symbol
        //             printf("Symbol %d was decoded\n", i);
        //             decoded[i] = true;
        //         }
        //     }
        // }
        payload.clear();
    }

    printf("Data decoded!");
    std::cout<<data_out.size()<<' '<<data_out.data()<<'\n';

    return 0; 

}

#endif
