// basic graph functions that are common to all graphs.

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

void Graph::addNode(int id, double lat, double lon, const std::vector<std::string>& poisList) {
    Node& a = nodes[id];          
    a.id = id;
    a.lat = lat;
    a.lon = lon;
    a.pois = poisList;
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
    auto it = edges.find(edge_id);
    if (it == edges.end() || !it->second.active) return false;  
    
    it->second.active = false;
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