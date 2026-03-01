#include <gtest/gtest.h>
#include "Orderbook.h"
#include "Order.h"
#include "OrderType.h"
#include "FixedSizePool.h" 

class OrderBookTest : public ::testing::Test 
{
protected:
    MemoryPool<Order>* pool;
    OrderBook* book;

    void SetUp() override {
        pool = new MemoryPool<Order>(1000); 
        book = new OrderBook(*pool, false); 
    }

    void TearDown() override {
        delete book;
        delete pool;
    }

    Order* CreateOrder(uint64_t id, Side side, Price price, Quantity qty) {
        return new Order(OrderType::GoodTillCancel, id, side, price, qty);
    }
};

TEST_F(OrderBookTest, AddsOrderCorrectly) 
{
    Order* bid = CreateOrder(1, Side::Buy, 150, 100);
    book->AddOrder(bid);

    EXPECT_EQ(book->Size(), 1);

    auto infos = book->GetOrderInfos();
    ASSERT_EQ(infos.GetBids().size(), 1); 
    EXPECT_EQ(infos.GetBids()[0].price_, 150);
    EXPECT_EQ(infos.GetBids()[0].quantity_, 100);
}

TEST_F(OrderBookTest, CancelsOrderCorrectly) 
{
    Order* bid = CreateOrder(1, Side::Buy, 150, 100);
    book->AddOrder(bid);
    
    book->CancelOrder(1);
    EXPECT_EQ(book->Size(), 0);
    auto infos = book->GetOrderInfos();
    EXPECT_TRUE(infos.GetBids().empty());
}

TEST_F(OrderBookTest, EnforcesPricePriority) 
{
    book->AddOrder(CreateOrder(1, Side::Buy, 150, 100)); 
    book->AddOrder(CreateOrder(2, Side::Buy, 151, 100)); 

    auto infos = book->GetOrderInfos();
    ASSERT_GE(infos.GetBids().size(), 2);
    EXPECT_EQ(infos.GetBids()[0].price_, 151);
}

TEST_F(OrderBookTest, ExactMatchClearsSpread) 
{
    book->AddOrder(CreateOrder(1, Side::Sell, 150, 100));
    
    auto trades = book->AddOrder(CreateOrder(2, Side::Buy, 150, 100));

    EXPECT_EQ(trades.size(), 1);
    EXPECT_EQ(book->Size(), 0); 
}

TEST_F(OrderBookTest, HandlesPartialFills) 
{
    book->AddOrder(CreateOrder(1, Side::Sell, 150, 1000));
    auto trades = book->AddOrder(CreateOrder(2, Side::Buy, 150, 100));

    EXPECT_EQ(trades.size(), 1);
    EXPECT_EQ(book->Size(), 1); 

    auto infos = book->GetOrderInfos();
    ASSERT_EQ(infos.GetAsks().size(), 1);
    EXPECT_EQ(infos.GetAsks()[0].quantity_, 900); 
}

TEST_F(OrderBookTest, WalksTheBook) 
{
    book->AddOrder(CreateOrder(1, Side::Sell, 150, 100));
    book->AddOrder(CreateOrder(2, Side::Sell, 151, 100));
    
    auto trades = book->AddOrder(CreateOrder(3, Side::Buy, 155, 200));

    EXPECT_EQ(trades.size(), 2);
    EXPECT_EQ(book->Size(), 0);
}