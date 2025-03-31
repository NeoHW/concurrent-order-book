import os
import random

def generate_test_case():
    script_dir = os.path.dirname(os.path.abspath(__file__))
    filename = os.path.join(script_dir, "random.in")
    
    try:
        num_clients = int(input("Enter number of clients (1-40): "))
        if not (1 <= num_clients <= 40):
            raise ValueError("Number of clients must be between 1 and 40.")
        num_instruments = int(input("Enter number of instruments: "))
        if num_instruments < 1:
            raise ValueError("There must be at least 1 instrument.")
        num_orders = int(input("Enter number of orders: "))
        if num_orders < 1:
            raise ValueError("There must be at least 1 order.")
    except ValueError as e:
        print("Invalid input:", e)
        return
    
    # Generate instrument names as SYM1, SYM2, ..., SYM<num_instruments>
    instruments = [f"SYM{i+1}" for i in range(num_instruments)]
    
    order_id = 0
    # Store active orders as a list of tuples: (order_id, client_id)
    active_orders = []
    
    with open(filename, "w") as f:
        # Write number of clients on the first line.
        f.write(f"{num_clients}\n")
        
        # Write a single connection command ("o").
        f.write("o\n")
        
        # Generate orders
        for _ in range(num_orders):
            # Randomly select an action
            action = random.choice(["buy", "sell", "cancel"])
            
            if action in ["buy", "sell"]:
                # Choose a random client id for creation
                client_id = random.randint(0, num_clients - 1)
                instrument = random.choice(instruments)
                price = random.randint(50, 500)
                quantity = random.randint(1, 100)
                # Format: <client_id> <B/S> <order_id> <instrument> <price> <quantity>
                cmd = f"{client_id} {'B' if action=='buy' else 'S'} {order_id} {instrument} {price} {quantity}\n"
                f.write(cmd)
                active_orders.append((order_id, client_id))
                order_id += 1
            elif action == "cancel" and active_orders:
                # Pick an active order along with its creator client id
                cancel_order_id, creator_client = random.choice(active_orders)
                # Format: <client_id> C <order_id>
                cmd = f"{creator_client} C {cancel_order_id}\n"
                f.write(cmd)
                active_orders.remove((cancel_order_id, creator_client))
            else:
                # If action is cancel but no active orders exist, default to a buy order
                client_id = random.randint(0, num_clients - 1)
                instrument = random.choice(instruments)
                price = random.randint(50, 500)
                quantity = random.randint(1, 100)
                cmd = f"{client_id} B {order_id} {instrument} {price} {quantity}\n"
                f.write(cmd)
                active_orders.append((order_id, client_id))
                order_id += 1
        
        # Write end-of-input command.
        f.write("x\n")
    
    print("Test case generated:", filename)

if __name__ == "__main__":
    generate_test_case()

