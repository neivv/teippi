#include "order.h"

#include "dat.h"
#include "offsets.h"
#include "unit.h"

DummyListHead<Order, Order::offset_of_allocated> first_allocated_order;

Order::Order(OrderType order_id, const Point &position, Unit *target, UnitType fow_unit_id) :
    order_id(order_id.Raw()), fow_unit(fow_unit_id.Raw()), position(position), target(target)
{
    allocated.Add(first_allocated_order);
    list.prev = nullptr;
    list.next = nullptr;
}

void Order::SingleDelete()
{
    allocated.Remove();
    delete this;
}

void Order::DeleteAll()
{
    auto it = first_allocated_order.begin();
    while (it != first_allocated_order.end())
    {
        Order *order = *it;
        ++it;
        delete order;
    }
    first_allocated_order.Reset();
}

OrderType Order::Type() const
{
    return OrderType(order_id);
}
