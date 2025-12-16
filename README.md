# Routing Engine

A Python query engine for shortest path routing on **H3 Spatial Hierarchy** shortcuts.

## Overview

This project implements bidirectional search algorithms for querying precomputed hierarchical shortcuts. The hierarchy is based on **H3 hexagonal grid resolutions** (not node contraction like traditional CH).

> [!NOTE]
> **Not Contraction Hierarchies (CH)**
> 
> This uses H3 spatial tree decomposition instead of node contraction order. See [tree_decomposition.md](../shortcuts-generation/docs/tree_decomposition.md) for theory.

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

| Algorithm | Description | Speedup |
|-----------|-------------|---------|
| **One-to-One Classic** | Bidirectional Dijkstra with `inside` filtering | Baseline |
| **One-to-One Pruned** | + H3 hierarchy `parent_check` + early termination | 1.3-2x |
| **Many-to-Many** | Multi-source/target initialization | N/A |

### Inside Values

| Value | Meaning | Used By |
|-------|---------|---------|
| `+1` | Upward shortcut (to coarser cell) | Forward search |
| `0` | Lateral shortcut (same level) | Backward @ high_cell |
| `-1` | Downward shortcut (to finer cell) | Backward search |
| `-2` | Edge shortcut (direct edge) | Backward for globals |

## Related Projects

| Project | Role |
|---------|------|
| [osm-to-road-network](../osm-to-road-network) | OSM → Road graph |
| [shortcuts-generation](../shortcuts-generation) | Graph → CH shortcuts |
| **routing-engine** (this) | Query engine |
