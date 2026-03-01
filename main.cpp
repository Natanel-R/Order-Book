#include <iostream>
#include <format>
#include <thread>
#include <atomic>
#include <memory>
#include <boost/asio.hpp>
#include <boost/lockfree/queue.hpp>
#include "Protocol.h"
#include "Orderbook.h"
#include "Order.h"
#include "OrderType.h"
#include <fstream>
#include <cstdio>
#include "FixedSizePool.h"


boost::lockfree::queue<NewOrderMsg, boost::lockfree::capacity<65000>> order_queue;
std::atomic<bool> server_running{true};
std::atomic<uint64_t> engine_processed_count{0};
std::atomic<uint64_t> network_received_count{0};

inline Order* AllocateOrder(MemoryPool<Order>& pool, bool use_pool, OrderId id, uint8_t side, uint64_t price, uint64_t quantity) 
{
    if (use_pool) {
        Order* raw_mem = pool.allocate();
        if (raw_mem == nullptr) throw std::bad_alloc();
        return new(raw_mem) Order(OrderType::GoodTillCancel, id, static_cast<Side>(side), static_cast<Price>(price), static_cast<Quantity>(quantity));
    } else {
        return new Order(OrderType::GoodTillCancel, id, static_cast<Side>(side), static_cast<Price>(price), static_cast<Quantity>(quantity));
    }
}

void save_book_snapshot(const OrderBook& orderbook)
{
    const auto& info = orderbook.GetOrderInfos();
    const auto& bids = info.GetBids();
    const auto& asks = info.GetAsks();

    std::ofstream f("book_state.json.temp");
    f << "{\"bids\":[";
    for (size_t i = 0; i < bids.size(); ++i)
    {
        f << "{\"price\":" << bids[i].price_
        << ",\"quantity\":" << bids[i].quantity_ << "}";
        f << (i == bids.size() - 1 ? "" : ",");
    }
    f << "], \"asks\": [";
    for (size_t i = 0; i < asks.size(); ++i)
    {
        f << "{\"price\":" << asks[i].price_
        << ",\"quantity\":" << asks[i].quantity_ << "}";
        f << (i == asks.size() - 1 ? "" : ",");
    }
    f << "]}";
    f.close();
    std::rename("book_state.json.temp", "book_state.json");
}

