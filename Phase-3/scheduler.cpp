#include "scheduler.hpp"
#include <algorithm>
#include <limits>
#include <iostream>
#include <cmath>
#include <cstdlib>
#include <list>
#include <random>
#include <unordered_map>

#define INF std::numeric_limits<double>::max()
#define SEED 42
#define K_MEANS_ITERATIONS 10
#define JUST_A_RANDOM_NUMBER 2654435761
#define MIN_PRICE 100.0
#define MAX_MINUS_MIN_PRICE_PLUS_1_BY_2 451
#define MAX_PRICE 1000.0
#define PREMIUM_CHANCE 25
#define CANCELLATION_CHANCE 5
#define PREMIUM_BENEFIT 5
#define PRICE_NORMALIZER 10
#define COST_SCALING_FACTOR 10.0
#define DROPOFF_BONUS 500.0
#define STALE_TIME 5.0
#define EPSILON_DISTANCE 1e-5

Scheduler::Scheduler(Graph& g) : graph(g), depotNode(0) {}

double Scheduler::calculateInsertionCost(int u, int v, int n) {
    double uv = graph.euclidean_estimate(u, v);
    double un = graph.euclidean_estimate(u, n);
    double nv = graph.euclidean_estimate(n, v);
    return un + nv - uv;
}

void Scheduler::kmeans_clustering(int K, std::vector<std::vector<int>>& assignments) {
    std::vector<int> valid_idxs;
    for (int i = 0; i < orders.size(); i++) 
        if (!orders[i].is_cancelled) 
            valid_idxs.push_back(i);
    if (valid_idxs.empty()) return;

    // if fewer orders than drivers
    if (valid_idxs.size() <= K) {
        for (int i = 0; i < valid_idxs.size(); i++)
            assignments[i].push_back(valid_idxs[i]);
        return;
    }

    std::vector<Point> centroids(K);
    auto pickupNode1 = graph.nodes[orders[valid_idxs[0]].pickupNode];
    auto dropNode1 = graph.nodes[orders[valid_idxs[0]].dropNode];
    centroids[0] = { (pickupNode1.lat + dropNode1.lat) / 2.0, (pickupNode1.lon + dropNode1.lon) / 2.0 };

    for (int i = 1; i < K; i++) {
        double max_dist = -1.0;
        int best_candidate = -1;
        
        // Find the order whose midpoint is furthest from all currently chosen centroids
        for (int idx : valid_idxs) {
            double min_dist_to_centroid = INF;
            auto pickupNode = graph.nodes[orders[idx].pickupNode];
            auto dropNode = graph.nodes[orders[idx].dropNode];
            double oLat = (pickupNode.lat + dropNode.lat) / 2.0;
            double oLon = (pickupNode.lon + dropNode.lon) / 2.0;
            
            for (int j = 0; j < i; ++j) {
                double d = std::pow(oLat - centroids[j].lat, 2) + std::pow(oLon - centroids[j].lon, 2);
                min_dist_to_centroid = std::min(min_dist_to_centroid, d);
            }
            if (min_dist_to_centroid > max_dist) {
                max_dist = min_dist_to_centroid;
                best_candidate = idx;
            }
        }

        if (best_candidate != -1) {
            auto pickupNode = graph.nodes[orders[best_candidate].pickupNode];
            auto dropNode = graph.nodes[orders[best_candidate].dropNode];
            double oLat = (pickupNode.lat + dropNode.lat) / 2.0;
            double oLon = (pickupNode.lon + dropNode.lon) / 2.0;
            centroids[i] = {oLat, oLon};
        } else {
            auto pickupNode = graph.nodes[orders[valid_idxs[i]].pickupNode];
            auto dropNode = graph.nodes[orders[valid_idxs[i]].dropNode];
            centroids[i] = { (pickupNode.lat + dropNode.lat) / 2.0, (pickupNode.lon + dropNode.lon) / 2.0 };
        }
    }

    for (int i = 0; i < K_MEANS_ITERATIONS; i++) {
        for(auto& group : assignments) group.clear();
        std::vector<Point> new_centroids(K, {0, 0});
        std::vector<int> counts(K);

        // Assign every order to the nearest centroid
        for (int idx : valid_idxs) {
            auto pickupNode = graph.nodes[orders[idx].pickupNode];
            auto dropNode = graph.nodes[orders[idx].dropNode];
            double oLat = (pickupNode.lat + dropNode.lat) / 2.0;
            double oLon = (pickupNode.lon + dropNode.lon) / 2.0;
            double bestDist = INF;
            int bestK = 0;

            for (int k = 0; k < K; k++) {
                double d = std::pow(oLat - centroids[k].lat, 2) + std::pow(oLon - centroids[k].lon, 2);
                if (d < bestDist) {
                    bestDist = d;
                    bestK = k;
                }
            }
            assignments[bestK].push_back(idx);
            new_centroids[bestK].lat += oLat;
            new_centroids[bestK].lon += oLon;
            counts[bestK]++;
        }

        // Moving centroids to average position of its assigned orders
        bool changed = false;
        for (int k = 0; k < K; ++k) {
            if (counts[k] > 0) {
                new_centroids[k].lat /= counts[k];
                new_centroids[k].lon /= counts[k];
                if (std::abs(new_centroids[k].lat - centroids[k].lat) > EPSILON_DISTANCE ||
                    std::abs(new_centroids[k].lon - centroids[k].lon) > EPSILON_DISTANCE) {
                    changed = true;
                }
                centroids[k] = new_centroids[k];
            }
        }
        if (!changed) break;
    }
}

