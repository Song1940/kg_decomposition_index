#include "kg_index.h"
#include <unordered_map>
#include <memory>
#include <iostream>
#include <algorithm>

class FastNaiveIndex {
private:
    std::unordered_map<uint64_t, std::unordered_set<int>> index_map;
    static const std::unordered_set<int> empty_set;
    
    static constexpr uint64_t make_key(int k, int g) noexcept {
        return (static_cast<uint64_t>(g) << 32) | static_cast<uint64_t>(k);
    }
    
public:
    explicit FastNaiveIndex(const std::shared_ptr<TreeNode>& tree) {
        if (!tree || tree->children.empty()) return;
        
        // 🚀 미리 크기 계산해서 한번에 reserve
        size_t estimated_size = 0;
        for (const auto& g_child : tree->children) {
            if (g_child && !g_child->children.empty()) {
                estimated_size += g_child->children.size();
            }
        }
        index_map.reserve(estimated_size);
        
        for (int g = 0; g < (int)tree->children.size(); g++) {
            if (!tree->children[g] || tree->children[g]->children.empty()) continue;
            
            for (int k = 0; k < (int)tree->children[g]->children.size(); k++) {
                if (!tree->children[g]->children[k]) continue;
                
                const auto& value = tree->children[g]->children[k]->value;
                if (!value.empty()) {
                    uint64_t key = make_key(k + 1, g + 1);
                    // 🚀 emplace 사용해서 복사 방지
                    index_map.emplace(key, value);
                }
            }
        }
    }
    
    const std::unordered_set<int>& query(int k, int g) const noexcept {
        uint64_t key = make_key(k, g);
        auto it = index_map.find(key);
        return (it != index_map.end()) ? it->second : empty_set;
    }
    
    bool exists(int k, int g) const noexcept {
        uint64_t key = make_key(k, g);
        return index_map.find(key) != index_map.end();
    }
    
    size_t size() const noexcept { return index_map.size(); }
    
    void print_stats() const {
        std::cout << "Fast Index Statistics:" << std::endl;
        std::cout << "  Total entries: " << index_map.size() << std::endl;
        std::cout << "  Load factor: " << index_map.load_factor() << std::endl;
        std::cout << "  Bucket count: " << index_map.bucket_count() << std::endl;
    }
};


// static 멤버 정의
const std::unordered_set<int> FastNaiveIndex::empty_set;

// 전역 인덱스
static std::unique_ptr<FastNaiveIndex> g_fast_index;

// ============================================================================
// 기존 함수들
// ============================================================================

Hypergraph load_hypergraph(const std::string& file_path) {
    Hypergraph hypergraph;
    std::ifstream file(file_path);
    
    if (!file.is_open()) {
        std::cerr << "Error: Cannot open file " << file_path << std::endl;
        return hypergraph;
    }
    
    std::string line;
    int line_count = 0;
    
    while (std::getline(file, line)) {
        line_count++;
        
        // 공백 제거
        line.erase(0, line.find_first_not_of(" \t"));
        line.erase(line.find_last_not_of(" \t") + 1);
        
        if (line.empty() || line[0] == '#') continue;
        
        std::unordered_set<int> hyperedge;
        
        if (line.find(',') != std::string::npos) {
            // 콤마로 구분된 경우
            std::stringstream ss(line);
            std::string node_str;
            
            while (std::getline(ss, node_str, ',')) {
                node_str.erase(0, node_str.find_first_not_of(" \t"));
                node_str.erase(node_str.find_last_not_of(" \t") + 1);
                
                if (!node_str.empty()) {
                    try {
                        int node = std::stoi(node_str);
                        hyperedge.insert(node);
                    } catch (const std::exception& e) {
                        std::cerr << "Error parsing node '" << node_str << "' on line " << line_count << std::endl;
                    }
                }
            }
        } else {
            // 공백/탭으로 구분된 경우
            std::istringstream iss(line);
            std::string node_str;
            
            while (iss >> node_str) {
                try {
                    int node = std::stoi(node_str);
                    hyperedge.insert(node);
                } catch (const std::exception& e) {
                    std::cerr << "Error parsing node '" << node_str << "' on line " << line_count << std::endl;
                }
            }
        }
        
        if (!hyperedge.empty()) {
            hypergraph.add_hyperedge(hyperedge);
        }
    }
    
    file.close();
    return hypergraph;
}