int main(int argc, char* argv[])
{
    try
    {
        bool run_live_server = false; // Set to true for Python TCP, false for pure C++ Benchmark
        bool use_queue = false;
        bool use_mempool = false;
        if (argc == 4) 
        {
            std::string mode_arg  = argv[1]; // "live" or "test"
            std::string queue_arg = argv[2]; // "queue" or "sync"
            std::string pool_arg  = argv[3]; // "mempool" or "os"

            // 1. Set the Execution Mode
            if (mode_arg == "live") {
                run_live_server = true;
            } else if (mode_arg == "test") {
                run_live_server = false;
            } else {
                std::cerr << "[ERROR] First argument must be 'live' or 'test'\n";
                return 1;
            }

            // 2. Set the Hardware Architecture
            use_queue = (queue_arg == "queue");
            use_mempool = (pool_arg == "mempool");
            
            std::cout << "[INIT] Booting with -> Mode: " << mode_arg 
                      << " | Threading: " << (use_queue ? "QUEUE" : "SYNC") 
                      << " | Memory: " << (use_mempool ? "MEMPOOL" : "OS HEAP") << "\n";
        }
        else 
        {
            // Manual if input is incorrect
            std::cerr << "========================================\n";
            std::cerr << "INVALID COMMAND. Usage instructions:\n";
            std::cerr << "./engine <mode> <threading> <memory>\n";
            std::cerr << "  <mode>      : live | test\n";
            std::cerr << "  <threading> : queue | sync\n";
            std::cerr << "  <memory>    : mempool | os\n\n";
            std::cerr << "Example: ./engine test sync mempool\n";
            std::cerr << "========================================\n";
            return 1;
        }

        MemoryPool<Order> order_pool(10000000);
        OrderBook orderbook(order_pool, use_mempool);
        std::thread engine_thread;

        // Start the Engine Thread
        if (use_queue)
        {
            engine_thread = std::thread([&orderbook, &order_pool, use_mempool]() {
                try 
                {
                    NewOrderMsg msg;
                    while (server_running) 
                    {
                        if (order_queue.pop(msg)) 
                        {
                            Order* new_order = AllocateOrder(order_pool, use_mempool, msg.order_id, msg.side, msg.price, msg.quantity);
                            orderbook.AddOrder(new_order);
                            
                            // Update every 250k processed orders
                            uint64_t prev_count = engine_processed_count.fetch_add(1, std::memory_order_relaxed);
                            if ((prev_count+1) % 250000 == 0)
                            {
                                save_book_snapshot(orderbook);
                            }
                        } 
                        else 
                        {
                            std::this_thread::yield();
                        }
                    }
                } 
                catch (const std::exception& e) 
                {
                    std::cerr << "\n[FATAL] Engine Thread died: " << e.what() << "\n";
                    server_running = false;
                }
            });
        }

        if (run_live_server)
        {
            // Start the Metrics Thread
            std::thread metrics_thread([]()
            {
                uint64_t last_network_count = 0;
                uint64_t last_engine_count = 0;
                while (server_running)
                {
                    std::this_thread::sleep_for(std::chrono::seconds(1));
                    uint64_t current_network_count = network_received_count.load();
                    uint64_t current_engine_count = engine_processed_count.load();
                    uint64_t network_ops_per_second = current_network_count - last_network_count;
                    uint64_t engine_ops_per_second = current_engine_count - last_engine_count;

                    std::ofstream f("metrics.json.temp");
                    f << "{\"network_ops\":" << network_ops_per_second
                    << ", \"engine_ops\": " << engine_ops_per_second 
                    << ", \"total_network\": " << current_network_count 
                    << ", \"total_engine\": " << current_engine_count << "}"; 
                    f.close();
                    std::rename("metrics.json.temp", "metrics.json");

                    last_network_count = current_network_count;
                    last_engine_count = current_engine_count;
                }
            });

            boost::asio::io_context io_context;
            boost::asio::ip::tcp::acceptor acceptor(io_context, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), 8080));
            while (server_running)
            {
                auto socket = std::make_shared<boost::asio::ip::tcp::socket>(io_context);
                // blocks main thread until python client connects
                boost::system::error_code accept_error;
                acceptor.accept(*socket, accept_error);
                if (accept_error) 
                {
                    std::cerr << "Accept error: " << accept_error.message() << "\n";
                    continue;
                }

                std::thread client_thread([socket, &orderbook, &order_pool, use_queue, use_mempool]()
                {
                    try
                    {
                        char data[65536];
                        size_t leftover = 0;
                        while (server_running)
                        {
                            boost::system::error_code error;
                            size_t length = socket->read_some(boost::asio::buffer(data + leftover, sizeof(data) - leftover), error);
                            if (error == boost::asio::error::eof || error == boost::asio::error::connection_reset)
                            {
                                std::cout << "[NETWORK] Client Disconnected.\n";
                                break;
                            }
                            else if (error) throw boost::system::system_error(error);

                            // Calculate exactly how many WHOLE messages we received
                            size_t total_bytes = leftover + length;
                            size_t num_messages = total_bytes / sizeof(NewOrderMsg);
                            size_t consumed_bytes = num_messages * sizeof(NewOrderMsg);

                            size_t offset = 0;
                            for (size_t i = 0; i < num_messages; ++i)
                            {
                                NewOrderMsg* msg = reinterpret_cast<NewOrderMsg*>(&data[offset]);
                                network_received_count.fetch_add(1, std::memory_order_relaxed);
                                if (use_queue) 
                                {
                                    while (!order_queue.push(*msg)) std::this_thread::yield();
                                }
                                else
                                {
                                    Order* order = AllocateOrder(order_pool, use_mempool, msg->order_id, msg->side, msg->price, msg->quantity);
                                    orderbook.AddOrder(order);
                                    
                                    uint64_t prev_count = engine_processed_count.fetch_add(1, std::memory_order_relaxed);
                                    if ((prev_count + 1) % 250000 == 0)
                                    {
                                        save_book_snapshot(orderbook);
                                    }
                                }

                                offset += sizeof(NewOrderMsg);
                            }
                            leftover = total_bytes - consumed_bytes;
                            if (leftover > 0) std::memmove(data, data + consumed_bytes, leftover);
                        }
                    }
                    catch(const std::exception& e)
                    {
                        std::cerr << "[NETWORK] Client Thread Exception: " << e.what() << "\n";
                    }
                    
                });
                client_thread.detach();
            }
            if (metrics_thread.joinable()) metrics_thread.join();
        }
        else
        {
            std::cout << "[INIT] Booting Offline Hardware Benchmark...\n";
            std::cout << "[BENCHMARK] Generating 10,000,000 orders in memory...\n";
            std::vector<NewOrderMsg> dummy_messages(10000000);
            for (int i = 0; i < 10000000; i++) {
                dummy_messages[i].type = MessageType::NewOrder;
                dummy_messages[i].order_id = i;
                dummy_messages[i].side = (i % 2 == 0) ? static_cast<uint8_t>(Side::Buy) : static_cast<uint8_t>(Side::Sell);
                dummy_messages[i].price = 100 + (i % 10);
                dummy_messages[i].quantity = 10;
            }

            std::cout << "[BENCHMARK] Firing into engine...\n";
            auto start_time = std::chrono::high_resolution_clock::now();

            if (use_queue) 
            {
                // QUEUE MODE: Main thread pushes, Engine thread pops
                for (int i = 0; i < 10000000; i++) {
                    while (!order_queue.push(dummy_messages[i])) std::this_thread::yield();
                }
                // Wait for engine thread to finish draining the queue
                while (engine_processed_count.load(std::memory_order_relaxed) < 10000000) {
                    std::this_thread::yield();
                }
            } 
            else 
            {
                // SYNC MODE: Main thread bypasses queue and matches directly
                for (int i = 0; i < 10000000; i++) {
                    Order* order = AllocateOrder(order_pool, use_mempool, dummy_messages[i].order_id, dummy_messages[i].side, dummy_messages[i].price, dummy_messages[i].quantity);
                    orderbook.AddOrder(order);
                }
            }

            auto end_time = std::chrono::high_resolution_clock::now();
            std::chrono::duration<double> duration_seconds = end_time - start_time;

            std::cout << "\n========================================\n";
            std::cout << "CONFIGURATION:\n";
            std::cout << "Queue: " << (use_queue ? "ON" : "OFF") << "\n";
            std::cout << "MemPool: " << (use_mempool ? "ON" : "OFF") << "\n";
            std::cout << "----------------------------------------\n";
            std::cout << "Processed 10,000,000 orders in " << duration_seconds.count() * 1000.0 << " ms.\n";
            std::cout << "THROUGHPUT: " << (10000000.0 / duration_seconds.count()) << " Ops/Sec\n";
            std::cout << "========================================\n";
        }
        // shutdown
        server_running = false;
        if (engine_thread.joinable()) engine_thread.join();
    }
    catch (const std::exception& e)
    {
        std::cerr << "Fatal Exception in Main: " << e.what() << "\n";
    }
    
    return 0;
}