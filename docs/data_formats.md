# Data Formats

Input data for the routing engine comes from the [shortcuts-generation](../../shortcuts-generation) project.

---

## Shortcuts Parquet

### Schema

| Column | Type | Description |
|--------|------|-------------|
| `incoming_edge` | int32 | Starting edge ID |
| `outgoing_edge` | int32 | Ending edge ID |
| `via_edge` | int32 | Intermediate edge for path expansion |
| `cost` | float64 | Total travel cost |
| `cell` | int64 | H3 cell for query filtering |
| `inside` | int8 | Direction indicator |

### Inside Values

| Value | Direction | Description |
|-------|-----------|-------------|
| **+1** | Upward | Forward search only |
| **0** | Lateral | Backward search (in high cell only) |
| **-1** | Downward | Backward search only |
| **-2** | Outer-only | Only valid for outer cell merges |

---

## Edge Metadata CSV

### Schema

| Column | Type | Description |
|--------|------|-------------|
| `id` | int32 | Edge identifier |
| `incoming_cell` | int64 | H3 cell at edge end |
| `outgoing_cell` | int64 | H3 cell at edge start |
| `lca_res` | int32 | LCA resolution |
| `length` | float64 | Edge length (meters) |
| `cost` | float64 | Edge traversal cost |

---

## Query Output

```python
@dataclass
class QueryResult:
    distance: float        # Total path cost
    path: list[int]        # Edge IDs
    reachable: bool        # True if path found
```
