#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <utility>
#include <cassert>
#include "graph.hpp"
#include "../nlohmann/json.hpp"
using json = nlohmann::json;

int main(int argc, char* argv[]) {
    if (argc != 4) {
        std::cerr << "Usage: " << argv[0] << " <graph.json> <queries.json> <output.json>" << std::endl;
        return 1;
    }

    std::ifstream graphFile(argv[1]);
    std::ofstream outputFile(argv[3]);
    if (!graphFile.is_open()) {
        std::cerr << "Failed to open " << argv[1] << std::endl;
        return 1;
    }

    json graphJson;
    graphFile >> graphJson;

    // Building graph
    int n = graphJson["meta"]["nodes"];
    Graph graph(n);

    for (const auto& nodeData : graphJson["nodes"]) {
        int nodeId = nodeData["id"];
        double lat = nodeData["lat"];
        double lon = nodeData["lon"];
        std::vector<std::string> pois;
        if (nodeData.contains("pois")) {
            for (const auto& p : nodeData["pois"]) {
                pois.push_back(p.get<std::string>());
            }
        }
        graph.addNode(nodeId, lat, lon, pois);
    }

    for (const auto& edgeData : graphJson["edges"]) {
        int edgeId = edgeData["id"];
        int u = edgeData["u"];
        int v = edgeData["v"];
        double length = edgeData["length"];
        double avg_time = 0.0;
        if (edgeData.contains("average_time")) {
            avg_time = edgeData["average_time"];
        }
        bool oneway = edgeData.value("oneway", false);
        std::string roadType = edgeData.value("road_type", "");
        std::vector<double> speedProfile;
        if (edgeData.contains("speed_profile")) {
            for (const auto& sp : edgeData["speed_profile"]) {
                speedProfile.push_back(sp.get<double>());
            }
        }
        graph.addEdge(edgeId, u, v, length, avg_time, oneway, roadType, speedProfile);
    }

    std::ifstream queryFile(argv[2]);
    if (!queryFile.is_open()) {
        std::cerr << "Failed to open " << argv[2] << std::endl;
        return 1;
    }

    json queriesJson;
    queryFile >> queriesJson;
    queryFile.close();


    json outputJson;
    outputJson["meta"] = queriesJson["meta"];
    outputJson["results"] = json::array();

    //processing query
    for (const auto& event : queriesJson["events"]) {
        auto start_time = std::chrono::high_resolution_clock::now();
        std::string type = event["type"].get<std::string>();
        json result;
        if (type == "remove_edge") {
            int eid = event["edge_id"];
            graph.removeEdge(eid);
            result["id"] = event["id"];
            result["done"] = true;
        }
        else if (type == "modify_edge") {
            int eid = event["edge_id"];
            result["id"] = event["id"];
            if(graph.edges.find(eid) == graph.edges.end() || (graph.edges[eid].active && (!event.contains("patch") || event["patch"].empty()))) {
                result["done"] = false;
            }else{
                //may contain length, average_time, speed_profile, road type
                std::vector<double> newSpeed = graph.edges[eid].speed_profile;
                double newLength = graph.edges[eid].length;
                double newAvgTime = graph.edges[eid].average_time;
                std::string newRoadType = graph.edges[eid].road_type;

                if (event.contains("patch")) {
                    const auto& patch = event["patch"];

                    if (patch.contains("length")) newLength = patch["length"];
                    if (patch.contains("average_time")) newAvgTime = patch["average_time"];
                    if (patch.contains("speed_profile")) {
                        newSpeed.clear();
                        for (const auto& sp : patch["speed_profile"]) {
                            newSpeed.push_back(sp.get<double>());
                        }
                    }
                    if (patch.contains("road_type")) newRoadType = patch["road_type"];
                    graph.modifyEdge(eid, newSpeed, newLength, newAvgTime, newRoadType);
                }else{
                    graph.edges[eid].active = true;
                }
                result["done"] = true;
            }
        }
        else if (type == "k_shortest_paths") {
            int qid = event["id"];
            int src = event["source"];
            int dst = event["target"];
            int K = event["k"];
            std::string mode = event.value("mode", "distance");
            std::vector<Path> paths = graph.kShortestPathsExact(src, dst, K, mode);
            result["id"] = qid;
            result["paths"] = json::array();
            for (const Path& p : paths) {
                json pathObj;
                pathObj["path"] = p.nodes;
                pathObj["length"] = p.cost;
                result["paths"].push_back(pathObj);
            }
        }
        else if (type == "k_shortest_paths_heuristic") {
            int qid = event["id"];
            int src = event["source"];
            int dst = event["target"];
            int K = event["k"];
            double overlap_threshold = event["overlap_threshold"];
            std::vector<Path> paths = graph.kShortestPathsHeuristic(src, dst, K, overlap_threshold);
            result["id"] = qid;
            result["paths"] = json::array();
            for (const Path& p : paths) {
                json pathObj;
                pathObj["path"] = p.nodes;
                pathObj["length"] = p.cost;
                result["paths"].push_back(pathObj);
            }
        }
        else if (type == "approx_shortest_path") {
            int qid = event["id"];
            std::vector<std::pair<int,int>> queryPairs;
            for (const auto& q : event["queries"]) {
                int src = q["source"];
                int dst = q["target"];
                queryPairs.emplace_back(src, dst);
            }
            double time_budget = event["time_budget_ms"], acceptable_error_pct = event["acceptable_error_pct"];
            std::vector<std::tuple<int, int, double>> distList = graph.approxShortestDistances(queryPairs, time_budget, acceptable_error_pct);
            result["id"] = qid;
            result["distances"] = json::array();
            for(auto dists : distList){
                json distObj;
                auto [src, dest, length] = dists;
                distObj["source"] = src;
                distObj["target"] = dest;
                if(length < INF) distObj["approx_shortest_distance"] = length;
                else distObj["approx_shortest_distance"] = -1;
                result["distances"].push_back(distObj);
            }
        }
        else {
            continue;
        }
        auto end_time = std::chrono::high_resolution_clock::now();
        result["processing_time"] = std::chrono::duration<double, std::milli>(end_time - start_time).count();
        outputJson["results"].push_back(result);
    }

    // Write output JSON to file (with indentation for readability)
    outputFile << outputJson.dump(4);
    return 0;
}