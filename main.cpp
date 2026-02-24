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


boost::lockfree::queue<NewOrderMsg, boost::lockfree::capacity<65000>> order_queue;
std::atomic<bool> server_running{true};
std::atomic<uint64_t> engine_processed_count{0};

// Allocation Helper
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

int main()
{
    try
    {
        std::cout << "--- PHASE 3: MULTITHREADED QUEUE + MEMPOOL ---\n";
        
        bool use_mempool = true; // KEEP THE POOL ON!
        MemoryPool<Order> order_pool(2000000);
        OrderBook orderbook(order_pool, use_mempool);

        // 1. Start the Engine Thread
        std::thread engine_thread([&orderbook, &order_pool, use_mempool]() {
            try {
                NewOrderMsg msg;
                while (server_running) {
                    if (order_queue.pop(msg)) {
                        Order* new_order = AllocateOrder(order_pool, use_mempool, msg.order_id, msg.side, msg.price, msg.quantity);
                        orderbook.AddOrder(new_order);
                        engine_processed_count.fetch_add(1, std::memory_order_relaxed);
                    } else {
                        std::this_thread::yield(); // Fast hardware yield
                    }
                }
            } catch (const std::exception& e) {
                std::cerr << "\n[FATAL] Engine Thread died: " << e.what() << "\n";
                server_running = false;
            }
        });

        // 2. Pre-generate dummy network messages
        std::vector<NewOrderMsg> dummy_messages(2000000);
        for (int i = 0; i < 2000000; i++) {
            dummy_messages[i].type = MessageType::NewOrder;
            dummy_messages[i].order_id = i;
            dummy_messages[i].side = (i % 2 == 0) ? static_cast<uint8_t>(Side::Buy) : static_cast<uint8_t>(Side::Sell);
            dummy_messages[i].price = 100 + (i % 10);
            dummy_messages[i].quantity = 10;
        }

        std::cout << "Pre-generation complete. Main thread firing into Lock-Free Queue...\n";

        // 3. Start clock and blast the queue
        auto start_time = std::chrono::high_resolution_clock::now();

        for (int i = 0; i < 2000000; i++) {
            while (!order_queue.push(dummy_messages[i])) { 
                std::this_thread::yield(); // Spin if the queue hits 65,000 capacity
            }
        }

        std::cout << "Push complete. Waiting for Engine Thread to drain the queue...\n";

        // 4. Wait for the engine to finish popping and matching
        while (engine_processed_count.load(std::memory_order_relaxed) < 2000000) {
            if (!server_running) break; 
            std::this_thread::yield();
        }

        auto end_time = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> duration_seconds = end_time - start_time;

        if (server_running) {
            std::cout << "\nSUCCESS: Processed 2,000,000 orders across threads in " << duration_seconds.count() * 1000.0 << " ms.\n";
            std::cout << "Multithreaded Throughput: " << (2000000.0 / duration_seconds.count()) << " Ops/Sec\n";
        }

        // 5. Clean shutdown
        server_running = false;
        engine_thread.join();
    }
    catch (const std::exception& e)
    {
        std::cerr << "Fatal Exception in Main: " << e.what() << "\n";
    }
    
    return 0;
}