std::unordered_map<int, int> neighbour_count_map(const Hypergraph& hypergraph, int v, int g) {
    std::unordered_map<int, int> neighbor_counts;
    
    if (!hypergraph.has_node(v)) {
        return neighbor_counts;
    }
    
    // 첫 번째 단계: 이웃 카운트 계산
    for (const auto& hyperedge : hypergraph.node_hyperedges.at(v)) {
        for (int neighbor : hyperedge) {
            if (neighbor != v) {
                neighbor_counts[neighbor]++;
            }
        }
    }
    
    // 두 번째 단계: g 이상인 것만 남기기 (in-place 필터링)
    auto it = neighbor_counts.begin();
    while (it != neighbor_counts.end()) {
        if (it->second < g) {
            it = neighbor_counts.erase(it);
        } else {
            ++it;
        }
    }
    
    return neighbor_counts;
}

std::unordered_set<int> get_neighbour(const Hypergraph& hypergraph, int v) {
    std::unordered_set<int> neighbor_set;
    
    if (!hypergraph.has_node(v)) {
        return neighbor_set;
    }
    
    for (const auto& hyperedge : hypergraph.node_hyperedges.at(v)) {
        for (int neighbor : hyperedge) {
            if (neighbor != v) {
                neighbor_set.insert(neighbor);
            }
        }
    }
    
    return neighbor_set;
}

std::unordered_set<int> find_kg_core(const Hypergraph& hypergraph, int k, int g) {
    bool changed = true;
    std::unordered_set<int> H = hypergraph.nodes();
    
    int count = 0;
    for (int v : H) {
        auto map = neighbour_count_map(hypergraph, v, g);
        if (++count >= 5) break;
    }
    
    while (changed) {
        changed = false;
        std::unordered_set<int> nodes = H;
        
        for (int v : nodes) {
            auto map = neighbour_count_map(hypergraph, v, g);
            
            std::unordered_map<int, int> filtered_map;
            for (const auto& pair : map) {
                if (nodes.find(pair.first) != nodes.end()) {
                    filtered_map[pair.first] = pair.second;
                }
            }
            
            if ((int)filtered_map.size() < k) {
                changed = true;
                H.erase(v);
            }
        }
    }
    
    return H;
}

std::unordered_set<int> kg_core(const Hypergraph& hypergraph, int k, int g) {
    return find_kg_core(hypergraph, k, g);
}

std::vector<std::unordered_set<int>> enumerate_kg_core_fixing_g(const Hypergraph& hypergraph, int g) {
    // 최대 노드 ID 찾기
    int max_node = 0;
    for (int node : hypergraph.nodes()) {
        max_node = std::max(max_node, node);
    }
    
    // 비트맵으로 메모리 최적화: 320MB -> 20MB
    std::vector<bool> H(max_node + 1, false);
    std::vector<bool> T(max_node + 1, false);
    
    // 초기 활성 노드 설정
    for (int node : hypergraph.nodes()) {
        H[node] = true;
    }
    
    std::vector<std::unordered_set<int>> S;
    int active_count = hypergraph.nodes().size();
    
    for (int k = 1; k < active_count; k++) {
        if (active_count <= k) break;
        
        while (true) {
            if (active_count <= k) break;
            
            bool changed = false;
            std::vector<int> nodes_to_remove;
            std::vector<int> nodes_to_add_to_T;
            
            // T가 비어있는지 확인 (효율적인 방식)
            bool T_empty = true;
            for (int i = 0; i <= max_node && T_empty; i++) {
                if (T[i]) T_empty = false;
            }
            
            if (T_empty) {
                // 모든 활성 노드에 대해 처리
                for (int v = 0; v <= max_node; v++) {
                    if (!H[v]) continue;
                    
                    T[v] = false;  // T에서 v 제거
                    
                    // 이웃 카운트 계산 (비트맵 활용)
                    int valid_neighbors = count_valid_neighbors_with_bitmap(hypergraph, v, g, H, max_node);
                    
                    if (valid_neighbors < k) {
                        nodes_to_remove.push_back(v);
                        changed = true;
                        
                        // 이웃들을 T에 추가할 목록에 추가
                        add_neighbors_to_T_list(hypergraph, v, nodes_to_add_to_T, max_node);
                    }
                }
            } else {
                // T와 H의 교집합에 대해서만 처리
                for (int v = 0; v <= max_node; v++) {
                    if (!H[v] || !T[v]) continue;
                    
                    T[v] = false;  // T에서 v 제거
                    
                    int valid_neighbors = count_valid_neighbors_with_bitmap(hypergraph, v, g, H, max_node);
                    
                    if (valid_neighbors < k) {
                        nodes_to_remove.push_back(v);
                        changed = true;
                        
                        add_neighbors_to_T_list(hypergraph, v, nodes_to_add_to_T, max_node);
                    }
                }
            }
            
            // 배치로 노드 제거
            for (int v : nodes_to_remove) {
                H[v] = false;
                active_count--;
            }
            
            // 배치로 T 업데이트
            for (int neighbor : nodes_to_add_to_T) {
                if (neighbor <= max_node) {
                    T[neighbor] = true;
                }
            }
            
            if (!changed) {
                // 현재 활성 노드들을 결과에 추가
                std::unordered_set<int> current_core;
                current_core.reserve(active_count);
                
                for (int i = 0; i <= max_node; i++) {
                    if (H[i]) {
                        current_core.insert(i);
                    }
                }
                
                if (!current_core.empty()) {
                    S.push_back(std::move(current_core));
                }
                
                // T 비우기
                std::fill(T.begin(), T.end(), false);
                break;
            }
        }
    }
    
    return S;
}


