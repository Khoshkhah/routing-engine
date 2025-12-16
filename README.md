# Routing Engine

A Python query engine for shortest path routing using H3 spatial hierarchy.

## Overview

This project implements bidirectional Dijkstra search algorithms for querying precomputed shortcuts. It serves as a Python prototype before production C++ implementation.

**Input**: Shortcut Parquet files from [shortcuts-generation](../shortcuts-generation)

## Project Structure

```
routing-engine/
├── docs/
│   ├── data_formats.md           # Input/output schemas
│   └── algorithms/
│       ├── one_to_one_classic.md # Basic bidirectional Dijkstra
│       ├── one_to_one_pruned.md  # H3-filtered pruned search
│       └── many_to_many.md       # Multi-source/target routing
├── notebooks/
│   ├── routing_prototype.ipynb   # Algorithm development
│   └── validation.ipynb          # Correctness testing
├── src/
│   ├── shortcut_graph.py         # Graph loading
│   ├── query.py                  # Routing algorithms
│   └── h3_utils.py               # H3 helpers
└── tests/
```

## Quick Start

```bash
# Install dependencies
pip install -r requirements.txt

# Run prototype notebook
jupyter notebook notebooks/routing_prototype.ipynb
```

## Algorithms

| Algorithm | Description | Use Case |
|-----------|-------------|----------|
| **One-to-One Classic** | Bidirectional Dijkstra with `inside` filtering | Single source/target |
| **One-to-One Pruned** | + H3 hierarchy `parent_check` pruning | Faster single queries |
| **Many-to-Many** | Multi-source/target initialization | KNN routing |

## Related Projects

| Project | Role |
|---------|------|
| [osm-to-road-network](../osm-to-road-network) | OSM → Road graph |
| [shortcuts-generation](../shortcuts-generation) | Graph → Shortcuts |
| **routing-engine** (this) | Query engine |
