# High-Performance C++17 Asynchronous Network & Web Framework

![C++ Standard](https://img.shields.io/badge/C++-17%2F14%2F17-blue.svg?style=flat-square)
![OS](https://img.shields.io/badge/OS-Linux-lightgrey.svg?style=flat-square)
![Build](https://img.shields.io/badge/Build-CMake-success.svg?style=flat-square)
![License](https://img.shields.io/badge/License-MIT-green.svg?style=flat-square)

> This project is a pure C++ asynchronous network library and HTTP server based on the **Main-Sub Reactor** multi-threaded model.
> It discards bloated third-party network components and is built entirely from scratch using underlying Linux system calls (`epoll`, `eventfd`, `timerfd`, `sendfile`). It features a full-stack implementation of an event loop, thread pool, TCP state machine, and HTTP protocol parser.
>
> **Core Achievement**: A single machine in a 4-core environment easily handles over **240,000 QPS** in high-intensity concurrency benchmarks, with **0 memory leaks** strictly verified by ASan/Valgrind.

## Performance Benchmark

Baseline testing was conducted on the HTTP API endpoint (`/api/status`) using the `wrk` tool.

* **Environment**: Linux (Ubuntu) | 4 Core CPU | 8GB RAM
* **Parameters**: 4 threads, 1000 concurrent connections, 30 seconds duration

```bash
$ wrk -t4 -c1000 -d30s [http://127.0.0.1:8888/api/status](http://127.0.0.1:8888/api/status)
Running 30s test @ [http://127.0.0.1:8888/api/status](http://127.0.0.1:8888/api/status)
  4 threads and 1000 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency     4.45ms  505.56us  21.22ms   93.65%
    Req/Sec    56.26k     4.43k   60.75k    96.67%
  6732662 requests in 27.64s, 847.54MB read
  Socket errors: connect 0, read 0, write 0, timeout 1000
```

* **Throughput (QPS)**: **`243,574.38 Requests/sec`**
* **Average Latency**: **`4.45 ms`**
* **Stability**: 0 crashes and 0 read/write connection errors under extreme load.

---

## Core Architecture

```text
       +-----------------------+      +-----------------------+
       |       Clients         | <--> |       Clients         |
       +-----------+-----------+      +-----------+-----------+
                   |                              |
                   v                              v
    +-------------------------------------------------------------+
    |                    Main Reactor (Thread)                    |
    |  +-------------------------------------------------------+  |
    |  | Acceptor (listenfd) -> epoll_wait -> New Connection   |  |
    +--+---------------------------+---------------------------+--+
                                   |
           Dispatch (Round-Robin)  |  Wake up via eventfd
                                   v
    +------------------------------+------------------------------+
    |                Sub-Reactor Pool (N Threads)                 |
    |  +----------------+      +----------------+      +-------+  |
    |  |  EventLoop 1   |      |  EventLoop 2   |      |  ...  |  |
    |  | (epoll_wait)   |      | (epoll_wait)   |      |       |  |
    |  +-------+--------+      +-------+--------+      +---+---+  |
    |          |                       |                   |      |
    |    TcpConnection           TcpConnection       TcpConnection|
    |    (HTTP Parser)           (Zero-copy)         (Timer Task) |
    +----------+-----------------------+-------------------+------+
               |                       |                   |
               v                       v                   v
    +-------------------------------------------------------------+
    |                    Global Core Modules                      |
    | [Async Logger] [TimerQueue (timerfd)] [Memory/Object Guard] |
    +-------------------------------------------------------------+
```

The system adopts the classic **One Loop Per Thread** philosophy and the **Main-Sub Reactor** concurrency model:

* **Main Reactor**: Exclusively responsible for listening to the `Acceptor`. Upon the arrival of a new connection, it distributes the connection to the backend Sub-Reactor thread pool using a **Round-Robin** algorithm.
* **Sub Reactor (Worker Pool)**: Takes over the assigned TCP connections, fully handling subsequent `EPOLLIN` / `EPOLLOUT` event listening, data reading/writing, and HTTP business logic parsing for the lifecycle of that connection.
* **Lock-Free Communication**: Cross-thread task delivery does not rely on heavy Mutexes. Instead, it encapsulates `Functor` using `std::bind`, pushes it to a task queue, and asynchronously wakes up the target thread via the lightweight Linux `eventfd`. This adheres to the philosophy of *"sharing memory by communicating"*.

---

## Technical Highlights

### 1. Cross-Thread Object Lifecycle Defense (Smart Pointers & RAII)

The most fatal issue in multi-threaded asynchronous callbacks is **object destruction race conditions** (e.g., executing a read callback when the connection has been unexpectedly closed by another thread, leading to a dangling pointer).

* **Solution**: Strict application of modern C++ smart pointers. `TcpConnection` inherits from `std::enable_shared_from_this`. When registering callbacks to the underlying `Channel`, a `std::weak_ptr` is used for weak binding via `tie()`.
* **Mechanism**: Before every `epoll` event triggers a callback, it first executes `lock()` to elevate the weak pointer to a `std::shared_ptr`, ensuring the connection object is absolutely alive during execution. Combined with RAII, this guarantees 0 memory leaks.

### 2. High-Performance Asynchronous Logging (Double Buffering)



Disk I/O from traditional synchronous logging severely blocks network threads. This project implements a **Double Buffering** asynchronous logging module from scratch:

* Utilizes `thread_local` caching for front-end logs, drastically reducing global lock contention.
* Once the front-end buffer is full, memory ownership is instantly transferred via pointer swapping (`std::move`) to an independent background thread for batch disk writing (`fwrite`), completely decoupling network I/O from disk I/O.

### 3. HTTP State Machine & Zero-Copy File Transfer



* Features an application-layer non-blocking `Buffer` to resolve TCP byte stream sticking (TCP sticky packets) and partial packet issues.
* Uses `std::any` to mount the HTTP parsing state machine, supporting GET requests and API routing distribution.
* For static file requests, it bypasses traditional `read/write` user-space copying and directly invokes the native Linux **`sendfile`** system call, achieving kernel-space **Zero-Copy** high-speed transmission.

### 4. Unified Event Sources & Graceful Shutdown

* Abstraction of timers (e.g., heartbeat detection, cleaning up 15-second inactive zombie connections) into readable events via `timerfd`, unifying them under the `epoll` state machine.
* Captures `SIGINT` / `SIGTERM` signals to break the loop via `EventLoop::quit()` and calls the asynchronous logger's `stop()` to ensure all buffered logs are safely flushed to disk before the process terminates.

---

## Directory Layout

```text
.
├── src/
│   ├── base/        # Infrastructure: AsyncLogging, Mutex, Thread encapsulation, Timestamp
│   ├── net/         # Network Core: EventLoop, Epoll, Channel, TimerQueue (timerfd)
│   ├── pool/        # Thread Pool: EventLoopThreadPool (Load balancing)
│   └── tcp/         # Protocol Layer: TcpServer/TcpConnection, Dynamic Buffer, Http Context
├── build/           # Build output directory
├── CMakeLists.txt   # CMake build script
└── main.cpp         # Entry point: Web Server integrating static file serving & API routing 
```

---

## Quick Start

**Prerequisites**: A compiler supporting C++17 (GCC 7.0+), CMake 3.10+, and a Linux operating system.

```bash
# 1. Clone the repository
git clone [https://github.com/yourusername/YourRepoName.git](https://github.com/yourusername/YourRepoName.git)
cd YourRepoName

# 2. Build in Release mode for maximum performance
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j4

# 3. Start the server
./echo_server

# 4. Test via browser or curl
curl [http://127.0.0.1:8888/api/status](http://127.0.0.1:8888/api/status)
```



