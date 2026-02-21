import socket
import struct
import time
import random
import threading

def trader_bot(trader_id, num_orders):
    msg_format = '<BQQIIB8s'
    symbols = [b'AAPL\x00\x00\x00\x00', b'TSLA\x00\x00\x00\x00', b'MSFT\x00\x00\x00\x00']
    
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.connect(('localhost', 8080))
        
        for i in range(num_orders):
            binary_payload = struct.pack(msg_format, 1, int(time.time_ns()), i, 
                random.randint(14900, 15100), random.randint(1, 100), 
                random.choice([0, 1]), random.choice(symbols)
            )
            s.sendall(binary_payload)
            
        s.close()
    except Exception as e:
        print(f"Trader {trader_id} failed: {e}")

def run_concurrent_test(num_traders=10, orders_per_trader=20000):
    print(f"Starting {num_traders} concurrent traders...")
    start_time = time.time()
    
    threads = []
    for i in range(num_traders):
        t = threading.Thread(target=trader_bot, args=(i, orders_per_trader))
        threads.append(t)
        t.start()
        
    for t in threads:
        t.join()
        
    duration = time.time() - start_time
    total_orders = num_traders * orders_per_trader
    
    print(f"Done! Processed {total_orders} orders from {num_traders} connections in {duration:.4f} seconds.")
    print(f"Throughput: {total_orders / duration:,.0f} orders/sec")

if __name__ == "__main__":
    run_concurrent_test(50, 4000)