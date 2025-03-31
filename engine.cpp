#include <algorithm>
#include <iostream>
#include <thread>

#include "io.hpp"
#include "engine.hpp"
#include "ConcurrentHashMap.hpp"

ConcurrentHashMap<uint32_t, std::shared_ptr<Order>> Engine::orders_hashmap;

void Engine::accept(ClientConnection connection)
{
	auto thread = std::thread(&Engine::connection_thread, this, std::move(connection));
	thread.detach();
}

void Engine::connection_thread(ClientConnection connection)
{
	while (true)
	{
		ClientCommand input{};
		switch (connection.readInput(input))
		{
		case ReadResult::Error:
			SyncCerr{} << "Error reading input" << std::endl;
		case ReadResult::EndOfFile:
			return;
		case ReadResult::Success:
			break;
		}

		// Functions for printing output actions in the prescribed format are
		// provided in the Output class:
		switch (input.type)
		{
		case input_cancel:
		{
			//SyncCerr{} << "Got cancel: ID: " << input.order_id << std::endl;
			processCancelOrder(input);
			break;
		}

		default:
		{
			//SyncCerr{} << "Got order: " << static_cast<char>(input.type) << " " << input.instrument << " x " << input.count << " @ " << input.price << " ID: " << input.order_id << std::endl;
			processNewOrder(input);
			break;
		}
		}
	}
}

void Engine::processNewOrder(const ClientCommand &input)
{
	//SyncCerr{} << "Processing new order: " << input.order_id << std::endl;
	auto order = std::make_shared<Order>();
	order->type = input.type;
	order->instrument = input.instrument;
	order->order_id = input.order_id;
	order->price = input.price;
	order->count = input.count;

	//SyncCerr{} << "[DEBUG] Inserting order " << input.order_id << " into orders_hashmap." << std::endl;
	orders_hashmap.insert(order->order_id, order);

	OrderBook *order_book;
	if (!orderBooks.find(order->instrument, order_book))
	{
		order_book = new OrderBook(order->instrument);
		orderBooks.insert(order->instrument, order_book);
		//SyncCerr{} << "[DEBUG] Created new order book for instrument: " << order->instrument << std::endl;
	}

	// always lock in same order to avoid deadlock
	std::unique_lock<std::mutex> book_lock(order_book->mtx);
	//SyncCerr{} << "[DEBUG] Locked book_lock" << std::endl;
	std::unique_lock<std::mutex> buy_lock(order_book->buy_book.head->mtx);
	//SyncCerr{} << "[DEBUG] Locked buy_lock" << std::endl;
    std::unique_lock<std::mutex> sell_lock(order_book->sell_book.head->mtx);
	//SyncCerr{} << "[DEBUG] Locked sell_lock" << std::endl;
	book_lock.unlock();

	// Pre-scan the opposite side and lock if we have to add a resting order
	if (order->type == input_buy)
	{
		//SyncCerr{} << "[DEBUG] Pre-scanning sell side for matching buy order " << order->order_id << std::endl;
		auto remaining_qty = order->count;
		PriceLevelNode *curr = order_book->sell_book.head->next;
		
		while (remaining_qty > 0 && curr)
		{
			std::unique_lock<std::mutex> currLock(curr->mtx);
			//SyncCerr{} << "[DEBUG] Checking sell PriceLevelNode with price " << curr->price << std::endl;

			// we can only match buy orders which have a higher price than sell
			if (curr->price > order->price)
			{
				//SyncCerr{} << "[DEBUG] Price level " << curr->price << " is too high for buy order price " << order->price << std::endl;
				break;
			}
			remaining_qty -= curr->total_volume;
			//SyncCerr{} << "[DEBUG] Reduced remaining quantity to " << remaining_qty << " after checking price level " << curr->price << std::endl;
			curr = curr->next;
		}

		// we do not need to add as resting order (no need serialise), so we can unlock buy_lock
		if (remaining_qty <= 0)
		{
			buy_lock.unlock();
		}

		order_book->matchBuyOrder(order, std::move(sell_lock));

		if (order->count > 0)
		{
			order_book->addRestingOrder(order, std::move(buy_lock));
		}
	}
	else
	{
		//SyncCerr{} << "[DEBUG] Pre-scanning buy side for matching sell order " << order->order_id << std::endl;
		auto remaining_qty = order->count;
		PriceLevelNode *curr = order_book->buy_book.head->next;

		while (remaining_qty > 0 && curr)
		{
			std::unique_lock<std::mutex> currLock(curr->mtx);
			//SyncCerr{} << "[DEBUG] Checking buy PriceLevelNode with price " << curr->price << std::endl;
			if (curr->price < order->price)
			{
				//SyncCerr{} << "[DEBUG] Price level " << curr->price << " is too low for sell order price " << order->price << std::endl;
				break;
			}
			remaining_qty -= curr->total_volume;
			//SyncCerr{} << "[DEBUG] Reduced remaining quantity to " << remaining_qty << " after checking price level " << curr->price << std::endl;
			curr = curr->next;
		}

		if (remaining_qty <= 0)
		{
			sell_lock.unlock();
		}

		order_book->matchSellOrder(order, std::move(buy_lock));

		if (order->count > 0)
		{
			order_book->addRestingOrder(order, std::move(sell_lock));
		}
	}
	//SyncCerr{} << "[DEBUG] Finished processing new order " << order->order_id << std::endl;
}

