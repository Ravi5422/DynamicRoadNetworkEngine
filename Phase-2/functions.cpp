#include "graph.hpp"
#include <iostream>
#include <algorithm>
#include <cmath>
#include <queue>
#include <cstring>
#include <limits>


//yen's algo
std::vector<Path> Graph::kShortestPathsExact(int src, int dest, int k, const std::string& mode) {
    std::vector<Path> result;
    if (k <= 0) return result;

    //Find the first shortest path
    std::vector<int> pathNodes;
    double cost;
    if (!shortestPathDistance(src, dest, pathNodes, cost, {}, {})) return result;

    Path firstPath;
    firstPath.nodes = pathNodes;
    firstPath.cost = cost;
    firstPath.penalty = 0.0;

    for (size_t i = 0; i + 1 < pathNodes.size(); ++i) {
        int u = pathNodes[i], v = pathNodes[i + 1];
        for (Edge* e : adj[u]) {
            int w = (e->u == u) ? e->v : (!e->oneWay && e->v == u) ? e->u : -1;
            if (w == v && e->active) {
                firstPath.edges.push_back(e->id);
                break;
            }
        }
    }
    result.push_back(firstPath);

    struct Candidate { Path path; double cost; };
    auto cmp = [](const Candidate& a, const Candidate& b){ return a.cost > b.cost; };
    std::priority_queue<Candidate, std::vector<Candidate>, decltype(cmp)> candidates(cmp);

    std::unordered_set<std::string> seenPaths;
    auto pathHash = [](const std::vector<int>& nodes) {
        std::string s;
        for (int n : nodes) s += std::to_string(n) + ",";
        return s;
    };
    seenPaths.insert(pathHash(firstPath.nodes));

    // Yen's algorithm loop
    for (int r = 1; r < k; ++r) {
        const Path& prev = result[r - 1];

        for (size_t i = 0; i + 1 < prev.nodes.size(); ++i) {
            int spurNode = prev.nodes[i];
            std::vector<int> rootPath(prev.nodes.begin(), prev.nodes.begin() + i + 1);

            std::unordered_set<int> bannedNodes(rootPath.begin(), rootPath.end());
            bannedNodes.erase(spurNode);

            std::unordered_set<int> bannedEdges;
            for (const Path& p : result) {
                if (p.nodes.size() > i && std::equal(p.nodes.begin(), p.nodes.begin() + i, rootPath.begin())) {
                    bannedEdges.insert(p.edges[i]);
                }
            }

            // Compute spur path
            std::vector<int> spurPath;
            double spurCost;
            bool spurFound = shortestPathDistance(spurNode, dest, spurPath, spurCost, bannedNodes, bannedEdges);

            if (!spurFound) continue;

            // Combine root + spur
            std::vector<int> totalNodes = rootPath;
            totalNodes.insert(totalNodes.end(), spurPath.begin() + 1, spurPath.end());

            std::string hash = pathHash(totalNodes);
            if (seenPaths.count(hash)) continue;
            seenPaths.insert(hash);

            Path cand;
            cand.nodes = totalNodes;
            cand.cost = 0.0;
            cand.penalty = 0.0;

            // Build edges for total path
            for (size_t j = 0; j + 1 < totalNodes.size(); ++j) {
                int u = totalNodes[j], v = totalNodes[j+1];
                for (Edge* e : adj[u]) {
                    int w = (e->u == u) ? e->v : (!e->oneWay && e->v == u) ? e->u : -1;
                    if (w == v && e->active) {
                        cand.edges.push_back(e->id);
                        cand.cost += (mode == "distance") ? e->length : e->average_time;
                        break;
                    }
                }
            }

            candidates.push({cand, cand.cost});
        }

        if (candidates.empty()) break;

        Path next = candidates.top().path;
        candidates.pop();
        result.push_back(next);
    }

    return result;
}

