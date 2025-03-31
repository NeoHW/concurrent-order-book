# Exchange Matching Engine in C++

This repository contains a C++ implementation of a multi-threaded exchange matching engine. It handles new buy/sell orders, partial or full matching against an order book, and cancellation of existing orders. The engine is designed to demonstrate concurrency using fine-grained locking mechanisms.

## Table of Contents

1. [Overview](#overview)  
2. [Features](#features)  
3. [Build Instructions](#build-instructions)  
4. [Usage](#usage)  
   - [Starting the Engine](#starting-the-engine)  
   - [Client Usage](#client-usage)  
   - [Command Format](#command-format)  
5. [System Architecture](#system-architecture)  
   - [Data Structures](#data-structures)  
   - [Concurrency and Synchronization](#concurrency-and-synchronization)  

---

## Overview

This matching engine continuously listens for incoming client connections via a UNIX domain socket. Each client can send orders (buy or sell) or cancel existing orders. When an order arrives, the engine attempts to match it against the corresponding opposite-side order book. If the order is partially filled or not filled at all, the remainder of the order is stored (“rested”) in the order book for future matching.

Key characteristics:

- **Fine-grained concurrency**: Each instrument’s order book is accessed independently. Hand-over-hand locking within each order book ensures minimal blocking for parallel operations.
- **Price-time priority**: Orders in the same price level are matched in FIFO (first-in-first-out) order, preserving time priority.
- **Cancellation support**: Existing orders can be canceled efficiently by referencing the stored order ID.

---

## Features

1. **ConcurrentHashMap**  
   - Custom concurrent hash map to manage:
     - (Instrument → `OrderBook*`)
     - (Order ID → `std::shared_ptr<Order>`)  
   - Bucket-level locking allows multiple reads in parallel.

2. **Multi-threaded**  
   - Each new client connection spawns a thread to handle incoming commands.
   - Orders for different instruments run concurrently with minimal locking overhead.

3. **Order matching**  
   - Orders match against opposite sides (buy vs. sell) if prices cross.
   - Partial matches reduce the active order quantity; leftover quantities become resting orders if unfilled.

4. **Price-level management**  
   - Orders with the same price share a **PriceLevelNode**.
   - Each price level node tracks total volume and a list of orders.

5. **Cancellations**  
   - Searching for an order by order ID is O(1) on average, thanks to a concurrent hash map.
   - Cancels remove the order from the order book (if not already matched).

---

## Build Instructions

### Requirements

- A C++17 (or newer) compiler (e.g., `g++` or `clang++`).
- A POSIX environment (Linux) that supports UNIX domain sockets.

### Steps

1. **Clone or download** this repository to your local machine.
2. Open a terminal in the root directory.
3. To build, simply run `make`

## Usage

### Starting the Engine

Run the engine (exchange server):

```bash
./engine <socket_path>
```
- `<socket_path>` is the path to the UNIX domain socket file.
- The engine creates and listens on this socket.
- The engine continues running, waiting for client connections. Terminate with `Ctrl+C` or send a termination signal.
- On exit, it cleans up the socket file.

### Client usage
Run the client:

```bash
./client <socket_path> < input.txt
```

- `<socket_path>` must match what you specified for the engine.
- `input.txt` is a file containing the commands to send.
- The client reads commands (one per line) from standard input and sends them to the engine.
- It then waits for server output until the session ends or the file is fully read.
- There are some example input files (*.in) provided.

---

# Command Format

## Buy Order

```text
B <order_id> <instrument> <price> <quantity>
```

**Example:**
```text
B 1001 IBM 130 10
```

## Sell Order

```text
S <order_id> <instrument> <price> <quantity>
```

**Example:**
```text
S 1002 IBM 135 5
```

## Cancel Order

```text
C <order_id>
```

**Example:**
```text
C 1001
```

> Lines beginning with `#` or empty lines are ignored.

### Notes

- `<order_id>` is a unique integer identifier for the order.
- `<instrument>` is an 8-character (or fewer) string, e.g., `IBM`.
- `<price>` and `<quantity>` are unsigned integers.

---

# System Architecture

## Data Structures

### ConcurrentHashMap<Key, Value>

- Maps: 
  - `(instrument → OrderBook*)` in one instance
  - `(order_id → std::shared_ptr<Order>)` in another
- Uses per-bucket locks (shared mutexes) to allow multiple concurrent readers.

### OrderBook

- For each instrument, stores two singly linked lists of `PriceLevelNode`:
  - `buy_book` (sorted descending by price)
  - `sell_book` (sorted ascending by price)
- Each list is protected by **hand-over-hand locking** (each node has its own mutex).

### PriceLevelNode

Tracks:
- `price`
- `total_volume` (aggregate quantity of all orders at this price)
- `orders_list` (a container of Orders, preserving FIFO order)
- `std::mutex mtx`
- Pointer to the next node in the list.

### Order

Fields:
- `order_id`
- `instrument`
- `price`
- `count` (remaining quantity)
- `type` (buy/sell)
- `execution_id`: increments for each partial fill

---

# Concurrency and Synchronization

## Concurrency Model

- Each incoming client connection spawns a new thread (`engine->accept()` in `main.cpp`).
- Orders on different instruments run in parallel since each instrument has its own `OrderBook`.

## Fine-grained Locks

- `ConcurrentHashMap`: per-bucket shared locks.
- `OrderBook`: separate instance per instrument to avoid global locks.
- `PriceLevelNode`: each node has its own mutex, used in hand-over-hand locking.

## Matching Strategy

### Buy Order Flow

- Lock relevant parts of the sell book to find matching orders at or below the buy price.
- Execute trades until the buy order is fully matched or no further matches are possible.
- If partially unfilled, rest it in the buy book.

### Sell Order Flow

- Symmetric approach to the buy flow.

## Cancellation

- Look up the order in a concurrent hash map by `order_id`.
- Traverse the relevant list with hand-over-hand locking to remove it if found.