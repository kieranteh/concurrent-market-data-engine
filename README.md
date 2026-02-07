# Concurrent Market Data Engine

A high-performance, concurrent market data processing engine built with C++, visualized in real-time using a Node.js backend and React frontend.

## Overview

This project simulates a high-frequency trading environment where market data events (orders) are generated and processed concurrently.

*   **Engine (C++)**: The core logic. It uses a producer-consumer pattern with lock-free queues to process events. It maintains an order book for multiple stock symbols (e.g., AAPL, MSFT), matches trades, and calculates latency metrics.
*   **Backend (Node.js)**: Acts as a bridge. It spawns the C++ engine process, parses its standard output (JSON), and broadcasts updates to the frontend via WebSockets.
*   **Frontend (React)**: A real-time dashboard displaying throughput, processing stats, and a live log of executed trades.

## Project Structure

```
concurrent-market-data-engine/
├── engine/         # C++ core logic (Producers, Consumers, OrderBook)
├── backend/        # Node.js Express server & Socket.io
└── frontend/       # React.js dashboard
```

## Features

*   **High Performance**: C++20 engine using `std::atomic` and lock-free data structures.
*   **Concurrency**: Multi-threaded architecture with sharded processing queues to minimize contention.
*   **Order Matching**: Simulates a limit order book with trade execution logic.
*   **Real-time Visualization**: Live updates of throughput (events/sec), total processed events, and trade execution logs.
*   **Interactive Control**: Ability to restart the simulation from the web interface.

## Setup and Running

### Prerequisites
*   C++ Compiler (GCC/MinGW)
*   CMake
*   Node.js & npm

### 1. Build the C++ Engine
```bash
cd engine
mkdir build
cd build
cmake .. -G "MinGW Makefiles"
cmake --build .
```

### 2. Start the Backend
```bash
cd backend
npm install
node server.js
```
The backend will launch the C++ executable and listen on port 4000.

### 3. Start the Frontend
```bash
cd frontend
npm install
npm start
```
Open http://localhost:3000 to view the dashboard.