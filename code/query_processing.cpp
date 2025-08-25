#include "kg_index.h"


// One level 인덱스 쿼리
std::unordered_set<int> querying_for_one_level(const std::shared_ptr<TreeNode>& tree, int k, int g) {
    std::unordered_set<int> empty_result;
    
    if (!tree || tree->children.empty()) return empty_result;
    
    int max_g = tree->children.size();
    if (g > max_g || g <= 0) {
        return empty_result;
    }
    
    int max_k = tree->children[g - 1]->children.size();
    if (k > max_k || k <= 0) {
        return empty_result;
    }
    
    std::unordered_set<int> core;
    auto header = tree->children[g - 1]->children[k - 1];
    
    while (header) {
        for (int node : header->value) {
            core.insert(node);
        }
        header = header->next;
    }
    
    return core;
}

// Two level 인덱스 쿼리 (jump compression 결과 쿼리)
std::unordered_set<int> querying_for_two_level(const std::shared_ptr<TreeNode>& tree, int k, int g) {
    std::unordered_set<int> empty_result;
    
    if (!tree || tree->children.empty()) return empty_result;
    
    int max_g = tree->children.size();
    if (g > max_g || g <= 0) {
        return empty_result;
    }
    
    int max_k = tree->children[g - 1]->children.size();
    if (k > max_k || k <= 0) {
        return empty_result;
    }
    
    std::vector<std::shared_ptr<TreeNode>> starters;
    auto header = tree->children[g - 1]->children[k - 1];
    
    // jump 포인터를 따라가며 시작점들 수집
    while (header) {
        starters.push_back(header);
        header = header->jump;
    }
    
    std::unordered_set<int> core;
    for (auto s : starters) {
        while (s) {
            for (int node : s->value) {
                core.insert(node);
            }
            s = s->next;
        }
    }
    
    return core;
}

// Diagonal 인덱스 쿼리
std::unordered_set<int> querying_for_diagonal(const std::shared_ptr<TreeNode>& tree, int k, int g) {
    std::unordered_set<int> empty_result;
    
    if (!tree || tree->children.empty()) return empty_result;
    
    int max_g = tree->children.size();
    if (g > max_g || g <= 0) {
        return empty_result;
    }
    
    int max_k = tree->children[g - 1]->children.size();
    if (k > max_k || k <= 0) {
        return empty_result;
    }
    
    std::vector<std::shared_ptr<TreeNode>> starters;
    auto header = tree->children[g - 1]->children[k - 1];
    
    // jump 포인터를 따라가며 시작점들 수집
    while (header) {
        starters.push_back(header);
        header = header->jump;
    }
    
    std::unordered_set<int> core;
    for (int s = 0; s < (int)starters.size(); s++) {
        auto head = starters[s];
        
        // head->value를 core에 추가
        for (int node : head->value) {
            core.insert(node);
        }
        
        // s != 0인 경우 aux[1]부터 aux[s]까지 추가
        if (s != 0) {
            for (int i = 1; i <= s; i++) {
                auto aux_it = head->aux.find(i);
                if (aux_it != head->aux.end()) {
                    for (int node : aux_it->second) {
                        core.insert(node);
                    }
                }
            }
        }
        
        head = head->next;
        int cnt = 1;
        
        while (head) {
            for (int node : head->value) {
                core.insert(node);
            }
            
            for (int i = 1; i <= cnt; i++) {
                auto aux_it = head->aux.find(i);
                if (aux_it != head->aux.end()) {
                    for (int node : aux_it->second) {
                        core.insert(node);
                    }
                }
            }
            
            head = head->next;
            cnt++;
        }
    }
    
    return core;
}

// 유틸리티 함수들
int count_total_nodes(const std::shared_ptr<TreeNode>& tree, const std::string& type) {
    if (!tree) return 0;
    
    if (type == "naive") {
        int total = 0;
        for (int i = 0; i < (int)tree->children.size(); i++) {
            for (int j = 0; j < (int)tree->children[i]->children.size(); j++) {
                total += tree->children[i]->children[j]->value.size();
            }
        }
        return total;
    }
    
    int total = 0;
    if (type == "diag") {
        for (int i = 0; i < (int)tree->children.size(); i++) {
            if (tree->children[i]->children.empty()) continue;
            auto head = tree->children[i]->children[0];
            
            while (head) {
                for (const auto& aux_pair : head->aux) {
                    total += aux_pair.second.size();
                }
                total += head->value.size();
                head = head->next;
            }
        }
        return total;
    } else {
        for (int i = 0; i < (int)tree->children.size(); i++) {
            if (tree->children[i]->children.empty()) continue;
            auto head = tree->children[i]->children[0];
            
            while (head) {
                total += head->value.size();
                head = head->next;
            }
        }
        return total;
    }
}

// 기타 유틸리티 함수들 (임시 구현)
std::map<int, int> count_each_nodes(const std::shared_ptr<TreeNode>& tree, const std::string& type) {
    return std::map<int, int>();
}

std::map<int, int> count_empty_leaf(const std::shared_ptr<TreeNode>& tree) {
    return std::map<int, int>();
}