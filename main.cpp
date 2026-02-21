#include <iostream>
#include <format>
#include <thread>
#include <atomic>
#include <memory>
#include <boost/asio.hpp>
#include <boost/lockfree/queue.hpp>
#include "Protocol.h"
#include "OrderBook.h"
#include "Order.h"
#include "OrderType.h"

using boost::asio::ip::tcp;
boost::lockfree::queue<NewOrderMsg, boost::lockfree::capacity<65000>> order_queue;
std::atomic<bool> server_running{true};
std::atomic<uint64_t> network_received_count{0};
bool use_queue = false; 

int main()
{
    try
    {
        OrderBook orderbook;
        std::thread engine_thread([&orderbook]() {
            NewOrderMsg msg;
            uint64_t engine_processed_count = 0;
            
            while (server_running)
            {
                if (order_queue.pop(msg))
                {
                    OrderPointer new_order = std::make_shared<Order>(
                        OrderType::GoodTillCancel,
                        msg.order_id,
                        static_cast<Side>(msg.side),
                        static_cast<Price>(msg.price),
                        static_cast<Quantity>(msg.quantity)
                    );
                    orderbook.AddOrder(new_order);
                    
                    engine_processed_count++;
                    if (engine_processed_count % 10000 == 0) 
                    {
                        std::cout << "[ENGINE THREAD] Matched " << engine_processed_count << " orders.\n";
                    }
                }
                else 
                {
                    std::this_thread::yield(); 
                }
            }
        });
        boost::asio::io_context io_context;
        tcp::acceptor acceptor(io_context, tcp::endpoint(tcp::v4(), 8080));
        std::cout << "Concurrent Matching Engine Gateway listening on port 8080...\n";
        std::cout << "Current Mode: " << (use_queue ? "Lock-Free Queue" : "Synchronous") << "\n";
        
        while (server_running)
        {
            auto socket = std::make_shared<tcp::socket>(io_context);
            acceptor.accept(*socket);
            std::cout << "\n[NETWORK] Client connected! Spawning new thread...\n";

            std::thread client_thread([socket, &orderbook]() {
                char data[65536]; 
                boost::system::error_code error;
                
                while (server_running) 
                {
                    size_t length = socket->read_some(boost::asio::buffer(data), error);
                    if (error) 
                    {
                        break; 
                    } 
                    
                    size_t offset = 0;
                    while (offset + sizeof(NewOrderMsg) <= length)
                    {
                        NewOrderMsg* msg = reinterpret_cast<NewOrderMsg*>(data + offset);
                        if (msg->type == MessageType::NewOrder)
                        {
                            uint64_t current_count = ++network_received_count;
                            if (use_queue) 
                            {
                                while (!order_queue.push(*msg)) { std::this_thread::yield(); }
                            } 
                            else 
                            {
                                OrderPointer new_order = std::make_shared<Order>(
                                    OrderType::GoodTillCancel,
                                    msg->order_id,
                                    static_cast<Side>(msg->side),
                                    static_cast<Price>(msg->price),
                                    static_cast<Quantity>(msg->quantity)
                                );
                                orderbook.AddOrder(new_order); 
                                if (current_count % 10000 == 0) 
                                {
                                    std::cout << "[NETWORK THREAD] Synchronously matched " << network_received_count << " orders.\n";
                                }
                            }
                        }
                        offset += sizeof(NewOrderMsg);
                    }
                }
            });
            client_thread.detach(); 
        }

        server_running = false;
        engine_thread.join();
    }
    catch (std::exception& e)
    {
        std::cerr << "Fatal Exception: " << e.what() << "\n";
    }
    return 0;
}