/*
 * Matches an active buy order against the sell side of the order book.
 */
void OrderBook::matchBuyOrder(std::shared_ptr<Order> active_order, std::unique_lock<std::mutex> sell_lock)
{
	//SyncCerr{} << "[DEBUG] matchBuyOrder: Start matching for active buy order " << active_order->order_id << std::endl;
	PriceLevelNode *dummy = sell_book.head;
	std::unique_lock<std::mutex> prevLock = std::move(sell_lock);
	PriceLevelNode *prev = dummy;
	PriceLevelNode *curr = dummy->next;

	while (curr && active_order->count > 0)
	{
		std::unique_lock<std::mutex> currLock(curr->mtx);
		//SyncCerr{} << "[DEBUG] matchBuyOrder: Checking PriceLevelNode with price " << curr->price << std::endl;

		// can match price
		if (active_order->price >= curr->price)
		{
			//SyncCerr{} << "[DEBUG] matchBuyOrder: Price match found. Active order price " << active_order->price << " >= current price " << curr->price << std::endl;

			for (auto it = curr->orders_list.begin(); it != curr->orders_list.end() && active_order->count > 0;)
			{
				std::shared_ptr<Order> resting_order = *it;
				uint32_t transaction_qty = std::min(active_order->count, resting_order->count);
				//SyncCerr{} << "[DEBUG] Executing trade: active order " << active_order->order_id << " resting order " << resting_order->order_id << " transaction qty " << transaction_qty << std::endl;
				active_order->count -= transaction_qty;
				resting_order->count -= transaction_qty;
				curr->total_volume -= transaction_qty;
				resting_order->execution_id++;

				auto output_time = getCurrentTimestamp();
				Output::OrderExecuted(resting_order->order_id, active_order->order_id, resting_order->execution_id, curr->price, transaction_qty, output_time);

				// remove resting order if fully executed
				if (resting_order->count == 0)
				{
					//SyncCerr{} << "[DEBUG] Resting order " << resting_order->order_id << " fully executed and removed." << std::endl;
					it = curr->orders_list.erase(it);
					Engine::orders_hashmap.erase(resting_order->order_id);
				}
				else
				{
					++it;
				}
			}

			// remove PriceLevelNode if there are no more orders at this price level
			if (curr->orders_list.empty())
			{
				//SyncCerr{} << "[DEBUG] PriceLevelNode with price " << curr->price << " is empty and will be removed." << std::endl;
				prev->next = curr->next;
				currLock.unlock();
				delete curr;
				curr = prev->next;
				continue;
			}
		}
		else
		{
			// wont be able to match other prices
			//SyncCerr{} << "[DEBUG] matchBuyOrder: Active order price " << active_order->price << " is less than PriceLevelNode price " << curr->price << ". No further matches possible." << std::endl;
			break;
		}

		// continue to next PriceLevelNode
		//SyncCerr{} << "[DEBUG] continuing to next PriceLevelNode" << std::endl;
		prevLock.unlock();
		prev = curr;
		prevLock = std::move(currLock);
		curr = curr->next;
	}
	//SyncCerr{} << "[DEBUG] matchBuyOrder: Finished matching for active buy order " << active_order->order_id << std::endl;
}

/*
 * Matches an active sell order against the buy side of the order book.
 */
