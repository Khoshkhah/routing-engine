# Routing Engine

H3-based hierarchical routing engine in Python and C++.

## Overview

This project implements bidirectional search algorithms for shortest path routing using H3 spatial hierarchy. The Python prototype validates the algorithms, and the C++ implementation provides production performance.

**Input**: Shortcut Parquet files from [shortcuts-generation](../shortcuts-generation)

## Quick Start

### Python (Prototype)

```bash
# Create and activate environment
conda create -n routing-engine python=3.10
conda activate routing-engine
pip install -r requirements.txt

# Run notebook
jupyter notebook notebooks/routing_prototype.ipynb
```

### C++ (Production)

```bash
# Activate environment with C++ deps
conda activate routing-engine
conda install -c conda-forge cmake ninja compilers arrow-cpp

# Install H3 (if not present)
./scripts/install_h3.sh

# Build
./scripts/build.sh

# Run query
./cpp/build/routing_engine \
    --shortcuts /path/to/shortcuts \
    --edges /path/to/edges.csv \
    --source 100 --target 200
```

## Project Structure

```
routing-engine/
├── cpp/                           # C++ implementation
│   ├── CMakeLists.txt
│   ├── include/
│   │   ├── shortcut_graph.hpp
│   │   └── h3_utils.hpp
│   └── src/
│       ├── shortcut_graph.cpp
│       ├── h3_utils.cpp
│       └── main.cpp
├── docs/                          # Algorithm documentation
│   ├── data_formats.md
│   └── algorithms/
├── notebooks/                     # Python prototype
│   └── routing_prototype.ipynb
├── scripts/
│   ├── install_h3.sh             # Build H3 from source
│   └── build.sh                  # Build C++ project
└── requirements.txt
```

## Algorithms

| Algorithm | Description | Use Case |
|-----------|-------------|----------|
| **Classic** | Bidirectional Dijkstra with `inside` filtering | Baseline |
| **Pruned** | + H3 hierarchy `parent_check` pruning | Faster single queries |
| **Multi** | Multi-source/target initialization | KNN routing |

## Related Projects

| Project | Role |
|---------|------|
| [osm-to-road-network](../osm-to-road-network) | OSM → Road graph |
| [shortcuts-generation](../shortcuts-generation) | Graph → Shortcuts |
| **routing-engine** (this) | Query engine |
