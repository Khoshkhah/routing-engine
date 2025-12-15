# One-to-One Classic Algorithm

Basic bidirectional Dijkstra search on Contraction Hierarchies.

## Overview

This algorithm finds the shortest path between a single source edge and target edge using bidirectional search with `inside` directional filtering.

```
         Source                              Target
           │                                   │
           ▼                                   ▼
    ┌──────────────┐                   ┌──────────────┐
    │ Forward      │                   │ Backward     │
    │ Search       │◄──── Meeting ────►│ Search       │
    │ (inside=+1)  │      Point        │ (inside≤0)   │
    └──────────────┘                   └──────────────┘
```

## Algorithm

### Initialization

```python
dist_fwd[source] = 0.0
dist_bwd[target] = target_edge_cost
pq_fwd.push((0.0, source))
pq_bwd.push((target_edge_cost, target))
```

### Forward Search
- Expand edges where `inside == +1` (upward shortcuts only)
- Update distances and parent pointers
- Check for meeting with backward search

### Backward Search
- Expand edges where `inside == -1` (downward shortcuts)
- Also allow `inside == 0` (lateral) in same cell as high_cell
- Update distances and parent pointers
- Check for meeting with forward search

### Termination
Stop when both priority queues have top elements ≥ best found path.

### Path Reconstruction
1. Trace parent pointers from meeting edge to source (forward)
2. Trace parent pointers from meeting edge to target (backward)
3. Concatenate paths

## Pseudocode

```python
def query(source_edge, target_edge):
    # Initialize
    dist_fwd = {source_edge: 0.0}
    dist_bwd = {target_edge: get_edge_cost(target_edge)}
    pq_fwd = [(0.0, source_edge)]
    pq_bwd = [(dist_bwd[target_edge], target_edge)]
    best = infinity
    meeting = None
    
    while pq_fwd or pq_bwd:
        # Forward step
        if pq_fwd:
            d, u = heappop(pq_fwd)
            if d >= best: continue
            
            for shortcut in fwd_adj[u]:
                if shortcut.inside != 1:
                    continue
                v = shortcut.to
                new_dist = d + shortcut.cost
                if new_dist < dist_fwd.get(v, inf):
                    dist_fwd[v] = new_dist
                    heappush(pq_fwd, (new_dist, v))
                    if v in dist_bwd:
                        total = new_dist + dist_bwd[v]
                        if total < best:
                            best = total
                            meeting = v
        
        # Backward step (symmetric, inside == -1 or 0)
        ...
        
        # Early termination
        if pq_fwd[0][0] + pq_bwd[0][0] >= best:
            break
    
    return reconstruct_path(meeting)
```

## Complexity

- **Time**: O((|E| + |V|) log |V|) — standard Dijkstra
- **Space**: O(|V|) for distance arrays