void OrderBook::matchSellOrder(std::shared_ptr<Order> active_order, std::unique_lock<std::mutex> buy_lock)
{
	//SyncCerr{} << "[DEBUG] matchSellOrder: Start matching for active sell order " << active_order->order_id << std::endl;
	PriceLevelNode *dummy = buy_book.head;
	std::unique_lock<std::mutex> prevLock = std::move(buy_lock);
	PriceLevelNode *prev = dummy;
	PriceLevelNode *curr = dummy->next;

	while (curr && active_order->count > 0)
	{
		std::unique_lock<std::mutex> currLock(curr->mtx);
		//SyncCerr{} << "[DEBUG] matchSellOrder: Checking PriceLevelNode with price " << curr->price << std::endl;

		if (active_order->price <= curr->price)
		{
			//SyncCerr{} << "[DEBUG] matchSellOrder: Price match found. Active order price " << active_order->price << " <= current price " << curr->price << std::endl;

			for (auto it = curr->orders_list.begin(); it != curr->orders_list.end() && active_order->count > 0;)
			{
				std::shared_ptr<Order> resting_order = *it;
				uint32_t transaction_qty = std::min(active_order->count, resting_order->count);
				//SyncCerr{} << "[DEBUG] Executing trade: active sell order " << active_order->order_id << " resting order " << resting_order->order_id << " transaction qty " << transaction_qty << std::endl;

				active_order->count -= transaction_qty;
				resting_order->count -= transaction_qty;
				curr->total_volume -= transaction_qty;
				resting_order->execution_id++;

				auto output_time = getCurrentTimestamp();
				Output::OrderExecuted(resting_order->order_id, active_order->order_id, resting_order->execution_id, curr->price, transaction_qty, output_time);

				if (resting_order->count == 0)
				{
					//SyncCerr{} << "[DEBUG] Resting order " << resting_order->order_id << " fully executed and removed." << std::endl;
					it = curr->orders_list.erase(it);
					Engine::orders_hashmap.erase(resting_order->order_id);
				}
				else
				{
					++it;
				}
			}

			if (curr->orders_list.empty())
			{
				//SyncCerr{} << "[DEBUG] PriceLevelNode with price " << curr->price << " is empty and will be removed." << std::endl;
				prev->next = curr->next;
				currLock.unlock();
				delete curr;
				curr = prev->next;
				continue;
			}
		}
		else
		{
			//SyncCerr{} << "[DEBUG] matchSellOrder: Active order price " << active_order->price << " is greater than PriceLevelNode price " << curr->price << ". No further matches possible." << std::endl;
			break;
		}

		// continue to next PriceLevelNode
		//SyncCerr{} << "[DEBUG] continuing to next PriceLevelNode" << std::endl;
		prev = curr;
		prevLock = std::move(currLock);
		curr = curr->next;
	}
	//SyncCerr{} << "[DEBUG] matchSellOrder: Finished matching for active sell order " << active_order->order_id << std::endl;
}

void OrderBook::addRestingOrder(std::shared_ptr<Order> resting_order, std::unique_lock<std::mutex> dummy_lock)
{
	//SyncCerr{} << "[DEBUG] Adding resting order: " << resting_order->order_id << " to order book." << std::endl;
	PriceLevelNode *dummy = (resting_order->type == input_buy) ? buy_book.head : sell_book.head;
	std::unique_lock<std::mutex> prevLock = std::move(dummy_lock);
	PriceLevelNode *prev = dummy;
	PriceLevelNode *curr = dummy->next;

	if (resting_order->type == input_buy)
	{
		while (curr && curr->price > resting_order->price)
		{
			//SyncCerr{} << "[DEBUG] addRestingOrder (buy): Skipping PriceLevelNode with price " << curr->price << std::endl;
			std::unique_lock<std::mutex> currLock(curr->mtx);
			prevLock.unlock();
			prev = curr;
			prevLock = std::move(currLock);
			curr = curr->next;
		}
	}
	else
	{
		while (curr && curr->price < resting_order->price)
		{
			//SyncCerr{} << "[DEBUG] addRestingOrder (sell): Skipping PriceLevelNode with price " << curr->price << std::endl;
			std::unique_lock<std::mutex> currLock(curr->mtx);
			prevLock.unlock();
			prev = curr;
			prevLock = std::move(currLock);
			curr = curr->next;
		}
	}

	// if there is an existing PriceLevelNode with the same price
	if (curr && curr->price == resting_order->price)
	{
		//SyncCerr{} << "[DEBUG] Found existing PriceLevelNode with matching price " << curr->price << ". Adding order to node." << std::endl;
		curr->orders_list.push_back(resting_order);
		curr->total_volume += resting_order->count;
	}
	else
	{
		//SyncCerr{} << "[DEBUG] Creating new PriceLevelNode for price " << resting_order->price << " with resting order " << resting_order->order_id << std::endl;
		// create a new PriceLevelNode
		PriceLevelNode *new_node = new PriceLevelNode(resting_order->price);
		new_node->orders_list.push_back(resting_order);
		new_node->total_volume = resting_order->count;

		prev->next = new_node;
		new_node->next = curr;
	}

	auto output_time = getCurrentTimestamp();
	Output::OrderAdded(resting_order->order_id, resting_order->instrument.c_str(), resting_order->price, resting_order->count, resting_order->type == input_sell, output_time);
	//SyncCerr{} << "[DEBUG] Finished adding resting order " << resting_order->order_id << std::endl;
}

