#include "../nlohmann/json.hpp"
#include <iostream>
#include <fstream>
#include <chrono>
#include <vector>
#include "graph.hpp"
#include "scheduler.hpp"

using json = nlohmann::json;
Graph* city_graph = nullptr;

int main(int argc, char* argv[]) {
    std::ios_base::sync_with_stdio(false);
    std::cin.tie(NULL);

    if (argc != 4) {
        std::cerr << "Usage: " << argv[0] << " <graph.json> <queries.json> <output.json>" << std::endl;
        return 1;
    }

    try {
        // Read graph from first file
        std::ifstream graph_file(argv[1]);
        if (!graph_file.is_open()) {
            std::cerr << "Failed to open " << argv[1] << std::endl;
            return 1;
        }
        json gData;
        graph_file >> gData;
        graph_file.close();

        int n = gData["meta"].value("nodes", 0);        
        int max_id = n;
        for(auto& node : gData["nodes"]) 
            max_id = std::max(max_id, node["id"].get<int>());

        city_graph = new Graph(max_id + 100);
        for (auto& node : gData["nodes"]) 
            city_graph->addNode(node["id"], node["lat"], node["lon"]);
            
        for (auto& edge : gData["edges"]) {
            city_graph->addEdge(
                edge.value("id", 0), 
                edge["u"].get<int>(), 
                edge["v"].get<int>(), 
                edge.value("length", 1.0), 
                edge.value("average_time", 10.0), 
                edge.value("oneway", false),
                edge.value("type", "road")
            );
        }

        // Read queries from second file
        std::ifstream queries_file(argv[2]);
        if (!queries_file.is_open()) {
            std::cerr << "Failed to open " << argv[2] << std::endl;
            return 1;
        }

        json queries_json;
        queries_file >> queries_json;
        queries_file.close();

        std::vector<json> results;
        Scheduler scheduler(*city_graph);

        for (auto& query : queries_json["events"]) {
            auto start_time = std::chrono::high_resolution_clock::now();
            json result = scheduler.process_query(query);
            auto end_time = std::chrono::high_resolution_clock::now();
            result["processing_time"] = std::chrono::duration<double, std::milli>(end_time - start_time).count();
            results.push_back(result);
        }

        std::ofstream output_file(argv[3]);
        if (!output_file.is_open()) {
            std::cerr << "Failed to open output.json for writing" << std::endl;
            return 1;
        }

        json output;
        output["results"] = results;
        output_file << output.dump(4) << std::endl;
        output_file.close();

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        if (city_graph) delete city_graph;
        return 1;
    }

    if (city_graph) delete city_graph;
    return 0;
}