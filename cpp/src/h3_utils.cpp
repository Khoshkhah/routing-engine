/**
 * @file h3_utils.cpp
 * @brief H3 utility functions implementation.
 */

#include "h3_utils.hpp"
#include <h3/h3api.h>

namespace h3_utils {

int get_resolution(uint64_t cell) {
    if (cell == 0) return -1;
    return getResolution(cell);
}

uint64_t cell_to_parent(uint64_t cell, int target_res) {
    if (cell == 0 || target_res < 0) return 0;
    
    int current_res = getResolution(cell);
    if (target_res >= current_res) return cell;
    
    H3Index parent = 0;
    if (cellToParent(cell, target_res, &parent) != E_SUCCESS) {
        return 0;
    }
    return parent;
}

uint64_t find_lca(uint64_t cell1, uint64_t cell2) {
    if (cell1 == 0 || cell2 == 0) return 0;
    
    int res1 = getResolution(cell1);
    int res2 = getResolution(cell2);
    int min_res = (res1 < res2) ? res1 : res2;
    
    uint64_t c1 = (res1 > min_res) ? cell_to_parent(cell1, min_res) : cell1;
    uint64_t c2 = (res2 > min_res) ? cell_to_parent(cell2, min_res) : cell2;
    
    while (c1 != c2 && min_res > 0) {
        min_res--;
        c1 = cell_to_parent(c1, min_res);
        c2 = cell_to_parent(c2, min_res);
    }
    
    return (c1 == c2) ? c1 : 0;
}

bool parent_check(uint64_t node_cell, uint64_t high_cell, int high_res) {
    if (high_cell == 0 || high_res < 0) return true;
    if (node_cell == 0) return false;
    
    int node_res = getResolution(node_cell);
    if (high_res > node_res) return false;
    
    uint64_t parent = cell_to_parent(node_cell, high_res);
    return parent == high_cell;
}

}  // namespace h3_utils
