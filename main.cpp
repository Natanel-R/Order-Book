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
#include <fstream>
#include <cstdio>
#include "FixedSizePool.h"


void save_book_snapshot(const OrderBook& orderbook) {
    auto info = orderbook.GetOrderInfos();
    std::ofstream f("book_state.json.tmp");
    const auto& bids = info.GetBids();
    const auto& asks = info.GetAsks();

    f << "{\"bids\": [";
    for (size_t i = 0; i < bids.size(); ++i) {
        f << "{\"price\":" << bids[i].price_ << ",\"quantity\":" << bids[i].quantity_ << "}"
          << (i == bids.size() - 1 ? "" : ",");
    }
    f << "], \"asks\": [";
    for (size_t i = 0; i < asks.size(); ++i) {
        f << "{\"price\":" << asks[i].price_ << ",\"quantity\":" << asks[i].quantity_ << "}"
          << (i == asks.size() - 1 ? "" : ",");
    }
    f << "]}";
    f.close();
    
    std::rename("book_state.json.tmp", "book_state.json");
}



using boost::asio::ip::tcp;
boost::lockfree::queue<NewOrderMsg, boost::lockfree::capacity<65000>> order_queue;
std::atomic<bool> server_running{true};
std::atomic<uint64_t> network_received_count{0};
std::atomic<uint64_t> engine_processed_count{0};


inline Order* AllocateOrder(MemoryPool<Order>& pool, bool use_pool, OrderId id, uint8_t side, uint64_t price, uint64_t quantity) 
{
    if (use_pool)
    {
        Order* raw_mem = pool.allocate();
        if (raw_mem == nullptr) throw std::bad_alloc();
        return new(raw_mem) Order(OrderType::GoodTillCancel, id, static_cast<Side>(side), 
        static_cast<Price>(price), static_cast<Quantity>(quantity));
    }
    else
    {
        return new Order(OrderType::GoodTillCancel, id, static_cast<Side>(side), 
        static_cast<Price>(price), static_cast<Quantity>(quantity));
    }
}

int main()
{
    bool use_queue = false; 
    bool use_mempool = false;
    try
    {
        MemoryPool<Order> order_pool(2000000);

        std::ifstream mode_file("engine_mode.txt");
        if (mode_file.is_open()) {
            std::string sync_mode, mem_mode;
            mode_file >> sync_mode >> mem_mode;
            use_queue = (sync_mode == "queue");
            use_mempool = (mem_mode == "mempool");
        }

        OrderBook orderbook(order_pool, use_mempool);
        std::thread engine_thread([&orderbook, &order_pool, use_mempool]() {
            NewOrderMsg msg;
            
            while (server_running)
            {
                if (order_queue.pop(msg))
                {
                    Order* new_order = AllocateOrder(order_pool, use_mempool, msg.order_id, msg.side, msg.price, msg.quantity);
                    orderbook.AddOrder(new_order);
                    engine_processed_count++;
                    if (engine_processed_count % 10000 == 0)
                    {
                        save_book_snapshot(orderbook);
                    }
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

        std::thread metrics_thread([]() {
        uint64_t last_network_count = 0;
        uint64_t last_engine_count = 0;

            while (server_running) {
                std::this_thread::sleep_for(std::chrono::seconds(1));
                uint64_t current_network = network_received_count.load();
                uint64_t current_engine = engine_processed_count.load();

                uint64_t network_ops = current_network - last_network_count;
                uint64_t engine_ops = current_engine - last_engine_count;

                std::ofstream f("metrics.json.tmp");
                f << "{\"network_ops\": " << network_ops 
                << ", \"engine_ops\": " << engine_ops 
                << ", \"total_network\": " << current_network 
                << ", \"total_engine\": " << current_engine << "}";
                f.close();
                std::rename("metrics.json.tmp", "metrics.json");

                last_network_count = current_network;
                last_engine_count = current_engine;
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

            std::thread client_thread([socket, &orderbook, &order_pool, use_mempool, use_queue]() {
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
                                Order* new_order = AllocateOrder(order_pool, use_mempool, msg->order_id,
                                     msg->side, msg->price, msg->quantity);
                                orderbook.AddOrder(new_order); 
                                if (current_count % 10000 == 0) 
                                {
                                    save_book_snapshot(orderbook);
                                }
                                if (current_count % 10000 == 0) 
                                {
                                    std::cout << "[NETWORK THREAD] Synchronously matched " << current_count << " orders.\n";
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
        metrics_thread.join();
    }
    catch (std::exception& e)
    {
        std::cerr << "Fatal Exception: " << e.what() << "\n";
    }
    return 0;
}