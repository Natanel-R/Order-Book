#pragma once
#include <map>
#include <unordered_map>
#include "Usings.h"
#include "Order.h"
#include "OrderModify.h"
#include "OrderbookLevelInfos.h"
#include "Trade.h"



class OrderBook
{
    private:

        struct OrderEntry
        {
            OrderPointer order_ { nullptr };
            OrderPointers::iterator location_;
        };

        std::map<Price, OrderPointers, std::greater<Price>> bids_;
        std::map<Price, OrderPointers, std::less<Price>> asks_;
        std::unordered_map<OrderId, OrderEntry> orders_;

        bool CanMatch(Side side, Price price) const;
        Trades MatchOrders();

    public:

        OrderBook();
        OrderBook(const OrderBook&) = delete;
        void operator=(const OrderBook&) = delete;
        OrderBook(const OrderBook&&) = delete;
        void operator=(const OrderBook&&) = delete;
        ~OrderBook();

        Trades AddOrder(OrderPointer order);
        void CancelOrder(OrderId orderId);
        Trades ModifyOrder(OrderModify order);

        std::size_t Size() const;
        OrderBookLevelInfos GetOrderInfos() const;

};