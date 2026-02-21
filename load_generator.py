import socket
import struct
import time
import random

def run_load_test(num_orders=100000):
    # < = little-endian, B=1b, Q=8b, Q=8b, I=4b, I=4b, B=1b, 8s=8b string
    msg_format = '<BQQIIB8s'
    symbols = [
        b'AAPL\x00\x00\x00\x00', 
        b'TSLA\x00\x00\x00\x00', 
        b'MSFT\x00\x00\x00\x00'
    ]
    
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.connect(('localhost', 8080))
        
        print(f"Connected! Blasting {num_orders} orders...")
        start_time = time.time()
        
        for i in range(num_orders):
            msg_type = 1
            timestamp = int(time.time_ns()) 
            order_id = i
            price = random.randint(14900, 15100) # Prices between $149.00 and $151.00
            quantity = random.randint(1, 100)
            side = random.choice([0, 1]) # 0 = Buy, 1 = Sell
            symbol = random.choice(symbols)
            
            binary_payload = struct.pack(msg_format, msg_type, timestamp, order_id, price, quantity, side, symbol)
            s.sendall(binary_payload)
            
        end_time = time.time()
        s.close()
        
        duration = end_time - start_time
        print(f"Done! Sent {num_orders} orders in {duration:.4f} seconds.")
        print(f"Throughput: {num_orders / duration:,.0f} orders/sec")
        
    except ConnectionRefusedError:
        print("Could not connect. Is your C++ server running?")

if __name__ == "__main__":
    run_load_test(100000) # Number of orders