json Scheduler::process_query(const json& query) {
    // for (auto ordre : query["events"]) {
        orders.clear();
        drivers.clear();

        // Parse orders
        for (auto& o : query["orders"]) {
            Order ord;
            ord.id = o["order_id"];
            ord.pickupNode = o["pickup"];
            ord.dropNode = o["dropoff"];

            int hash = (ord.id * JUST_A_RANDOM_NUMBER);
            std::mt19937 rng(SEED);
            std::uniform_int_distribution<int> percent_dist(0, 99);
            ord.order_price = MIN_PRICE + 2.0 * (hash / MAX_MINUS_MIN_PRICE_PLUS_1_BY_2);
            ord.is_premium = (percent_dist(rng) % 100) < PREMIUM_CHANCE;
            ord.is_cancelled = (percent_dist(rng) % 100) < CANCELLATION_CHANCE;
            orders.push_back(ord);
        }

        // Priority Score P = a * b
        // a = GIF((price / max_price) * 10), b = 1 (normal) or 5 (premium)
        for (auto& ord : orders) {
            if (ord.is_cancelled) {
                ord.priority_score = 0;
                continue;
            }

            int a = std::floor((ord.order_price / MAX_PRICE) * PRICE_NORMALIZER);
            int b = ord.is_premium ? PREMIUM_BENEFIT : 1;
            ord.priority_score = a * b;
        }

        // Parse Fleet
        int numDrivers = query["fleet"]["num_delivery_guys"];
        depotNode = query["fleet"]["depot_node"];

        solving_the_delivery_scheduling_problem(numDrivers);

        json output;
        output["assignments"] = json::array();
        double totalDeliveryTime = 0.0;
        double max_travel_time_s = 0.0;
        for(auto& o : orders) 
            if(!o.is_cancelled && o.delivered) 
                totalDeliveryTime += o.completionTime;

        for(auto& d : drivers) {
            json assign;
            assign["driver_id"] = d.driver_id;
            assign["route"] = d.route;
            assign["order_ids"] = d.order_ids;
            totalDeliveryTime += d.currentTime;
            max_travel_time_s = std::max(max_travel_time_s, d.currentTime);
            output["assignments"].push_back(assign);
        }
        json metrics;
        metrics["total_delivery_time_s"] = totalDeliveryTime;
        metrics["max_travel_time_s"] = max_travel_time_s;
        output["metrics"] = metrics;
        return output;
    // }
}

void Scheduler::solving_the_delivery_scheduling_problem(int numDrivers) {
    drivers.clear();
    if (numDrivers < 1) numDrivers = 1;
    drivers.resize(numDrivers);
    for (int i = 0; i < numDrivers; ++i) {
        drivers[i].driver_id = i;
        drivers[i].currentNode = depotNode;
        drivers[i].currentTime = 0.0;
        drivers[i].route.clear();
        drivers[i].actual_path.clear();
        drivers[i].order_ids.clear();
        drivers[i].carryingBag.clear();
    }

    if(orders.empty()) {
        for (int i = 0; i < numDrivers; i++) {
            drivers[i].route = {depotNode};
            drivers[i].actual_path = {depotNode};
        }
        return;
    }
    std::vector<std::vector<int>> assignments(numDrivers);
    kmeans_clustering(numDrivers, assignments);

    for(int i = 0; i < numDrivers; ++i) {
        if(i < assignments.size() && !assignments[i].empty()) {
            solve_driver_route(drivers[i], assignments[i]);
        } else {
            drivers[i].currentTime = 0.0;
            drivers[i].route = {depotNode};
            drivers[i].actual_path = {depotNode};
            drivers[i].currentNode = depotNode;
        }
    }
}


