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
        std::cout << "Echo server listening on port 8080...\n";

        while (true)
        {
            tcp::socket socket(io_context);
            acceptor.accept(socket);
            std::cout << "\nA Client connected!\n";

            char data[1024];
            boost::system::error_code error;
            size_t length = socket.read_some(boost::asio::buffer(data), error);
            if (error == boost::asio::stream_errc::eof)
            {
                std::cout << "Client closed the connection naturally\n";
            }
            else if (error)
            {
                throw boost::system::system_error(error);
            }
            else
            {
                if (length >= sizeof(NewOrderMsg))
                {
                    NewOrderMsg* msg = reinterpret_cast<NewOrderMsg*>(data);
                    if (msg->type == MessageType::NewOrder)
                    {
                        std::cout << std::format("Received BUY/SELL for Symbol: {} | Price: {} | Qty: {}\n", msg->symbol, msg->price, msg->quantity);
                    }
                }
                boost::asio::write(socket, boost::asio::buffer(data, length));
            }
        }
    }
    catch (std::exception& e)
    {
        std::cerr << "Fatal Exception: " << e.what() << "\n";
    }
    return 0;
}