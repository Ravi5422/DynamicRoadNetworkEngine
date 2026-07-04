#!/usr/bin/env python3

import json
import math
import random

# Configuration
N_NODES = 100
N_EVENTS = 3
MIN_ORDERS_PER_EVENT = 3
MAX_ORDERS_PER_EVENT = 7
DEPOT_NODE = 0

BASE_LAT = 19.0760   # roughly Mumbai, just to look "real"
BASE_LON = 72.8777

random.seed(42)

def generate_nodes():
    nodes = []
    for i in range(N_NODES):
        # random jitter of up to ~3km in each direction
        dlat = random.uniform(-0.03, 0.03)
        dlon = random.uniform(-0.03, 0.03)
        nodes.append({
            "id": i,
            "lat": BASE_LAT + dlat,
            "lon": BASE_LON + dlon
        })
    return nodes

def approx_length_m(a, b):
    # cheap distance approximation in meters using lat/lon
    dlat = (a["lat"] - b["lat"]) * 111_000.0
    # longitude length shrinks by cos(lat); use base lat
    dlon = (a["lon"] - b["lon"]) * 111_000.0 * math.cos(math.radians(BASE_LAT))
    return math.hypot(dlat, dlon)

def generate_edges(nodes, k_nearest=4):
    edges = []
    edge_id = 0
    N = len(nodes)
    for i in range(N):
        dists = []
        for j in range(N):
            if i == j:
                continue
            d = approx_length_m(nodes[i], nodes[j])
            dists.append((d, j))
        dists.sort()
        # connect to k nearest neighbors
        for d, j in dists[:k_nearest]:
            if i < j:  # avoid duplicates
                length = d
                # random average speed between 5 and 15 m/s
                speed = random.uniform(5.0, 15.0)
                avg_time = length / speed
                edges.append({
                    "id": edge_id,
                    "u": i,
                    "v": j,
                    "length": length,
                    "average_time": avg_time,
                    "oneway": False,
                    "type": "road"
                })
                edge_id += 1
    return edges

def generate_graph():
    nodes = generate_nodes()
    edges = generate_edges(nodes)
    graph = {
        "meta": {
            "nodes": len(nodes)
        },
        "nodes": nodes,
        "edges": edges
    }
    with open("graph.json", "w") as f:
        json.dump(graph, f, indent=2)
    print(f"graph.json written with {len(nodes)} nodes and {len(edges)} edges")

def generate_queries():
    events = []
    global_order_id = 1
    for _ in range(N_EVENTS):
        n_orders = random.randint(MIN_ORDERS_PER_EVENT, MAX_ORDERS_PER_EVENT)
        orders = []
        for _ in range(n_orders):
            pickup = random.randint(1, N_NODES - 1)
            dropoff = random.randint(1, N_NODES - 1)
            while dropoff == pickup:
                dropoff = random.randint(1, N_NODES - 1)
            price = random.uniform(150.0, 800.0)
            is_premium = random.random() < 0.3   # ~30% premium
            is_cancelled = random.random() < 0.1 # ~10% cancelled

            orders.append({
                "order_id": global_order_id,
                "pickup": pickup,
                "dropoff": dropoff,
                "order_price": round(price, 2),
                "is_premium": is_premium,
                "is_cancelled": is_cancelled
            })
            global_order_id += 1

        fleet = {
            "num_delievery_guys": random.randint(1, 4),
            "depot_node": DEPOT_NODE
        }

        events.append({
            "orders": orders,
            "fleet": fleet
        })

    queries = {
        "meta": {
            "description": "Randomly generated test events",
            "num_events": len(events)
        },
        "events": events
    }

    with open("queries.json", "w") as f:
        json.dump(queries, f, indent=2)
    print(f"queries.json written with {len(events)} events")

def generate_expected_output():
    with open("queries.json") as f:
        queries = json.load(f)

    results = []
    for event in queries["events"]:
        # ignore cancelled orders in this baseline
        orders = [o for o in event["orders"] if not o["is_cancelled"]]
        depot = event["fleet"]["depot_node"]
        route = [depot]
        order_ids = []

        # Very simple baseline: single driver, sequential pickups and dropoffs
        for o in orders:
            route.append(o["pickup"])
            route.append(o["dropoff"])
            order_ids.append(o["order_id"])

        # We don't know the real shortest path here; just use 0 as placeholder
        total_delivery_time = 0.0

        result = {
            "assignments": [
                {
                    "driver_id": 0,
                    "route": route,
                    "order_ids": order_ids
                }
            ],
            "metrics": {
                "total_delivery_time_s": total_delivery_time
            },
            "processing_time": 0.0
        }
        results.append(result)

    expected = {
        "results": results
    }

    with open("expected_output.json", "w") as f:
        json.dump(expected, f, indent=2)
    print(f"expected_output.json written with {len(results)} results")

if __name__ == "__main__":
    generate_graph()
    generate_queries()
    generate_expected_output()
