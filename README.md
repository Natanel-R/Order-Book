# ⚡ High-Frequency Trading (HFT) Matching Engine & Profiler

An ultra-low latency **C++20 Limit Order Book (LOB)** built for maximum throughput and deterministic performance.

This project focuses on eliminating traditional OS bottlenecks through lock-free concurrency, zero-allocation matching, and custom memory management, while ensuring absolute mathematical accuracy through rigorous unit testing.

Designed to explore real-world matching engine constraints such as cache coherence, network ingestion pressure, allocator contention, and market microstructure rules.

---

## 🚀 Architecture Highlights

### 🧠 Zero-Allocation Hot Path
Standard heap allocation is replaced with a custom fixed-size memory pool for `Order` objects.
**Result:**
* No allocator contention
* No global heap lock
* Deterministic memory reuse
* True zero allocations during matching

### 🔒 Lock-Free Concurrency (SPSC)
A **Single-Producer / Single-Consumer pipeline** decouples network ingestion from matching engine processing, preventing burst traffic from stalling the core engine.
**Implementation:**
* Lock-free queue (Boost.Lockfree)
* 128-bit atomic operations for ABA-prevention
* Minimal synchronization overhead

### 🛡️ Deterministic State Validation
The core matching logic is mathematically validated using **Google Test (GTest)** to ensure strict adherence to financial exchange rules.
**Test Coverage Includes:**
* Order lifecycle (CRUD) and memory leak prevention
* Strict Price-Time Priority enforcement
* Complex spread crossing (Partial Fills and "Walking the Book")

### 📦 Binary Wire Protocol (Zero-Copy Parsing)
Orders are transmitted using a compact fixed-size binary struct.
**Benefits:**
* No parsing overhead or serialization cost
* Direct memory reinterpretation
* Fully deterministic packet handling

---

## 📊 Performance Benchmarks (Local Hardware)

| Execution Mode | Threading       | Memory      | Throughput       |
| -------------- | --------------- | ----------- | ---------------- |
| Benchmark      | Synchronous     | Memory Pool | **~6.2M OPS** |
| Benchmark      | Synchronous     | OS Heap     | ~5.0M OPS        |
| Benchmark      | Lock-Free Queue | Memory Pool | ~3.6M OPS* |
| Live Network   | Lock-Free Queue | Memory Pool | ~35K – 60K OPS** |

### Performance Notes
* *Queue mode slowdown is caused by cross-core cache coherence traffic (MESI protocol, cache line migration, L1/L2 misses).*
* ** *Live throughput is currently limited by the Python TCP load generator.*

---

## 🛠 Technology Stack

### Engine
* C++20 (STL, Smart Pointers)
* CMake (Build System)
* Google Test / GTest (Unit Testing)
* Boost.Asio & Boost.Thread
* Custom Memory Pools & 128-bit Atomics (`libatomic`)

### Networking
* TCP raw binary protocol
* Zero-copy message parsing
* Packet fragmentation handling

### Telemetry & Profiling
* Linux `perf` & Flamegraphs (Hardware Counters)
* Python 3.12 (Streamlit dashboard, Pandas, NumPy analytics)

---

## ⚙️ Build & Run

### 🔧 1. Compile the Engine (Out-of-Source Build)
The project uses CMake with specific build configurations. Use `Release` for maximum throughput, or `RelWithDebInfo` to attach debug symbols and frame pointers for hardware profiling.

```bash
mkdir build
cd build

# For Maximum Performance (Default)
cmake -DCMAKE_BUILD_TYPE=Release ..

# For Hardware Profiling & Flamegraphs
# cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo ..

make
```

### ✅ 2. Run the Unit Tests
Verify the integrity of the Limit Order Book's matching logic and queue priority before executing benchmarks.
```bash
./run_tests
```

### 🧪 3. Offline Hardware Benchmark
Measure raw engine throughput without networking overhead.
```bash
# Usage: ./engine <mode: live/test> <threading: queue/sync> <memory: mempool/os>
./engine test sync mempool
```

### 🌐 4. Live Server Mode
Start the matching engine to listen for TCP connections:
```bash
./engine live queue mempool
```
Launch the monitoring dashboard (from the root directory):
```bash
streamlit run dashboard.py
```

### 🔬 5. Hardware Profiling (Linux Only)
Measure L1 cache loads, branch mispredictions, and IPC using the Linux kernel profiler:
```bash
sudo perf stat -d ./engine test sync mempool
```

---

## 📈 Engineering Insights

### Validating the Memory Wall (L1 Cache Coherence)
Using `perf stat`, hardware registers confirmed the efficiency of the custom `MemoryPool`. During a 10-million order benchmark, the engine executes with a **~1.6% L1 cache miss rate** and an IPC of **1.72**. The CPU pipeline is almost entirely bound by branch misprediction (`tma_bad_speculation` at ~98%) rather than memory fetching, proving the elimination of standard OS heap fragmentation.

### Flamegraph Diagnostics
Flamegraphs were utilized to identify thread-scheduling bottlenecks. A call stack visualization revealed that an unused `std::thread` spinning via `std::this_thread::yield()` in synchronous mode was triggering excessive OS context switches. Properly scoping the thread spawn logic halved total CPU cycles and restored strictly single-core execution.

### The I/O Tax
Initial throughput was limited by disk writes. Reducing snapshot frequency to once every **250,000 orders** eliminated kernel-level blocking and restored linear scaling.

### TCP Fragmentation Handling
Incoming packets may arrive split across multiple reads. A ring-buffer shift strategy ensures struct alignment is preserved, binary reinterpretation is safe, and no partial-packet corruption occurs.

### Memory Layout Determinism
Explicit packed struct alignment guarantees identical layout across languages. The Python sender and C++ receiver share an exact binary mapping with no padding, allowing for true zero-copy communication.

---

## 🎯 Project Goals
* Explore real-world matching engine performance constraints
* Measure allocator and cache coherence impact natively on silicon
* Demonstrate lock-free pipeline behavior
* Mathematically prove market microstructure matching rules
* Build reproducible latency and throughput benchmarks

---

## 📜 License
MIT License
