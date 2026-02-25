# ⚡ High-Frequency Trading (HFT) Matching Engine & Profiler

An ultra-low latency **C++20 Limit Order Book (LOB)** built for maximum throughput and deterministic performance.

This project focuses on eliminating traditional OS bottlenecks through:

* Lock-free concurrency
* Zero-allocation matching
* Custom memory management
* Binary wire protocol ingestion
* Integrated performance benchmarking

Designed to explore real-world matching engine constraints such as cache coherence, network ingestion pressure, and allocator contention.

---

## 🚀 Architecture Highlights

### 🧠 Zero-Allocation Hot Path

Standard heap allocation is replaced with a custom fixed-size memory pool for `Order` objects.

**Result:**

* No allocator contention
* No global heap lock
* Deterministic memory reuse
* True zero allocations during matching

---

### 🔒 Lock-Free Concurrency (SPSC)

A **Single-Producer / Single-Consumer pipeline** decouples:

* Network ingestion
* Matching engine processing

This prevents burst traffic from stalling the core engine.

**Implementation:**

* Lock-free queue
* Cache-aware data flow
* Minimal synchronization overhead

---

### 📦 Binary Wire Protocol (Zero-Copy Parsing)

Orders are transmitted using a compact fixed-size binary struct.

**Benefits:**

* No parsing overhead
* No serialization cost
* Direct memory reinterpretation
* Fully deterministic packet handling

---

### 🧪 Integrated Benchmark Harness

Built-in CLI benchmarking enables **hardware-level throughput testing** without external tooling.

Capable of processing:

> **10 million orders in under 2 seconds** on consumer hardware.

---

## 📊 Performance Benchmarks (Local Hardware)

| Execution Mode | Threading       | Memory      | Throughput       |
| -------------- | --------------- | ----------- | ---------------- |
| Benchmark      | Synchronous     | Memory Pool | **~6.2M OPS**    |
| Benchmark      | Synchronous     | OS Heap     | ~5.0M OPS        |
| Benchmark      | Lock-Free Queue | Memory Pool | ~3.6M OPS*       |
| Live Network   | Lock-Free Queue | Memory Pool | ~35K – 60K OPS** |

### Performance Notes

* Queue mode slowdown is caused by cross-core cache coherence traffic
(MESI protocol, cache line migration, L1/L2 misses).

** Live throughput is currently limited by the Python TCP load generator.

---

## 🛠 Technology Stack

### Engine

* C++20
* Lock-free data structures
* Custom memory pools
* Atomic operations

### Networking

* TCP raw binary protocol
* Zero-copy message parsing
* Packet fragmentation handling

### Telemetry / Visualization

* Python 3.12
* Streamlit dashboard
* Pandas / NumPy analytics

---

## ⚙️ Build & Run

### 🔧 Compile Engine

```bash
g++ -std=c++20 -O3 -Wall -Wextra -pthread *.cpp -o engine \
    -lboost_system -lboost_thread
```

---

### 🧪 Offline Hardware Benchmark

Measure raw engine throughput without networking overhead.

```bash
# Usage:
# ./engine <mode: live/test> <threading: queue/sync> <memory: mempool/os>

./engine test sync mempool
```

---

### 🌐 Live Server Mode

Start matching engine:

```bash
./engine live queue mempool
```

Launch monitoring dashboard:

```bash
streamlit run dashboard.py
```

---

## 📈 Engineering Insights

### The I/O Tax

Initial throughput was limited by disk writes.

Reducing snapshot frequency to once every **250,000 orders**
eliminated kernel-level blocking and restored linear scaling.

---

### TCP Fragmentation Handling

Incoming packets may arrive split across multiple reads.

A ring-buffer shift strategy ensures:

* Struct alignment is preserved
* Binary reinterpretation is always safe
* No partial-packet corruption

---

### Memory Layout Determinism

Explicit packed struct alignment guarantees identical layout across languages.

Result:

* Python sender and C++ receiver share exact binary mapping
* No padding
* No transformation
* True zero-copy communication

---

## 🎯 Project Goals

* Explore real-world matching engine performance constraints
* Measure allocator and cache coherence impact
* Demonstrate lock-free pipeline behavior
* Build reproducible latency and throughput benchmarks

---

## 📜 License

MIT License
