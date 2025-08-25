#ifndef KG_INDEX_H
#define KG_INDEX_H

#include <iostream>
#include <vector>
#include <set>
#include <unordered_set>
#include <unordered_map>
#include <map>
#include <string>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <memory>
#include <chrono>
#include <filesystem>
#include <iomanip>
#include <ctime>

// TreeNode 클래스 - Python의 TreeNode와 동일한 구조
class TreeNode {
public:
    std::unordered_map<int, std::unordered_set<int>> aux;  // aux 딕셔너리
    std::string name;                                      // 노드 이름
    std::vector<std::shared_ptr<TreeNode>> children;       // 자식 노드들
    std::shared_ptr<TreeNode> next;                        // 다음 노드 포인터
    std::unordered_set<int> value;                         // 값 집합
    std::shared_ptr<TreeNode> jump;                        // 점프 포인터
    
    // 생성자
    TreeNode(const std::string& node_name) : name(node_name), next(nullptr), jump(nullptr) {}
    
    // 소멸자
    ~TreeNode() = default;
};

// 하이퍼그래프를 나타내는 클래스
class Hypergraph {
public:
    // 노드 ID와 해당 노드가 포함된 하이퍼엣지들의 맵
    std::unordered_map<int, std::vector<std::unordered_set<int>>> node_hyperedges;
    
    // 전체 하이퍼엣지들의 리스트
    std::vector<std::unordered_set<int>> E;
    
    // 생성자
    Hypergraph() = default;
    
    // 노드 추가
    void add_node(int node) {
        if (node_hyperedges.find(node) == node_hyperedges.end()) {
            node_hyperedges[node] = std::vector<std::unordered_set<int>>();
        }
    }
    
    // 노드가 존재하는지 확인
    bool has_node(int node) const {
        return node_hyperedges.find(node) != node_hyperedges.end();
    }
    
    // 모든 노드 반환
    std::unordered_set<int> nodes() const {
        std::unordered_set<int> all_nodes;
        for (const auto& pair : node_hyperedges) {
            all_nodes.insert(pair.first);
        }
        return all_nodes;
    }
    
    // 하이퍼엣지 추가
    void add_hyperedge(const std::unordered_set<int>& hyperedge) {
        E.push_back(hyperedge);
        for (int node : hyperedge) {
            add_node(node);
            node_hyperedges[node].push_back(hyperedge);
        }
    }
};
  
// 헤더 파일 (.h 또는 .hpp)에 추가
int count_valid_neighbors_with_bitmap(const Hypergraph& hypergraph, int v, int g, 
                                     const std::vector<bool>& active_nodes, int max_node);

void add_neighbors_to_T_list(const Hypergraph& hypergraph, int v, 
                            std::vector<int>& nodes_to_add, int max_node);

// 함수 선언들
Hypergraph load_hypergraph(const std::string& file_path);

std::unordered_map<int, int> neighbour_count_map(const Hypergraph& hypergraph, int v, int g);

std::unordered_set<int> get_neighbour(const Hypergraph& hypergraph, int v);

Hypergraph get_induced_subhypergraph(const Hypergraph& hypergraph, const std::unordered_set<int>& node_set);

std::unordered_set<int> find_kg_core(const Hypergraph& hypergraph, int k, int g);

std::unordered_set<int> kg_core_peeling(const Hypergraph& hypergraph, int k, int g);

std::vector<std::unordered_set<int>> enumerate_kg_core_fixing_g(const Hypergraph& hypergraph, int g);

std::shared_ptr<TreeNode> naive_index_construction(const Hypergraph& hypergraph, const std::vector<std::unordered_set<int>>& E);

std::vector<std::unordered_set<int>> enumerate_1_g(const Hypergraph& hypergraph, int g);

std::shared_ptr<TreeNode> one_level_compression(const Hypergraph& hypergraph, const std::vector<std::unordered_set<int>>& E);

std::pair<std::shared_ptr<TreeNode>, double> jump_compression(const Hypergraph& hypergraph, const std::vector<std::unordered_set<int>>& E);

std::tuple<std::shared_ptr<TreeNode>, double, double> diagonal_compression(const Hypergraph& hypergraph, const std::vector<std::unordered_set<int>>& E);

// 쿼리 함수들
const std::unordered_set<int>& querying_for_naive_index(const std::shared_ptr<TreeNode>& tree, int k, int g);

std::unordered_set<int> querying_for_one_level(const std::shared_ptr<TreeNode>& tree, int k, int g);

std::unordered_set<int> querying_for_two_level(const std::shared_ptr<TreeNode>& tree, int k, int g);

std::unordered_set<int> querying_for_diagonal(const std::shared_ptr<TreeNode>& tree, int k, int g);

std::unordered_set<int> kg_core(const Hypergraph& hypergraph, int k, int g);

// 유틸리티 함수들
int count_total_nodes(const std::shared_ptr<TreeNode>& tree, const std::string& type);

std::map<int, int> count_each_nodes(const std::shared_ptr<TreeNode>& tree, const std::string& type);

std::map<int, int> count_empty_leaf(const std::shared_ptr<TreeNode>& tree);

// 인덱스 저장/불러오기 함수들
bool save_index_to_file(const std::shared_ptr<TreeNode>& tree, const std::string& filename, const std::string& index_type = "binary");

std::shared_ptr<TreeNode> load_index_from_file(const std::string& filename, const std::string& index_type = "binary");

// 포인터 재구성 함수 (로드 후 next, jump 포인터 복원)
void reconstruct_pointers(std::shared_ptr<TreeNode>& tree);

// JSON 형태로 저장/불러오기 (디버깅용)
bool save_index_to_json(const std::shared_ptr<TreeNode>& tree, const std::string& filename);

std::shared_ptr<TreeNode> load_index_from_json(const std::string& filename);

// 텍스트 형태로 저장 (사람이 읽을 수 있는 형태)
bool save_index_to_text(const std::shared_ptr<TreeNode>& tree, const std::string& filename);

// 인덱스 메타데이터 저장/불러오기
struct IndexMetadata {
    std::string creation_time;
    std::string hypergraph_filename;
    std::string compression_type;  // "naive", "one_level", "jump", "diagonal"
    int num_nodes;
    int num_hyperedges;
    int max_g_level;
    double construction_time;
    size_t file_size_bytes;
};

bool save_metadata(const IndexMetadata& metadata, const std::string& filename);
IndexMetadata load_metadata(const std::string& filename);

// CSV 결과 로깅 함수들
void log_construction_result(const std::string& index_type, 
                           double construction_time,
                           int tree_nodes,
                           int vertices_in_index,
                           size_t memory_bytes);

void log_query_result(const std::string& index_type,
                     int k, int g,
                     double query_time,
                     int result_nodes);

// 유틸리티 함수들
std::string get_current_time_string();
size_t get_file_size(const std::string& filename);
bool save_complete_index(const std::shared_ptr<TreeNode>& tree, 
                        const std::string& base_filename,
                        const std::string& compression_type,
                        const Hypergraph& hypergraph,
                        double construction_time);

#endif // KG_INDEX_H