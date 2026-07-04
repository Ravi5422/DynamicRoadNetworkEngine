#include "graph.hpp"
#include <iostream>
#include <algorithm>
#include <cmath>
#include <queue>
#include <cstring>
#include <limits>

static const double INF = std::numeric_limits<double>::infinity();

// departTime is the time taken till now from start in sec
static double computeTravelTime(const Edge* e, double departTime){
    if (!e->speed_profile.empty()) {
        double remainingDist = e->length;

        // 900 sec = 15 min, and each slot = 15 min
        int slot_number = int(departTime / 900)%96;

        // time left for slot change
        double time_left = 900 * (int(departTime / 900) + 1) - departTime;

        double init_speed = e->speed_profile[slot_number];
        double time_spent = 0;

        while (init_speed * time_left < remainingDist) {
            remainingDist -= init_speed * time_left;
            time_spent += time_left;

            slot_number = (slot_number + 1) % 96;

            init_speed = e->speed_profile[slot_number];
            time_left = 900;
        }

        time_spent += remainingDist / init_speed;
        return time_spent;
    }
    else {
        return e->average_time;
    }
}

double geoDistance(double lat1, double lon1, double lat2, double lon2) {
    // degrees -> radians
    lat1 *= M_PI / 180.0;
    lon1 *= M_PI / 180.0;
    lat2 *= M_PI / 180.0;
    lon2 *= M_PI / 180.0;

    double dlat = lat2 - lat1;
    double dlon = lon2 - lon1;

    double sin_dlat = sin(dlat / 2.0);
    double sin_dlon = sin(dlon / 2.0);

    double a = sin_dlat * sin_dlat +
               cos(lat1) * cos(lat2) * sin_dlon * sin_dlon;

    // Clamp 'a' to [0, 1] to avoid NaNs from rounding error
    a = std::max(0.0, std::min(1.0, a));

    double c = 2.0 * atan2(std::sqrt(a), std::sqrt(1.0 - a));

    constexpr double R = 6371000.0; // meters
    return R * c;
}


bool Graph::shortestPathDistance(int src, int dest, std::vector<int>& outPath, double& outDist, 
    const std::unordered_set<int>& forbidNodes, const std::unordered_set<std::string>& forbidRoads)
{
    outPath.clear();
    outDist = 0.0;

    if (src == dest) {
        outPath.push_back(src);
        outDist = 0.0;
        return true;
    }

    int N = numNodes;
    std::vector<double> dist(N, INF);
    std::vector<int> parent(N, -1);
    std::priority_queue<std::pair<double, int>, std::vector<std::pair<double, int>>, std::greater<>> pq;

    dist[src] = 0.0;
    pq.emplace(0.0, src);

    while (!pq.empty()) {
        auto [curDist, u] = pq.top(); pq.pop();

        if (curDist > dist[u]) continue;
        if (forbidNodes.count(u)) continue;

        for (Edge* e : adj[u]) {
            if (!e->active) continue;
            if (forbidRoads.count(e->road_type)) continue;

            int v;
            if (e->u == u) v = e->v;
            else if (!e->oneway && e->v == u) v = e->u;
            else continue;

            if (forbidNodes.count(v)) continue;

            double alt = dist[u] + e->length;
            if (alt < dist[v]) {
                dist[v] = alt;
                parent[v] = u;
                pq.emplace(alt, v);
            }
        }
    }

    if (dist[dest] == INF) {
        outPath.clear();
        outDist = INF;
        return false;
    }

    std::vector<int> path;
    for (int v = dest; v != -1; v = parent[v]) path.push_back(v);
    std::reverse(path.begin(), path.end());

    outPath = std::move(path);
    outDist = dist[dest];
    return true;
}

