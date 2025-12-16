/**
 * @file shortcut_graph.cpp
 * @brief ShortcutGraph implementation - loading and query algorithms.
 */

#include "shortcut_graph.hpp"
#include "h3_utils.hpp"

#include <arrow/api.h>
#include <arrow/io/api.h>
#include <parquet/arrow/reader.h>

#include <fstream>
#include <queue>
#include <sstream>
#include <limits>
#include <algorithm>
#include <filesystem>

namespace fs = std::filesystem;

// Priority queue entry
struct PQEntry {
    double dist;
    uint32_t edge;
    bool operator>(const PQEntry& o) const { return dist > o.dist; }
};

using MinHeap = std::priority_queue<PQEntry, std::vector<PQEntry>, std::greater<PQEntry>>;

// Helper to load a single parquet file
static bool load_parquet_file(const std::string& filepath, 
                              std::vector<Shortcut>& shortcuts,
                              std::unordered_map<uint32_t, std::vector<size_t>>& fwd_adj,
                              std::unordered_map<uint32_t, std::vector<size_t>>& bwd_adj) {
    arrow::MemoryPool* pool = arrow::default_memory_pool();
    
    std::shared_ptr<arrow::io::ReadableFile> infile;
    PARQUET_ASSIGN_OR_THROW(infile, arrow::io::ReadableFile::Open(filepath, pool));
    
    std::unique_ptr<parquet::arrow::FileReader> reader;
    PARQUET_THROW_NOT_OK(parquet::arrow::OpenFile(infile, pool, &reader));
    
    std::shared_ptr<arrow::Table> table;
    PARQUET_THROW_NOT_OK(reader->ReadTable(&table));
    
    // Handle chunked columns
    auto incoming_chunked = table->GetColumnByName("incoming_edge");
    auto outgoing_chunked = table->GetColumnByName("outgoing_edge");
    auto cost_chunked = table->GetColumnByName("cost");
    auto via_chunked = table->GetColumnByName("via_edge");
    auto cell_chunked = table->GetColumnByName("cell");
    auto inside_chunked = table->GetColumnByName("inside");
    
    for (int chunk = 0; chunk < incoming_chunked->num_chunks(); ++chunk) {
        auto incoming = std::static_pointer_cast<arrow::Int64Array>(incoming_chunked->chunk(chunk));
        auto outgoing = std::static_pointer_cast<arrow::Int64Array>(outgoing_chunked->chunk(chunk));
        auto cost_col = std::static_pointer_cast<arrow::DoubleArray>(cost_chunked->chunk(chunk));
        auto via_col = std::static_pointer_cast<arrow::Int64Array>(via_chunked->chunk(chunk));
        auto cell_col = std::static_pointer_cast<arrow::Int64Array>(cell_chunked->chunk(chunk));
        auto inside_col = std::static_pointer_cast<arrow::Int8Array>(inside_chunked->chunk(chunk));
        
        for (int64_t i = 0; i < incoming->length(); ++i) {
            Shortcut sc;
            sc.from = static_cast<uint32_t>(incoming->Value(i));
            sc.to = static_cast<uint32_t>(outgoing->Value(i));
            sc.cost = cost_col->Value(i);
            sc.via_edge = static_cast<uint32_t>(via_col->Value(i));
            sc.cell = static_cast<uint64_t>(cell_col->Value(i));
            sc.inside = inside_col->Value(i);
            
            size_t idx = shortcuts.size();
            shortcuts.push_back(sc);
            fwd_adj[sc.from].push_back(idx);
            bwd_adj[sc.to].push_back(idx);
        }
    }
    
    return true;
}

bool ShortcutGraph::load_shortcuts(const std::string& path) {
    shortcuts_.clear();
    fwd_adj_.clear();
    bwd_adj_.clear();
    
    if (fs::is_directory(path)) {
        // Load all .parquet files in directory
        for (const auto& entry : fs::directory_iterator(path)) {
            if (entry.path().extension() == ".parquet") {
                load_parquet_file(entry.path().string(), shortcuts_, fwd_adj_, bwd_adj_);
            }
        }
    } else {
        // Load single file
        load_parquet_file(path, shortcuts_, fwd_adj_, bwd_adj_);
    }
    
    return !shortcuts_.empty();
}

