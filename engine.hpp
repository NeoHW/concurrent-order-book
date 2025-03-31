// This file contains declarations for the main Engine class. You will
// need to add declarations to this file as you develop your Engine.

#ifndef ENGINE_HPP
#define ENGINE_HPP

#include <chrono>
#include <vector>
#include <memory>

#include "io.hpp"
#include "order.hpp"
#include "ConcurrentHashMap.hpp"


/*
 * PriceLevelNode is a node in the linked list of orders at a particular price level.
 * It contains a list of orders at that price level 
 * It starts with a dummy head node.
 */
struct PriceLevelNode
{
	uint32_t price;
	uint32_t total_volume;
	PriceLevelNode* next;
	std::mutex mtx;
	std::vector<std::shared_ptr<Order>> orders_list;

	PriceLevelNode() : price(0), total_volume(0), next(nullptr), mtx(), orders_list() {}
	PriceLevelNode(uint32_t price) : price(price), total_volume(0), next(nullptr), mtx(), orders_list() {}

	std::string toString() const
    {
        std::ostringstream oss;
        oss << "PriceLevelNode { price: " << price
            << ", total_volume: " << total_volume
            << ", orders_count: " << orders_list.size()
            << " }";
        return oss.str();
    }
};

/*
 * BuyBook is a linked list of PriceLevelNodes representing the buy side of the order book.
 * It starts with a dummy head node.
 */
struct BuyBook
{
	PriceLevelNode* head;

	BuyBook() : head(new PriceLevelNode()) {}
};

/*
 * SellBook is a linked list of PriceLevelNodes representing the sell side of the order book.
 * It starts with a dummy head node.
 */
struct SellBook
{
	PriceLevelNode* head;

	SellBook() : head(new PriceLevelNode()) {}
};

struct OrderBook {
	std::string instrument;
	BuyBook buy_book;
	SellBook sell_book;
	std::mutex mtx;

	void cancelOrder(std::shared_ptr<Order> order);
	void matchBuyOrder(std::shared_ptr<Order> active_order, std::unique_lock<std::mutex> sell_lock);
	void matchSellOrder(std::shared_ptr<Order> active_order, std::unique_lock<std::mutex> buy_lock);
	void addRestingOrder(std::shared_ptr<Order> order, std::unique_lock<std::mutex> dummy_lock);

	OrderBook(const std::string& instrument) : instrument(instrument), buy_book(), sell_book() {}
};

struct Engine
{
public:
	ConcurrentHashMap<std::string, OrderBook*> orderBooks;
	static ConcurrentHashMap<uint32_t, std::shared_ptr<Order>> orders_hashmap;

	void accept(ClientConnection conn);
	void processCancelOrder(const ClientCommand& input);
	void processNewOrder(const ClientCommand& input);

	Engine() = default;

private:
	void connection_thread(ClientConnection conn);
};

inline std::chrono::microseconds::rep getCurrentTimestamp() noexcept
{
	return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

#endif
