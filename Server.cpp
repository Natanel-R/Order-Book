#include <iostream>
#include <boost/asio.hpp>
#include "Protocol.h"
#include "OrderBook.h"
#include <format>


using boost::asio::ip::tcp;

int main()
{
    try
    {
        boost::asio::io_context io_context;
        tcp::acceptor acceptor(io_context, tcp::endpoint(tcp::v4(), 8080));
        std::cout << "Matching Engine Gateway listening on port 8080...\n";
        uint64_t total_orders_received = 0;

        while (true)
        {
            tcp::socket socket(io_context);
            acceptor.accept(socket);
            std::cout << "\nA Client connected!\n";

            while (true)
            {
                char data[4096];
                boost::system::error_code error;
                size_t length = socket.read_some(boost::asio::buffer(data), error);

                if (error == boost::asio::stream_errc::eof)
                {
                    std::cout << "Client closed the connection naturally\n";
                    break;
                }
                else if (error)
                {
                    std::cerr << "Network Error: " << error.message() << "\n";
                    break;
                }
                else
                {
                    size_t offset = 0;
                    while (offset + sizeof(NewOrderMsg) <= length)
                    {
                        NewOrderMsg* msg = reinterpret_cast<NewOrderMsg*>(data + offset);
                        if (msg->type == MessageType::NewOrder)
                        {
                            total_orders_received++;
                            if (total_orders_received % 10000 == 0)
                            {
                                std::cout << "Received: " << total_orders_received << " orders so far...\n";
                            }
                        }
                        offset += sizeof(NewOrderMsg);
                    }
                }
            }

            
        }
    }
    catch (std::exception& e)
    {
        std::cerr << "Fatal Exception: " << e.what() << "\n";
    }
    return 0;
}