void Scheduler::solve_driver_route(Driver& driver, std::vector<int>& assigned_orders) {
    // Reset driver state
    driver.currentTime = 0.0;
    driver.currentNode = depotNode;
    driver.route.clear();
    driver.actual_path.clear();
    driver.order_ids.clear();
    driver.carryingBag.clear();

    // Filter-out cancelled orders for this driver
    std::vector<int> effective_orders;
    effective_orders.reserve(assigned_orders.size());
    for (int idx : assigned_orders) {
        if (!orders[idx].is_cancelled) {
            effective_orders.push_back(idx);
            driver.order_ids.push_back(orders[idx].id);
            orders[idx].pickedUp  = false;
            orders[idx].delivered = false;
            orders[idx].completionTime = 0.0;
        }
    }

    // If nothing to do, stay at depot
    if (effective_orders.empty()) {
        driver.route.push_back(depotNode);
        driver.actual_path.push_back(depotNode);
        return;
    }

    // Sort orders by priority_score (desc), tie-breaker = nearer pickup to depot
    std::sort(effective_orders.begin(), effective_orders.end(), [&](int a, int b) {
        const Order& oa = orders[a];
        const Order& ob = orders[b];
        if (oa.priority_score != ob.priority_score)
            return oa.priority_score > ob.priority_score;
        double da = graph.euclidean_estimate(depotNode, oa.pickupNode);
        double db = graph.euclidean_estimate(depotNode, ob.pickupNode);
        return da < db;
    });

    // Phase 1: build logical route on a small "shadow" graph using euclidean_estimate
    std::list<int> logical_route;
    logical_route.push_back(depotNode);

    for (int idx : effective_orders) {
        Order& ord = orders[idx];

        double bestScore = INF;
        auto bestPickupPos = logical_route.end();
        auto bestDropPos   = logical_route.end();

        // pickup/drop insertion heuristic
        for (auto itP = logical_route.begin(); itP != logical_route.end(); ++itP) {
            int u = *itP;
            auto itP_next = std::next(itP);
            int v = (itP_next == logical_route.end()) ? -1 : *itP_next;

            double pickupDelta = (v == -1)
                ? graph.euclidean_estimate(u, ord.pickupNode)
                : calculateInsertionCost(u, v, ord.pickupNode);

            // drop has to come at or after pickup position
            auto itDStart = itP_next;

            // special case: drop immediately after pickup at end
            if (itDStart == logical_route.end()) {
                double dropDelta = graph.euclidean_estimate(ord.pickupNode, ord.dropNode);
                double totalDelta = pickupDelta + dropDelta;

                // Bias by priority: higher priority effectively reduces cost
                double weighted = totalDelta / (1.0 + 0.1 * ord.priority_score);

                if (weighted < bestScore) {
                    bestScore = weighted;
                    bestPickupPos = itP_next;
                    bestDropPos   = logical_route.end(); // append at the end
                }
            } else {
                // general case: drop somewhere after pickup
                for (auto itD = itDStart; itD != logical_route.end(); ++itD) {
                    int x = *itD;
                    auto itD_next = std::next(itD);
                    int y = (itD_next == logical_route.end()) ? -1 : *itD_next;

                    double dropDelta = (y == -1)
                        ? graph.euclidean_estimate(x, ord.dropNode)
                        : calculateInsertionCost(x, y, ord.dropNode);

                    double totalDelta = pickupDelta + dropDelta;
                    double weighted   = totalDelta / (1.0 + 0.1 * ord.priority_score);

                    if (weighted < bestScore) {
                        bestScore = weighted;
                        bestPickupPos = itP_next;
                        bestDropPos   = itD_next; // insert before y
                    }
                }
            }
        }

        // apply best insertion (pickup first, then dropoff)
        logical_route.insert(bestPickupPos, ord.pickupNode);
        if (bestDropPos == logical_route.end()) {
            logical_route.insert(logical_route.end(), ord.dropNode);
        } else {
            logical_route.insert(bestDropPos, ord.dropNode);
        }
    }

    // Move logical route into driver.route
    driver.route.assign(logical_route.begin(), logical_route.end());

    // Phase 2: simulate this route on the real road graph with A*
    driver.actual_path.clear();
    driver.actual_path.push_back(depotNode);
    driver.currentNode = depotNode;
    driver.currentTime = 0.0;

    int prev = depotNode;

    for (size_t i = 1; i < driver.route.size(); ++i) {
        int next = driver.route[i];
        if (next == prev) continue;

        std::vector<int> segPath;
        double segTime = 0.0;
        bool ok = graph.getShortestPath(prev, next, segPath, segTime);

        if (!ok) {
            // If we cannot reach this stop, penalize and skip it
            driver.currentTime += STALE_TIME;
            continue;
        }

        driver.currentTime += segTime;
        if (segPath.size() > 1) {
            driver.actual_path.insert(
                driver.actual_path.end(),
                segPath.begin() + 1,
                segPath.end()
            );
        }
        prev = next;
        driver.currentNode = next;

        // Check pick/drop events for orders owned by this driver
        for (int idx : effective_orders) {
            Order& ord = orders[idx];
            if (ord.is_cancelled) continue;

            if (!ord.pickedUp && ord.pickupNode == next) {
                ord.pickedUp = true;
            }
            if (ord.pickedUp && !ord.delivered && ord.dropNode == next) {
                ord.delivered = true;
                ord.completionTime = driver.currentTime;
            }
        }
    }
}

