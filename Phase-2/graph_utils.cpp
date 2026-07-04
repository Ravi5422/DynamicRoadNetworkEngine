
#include "graph.hpp"
#include <iostream>
#include <algorithm>
#include <cmath>
#include <queue>
#include <cstring>
#include <limits>


// constructor
Graph::Graph(int n) : numNodes(n) {
    nodes.resize(n);
    adj.resize(n);
}

// destructor
Graph::~Graph() {
    for (auto& edgeList : adj) {
        edgeList.clear(); 
    }
    adj.clear();
    nodes.clear();
    edges.clear();
}

void Graph::addNode(int id, double lat, double lon, const std::vector<std::string>& poisList){

    Node& a = nodes[id];

    a.id = id;
    a.lat=lat;
    a.lon=lon;

    a.pois.clear();
    for(auto x:poisList){
        a.pois.push_back(x);
    }

}

void Graph::addEdge(int id, int u, int v, double length, double avg_time,bool oneWay,
                     const std::string &road_type,const std::vector<double>& speed_profile)
{

    Edge e{ id, u, v, length, avg_time, oneWay, speed_profile, road_type, true };

    edges[id]=e;
    adj[u].push_back(&edges[id]);

    // if its both way add the edge in other vertice aswell
    if (!oneWay) {
        adj[v].push_back(&edges[id]);
    }
}

bool Graph::removeEdge(int edge_id){
    // if it is inactive or if the edge is not there return false
    if(!edges[edge_id].active || edges.find(edge_id) == edges.end()) return false;

    edges[edge_id].active=false;

    return true;
}


bool Graph::modifyEdge(int edge_id, const std::vector<double>& new_speed_profile,
                       double new_length, double new_avg_time, std::string newRoadType)
{
    if(!edges[edge_id].active) edges[edge_id].active = true;
    edges[edge_id].average_time = new_avg_time;
    edges[edge_id].length = new_length;
    edges[edge_id].road_type = newRoadType;
    edges[edge_id].speed_profile = new_speed_profile;
    return true;
}
// Time-dependent travel time helper (same as Phase 1)
static double computeTravelTime(const Edge* e, double departTime) {
    if (!e->speed_profile.empty()) {
        double t = departTime;
        double remainingDist = e->length;
        while (remainingDist > 1e-9) {
            long t_sec = static_cast<long>(t);
            int slotIndex = (t_sec % 86400) / 900;
            double slotStart = std::floor(t / 900.0) * 900.0;
            double slotEnd = slotStart + 900.0;
            if (slotIndex < 0 || slotIndex >= (int)e->speed_profile.size()) {
                return e->average_time + (t - departTime);
            }
            double speed = e->speed_profile[slotIndex];
            if (speed < 1e-6) {
                t = slotEnd;
                continue;
            }
            double timeToSlotEnd = slotEnd - t;
            double distPossible = speed * timeToSlotEnd;
            if (distPossible >= remainingDist) {
                t += remainingDist / speed;
                remainingDist = 0.0;
            } else {
                remainingDist -= distPossible;
                t = slotEnd;
            }
        }
        return t - departTime;
    } else {
        if (e->average_time > 0.0) {
            return e->average_time;
        } else {
            return e->length;
        }
    }
}

bool Graph::shortestPathDistance(int src, int dest, std::vector<int>& outPath, double& outDist,
                                 const std::unordered_set<int>& forbidNodes,
                                 const std::unordered_set<int>& forbidEdges) {
    outPath.clear();
    outDist = 0.0;
    if (src == dest) {
        outPath.push_back(src);
        return true;
    }

    int N = numNodes;
    std::vector<double> dist(N, INF);
    std::vector<int> parent(N, -1);
    dist[src] = 0.0;

    using NodePair = std::pair<double,int>;
    std::priority_queue<NodePair, std::vector<NodePair>, std::greater<NodePair>> pq;
    pq.emplace(0.0, src);

    std::vector<bool> visited(N, false);

    while (!pq.empty()) {
        double d = pq.top().first;
        int u = pq.top().second;
        pq.pop();

        if (d != dist[u]) continue;
        if (visited[u]) continue;
        visited[u] = true;
        if (u == dest) break;

        for (Edge* e : adj[u]) {
            if (!e->active) continue;
            if (forbidEdges.count(e->id)) continue; 

            int v = (e->u == u) ? e->v : (!e->oneWay && e->v == u) ? e->u : -1;
            if (v == -1) continue;
            if (forbidNodes.count(v)) continue;

            double newD = d + e->length;
            if (newD < dist[v]) {
                dist[v] = newD;
                parent[v] = u;
                pq.emplace(newD, v);
            }
        }
    }

    if (dist[dest] == INF) return false;

    outDist = dist[dest];
    outPath.clear();
    for (int cur = dest; cur != -1; cur = parent[cur]) {
        outPath.push_back(cur);
    }
    std::reverse(outPath.begin(), outPath.end());
    return true;
}

bool Graph::shortestPathTime(int src, int dest, std::vector<int>& outPath, double& outTime,
                             const std::unordered_set<int>& forbidNodes,
                             const std::unordered_set<std::string>& forbidRoads) {
    outPath.clear();
    outTime = 0.0;
    if (src == dest) {
        outPath.push_back(src);
        outTime = 0.0;
        return true;
    }
    int N = numNodes;
    std::vector<double> time(N, INF);
    std::vector<int> parent(N, -1);
    time[src] = 0.0;
    using NodePair = std::pair<double,int>;
    std::priority_queue<NodePair, std::vector<NodePair>, std::greater<NodePair>> pq;
    pq.emplace(0.0, src);
    std::vector<bool> processed(N, false);

    while (!pq.empty()) {
        double t = pq.top().first;
        int u = pq.top().second;
        pq.pop();
        if (t != time[u]) continue;
        if (processed[u]) continue;
        processed[u] = true;
        if (u == dest) break;
        for (Edge* e : adj[u]) {
            if (!e->active) continue;
            int v;
            if (e->u == u) {
                v = e->v;
            } else if (!e->oneWay && e->v == u) {
                v = e->u;
            } else {
                continue;
            }
            if (forbidNodes.count(v)) continue;
            if (forbidRoads.count(e->road_type)) continue;
            double travel = computeTravelTime(e, t);
            double arrive = t + travel;
            if (arrive < time[v]) {
                time[v] = arrive;
                parent[v] = u;
                pq.emplace(arrive, v);
            }
        }
    }
    if (time[dest] == INF) {
        return false;
    }
    outTime = time[dest];
    outPath.clear();
    for (int cur = dest; cur != -1; cur = parent[cur]) {
        outPath.push_back(cur);
    }
    std::reverse(outPath.begin(), outPath.end());
    return true;
}

