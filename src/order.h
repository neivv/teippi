#ifndef ORDER_H
#define ORDER_H

#include "types.h"
#include "list.h"

bool IsTargetableOrder(int order); // kai?

class Order
{
    public:
        static const size_t offset_of_allocated = 0x14;
        RevListEntry<Order, 0x0> list;
        uint8_t order_id;
        uint8_t dc9;
        uint16_t fow_unit;
        Point position;
        Unit *target;

        Order();
        static Order *RawAlloc() { return new Order(true); }
        ~Order() {}

        static Order *Allocate(OrderType order, const Point &position, Unit *target, UnitType fow_unit_id);
        void SingleDelete();

        static void DeleteAll();
        static void FreeMemory(int count);

        OrderType Type() const;

        DummyListEntry<Order, offset_of_allocated> allocated; // 0x14

    private:
        Order(bool) { } // raw alloc
};

extern DummyListHead<Order, Order::offset_of_allocated> first_allocated_order;
static_assert(Order::offset_of_allocated == offsetof(Order, allocated), "Order::allocated offset");

#endif // ORDER_H

