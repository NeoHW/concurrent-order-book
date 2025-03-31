import os
import random

def generate_test_case():
    script_dir = os.path.dirname(os.path.abspath(__file__))
    filename=os.path.join(script_dir, "basic-random.in")

    symbols = ["AAPL", "GOOG", "MSFT", "TSLA", "AMZN"]
    order_id = 0
    active_orders = []
    
    with open(filename, "w") as f:
        f.write("# Single-threaded test case\n\n")
        f.write("1\n") 
        
        f.write("o\n")
        
        for _ in range(5000):
            action = random.choice(["buy", "sell", "cancel"])
            
            if action in ["buy", "sell"]:
                symbol = random.choice(symbols)
                price = random.randint(50, 500)
                quantity = random.randint(1, 100)
                
                f.write(f"{'B' if action == 'buy' else 'S'} {order_id} {symbol} {price} {quantity}\n")
                
                active_orders.append(order_id)
                order_id += 1
            
            elif action == "cancel" and active_orders:
                cancel_id = random.choice(active_orders)
                f.write(f"C {cancel_id}\n")
                active_orders.remove(cancel_id)
        
        f.write("x\n")

if __name__ == "__main__":
    generate_test_case()
    print("Test case generated")