bool ShortcutGraph::load_edge_metadata(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) return false;
    
    std::string line;
    std::getline(file, line);  // Skip header
    
    edge_meta_.clear();
    while (std::getline(file, line)) {
        // Parse CSV with quote handling
        std::vector<std::string> row;
        std::string field;
        bool in_quotes = false;
        
        for (char c : line) {
            if (c == '"') {
                in_quotes = !in_quotes;
            } else if (c == ',' && !in_quotes) {
                row.push_back(field);
                field.clear();
            } else {
                field += c;
            }
        }
        row.push_back(field);  // Last field
        
        // Columns: source, target, length, maxspeed, geometry, highway, cost, incoming_cell, outgoing_cell, lca_res, id
        // Indices:   0       1        2         3         4       5       6        7              8           9     10
        if (row.size() >= 11) {
            try {
                uint32_t id = std::stoul(row[10]);
                EdgeMeta meta;
                meta.incoming_cell = std::stoull(row[7]);
                meta.outgoing_cell = std::stoull(row[8]);
                meta.lca_res = std::stoi(row[9]);
                meta.length = std::stod(row[2]);
                meta.cost = std::stod(row[6]);
                edge_meta_[id] = meta;
            } catch (...) {
                // Skip malformed rows
            }
        }
    }
    
    return !edge_meta_.empty();
}

double ShortcutGraph::get_edge_cost(uint32_t edge_id) const {
    auto it = edge_meta_.find(edge_id);
    return (it != edge_meta_.end()) ? it->second.cost : 0.0;
}

uint64_t ShortcutGraph::get_edge_cell(uint32_t edge_id) const {
    auto it = edge_meta_.find(edge_id);
    return (it != edge_meta_.end()) ? it->second.incoming_cell : 0;
}

HighCell ShortcutGraph::compute_high_cell(uint32_t source_edge, uint32_t target_edge) const {
    auto src_it = edge_meta_.find(source_edge);
    auto dst_it = edge_meta_.find(target_edge);
    
    if (src_it == edge_meta_.end() || dst_it == edge_meta_.end()) {
        return {0, -1};
    }
    
    uint64_t src_cell = src_it->second.incoming_cell;
    uint64_t dst_cell = dst_it->second.incoming_cell;
    int src_res = src_it->second.lca_res;
    int dst_res = dst_it->second.lca_res;
    
    if (src_cell == 0 || dst_cell == 0) {
        return {0, -1};
    }
    
    if (src_res >= 0) src_cell = h3_utils::cell_to_parent(src_cell, src_res);
    if (dst_res >= 0) dst_cell = h3_utils::cell_to_parent(dst_cell, dst_res);
    
    uint64_t lca = h3_utils::find_lca(src_cell, dst_cell);
    int res = (lca != 0) ? h3_utils::get_resolution(lca) : -1;
    return {lca, res};
}

