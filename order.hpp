#include "io.hpp"

struct Order
{
    enum CommandType type;
    std::string instrument;
    uint32_t order_id;
    uint32_t execution_id;
    uint32_t price;
    uint32_t count;
};