int count_valid_neighbors_with_bitmap(const Hypergraph& hypergraph, int v, int g, 
                                     const std::vector<bool>& active_nodes, int max_node) {
    if (!hypergraph.has_node(v)) {
        return 0;
    }
    
    // 임시 카운터 - 스택 할당으로 빠른 접근
    std::unordered_map<int, int> neighbor_counts;
    neighbor_counts.reserve(100);  // 평균 이웃 수 예상
    
    // 이웃 카운트 계산
    for (const auto& hyperedge : hypergraph.node_hyperedges.at(v)) {
        for (int neighbor : hyperedge) {
            if (neighbor != v && neighbor <= max_node && active_nodes[neighbor]) {
                neighbor_counts[neighbor]++;
            }
        }
    }
    
    // g 이상인 이웃만 카운트
    int valid_count = 0;
    for (const auto& [neighbor, count] : neighbor_counts) {
        if (count >= g) {
            valid_count++;
        }
    }
    
    return valid_count;
}

// T에 추가할 이웃들을 리스트에 수집
void add_neighbors_to_T_list(const Hypergraph& hypergraph, int v, 
                            std::vector<int>& nodes_to_add, int max_node) {
    if (!hypergraph.has_node(v)) {
        return;
    }
    
    for (const auto& hyperedge : hypergraph.node_hyperedges.at(v)) {
        for (int neighbor : hyperedge) {
            if (neighbor != v && neighbor <= max_node) {
                nodes_to_add.push_back(neighbor);
            }
        }
    }
}

std::shared_ptr<TreeNode> naive_index_construction(
    const Hypergraph& hypergraph, 
    const std::vector<std::unordered_set<int>>& E) {
    
    auto T = std::make_shared<TreeNode>("root");
    T->children.reserve(E.size());  // 메모리 예약
    
    std::cout << "🔧 Naive: Processing g-values (Bitmap Optimized)..." << std::endl;
    
    for (int g = 1; g < static_cast<int>(E.size()); g++) {
        std::cout << "   g=" << g << ": Computing cores..." << std::flush;
        
        auto S = enumerate_kg_core_fixing_g(hypergraph, g);
        
        if (S.empty()) {
            std::cout << " no cores found, stopping at g=" << (g-1) << std::endl;
            break;
        }
        
        std::cout << " found " << S.size() << " cores" << std::endl;
        
        auto g_node = std::make_shared<TreeNode>("");
        g_node->children.reserve(S.size());
        
        for (auto& core : S) {
            auto leaf_node = std::make_shared<TreeNode>("");
            leaf_node->value = std::move(core);
            g_node->children.push_back(std::move(leaf_node));
        }
        
        T->children.push_back(std::move(g_node));
    }
    
    std::cout << "✅ Naive: Completed with " << T->children.size() << " g-levels" << std::endl;
    
    // FastNaiveIndex 생성
    std::cout << "🚀 Naive: Creating fast index..." << std::endl;
    g_fast_index = std::make_unique<FastNaiveIndex>(T);
    std::cout << "✅ Naive: Fast index ready!" << std::endl;
    g_fast_index->print_stats();
    
    return T;
}