std::vector<Path> Graph::kShortestPathsHeuristic( int src, int dest, int k, double overlapPenaltyFactor)
{
    std::vector<Path> result;
    if (k <= 0) return result;

    //best path
    std::vector<int> bestNodes;
    double bestDist;
    if (!shortestPathDistance(src, dest, bestNodes, bestDist, {}, {}))
        return result;

    // Build Path object
    Path bestPath;
    bestPath.nodes = bestNodes;
    bestPath.cost = bestDist;
    for (size_t i = 0; i + 1 < bestNodes.size(); ++i) {
        int u = bestNodes[i], v = bestNodes[i + 1];
        for (Edge* e : adj[u]) {
            int w = (e->u == u) ? e->v : (e->oneWay ? -1 : (e->v == u ? e->u : -1));
            if (w == v && e->active) {
                bestPath.edges.push_back(e->id);
                break;
            }
        }
    }
    result.push_back(bestPath);

    std::unordered_map<int, int> usage;
    for (int eid : bestPath.edges) usage[eid]++;

    for (int c = 1; c < k; c++) {
        std::vector<int> modified;
        std::vector<double> saved;

        //overlap penalty
        for (auto& [eid, ed] : edges) {
            if (!ed.active) continue;
            int u = usage[eid];
            if (u > 0) {
                modified.push_back(eid);
                saved.push_back(ed.length);
                ed.length *= (1.0 + overlapPenaltyFactor * u );
            }
        }

        std::vector<int> altNodes;
        double altDist;
        bool ok = shortestPathDistance(src, dest, altNodes, altDist, {}, {});

        // Restore original weights
        for (size_t i = 0; i < modified.size(); ++i)
            edges[modified[i]].length = saved[i];

        if (!ok || altNodes.empty()) break;

        bool duplicate = false;
        for (auto& p : result) {
            if (p.nodes == altNodes) { duplicate = true; break; }
        }
        if (duplicate) break;

        Path alt;
        alt.nodes = altNodes;
        double realCost = 0.0;

        for (size_t i = 0; i + 1 < altNodes.size(); ++i) {
            int u = altNodes[i], v = altNodes[i + 1];
            for (Edge* e : adj[u]) {
                int w = (e->u == u) ? e->v : (e->oneWay ? -1 : (e->v == u ? e->u : -1));
                if (w == v && e->active) {
                    alt.edges.push_back(e->id);
                    realCost += e->length;
                    break;
                }
            }
        }

        alt.cost = realCost;
        for (int eid : alt.edges) usage[eid]++;

        result.push_back(alt);
    }

    return result;
}

double Graph::aStarDistance(int src, int dest, double pct) {
    if (src == dest) return 0.0;

    const int N = numNodes;
    std::vector<double> g(N, INF);
    std::vector<bool> closed(N, false);
    g[src] = 0.0;

    // ε-admissible multiplier for approximate A*
    double eps = 1.0 + pct / 100.0;

    // Precompute cosine for longitude scaling
    double cosLatDest = cos(nodes[dest].lat * M_PI / 180.0);

    // Euclidean projection heuristic (scaled meters)
    auto heuristic = [&](int node) -> double {
        double dLat = (nodes[node].lat - nodes[dest].lat) * 111000.0;
        double dLon = (nodes[node].lon - nodes[dest].lon) * 111000.0 * cosLatDest;
        return sqrt(dLat * dLat + dLon * dLon);
    };

    using PQNode = std::pair<double, int>; // (f, node)
    std::priority_queue<PQNode, std::vector<PQNode>, std::greater<PQNode>> openSet;
    openSet.emplace(eps * heuristic(src), src);

    while (!openSet.empty()) {
        auto [fCurr, u] = openSet.top();
        openSet.pop();
        if (closed[u]) continue;
        closed[u] = true;

        if (u == dest) return g[dest];

        for (Edge* e : adj[u]) {
            if (!e->active) continue;

            int v = -1;
            if (e->u == u) v = e->v;
            else if (!e->oneWay && e->v == u) v = e->u;

            if (v == -1 || closed[v]) continue;

            double tentativeG = g[u] + e->length;
            if (tentativeG < g[v]) {
                g[v] = tentativeG;
                openSet.emplace(tentativeG + eps * heuristic(v), v);
            }
        }
    }

    return INF; // no path found
}

std::vector<std::tuple<int, int, double>> Graph::approxShortestDistances(const std::vector<std::pair<int,int>>& queries, double budget, double pct) {
    std::vector<std::tuple<int,int,double>> results;
    results.reserve(queries.size());
    for (auto &pr : queries) {
        int src = pr.first, dst = pr.second;
        double dist = aStarDistance(src, dst, pct);
        results.emplace_back(src, dst, dist);
    }
    return results;
}
