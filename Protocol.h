#pragma once
#include <cstdint>
#pragma pack(push, 1)

enum class  MessageType : uint8_t
{
    NewOrder = 1,
    CancelOrder = 2
};

struct NewOrderMsg
{
    MessageType type;
    uint64_t timestamp;
    uint64_t order_id;
    uint32_t price;
    uint32_t quantity;
    uint8_t side;
    char symbol[8];
};

struct CancelOrder
{
    MessageType type;
    uint64_t order_id;
};

#pragma pack(pop)