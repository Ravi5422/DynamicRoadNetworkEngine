# Dynamic Road Network Engine

A high-performance C++ and Python simulation and routing engine designed for dynamic city road networks, real-time traffic updates, and intelligent fleet delivery scheduling.

---

## 📌 Overview

The **Dynamic Road Network Engine** is a modular, multi-phase routing and logistics system built in C++17. It models complex city road infrastructures, supports real-time edge updates (traffic delays, road closures, speed profile alterations), and solves multi-objective routing problems such as time-dependent shortest paths, K-Shortest Paths, approximate distance heuristics, and fleet dispatch optimization.

## 🏗️ Architecture & Phases

The project is structured into three progressive architectural phases:

### Phase 1: Static Road Network & Core Queries
- **Graph Representation**: Efficient adjacency list and hash map representation of road networks with support for multi-attribute edges (length, average travel time, 24-hour speed profiles, one-way constraints, road types).
- **Dynamic Updates**: Support for real-time edge removal and edge property modifications (`modify_edge`, `remove_edge`).
- **Shortest Path Algorithms**: Dijkstra and A* algorithms supporting both distance and time optimization modes, with support for complex routing constraints (forbidden nodes, forbidden road types).
- **K-Nearest Neighbors (KNN)**: Spatial KNN search by Euclidean distance and network travel time/distance.

### Phase 2: Advanced Routing & Heuristics
- **K-Shortest Paths**: Implementation of Yen's Algorithm for finding exact $K$ shortest loopless paths between source and destination nodes.
- **Heuristic K-Shortest Paths**: Modified path finding with overlap thresholding to generate diverse, alternative driving routes without excessive overlap.
- **Approximate Shortest Paths**: Bounded-error approximate distance queries (`approx_shortest_path`) operating within tight millisecond time budgets for rapid city-scale distance matrices.

### Phase 3: Real-Time Fleet & Delivery Scheduling
- **Order Management & Priority Scoring**: Dynamic handling of regular and premium delivery orders with time-dependent priority scoring based on order value and premium status.
- **K-Means Clustering**: Spatial clustering of delivery orders based on pickup-dropoff midpoints to efficiently partition workload among available delivery drivers.
- **Shadow Graph Route Insertion**: An insertion heuristic operating on shadow graphs to solve the Pickup and Delivery Problem (PDP) with time windows and priority weighting.
- **Traffic Simulation**: Step-by-step route simulation over live road networks accounting for time-dependent speed profiles and unreachable stop handling.

---

## 📂 Project Structure

```text
DynamicRoadNetworkEngine/
├── Makefile                # Top-level build configuration for all phases
├── Report.pdf              # Comprehensive technical report and algorithm details
├── Phase-1/                # Phase 1 source code & headers
│   ├── Graph.hpp           # Core graph structures and class definitions
│   ├── functions.cpp       # Dijkstra, A*, and KNN query implementations
│   ├── graph_utils.cpp     # Graph construction and edge modification utilities
│   └── main.cpp            # Phase 1 CLI execution entry point
├── Phase-2/                # Phase 2 source code & headers
│   ├── Graph.hpp           # Extended graph definitions for path structures
│   ├── functions.cpp       # Yen's algorithm, heuristic K-paths, and approx distance
│   ├── graph_utils.cpp     # Helper functions and time-dependent travel time models
│   └── main.cpp            # Phase 2 CLI execution entry point
├── Phase-3/                # Phase 3 source code & headers
│   ├── graph.hpp / .cpp    # A* and Haversine distance models
│   ├── scheduler.hpp / .cpp# Fleet dispatch, K-means clustering, and PDP solver
│   ├── main.cpp            # Phase 3 simulation engine entry point
│   └── generate_test_data.py# Python test generator for road networks & delivery events
└── nlohmann/               # Modern C++ JSON library dependency
    └── json.hpp
```

---

## 🛠️ Building the Project

The project includes a top-level `Makefile` that builds optimized binaries for all three phases using `g++` with C++17 support.

### Prerequisites
- **Compiler**: GCC/Clang with C++17 support (`g++` >= 7.0 or `clang++` >= 5.0)
- **Python**: Python 3.8+ (for running test data generation scripts in Phase 3)

### Compilation

To compile all three phases simultaneously:

```bash
make all
```

This generates three executable binaries in the root directory:
- `phase1` (Compiled with `-O3`)
- `phase2` (Compiled with `-O3`)
- `phase3` (Compiled with `-O2 -Wextra -pedantic`)

To clean compiled binaries:

```bash
make clean
```

---

## 🚀 Running the Engine

Each phase binary takes three command-line arguments: the input graph JSON, the query/event JSON, and the desired output JSON file.

### Phase 1 Execution
```bash
./phase1 Phase-1/graph.json Phase-1/queries.json output_phase1.json
```

### Phase 2 Execution
```bash
./phase2 Phase-2/graph.json Phase-2/queries.json output_phase2.json
```

### Phase 3 Execution
```bash
./phase3 Phase-3/graph.json Phase-3/queries.json output_phase3.json
```

---

## 🧪 Generating Test Data

Phase 3 includes a Python utility `generate_test_data.py` that generates synthetic city networks, road speed profiles, and delivery order streams.

```bash
cd Phase-3
python3 generate_test_data.py
```

This script produces:
1. `graph.json` – A geometric city road graph with random speed profiles and coordinates.
2. `queries.json` – A sequence of simulated fleet dispatch events with regular and premium orders.
3. `expected_output.json` – Baseline delivery route structures for validation.

---

## 📊 Input/Output Format

### Graph Specification (`graph.json`)
```json
{
  "meta": {
    "nodes": 100,
    "description": "City Road Network"
  },
  "nodes": [
    { "id": 0, "lat": 19.0760, "lon": 72.8777, "pois": ["depot", "fuel"] }
  ],
  "edges": [
    {
      "id": 0,
      "u": 0,
      "v": 1,
      "length": 450.5,
      "average_time": 30.2,
      "oneway": false,
      "road_type": "primary",
      "speed_profile": [15.0, 15.0, 12.5, ...]
    }
  ]
}
```

### Query Events (`queries.json`)
The engine handles dynamic event types in sequential order:
- `modify_edge`: Patch edge travel times or speed profiles dynamically.
- `remove_edge`: Simulate road closures or accidents.
- `shortest_path`: Query optimal route by distance or time with node/road constraints.
- `knn`: Locate nearest points of interest (e.g., nearest hospitals or gas stations).
- `k_shortest_paths`: Retrieve top $K$ alternative routes.
- `approx_shortest_path`: Fast approximate distance queries within time budgets.

---

## 📄 License
This project is open-source and available for educational and research use in transportation network optimization and algorithmic routing.