const std::unordered_set<int>& querying_for_naive_index(
    const std::shared_ptr<TreeNode>& tree, int k, int g) {
    
    // 🚀 빠른 인덱스가 있으면 사용
    if (g_fast_index) {
        return g_fast_index->query(k, g);
    }
    
    // fallback: 기존 방식
    static const std::unordered_set<int> empty_result;
    
    if (!tree || tree->children.empty()) return empty_result;
    
    const int g_idx = g - 1;
    const int k_idx = k - 1;
    
    if (g_idx < 0 || g_idx >= (int)tree->children.size()) return empty_result;
    if (k_idx < 0 || k_idx >= (int)tree->children[g_idx]->children.size()) return empty_result;
    
    return tree->children[g_idx]->children[k_idx]->value;
}

// ============================================================================
// 나머지 함수들
// ============================================================================

std::vector<std::unordered_set<int>> enumerate_1_g(const Hypergraph& hypergraph, int g) {
    std::unordered_set<int> H = hypergraph.nodes();
    std::vector<std::unordered_set<int>> S;
    std::unordered_set<int> T;
    std::unordered_set<int> temp;
    
    for (int k = 1; k < (int)hypergraph.nodes().size(); k++) {
        if ((int)H.size() <= k) {
            break;
        }
        
        while (true) {
            if ((int)H.size() <= k) {
                break;
            }
            
            bool changed = false;
            std::unordered_set<int> nodes = H;
            
            if (!T.empty()) {
                std::unordered_set<int> intersection;
                for (int node : nodes) {
                    if (T.find(node) != T.end()) {
                        intersection.insert(node);
                    }
                }
                nodes = intersection;
            }
            
            for (int v : nodes) {
                T.erase(v);
                
                auto map = neighbour_count_map(hypergraph, v, g);
                
                std::unordered_map<int, int> filtered_map;
                for (const auto& pair : map) {
                    if (nodes.find(pair.first) != nodes.end()) {
                        filtered_map[pair.first] = pair.second;
                    }
                }
                
                if ((int)filtered_map.size() < k) {
                    H.erase(v);
                    changed = true;
                }
            }
            
            if (!changed) {
                if (!temp.empty()) {
                    std::unordered_set<int> difference;
                    for (int node : temp) {
                        if (H.find(node) == H.end()) {
                            difference.insert(node);
                        }
                    }
                    S.push_back(difference);
                    T.clear();
                }
                temp = H;
                break;
            }
        }
    }
    
    if (!temp.empty()) {
        S.push_back(temp);
    }
    
    return S;
}

std::shared_ptr<TreeNode> one_level_compression(const Hypergraph& hypergraph, const std::vector<std::unordered_set<int>>& E) {
    auto T = std::make_shared<TreeNode>("root");
    
    std::cout << "      🔧 One-Level: Processing g-values..." << std::endl;
    
    for (int g = 1; g < (int)E.size(); g++) {
        std::cout << "         g=" << g << ": Computing cores..." << std::flush;
        
        auto S = enumerate_1_g(hypergraph, g);
        
        if (S.empty()) {
            std::cout << " no cores found, stopping at g=" << (g-1) << std::endl;
            break;
        }
        
        std::cout << " found " << S.size() << " cores" << std::endl;
        
        T->children.push_back(std::make_shared<TreeNode>(std::to_string(g)));
        
        std::shared_ptr<TreeNode> prev = nullptr;
        
        for (int s = 0; s < (int)S.size(); s++) {
            // k값별 진행상황 (너무 많으면 간략히)
            if (S.size() <= 10 || s % std::max(1, (int)S.size() / 10) == 0 || s == (int)S.size() - 1) {
                std::cout << "            k=" << (s+1) << " (" << S[s].size() << " nodes)" << std::endl;
            }
            
            std::string node_name = "(" + std::to_string(s + 1) + "," + std::to_string(g) + ")";
            T->children[g - 1]->children.push_back(std::make_shared<TreeNode>(node_name));
            
            T->children[g - 1]->children[s]->value = S[s];
            
            auto u = T->children[g - 1]->children[s];
            
            if (prev != nullptr) {
                prev->next = u;
            }
            
            prev = u;
        }
    }
    
    std::cout << "      ✅ One-Level: Completed with " << T->children.size() << " g-levels" << std::endl;
    return T;
}