void Engine::processCancelOrder(const ClientCommand &input)
{
	//SyncCerr{} << "[DEBUG] Begin processing cancel order for Order ID: " << input.order_id << std::endl;
	std::shared_ptr<Order> cancellation_order;
	if (!orders_hashmap.find(input.order_id, cancellation_order))
	{
		//SyncCerr{} << "[DEBUG] Cancel order " << input.order_id << " not found in orders_hashmap." << std::endl;
		auto output_time = getCurrentTimestamp();
		Output::OrderDeleted(input.order_id, false, output_time);
		return;
	}

	OrderBook *order_book;
	// if no orderbook exists
	if (!orderBooks.find(cancellation_order->instrument, order_book))
	{
		//SyncCerr{} << "[DEBUG] Order book for instrument " << cancellation_order->instrument << " not found during cancelation." << std::endl;
		auto output_time = getCurrentTimestamp();
		Output::OrderDeleted(input.order_id, false, output_time);
		return;
	}

	//SyncCerr{} << "[DEBUG] Found order book for instrument " << cancellation_order->instrument << ". Proceeding with cancellation." << std::endl;
	order_book->cancelOrder(cancellation_order);
}

void OrderBook::cancelOrder(std::shared_ptr<Order> order)
{
	//SyncCerr{} << "[DEBUG] Attempting to cancel order " << order->order_id << " from order book." << std::endl;
	// Find the order in the order book
	PriceLevelNode *dummy = (order->type == input_buy) ? buy_book.head : sell_book.head;

	std::unique_lock<std::mutex> prevLock(dummy->mtx);
	PriceLevelNode *prev = dummy;
	PriceLevelNode *curr = dummy->next;

	// traverse LL of PriceLevelNode using hand-over-hand locking
	while (curr)
	{
		std::unique_lock<std::mutex> currLock(curr->mtx);
		//SyncCerr{} << "[DEBUG] cancelOrder: Inspecting PriceLevelNode with price " << curr->price << std::endl;

		// found PriceLevelNode with the same price
		if (curr->price == order->price)
		{
			auto it = std::find_if(curr->orders_list.begin(), curr->orders_list.end(), [&order](const std::shared_ptr<Order> &o)
								   { return o->order_id == order->order_id; });

			// order not found
			if (it == curr->orders_list.end())
			{
				//SyncCerr{} << "[DEBUG] cancelOrder: Order " << order->order_id << " not found in PriceLevelNode." << std::endl;
				auto output_time = getCurrentTimestamp();
				Output::OrderDeleted(order->order_id, false, output_time);
				return;
			}

			//SyncCerr{} << "[DEBUG] cancelOrder: Found order " << order->order_id << ". Removing from PriceLevelNode." << std::endl;
			// remove order from the orders vector
			curr->orders_list.erase(it);
			curr->total_volume -= order->count;

			// remove order from the orders hashmap
			Engine::orders_hashmap.erase(order->order_id);

			auto output_time = getCurrentTimestamp();
			Output::OrderDeleted(order->order_id, true, output_time);

			// if there are no more orders at this price level, delete the PriceLevelNode
			if (curr->orders_list.empty())
			{
				//SyncCerr{} << "[DEBUG] cancelOrder: PriceLevelNode with price " << curr->price << " is empty and will be deleted." << std::endl;
				prev->next = curr->next;
				currLock.unlock(); 
				delete curr;
			}
			return;
		}

		// we cannot unlock earlier because we might need to delete current PriceLevelNode
		prevLock.unlock();
		prev = curr;
		prevLock = std::move(currLock);
		curr = curr->next;
	}

	//SyncCerr{} << "[DEBUG] cancelOrder: PriceLevelNode with price " << order->price << " is not found, cancel order fails" << std::endl;
	auto output_time = getCurrentTimestamp();
	Output::OrderDeleted(order->order_id, false, output_time);
}