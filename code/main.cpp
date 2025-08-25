#include "kg_index.h"
#include <iostream>
#include <string>
#include <sys/resource.h>  // getrusage() 함수용
#include <unistd.h>        // getpid() 함수용
#include <fstream>         // /proc/self/status 읽기용
#include <ctime>        // 타임스탬프용
#include <sstream>      // std::istringstream용 추가
#include <map>          // std::map용 추가
#include <algorithm>    // std::sort용 추가
#include <random>       // 랜덤 선택용
#include <cmath>        // std::round용 추가
#include <chrono>       // std::chrono용 추가  
// 메모리 사용량 측정 함수들
size_t get_memory_usage_kb() {
    std::ifstream file("/proc/self/status");
    std::string line;
    while (std::getline(file, line)) {
        if (line.substr(0, 6) == "VmRSS:") {
            std::istringstream iss(line);
            std::string label;
            size_t memory_kb;
            iss >> label >> memory_kb;
            return memory_kb;
        }
    }
    return 0;
}

size_t get_peak_memory_usage_kb() {
    std::ifstream file("/proc/self/status");
    std::string line;
    while (std::getline(file, line)) {
        if (line.substr(0, 9) == "VmHWM:") {
            std::istringstream iss(line);
            std::string label;
            size_t memory_kb;
            iss >> label >> memory_kb;
            return memory_kb;
        }
    }
    return 0;
}

std::string format_memory(size_t kb) {
    if (kb < 1024) {
        return std::to_string(kb) + " KB";
    } else if (kb < 1024 * 1024) {
        return std::to_string(kb / 1024) + "." + std::to_string((kb % 1024) / 102) + " MB";
    } else {
        return std::to_string(kb / (1024 * 1024)) + "." + std::to_string((kb % (1024 * 1024)) / (102 * 1024)) + " GB";
    }
}

size_t tree_memory(const std::shared_ptr<TreeNode>& tree) {
    if (!tree) return 0;
    
    size_t total_size = 0;
    
    // TreeNode 자체의 크기
    total_size += sizeof(TreeNode);
    
    // name 문자열
    total_size += tree->name.capacity();
    
    // value set의 크기 - 노드 수 기준으로만 계산
    total_size += tree->value.size() * sizeof(int);
    
    // aux map의 크기 - 노드 수 기준으로만 계산
    for (const auto& aux_pair : tree->aux) {
        total_size += aux_pair.second.size() * sizeof(int); // value set의 노드들
    }
    
    // children의 크기
    total_size += tree->children.capacity() * sizeof(std::shared_ptr<TreeNode>);
    
    // 재귀적으로 자식들의 크기 계산
    for (const auto& child : tree->children) {
        total_size += tree_memory(child);
    }
    
    return total_size;
}

struct LeafNodeInfo;
std::vector<LeafNodeInfo> collect_leaf_nodes(const std::shared_ptr<TreeNode>& naive_tree);
std::vector<std::pair<int, int>> select_percentile_queries(const std::vector<LeafNodeInfo>& leaf_nodes);

// Leaf node 정보를 담는 구조체
struct LeafNodeInfo {
    int k;
    int g;
    size_t size;
    
    LeafNodeInfo(int k_val, int g_val, size_t s) : k(k_val), g(g_val), size(s) {}
};