bool Graph::shortestPathTime(int src, int dest, std::vector<int>& outPath, double& outTime, const std::unordered_set<int>& forbidNodes,
                             const std::unordered_set<std::string>& forbidRoads) 
{
    outPath.clear();
    outTime = 0.0;

    if (src == dest) {
        outPath.push_back(src);
        outTime = 0.0;
        return true;
    }

    int N = numNodes;
    std::vector<double> dist(N, INF);
    std::vector<int> parent(N, -1);
    std::priority_queue<std::pair<double, int>, std::vector<std::pair<double, int>>, std::greater<>> pq;

    dist[src] = 0.0;
    pq.emplace(0.0, src);

    while (!pq.empty()) {
        auto [curDist, u] = pq.top(); pq.pop();

        if (curDist > dist[u]) continue;
        if (forbidNodes.find(u) != forbidNodes.end()) continue;

        for (Edge* e : adj[u]) {
            if (!e->active) continue;
            if (forbidRoads.find(e->road_type) != forbidRoads.end()) continue;

            int v;
            if (e->u == u) v = e->v;
            else if (!e->oneway && e->v == u) v = e->u;
            else continue;

            if (forbidNodes.find(v) != forbidNodes.end()) continue;

            double travel = computeTravelTime(e, dist[u]);
            double alt = dist[u] + travel;
            if (alt < dist[v]) {
                dist[v] = alt;
                parent[v] = u;
                pq.emplace(alt, v);
            }
        }
    }

    if (dist[dest] == INF) {
        outPath.clear();
        outTime = INF;
        return false;
    }

    outTime = dist[dest];
    outPath.clear();
    for (int cur = dest; cur != -1; cur = parent[cur]) {
        outPath.push_back(cur);
    }
    std::reverse(outPath.begin(), outPath.end());
    return true;
}

std::vector<int> Graph::knnQueryEuclidean(const std::string& poiType, double queryLat, double queryLon, int k)
{
    std::vector<int> candidates;

    for (int i = 0; i < numNodes; ++i) {
        for (const std::string& poi : nodes[i].pois) {
            if (poi == poiType) {
                candidates.push_back(i);
                break;
            }
        }
    }

    if (candidates.empty()) return {};

    std::vector<std::pair<double, int>> distList;
    distList.reserve(candidates.size());

    for (int nodeId : candidates) {
        double dist = geoDistance(nodes[nodeId].lat, nodes[nodeId].lon, queryLat, queryLon);
        distList.emplace_back(dist, nodeId);
    }

    std::sort(distList.begin(), distList.end());

    std::vector<int> result;
    result.reserve(k);
    for (int i = 0; i < k && i < (int)distList.size(); ++i) {
        result.push_back(distList[i].second);
    }

    return result;
}


std::vector<int> Graph::knnQueryShortestPath(const std::string& poiType, double queryLat, double queryLon, int k)
{
    int startNode = -1;
    double bestDist = INF;
    for (int i = 0; i < numNodes; ++i) {
        double dist = geoDistance(nodes[i].lat, nodes[i].lon, queryLat, queryLon);
        if (dist < bestDist) {
            bestDist = dist;
            startNode = i;
        }
    }
    if (startNode == -1) {
        return {};
    }

    std::vector<bool> isTarget(numNodes, false);
    for (int i = 0; i < numNodes; ++i) {
        for (const std::string& poi : nodes[i].pois) {
            if (poi == poiType) {
                isTarget[i] = true;
                break;
            }
        }
    }

    std::vector<double> dist(numNodes, INF);
    std::vector<int> parent(numNodes, -1);
    std::vector<bool> vis(numNodes, false);
    using NodePair = std::pair<double, int>; // (dist, node)
    std::priority_queue<NodePair, std::vector<NodePair>, std::greater<NodePair>> pq;

    dist[startNode] = 0.0;
    pq.emplace(0.0, startNode);

    std::vector<int> result;
    result.reserve(k);

    while (!pq.empty() && (int)result.size() < k) {
        auto [curDist, u] = pq.top(); pq.pop();
        if (vis[u]) continue;
        vis[u] = true;

        if (isTarget[u]) {
            result.push_back(u);
            if ((int)result.size() == k) break;
        }

        for (Edge* e : adj[u]) {
            if (!e->active) continue;

            int v;
            if (e->u == u) v = e->v;
            else if (!e->oneway && e->v == u) v = e->u;
            else continue;

            double alt = dist[u] + e->length;
            if (alt < dist[v]) {
                dist[v] = alt;
                parent[v] = u;
                pq.emplace(alt, v);
            }
        }
    }

    return result;
}
