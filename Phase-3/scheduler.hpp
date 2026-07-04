#ifndef SCHEDULER_HPP
#define SCHEDULER_HPP

#include "graph.hpp"
#include "../nlohmann/json.hpp"
#include <vector>

using json = nlohmann::json;

struct Point {
    double lat, lon;
};

struct Order {
    int id;
    int pickupNode;
    int dropNode;
    bool pickedUp = false;
    bool delivered = false;
    double completionTime = 0.0;

    double order_price;     
    bool is_premium;        
    bool is_cancelled;      
    int priority_score;
};

struct Driver {
    int driver_id;
    int currentNode;
    double currentTime;
    std::vector<int> route; 
    std::vector<int> order_ids;
    std::vector<int> carryingBag;
    std::vector<int> actual_path;
};

class Scheduler {
    Graph& graph;
    std::vector<Order> orders;
    std::vector<Driver> drivers;
    int depotNode;

    double calculateInsertionCost(int u, int v, int n);
    void kmeans_clustering(int K, std::vector<std::vector<int>>& assignments);
    void solve_driver_route(Driver& driver, std::vector<int>& assigned_orders);
    // void optimize_route(Driver& driver);
    // void simulate_and_calculate_metrics(json& result_metrics);
    void solving_the_delivery_scheduling_problem(int numDrivers);

public:
    Scheduler(Graph& g);
    json process_query(const json& query);
};

#endif // SCHEDULER_HPP