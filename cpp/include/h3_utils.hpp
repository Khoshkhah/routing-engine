/**
 * @file h3_utils.hpp
 * @brief H3 utility functions for hierarchical routing.
 */

#pragma once

#include <cstdint>

namespace h3_utils {

/**
 * @brief Get resolution of an H3 cell.
 */
int get_resolution(uint64_t cell);

/**
 * @brief Get parent cell at target resolution.
 */
uint64_t cell_to_parent(uint64_t cell, int target_res);

/**
 * @brief Find lowest common ancestor of two H3 cells.
 */
uint64_t find_lca(uint64_t cell1, uint64_t cell2);

/**
 * @brief Check if node_cell is within high_cell region.
 * @return true if node is within the high_cell ancestor
 */
bool parent_check(uint64_t node_cell, uint64_t high_cell, int high_res);

}  // namespace h3_utils