QueryResult ShortcutGraph::query_classic(uint32_t source_edge, uint32_t target_edge) const {
    constexpr double INF = std::numeric_limits<double>::infinity();
    
    if (source_edge == target_edge) {
        return {get_edge_cost(source_edge), {source_edge}, true};
    }
    
    std::unordered_map<uint32_t, double> dist_fwd, dist_bwd;
    std::unordered_map<uint32_t, uint32_t> parent_fwd, parent_bwd;
    MinHeap pq_fwd, pq_bwd;
    
    dist_fwd[source_edge] = 0.0;
    parent_fwd[source_edge] = source_edge;
    pq_fwd.push({0.0, source_edge});
    
    double target_cost = get_edge_cost(target_edge);
    dist_bwd[target_edge] = target_cost;
    parent_bwd[target_edge] = target_edge;
    pq_bwd.push({target_cost, target_edge});
    
    double best = INF;
    uint32_t meeting = 0;
    bool found = false;
    
    while (!pq_fwd.empty() || !pq_bwd.empty()) {
        // Forward step
        if (!pq_fwd.empty()) {
            auto [d, u] = pq_fwd.top(); pq_fwd.pop();
            
            auto it = dist_fwd.find(u);
            if (it != dist_fwd.end() && d > it->second) continue;
            if (d >= best) continue;
            
            auto adj_it = fwd_adj_.find(u);
            if (adj_it != fwd_adj_.end()) {
                for (size_t idx : adj_it->second) {
                    const Shortcut& sc = shortcuts_[idx];
                    if (sc.inside != 1) continue;
                    
                    double nd = d + sc.cost;
                    auto v_it = dist_fwd.find(sc.to);
                    if (v_it == dist_fwd.end() || nd < v_it->second) {
                        dist_fwd[sc.to] = nd;
                        parent_fwd[sc.to] = u;
                        pq_fwd.push({nd, sc.to});
                        
                        auto bwd_it = dist_bwd.find(sc.to);
                        if (bwd_it != dist_bwd.end()) {
                            double total = nd + bwd_it->second;
                            if (total < best) {
                                best = total;
                                meeting = sc.to;
                                found = true;
                            }
                        }
                    }
                }
            }
        }
        
        // Backward step
        if (!pq_bwd.empty()) {
            auto [d, u] = pq_bwd.top(); pq_bwd.pop();
            
            auto it = dist_bwd.find(u);
            if (it != dist_bwd.end() && d > it->second) continue;
            if (d >= best) continue;
            
            auto adj_it = bwd_adj_.find(u);
            if (adj_it != bwd_adj_.end()) {
                for (size_t idx : adj_it->second) {
                    const Shortcut& sc = shortcuts_[idx];
                    if (sc.inside != -1 && sc.inside != 0) continue;
                    
                    double nd = d + sc.cost;
                    auto prev_it = dist_bwd.find(sc.from);
                    if (prev_it == dist_bwd.end() || nd < prev_it->second) {
                        dist_bwd[sc.from] = nd;
                        parent_bwd[sc.from] = u;
                        pq_bwd.push({nd, sc.from});
                        
                        auto fwd_it = dist_fwd.find(sc.from);
                        if (fwd_it != dist_fwd.end()) {
                            double total = fwd_it->second + nd;
                            if (total < best) {
                                best = total;
                                meeting = sc.from;
                                found = true;
                            }
                        }
                    }
                }
            }
        }
        
        // Early termination
        if (!pq_fwd.empty() && !pq_bwd.empty()) {
            if (pq_fwd.top().dist >= best && pq_bwd.top().dist >= best) break;
        } else if (pq_fwd.empty() && pq_bwd.empty()) {
            break;
        }
    }
    
    if (!found) return {-1, {}, false};
    
    // Reconstruct path
    std::vector<uint32_t> path;
    uint32_t curr = meeting;
    while (parent_fwd[curr] != curr) {
        path.push_back(curr);
        curr = parent_fwd[curr];
    }
    path.push_back(curr);
    std::reverse(path.begin(), path.end());
    
    curr = meeting;
    while (parent_bwd[curr] != curr) {
        curr = parent_bwd[curr];
        path.push_back(curr);
    }
    
    return {best, path, true};
}

