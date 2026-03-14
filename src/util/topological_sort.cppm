// SPDX-FileCopyrightText: 2026 Jan Babin
// SPDX-License-Identifier: AGPL-3.0-only
//
// Part of gtfs2rdf. See the LICENSE file for details.

module;

#include <algorithm>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

export module util:topological_sort;

export namespace util::topological_sort {

// Topological Sort using Kahn's Algorithm (https://en.wikipedia.org/wiki/Topological_sorting)
// Upon researching briefly, I couldn't find a plug-and-play TopoSort implementation without adding
// a heavy dependency, so I implemented this simple version here. Due to its use of an adjacency
// matrix for simplicity, performance will likely cause issues for large graphs, but for the use
// cases in gtfs2rdf (dozens of nodes at most), this should be fine.
class TopologicalSort {
  public:
	// enter estimated number of nodes to avoid reallocations for efficiency
	TopologicalSort(size_t num_nodes) {
		edges_.reserve(num_nodes * 2); // assume very sparse graph
		node_index_.reserve(num_nodes);
		index_to_name_.reserve(num_nodes);
		L_.reserve(num_nodes);
		S_.reserve(num_nodes);
	}

	// this means from_node must come before / to_node depends on from_node
	void addEdge(size_t from_node, size_t to_node, bool allow_self_loops = false) {
		auto [itr, res] = node_index_.emplace(from_node, node_index_.size());
		if (res) {
			index_to_name_.push_back(from_node);
		}
		auto [it2, res2] = node_index_.emplace(to_node, node_index_.size());
		if (res2) {
			index_to_name_.push_back(to_node);
		}

		if (from_node == to_node) {
			if (!allow_self_loops) {
				error_node_ = from_node;
				throw std::runtime_error("TopologicalSort error: self-loop detected for node " +
				                         std::to_string(error_node_));
			}
		} else {
			// don't add self-loops as edges, that would be pointless
			edges_.emplace_back(itr->second, it2->second);
		}
	}

	// this can be used for nodes which might not have any edges but should still appear in the
	// sorted output
	void addNode(size_t node) {
		auto [itr, res] = node_index_.emplace(node, node_index_.size());
		if (res) {
			index_to_name_.push_back(node);
		}
	}

	std::vector<size_t> sort() {
		// initialise adjacency matrix
		A_ = std::vector<std::vector<bool>>(node_index_.size(),
		                                    std::vector<bool>(node_index_.size(), false));
		for (const auto& [from_id, to_id] : edges_) { // NOLINT(readability-use-anyallof)
			A_[from_id][to_id] = true;
		}

		L_.clear();
		S_.clear();

		// initialise set of nodes with no incoming edges
		for (size_t i = 0; i < A_.size(); ++i) {
			if (isColFalse_(i)) {
				S_.push_back(i);
			}
		}

		while (!S_.empty()) {
			size_t n =
			    S_.back(); // NOLINT(readability-identifier-length): common name for node index
			S_.pop_back();
			L_.push_back(n);

			// for each node m with an edge e from n to m
			for (size_t m = 0; m < A_.size();
			     ++m) { // NOLINT(readability-identifier-length): common matrix column index name
				if (A_[n][m]) {
					// remove edge e from the graph
					A_[n][m] = false;
					// if m has no other incoming edges then insert m into S
					if (isColFalse_(m)) {
						S_.push_back(m);
					}
				}
			}
		}

		// check for cycles
		for (size_t i = 0; i < A_.size(); ++i) {
			if (!isRowFalse_(i)) {
				error_node_ = index_to_name_[i];
				throw std::runtime_error(
				    "TopologicalSort error: graph has at least one cycle at node " +
				    std::to_string(error_node_));
			}
		}

		// build result
		std::vector<size_t> result;
		result.reserve(L_.size());
		for (size_t index : L_) {
			result.push_back(index_to_name_[index]);
		}
		return result;
	}

	size_t getErrorNode() const {
		return error_node_;
	}

  private:
	// adjacency matrix
	std::vector<std::vector<bool>> A_; // NOLINT(readability-identifier-naming): common matrix name

	std::vector<std::pair<size_t, size_t>> edges_;

	// map from node name to index in adjacency matrix
	std::unordered_map<size_t, size_t> node_index_;
	std::vector<size_t> index_to_name_;

	std::vector<size_t> L_; // NOLINT(readability-identifier-naming):  name follows algorithm
	std::vector<size_t> S_; // NOLINT(readability-identifier-naming):  name follows algorithm

	size_t error_node_ = -1; // set to MAXSIZE_T on no error

	bool isRowFalse_(size_t row) {
		return std::all_of(A_[row].begin(), A_[row].end(), [](bool val) { return !val; });
	}

	bool isColFalse_(size_t col) {
		return std::ranges::all_of(A_, [col](const std::vector<bool>& row) { return !row[col]; });
	}
};

} // namespace util::topological_sort