std::pair<std::shared_ptr<TreeNode>, double> jump_compression(const Hypergraph& hypergraph, const std::vector<std::unordered_set<int>>& E) {
    auto h_time_start = std::chrono::high_resolution_clock::now();
    
    auto T_1 = one_level_compression(hypergraph, E);
    
    auto h_time_end = std::chrono::high_resolution_clock::now();
    
    auto T_2 = T_1;
    
    int max_g = T_2->children.size();
    
    std::cout << "      🔧 Jump: Adding jump pointers..." << std::endl;
    
    for (int g = 0; g < max_g - 1; g++) {
        int max_k = T_2->children[g + 1]->children.size();
        
        std::cout << "         g=" << (g+1) << ": Processing " << max_k << " jump connections..." << std::flush;
        
        int processed = 0;
        for (int k = 0; k < max_k; k++) {
            if (k < (int)T_2->children[g]->children.size()) {
                T_2->children[g]->children[k]->jump = T_2->children[g + 1]->children[k];
                
                std::unordered_set<int> difference;
                const auto& current_value = T_2->children[g]->children[k]->value;
                const auto& jump_value = T_2->children[g]->children[k]->jump->value;
                
                for (int node : current_value) {
                    if (jump_value.find(node) == jump_value.end()) {
                        difference.insert(node);
                    }
                }
                
                T_2->children[g]->children[k]->value = difference;
                processed++;
            }
        }
        std::cout << " " << processed << " completed" << std::endl;
    }
    
    auto h_time = std::chrono::duration<double>(h_time_end - h_time_start).count();
    
    std::cout << "      ✅ Jump: Completed jump compression" << std::endl;
    return {T_2, h_time};
}

std::unordered_set<int> set_intersection(const std::unordered_set<int>& set1, const std::unordered_set<int>& set2) {
    std::unordered_set<int> result;
    for (int element : set1) {
        if (set2.find(element) != set2.end()) {
            result.insert(element);
        }
    }
    return result;
}

std::unordered_set<int> set_difference(const std::unordered_set<int>& set1, const std::unordered_set<int>& set2) {
    std::unordered_set<int> result;
    for (int element : set1) {
        if (set2.find(element) == set2.end()) {
            result.insert(element);
        }
    }
    return result;
}

std::tuple<std::shared_ptr<TreeNode>, double, double> diagonal_compression(const Hypergraph& hypergraph, const std::vector<std::unordered_set<int>>& E) {
    auto v_time_start = std::chrono::high_resolution_clock::now();
    
    auto [T, h_time] = jump_compression(hypergraph, E);
    
    auto v_time_end = std::chrono::high_resolution_clock::now();
    
    std::cout << "      🔧 Diagonal: Starting diagonal compression..." << std::endl;
    
    for (int g = 0; g < (int)T->children.size(); g++) {
        if (T->children[g]->children.empty()) continue;
        auto head = T->children[g]->children[0];
        
        if (!head->next) {
            break;
        }
        
        std::cout << "         g=" << (g+1) << ": Processing " << T->children[g]->children.size() << " diagonal operations..." << std::flush;
        
        int processed = 0;
        for (int k = 0; k < (int)T->children[g]->children.size() - 1; k++) {
            if (head->next && head->jump) {
                auto diag = head->jump;
                
                head = head->next;
                
                auto intersect = set_intersection(head->value, diag->value);
                
                if (!diag->next) {
                    head->jump = std::make_shared<TreeNode>("aux");
                    diag->next = head->jump;
                }
                
                diag->next->aux[1] = intersect;
                
                for (const auto& aux_pair : diag->aux) {
                    int i = aux_pair.first;
                    if (head->aux.find(i) != head->aux.end()) {
                        diag->next->aux[i + 1] = set_intersection(diag->aux[i], head->aux[i]);
                        head->aux[i] = set_difference(head->aux[i], diag->next->aux[i + 1]);
                    }
                }
                
                head->value = set_difference(head->value, intersect);
                
                if (head->next && !head->next->aux.empty()) {
                    if (head->next->aux.find(1) != head->next->aux.end()) {
                        head->value = set_difference(head->value, head->next->aux[1]);
                    }
                }
                
            } else {
                while (head->next) {
                    for (const auto& aux_pair : head->next->aux) {
                        int i = aux_pair.first;
                        
                        if (i == 1) {
                            head->value = set_difference(head->value, head->next->aux[i]);
                        } else {
                            if (head->aux.find(i - 1) != head->aux.end()) {
                                head->aux[i - 1] = set_difference(head->aux[i - 1], head->next->aux[i]);
                            }
                        }
                    }
                    head = head->next;
                }
            }
            processed++;
        }
        std::cout << " " << processed << " completed" << std::endl;
    }
    
    auto v_time = std::chrono::duration<double>(v_time_end - v_time_start).count();
    
    std::cout << "      ✅ Diagonal: Completed diagonal compression" << std::endl;
    return {T, h_time, v_time};
}