QueryResult ShortcutGraph::query_pruned(uint32_t source_edge, uint32_t target_edge) const {
    constexpr double INF = std::numeric_limits<double>::infinity();
    
    if (source_edge == target_edge) {
        return {get_edge_cost(source_edge), {source_edge}, true};
    }
    
    HighCell high = compute_high_cell(source_edge, target_edge);
    
    std::unordered_map<uint32_t, double> dist_fwd, dist_bwd;
    std::unordered_map<uint32_t, uint32_t> parent_fwd, parent_bwd;
    MinHeap pq_fwd, pq_bwd;
    
    dist_fwd[source_edge] = 0.0;
    parent_fwd[source_edge] = source_edge;
    pq_fwd.push({0.0, source_edge});
    
    double target_cost = get_edge_cost(target_edge);
    dist_bwd[target_edge] = target_cost;
    parent_bwd[target_edge] = target_edge;
    pq_bwd.push({target_cost, target_edge});
    
    double best = INF;
    uint32_t meeting = 0;
    bool found = false;
    
    while (!pq_fwd.empty() || !pq_bwd.empty()) {
        // Forward step
        if (!pq_fwd.empty()) {
            auto [d, u] = pq_fwd.top(); pq_fwd.pop();
            
            // Check meeting
            auto bwd_it = dist_bwd.find(u);
            if (bwd_it != dist_bwd.end()) {
                double total = d + bwd_it->second;
                if (total < best) {
                    best = total;
                    meeting = u;
                    found = true;
                }
            }
            
            auto it = dist_fwd.find(u);
            if (it != dist_fwd.end() && d > it->second) continue;
            if (d >= best) continue;
            
            // Pruning
            uint64_t u_cell = get_edge_cell(u);
            if (!h3_utils::parent_check(u_cell, high.cell, high.res)) continue;
            
            auto adj_it = fwd_adj_.find(u);
            if (adj_it != fwd_adj_.end()) {
                for (size_t idx : adj_it->second) {
                    const Shortcut& sc = shortcuts_[idx];
                    if (sc.inside != 1) continue;
                    
                    double nd = d + sc.cost;
                    auto v_it = dist_fwd.find(sc.to);
                    if (v_it == dist_fwd.end() || nd < v_it->second) {
                        dist_fwd[sc.to] = nd;
                        parent_fwd[sc.to] = u;
                        pq_fwd.push({nd, sc.to});
                    }
                }
            }
        }
        
        // Backward step
        if (!pq_bwd.empty()) {
            auto [d, u] = pq_bwd.top(); pq_bwd.pop();
            
            // Check meeting
            auto fwd_it = dist_fwd.find(u);
            if (fwd_it != dist_fwd.end()) {
                double total = fwd_it->second + d;
                if (total < best) {
                    best = total;
                    meeting = u;
                    found = true;
                }
            }
            
            auto it = dist_bwd.find(u);
            if (it != dist_bwd.end() && d > it->second) continue;
            if (d >= best) continue;
            
            // Pruning
            uint64_t u_cell = get_edge_cell(u);
            bool check = h3_utils::parent_check(u_cell, high.cell, high.res);
            bool at_high = (u_cell == high.cell);
            
            auto adj_it = bwd_adj_.find(u);
            if (adj_it != bwd_adj_.end()) {
                for (size_t idx : adj_it->second) {
                    const Shortcut& sc = shortcuts_[idx];
                    
                    // Backward filtering
                    bool allowed = false;
                    if (sc.inside == -1 && check) allowed = true;
                    else if (sc.inside == 0 && (at_high || !check)) allowed = true;
                    else if (sc.inside == -2 && !check) allowed = true;
                    
                    if (!allowed) continue;
                    
                    double nd = d + sc.cost;
                    auto prev_it = dist_bwd.find(sc.from);
                    if (prev_it == dist_bwd.end() || nd < prev_it->second) {
                        dist_bwd[sc.from] = nd;
                        parent_bwd[sc.from] = u;
                        pq_bwd.push({nd, sc.from});
                    }
                }
            }
        }
        
        // Early termination
        if (best < INF) {
            bool fwd_can = !pq_fwd.empty() && pq_fwd.top().dist < best;
            bool bwd_can = !pq_bwd.empty() && pq_bwd.top().dist < best;
            if (!fwd_can && !bwd_can) break;
        }
    }
    
    if (!found) return {-1, {}, false};
    
    // Reconstruct path
    std::vector<uint32_t> path;
    uint32_t curr = meeting;
    while (parent_fwd[curr] != curr) {
        path.push_back(curr);
        curr = parent_fwd[curr];
    }
    path.push_back(curr);
    std::reverse(path.begin(), path.end());
    
    curr = meeting;
    while (parent_bwd[curr] != curr) {
        curr = parent_bwd[curr];
        path.push_back(curr);
    }
    
    return {best, path, true};
}