int main(int argc, char* argv[]) {
    try {
        // 기본값 설정
        std::string hypergraph_file = "network.dat";
        int k = 1, g = 1;
        bool test_mode = false;
        bool test_naive = false;
        bool test_one_level = false;
        bool test_jump = false;
        bool test_diagonal = false;
        bool interactive_mode = false;
        bool benchmark_mode = false;  // 통합 interactive 모드
        
        // 간단한 명령행 파싱
        std::cout << "=== Command Line Arguments ===" << std::endl;
        for (int i = 0; i < argc; i++) {
            std::cout << "argv[" << i << "] = '" << argv[i] << "'" << std::endl;
        }
        
        // 파일명 찾기
        for (int i = 1; i < argc; i++) {
            std::string arg = argv[i];
            if (arg.substr(0, 7) == "--file=") {
                hypergraph_file = arg.substr(7);
                std::cout << "File set to: " << hypergraph_file << std::endl;
            }
            else if (arg == "--test-core") {
                test_mode = true;
                std::cout << "Test mode enabled" << std::endl;
            }
            else if (arg == "--test-naive" || arg == "--build=naive") {
                test_naive = true;
                std::cout << "Naive index test mode enabled" << std::endl;
            }
            else if (arg == "--interactive") {  // 통합 interactive 모드
                interactive_mode = true;
                std::cout << "Interactive mode enabled" << std::endl;
            }
            else if (arg == "--benchmark") {  // 🆕 벤치마크 모드 추가
                benchmark_mode = true;
                std::cout << "Benchmark mode enabled" << std::endl;
            }
            else if (arg == "--test-one-level" || arg == "--build=one-level") {
                test_one_level = true;
                std::cout << "One-level compression test mode enabled" << std::endl;
            }
            else if (arg == "--test-jump" || arg == "--build=jump") {
                test_jump = true;
                std::cout << "Jump compression test mode enabled" << std::endl;
            }
            else if (arg == "--test-diagonal" || arg == "--build=diagonal") {
                test_diagonal = true;
                std::cout << "Diagonal compression test mode enabled" << std::endl;
            }
            else if (arg.substr(0, 2) == "k=") {
                k = std::stoi(arg.substr(2));
                std::cout << "k set to: " << k << std::endl;
            }
            else if (arg.substr(0, 2) == "g=") {
                g = std::stoi(arg.substr(2));
                std::cout << "g set to: " << g << std::endl;
            }
        }
        
        // 도움말
        if (argc == 1) {
            std::cout << "\n=== Usage ===" << std::endl;
            std::cout << argv[0] << " --file=filename --test-core k=K g=G" << std::endl;
            std::cout << argv[0] << " --file=filename --test-naive" << std::endl;
            std::cout << argv[0] << " --file=filename --test-naive k=K g=G" << std::endl;
            std::cout << argv[0] << " --file=filename --interactive" << std::endl;  // 통합 interactive
            std::cout << argv[0] << " --file=filename --test-one-level" << std::endl;
            std::cout << argv[0] << " --file=filename --test-jump" << std::endl;
            std::cout << argv[0] << " --file=filename --test-diagonal" << std::endl;
            std::cout << argv[0] << " --file=filename --benchmark" << std::endl; 
            std::cout << argv[0] << " --file=filename --build=naive" << std::endl;
            std::cout << argv[0] << " --file=filename --build=one-level" << std::endl;
            std::cout << argv[0] << " --file=filename --build=jump" << std::endl;
            std::cout << argv[0] << " --file=filename --build=diagonal" << std::endl;
            std::cout << "\nExamples:" << std::endl;
            std::cout << argv[0] << " --file=real/contact/network.hyp --test-core k=1 g=1" << std::endl;
            std::cout << argv[0] << " --file=real/contact/network.hyp --test-naive" << std::endl;
            std::cout << argv[0] << " --file=real/contact/network.hyp --interactive" << std::endl;
            std::cout << argv[0] << " --file=real/contact/network.hyp --benchmark" << std::endl;  // 추가
            std::cout << argv[0] << " --file=real/contact/network.hyp --test-one-level" << std::endl;
            std::cout << argv[0] << " --file=real/contact/network.hyp --test-jump" << std::endl;
            std::cout << argv[0] << " --file=real/contact/network.hyp --test-diagonal" << std::endl;
            return 0;
        }
        
        // 하이퍼그래프 로드
        std::cout << "\n=== Loading Hypergraph ===" << std::endl;
        std::cout << "Loading from: " << hypergraph_file << std::endl;
        
        Hypergraph hypergraph = load_hypergraph(hypergraph_file);
        
        if (hypergraph.nodes().empty()) {
            std::cerr << "ERROR: Failed to load hypergraph or hypergraph is empty!" << std::endl;
            return -1;
        }
        
        std::cout << "✅ Successfully loaded hypergraph:" << std::endl;
        std::cout << "   Nodes: " << hypergraph.nodes().size() << std::endl;
        std::cout << "   Hyperedges: " << hypergraph.E.size() << std::endl;
        
        // 각종 테스트 모드들
        if (test_mode) {
            // 기존 test-core 모드 코드 그대로...
            std::cout << "\n=== Testing find_kg_core Function ===" << std::endl;
            std::cout << "Parameters: k=" << k << ", g=" << g << std::endl;
            
            std::cout << "🔍 Calling find_kg_core..." << std::endl;
            
            auto start_time = std::chrono::high_resolution_clock::now();
            auto result = find_kg_core(hypergraph, k, g);
            auto end_time = std::chrono::high_resolution_clock::now();
            
            auto duration = std::chrono::duration<double>(end_time - start_time).count();
            
            std::cout << "\n🎉 Results:" << std::endl;
            std::cout << "   (" << k << "," << g << ")-core size: " << result.size() << " nodes" << std::endl;
            std::cout << "   Computation time: " << std::fixed << std::setprecision(6) << duration << " seconds" << std::endl;
            std::cout << "   Density: " << std::fixed << std::setprecision(2) 
                      << (double)result.size() / hypergraph.nodes().size() * 100 << "%" << std::endl;
            
            // 결과 출력 로직...
            if (result.size() > 0) {
                std::cout << "\n📋 Core nodes ";
                if (result.size() <= 30) {
                    std::cout << "(all " << result.size() << "):" << std::endl;
                    std::cout << "   {";
                    bool first = true;
                    for (int node : result) {
                        if (!first) std::cout << ", ";
                        std::cout << node;
                        first = false;
                    }
                    std::cout << "}" << std::endl;
                } else {
                    std::cout << "(first 20 of " << result.size() << "):" << std::endl;
                    std::cout << "   {";
                    int count = 0;
                    for (int node : result) {
                        if (count >= 20) break;
                        if (count > 0) std::cout << ", ";
                        std::cout << node;
                        count++;
                    }
                    std::cout << ", ...}" << std::endl;
                }
            } else {
                std::cout << "\n❌ No nodes satisfy the (" << k << "," << g << ")-core condition" << std::endl;
                std::cout << "💡 Try smaller values like k=1 g=1" << std::endl;
            }
        } else if (benchmark_mode) {
            // 🆕 벤치마크 모드
            std::cout << "\n=== Benchmark Mode ===" << std::endl;
            std::cout << "🚀 Starting comprehensive benchmark..." << std::endl;
            
            // CSV 파일 경로 생성
            std::string dataset_path = hypergraph_file;
            std::string dataset_dir = dataset_path.substr(0, dataset_path.find_last_of("/\\"));
            std::string dataset_name = dataset_path.substr(dataset_path.find_last_of("/\\") + 1);
            std::string dataset_base = dataset_name.substr(0, dataset_name.find_last_of("."));
            std::string csv_file = dataset_dir + "/" + dataset_base + "_benchmark.csv";
            
            std::cout << "📄 Results will be saved to: " << csv_file << std::endl;
            
            // === STEP 1: Index Construction ===
            std::cout << "\n📍 Step 1/3: Building all indexes..." << std::endl;
            
            // 1-1. Naive Index
            std::cout << "  🔧 Building Naive index..." << std::endl;
            auto naive_start = std::chrono::high_resolution_clock::now();
            auto naive_tree = naive_index_construction(hypergraph, hypergraph.E);
            auto naive_end = std::chrono::high_resolution_clock::now();
            double naive_construction_time = std::chrono::duration<double>(naive_end - naive_start).count();
            std::cout << "     ✅ Completed (" << std::fixed << std::setprecision(3) << naive_construction_time << "s)" << std::endl;
            
            // 1-2. One-Level Index
            std::cout << "  🔧 Building One-level index..." << std::endl;
            auto one_level_start = std::chrono::high_resolution_clock::now();
            auto one_level_tree = one_level_compression(hypergraph, hypergraph.E);
            auto one_level_end = std::chrono::high_resolution_clock::now();
            double one_level_construction_time = std::chrono::duration<double>(one_level_end - one_level_start).count();
            std::cout << "     ✅ Completed (" << std::fixed << std::setprecision(3) << one_level_construction_time << "s)" << std::endl;
            
            // 1-3. Jump Index
            std::cout << "  🔧 Building Jump index..." << std::endl;
            auto jump_start = std::chrono::high_resolution_clock::now();
            auto [jump_tree, compression_rate] = jump_compression(hypergraph, hypergraph.E);
            auto jump_end = std::chrono::high_resolution_clock::now();
            double jump_construction_time = std::chrono::duration<double>(jump_end - jump_start).count();
            std::cout << "     ✅ Completed (" << std::fixed << std::setprecision(3) << jump_construction_time << "s)" << std::endl;
            
            // 1-4. Diagonal Index
            std::cout << "  🔧 Building Diagonal index..." << std::endl;
            auto diagonal_start = std::chrono::high_resolution_clock::now();
            auto [diagonal_tree, h_time, v_time] = diagonal_compression(hypergraph, hypergraph.E);
            auto diagonal_end = std::chrono::high_resolution_clock::now();
            double diagonal_construction_time = std::chrono::duration<double>(diagonal_end - diagonal_start).count();
            std::cout << "     ✅ Completed (" << std::fixed << std::setprecision(3) << diagonal_construction_time << "s)" << std::endl;
            
            // === STEP 2: Query Selection ===
            std::cout << "\n📍 Step 2/3: Selecting benchmark queries..." << std::endl;
            
            // Leaf node 수집
            auto leaf_nodes = collect_leaf_nodes(naive_tree);
            std::cout << "  📊 Found " << leaf_nodes.size() << " leaf nodes" << std::endl;
            
            // Percentile 기반 쿼리 선택
            auto selected_queries = select_percentile_queries(leaf_nodes);
            std::cout << "  🎯 Selected " << selected_queries.size() << " queries (1-100 percentile)" << std::endl;
            
            // 선택된 쿼리 미리보기
            std::cout << "  📋 Sample queries: ";
            for (int i = 0; i < std::min(5, (int)selected_queries.size()); i++) {
                if (i > 0) std::cout << ", ";
                std::cout << "(" << selected_queries[i].first << "," << selected_queries[i].second << ")";
            }
            if (selected_queries.size() > 5) std::cout << ", ...";
            std::cout << std::endl;
            
            // === STEP 3: Benchmark Execution ===
            std::cout << "\n📍 Step 3/3: Running benchmark queries..." << std::endl;
            
            double find_kg_core_total_time = 0.0;
            double naive_query_total_time = 0.0;
            double one_level_query_total_time = 0.0;
            double jump_query_total_time = 0.0;
            double diagonal_query_total_time = 0.0;
            
            int progress_count = 0;
            for (const auto& query : selected_queries) {
                int query_k = query.first;
                int query_g = query.second;
                
                progress_count++;
                if (progress_count % 20 == 0 || progress_count == (int)selected_queries.size()) {
                    std::cout << "  🔍 Progress: " << progress_count << "/" << selected_queries.size() 
                              << " (" << std::fixed << std::setprecision(1) 
                              << (double)progress_count / selected_queries.size() * 100 << "%)" << std::endl;
                }
                
                // 3-1. find_kg_core
                auto start = std::chrono::high_resolution_clock::now();
                auto result1 = find_kg_core(hypergraph, query_k, query_g);
                auto end = std::chrono::high_resolution_clock::now();
                find_kg_core_total_time += std::chrono::duration<double>(end - start).count();
                
                // 3-2. naive query
                start = std::chrono::high_resolution_clock::now();
                auto result2 = querying_for_naive_index(naive_tree, query_k, query_g);
                end = std::chrono::high_resolution_clock::now();
                naive_query_total_time += std::chrono::duration<double>(end - start).count();
                
                // 3-3. one-level query
                start = std::chrono::high_resolution_clock::now();
                auto result3 = querying_for_one_level(one_level_tree, query_k, query_g);
                end = std::chrono::high_resolution_clock::now();
                one_level_query_total_time += std::chrono::duration<double>(end - start).count();
                
                // 3-4. jump query
                start = std::chrono::high_resolution_clock::now();
                auto result4 = querying_for_two_level(jump_tree, query_k, query_g);
                end = std::chrono::high_resolution_clock::now();
                jump_query_total_time += std::chrono::duration<double>(end - start).count();
                
                // 3-5. diagonal query
                start = std::chrono::high_resolution_clock::now();
                auto result5 = querying_for_diagonal(diagonal_tree, query_k, query_g);
                end = std::chrono::high_resolution_clock::now();
                diagonal_query_total_time += std::chrono::duration<double>(end - start).count();
            }
            
            // === STEP 4: Results Output ===
            std::cout << "\n🎉 Benchmark completed!" << std::endl;
            std::cout << "\n📊 Summary:" << std::endl;
            std::cout << "  Index construction times:" << std::endl;
            std::cout << "    Naive:     " << std::fixed << std::setprecision(6) << naive_construction_time << "s" << std::endl;
            std::cout << "    One-level: " << one_level_construction_time << "s" << std::endl;
            std::cout << "    Jump:      " << jump_construction_time << "s" << std::endl;
            std::cout << "    Diagonal:  " << diagonal_construction_time << "s" << std::endl;
            
            std::cout << "\n  Query execution total times (100 queries):" << std::endl;
            std::cout << "    find_kg_core:    " << std::fixed << std::setprecision(6) << find_kg_core_total_time << "s" << std::endl;
            std::cout << "    Naive query:     " << naive_query_total_time << "s" << std::endl;
            std::cout << "    One-level query: " << one_level_query_total_time << "s" << std::endl;
            std::cout << "    Jump query:      " << jump_query_total_time << "s" << std::endl;
            std::cout << "    Diagonal query:  " << diagonal_query_total_time << "s" << std::endl;
            
            // === CSV 파일 저장 ===
            std::cout << "\n💾 Saving results to CSV..." << std::endl;
            
            std::ofstream csv_out(csv_file);
            if (csv_out.is_open()) {
                // CSV 헤더
                csv_out << "dataset,";
                csv_out << "naive_index_construction_time,one_level_construction_time,jump_construction_time,diagonal_construction_time,";
                csv_out << "find_kg_core_total_time,naive_query_total_time,one_level_query_total_time,jump_query_total_time,diagonal_query_total_time,";
                csv_out << "total_queries,timestamp" << std::endl;
                
                // 현재 시간
                auto now = std::time(nullptr);
                auto tm = *std::localtime(&now);
                char timestamp[100];
                std::strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", &tm);
                
                // 데이터 작성
                csv_out << dataset_name << ",";
                csv_out << std::fixed << std::setprecision(6);
                csv_out << naive_construction_time << "," << one_level_construction_time << "," 
                        << jump_construction_time << "," << diagonal_construction_time << ",";
                csv_out << find_kg_core_total_time << "," << naive_query_total_time << "," 
                        << one_level_query_total_time << "," << jump_query_total_time << "," << diagonal_query_total_time << ",";
                csv_out << selected_queries.size() << ",";
                csv_out << timestamp << std::endl;
                
                csv_out.close();
                std::cout << "✅ Results saved to: " << csv_file << std::endl;
            } else {
                std::cout << "❌ Failed to create CSV file: " << csv_file << std::endl;
            }
            
            // 성능 분석
            std::cout << "\n📈 Performance Analysis:" << std::endl;
            
            // 가장 빠른 쿼리 방법
            std::vector<std::pair<std::string, double>> query_times = {
                {"find_kg_core", find_kg_core_total_time},
                {"Naive query", naive_query_total_time},
                {"One-level query", one_level_query_total_time},
                {"Jump query", jump_query_total_time},
                {"Diagonal query", diagonal_query_total_time}
            };
            
            std::sort(query_times.begin(), query_times.end(), 
                      [](const auto& a, const auto& b) { return a.second < b.second; });
            
            std::cout << "  🏆 Query methods ranked by speed:" << std::endl;
            for (int i = 0; i < (int)query_times.size(); i++) {
                std::cout << "    " << (i + 1) << ". " << query_times[i].first 
                          << ": " << std::fixed << std::setprecision(6) << query_times[i].second << "s";
                if (i == 0) std::cout << " (fastest)";
                std::cout << std::endl;
            }
            
            // 스피드업 비율
            double fastest_time = query_times[0].second;
            std::cout << "\n  ⚡ Speedup vs " << query_times[0].first << ":" << std::endl;
            for (int i = 1; i < (int)query_times.size(); i++) {
                double speedup = query_times[i].second / fastest_time;
                std::cout << "    " << query_times[i].first << ": " 
                          << std::fixed << std::setprecision(2) << speedup << "x slower" << std::endl;
            }    
        } else if (interactive_mode) {
            // 🆕 통합 Interactive 모드
            std::cout << "\n=== Unified Interactive Mode ===" << std::endl;
            std::cout << "🔧 Building all index types..." << std::endl;
            
            size_t memory_before = get_memory_usage_kb();
            std::cout << "💾 Memory before construction: " << format_memory(memory_before) << std::endl;
            
            // 1. Naive Index 구성
            std::cout << "\n📍 Step 1/4: Building Naive Index..." << std::endl;
            auto naive_start = std::chrono::high_resolution_clock::now();
            auto naive_tree = naive_index_construction(hypergraph, hypergraph.E);
            auto naive_end = std::chrono::high_resolution_clock::now();
            auto naive_time = std::chrono::duration<double>(naive_end - naive_start).count();
            std::cout << "   ✅ Naive index completed (" << std::fixed << std::setprecision(3) << naive_time << "s)" << std::endl;
            
            // 2. One-Level Index 구성
            std::cout << "\n📍 Step 2/4: Building One-Level Index..." << std::endl;
            auto one_level_start = std::chrono::high_resolution_clock::now();
            auto one_level_tree = one_level_compression(hypergraph, hypergraph.E);
            auto one_level_end = std::chrono::high_resolution_clock::now();
            auto one_level_time = std::chrono::duration<double>(one_level_end - one_level_start).count();
            std::cout << "   ✅ One-level index completed (" << std::fixed << std::setprecision(3) << one_level_time << "s)" << std::endl;
            
            // 3. Jump Index 구성
            std::cout << "\n📍 Step 3/4: Building Jump Index..." << std::endl;
            auto jump_start = std::chrono::high_resolution_clock::now();
            auto [jump_tree, compression_rate] = jump_compression(hypergraph, hypergraph.E);
            auto jump_end = std::chrono::high_resolution_clock::now();
            auto jump_time = std::chrono::duration<double>(jump_end - jump_start).count();
            std::cout << "   ✅ Jump index completed (" << std::fixed << std::setprecision(3) << jump_time << "s)" << std::endl;
            
            // 4. Diagonal Index 구성
            std::cout << "\n📍 Step 4/4: Building Diagonal Index..." << std::endl;
            auto diagonal_start = std::chrono::high_resolution_clock::now();
            auto [diagonal_tree, h_time, v_time] = diagonal_compression(hypergraph, hypergraph.E);
            auto diagonal_end = std::chrono::high_resolution_clock::now();
            auto diagonal_time = std::chrono::duration<double>(diagonal_end - diagonal_start).count();
            std::cout << "   ✅ Diagonal index completed (" << std::fixed << std::setprecision(3) << diagonal_time << "s)" << std::endl;
            
            size_t memory_after = get_memory_usage_kb();
            size_t memory_used = memory_after - memory_before;
            
            std::cout << "\n🎉 All indexes constructed successfully!" << std::endl;
            std::cout << "   ⏱️  Total construction time: " << std::fixed << std::setprecision(3) 
                      << (naive_time + one_level_time + jump_time + diagonal_time) << " seconds" << std::endl;
            std::cout << "   💾 Total memory used: " << format_memory(memory_used) << std::endl;
            
            // 사용 가능한 범위 출력
            std::cout << "\n📋 Available query ranges:" << std::endl;
            for (int level_g = 1; level_g <= (int)naive_tree->children.size(); level_g++) {
                int max_k_for_g = naive_tree->children[level_g - 1]->children.size();
                if (max_k_for_g > 0) {
                    std::cout << "   g=" << level_g << ": k can be 1 to " << max_k_for_g << std::endl;
                }
            }
            
            // 대화형 쿼리 루프
            std::cout << "\n🎯 Unified Interactive Query Mode Started!" << std::endl;
            std::cout << "Available index types:" << std::endl;
            std::cout << "  1) Naive        - Basic index" << std::endl;
            std::cout << "  2) One-level    - One-level compression" << std::endl;
            std::cout << "  3) Jump         - Jump compression" << std::endl;
            std::cout << "  4) Diagonal     - Diagonal compression" << std::endl;
            std::cout << "\nCommands:" << std::endl;
            std::cout << "  method k,g     - Query using method (e.g., '1 2,1' or 'naive 2,1')" << std::endl;
            std::cout << "  help           - Show this help" << std::endl;
            std::cout << "  ranges         - Show available ranges" << std::endl;
            std::cout << "  compare k,g    - Compare all methods with same k,g" << std::endl;
            std::cout << "  quit/exit      - Exit program" << std::endl;
            std::cout << "========================================" << std::endl;
            
            std::string input;
            int query_count = 0;
            std::map<std::string, double> total_times;
            std::map<std::string, int> query_counts;
            
            while (true) {
                std::cout << "\n🔍 Enter command: ";
                
                if (!std::getline(std::cin, input)) {
                    break;
                }
                
                // 입력 정리
                input.erase(0, input.find_first_not_of(" \t"));
                input.erase(input.find_last_not_of(" \t") + 1);
                
                if (input.empty()) {
                    continue;
                }
                
                // 종료 명령 확인
                if (input == "quit" || input == "exit" || input == "q") {
                    break;
                }
                
                // 도움말 명령
                if (input == "help" || input == "h") {
                    std::cout << "\n📋 Available commands:" << std::endl;
                    std::cout << "  Method selection:" << std::endl;
                    std::cout << "    1 k,g  or  naive k,g     - Query using naive index" << std::endl;
                    std::cout << "    2 k,g  or  one k,g       - Query using one-level index" << std::endl;
                    std::cout << "    3 k,g  or  jump k,g      - Query using jump index" << std::endl;
                    std::cout << "    4 k,g  or  diag k,g      - Query using diagonal index" << std::endl;
                    std::cout << "  Other commands:" << std::endl;
                    std::cout << "    compare k,g              - Compare all methods" << std::endl;
                    std::cout << "    ranges                   - Show valid k,g ranges" << std::endl;
                    std::cout << "  Examples:" << std::endl;
                    std::cout << "    naive 2,1" << std::endl;
                    std::cout << "    3 2,1" << std::endl;
                    std::cout << "    compare 2,1" << std::endl;
                    continue;
                }
                
                // 범위 보기 명령
                if (input == "ranges" || input == "range") {
                    std::cout << "\n📋 Available query ranges:" << std::endl;
                    for (int level_g = 1; level_g <= (int)naive_tree->children.size(); level_g++) {
                        int max_k_for_g = naive_tree->children[level_g - 1]->children.size();
                        if (max_k_for_g > 0) {
                            std::cout << "   g=" << level_g << ": k can be 1 to " << max_k_for_g << std::endl;
                        }
                    }
                    continue;
                }
                
                // compare 명령 파싱
                if (input.substr(0, 7) == "compare") {
                    std::string params = input.substr(7);
                    params.erase(0, params.find_first_not_of(" \t"));
                    
                    int query_k = 0, query_g = 0;
                    size_t comma_pos = params.find(',');
                    size_t space_pos = params.find(' ');
                    
                    try {
                        if (comma_pos != std::string::npos) {
                            query_k = std::stoi(params.substr(0, comma_pos));
                            query_g = std::stoi(params.substr(comma_pos + 1));
                        } else if (space_pos != std::string::npos) {
                            query_k = std::stoi(params.substr(0, space_pos));
                            query_g = std::stoi(params.substr(space_pos + 1));
                        } else {
                            std::cout << "❌ Invalid format. Use 'compare k,g' (e.g., 'compare 2,1')" << std::endl;
                            continue;
                        }
                    } catch (const std::exception&) {
                        std::cout << "❌ Invalid numbers in compare command" << std::endl;
                        continue;
                    }
                    
                    if (query_k <= 0 || query_g <= 0) {
                        std::cout << "❌ k and g must be positive integers" << std::endl;
                        continue;
                    }
                    
                    std::cout << "\n🔍 Comparing all methods for (" << query_k << "," << query_g << ")-core..." << std::endl;
                    std::cout << "========================================" << std::endl;
                    
                    // Naive
                    auto naive_query_start = std::chrono::high_resolution_clock::now();
                    auto naive_result = querying_for_naive_index(naive_tree, query_k, query_g);
                    auto naive_query_end = std::chrono::high_resolution_clock::now();
                    auto naive_query_time = std::chrono::duration<double>(naive_query_end - naive_query_start).count();
                    
                    // One-level
                    auto one_level_query_start = std::chrono::high_resolution_clock::now();
                    auto one_level_result = querying_for_one_level(one_level_tree, query_k, query_g);
                    auto one_level_query_end = std::chrono::high_resolution_clock::now();
                    auto one_level_query_time = std::chrono::duration<double>(one_level_query_end - one_level_query_start).count();
                    
                    // Jump
                    auto jump_query_start = std::chrono::high_resolution_clock::now();
                    auto jump_result = querying_for_two_level(jump_tree, query_k, query_g);
                    auto jump_query_end = std::chrono::high_resolution_clock::now();
                    auto jump_query_time = std::chrono::duration<double>(jump_query_end - jump_query_start).count();
                    
                    // Diagonal
                    auto diagonal_query_start = std::chrono::high_resolution_clock::now();
                    auto diagonal_result = querying_for_diagonal(diagonal_tree, query_k, query_g);
                    auto diagonal_query_end = std::chrono::high_resolution_clock::now();
                    auto diagonal_query_time = std::chrono::duration<double>(diagonal_query_end - diagonal_query_start).count();
                    
                    // 결과 출력
                    std::cout << "📊 Comparison Results:" << std::endl;
                    std::cout << "  Naive:     " << std::fixed << std::setprecision(6) << naive_query_time 
                              << "s → " << naive_result.size() << " nodes" << std::endl;
                    std::cout << "  One-level: " << one_level_query_time 
                              << "s → " << one_level_result.size() << " nodes" << std::endl;
                    std::cout << "  Jump:      " << jump_query_time 
                              << "s → " << jump_result.size() << " nodes" << std::endl;
                    std::cout << "  Diagonal:  " << diagonal_query_time 
                              << "s → " << diagonal_result.size() << " nodes" << std::endl;
                    
                    // 가장 빠른 방법 찾기
                    double fastest_time = std::min({naive_query_time, one_level_query_time, jump_query_time, diagonal_query_time});
                    std::string fastest_method;
                    if (fastest_time == naive_query_time) fastest_method = "Naive";
                    else if (fastest_time == one_level_query_time) fastest_method = "One-level";
                    else if (fastest_time == jump_query_time) fastest_method = "Jump";
                    else fastest_method = "Diagonal";
                    
                    std::cout << "🏆 Fastest: " << fastest_method << " (" << std::fixed << std::setprecision(6) << fastest_time << "s)" << std::endl;
                    
                    // 결과 일치성 확인
                    bool results_match = (naive_result.size() == one_level_result.size() && 
                                        one_level_result.size() == jump_result.size() && 
                                        jump_result.size() == diagonal_result.size());
                    
                    if (results_match) {
                        std::cout << "✅ All methods returned the same number of nodes" << std::endl;
                    } else {
                        std::cout << "⚠️  Methods returned different numbers of nodes!" << std::endl;
                    }
                    
                    query_count++;
                    continue;
                }
                
                // 일반 쿼리 명령 파싱 (method k,g 형식)
                std::istringstream iss(input);
                std::string method_str;
                std::string params_str;
                
                if (!(iss >> method_str)) {
                    std::cout << "❌ Invalid format. Type 'help' for usage examples" << std::endl;
                    continue;
                }
                
                std::getline(iss, params_str);
                params_str.erase(0, params_str.find_first_not_of(" \t"));
                
                // 메서드 번호/이름 파싱
                int method_num = 0;
                std::string method_name;
                
                if (method_str == "1" || method_str == "naive") {
                    method_num = 1;
                    method_name = "Naive";
                } else if (method_str == "2" || method_str == "one" || method_str == "one-level") {
                    method_num = 2;
                    method_name = "One-level";
                } else if (method_str == "3" || method_str == "jump") {
                    method_num = 3;
                    method_name = "Jump";
                } else if (method_str == "4" || method_str == "diag" || method_str == "diagonal") {
                    method_num = 4;
                    method_name = "Diagonal";
                } else {
                    std::cout << "❌ Invalid method. Use 1-4 or naive/one/jump/diag" << std::endl;
                    continue;
                }
                
                // k,g 파싱
                int query_k = 0, query_g = 0;
                size_t comma_pos = params_str.find(',');
                size_t space_pos = params_str.find(' ');
                
                try {
                    if (comma_pos != std::string::npos) {
                        query_k = std::stoi(params_str.substr(0, comma_pos));
                        query_g = std::stoi(params_str.substr(comma_pos + 1));
                    } else if (space_pos != std::string::npos) {
                        query_k = std::stoi(params_str.substr(0, space_pos));
                        query_g = std::stoi(params_str.substr(space_pos + 1));
                    } else {
                        std::cout << "❌ Invalid format. Use 'method k,g' (e.g., 'naive 2,1')" << std::endl;
                        continue;
                    }
                } catch (const std::exception&) {
                    std::cout << "❌ Invalid numbers. Use integers only" << std::endl;
                    continue;
                }
                
                if (query_k <= 0 || query_g <= 0) {
                    std::cout << "❌ k and g must be positive integers" << std::endl;
                    continue;
                }
                
                // 쿼리 실행
                std::cout << "🔍 Querying (" << query_k << "," << query_g << ")-core using " << method_name << " index..." << std::endl;
                
                auto query_start = std::chrono::high_resolution_clock::now();
                std::unordered_set<int> query_result;
                
                switch (method_num) {
                    case 1:
                        query_result = querying_for_naive_index(naive_tree, query_k, query_g);
                        break;
                    case 2:
                        query_result = querying_for_one_level(one_level_tree, query_k, query_g);
                        break;
                    case 3:
                        query_result = querying_for_two_level(jump_tree, query_k, query_g);
                        break;
                    case 4:
                        query_result = querying_for_diagonal(diagonal_tree, query_k, query_g);
                        break;
                }
                
                auto query_end = std::chrono::high_resolution_clock::now();
                auto query_time = std::chrono::duration<double>(query_end - query_start).count();
                
                query_count++;
                total_times[method_name] += query_time;
                query_counts[method_name]++;
                
                // 결과 출력
                if (query_result.size() > 0) {
                    std::cout << "✅ Query #" << query_count << " successful!" << std::endl;
                    std::cout << "   📊 Method: " << method_name << std::endl;
                    std::cout << "   ⏱️  Query time: " << std::fixed << std::setprecision(6) << query_time << " seconds" << std::endl;
                    std::cout << "   📊 Result size: " << query_result.size() << " nodes" << std::endl;
                    std::cout << "   🎯 Density: " << std::fixed << std::setprecision(2) 
                              << (double)query_result.size() / hypergraph.nodes().size() * 100 << "%" << std::endl;
                    
                    // 결과 노드들 출력 (15개까지만)
                    if (query_result.size() <= 15) {
                        std::cout << "   📋 Nodes: {";
                        bool first = true;
                        for (int node : query_result) {
                            if (!first) std::cout << ", ";
                            std::cout << node;
                            first = false;
                        }
                        std::cout << "}" << std::endl;
                    } else {
                        std::cout << "   📋 First 10 nodes: {";
                        int count = 0;
                        for (int node : query_result) {
                            if (count >= 10) break;
                            if (count > 0) std::cout << ", ";
                            std::cout << node;
                            count++;
                        }
                        std::cout << ", ...}" << std::endl;
                    }
                } else {
                    std::cout << "❌ No nodes found for (" << query_k << "," << query_g << ")-core using " << method_name << std::endl;
                    std::cout << "   💡 Try smaller k or g values, or type 'ranges' for valid ranges" << std::endl;
                }
            }
            
            // 세션 요약
            std::cout << "\n📊 Session Summary:" << std::endl;
            std::cout << "   Total queries executed: " << query_count << std::endl;
            
            if (query_count > 0) {
                std::cout << "\n📈 Performance by method:" << std::endl;
                for (const auto& method : {"Naive", "One-level", "Jump", "Diagonal"}) {
                    if (query_counts[method] > 0) {
                        double avg_time = total_times[method] / query_counts[method];
                        std::cout << "   " << method << ": " << query_counts[method] << " queries, "
                                  << "avg " << std::fixed << std::setprecision(6) << avg_time << "s" << std::endl;
                    }
                }
                
                std::cout << "\n⚡ Index construction times:" << std::endl;
                std::cout << "   Naive: " << std::fixed << std::setprecision(3) << naive_time << "s" << std::endl;
                std::cout << "   One-level: " << one_level_time << "s" << std::endl;
                std::cout << "   Jump: " << jump_time << "s" << std::endl;
                std::cout << "   Diagonal: " << diagonal_time << "s" << std::endl;
            }
            
            std::cout << "\n👋 Interactive session ended. Goodbye!" << std::endl;
            
        
        } else if (test_naive) {
            std::cout << "\n=== Testing Naive Index Construction ===" << std::endl;
            
            // 구성 전 메모리 측정
            size_t memory_before = get_memory_usage_kb();
            std::cout << "💾 Memory before construction: " << format_memory(memory_before) << std::endl;
            
            auto start_time = std::chrono::high_resolution_clock::now();
            auto naive_tree = naive_index_construction(hypergraph, hypergraph.E);
            auto end_time = std::chrono::high_resolution_clock::now();
            
            auto duration = std::chrono::duration<double>(end_time - start_time).count();
            
            std::cout << "\n🎉 Naive Index Construction Results:" << std::endl;
            std::cout << "   ⏱️  Construction time: " << std::fixed << std::setprecision(6) << duration << " seconds" << std::endl;
            std::cout << "   📊 Index levels (g-values): " << naive_tree->children.size() << std::endl;
            
            // 구성 후 메모리 측정
            size_t memory_after = get_memory_usage_kb();
            size_t memory_used = memory_after - memory_before;
            std::cout << "   💾 Memory usage: " << format_memory(memory_used) << std::endl;
            
            // 인덱스 자체 메모리 추정
            size_t estimated_tree_size = tree_memory(naive_tree);
            std::cout << "   📦 Estimated index size: " << format_memory(estimated_tree_size / 1024) << std::endl;
            
            // === 노드 개수 계산 ===
            int total_index_nodes = 0;
            std::function<void(const std::shared_ptr<TreeNode>&)> count_nodes = 
                [&](const std::shared_ptr<TreeNode>& node) {
                    if (!node) return;
                    total_index_nodes += node->value.size();
                    for (const auto& aux_pair : node->aux) {
                        total_index_nodes += aux_pair.second.size();
                    }
                    for (const auto& child : node->children) {
                        count_nodes(child);
                    }
                };
            count_nodes(naive_tree);
            std::cout << "   📝 Total index node references: " << total_index_nodes << std::endl;
            
            // === Percentile Query Selection and Benchmarking ===
            std::cout << "\n=== Percentile Query Benchmarking ===" << std::endl;
            
            // Leaf node 수집
            auto leaf_nodes = collect_leaf_nodes(naive_tree);
            std::cout << "   📊 Found " << leaf_nodes.size() << " leaf nodes" << std::endl;
            
            // 벤치마크 변수들 초기화
            double total_query_time = 0.0;
            std::vector<std::pair<int, int>> selected_queries;
            
            if (leaf_nodes.empty()) {
                std::cout << "   ❌ No leaf nodes found - cannot perform benchmark" << std::endl;
            } else {
                // Percentile 기반 쿼리 선택
                selected_queries = select_percentile_queries(leaf_nodes);
                std::cout << "   🎯 Selected " << selected_queries.size() << " percentile queries" << std::endl;
                
                // 선택된 쿼리 미리보기
                std::cout << "   📋 Sample queries: ";
                for (int i = 0; i < std::min(5, (int)selected_queries.size()); i++) {
                    if (i > 0) std::cout << ", ";
                    std::cout << "(" << selected_queries[i].first << "," << selected_queries[i].second << ")";
                }
                if (selected_queries.size() > 5) std::cout << ", ...";
                std::cout << std::endl;
                
                // 모든 선택된 쿼리들에 대해 벤치마크 실행
                std::cout << "   🔍 Running " << selected_queries.size() << " benchmark queries..." << std::endl;
                
                int progress_count = 0;
                
                for (const auto& query : selected_queries) {
                    progress_count++;
                    
                    // 진행상황 표시 (20개마다 또는 마지막)
                    if (progress_count % 20 == 0 || progress_count == (int)selected_queries.size()) {
                        std::cout << "     Progress: " << progress_count << "/" << selected_queries.size() 
                                  << " (" << std::fixed << std::setprecision(1) 
                                  << (double)progress_count / selected_queries.size() * 100 << "%)" << std::endl;
                    }
                    
                    // 쿼리 실행 및 시간 측정
                    auto query_start = std::chrono::high_resolution_clock::now();
                    auto query_result = querying_for_naive_index(naive_tree, query.first, query.second);
                    auto query_end = std::chrono::high_resolution_clock::now();
                    
                    double query_time = std::chrono::duration<double>(query_end - query_start).count();
                    total_query_time += query_time;
                }
                
                // 벤치마크 결과 출력
                std::cout << "\n🎉 Naive Index Query Benchmark Results:" << std::endl;
                std::cout << "   📊 Total queries executed: " << selected_queries.size() << std::endl;
                std::cout << "   ⏱️  Total query time: " << std::fixed << std::setprecision(6) << total_query_time << " seconds" << std::endl;
                std::cout << "   ⚡ Average query time: " << std::fixed << std::setprecision(8) 
                          << total_query_time / selected_queries.size() << " seconds" << std::endl;
                std::cout << "   🎯 Queries per second: " << std::fixed << std::setprecision(2) 
                          << selected_queries.size() / total_query_time << " QPS" << std::endl;
            }
            
            // === 간단한 (1,1) 쿼리 테스트 ===
            std::cout << "\n=== Simple (1,1) Query Test ===" << std::endl;
            
            int simple_query_result_size = 0;
            double simple_query_time = 0.0;
            
            if (naive_tree->children.size() > 0 && naive_tree->children[0]->children.size() > 0) {
                auto query_start = std::chrono::high_resolution_clock::now();
                const auto& query_result = querying_for_naive_index(naive_tree, 1, 1);
                auto query_end = std::chrono::high_resolution_clock::now();
                
                simple_query_time = std::chrono::duration<double>(query_end - query_start).count();
                simple_query_result_size = query_result.size();
                
                std::cout << "   🔍 Simple query (1,1): " << simple_query_result_size << " nodes" << std::endl;
                std::cout << "   ⏱️  Query time: " << std::fixed << std::setprecision(8) << simple_query_time << " seconds" << std::endl;
            } else {
                std::cout << "   ❌ No index data available for (1,1) querying" << std::endl;
            }
            
            // === CSV 파일 저장 ===
            std::cout << "\n💾 Saving results to CSV..." << std::endl;
            
            // CSV 파일 경로 생성
            std::string dataset_path = hypergraph_file;
            std::string dataset_dir = dataset_path.substr(0, dataset_path.find_last_of("/\\"));
            std::string dataset_name = dataset_path.substr(dataset_path.find_last_of("/\\") + 1);
            std::string dataset_base = dataset_name.substr(0, dataset_name.find_last_of("."));
            std::string csv_file = dataset_dir + "/" + dataset_base + "_naive_results.csv";
            
            std::ofstream csv_out(csv_file);
            if (csv_out.is_open()) {
                // CSV 헤더
                csv_out << "dataset,";
                csv_out << "naive_construction_time,naive_index_memory_kb,naive_index_memory_bytes,naive_total_nodes,";
                csv_out << "index_levels,total_benchmark_queries,total_query_time,average_query_time,queries_per_second,";
                csv_out << "simple_query_nodes,simple_query_time,";
                csv_out << "unique_nodes,hyperedges,timestamp" << std::endl;
                
                // 현재 시간
                auto now = std::time(nullptr);
                auto tm = *std::localtime(&now);
                char timestamp[100];
                std::strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", &tm);
                
                // 데이터 작성
                csv_out << dataset_name << ",";
                csv_out << std::fixed << std::setprecision(6);
                csv_out << duration << "," << memory_used << "," << (memory_used * 1024) << "," << total_index_nodes << ",";
                csv_out << naive_tree->children.size() << "," << selected_queries.size() << ",";
                csv_out << total_query_time << ",";
                
                if (selected_queries.size() > 0) {
                    csv_out << (total_query_time / selected_queries.size()) << ",";
                    csv_out << (selected_queries.size() / total_query_time) << ",";
                } else {
                    csv_out << "0,0,";
                }
                
                csv_out << simple_query_result_size << "," << simple_query_time << ",";
                csv_out << hypergraph.nodes().size() << "," << hypergraph.E.size() << ",";
                csv_out << timestamp << std::endl;
                
                csv_out.close();
                std::cout << "✅ Naive results saved to: " << csv_file << std::endl;
            } else {
                std::cout << "❌ Failed to create CSV file: " << csv_file << std::endl;
            }
            
            // === 최종 요약 ===
            std::cout << "\n📊 Naive Index Analysis Summary:" << std::endl;
            std::cout << "   🏗️  Construction: " << std::fixed << std::setprecision(3) << duration << "s" << std::endl;
            std::cout << "   💾 Memory: " << format_memory(memory_used) << std::endl;
            std::cout << "   📊 Index efficiency: " << std::fixed << std::setprecision(1) 
                      << (double)total_index_nodes / hypergraph.nodes().size() << "x original data" << std::endl;
            
            if (selected_queries.size() > 0) {
                std::cout << "   ⚡ Avg query: " << std::fixed << std::setprecision(6) 
                          << total_query_time / selected_queries.size() << "s" << std::endl;
                std::cout << "   🎯 Throughput: " << std::fixed << std::setprecision(1) 
                          << selected_queries.size() / total_query_time << " QPS" << std::endl;
            }
            
            // 메모리 정리
            naive_tree.reset();
            std::cout << "   🗑️  Index cleaned up" << std::endl;
        } else if (test_one_level) {
            // one-level compression 인덱스 구성 테스트
            std::cout << "\n=== Testing One-Level Compression Index Construction ===" << std::endl;
            
            // 구성 전 메모리 측정
            size_t memory_before = get_memory_usage_kb();
            size_t peak_before = get_peak_memory_usage_kb();
            
            std::cout << "🔧 Building one-level compression index..." << std::endl;
            std::cout << "💾 Memory before construction: " << format_memory(memory_before) << std::endl;
            
            auto start_time = std::chrono::high_resolution_clock::now();
            auto one_level_tree = one_level_compression(hypergraph, hypergraph.E);
            auto end_time = std::chrono::high_resolution_clock::now();
            
            // 구성 후 메모리 측정
            size_t memory_after = get_memory_usage_kb();
            size_t peak_after = get_peak_memory_usage_kb();
            size_t memory_used = memory_after - memory_before;
            size_t peak_used = peak_after - peak_before;
            
            auto duration = std::chrono::duration<double>(end_time - start_time).count();
            
            std::cout << "\n🎉 One-Level Compression Results:" << std::endl;
            std::cout << "   ⏱️  Construction time: " << std::fixed << std::setprecision(6) << duration << " seconds" << std::endl;
            std::cout << "   📊 Index levels (g-values): " << one_level_tree->children.size() << std::endl;
            
            // 메모리 정보
            std::cout << "\n💾 Memory Usage Analysis:" << std::endl;
            std::cout << "   Current memory: " << format_memory(memory_after) << std::endl;
            std::cout << "   Memory increase: " << format_memory(memory_used) << std::endl;
            std::cout << "   Peak memory increase: " << format_memory(peak_used) << std::endl;
            
            // 인덱스 자체 메모리 추정
            size_t estimated_tree_size = tree_memory(one_level_tree);
            std::cout << "   Estimated index size: " << format_memory(estimated_tree_size / 1024) << std::endl;
            std::cout << "   Memory efficiency: " << std::fixed << std::setprecision(1) 
                      << (double)estimated_tree_size / (hypergraph.nodes().size() * sizeof(int)) << "x original data" << std::endl;
            
            // 각 레벨별 통계
            std::cout << "\n📈 Index Structure Analysis:" << std::endl;
            int total_cores = 0;
            for (int g = 0; g < (int)one_level_tree->children.size(); g++) {
                int level_cores = one_level_tree->children[g]->children.size();
                total_cores += level_cores;
            }
            
            std::cout << "   📊 Total cores: " << total_cores << std::endl;
            
            // 인덱스 크기 분석
            int total_entries = 0;
            for (int g = 0; g < (int)one_level_tree->children.size(); g++) {
                for (int k = 0; k < (int)one_level_tree->children[g]->children.size(); k++) {
                    total_entries += one_level_tree->children[g]->children[k]->value.size();
                }
            }
            std::cout << "   📝 Total index entries: " << total_entries << std::endl;
            std::cout << "   🔍 Storage overhead: " << std::fixed << std::setprecision(2) 
                      << (double)total_entries / hypergraph.nodes().size() << "x original nodes" << std::endl;
            
            // 평균 메모리 사용량 분석
            if (total_cores > 0) {
                double avg_memory_per_core = (double)estimated_tree_size / total_cores;
                std::cout << "   💾 Average memory per core: " << std::fixed << std::setprecision(1) 
                          << avg_memory_per_core << " bytes" << std::endl;
            }
            
        } else if (test_jump) {
            // jump compression 인덱스 구성 테스트
            std::cout << "\n=== Testing Jump Compression Index Construction ===" << std::endl;
            
            // 구성 전 메모리 측정
            size_t memory_before = get_memory_usage_kb();
            size_t peak_before = get_peak_memory_usage_kb();
            
            std::cout << "🔧 Building jump compression index..." << std::endl;
            std::cout << "💾 Memory before construction: " << format_memory(memory_before) << std::endl;
            
            auto start_time = std::chrono::high_resolution_clock::now();
            auto [jump_tree, compression_rate] = jump_compression(hypergraph, hypergraph.E);
            auto end_time = std::chrono::high_resolution_clock::now();
            
            // 구성 후 메모리 측정
            size_t memory_after = get_memory_usage_kb();
            size_t peak_after = get_peak_memory_usage_kb();
            size_t memory_used = memory_after - memory_before;
            size_t peak_used = peak_after - peak_before;
            
            auto duration = std::chrono::duration<double>(end_time - start_time).count();
            
            std::cout << "\n🎉 Jump Compression Results:" << std::endl;
            std::cout << "   ⏱️  Construction time: " << std::fixed << std::setprecision(6) << duration << " seconds" << std::endl;
            std::cout << "   📊 Index levels (g-values): " << jump_tree->children.size() << std::endl;
            std::cout << "   🗜️  Compression rate: " << std::fixed << std::setprecision(2) << compression_rate << std::endl;
            
            // 메모리 정보
            std::cout << "\n💾 Memory Usage Analysis:" << std::endl;
            std::cout << "   Current memory: " << format_memory(memory_after) << std::endl;
            std::cout << "   Memory increase: " << format_memory(memory_used) << std::endl;
            std::cout << "   Peak memory increase: " << format_memory(peak_used) << std::endl;
            
            // 인덱스 자체 메모리 추정
            size_t estimated_tree_size = tree_memory(jump_tree);
            std::cout << "   Estimated index size: " << format_memory(estimated_tree_size / 1024) << std::endl;
            std::cout << "   Memory efficiency: " << std::fixed << std::setprecision(1) 
                      << (double)estimated_tree_size / (hypergraph.nodes().size() * sizeof(int)) << "x original data" << std::endl;
            
            // 각 레벨별 통계
            std::cout << "\n📈 Index Structure Analysis:" << std::endl;
            int total_cores = 0;
            for (int g = 0; g < (int)jump_tree->children.size(); g++) {
                int level_cores = jump_tree->children[g]->children.size();
                total_cores += level_cores;
            }
            
            std::cout << "   📊 Total cores: " << total_cores << std::endl;
            
            // 인덱스 크기 분석
            int total_entries = 0;
            for (int g = 0; g < (int)jump_tree->children.size(); g++) {
                for (int k = 0; k < (int)jump_tree->children[g]->children.size(); k++) {
                    total_entries += jump_tree->children[g]->children[k]->value.size();
                }
            }
            std::cout << "   📝 Total index entries: " << total_entries << std::endl;
            std::cout << "   🔍 Storage overhead: " << std::fixed << std::setprecision(2) 
                      << (double)total_entries / hypergraph.nodes().size() << "x original nodes" << std::endl;
            
            // 평균 메모리 사용량 분석
            if (total_cores > 0) {
                double avg_memory_per_core = (double)estimated_tree_size / total_cores;
                std::cout << "   💾 Average memory per core: " << std::fixed << std::setprecision(1) 
                          << avg_memory_per_core << " bytes" << std::endl;
            }
            
        } else if (test_diagonal) {
            // 메모리 최적화된 통합 벤치마크 + Diagonal 분석
            std::cout << "\n=== Memory-Optimized Unified Benchmark + Diagonal Analysis ===" << std::endl;
            std::cout << "🔄 Processing order: Progressive Compression → Naive (memory ascending)" << std::endl;
            
            // CSV 파일 경로 생성
            std::string dataset_path = hypergraph_file;
            std::string dataset_dir = dataset_path.substr(0, dataset_path.find_last_of("/\\"));
            std::string dataset_name = dataset_path.substr(dataset_path.find_last_of("/\\") + 1);
            std::string dataset_base = dataset_name.substr(0, dataset_name.find_last_of("."));
            std::string csv_file = dataset_dir + "/" + dataset_base + "_unified_results.csv";
            
            std::cout << "📄 Results will be saved to: " << csv_file << std::endl;
            std::cout << "🚀 Memory-optimized processing with immediate cleanup..." << std::endl;
            
            // === 메모리 체크 ===
            size_t initial_memory = get_memory_usage_kb();
            std::cout << "🔍 Initial memory: " << format_memory(initial_memory) << std::endl;
            
            // 미사용 변수 제거를 위한 더미 사용
            (void)initial_memory;
            
            // === 노드 개수 계산 헬퍼 함수 ===
            auto calculate_total_node_references = [](const std::shared_ptr<TreeNode>& tree) -> int {
                if (!tree) return 0;
                int total = 0;
                std::function<void(const std::shared_ptr<TreeNode>&)> count_all = 
                    [&](const std::shared_ptr<TreeNode>& node) {
                        if (!node) return;
                        total += node->value.size();
                        for (const auto& aux_pair : node->aux) {
                            total += aux_pair.second.size();
                        }
                        for (const auto& child : node->children) {
                            count_all(child);
                        }
                    };
                count_all(tree);
                return total;
            };
            
            // === Original Dataset 분석 ===
            size_t original_memory = hypergraph.nodes().size() * sizeof(int) + hypergraph.E.size() * 20;
            int original_total_nodes = 0;
            for (const auto& hyperedge : hypergraph.E) {
                original_total_nodes += hyperedge.size();
            }
            
            std::cout << "\n📊 Dataset: " << hypergraph.nodes().size() << " nodes, " << hypergraph.E.size() << " edges" << std::endl;
            
            // === 결과 저장용 변수들 ===
            double naive_time = 0.0, step1_time = 0.0, step2_time = 0.0, step3_time = 0.0;
            size_t naive_memory = 0, one_level_memory = 0, jump_memory = 0, diagonal_memory = 0;
            int naive_nodes = 0, one_level_nodes = 0, jump_nodes = 0, diagonal_nodes = 0;
            
            // 쿼리 성능 저장용
            double naive_query_total = 0.0, one_level_query_total = 0.0;
            double jump_query_total = 0.0, diagonal_query_total = 0.0;
            double find_kg_core_total = 0.0;
            
            std::vector<std::pair<int, int>> benchmark_queries;
            
            // ========================================
            // PHASE 1: Progressive Compression (한 번에 모든 단계) - 먼저 처리
            // ========================================
            {
                std::cout << "\n📍 Phase 1: Progressive Compression (One-Level → Jump → Diagonal)" << std::endl;
                size_t memory_before = get_memory_usage_kb();
                std::cout << "  💾 Memory before progressive build: " << format_memory(memory_before) << std::endl;
                
                // === Step 1: One-Level ===
                std::cout << "  🔧 Building One-Level..." << std::endl;
                auto step1_start = std::chrono::high_resolution_clock::now();
                auto progressive_tree = one_level_compression(hypergraph, hypergraph.E);
                auto step1_end = std::chrono::high_resolution_clock::now();
                
                step1_time = std::chrono::duration<double>(step1_end - step1_start).count();
                one_level_memory = tree_memory(progressive_tree);
                one_level_nodes = calculate_total_node_references(progressive_tree);
                
                std::cout << "     ✅ One-Level: " << std::fixed << std::setprecision(3) << step1_time << "s, " 
                          << format_memory(one_level_memory / 1024) << ", " << one_level_nodes << " nodes" << std::endl;
                
                // === Benchmark Query Selection (첫 번째에서 수행) ===
                std::cout << "  🎯 Generating benchmark queries from One-Level index..." << std::endl;
                std::set<std::pair<int, int>> valid_queries;
                
                for (int g = 0; g < (int)progressive_tree->children.size(); g++) {
                    for (int k = 0; k < (int)progressive_tree->children[g]->children.size(); k++) {
                        valid_queries.insert({k + 1, g + 1});
                    }
                }
                
                // Percentile 기반 쿼리 선택
                std::vector<std::pair<int, int>> all_queries(valid_queries.begin(), valid_queries.end());
                benchmark_queries.clear();
                
                if (!all_queries.empty()) {
                    for (int percentile = 1; percentile <= 100; percentile++) {
                        double position = (percentile / 100.0) * (all_queries.size() - 1);
                        int index = (int)std::round(position);
                        benchmark_queries.push_back(all_queries[index]);
                    }
                }
                
                // 중복 제거
                std::sort(benchmark_queries.begin(), benchmark_queries.end());
                benchmark_queries.erase(std::unique(benchmark_queries.begin(), benchmark_queries.end()), benchmark_queries.end());
                
                std::cout << "  📋 Selected " << benchmark_queries.size() << " unique queries" << std::endl;
                
                // === One-Level Benchmark ===
                std::cout << "     🔍 One-Level queries..." << std::endl;
                for (const auto& query : benchmark_queries) {
                    auto start = std::chrono::high_resolution_clock::now();
                    auto result = querying_for_one_level(progressive_tree, query.first, query.second);
                    auto end = std::chrono::high_resolution_clock::now();
                    one_level_query_total += std::chrono::duration<double>(end - start).count();
                }
                std::cout << "     ⚡ " << std::fixed << std::setprecision(6) << one_level_query_total << "s" << std::endl;
                
                // === Step 2: Jump (기존 트리 수정) ===
                std::cout << "  🔧 Upgrading to Jump..." << std::endl;
                auto step2_start = std::chrono::high_resolution_clock::now();
                
                // progressive_tree를 jump로 변환
                int max_g = progressive_tree->children.size();
                for (int g = 0; g < max_g - 1; g++) {
                    int max_k = progressive_tree->children[g + 1]->children.size();
                    for (int k = 0; k < max_k; k++) {
                        if (k < (int)progressive_tree->children[g]->children.size()) {
                            progressive_tree->children[g]->children[k]->jump = progressive_tree->children[g + 1]->children[k];
                            
                            std::unordered_set<int> difference;
                            const auto& current_value = progressive_tree->children[g]->children[k]->value;
                            const auto& jump_value = progressive_tree->children[g]->children[k]->jump->value;
                            
                            for (int node : current_value) {
                                if (jump_value.find(node) == jump_value.end()) {
                                    difference.insert(node);
                                }
                            }
                            progressive_tree->children[g]->children[k]->value = std::move(difference);
                        }
                    }
                }
                
                auto step2_end = std::chrono::high_resolution_clock::now();
                step2_time = std::chrono::duration<double>(step2_end - step2_start).count();
                jump_memory = tree_memory(progressive_tree);
                jump_nodes = calculate_total_node_references(progressive_tree);
                
                std::cout << "     ✅ Jump: +" << std::fixed << std::setprecision(3) << step2_time << "s, " 
                          << format_memory(jump_memory / 1024) << ", " << jump_nodes << " nodes" << std::endl;
                
                // === Jump Benchmark ===
                std::cout << "     🔍 Jump queries..." << std::endl;
                for (const auto& query : benchmark_queries) {
                    auto start = std::chrono::high_resolution_clock::now();
                    auto result = querying_for_two_level(progressive_tree, query.first, query.second);
                    auto end = std::chrono::high_resolution_clock::now();
                    jump_query_total += std::chrono::duration<double>(end - start).count();
                }
                std::cout << "     ⚡ " << std::fixed << std::setprecision(6) << jump_query_total << "s" << std::endl;
                
                // === Step 3: Diagonal (기존 트리 수정) ===
                std::cout << "  🔧 Upgrading to Diagonal..." << std::endl;
                auto step3_start = std::chrono::high_resolution_clock::now();
                
                // 집합 연산 헬퍼 함수들
                auto set_intersection = [](const std::unordered_set<int>& set1, const std::unordered_set<int>& set2) {
                    std::unordered_set<int> result;
                    result.reserve(std::min(set1.size(), set2.size()));
                    for (int element : set1) {
                        if (set2.find(element) != set2.end()) {
                            result.insert(element);
                        }
                    }
                    return result;
                };
                
                auto set_difference = [](const std::unordered_set<int>& set1, const std::unordered_set<int>& set2) {
                    std::unordered_set<int> result;
                    result.reserve(set1.size());
                    for (int element : set1) {
                        if (set2.find(element) == set2.end()) {
                            result.insert(element);
                        }
                    }
                    return result;
                };
                
                // Diagonal compression logic
                for (int g = 0; g < (int)progressive_tree->children.size(); g++) {
                    if (progressive_tree->children[g]->children.empty()) continue;
                    auto head = progressive_tree->children[g]->children[0];
                    
                    if (!head->next) break;
                    
                    for (int k = 0; k < (int)progressive_tree->children[g]->children.size() - 1; k++) {
                        if (head->next && head->jump) {
                            auto diag = head->jump;
                            head = head->next;
                            auto intersect = set_intersection(head->value, diag->value);
                            
                            if (!diag->next) {
                                head->jump = std::make_shared<TreeNode>("aux");
                                diag->next = head->jump;
                            }
                            
                            diag->next->aux[1] = std::move(intersect);
                            
                            for (const auto& aux_pair : diag->aux) {
                                int i = aux_pair.first;
                                if (head->aux.find(i) != head->aux.end()) {
                                    diag->next->aux[i + 1] = set_intersection(diag->aux[i], head->aux[i]);
                                    head->aux[i] = set_difference(head->aux[i], diag->next->aux[i + 1]);
                                }
                            }
                            
                            head->value = set_difference(head->value, diag->next->aux[1]);
                            
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
                    }
                }
                
                auto step3_end = std::chrono::high_resolution_clock::now();
                step3_time = std::chrono::duration<double>(step3_end - step3_start).count();
                diagonal_memory = tree_memory(progressive_tree);
                diagonal_nodes = calculate_total_node_references(progressive_tree);
                
                std::cout << "     ✅ Diagonal: +" << std::fixed << std::setprecision(3) << step3_time << "s, " 
                          << format_memory(diagonal_memory / 1024) << ", " << diagonal_nodes << " nodes" << std::endl;
                
                // === Diagonal Benchmark ===
                std::cout << "     🔍 Diagonal queries..." << std::endl;
                for (const auto& query : benchmark_queries) {
                    auto start = std::chrono::high_resolution_clock::now();
                    auto result = querying_for_diagonal(progressive_tree, query.first, query.second);
                    auto end = std::chrono::high_resolution_clock::now();
                    diagonal_query_total += std::chrono::duration<double>(end - start).count();
                }
                std::cout << "     ⚡ " << std::fixed << std::setprecision(6) << diagonal_query_total << "s" << std::endl;
                
                size_t memory_final = get_memory_usage_kb();
                std::cout << "  💾 Progressive memory peak: " << format_memory(memory_final) << std::endl;
                
                // === 🗑️ Progressive 인덱스들 정리 ===
                progressive_tree.reset();
                size_t memory_cleaned = get_memory_usage_kb();
                std::cout << "  🗑️  Progressive indexes DELETED. Memory: " << format_memory(memory_cleaned) << std::endl;
                
                // 메모리 정리 대기
                std::cout << "  🧹 Memory cleanup pause..." << std::endl;
                auto cleanup_start = std::chrono::high_resolution_clock::now();
                while (std::chrono::duration<double>(std::chrono::high_resolution_clock::now() - cleanup_start).count() < 0.3) {
                    // 300ms 대기
                }
            }
            
            // ========================================
            // PHASE 2: Naive Index (가장 큰 메모리 - 마지막에 처리)
            // ========================================
            {
                std::cout << "\n📍 Phase 2: Naive Index (Largest Memory - Last)" << std::endl;
                size_t memory_before = get_memory_usage_kb();
                std::cout << "  💾 Memory before Naive construction: " << format_memory(memory_before) << std::endl;
                std::cout << "  ⚠️  This will likely use the most memory..." << std::endl;
                
                auto naive_start = std::chrono::high_resolution_clock::now();
                auto naive_tree = naive_index_construction(hypergraph, hypergraph.E);
                auto naive_end = std::chrono::high_resolution_clock::now();
                
                naive_time = std::chrono::duration<double>(naive_end - naive_start).count();
                naive_memory = tree_memory(naive_tree);
                naive_nodes = calculate_total_node_references(naive_tree);
                
                size_t memory_after = get_memory_usage_kb();
                
                std::cout << "  ✅ Construction: " << std::fixed << std::setprecision(3) << naive_time << "s" << std::endl;
                std::cout << "  💾 Memory: " << format_memory(memory_after) << " (+" << format_memory(memory_after - memory_before) << ")" << std::endl;
                std::cout << "  📊 Index size: " << format_memory(naive_memory / 1024) << ", " << naive_nodes << " nodes" << std::endl;
                
                // === Naive Benchmark + find_kg_core ===
                std::cout << "  🔍 Running naive benchmarks..." << std::endl;
                for (const auto& query : benchmark_queries) {
                    // find_kg_core 측정
                    auto start = std::chrono::high_resolution_clock::now();
                    auto result1 = find_kg_core(hypergraph, query.first, query.second);
                    auto end = std::chrono::high_resolution_clock::now();
                    find_kg_core_total += std::chrono::duration<double>(end - start).count();
                    
                    // naive query 측정
                    start = std::chrono::high_resolution_clock::now();
                    auto result2 = querying_for_naive_index(naive_tree, query.first, query.second);
                    end = std::chrono::high_resolution_clock::now();
                    naive_query_total += std::chrono::duration<double>(end - start).count();
                }
                std::cout << "  ✅ Benchmarks completed: " << std::fixed << std::setprecision(6) 
                          << naive_query_total << "s (naive), " << find_kg_core_total << "s (find_kg_core)" << std::endl;
                
                // === 🗑️ NAIVE INDEX 삭제 ===
                naive_tree.reset();
                size_t memory_final = get_memory_usage_kb();
                std::cout << "  🗑️ Naive index DELETED. Memory: " << format_memory(memory_final) << std::endl;
            }
            
            // ========================================
            // 결과 요약 및 저장 (나머지 동일)
            // ========================================
            
            double total_construction_time = naive_time + step1_time + step2_time + step3_time;
            
            std::cout << "\n🎉 Memory-Optimized Analysis Completed!" << std::endl;
            std::cout << "🔄 Processing order optimized memory usage throughout the benchmark" << std::endl;
            
            std::cout << "\n⏱️ Construction Times (Reordered):" << std::endl;
            std::cout << "   1. One-Level: " << std::fixed << std::setprecision(6) << step1_time << "s (smallest first)" << std::endl;
            std::cout << "   2. Jump: +" << step2_time << "s (upgrade)" << std::endl;
            std::cout << "   3. Diagonal: +" << step3_time << "s (upgrade)" << std::endl;
            std::cout << "   4. Naive: " << naive_time << "s (largest last)" << std::endl;
            
            std::cout << "\n⚡ Query Performance (" << benchmark_queries.size() << " queries):" << std::endl;
            std::cout << "   find_kg_core: " << std::fixed << std::setprecision(6) << find_kg_core_total << "s" << std::endl;
            std::cout << "   One-level: " << one_level_query_total << "s" << std::endl;
            std::cout << "   Jump: " << jump_query_total << "s" << std::endl;
            std::cout << "   Diagonal: " << diagonal_query_total << "s" << std::endl;
            std::cout << "   Naive: " << naive_query_total << "s" << std::endl;
            
            // === CSV 저장 ===
            std::cout << "\n💾 Saving results..." << std::endl;
            
            std::ofstream csv_out(csv_file);
            if (csv_out.is_open()) {
                csv_out << "dataset,processing_order,";
                csv_out << "original_memory_bytes,naive_memory_bytes,one_level_memory_bytes,jump_memory_bytes,diagonal_memory_bytes,";
                csv_out << "original_total_nodes,naive_nodes,one_level_nodes,jump_nodes,diagonal_nodes,";
                csv_out << "naive_construction_time,one_level_construction_time,jump_construction_time,diagonal_construction_time,";
                csv_out << "find_kg_core_query_time,naive_query_time,one_level_query_time,jump_query_time,diagonal_query_time,";
                csv_out << "total_queries,unique_nodes,hyperedges,timestamp" << std::endl;
                
                auto now = std::time(nullptr);
                auto tm = *std::localtime(&now);
                char timestamp[100];
                std::strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", &tm);
                
                csv_out << dataset_name << ",";
                csv_out << "progressive_then_naive,";  // 처리 순서 표시
                csv_out << original_memory << "," << naive_memory << "," << one_level_memory << "," << jump_memory << "," << diagonal_memory << ",";
                csv_out << original_total_nodes << "," << naive_nodes << "," << one_level_nodes << "," << jump_nodes << "," << diagonal_nodes << ",";
                csv_out << std::fixed << std::setprecision(6);
                csv_out << naive_time << "," << step1_time << "," << (step1_time + step2_time) << "," << (step1_time + step2_time + step3_time) << ",";
                csv_out << find_kg_core_total << "," << naive_query_total << "," << one_level_query_total << "," << jump_query_total << "," << diagonal_query_total << ",";
                csv_out << benchmark_queries.size() << "," << hypergraph.nodes().size() << "," << hypergraph.E.size() << ",";
                csv_out << timestamp << std::endl;
                
                csv_out.close();
                std::cout << "✅ Results saved to: " << csv_file << std::endl;
            }
            
            std::cout << "\n📈 Best method: ";
            std::vector<std::pair<std::string, double>> query_times = {
                {"Naive", naive_query_total}, {"One-level", one_level_query_total},
                {"Jump", jump_query_total}, {"Diagonal", diagonal_query_total}
            };
            std::sort(query_times.begin(), query_times.end(), 
                      [](const auto& a, const auto& b) { return a.second < b.second; });
            std::cout << query_times[0].first << " (" << std::fixed << std::setprecision(6) << query_times[0].second << "s)" << std::endl;
            
            std::cout << "\n🎯 Memory Optimization Benefits:" << std::endl;
            std::cout << "  ✅ Progressive indexes (One-Level → Jump → Diagonal) processed first" << std::endl;
            std::cout << "  ✅ All progressive indexes built from single construction call" << std::endl;
            std::cout << "  ✅ Naive (largest) processed last when progressive indexes are cleared" << std::endl;
            std::cout << "  ✅ Immediate cleanup after each phase" << std::endl;
        }
        
        else {
            // 기본 모드: 간단한 통계만
            std::cout << "\n=== Basic Statistics ===" << std::endl;
            std::cout << "Total nodes: " << hypergraph.nodes().size() << std::endl;
            std::cout << "Total hyperedges: " << hypergraph.E.size() << std::endl;
            
            // 하이퍼엣지 크기 분포
            std::map<int, int> size_dist;
            for (const auto& edge : hypergraph.E) {
                size_dist[edge.size()]++;
            }
            
            std::cout << "\nHyperedge size distribution:" << std::endl;
            for (const auto& pair : size_dist) {
                std::cout << "   Size " << pair.first << ": " << pair.second << " edges" << std::endl;
            }
            
            std::cout << "\n💡 To test find_kg_core function:" << std::endl;
            std::cout << "   " << argv[0] << " --file=" << hypergraph_file << " --test-core k=1 g=1" << std::endl;
            std::cout << "\n💡 To test naive index construction:" << std::endl;
            std::cout << "   " << argv[0] << " --file=" << hypergraph_file << " --test-naive" << std::endl;
            std::cout << "\n💡 To test one-level compression index construction:" << std::endl;
            std::cout << "   " << argv[0] << " --file=" << hypergraph_file << " --test-one-level" << std::endl;
            std::cout << "\n💡 To test jump compression index construction:" << std::endl;
            std::cout << "   " << argv[0] << " --file=" << hypergraph_file << " --test-jump" << std::endl;
            std::cout << "\n💡 To test diagonal compression index construction:" << std::endl;
            std::cout << "   " << argv[0] << " --file=" << hypergraph_file << " --test-diagonal" << std::endl;
        }
        
        std::cout << "\n✅ Program completed successfully!" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "❌ Error: " << e.what() << std::endl;
        return -1;
    }
    
    return 0;
}

std::vector<LeafNodeInfo> collect_leaf_nodes(const std::shared_ptr<TreeNode>& naive_tree) {
    std::vector<LeafNodeInfo> leaf_nodes;
    
    for (int g = 0; g < (int)naive_tree->children.size(); g++) {
        for (int k = 0; k < (int)naive_tree->children[g]->children.size(); k++) {
            auto& leaf = naive_tree->children[g]->children[k];
            size_t node_size = leaf->value.size();
            
            // k는 1부터 시작, g도 1부터 시작
            leaf_nodes.emplace_back(k + 1, g + 1, node_size);
        }
    }
    
    return leaf_nodes;
}

std::vector<std::pair<int, int>> select_percentile_queries(const std::vector<LeafNodeInfo>& leaf_nodes) {
    // 크기순으로 정렬
    std::vector<LeafNodeInfo> sorted_nodes = leaf_nodes;
    std::sort(sorted_nodes.begin(), sorted_nodes.end(), 
              [](const LeafNodeInfo& a, const LeafNodeInfo& b) {
                  return a.size < b.size;
              });
    
    std::vector<std::pair<int, int>> selected_queries;
    std::random_device rd;
    std::mt19937 gen(rd());
    
    // 1부터 100 percentile까지
    for (int percentile = 1; percentile <= 100; percentile++) {
        // percentile에 해당하는 인덱스 계산
        double position = (percentile / 100.0) * (sorted_nodes.size() - 1);
        int index = (int)std::round(position);
        
        // 동일한 크기의 노드들이 여러 개 있을 수 있으므로 해당 크기의 노드들 중 랜덤 선택
        size_t target_size = sorted_nodes[index].size;
        
        std::vector<int> candidates;
        for (int i = 0; i < (int)sorted_nodes.size(); i++) {
            if (sorted_nodes[i].size == target_size) {
                candidates.push_back(i);
            }
        }
        
        // 후보 중 랜덤 선택
        std::uniform_int_distribution<> dis(0, candidates.size() - 1);
        int chosen_idx = candidates[dis(gen)];
        
        selected_queries.emplace_back(sorted_nodes[chosen_idx].k, sorted_nodes[chosen_idx].g);
    }
    
    return selected_queries;
}