/**
 * @file shortcut_graph.hpp
 * @brief H3-based hierarchical routing graph and query engine.
 */

#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

/**
 * @brief Result of a shortest path query.
 */
struct QueryResult {
    double distance;              ///< Total path cost
    std::vector<uint32_t> path;   ///< Sequence of edge IDs
    bool reachable;               ///< True if a path was found
};

/**
 * @brief H3 cell constraint for pruned search.
 */
struct HighCell {
    uint64_t cell = 0;  ///< H3 cell ID
    int res = -1;       ///< Cell resolution
};

/**
 * @brief Edge metadata for H3-based routing.
 */
struct EdgeMeta {
    uint64_t incoming_cell = 0;
    uint64_t outgoing_cell = 0;
    int lca_res = -1;
    double length = 0.0;
    double cost = 0.0;
};

/**
 * @brief Shortcut edge in the graph.
 */
struct Shortcut {
    uint32_t from;       ///< Source edge ID
    uint32_t to;         ///< Target edge ID
    double cost;         ///< Traversal cost
    uint32_t via_edge;   ///< Intermediate edge (0 if direct)
    uint64_t cell;       ///< H3 cell bounding this shortcut
    int8_t inside;       ///< Direction: +1 up, 0 lateral, -1 down, -2 edge
};

/**
 * @brief H3-based hierarchical routing graph.
 */
class ShortcutGraph {
public:
    /**
     * @brief Load shortcuts from Parquet directory.
     * @param path Path to Parquet files
     * @return true if successful
     */
    bool load_shortcuts(const std::string& path);

    /**
     * @brief Load edge metadata from CSV.
     * @param path Path to CSV file
     * @return true if successful
     */
    bool load_edge_metadata(const std::string& path);

    /**
     * @brief Classic bidirectional Dijkstra with inside filtering.
     */
    QueryResult query_classic(uint32_t source_edge, uint32_t target_edge) const;

    /**
     * @brief Pruned bidirectional Dijkstra with H3 parent_check.
     */
    QueryResult query_pruned(uint32_t source_edge, uint32_t target_edge) const;

    /**
     * @brief Multi-source/target bidirectional search.
     */
    QueryResult query_multi(
        const std::vector<uint32_t>& source_edges,
        const std::vector<double>& source_dists,
        const std::vector<uint32_t>& target_edges,
        const std::vector<double>& target_dists
    ) const;

    /**
     * @brief Get edge cost.
     */
    double get_edge_cost(uint32_t edge_id) const;

    /**
     * @brief Get edge H3 cell.
     */
    uint64_t get_edge_cell(uint32_t edge_id) const;

    /**
     * @brief Get number of shortcuts loaded.
     */
    size_t shortcut_count() const { return shortcuts_.size(); }

    /**
     * @brief Get number of edges with metadata.
     */
    size_t edge_count() const { return edge_meta_.size(); }

private:
    HighCell compute_high_cell(uint32_t source_edge, uint32_t target_edge) const;

    std::vector<Shortcut> shortcuts_;
    std::unordered_map<uint32_t, std::vector<size_t>> fwd_adj_;  // edge -> shortcut indices
    std::unordered_map<uint32_t, std::vector<size_t>> bwd_adj_;
    std::unordered_map<uint32_t, EdgeMeta> edge_meta_;
};