QueryResult ShortcutGraph::query_multi(
    const std::vector<uint32_t>& source_edges,
    const std::vector<double>& source_dists,
    const std::vector<uint32_t>& target_edges,
    const std::vector<double>& target_dists
) const {
    constexpr double INF = std::numeric_limits<double>::infinity();
    
    std::unordered_map<uint32_t, double> dist_fwd, dist_bwd;
    std::unordered_map<uint32_t, uint32_t> parent_fwd, parent_bwd;
    MinHeap pq_fwd, pq_bwd;
    
    // Initialize from all sources
    for (size_t i = 0; i < source_edges.size(); ++i) {
        uint32_t src = source_edges[i];
        double d = source_dists[i];
        if (edge_meta_.find(src) != edge_meta_.end()) {
            dist_fwd[src] = d;
            parent_fwd[src] = src;
            pq_fwd.push({d, src});
        }
    }
    
    // Initialize from all targets
    for (size_t i = 0; i < target_edges.size(); ++i) {
        uint32_t tgt = target_edges[i];
        double d = target_dists[i] + get_edge_cost(tgt);
        if (edge_meta_.find(tgt) != edge_meta_.end()) {
            dist_bwd[tgt] = d;
            parent_bwd[tgt] = tgt;
            pq_bwd.push({d, tgt});
        }
    }
    
    double best = INF;
    uint32_t meeting = 0;
    bool found = false;
    
    while (!pq_fwd.empty() || !pq_bwd.empty()) {
        // Forward step
        if (!pq_fwd.empty()) {
            auto [d, u] = pq_fwd.top(); pq_fwd.pop();
            
            auto bwd_it = dist_bwd.find(u);
            if (bwd_it != dist_bwd.end() && d + bwd_it->second < best) {
                best = d + bwd_it->second;
                meeting = u;
                found = true;
            }
            
            if (d >= best || d > dist_fwd[u]) continue;
            
            auto adj_it = fwd_adj_.find(u);
            if (adj_it != fwd_adj_.end()) {
                for (size_t idx : adj_it->second) {
                    const Shortcut& sc = shortcuts_[idx];
                    if (sc.inside != 1) continue;
                    
                    double nd = d + sc.cost;
                    auto v_it = dist_fwd.find(sc.to);
                    if (v_it == dist_fwd.end() || nd < v_it->second) {
                        dist_fwd[sc.to] = nd;
                        parent_fwd[sc.to] = u;
                        pq_fwd.push({nd, sc.to});
                    }
                }
            }
        }
        
        // Backward step
        if (!pq_bwd.empty()) {
            auto [d, u] = pq_bwd.top(); pq_bwd.pop();
            
            auto fwd_it = dist_fwd.find(u);
            if (fwd_it != dist_fwd.end() && fwd_it->second + d < best) {
                best = fwd_it->second + d;
                meeting = u;
                found = true;
            }
            
            if (d >= best || d > dist_bwd[u]) continue;
            
            auto adj_it = bwd_adj_.find(u);
            if (adj_it != bwd_adj_.end()) {
                for (size_t idx : adj_it->second) {
                    const Shortcut& sc = shortcuts_[idx];
                    if (sc.inside != -1 && sc.inside != 0) continue;
                    
                    double nd = d + sc.cost;
                    auto prev_it = dist_bwd.find(sc.from);
                    if (prev_it == dist_bwd.end() || nd < prev_it->second) {
                        dist_bwd[sc.from] = nd;
                        parent_bwd[sc.from] = u;
                        pq_bwd.push({nd, sc.from});
                    }
                }
            }
        }
        
        // Early termination
        if (best < INF) {
            if (!pq_fwd.empty() && pq_fwd.top().dist >= best) while (!pq_fwd.empty()) pq_fwd.pop();
            if (!pq_bwd.empty() && pq_bwd.top().dist >= best) while (!pq_bwd.empty()) pq_bwd.pop();
        }
    }
    
    if (!found) return {-1, {}, false};
    
    // Reconstruct path
    std::vector<uint32_t> path;
    uint32_t curr = meeting;
    while (parent_fwd[curr] != curr) {
        path.push_back(curr);
        curr = parent_fwd[curr];
    }
    path.push_back(curr);
    std::reverse(path.begin(), path.end());
    
    curr = meeting;
    while (parent_bwd[curr] != curr) {
        curr = parent_bwd[curr];
        path.push_back(curr);
    }
    
    return {best, path, true};
}
